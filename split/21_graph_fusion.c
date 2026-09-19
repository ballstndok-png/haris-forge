/* -------- Graph optimization: explicit fusion for Linear -> ReLU -------- */
static Value tensor_linear_relu(VM*vm,int n,Value*a){if(n!=3||!tensor_handle(a[0])||!tensor_handle(a[1])||!tensor_handle(a[2]))return vn();Value t=tensor_linear(vm,3,a);if(!tensor_handle(t))return vn();HTensor*lin=t.u.handle;HTensor*out=(HTensor*)tensor_make_op(TOP_FUSED_LINEAR_RELU,lin->a,lin->b,lin->c);if(!out)return vn();for(size_t i=0;i<out->n;i++){size_t row=i/out->b->shape[0],col=i%out->b->shape[0];double z=lin->data[i];out->data[i]=z>0?z:0;}out->aux=1.0;return tensor_v(out);}
static Value tensor_optimize_graph(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*t=a[0].u.handle;Value r=vsobj();size_t nodes=0;HTensor*stack[256];int sp=0;if(t)stack[sp++]=t;while(sp){HTensor*x=stack[--sp];if(!x)continue;nodes++;if(sp<253){if(x->a)stack[sp++]=x->a;if(x->b)stack[sp++]=x->b;if(x->c)stack[sp++]=x->c;}}int fused=(t->op==TOP_RELU&&t->a&&t->a->op==TOP_LINEAR);stput(r.u.st,"nodes",vi((long long)nodes));stput(r.u.st,"fused_linear_relu",vb(fused));stput(r.u.st,"optimized",vb(fused));if(fused){Value aa[3]={tensor_v(t->a->a),tensor_v(t->a->b),tensor_v(t->a->c)};Value opt=tensor_linear_relu(vm,3,aa);if(tensor_handle(opt))stput(r.u.st,"graph",opt);}return r;}

/* ===================== In-process LLM backend (optional llama.cpp) ===================== */
struct HLLM {
    int kind;
    void *model;
    void *ctx;
    void *sampler;
    void *vocab;
    char *path;
    int n_ctx;
    int n_threads;
    int gpu_layers;
};
#if defined(HARIS_HAVE_LLAMA)
static int g_llama_backend_refs=0;
#endif
static void hllm_dtor(void *p){
    HLLM*m=(HLLM*)p; if(!m)return;
#if defined(HARIS_HAVE_LLAMA)
    if(m->sampler){llama_sampler_free((struct llama_sampler*)m->sampler);m->sampler=NULL;}
    if(m->ctx){llama_free((struct llama_context*)m->ctx);m->ctx=NULL;}
    if(m->model){llama_model_free((struct llama_model*)m->model);m->model=NULL;}
    if(g_llama_backend_refs>0 && --g_llama_backend_refs==0)llama_backend_free();
#endif
    if(m->path){xfree(m->path);m->path=NULL;} m->kind=HK_CLOSED;
}
#if defined(HARIS_HAVE_LLAMA)
static int hllm_tokenize(const struct llama_vocab*v,const char*text,llama_token**out){
    if(!v||!text||!out)return -1; size_t len=strlen(text); if(len>INT_MAX-16)return -1;
    int cap=(int)len+16; if(cap<32)cap=32; llama_token*t=(llama_token*)xmalloc((size_t)cap*sizeof(*t));
    int n=llama_tokenize(v,text,(int)len,t,cap,true,false);
    if(n<0){int need=-n; if(need<=0||need>H_CALL_MAX*1024*1024){xfree(t);return -1;} t=(llama_token*)xrealloc(t,(size_t)need*sizeof(*t)); n=llama_tokenize(v,text,(int)len,t,need,true,false);}
    if(n<0){xfree(t);return -1;} *out=t; return n;
}
static Value hllm_piece(const struct llama_vocab*v,llama_token tok){
    char buf[4096];int n=llama_token_to_piece(v,tok,buf,(int)sizeof(buf),0,false);if(n<=0)return vs("");char*q=xmalloc((size_t)n+1);memcpy(q,buf,(size_t)n);q[n]=0;Value r=vs(q);xfree(q);return r;
}
#endif
static Value ai_llm_info(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();Value o=vsobj();
#if defined(HARIS_HAVE_LLAMA)
    stput(o.u.st,"backend",vs("llama.cpp"));stput(o.u.st,"in_process",vb(1));stput(o.u.st,"subprocess",vb(0));stput(o.u.st,"gpu_offload",vb(llama_supports_gpu_offload()));stput(o.u.st,"version",vs(llama_version()));
#else
    stput(o.u.st,"backend",vs("unavailable"));stput(o.u.st,"in_process",vb(0));stput(o.u.st,"subprocess",vb(0));stput(o.u.st,"message",vs("Build with HARIS_USE_LLAMA_CPP and link llama.cpp for in-process GGUF/LLM support."));
#endif
    return o;}
static Value ai_llm_load(VM*vm,int n,Value*a){(void)vm;if(n<1||n>3||a[0].t!=VSTR)return vn();
#if !defined(HARIS_HAVE_LLAMA)
    return vn();
#else
    int nctx=n>=2&&a[1].t==VINT?(int)a[1].u.i:4096;int ngl=n>=3&&a[2].t==VINT?(int)a[2].u.i:0; if(nctx<128||nctx>131072||ngl<0||ngl>1000)return vn();
    int nth=0; const char*ets=getenv("HARIS_LLM_THREADS"); if(ets)nth=atoi(ets); if(nth<1)nth=0;
    EXEC_LOCK(); if(g_llama_backend_refs==0)llama_backend_init();
    struct llama_model_params mp=llama_model_default_params();mp.n_gpu_layers=ngl;struct llama_model*m=llama_model_load_from_file(a[0].u.s,mp);if(!m){EXEC_UNLOCK();return vn();}
    struct llama_context_params cp=llama_context_default_params();cp.n_ctx=(uint32_t)nctx;cp.n_batch=(uint32_t)(nctx<4096?nctx:4096);if(nth>0){cp.n_threads=nth;cp.n_threads_batch=nth;}struct llama_context*c=llama_init_from_model(m,cp);if(!c){llama_model_free(m);EXEC_UNLOCK();return vn();}
    struct llama_sampler*sm=llama_sampler_chain_init(llama_sampler_chain_default_params());if(!sm){llama_free(c);llama_model_free(m);EXEC_UNLOCK();return vn();}
    llama_sampler_chain_add(sm,llama_sampler_init_min_p(0.05f,1));llama_sampler_chain_add(sm,llama_sampler_init_temp(0.7f));llama_sampler_chain_add(sm,llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
    HLLM*h=(HLLM*)xmalloc_dtor(sizeof(*h),hllm_dtor);memset(h,0,sizeof*h);h->kind=HK_LLM;h->model=m;h->ctx=c;h->sampler=sm;h->vocab=(void*)llama_model_get_vocab(m);h->path=xdup(a[0].u.s);h->n_ctx=nctx;h->n_threads=nth;h->gpu_layers=ngl;g_llama_backend_refs++;EXEC_UNLOCK();return (Value){.t=VHANDLE,.u.handle=h};
#endif
}
static Value ai_llm_generate(VM*vm,int n,Value*a){(void)vm;if(n<2||n>5||!hkind(a[0],HK_LLM)||a[1].t!=VSTR)return vn();
#if !defined(HARIS_HAVE_LLAMA)
    return vn();
#else
    HLLM*h=(HLLM*)a[0].u.handle;int max_tokens=n>=3&&a[2].t==VINT?(int)a[2].u.i:256;double temp=n>=4&&isnum(a[3])?dn(a[3]):0.7;double top_p=n>=5&&isnum(a[4])?dn(a[4]):0.95;if(max_tokens<1||max_tokens>32768||temp<0||temp>5||top_p<=0||top_p>1)return vn();
    EXEC_LOCK();
    struct llama_context*c=(struct llama_context*)h->ctx;const struct llama_vocab*v=(const struct llama_vocab*)h->vocab;llama_memory_clear(llama_get_memory(c),true);llama_sampler_reset((struct llama_sampler*)h->sampler);
    llama_token*toks=NULL;int nt=hllm_tokenize(v,a[1].u.s,&toks);if(nt<=0||nt+max_tokens>(int)llama_n_ctx(c)){if(toks)xfree(toks);EXEC_UNLOCK();return vn();}
    struct llama_sampler *sm=(struct llama_sampler*)h->sampler;
    if(sm)llama_sampler_free(sm);
    sm=llama_sampler_chain_init(llama_sampler_chain_default_params());
    if(!sm){xfree(toks);EXEC_UNLOCK();return vn();}
    llama_sampler_chain_add(sm,llama_sampler_init_min_p(0.05f,1));
    llama_sampler_chain_add(sm,llama_sampler_init_temp((float)temp));
    llama_sampler_chain_add(sm,llama_sampler_init_top_p((float)top_p,1));
    llama_sampler_chain_add(sm,llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
    h->sampler=sm;
    struct llama_batch b=llama_batch_get_one(toks,nt);int rc=llama_decode(c,b);if(rc!=0){xfree(toks);EXEC_UNLOCK();return vn();}
    DBuf out;dbuf_init(&out);for(int step=0;step<max_tokens;step++){llama_token tok=llama_sampler_sample(sm,c,-1);llama_sampler_accept(sm,tok);if(llama_vocab_is_eog(v,tok))break;Value piece=hllm_piece(v,tok);if(piece.t!=VSTR){xfree(toks);if(out.d)xfree(out.d);EXEC_UNLOCK();return vn();}dbuf_puts(&out,piece.u.s);if(tok==LLAMA_TOKEN_NULL)break;b=llama_batch_get_one(&tok,1);rc=llama_decode(c,b);if(rc!=0)break;}
    xfree(toks);Value r=vs(out.d?out.d:"");if(out.d)xfree(out.d);EXEC_UNLOCK();return r;
#endif
}
static Value ai_llm_close(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_LLM))return vb(0);hllm_dtor(a[0].u.handle);return vb(1);}
static Value ai_gguf(VM*vm,int n,Value*a){
    /* Backward compatible facade: ai.gguf(model) returns a loaded handle;
       ai.gguf(model,prompt[,max_tokens]) performs a one-shot in-process run. */
    if(n==1) return ai_llm_load(vm,n,a);
    if(n>=2 && a[0].t==VSTR && a[1].t==VSTR){
        Value h=ai_llm_load(vm,1,a);
        if(h.t!=VHANDLE)return vn();
        Value args[4]; args[0]=h; args[1]=a[1]; args[2]=n>=3&&a[2].t==VINT?a[2]:vi(128); args[3]=vf(0.7);
        Value out=ai_llm_generate(vm,4,args);
        Value close_args[1]={h}; ai_llm_close(vm,1,close_args);
        return out;
    }
    return vn();
}
static Value agent_create(VM*vm,int n,Value*a){
    (void)vm;if(n<3||n>5||a[0].t!=VSTR||a[1].t!=VSTR||a[2].t!=VSTR)return vn();HAgent*g=xmalloc_dtor(sizeof(*g),hagent_dtor);memset(g,0,sizeof *g);g->kind=HK_AGENT;g->endpoint=xdup(a[0].u.s);g->key=xdup(a[1].u.s);g->model=xdup(a[2].u.s);g->system=xdup(n>=4&&a[3].t==VSTR?a[3].u.s:"You are a Haris agent. Use tools when needed.");g->tools=n==5?a[4]:va();g->max_steps=8;return (Value){.t=VHANDLE,.u.handle=g};
}
static Value agent_step(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"ai.agent_step");if(n!=2||!handle_kind(a[0],HK_AGENT)||a[1].t!=VARR)return vn();HAgent*g=a[0].u.handle;Value sys=ai_message_common(1,(Value[]){vs(g->system)},"system");Value msgs=va();ap(msgs.u.a,sys);for(size_t i=0;i<a[1].u.a->n;i++)if(a[1].u.a->v[i].t==VSTRUCT)ap(msgs.u.a,a[1].u.a->v[i]);char*body=ai_json_body_messages(msgs,g->model);if(!body)return vn();long code=0;char*r=ai_http_post_json(g->endpoint,g->key,body,&code);xfree(body);Value out=vsobj();stput(out.u.st,"status",vi(code));stput(out.u.st,"body",vs(r?r:""));if(r)xfree(r);return out;
}
static Value agent_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!handle_kind(a[0],HK_AGENT))return vn();HAgent*g=a[0].u.handle;Value o=vsobj();stput(o.u.st,"endpoint",vs(g->endpoint));stput(o.u.st,"model",vs(g->model));stput(o.u.st,"max_steps",vi(g->max_steps));return o;}
static Value agent_close(VM*vm,int n,Value*a){(void)vm;if(n!=1||!handle_kind(a[0],HK_AGENT))return vb(0);HAgent*g=a[0].u.handle;hagent_dtor(g);return vb(1);}


