/* ---------------- Extended AI / Game Engine SDK ---------------- */
struct HEntity { int kind; double x,y,vx,vy,health,max_health,radius; int alive; };
struct HWorld { int kind; HEntity **e; size_t n,cap; double gravity_x,gravity_y; double t; };
struct HModel { int kind; int in,out; double w[2048]; double b[32]; };
#define HK_WORLD 0x454E47
#define HK_ENTITY 0x454E54
#define HK_MODEL 0x41494D
static void hworld_dtor(void*p){HWorld*w=(HWorld*)p;if(w&&w->e){xfree(w->e);w->e=NULL;w->n=w->cap=0;}}
static int hkind(Value v,int k){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==k;}
static Value game_pair(double x,double y){Value o=va();ap(o.u.a,vf(x));ap(o.u.a,vf(y));return o;}
#define HK_GAME_AI 0x47414931
#define HK_GAME_SERVER 0x47535652
typedef struct HGameAI {
    int kind, in, hidden, actions;
    double *W,*B,*TW,*TB,*MW,*VW,*MB,*VB;
    size_t replay_cap,replay_n,replay_head,prio_base;
    double *replay_s,*replay_ns,*replay_r,*replay_p,*prio_tree;
    int *replay_a,*replay_d;
    double gamma,epsilon,tau,lr,prio_alpha,prio_beta,epsilon_min,epsilon_decay;
    double adam_b1_pow,adam_b2_pow;
    uint64_t rng,learn_steps; int skill_rating;
} HGameAI;
static uint64_t game_ai_rng_next(HGameAI*g){uint64_t x=g->rng?g->rng:UINT64_C(0x9e3779b97f4a7c15);x^=x>>12;x^=x<<25;x^=x>>27;g->rng=x;return x*UINT64_C(0x2545F4914F6CDD1D);}
static double game_ai_rng01(HGameAI*g){return (double)(game_ai_rng_next(g)>>11)*(1.0/9007199254740992.0);}
static void game_ai_dtor(void*p){HGameAI*g=(HGameAI*)p;if(!g)return;xfree(g->W);xfree(g->B);xfree(g->TW);xfree(g->TB);xfree(g->MW);xfree(g->VW);xfree(g->MB);xfree(g->VB);xfree(g->replay_s);xfree(g->replay_ns);xfree(g->replay_r);xfree(g->replay_p);xfree(g->prio_tree);xfree(g->replay_a);xfree(g->replay_d);}

/* ===== Haris v2.0 AI + Godot-oriented Game SDK ===== */
/* These APIs are runtime primitives for an eventual Godot adapter, not a claim
   that Godot itself is embedded in Haris. */

/* Conservative guardrails for nested managed arrays returned by batch AI APIs.
   The scalar limit keeps both allocation growth and pretty-printing bounded;
   the VM memory limit is checked as a second line of defense. */
#ifndef HARIS_AI_BATCH_MAX
#define HARIS_AI_BATCH_MAX 4096ULL
#endif
#ifndef HARIS_AI_BATCH_VALUES_MAX
#define HARIS_AI_BATCH_VALUES_MAX (1024ULL*1024ULL)
#endif
static int ai_batch_memory_ok(const VM *vm,size_t rows,size_t width){
    size_t scalars=0;
    if(rows>HARIS_AI_BATCH_MAX||!size_mul_ok(rows,width,&scalars)||scalars>HARIS_AI_BATCH_VALUES_MAX)return 0;
    if(vm&&vm->mem_limit_bytes){
        /* arr_reserve doubles capacity, and each prediction owns a small Arr.
           Use a conservative 3x scalar estimate to fail before xmalloc()/oom(). */
        size_t bytes=0, per=0;
        if(!size_mul_ok(scalars,sizeof(Value),&per)||!size_mul_ok(per,3,&bytes))return 0;
        size_t overhead=0;
        if(!size_mul_ok(rows,sizeof(Arr)+sizeof(Value)*2,&overhead))return 0;
        if(bytes>SIZE_MAX-overhead)return 0;
        bytes+=overhead;
        if(g_mem_used>vm->mem_limit_bytes||bytes>vm->mem_limit_bytes-g_mem_used)return 0;
    }
    return 1;
}
static double ai_xavier_limit(int fan_in,int fan_out){
    return (fan_in>0&&fan_out>0)?sqrt(6.0/((double)fan_in+(double)fan_out)):0.0;
}
static double ai_rand_uniform(VM*vm){
    return hr_rng_unit(vm)*2.0-1.0;
}
static double ai_xavier_weight(VM*vm,int fan_in,int fan_out){
    return ai_rand_uniform(vm)*ai_xavier_limit(fan_in,fan_out);
}

static Value ai_mlp(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VINT||a[1].t!=VINT||a[2].t!=VINT)return vn();int in=(int)a[0].u.i,h=(int)a[1].u.i,out=(int)a[2].u.i;if(in<1||in>128||h<1||h>128||out<1||out>64)return vn();if((size_t)in*(size_t)h>16384|| (size_t)h*(size_t)out>8192)return vn();HMLP*m=(HMLP*)xmalloc_dtor(sizeof(*m),hmlp_dtor);memset(m,0,sizeof(*m));m->kind=HK_MLP;m->in=in;m->hidden=h;m->out=out;m->w1=(double*)xmalloc((size_t)in*h*sizeof(double));m->b1=(double*)xmalloc((size_t)h*sizeof(double));m->w2=(double*)xmalloc((size_t)h*out*sizeof(double));m->b2=(double*)xmalloc((size_t)out*sizeof(double));memset(m->b1,0,(size_t)h*sizeof(double));memset(m->b2,0,(size_t)out*sizeof(double));for(int i=0;i<in*h;i++)m->w1[i]=ai_xavier_weight(vm,in,h);for(int i=0;i<h*out;i++)m->w2[i]=ai_xavier_weight(vm,h,out);return (Value){.t=VHANDLE,.u.handle=m};}
static Value ai_mlp_predict(VM*vm,int n,Value*a){
    (void)vm;if(n!=2||!hkind(a[0],HK_MLP)||a[1].t!=VARR)return vn();HMLP*m=(HMLP*)a[0].u.handle;if((int)a[1].u.a->n!=m->in)return vn();
    double h[128];const size_t in=(size_t)m->in;
    for(int i=0;i<m->in;i++)if(!isnum(a[1].u.a->v[i]))return vn();
    for(int j=0;j<m->hidden;j++){const double *w=m->w1+(size_t)j*in;double z=m->b1[j];for(size_t i=0;i<in;i++)z+=w[i]*dn(a[1].u.a->v[i]);h[j]=z>0?z:0;}
    Value o=va();for(int k=0;k<m->out;k++){const double*w=m->w2+(size_t)k*(size_t)m->hidden;double z=m->b2[k];for(int j=0;j<m->hidden;j++)z+=w[j]*h[j];ap(o.u.a,vf(z));}return o;
}
static Value ai_mlp_predict_batch(VM*vm,int n,Value*a){
    if(n!=2||!hkind(a[0],HK_MLP)||a[1].t!=VARR)return vn();HMLP*m=(HMLP*)a[0].u.handle;size_t rows=a[1].u.a->n;if(!ai_batch_memory_ok(vm,rows,(size_t)m->out))return vn();Value out=va();double h[128];const size_t in=(size_t)m->in;
    for(size_t sidx=0;sidx<a[1].u.a->n;sidx++){Value row=a[1].u.a->v[sidx];if(row.t!=VARR||(int)row.u.a->n!=m->in)return vn();for(int i=0;i<m->in;i++)if(!isnum(row.u.a->v[i]))return vn();
        for(int j=0;j<m->hidden;j++){const double*w=m->w1+(size_t)j*in;double z=m->b1[j];for(size_t i=0;i<in;i++)z+=w[i]*dn(row.u.a->v[i]);h[j]=z>0?z:0;}
        Value pred=va();for(int k=0;k<m->out;k++){const double*w=m->w2+(size_t)k*(size_t)m->hidden;double z=m->b2[k];for(int j=0;j<m->hidden;j++)z+=w[j]*h[j];ap(pred.u.a,vf(z));}ap(out.u.a,pred);
    }return out;
}
static Value ai_mlp_predict_proba(VM*vm,int n,Value*a){
    Value p=ai_mlp_predict(vm,n,a);if(p.t!=VARR||!p.u.a->n)return vn();
    double mx=-INFINITY,sum=0;for(size_t i=0;i<p.u.a->n;i++){double x=dn(p.u.a->v[i]);if(x>mx)mx=x;}
    for(size_t i=0;i<p.u.a->n;i++){double e=exp(dn(p.u.a->v[i])-mx);p.u.a->v[i]=vf(e);sum+=e;}
    if(!isfinite(sum)||sum<=0)return vn();
    for(size_t i=0;i<p.u.a->n;i++)p.u.a->v[i]=vf(dn(p.u.a->v[i])/sum);
    return p;
}
static Value ai_predict_class(VM*vm,int n,Value*a){Value p=ai_mlp_predict(vm,n,a);if(p.t!=VARR||!p.u.a->n)return vn();size_t k=0;for(size_t i=1;i<p.u.a->n;i++)if(dn(p.u.a->v[i])>dn(p.u.a->v[k]))k=i;return vi((long long)k);}
static inline void hmlp_adam(double*p,double g,double*m,double*v,double lr,double bc1,double bc2){const double b1=0.9,b2=0.999,eps=1e-8;*m=b1*(*m)+(1-b1)*g;*v=b2*(*v)+(1-b2)*g*g;*p-=lr*((*m)/bc1)/(sqrt((*v)/bc2)+eps);}
static Value ai_mlp_train(VM*vm,int n,Value*a){
    (void)vm;if(n!=5||!hkind(a[0],HK_MLP)||a[1].t!=VARR||a[2].t!=VARR||!isnum(a[3])||a[4].t!=VINT)return vb(0);HMLP*m=(HMLP*)a[0].u.handle;size_t samples=a[1].u.a->n;if(!samples||samples!=a[2].u.a->n)return vb(0);int epochs=(int)a[4].u.i;if(epochs<1||epochs>5000)return vb(0);double lr=dn(a[3]);if(!isfinite(lr)||lr<=0||!hmlp_opt_init(m))return vb(0);
    size_t xn=samples*(size_t)m->in,yn=samples*(size_t)m->out;if(samples>(SIZE_MAX/((size_t)m->in*sizeof(double)))||samples>(SIZE_MAX/((size_t)m->out*sizeof(double)))||xn>12000000ULL||yn>6000000ULL)return vb(0);double*X=(double*)xmalloc(xn*sizeof(double)),*Y=(double*)xmalloc(yn*sizeof(double));
    int ok=1;for(size_t sidx=0;sidx<samples&&ok;sidx++){Value xv=a[1].u.a->v[sidx],yv=a[2].u.a->v[sidx];if(xv.t!=VARR||yv.t!=VARR||(int)xv.u.a->n!=m->in||(int)yv.u.a->n!=m->out){ok=0;break;}for(int i=0;i<m->in;i++){if(!isnum(xv.u.a->v[i])){ok=0;break;}X[sidx*(size_t)m->in+i]=dn(xv.u.a->v[i]);}for(int k=0;k<m->out&&ok;k++){if(!isnum(yv.u.a->v[k])){ok=0;break;}Y[sidx*(size_t)m->out+k]=dn(yv.u.a->v[k]);}}
    if(!ok){xfree(X);xfree(Y);return vb(0);}double h[128],dh[128],err[64];const size_t in=(size_t)m->in,hidden=(size_t)m->hidden,out=(size_t)m->out;
    for(int ep=0;ep<epochs;ep++)for(size_t si=0;si<samples;si++){const double*x=X+si*in,*y=Y+si*out;for(size_t j=0;j<hidden;j++){const double*w=m->w1+j*in;double z=m->b1[j];for(size_t i=0;i<in;i++)z+=w[i]*x[i];h[j]=z>0?z:0;dh[j]=0;}for(size_t k=0;k<out;k++){const double*w=m->w2+k*hidden;double z=m->b2[k];for(size_t j=0;j<hidden;j++)z+=w[j]*h[j];err[k]=z-y[k];}for(size_t j=0;j<hidden;j++){double g=0;for(size_t k=0;k<out;k++)g+=m->w2[k*hidden+j]*err[k];dh[j]=h[j]>0?g:0;}
        m->opt_step++;m->opt_b1_pow*=0.9;m->opt_b2_pow*=0.999;double bc1=1.0-m->opt_b1_pow,bc2=1.0-m->opt_b2_pow;
        for(size_t k=0;k<out;k++){for(size_t j=0;j<hidden;j++)hmlp_adam(&m->w2[k*hidden+j],err[k]*h[j],&m->m2[k*hidden+j],&m->v2[k*hidden+j],lr,bc1,bc2);hmlp_adam(&m->b2[k],err[k],&m->mb2[k],&m->vb2[k],lr,bc1,bc2);}
        for(size_t j=0;j<hidden;j++){for(size_t i=0;i<in;i++)hmlp_adam(&m->w1[j*in+i],dh[j]*x[i],&m->m1[j*in+i],&m->v1[j*in+i],lr,bc1,bc2);hmlp_adam(&m->b1[j],dh[j],&m->mb1[j],&m->vb1[j],lr,bc1,bc2);}
    }
    xfree(X);xfree(Y);return vb(ok);
}
static Value ai_mlp_train_class(VM*vm,int n,Value*a){
    (void)vm;if(n!=5||!hkind(a[0],HK_MLP)||a[1].t!=VARR||a[2].t!=VARR||!isnum(a[3])||a[4].t!=VINT)return vb(0);HMLP*m=(HMLP*)a[0].u.handle;size_t samples=a[1].u.a->n;if(!samples||samples!=a[2].u.a->n)return vb(0);int epochs=(int)a[4].u.i;if(epochs<1||epochs>5000)return vb(0);double lr=dn(a[3]);if(!isfinite(lr)||lr<=0||!hmlp_opt_init(m))return vb(0);
    size_t xcount=samples*(size_t)m->in;if(samples>SIZE_MAX/((size_t)m->in*sizeof(double))||xcount>12000000ULL||samples>SIZE_MAX/sizeof(int))return vb(0);double*X=(double*)xmalloc(xcount*sizeof(double));int*labels=(int*)xmalloc(samples*sizeof(int));int ok=1;
    for(size_t sidx=0;sidx<samples&&ok;sidx++){Value xv=a[1].u.a->v[sidx],yv=a[2].u.a->v[sidx];if(xv.t!=VARR||yv.t!=VINT||(int)xv.u.a->n!=m->in||yv.u.i<0||yv.u.i>=m->out){ok=0;break;}labels[sidx]=(int)yv.u.i;for(int i=0;i<m->in;i++){if(!isnum(xv.u.a->v[i])){ok=0;break;}X[sidx*(size_t)m->in+i]=dn(xv.u.a->v[i]);}}
    if(!ok){xfree(X);xfree(labels);return vb(0);}double h[128],dh[128],p[64],delta[64];const size_t in=(size_t)m->in,hidden=(size_t)m->hidden,out=(size_t)m->out;
    for(int ep=0;ep<epochs;ep++)for(size_t si=0;si<samples;si++){const double*x=X+si*in;for(size_t j=0;j<hidden;j++){const double*w=m->w1+j*in;double z=m->b1[j];for(size_t i=0;i<in;i++)z+=w[i]*x[i];h[j]=z>0?z:0;dh[j]=0;}double mx=-INFINITY;for(size_t k=0;k<out;k++){double z=m->b2[k];const double*w=m->w2+k*hidden;for(size_t j=0;j<hidden;j++)z+=w[j]*h[j];p[k]=z;if(z>mx)mx=z;}double sum=0;for(size_t k=0;k<out;k++){p[k]=exp(p[k]-mx);sum+=p[k];}double inv=1.0/sum;for(size_t k=0;k<out;k++)delta[k]=p[k]*inv-(k==(size_t)labels[si]?1.0:0.0);for(size_t j=0;j<hidden;j++){double g=0;for(size_t k=0;k<out;k++)g+=m->w2[k*hidden+j]*delta[k];dh[j]=h[j]>0?g:0;}
        m->opt_step++;m->opt_b1_pow*=0.9;m->opt_b2_pow*=0.999;double bc1=1.0-m->opt_b1_pow,bc2=1.0-m->opt_b2_pow;
        for(size_t k=0;k<out;k++){for(size_t j=0;j<hidden;j++)hmlp_adam(&m->w2[k*hidden+j],delta[k]*h[j],&m->m2[k*hidden+j],&m->v2[k*hidden+j],lr,bc1,bc2);hmlp_adam(&m->b2[k],delta[k],&m->mb2[k],&m->vb2[k],lr,bc1,bc2);}
        for(size_t j=0;j<hidden;j++){for(size_t i=0;i<in;i++)hmlp_adam(&m->w1[j*in+i],dh[j]*x[i],&m->m1[j*in+i],&m->v1[j*in+i],lr,bc1,bc2);hmlp_adam(&m->b1[j],dh[j],&m->mb1[j],&m->vb1[j],lr,bc1,bc2);}
    }
    xfree(X);xfree(labels);return vb(ok);
}
static Value ai_mlp_predict_proba_batch(VM*vm,int n,Value*a){
    if(n!=2||!hkind(a[0],HK_MLP)||a[1].t!=VARR)return vn();HMLP*m=(HMLP*)a[0].u.handle;size_t rows=a[1].u.a->n;if(!ai_batch_memory_ok(vm,rows,(size_t)m->out))return vn();Value out=va();double h[128],logits[64];const size_t in=(size_t)m->in;
    for(size_t sidx=0;sidx<a[1].u.a->n;sidx++){Value row=a[1].u.a->v[sidx];if(row.t!=VARR||(int)row.u.a->n!=m->in)return vn();for(int i=0;i<m->in;i++)if(!isnum(row.u.a->v[i]))return vn();
        for(int j=0;j<m->hidden;j++){const double*w=m->w1+(size_t)j*in;double z=m->b1[j];for(size_t i=0;i<in;i++)z+=w[i]*dn(row.u.a->v[i]);h[j]=z>0?z:0;}
        double mx=-INFINITY;for(int k=0;k<m->out;k++){const double*w=m->w2+(size_t)k*(size_t)m->hidden;double z=m->b2[k];for(int j=0;j<m->hidden;j++)z+=w[j]*h[j];logits[k]=z;if(z>mx)mx=z;}
        double sum=0;for(int k=0;k<m->out;k++){logits[k]=exp(logits[k]-mx);sum+=logits[k];}double inv=sum>0?1.0/sum:0;Value pred=va();for(int k=0;k<m->out;k++)ap(pred.u.a,vf(logits[k]*inv));ap(out.u.a,pred);
    }return out;
}
static Value ai_mlp_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_MLP))return vn();HMLP*m=(HMLP*)a[0].u.handle;Value o=va();ap(o.u.a,vi(m->in));ap(o.u.a,vi(m->hidden));ap(o.u.a,vi(m->out));return o;}
static Value ai_mlp_optimizer_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_MLP))return vn();HMLP*m=(HMLP*)a[0].u.handle;Value o=vsobj();stput(o.u.st,"optimizer",vs("adam"));stput(o.u.st,"steps",vi((long long)m->opt_step));return o;}

/* ===== Deep (multi hidden-layer) MLP ===== */
/* ai.mlp above is a single-hidden-layer net. This is the general case:
   arbitrary layer count/width, ReLU on every hidden layer, linear output,
   full backprop with plain SGD. Handle stays separate (HK_DMLP) so the
   two families don't collide. */
#define HK_DMLP 0x444D4C50
typedef struct { int kind; int nlayers; int *sizes; double **W; double **B; } HDMLP;
static void hdmlp_dtor(void*p){HDMLP*m=(HDMLP*)p;if(!m)return;if(m->W){for(int i=0;i<m->nlayers;i++)if(m->W[i])xfree(m->W[i]);xfree(m->W);m->W=NULL;}if(m->B){for(int i=0;i<m->nlayers;i++)if(m->B[i])xfree(m->B[i]);xfree(m->B);m->B=NULL;}if(m->sizes){xfree(m->sizes);m->sizes=NULL;}}
static double** dmlp_forward_alloc(HDMLP*m){
    if(!m||m->nlayers<1)return NULL;
    size_t pc=0; if(!size_mul_ok((size_t)m->nlayers+1,sizeof(double*),&pc))return NULL;
    double**acts=(double**)xmalloc(pc);memset(acts,0,pc);
    for(int i=0;i<=m->nlayers;i++){size_t z=0;if(!size_mul_ok((size_t)m->sizes[i],sizeof(double),&z)){for(int j=0;j<i;j++)xfree(acts[j]);xfree(acts);return NULL;}acts[i]=(double*)xmalloc(z);}
    return acts;
}
static void dmlp_forward_free(HDMLP*m,double**acts){for(int i=0;i<=m->nlayers;i++)xfree(acts[i]);xfree(acts);}
static int dmlp_forward(HDMLP*m,Arr*input,double**acts){
    if((int)input->n!=m->sizes[0])return 0;
    for(int i=0;i<m->sizes[0];i++){if(!isnum(input->v[i]))return 0;acts[0][i]=dn(input->v[i]);}
    for(int l=0;l<m->nlayers;l++){
        int in=m->sizes[l],out=m->sizes[l+1];double*W=m->W[l],*B=m->B[l];
        for(int j=0;j<out;j++){double z=B[j];for(int i=0;i<in;i++)z+=W[j*in+i]*acts[l][i];acts[l+1][j]=(l<m->nlayers-1)?(z>0?z:0):z;}
    }
    return 1;
}
/* sizes: array of layer widths, e.g. [in, h1, h2, ..., out]. 2-9 entries
   (i.e. up to 7 hidden layers), each width 1-256, capped total weight
   count to keep allocation and training time bounded. */
static Value ai_deep_mlp(VM*vm,int n,Value*a){
    (void)vm;if(n!=1||a[0].t!=VARR)return vn();
    int nl=(int)a[0].u.a->n;if(nl<2||nl>9)return vn();
    int*sizes=(int*)xmalloc((size_t)nl*sizeof(int));
    for(int i=0;i<nl;i++){
        if(a[0].u.a->v[i].t!=VINT){xfree(sizes);return vn();}
        int s=(int)a[0].u.a->v[i].u.i;if(s<1||s>256){xfree(sizes);return vn();}
        sizes[i]=s;
    }
    int nlayers=nl-1;size_t totalw=0;
    for(int i=0;i<nlayers;i++){size_t wz=(size_t)sizes[i]*(size_t)sizes[i+1];if(wz>65536){xfree(sizes);return vn();}totalw+=wz;}
    if(totalw>262144){xfree(sizes);return vn();}
    HDMLP*m=(HDMLP*)xmalloc_dtor(sizeof(*m),hdmlp_dtor);m->kind=HK_DMLP;m->nlayers=nlayers;m->sizes=sizes;
    m->W=(double**)xmalloc((size_t)nlayers*sizeof(double*));m->B=(double**)xmalloc((size_t)nlayers*sizeof(double*));
    for(int i=0;i<nlayers;i++){
        size_t wz=(size_t)sizes[i]*(size_t)sizes[i+1];
        m->W[i]=(double*)xmalloc(wz*sizeof(double));m->B[i]=(double*)xmalloc((size_t)sizes[i+1]*sizeof(double));
        for(size_t k=0;k<wz;k++)m->W[i][k]=ai_xavier_weight(vm,sizes[i],sizes[i+1]);
        memset(m->B[i],0,(size_t)sizes[i+1]*sizeof(double));
    }
    return (Value){.t=VHANDLE,.u.handle=m};
}
static Value ai_deep_mlp_predict(VM*vm,int n,Value*a){
    (void)vm;if(n!=2||!hkind(a[0],HK_DMLP)||a[1].t!=VARR)return vn();
    HDMLP*m=(HDMLP*)a[0].u.handle;double**acts=dmlp_forward_alloc(m);
    if(!dmlp_forward(m,a[1].u.a,acts)){dmlp_forward_free(m,acts);return vn();}
    Value o=va();for(int i=0;i<m->sizes[m->nlayers];i++)ap(o.u.a,vf(acts[m->nlayers][i]));
    dmlp_forward_free(m,acts);return o;
}
static Value ai_deep_mlp_predict_batch(VM*vm,int n,Value*a){
    if(n!=2||!hkind(a[0],HK_DMLP)||a[1].t!=VARR)return vn();HDMLP*m=(HDMLP*)a[0].u.handle;size_t rows=a[1].u.a->n;if(!ai_batch_memory_ok(vm,rows,(size_t)m->sizes[m->nlayers]))return vn();double**acts=dmlp_forward_alloc(m);if(!acts)return vn();Value out=va();
    for(size_t sidx=0;sidx<a[1].u.a->n;sidx++){Value row=a[1].u.a->v[sidx];if(row.t!=VARR||!dmlp_forward(m,row.u.a,acts)){dmlp_forward_free(m,acts);return vn();}Value pred=va();for(int i=0;i<m->sizes[m->nlayers];i++)ap(pred.u.a,vf(acts[m->nlayers][i]));ap(out.u.a,pred);}dmlp_forward_free(m,acts);return out;
}
static Value ai_predict_class_deep(VM*vm,int n,Value*a){Value p=ai_deep_mlp_predict(vm,n,a);if(p.t!=VARR||!p.u.a->n)return vn();size_t k=0;for(size_t i=1;i<p.u.a->n;i++)if(dn(p.u.a->v[i])>dn(p.u.a->v[k]))k=i;return vi((long long)k);}
static Value ai_deep_mlp_train(VM*vm,int n,Value*a){
    (void)vm;if(n!=5||!hkind(a[0],HK_DMLP)||a[1].t!=VARR||a[2].t!=VARR||!isnum(a[3])||a[4].t!=VINT)return vb(0);
    HDMLP*m=(HDMLP*)a[0].u.handle;
    if(a[1].u.a->n!=a[2].u.a->n||!a[1].u.a->n)return vb(0);
    int epochs=(int)a[4].u.i;if(epochs<1||epochs>5000)return vb(0);
    double lr=dn(a[3]);if(!isfinite(lr)||lr<=0)return vb(0);
    int nl=m->nlayers;
    double**acts=dmlp_forward_alloc(m);
    double**deltas=(double**)xmalloc((size_t)nl*sizeof(double*));
    for(int i=0;i<nl;i++)deltas[i]=(double*)xmalloc((size_t)m->sizes[i+1]*sizeof(double));
    int ok=1;
    for(int ep=0;ep<epochs&&ok;ep++){
        for(size_t si=0;si<a[1].u.a->n&&ok;si++){
            Value X=a[1].u.a->v[si],Y=a[2].u.a->v[si];
            if(X.t!=VARR||Y.t!=VARR||(int)Y.u.a->n!=m->sizes[nl]){ok=0;break;}
            if(!dmlp_forward(m,X.u.a,acts)){ok=0;break;}
            int outn=m->sizes[nl],good=1;
            for(int k=0;k<outn;k++){
                if(!isnum(Y.u.a->v[k])){good=0;break;}
                deltas[nl-1][k]=acts[nl][k]-dn(Y.u.a->v[k]);
            }
            if(!good){ok=0;break;}
            for(int l=nl-2;l>=0;l--){
                int cur=m->sizes[l+1],nxt=m->sizes[l+2];
                for(int j=0;j<cur;j++){double e=0;for(int k=0;k<nxt;k++)e+=m->W[l+1][k*cur+j]*deltas[l+1][k];deltas[l][j]=(acts[l+1][j]>0)?e:0;}
            }
            for(int l=0;l<nl;l++){
                int in=m->sizes[l],out=m->sizes[l+1];
                for(int j=0;j<out;j++){for(int i=0;i<in;i++)m->W[l][j*in+i]-=lr*deltas[l][j]*acts[l][i];m->B[l][j]-=lr*deltas[l][j];}
            }
        }
    }
    for(int i=0;i<nl;i++)xfree(deltas[i]);
    xfree(deltas);dmlp_forward_free(m,acts);
    return vb(ok);
}
static Value ai_deep_mlp_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_DMLP))return vn();HDMLP*m=(HDMLP*)a[0].u.handle;Value o=va();for(int i=0;i<=m->nlayers;i++)ap(o.u.a,vi(m->sizes[i]));return o;}

static int game_ai_state(Value v,HGameAI*g,double*out){if(v.t!=VARR||(int)v.u.a->n!=g->in)return 0;for(int i=0;i<g->in;i++)if(!isnum(v.u.a->v[i])||!isfinite(dn(v.u.a->v[i])))return 0;for(int i=0;i<g->in;i++)out[i]=dn(v.u.a->v[i]);return 1;}
static void game_ai_q(HGameAI*g,const double*x,double*q){for(int k=0;k<g->actions;k++){double z=g->B[k];for(int j=0;j<g->hidden;j++){double h=g->B[j];for(int i=0;i<g->in;i++)h+=g->W[j*g->in+i]*x[i];if(h<0)h=0;z+=g->W[g->hidden*g->in+k*g->hidden+j]*h;}q[k]=z;}}
/* Compact two-layer dueling-ready core: online network + target network + Adam + replay buffer. */
static Value ai_game_brain(VM*vm,int n,Value*a){(void)vm;int in=0,h=0,ac=0,skill=1200;size_t cap=4096;
    if(n==1&&a[0].t==VINT){skill=(int)a[0].u.i;if(skill<100||skill>5000)return vn();in=8;h=32;ac=5;cap=4096;}
    else {if(n<3||n>4||a[0].t!=VINT||a[1].t!=VINT||a[2].t!=VINT)return vn();in=(int)a[0].u.i;h=(int)a[1].u.i;ac=(int)a[2].u.i;if(n==4&&a[3].t==VINT)cap=(size_t)a[3].u.i;skill=1200;}
    if(in<1||in>256||h<1||h>512||ac<1||ac>128||cap<64||cap>8192)return vn();HGameAI*g=(HGameAI*)xmalloc_dtor(sizeof(*g),game_ai_dtor);memset(g,0,sizeof*g);g->kind=HK_GAME_AI;g->in=in;g->hidden=h;g->actions=ac;g->replay_cap=cap;g->gamma=0.99;g->epsilon=1.0;g->epsilon_min=0.05;g->epsilon_decay=0.997;g->tau=0.02;g->lr=0.001;g->prio_alpha=0.6;g->prio_beta=0.4;g->adam_b1_pow=1.0;g->adam_b2_pow=1.0;g->skill_rating=skill;g->epsilon=fmax(0.05,fmin(1.0,630.0/(double)skill));g->rng=UINT64_C(0xD1B54A32D192ED03)^((uint64_t)time(NULL)<<17);
    size_t w1=(size_t)in*h,w2=(size_t)h*ac;g->W=(double*)xmalloc((w1+w2)*sizeof(double));g->B=(double*)xmalloc((size_t)(h+ac)*sizeof(double));g->TW=(double*)xmalloc((w1+w2)*sizeof(double));g->TB=(double*)xmalloc((size_t)(h+ac)*sizeof(double));g->MW=(double*)calloc(w1+w2,sizeof(double));g->VW=(double*)calloc(w1+w2,sizeof(double));g->MB=(double*)calloc(h+ac,sizeof(double));g->VB=(double*)calloc(h+ac,sizeof(double));
    g->replay_s=(double*)xmalloc(cap*(size_t)in*sizeof(double));g->replay_ns=(double*)xmalloc(cap*(size_t)in*sizeof(double));g->replay_r=(double*)xmalloc(cap*sizeof(double));g->replay_p=(double*)xmalloc(cap*sizeof(double));
    g->replay_a=(int*)xmalloc(cap*sizeof(int));g->replay_d=(int*)xmalloc(cap*sizeof(int));g->prio_base=1;while(g->prio_base<cap)g->prio_base<<=1;g->prio_tree=(double*)xmalloc((g->prio_base*2)*sizeof(double));memset(g->prio_tree,0,(g->prio_base*2)*sizeof(double));
    double s1=sqrt(2.0/(double)in),s2=sqrt(2.0/(double)h);for(size_t i=0;i<w1;i++)g->W[i]=(game_ai_rng01(g)*2.0-1.0)*s1;for(size_t i=0;i<w2;i++)g->W[w1+i]=(game_ai_rng01(g)*2.0-1.0)*s2;memset(g->B,0,(h+ac)*sizeof(double));
    for(size_t i=0;i<cap;i++)g->replay_p[i]=1.0;for(size_t i=0;i<cap;i++){g->prio_tree[g->prio_base+i]=1.0;size_t k=(g->prio_base+i)>>1;while(k){g->prio_tree[k]=g->prio_tree[k<<1]+g->prio_tree[(k<<1)|1];k>>=1;}}
    memcpy(g->TW,g->W,(w1+w2)*sizeof(double));memcpy(g->TB,g->B,(h+ac)*sizeof(double));return (Value){.t=VHANDLE,.u.handle=g};}
static HGameAI*game_ai_handle(Value v){return (v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==HK_GAME_AI)?(HGameAI*)v.u.handle:NULL;}
static HARIS_HOT HARIS_ALWAYS_INLINE void game_ai_forward(HGameAI*g,const double*x,double*h,double*q,int target){size_t w1=(size_t)g->in*g->hidden;double *W=target?g->TW:g->W,*B=target?g->TB:g->B;double *b1=B,*b2=B+g->hidden;for(int j=0;j<g->hidden;j++){double z=b1[j];for(int i=0;i<g->in;i++)z+=W[(size_t)j*g->in+i]*x[i];h[j]=z>0?z:0;}for(int k=0;k<g->actions;k++){double z=b2[k];for(int j=0;j<g->hidden;j++)z+=W[w1+(size_t)k*g->hidden+j]*h[j];q[k]=z;}}
static Value ai_game_act(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||!game_ai_handle(a[0]))return vn();HGameAI*g=game_ai_handle(a[0]);double eps=n==3&&isnum(a[2])?dn(a[2]):g->epsilon;if(eps<0||eps>1)return vn();double x[256],h[512],q[128];if(!game_ai_state(a[1],g,x))return vn();game_ai_forward(g,x,h,q,0);int best=0;if(game_ai_rng01(g)<eps)best=(int)(game_ai_rng_next(g)%((uint64_t)g->actions));else for(int i=1;i<g->actions;i++)if(q[i]>q[best])best=i;return vi(best);}
static void game_ai_priority_set(HGameAI*g,size_t idx,double p);

static void game_ai_priority_set(HGameAI*g,size_t idx,double p){if(!g||!g->prio_tree||idx>=g->replay_cap)return;double z=pow(fmax(p,1e-6),g->prio_alpha);size_t k=g->prio_base+idx;g->prio_tree[k]=z;for(k>>=1;k;k>>=1)g->prio_tree[k]=g->prio_tree[k<<1]+g->prio_tree[(k<<1)|1];}
static HARIS_ALWAYS_INLINE int game_ai_act_raw(HGameAI*g,const double*x){if(!g||!x||g->actions<1||g->in!=8)return -1;double h[512],q[128];game_ai_forward(g,x,h,q,0);int best=0;if(game_ai_rng01(g)<g->epsilon)best=(int)(game_ai_rng_next(g)%((uint64_t)g->actions));else for(int i=1;i<g->actions;i++)if(q[i]>q[best])best=i;return best;}
static HARIS_ALWAYS_INLINE int game_ai_remember_raw(HGameAI*g,const double*x,int act,double reward,const double*nx,int done){if(!g||!x||!nx||act<0||act>=g->actions)return 0;size_t idx=g->replay_head%g->replay_cap;memcpy(g->replay_s+idx*(size_t)g->in,x,(size_t)g->in*sizeof(double));memcpy(g->replay_ns+idx*(size_t)g->in,nx,(size_t)g->in*sizeof(double));g->replay_r[idx]=reward;g->replay_p[idx]=1.0;game_ai_priority_set(g,idx,1.0);g->replay_a[idx]=act;g->replay_d[idx]=done?1:0;g->replay_head++;if(g->replay_n<g->replay_cap)g->replay_n++;return 1;}
static Value ai_game_remember(VM*vm,int n,Value*a){(void)vm;if((n!=4&&n!=6)||!game_ai_handle(a[0])||!isnum(a[3]))return vb(0);HGameAI*g=game_ai_handle(a[0]);double x[256],nx[256];Value next=(n==6?a[4]:a[1]);Value done=(n==6?a[5]:vb(0));if(!game_ai_state(a[1],g,x)||!game_ai_state(next,g,nx)||a[2].t!=VINT||a[2].u.i<0||a[2].u.i>=g->actions||done.t!=VBOOL)return vb(0);size_t idx=g->replay_head%g->replay_cap;memcpy(g->replay_s+idx*(size_t)g->in,x,(size_t)g->in*sizeof(double));memcpy(g->replay_ns+idx*(size_t)g->in,nx,(size_t)g->in*sizeof(double));g->replay_r[idx]=dn(a[3]);g->replay_p[idx]=1.0;game_ai_priority_set(g,idx,1.0);g->replay_a[idx]=(int)a[2].u.i;g->replay_d[idx]=(n==6?a[5].u.b:0);g->replay_head++;if(g->replay_n<g->replay_cap)g->replay_n++;return vb(1);}
static inline void game_ai_adam(double*p,double*g,double*m,double*v,double lr,double bc1,double bc2){const double b1=0.9,b2=0.999,eps=1e-8;*m=b1*(*m)+(1-b1)*(*g);*v=b2*(*v)+(1-b2)*(*g)*(*g);*p-=lr*((*m)/bc1)/(sqrt((*v)/bc2)+eps);}
static size_t game_ai_sample_priority(HGameAI*g){
    if(!g->replay_n)return 0;double sum=(g->prio_tree&&g->prio_base)?g->prio_tree[1]:0;
    if(!(sum>0)||!isfinite(sum))return (size_t)(game_ai_rng_next(g)%g->replay_n);
    double r=game_ai_rng01(g)*sum;size_t node=1;
    while(node<g->prio_base){double left=g->prio_tree[node<<1];if(r<left)node<<=1;else{r-=left;node=(node<<1)|1;}}
    size_t idx=node-g->prio_base;if(idx>=g->replay_n)idx=g->replay_n-1;return idx;
}
static Value ai_game_train_step(VM*vm,int n,Value*a){
    (void)vm;if(n<1||n>3||!game_ai_handle(a[0]))return vn();HGameAI*g=game_ai_handle(a[0]);int batch=n>=2&&a[1].t==VINT?(int)a[1].u.i:32;double lr=n==3&&isnum(a[2])?dn(a[2]):g->lr;if(batch<1||batch>512||!isfinite(lr)||lr<=0||!g->replay_n)return vi(0);
    double x[256],nx[256],h[512],nh[512],q[128],next_online[128],next_target[128];size_t w1=(size_t)g->in*g->hidden,w2=(size_t)g->hidden*g->actions;int steps=0;
    for(int b=0;b<batch;b++){size_t idx=game_ai_sample_priority(g);memcpy(x,g->replay_s+idx*(size_t)g->in,(size_t)g->in*sizeof(double));memcpy(nx,g->replay_ns+idx*(size_t)g->in,(size_t)g->in*sizeof(double));int act=g->replay_a[idx];game_ai_forward(g,x,h,q,0);double target=g->replay_r[idx];
        if(!g->replay_d[idx]){game_ai_forward(g,nx,nh,next_online,0);int best=0;for(int k=1;k<g->actions;k++)if(next_online[k]>next_online[best])best=k;game_ai_forward(g,nx,nh,next_target,1);target+=g->gamma*next_target[best];}
        double td=q[act]-target,grad_out=fabs(td)<=1.0?td:(td>0?1.0:-1.0);g->replay_p[idx]=fmin(fabs(td)+1e-3,1e6);game_ai_priority_set(g,idx,g->replay_p[idx]);
        g->adam_b1_pow*=0.9;g->adam_b2_pow*=0.999;double bc1=1.0-g->adam_b1_pow,bc2=1.0-g->adam_b2_pow;
        for(int j=0;j<g->hidden;j++){double grad=(h[j]>0?g->W[w1+(size_t)act*g->hidden+j]*grad_out:0);for(int i=0;i<g->in;i++){double gg=grad*x[i];game_ai_adam(&g->W[(size_t)j*g->in+i],&gg,&g->MW[(size_t)j*g->in+i],&g->VW[(size_t)j*g->in+i],lr,bc1,bc2);}double bg=grad;game_ai_adam(&g->B[j],&bg,&g->MB[j],&g->VB[j],lr,bc1,bc2);}
        for(int j=0;j<g->hidden;j++){double gg=grad_out*h[j];game_ai_adam(&g->W[w1+(size_t)act*g->hidden+j],&gg,&g->MW[w1+(size_t)act*g->hidden+j],&g->VW[w1+(size_t)act*g->hidden+j],lr,bc1,bc2);}double bg2=grad_out;game_ai_adam(&g->B[g->hidden+act],&bg2,&g->MB[g->hidden+act],&g->VB[g->hidden+act],lr,bc1,bc2);g->learn_steps++;steps++;}
    double tau=g->tau;if(tau<0)tau=0;if(tau>1)tau=1;for(size_t i=0;i<w1+w2;i++)g->TW[i]=(1-tau)*g->TW[i]+tau*g->W[i];for(size_t i=0;i<(size_t)g->hidden+g->actions;i++)g->TB[i]=(1-tau)*g->TB[i]+tau*g->B[i];if(g->epsilon>g->epsilon_min){g->epsilon*=g->epsilon_decay;if(g->epsilon<g->epsilon_min)g->epsilon=g->epsilon_min;}return vi(steps);
}
static Value ai_game_target_update(VM*vm,int n,Value*a){(void)vm;if(n!=2||!game_ai_handle(a[0])||!isnum(a[1]))return vb(0);HGameAI*g=game_ai_handle(a[0]);double tau=dn(a[1]);if(tau<0||tau>1)return vb(0);size_t z=(size_t)g->in*g->hidden+(size_t)g->hidden*g->actions;for(size_t i=0;i<z;i++)g->TW[i]=(1-tau)*g->TW[i]+tau*g->W[i];for(size_t i=0;i<(size_t)g->hidden+g->actions;i++)g->TB[i]=(1-tau)*g->TB[i]+tau*g->B[i];return vb(1);}
static Value ai_game_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!game_ai_handle(a[0]))return vn();HGameAI*g=game_ai_handle(a[0]);Value o=vsobj();stput(o.u.st,"input",vi(g->in));stput(o.u.st,"hidden",vi(g->hidden));stput(o.u.st,"actions",vi(g->actions));stput(o.u.st,"skill",vi(g->skill_rating));stput(o.u.st,"replay",vi((long long)g->replay_n));stput(o.u.st,"epsilon",vf(g->epsilon));stput(o.u.st,"gamma",vf(g->gamma));stput(o.u.st,"lr",vf(g->lr));stput(o.u.st,"tau",vf(g->tau));stput(o.u.st,"priority_alpha",vf(g->prio_alpha));stput(o.u.st,"priority_beta",vf(g->prio_beta));stput(o.u.st,"epsilon_min",vf(g->epsilon_min));stput(o.u.st,"epsilon_decay",vf(g->epsilon_decay));stput(o.u.st,"learn_steps",vi((long long)g->learn_steps));return o;}
static Value ai_game_config(VM*vm,int n,Value*a){(void)vm;if(n<2||n>6||!game_ai_handle(a[0]))return vb(0);HGameAI*g=game_ai_handle(a[0]);if(n>=2){if(!isnum(a[1]))return vb(0);double v=dn(a[1]);if(v<0||v>=1)return vb(0);g->gamma=v;}if(n>=3){if(!isnum(a[2]))return vb(0);double v=dn(a[2]);if(v<0||v>1)return vb(0);g->epsilon=v;}if(n>=4){if(!isnum(a[3]))return vb(0);double v=dn(a[3]);if(v<=0||v>1)return vb(0);g->tau=v;}if(n>=5){if(!isnum(a[4]))return vb(0);double v=dn(a[4]);if(v<=0||v>=1)return vb(0);g->lr=v;}if(n>=6){if(!isnum(a[5]))return vb(0);double v=dn(a[5]);if(v<=0||v>=1)return vb(0);g->epsilon_decay=v;}return vb(1);}


static Value game_nav_astar(VM*vm,int n,Value*a){
    (void)vm;if(n!=7||a[0].t!=VARR||a[1].t!=VINT||a[2].t!=VINT||a[3].t!=VINT||a[4].t!=VINT||a[5].t!=VINT||a[6].t!=VINT)return vn();int w=(int)a[1].u.i,h=(int)a[2].u.i,sx=(int)a[3].u.i,sy=(int)a[4].u.i,gx=(int)a[5].u.i,gy=(int)a[6].u.i;if(w<1||h<1||w>2048||h>2048||a[0].u.a->n!=(size_t)w*(size_t)h||sx<0||sx>=w||sy<0||sy>=h||gx<0||gx>=w||gy<0||gy>=h)return vn();size_t N=(size_t)w*(size_t)h;unsigned char*closed=(unsigned char*)xmalloc(N);int*parent=(int*)xmalloc(N*sizeof(int));int*gscore=(int*)xmalloc(N*sizeof(int));int*fscore=(int*)xmalloc(N*sizeof(int));int*heap=(int*)xmalloc(N*sizeof(int));size_t hn=0;memset(closed,0,N);for(size_t i=0;i<N;i++){parent[i]=-1;gscore[i]=INT_MAX;fscore[i]=INT_MAX;}
    #define HEAP_UP(pos) do{size_t _i=(pos);while(_i){size_t _p=(_i-1)/2;if(fscore[heap[_p]]<=fscore[heap[_i]])break;int _t=heap[_p];heap[_p]=heap[_i];heap[_i]=_t;_i=_p;}}while(0)
    #define HEAP_DOWN(pos) do{size_t _i=(pos);for(;;){size_t _l=_i*2+1,_r=_l+1,_m=_i;if(_l<hn&&fscore[heap[_l]]<fscore[heap[_m]])_m=_l;if(_r<hn&&fscore[heap[_r]]<fscore[heap[_m]])_m=_r;if(_m==_i)break;int _t=heap[_i];heap[_i]=heap[_m];heap[_m]=_t;_i=_m;}}while(0)
    int starti=sy*w+sx,goali=gy*w+gx;gscore[starti]=0;fscore[starti]=abs(gx-sx)+abs(gy-sy);heap[hn++]=starti;int found=0;const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};while(hn){int cur=heap[0];heap[0]=heap[--hn];if(hn)HEAP_DOWN(0);if(closed[cur])continue;closed[cur]=1;if(cur==goali){found=1;break;}int cx=cur%w,cy=cur/w;for(int k=0;k<4;k++){int nx=cx+dx[k],ny=cy+dy[k];if(nx<0||nx>=w||ny<0||ny>=h)continue;int ni=ny*w+nx;Value cell=a[0].u.a->v[ni];if(cell.t!=VINT||cell.u.i!=0||closed[ni])continue;int tg=gscore[cur]+1;if(tg<gscore[ni]){parent[ni]=cur;gscore[ni]=tg;fscore[ni]=tg+abs(gx-nx)+abs(gy-ny);if(hn<N){heap[hn++]=ni;HEAP_UP(hn-1);}}}}Value out=va();if(found){int cur=goali;while(cur>=0){ap(out.u.a,vi(cur));cur=parent[cur];}for(size_t i=0,j=out.u.a->n?out.u.a->n-1:0;i<j;i++,j--){Value t=out.u.a->v[i];out.u.a->v[i]=out.u.a->v[j];out.u.a->v[j]=t;}}xfree(heap);xfree(closed);xfree(parent);xfree(gscore);xfree(fscore);
    #undef HEAP_UP
    #undef HEAP_DOWN
    return out;
}


static Value ai_entropy(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR)return vn();double e=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();double p=dn(a[0].u.a->v[i]);if(p>1e-15)e-=p*log(p);}return vf(e);}
static Value ai_epsilon_greedy(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR||!isnum(a[1])||!a[0].u.a->n)return vn();double eps=dn(a[1]);if(eps<0||eps>1)return vn();if(hr_rng_unit(vm)<eps)return vi((long long)(hr_rng_next(vm)%(uint64_t)a[0].u.a->n));size_t k=0;for(size_t i=1;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();if(dn(a[0].u.a->v[i])>dn(a[0].u.a->v[k]))k=i;}return vi((long long)k);}
static Value ai_q_target(VM*vm,int n,Value*a){(void)vm;if(n!=3||!isnum(a[0])||!isnum(a[1])||!isnum(a[2]))return vn();return vf(dn(a[0])+dn(a[1])*dn(a[2]));}
static Value ai_reward_progress(VM*vm,int n,Value*a){(void)vm;if(n!=2||!isnum(a[0])||!isnum(a[1]))return vn();return vf(dn(a[0])-dn(a[1]));}

struct HSceneNode {int kind;char*name;char*type;double x,y,sx,sy,rotation;int visible;int parent;int*children;size_t nchild,capchild;};
struct HScene {int kind;HSceneNode**nodes;size_t n,cap;};
#define HK_SCENE 0x53434E
#define HK_NODE 0x4E4F44
static void hnode_dtor(void*p){HSceneNode*x=(HSceneNode*)p;if(!x)return;if(x->name){xfree(x->name);x->name=NULL;}if(x->type){xfree(x->type);x->type=NULL;}if(x->children){xfree(x->children);x->children=NULL;}x->nchild=x->capchild=0;}
static void hscene_dtor(void*p){HScene*s=(HScene*)p;if(!s)return;if(s->nodes){xfree(s->nodes);s->nodes=NULL;}s->n=s->cap=0;}

/* Trace native handles that are backed by the Haris managed heap. External
   pointers (e.g. SQLite) are deliberately not treated as GC objects. */
static void gc_mark_handle(void*p){
    if(!p||!heap_is_ptr(p))return;
    int kind=*((int*)p);gc_mark_ptr(p);
    switch(kind){
        case HK_ENTITY: break;
        case HK_WORLD:{HWorld*w=p;if(w->e){gc_mark_ptr(w->e);for(size_t i=0;i<w->n;i++)if(w->e[i])gc_mark_handle(w->e[i]);}break;}
        case HK_MODEL: break;
        case HK_MLP:{HMLP*m=p;if(m->w1)gc_mark_ptr(m->w1);if(m->b1)gc_mark_ptr(m->b1);if(m->w2)gc_mark_ptr(m->w2);if(m->b2)gc_mark_ptr(m->b2);if(m->m1)gc_mark_ptr(m->m1);if(m->v1)gc_mark_ptr(m->v1);if(m->mb1)gc_mark_ptr(m->mb1);if(m->vb1)gc_mark_ptr(m->vb1);if(m->m2)gc_mark_ptr(m->m2);if(m->v2)gc_mark_ptr(m->v2);if(m->mb2)gc_mark_ptr(m->mb2);if(m->vb2)gc_mark_ptr(m->vb2);break;}
        case HK_TENSOR:{HTensor*t=p;if(t->data)gc_mark_ptr(t->data);if(t->grad)gc_mark_ptr(t->grad);if(t->opt_m)gc_mark_ptr(t->opt_m);if(t->opt_v)gc_mark_ptr(t->opt_v);if(t->a)gc_mark_handle(t->a);if(t->b)gc_mark_handle(t->b);if(t->c)gc_mark_handle(t->c);break;}
        case HK_LLM: break;
        case HK_TRANSFORMER: break;
        case HK_RNN: break;
        case HK_LSTM: break;
        case HK_DIST:{HDist*d=p;if(d->peer)gc_mark_ptr(d->peer);break;}
        case HK_DMLP:{HDMLP*m=p;if(m->sizes)gc_mark_ptr(m->sizes);if(m->W){gc_mark_ptr(m->W);for(int i=0;i<m->nlayers;i++)if(m->W[i])gc_mark_ptr(m->W[i]);}if(m->B){gc_mark_ptr(m->B);for(int i=0;i<m->nlayers;i++)if(m->B[i])gc_mark_ptr(m->B[i]);}break;}
        case HK_MEMORY:{HMemory*m=p;if(m->path)gc_mark_ptr(m->path);break;}
        case HK_AGENT:{HAgent*g=p;if(g->endpoint)gc_mark_ptr(g->endpoint);if(g->key)gc_mark_ptr(g->key);if(g->model)gc_mark_ptr(g->model);if(g->system)gc_mark_ptr(g->system);gc_mark_value(g->tools);break;}
        case HK_SCENE:{HScene*s=p;if(s->nodes){gc_mark_ptr(s->nodes);for(size_t i=0;i<s->n;i++)if(s->nodes[i])gc_mark_handle(s->nodes[i]);}break;}
        case HK_NODE:{HSceneNode*x=p;if(x->name)gc_mark_ptr(x->name);if(x->type)gc_mark_ptr(x->type);if(x->children)gc_mark_ptr(x->children);break;}
        case HK_UDP: case HK_WS_LISTENER: case HK_WS: break;
        case HK_GAME_SERVER: gc_mark_game_server_handle(p); break;
        case HK_GAME_AI:{HGameAI*g=p;if(g->W)gc_mark_ptr(g->W);if(g->B)gc_mark_ptr(g->B);if(g->TW)gc_mark_ptr(g->TW);if(g->TB)gc_mark_ptr(g->TB);if(g->MW)gc_mark_ptr(g->MW);if(g->VW)gc_mark_ptr(g->VW);if(g->MB)gc_mark_ptr(g->MB);if(g->VB)gc_mark_ptr(g->VB);if(g->replay_s)gc_mark_ptr(g->replay_s);if(g->replay_ns)gc_mark_ptr(g->replay_ns);if(g->replay_p)gc_mark_ptr(g->replay_p);if(g->prio_tree)gc_mark_ptr(g->prio_tree);if(g->replay_a)gc_mark_ptr(g->replay_a);if(g->replay_r)gc_mark_ptr(g->replay_r);if(g->replay_d)gc_mark_ptr(g->replay_d);break;}
        case HK_CLOSED: break;
        default: gc_mark_late_handle(p,kind); break;
    }
}

static Value engine_scene(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();HScene*s=(HScene*)xmalloc_dtor(sizeof(*s),hscene_dtor);memset(s,0,sizeof(*s));s->kind=HK_SCENE;return (Value){.t=VHANDLE,.u.handle=s};}
static Value engine_node(VM*vm,int n,Value*a){(void)vm;if((n!=2&&n!=3)||!hkind(a[0],HK_SCENE)||a[1].t!=VSTR||(n==3&&a[2].t!=VSTR))return vn();HScene*s=a[0].u.handle;HSceneNode*x=(HSceneNode*)xmalloc_dtor(sizeof(*x),hnode_dtor);memset(x,0,sizeof(*x));x->kind=HK_NODE;x->name=xdup(a[1].u.s);x->type=n==3?xdup(a[2].u.s):xdup("Node");x->sx=x->sy=1;x->visible=1;x->parent=-1;if(s->n==s->cap){s->cap=s->cap?s->cap*2:32;s->nodes=(HSceneNode**)xrealloc(s->nodes,s->cap*sizeof(*s->nodes));}s->nodes[s->n++]=x;return (Value){.t=VHANDLE,.u.handle=x};}
static Value engine_add_child(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hkind(a[0],HK_SCENE)||!hkind(a[1],HK_NODE)||!hkind(a[2],HK_NODE))return vb(0);HScene*s=a[0].u.handle;HSceneNode*p=a[1].u.handle,*c=a[2].u.handle;int pi=-1,ci=-1;for(size_t i=0;i<s->n;i++){if(s->nodes[i]==p)pi=(int)i;if(s->nodes[i]==c)ci=(int)i;}if(pi<0||ci<0||pi==ci)return vb(0);for(size_t j=0;j<p->nchild;j++)if(p->children[j]==ci)return vb(1);if(p->nchild==p->capchild){p->capchild=p->capchild?p->capchild*2:4;p->children=(int*)xrealloc(p->children,p->capchild*sizeof(int));}p->children[p->nchild++]=ci;c->parent=pi;return vb(1);}
static Value engine_node_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_NODE))return vn();HSceneNode*x=a[0].u.handle;Value o=va();ap(o.u.a,vs(x->name));ap(o.u.a,vs(x->type));ap(o.u.a,vf(x->x));ap(o.u.a,vf(x->y));ap(o.u.a,vf(x->sx));ap(o.u.a,vf(x->sy));ap(o.u.a,vf(x->rotation));ap(o.u.a,vb(x->visible));ap(o.u.a,vi((long long)x->nchild));return o;}
static Value engine_node_set_position(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hkind(a[0],HK_NODE)||!isnum(a[1])||!isnum(a[2]))return vb(0);HSceneNode*x=a[0].u.handle;x->x=dn(a[1]);x->y=dn(a[2]);return vb(1);}
static Value engine_node_position(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_NODE))return vn();HSceneNode*x=a[0].u.handle;return game_pair(x->x,x->y);}
static Value engine_node_set_scale(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hkind(a[0],HK_NODE)||!isnum(a[1])||!isnum(a[2]))return vb(0);HSceneNode*x=a[0].u.handle;x->sx=dn(a[1]);x->sy=dn(a[2]);return vb(1);}
static Value engine_node_set_rotation(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hkind(a[0],HK_NODE)||!isnum(a[1]))return vb(0);((HSceneNode*)a[0].u.handle)->rotation=dn(a[1]);return vb(1);}
static Value engine_node_visible(VM*vm,int n,Value*a){(void)vm;if(n==1&&hkind(a[0],HK_NODE))return vb(((HSceneNode*)a[0].u.handle)->visible);if(n==2&&hkind(a[0],HK_NODE)&&a[1].t==VBOOL){((HSceneNode*)a[0].u.handle)->visible=a[1].u.b;return vb(1);}return vb(0);}
static Value engine_scene_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_SCENE))return vn();HScene*s=a[0].u.handle;Value o=va();ap(o.u.a,vi((long long)s->n));return o;}

static Value ngwander(VM*vm,int n,Value*a){(void)vm;if(n!=6)return vn();double x=dn(a[0]),y=dn(a[1]),ang=dn(a[2]),r=dn(a[3]),jit=dn(a[4]),dt=dn(a[5]);ang+=(hr_rng_unit(vm)*2.0-1.0)*jit*dt;return ngseek(vm,6,(Value[]){vf(x),vf(y),vf(x+cos(ang)*r),vf(y+sin(ang)*r),vf(1),vf(dt)});}
static Value ngseparation(VM*vm,int n,Value*a){(void)vm;if(n!=6||a[0].t!=VARR)return vn();double x=dn(a[1]),y=dn(a[2]),radius=dn(a[3]),strength=dn(a[4]),dt=dn(a[5]);double sx=0,sy=0;for(size_t i=0;i<a[0].u.a->n;i++){Value p=a[0].u.a->v[i];if(p.t!=VARR||p.u.a->n<2)continue;double dx=x-dn(p.u.a->v[0]),dy=y-dn(p.u.a->v[1]),d=sqrt(dx*dx+dy*dy);if(d>1e-9&&d<radius){double w=(radius-d)/radius;sx+=dx/d*w;sy+=dy/d*w;}}return game_pair(x+sx*strength*dt,y+sy*strength*dt);}
static Value engine_query_radius(VM*vm,int n,Value*a){(void)vm;if(n!=4||!hkind(a[0],HK_WORLD)||!isnum(a[1])||!isnum(a[2])||!isnum(a[3])||!isfinite(dn(a[1]))||!isfinite(dn(a[2]))||!isfinite(dn(a[3])))return vn();HWorld*w=a[0].u.handle;double x=dn(a[1]),y=dn(a[2]),r=dn(a[3]);if(r<0)r=0;Value o=va();for(size_t i=0;i<w->n;i++){HEntity*e=w->e[i];if(!e||!e->alive)continue;double dx=e->x-x,dy=e->y-y;if(dx*dx+dy*dy<=r*r)ap(o.u.a,(Value){.t=VHANDLE,.u.handle=e});}return o;}
static Value engine_stats(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_WORLD))return vn();HWorld*w=a[0].u.handle;long long alive=0;double hp=0;for(size_t i=0;i<w->n;i++)if(w->e[i]&&w->e[i]->alive){alive++;hp+=w->e[i]->health;}Value o=vsobj();stput(o.u.st,"entities",vi((long long)w->n));stput(o.u.st,"alive",vi(alive));stput(o.u.st,"time",vf(w->t));stput(o.u.st,"total_health",vf(hp));stput(o.u.st,"gravity_x",vf(w->gravity_x));stput(o.u.st,"gravity_y",vf(w->gravity_y));return o;}
static Value engine_world(VM*vm,int n,Value*a){(void)vm;(void)a;if(n!=0)return vn();HWorld*w=xmalloc_dtor(sizeof(HWorld),hworld_dtor);memset(w,0,sizeof* w);w->kind=HK_WORLD;w->gravity_y=0;return (Value){.t=VHANDLE,.u.handle=w};}
static Value engine_entity(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_WORLD))return vn();HWorld*w=(HWorld*)a[0].u.handle;HEntity*e=xmalloc(sizeof(HEntity));memset(e,0,sizeof*e);e->kind=HK_ENTITY;e->health=e->max_health=100;e->radius=16;e->alive=1;if(w->n==w->cap){w->cap=w->cap?w->cap*2:64;w->e=xrealloc(w->e,w->cap*sizeof(*w->e));}w->e[w->n++]=e;return (Value){.t=VHANDLE,.u.handle=e};}
static Value engine_set_position(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hkind(a[0],HK_ENTITY)||!isnum(a[1])||!isnum(a[2]))return vb(0);HEntity*e=a[0].u.handle;e->x=dn(a[1]);e->y=dn(a[2]);return vb(1);}
static Value engine_position(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_ENTITY))return vn();HEntity*e=a[0].u.handle;return game_pair(e->x,e->y);}
static Value engine_set_velocity(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hkind(a[0],HK_ENTITY)||!isnum(a[1])||!isnum(a[2]))return vb(0);HEntity*e=a[0].u.handle;e->vx=dn(a[1]);e->vy=dn(a[2]);return vb(1);}
static Value engine_velocity(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_ENTITY))return vn();HEntity*e=a[0].u.handle;return game_pair(e->vx,e->vy);}
static Value engine_set_health(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||!hkind(a[0],HK_ENTITY)||!isnum(a[1]))return vb(0);HEntity*e=a[0].u.handle;e->max_health=n==3&&isnum(a[2])?dn(a[2]):e->max_health;e->health=dn(a[1]);if(e->health>e->max_health)e->max_health=e->health;e->alive=e->health>0;return vb(1);}
static Value engine_health(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_ENTITY))return vn();HEntity*e=a[0].u.handle;Value o=va();ap(o.u.a,vf(e->health));ap(o.u.a,vf(e->max_health));ap(o.u.a,vb(e->alive));return o;}
static Value engine_step(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hkind(a[0],HK_WORLD)||!isnum(a[1]))return vb(0);HWorld*w=a[0].u.handle;double dt=dn(a[1]);if(dt<0||dt>10)return vb(0);for(size_t i=0;i<w->n;i++){HEntity*e=w->e[i];if(!e||!e->alive)continue;e->vx+=w->gravity_x*dt;e->vy+=w->gravity_y*dt;e->x+=e->vx*dt;e->y+=e->vy*dt;}w->t+=dt;return vb(1);}
static Value engine_world_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_WORLD))return vn();HWorld*w=a[0].u.handle;Value o=va();ap(o.u.a,vi((long long)w->n));ap(o.u.a,vf(w->t));ap(o.u.a,vf(w->gravity_x));ap(o.u.a,vf(w->gravity_y));return o;}
static Value engine_gravity(VM*vm,int n,Value*a){(void)vm;if(n!=3||!hkind(a[0],HK_WORLD)||!isnum(a[1])||!isnum(a[2]))return vb(0);HWorld*w=a[0].u.handle;w->gravity_x=dn(a[1]);w->gravity_y=dn(a[2]);return vb(1);}
static Value engine_destroy(VM*vm,int n,Value*a){(void)vm;if(n!=1||!hkind(a[0],HK_ENTITY))return vb(0);HEntity*e=a[0].u.handle;e->alive=0;return vb(1);}
static Value engine_distance(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hkind(a[0],HK_ENTITY)||!hkind(a[1],HK_ENTITY))return vn();HEntity*x=a[0].u.handle,*y=a[1].u.handle;double dx=x->x-y->x,dy=x->y-y->y;return vf(sqrt(dx*dx+dy*dy));}
static Value engine_circle_collision(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hkind(a[0],HK_ENTITY)||!hkind(a[1],HK_ENTITY))return vb(0);HEntity*x=a[0].u.handle,*y=a[1].u.handle;double dx=x->x-y->x,dy=x->y-y->y,r=x->radius+y->radius;return vb(dx*dx+dy*dy<=r*r);}
static Value engine_damage(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hkind(a[0],HK_ENTITY)||!isnum(a[1]))return vn();HEntity*e=a[0].u.handle;double d=dn(a[1]);if(d<0)d=0;e->health-=d;if(e->health<0)e->health=0;e->alive=e->health>0;return engine_health(vm,1,(Value[]){a[0]});}
static Value engine_set_radius(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hkind(a[0],HK_ENTITY)||!isnum(a[1])||dn(a[1])<0)return vb(0);((HEntity*)a[0].u.handle)->radius=dn(a[1]);return vb(1);}
static Value engine_nearest(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hkind(a[0],HK_WORLD)||!hkind(a[1],HK_ENTITY))return vn();HWorld*w=a[0].u.handle;HEntity*src=a[1].u.handle;HEntity*best=NULL;double bd=INFINITY;for(size_t i=0;i<w->n;i++){HEntity*e=w->e[i];if(!e||!e->alive||e==src)continue;double dx=e->x-src->x,dy=e->y-src->y,d=dx*dx+dy*dy;if(d<bd){bd=d;best=e;}}return best?(Value){.t=VHANDLE,.u.handle=best}:vn();}
static Value engine_raycast(VM*vm,int n,Value*a){(void)vm;if(n!=6||!hkind(a[0],HK_WORLD))return vn();HWorld*w=a[0].u.handle;double x=dn(a[1]),y=dn(a[2]),dx=dn(a[3]),dy=dn(a[4]),maxd=dn(a[5]);if(maxd<0)return vn();double len=sqrt(dx*dx+dy*dy);if(len<1e-9)return vn();dx/=len;dy/=len;HEntity*hit=NULL;double best=maxd;for(size_t i=0;i<w->n;i++){HEntity*e=w->e[i];if(!e||!e->alive)continue;double ox=e->x-x,oy=e->y-y,t=ox*dx+oy*dy;if(t<0||t>maxd)continue;double px=x+dx*t,py=y+dy*t,ddx=e->x-px,ddy=e->y-py;if(ddx*ddx+ddy*ddy<=e->radius*e->radius&&t<best){best=t;hit=e;}}if(!hit)return vn();Value o=va();ap(o.u.a,(Value){.t=VHANDLE,.u.handle=hit});ap(o.u.a,vf(best));return o;}

/* ===== Haris Forge 3.6: higher-level game runtime ===== */
static Value game_spawn(VM*vm,int n,Value*a){
    if(n<3||n>7||!hkind(a[0],HK_WORLD))return vn();
    for(int i=1;i<n;i++)if(!isnum(a[i]))return vn();
    Value args[1]={a[0]};Value ev=engine_entity(vm,1,args);if(ev.t!=VHANDLE)return vn();
    double x=dn(a[1]),y=dn(a[2]);engine_set_position(vm,3,(Value[]){ev,a[1],a[2]});
    if(n>=5)engine_set_velocity(vm,3,(Value[]){ev,a[3],a[4]});
    if(n>=6)engine_set_radius(vm,2,(Value[]){ev,a[5]});
    if(n>=7)engine_set_health(vm,2,(Value[]){ev,a[6]});
    (void)x;(void)y;return ev;
}
static Value game_entities(VM*vm,int n,Value*a){
    (void)vm;if(n!=1||!hkind(a[0],HK_WORLD))return vn();
    HWorld*w=a[0].u.handle;Value out=va();for(size_t i=0;i<w->n;i++)if(w->e[i])ap(out.u.a,(Value){.t=VHANDLE,.u.handle=w->e[i]});return out;
}
static Value game_snapshot(VM*vm,int n,Value*a){
    (void)vm;if(n!=1||!hkind(a[0],HK_WORLD))return vn();
    HWorld*w=a[0].u.handle;Value out=va();
    for(size_t i=0;i<w->n;i++){HEntity*e=w->e[i];if(!e)continue;Value o=vsobj();
        stput(o.u.st,"entity",(Value){.t=VHANDLE,.u.handle=e});
        stput(o.u.st,"x",vf(e->x));stput(o.u.st,"y",vf(e->y));
        stput(o.u.st,"vx",vf(e->vx));stput(o.u.st,"vy",vf(e->vy));
        stput(o.u.st,"health",vf(e->health));stput(o.u.st,"max_health",vf(e->max_health));
        stput(o.u.st,"radius",vf(e->radius));stput(o.u.st,"alive",vb(e->alive));ap(out.u.a,o);
    }
    return out;
}
static Value game_fixed_step(VM*vm,int n,Value*a){
    (void)vm;if(n!=3||!hkind(a[0],HK_WORLD)||!isnum(a[1])||a[2].t!=VINT)return vn();
    long long steps=a[2].u.i;if(steps<0||steps>100000)return vn();double dt=dn(a[1]);if(dt<=0||dt>1)return vn();
    HWorld*w=a[0].u.handle;for(long long i=0;i<steps;i++){Value r=engine_step(vm,2,(Value[]){(Value){.t=VHANDLE,.u.handle=w},a[1]});if(!truth(r))return vb(0);}
    return vb(1);
}
static Value game_collisions(VM*vm,int n,Value*a){
    (void)vm;if(n!=1||!hkind(a[0],HK_WORLD))return vn();
    HWorld*w=a[0].u.handle;Value out=va();
    for(size_t i=0;i<w->n;i++){HEntity*A=w->e[i];if(!A||!A->alive)continue;for(size_t j=i+1;j<w->n;j++){HEntity*B=w->e[j];if(!B||!B->alive)continue;
        double dx=A->x-B->x,dy=A->y-B->y,rr=A->radius+B->radius;if(dx*dx+dy*dy<=rr*rr){Value o=vsobj();
            stput(o.u.st,"a",(Value){.t=VHANDLE,.u.handle=A});stput(o.u.st,"b",(Value){.t=VHANDLE,.u.handle=B});stput(o.u.st,"distance",vf(sqrt(dx*dx+dy*dy)));ap(out.u.a,o);}
    }}
    return out;
}
static Value game_spawn_batch(VM*vm,int n,Value*a){
    if(n!=2||!hkind(a[0],HK_WORLD)||a[1].t!=VARR)return vn();if(a[1].u.a->n>10000)return vn();
    Value out=va();for(size_t i=0;i<a[1].u.a->n;i++){Value row=a[1].u.a->v[i];if(row.t!=VARR||row.u.a->n<2||row.u.a->n>6)continue;
        Value args[7];args[0]=a[0];for(size_t k=0;k<row.u.a->n;k++)args[k+1]=row.u.a->v[k];
        Value e=game_spawn(vm,(int)row.u.a->n+1,args);if(e.t==VHANDLE)ap(out.u.a,e);
    }return out;
}
static Value game_distance(VM*vm,int n,Value*a){return engine_distance(vm,n,a);}
static Value game_nearest(VM*vm,int n,Value*a){return engine_nearest(vm,n,a);}
static Value game_raycast(VM*vm,int n,Value*a){return engine_raycast(vm,n,a);}
static Value game_set_gravity(VM*vm,int n,Value*a){return engine_gravity(vm,n,a);}


static Value ai_rand(VM*vm,int n,Value*a){if(n!=2||!isnum(a[0])||!isnum(a[1]))return vn();double lo=dn(a[0]),hi=dn(a[1]);return vf(lo+(hi-lo)*hr_rng_unit(vm));}
static Value ai_one_hot(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VINT||a[1].t!=VINT||a[1].u.i<=0||a[0].u.i<0||a[0].u.i>=a[1].u.i)return vn();Value o=va();for(long long i=0;i<a[1].u.i;i++)ap(o.u.a,vf(i==a[0].u.i?1:0));return o;}
static Value ai_cross_entropy(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR||a[1].t!=VINT||a[1].u.i<0||(size_t)a[1].u.i>=a[0].u.a->n)return vn();double p=dn(a[0].u.a->v[a[1].u.i]);if(p<1e-15)p=1e-15;return vf(-log(p));}
static Value ai_dropout(VM*vm,int n,Value*a){if(n!=2||a[0].t!=VARR||!isnum(a[1]))return vn();double rate=dn(a[1]);if(rate<0||rate>=1)return vn();Value o=va();for(size_t i=0;i<a[0].u.a->n;i++){double keep=hr_rng_unit(vm)>=rate;if(keep)ap(o.u.a,vf(dn(a[0].u.a->v[i])/(1.0-rate)));else ap(o.u.a,vf(0));}return o;}
static Value ai_argmin(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR||!a[0].u.a->n)return vn();size_t k=0;for(size_t i=1;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();if(dn(a[0].u.a->v[i])<dn(a[0].u.a->v[k]))k=i;}return vi((long long)k);}
static Value ai_mean(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR||!a[0].u.a->n)return vn();double s=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();s+=dn(a[0].u.a->v[i]);}return vf(s/(double)a[0].u.a->n);}
static Value ai_variance(VM*vm,int n,Value*a){Value m=ai_mean(vm,n,a);if(m.t!=VFLOAT)return m;double s=0;for(size_t i=0;i<a[0].u.a->n;i++){double d=dn(a[0].u.a->v[i])-m.u.f;s+=d*d;}return vf(s/(double)a[0].u.a->n);}
static Value ai_clamp_grad(VM*vm,int n,Value*a){(void)vm;if(n!=3||!isnum(a[0])||!isnum(a[1])||!isnum(a[2]))return vn();double x=dn(a[0]),lo=dn(a[1]),hi=dn(a[2]);if(x<lo)x=lo;if(x>hi)x=hi;return vf(x);}

static Value ai_model_linear(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VINT||a[1].t!=VINT)return vn();int in=(int)a[0].u.i,out=(int)a[1].u.i;if(in<1||in>64||out<1||out>32)return vn();HModel*m=xmalloc(sizeof(HModel));memset(m,0,sizeof* m);m->kind=HK_MODEL;m->in=in;m->out=out;for(int j=0;j<out;j++)for(int i=0;i<in;i++)m->w[j*in+i]=(hr_rng_unit(vm)-0.5)*0.02;return (Value){.t=VHANDLE,.u.handle=m};}
static Value ai_model_predict(VM*vm,int n,Value*a){(void)vm;if(n!=2||!hkind(a[0],HK_MODEL)||a[1].t!=VARR)return vn();HModel*m=a[0].u.handle;if((int)a[1].u.a->n!=m->in)return vn();Value o=va();for(int j=0;j<m->out;j++){double z=m->b[j];for(int i=0;i<m->in;i++){if(!isnum(a[1].u.a->v[i]))return vn();z+=m->w[j*m->in+i]*dn(a[1].u.a->v[i]);}ap(o.u.a,vf(z));}return o;}
static Value ai_model_train_linear(VM*vm,int n,Value*a){(void)vm;if(n!=5||!hkind(a[0],HK_MODEL)||a[1].t!=VARR||a[2].t!=VARR||!isnum(a[3])||a[4].t!=VINT)return vb(0);HModel*m=a[0].u.handle;if((size_t)a[1].u.a->n!=a[2].u.a->n||a[2].u.a->n==0)return vb(0);if((int)a[1].u.a->n==0)return vb(0);int epochs=(int)a[4].u.i;if(epochs<1||epochs>10000)return vb(0);double lr=dn(a[3]);if(!isfinite(lr)||lr<=0)return vb(0);for(int ep=0;ep<epochs;ep++){for(size_t sidx=0;sidx<a[1].u.a->n;sidx++){if(a[1].u.a->v[sidx].t!=VARR||a[1].u.a->v[sidx].u.a->n!=(size_t)m->in||a[2].u.a->v[sidx].t!=VARR||a[2].u.a->v[sidx].u.a->n!=(size_t)m->out)return vb(0);for(int j=0;j<m->out;j++){double z=m->b[j];for(int i=0;i<m->in;i++){if(!isnum(a[1].u.a->v[sidx].u.a->v[i]))return vb(0);z+=m->w[j*m->in+i]*dn(a[1].u.a->v[sidx].u.a->v[i]);}double e=z-dn(a[2].u.a->v[sidx].u.a->v[j]);for(int i=0;i<m->in;i++)m->w[j*m->in+i]-=lr*2.0*e*dn(a[1].u.a->v[sidx].u.a->v[i]);m->b[j]-=lr*2.0*e;}}}return vb(1);}
static Value ai_l2(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR)return vn();double s=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();double x=dn(a[0].u.a->v[i]);s+=x*x;}return vf(sqrt(s));}
static Value ai_leaky_relu(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||!isnum(a[0]))return vn();double slope=n==2&&isnum(a[1])?dn(a[1]):0.01;double x=dn(a[0]);return vf(x>0?x:x*slope);}
/* ai.accuracy(predictions, actual): fraction of matching elements, comparing
   with the same numeric-vs-string equality rules the rest of the language
   uses (numbers compared by value, everything else by exact match). */
static Value ai_accuracy(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR||a[1].t!=VARR||a[0].u.a->n!=a[1].u.a->n||!a[0].u.a->n)return vn();size_t hit=0,total=a[0].u.a->n;for(size_t i=0;i<total;i++){Value p=a[0].u.a->v[i],y=a[1].u.a->v[i];int eq;if(isnum(p)&&isnum(y))eq=dn(p)==dn(y);else if(p.t==VSTR&&y.t==VSTR)eq=!strcmp(p.u.s,y.u.s);else if(p.t==VBOOL&&y.t==VBOOL)eq=p.u.b==y.u.b;else eq=0;if(eq)hit++;}return vf((double)hit/(double)total);}
/* ai.knn(train_x, train_y, k, query): classic k-nearest-neighbors classifier.
   train_x is an array of numeric feature vectors (arrays), train_y is the
   parallel array of labels (any Value type, compared by identity/equality
   only for the majority vote), k is how many neighbors to poll, query is
   the feature vector to classify. Returns the majority-vote label, or null
   on shape mismatch. Ties break toward the label seen first among the
   k nearest (stable, deterministic). */
static Value ai_knn(VM*vm,int n,Value*a){(void)vm;
    if(n!=4||a[0].t!=VARR||a[1].t!=VARR||a[2].t!=VINT||a[3].t!=VARR)return vn();
    Arr*X=a[0].u.a,*Y=a[1].u.a,*q=a[3].u.a;long long k=a[2].u.i;
    size_t N=X->n; if(!N||Y->n!=N||k<1||(size_t)k>N||N>4096)return vn();
    size_t d=q->n; if(!d||d>512)return vn();
    for(size_t i=0;i<d;i++)if(!isnum(q->v[i]))return vn();
    double*dist=(double*)xmalloc(N*sizeof(double));
    for(size_t i=0;i<N;i++){
        if(X->v[i].t!=VARR||X->v[i].u.a->n!=d){xfree(dist);return vn();}
        double s=0;for(size_t j=0;j<d;j++){if(!isnum(X->v[i].u.a->v[j])){xfree(dist);return vn();}double diff=dn(X->v[i].u.a->v[j])-dn(q->v[j]);s+=diff*diff;}
        dist[i]=s;
    }
    /* Select the k smallest distances by simple partial selection sort -
       N is capped at 4096 and k<=N, so this stays comfortably bounded. */
    size_t*idx=(size_t*)xmalloc(N*sizeof(size_t));for(size_t i=0;i<N;i++)idx[i]=i;
    for(size_t i=0;i<(size_t)k;i++){size_t best=i;for(size_t j=i+1;j<N;j++)if(dist[idx[j]]<dist[idx[best]])best=j;size_t t=idx[i];idx[i]=idx[best];idx[best]=t;}
    /* Majority vote among the k nearest, using each label's first-seen
       Value for output and Haris's own value-equality rules for grouping. */
    Value votes_label[64];int votes_count[64];int nv=0;
    for(size_t i=0;i<(size_t)k&&i<64;i++){
        Value lab=Y->v[idx[i]]; int found=0;
        for(int v=0;v<nv;v++){Value o=votes_label[v];int eq;
            if(isnum(o)&&isnum(lab))eq=dn(o)==dn(lab);
            else if(o.t==VSTR&&lab.t==VSTR)eq=!strcmp(o.u.s,lab.u.s);
            else if(o.t==VBOOL&&lab.t==VBOOL)eq=o.u.b==lab.u.b;
            else eq=0;
            if(eq){votes_count[v]++;found=1;break;}
        }
        if(!found&&nv<64){votes_label[nv]=lab;votes_count[nv]=1;nv++;}
    }
    xfree(dist);xfree(idx);
    if(!nv)return vn();
    int best=0;for(int v=1;v<nv;v++)if(votes_count[v]>votes_count[best])best=v;
    return votes_label[best];
}
/* ai.kmeans(data, k, iters): Lloyd's algorithm for k-means clustering.
   data is an array of numeric feature vectors, k the cluster count, iters
   the number of refinement passes. Returns a struct with fields
   "centroids" (array of k feature vectors) and "assignments" (array of
   per-point cluster indices, 0-based). Centroids are seeded from the first
   k points, which is deterministic (not randomized) so a given input
   always reproduces the same result. */
static Value ai_kmeans(VM*vm,int n,Value*a){(void)vm;
    if(n!=3||a[0].t!=VARR||a[1].t!=VINT||a[2].t!=VINT)return vn();
    Arr*X=a[0].u.a; long long k=a[1].u.i, iters=a[2].u.i;
    size_t N=X->n; if(!N||k<1||(size_t)k>N||N>4096||iters<0||iters>1000)return vn();
    if(X->v[0].t!=VARR)return vn(); size_t d=X->v[0].u.a->n; if(!d||d>128)return vn();
    for(size_t i=0;i<N;i++){if(X->v[i].t!=VARR||X->v[i].u.a->n!=d)return vn();for(size_t j=0;j<d;j++)if(!isnum(X->v[i].u.a->v[j]))return vn();}
    double*cent=(double*)xmalloc((size_t)k*d*sizeof(double));
    for(long long c=0;c<k;c++)for(size_t j=0;j<d;j++)cent[c*d+j]=dn(X->v[c].u.a->v[j]);
    int*assign=(int*)xmalloc(N*sizeof(int));for(size_t i=0;i<N;i++)assign[i]=0;
    double*sum=(double*)xmalloc((size_t)k*d*sizeof(double));
    long long*cnt=(long long*)xmalloc((size_t)k*sizeof(long long));
    for(long long it=0;it<iters;it++){
        for(size_t i=0;i<N;i++){
            double best=-1;int bi=0;
            for(long long c=0;c<k;c++){double s=0;for(size_t j=0;j<d;j++){double diff=dn(X->v[i].u.a->v[j])-cent[c*d+j];s+=diff*diff;}if(best<0||s<best){best=s;bi=(int)c;}}
            assign[i]=bi;
        }
        memset(sum,0,(size_t)k*d*sizeof(double));memset(cnt,0,(size_t)k*sizeof(long long));
        for(size_t i=0;i<N;i++){int c=assign[i];cnt[c]++;for(size_t j=0;j<d;j++)sum[c*d+j]+=dn(X->v[i].u.a->v[j]);}
        for(long long c=0;c<k;c++)if(cnt[c]>0)for(size_t j=0;j<d;j++)cent[c*d+j]=sum[c*d+j]/(double)cnt[c];
    }
    Value out=vsobj();
    Value centroids=va();for(long long c=0;c<k;c++){Value row=va();for(size_t j=0;j<d;j++)ap(row.u.a,vf(cent[c*d+j]));ap(centroids.u.a,row);}
    Value assignments=va();for(size_t i=0;i<N;i++)ap(assignments.u.a,vi(assign[i]));
    stput(out.u.st,"centroids",centroids); stput(out.u.st,"assignments",assignments);
    xfree(cent);xfree(assign);xfree(sum);xfree(cnt);
    return out;
}
static Value nnuc_limits(VM*vm,int n,Value*a){if(n!=2||a[0].t!=VINT||a[1].t!=VINT||a[0].u.i<0||a[1].u.i<0)return vb(0);vm->cpu_limit_ms=a[0].u.i;vm->mem_limit_bytes=(size_t)a[1].u.i*1024u*1024u;g_mem_limit=vm->mem_limit_bytes;vm->memory_auto=0;return vb(1);}
static void apply_auto_memory(VM*vm);
static Value nnuc_reset(VM*vm,int n,Value*a){(void)a;if(n!=0)return vb(0);vm->cpu_limit_ms=0;vm->cpu_cores=0;vm->cpu_percent=100;vm->cpu_slice_wall_ms=0;vm->cpu_slice_cpu_ms=0;apply_auto_memory(vm);return vb(1);}
static Value nnuc_capabilities(VM*vm,int n,Value*a){(void)vm;(void)a;if(n!=0)return vn();Value o=va();ap(o.u.a,vs("cpu-limit"));ap(o.u.a,vs("ram-limit"));ap(o.u.a,vs("profiles"));ap(o.u.a,vs("vm-profiler"));return o;}
static Value nnuc_resource(VM*vm,int n,Value*a){
 if(n!=4||a[0].t!=VINT||a[1].t!=VINT||a[2].t!=VINT||a[3].t!=VINT)return vb(0);
 if(a[0].u.i<0||a[1].u.i<0||a[2].u.i<0||a[3].u.i<1||a[3].u.i>100)return vb(0);
 vm->cpu_limit_ms=a[0].u.i;vm->mem_limit_bytes=(size_t)a[1].u.i*1024u*1024u;vm->cpu_cores=(int)a[2].u.i;vm->cpu_percent=(int)a[3].u.i;vm->cpu_slice_wall_ms=0;vm->cpu_slice_cpu_ms=0;g_mem_limit=vm->mem_limit_bytes;if(!apply_cpu_affinity(vm))return vb(0);return vb(1);
}
static Value nnuc_set_cpu(VM*vm,int n,Value*a){if(n!=2||a[0].t!=VINT||a[1].t!=VINT||a[0].u.i<0||a[1].u.i<1||a[1].u.i>100)return vb(0);vm->cpu_cores=(int)a[0].u.i;vm->cpu_percent=(int)a[1].u.i;vm->cpu_slice_wall_ms=0;vm->cpu_slice_cpu_ms=0;if(!apply_cpu_affinity(vm))return vb(0);return vb(1);}
static Value nnuc_set_memory(VM*vm,int n,Value*a){if(n!=1||a[0].t!=VINT||a[0].u.i<0)return vb(0);vm->mem_limit_bytes=(size_t)a[0].u.i*1024u*1024u;g_mem_limit=vm->mem_limit_bytes;vm->memory_auto=0;return vb(1);}

static size_t system_memory_bytes(void){
#ifdef _WIN32
 MEMORYSTATUSEX ms;memset(&ms,0,sizeof ms);ms.dwLength=sizeof ms;if(GlobalMemoryStatusEx(&ms)){unsigned long long x=ms.ullTotalPhys;if(x>SIZE_MAX)x=SIZE_MAX;return (size_t)x;}return (size_t)512*1024*1024;
#else
 long pages=sysconf(_SC_PHYS_PAGES), page=sysconf(_SC_PAGESIZE);
 if(pages>0&&page>0){unsigned long long x=(unsigned long long)pages*(unsigned long long)page; if(x>SIZE_MAX)x=SIZE_MAX;return (size_t)x;}
 return (size_t)512*1024*1024;
#endif
}
static void apply_auto_memory(VM*vm){
 size_t phys=system_memory_bytes();
 /* Keep a safety reserve for the OS and native libraries. The limit is only
    for Haris-managed allocations; it is not an OS hard memory limit. */
 size_t lim=phys/2; if(lim<(size_t)128*1024*1024)lim=(size_t)128*1024*1024;
 if(lim>=(size_t)8*1024*1024*1024ULL)lim=(size_t)8*1024*1024*1024ULL;
 vm->mem_limit_bytes=lim;g_mem_limit=lim;vm->memory_auto=1;
}
static Value nnuc_auto_memory(VM*vm,int n,Value*a){(void)a;if(n!=0)return vb(0);apply_auto_memory(vm);return vb(1);}
static Value nuclear_gc(VM*vm,int n,Value*a){(void)a;if(n!=0)return vb(0);size_t before=g_mem_used;gc_collect(vm);return vi((long long)(before-g_mem_used));}

static Value njit_status(VM*vm,int n,Value*a){(void)vm;(void)a;if(n!=0)return vn();Value o=va();size_t blocks=0;for(JitBlock*b=g_jit_blocks;b;b=b->next)blocks++;ap(o.u.a,vi((long long)blocks));ap(o.u.a,vi(100));return o;}
static Value ntype(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();return vs(type_name(a[0]));}
static Value nis_type(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[1].t!=VSTR)return vb(0);return vb(!strcmp(type_name(a[0]),a[1].u.s));}
static Value nassert_type(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[1].t!=VSTR)return vb(0);if(strcmp(type_name(a[0]),a[1].u.s)&&!(a[0].t==VINT&&a[1].u.s&&strcmp(a[1].u.s,"float")==0))die("Haris type error: expected %s, got %s",a[1].u.s,type_name(a[0]));return vb(1);}
static Value nasync_join(VM*vm,int n,Value*a);
static void async_cleanup(AsyncTask*t){if(!t)return;if(t->args){xfree(t->args);t->args=NULL;}if(t->file){xfree(t->file);t->file=NULL;}t->fn=NULL;t->result=vn();}
static Value async_clone_value(Value v){if(v.t==VSTR)return vs(v.u.s);if(v.t==VARR){Value o=va();if(!arr_reserve(o.u.a,v.u.a->n))return vn();for(size_t i=0;i<v.u.a->n;i++)ap(o.u.a,async_clone_value(v.u.a->v[i]));return o;}if(v.t==VSTRUCT){Value o=vsobj();for(size_t i=0;i<v.u.st->n;i++)stput(o.u.st,v.u.st->v[i].name,async_clone_value(v.u.st->v[i].value));return o;}return v;}
static AsyncTask *async_task_new(void){if(g_task_n==g_task_cap){size_t nc=g_task_cap?g_task_cap*2:8;if(nc>SIZE_MAX/sizeof(*g_tasks))oom();g_tasks=xrealloc(g_tasks,nc*sizeof(*g_tasks));g_task_cap=nc;}AsyncTask*t=xmalloc(sizeof(*t));memset(t,0,sizeof*t);t->kind=HK_ASYNC;t->id=g_next_task++;g_tasks[g_task_n++]=t;return t;}
/* Async workers deliberately do not hold EXEC_LOCK while executing.  Each worker
   owns its VM, stack and argument copies; the managed heap has its own HEAP_LOCK.
   Completion is synchronized by join/WaitForSingleObject, so await never has to
   unlock a global execution mutex.  g_tasks is a GC root, keeping result Values
   alive until await/join consumes them. */
#ifdef _WIN32
static unsigned __stdcall async_worker_win(void*arg){AsyncTask*t=(AsyncTask*)arg;Fn*fn=t->fn;if(!fn&&t->file){FILE*f=fopen(t->file,"rb");if(f){fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);if(n>=0&&n<=16*1024*1024){char*b=xmalloc((size_t)n+1);if(fread(b,1,(size_t)n,f)==(size_t)n){b[n]=0;fn=compile(b,t->file);}xfree(b);}fclose(f);}}if(fn){VM child;init(&child);t->result=run(&child,fn,t->argc,t->args);}t->rc=fn?0:2;t->done=1;return 0;}
#else
static void *async_worker(void*arg){AsyncTask*t=(AsyncTask*)arg;Fn*fn=t->fn;if(!fn&&t->file){FILE*f=fopen(t->file,"rb");if(f){fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);if(n>=0&&n<=16*1024*1024){char*b=xmalloc((size_t)n+1);if(fread(b,1,(size_t)n,f)==(size_t)n){b[n]=0;fn=compile(b,t->file);}xfree(b);}fclose(f);}}if(fn){VM child;init(&child);t->result=run(&child,fn,t->argc,t->args);}t->rc=fn?0:2;t->done=1;return NULL;}
#endif
static Value nasync_spawn_fn(VM*vm,Fn*fn,int argc,Value*args){(void)vm;if(!fn||!fn->is_async||argc<0||argc>H_CALL_MAX)return vn();AsyncTask*t=async_task_new();t->fn=fn;t->argc=argc;if(argc){size_t z;if(!size_mul_ok((size_t)argc,sizeof(Value),&z))return vn();t->args=xmalloc(z);for(int i=0;i<argc;i++)t->args[i]=async_clone_value(args[i]);}
#ifdef _WIN32
typedef uintptr_t haris_th_t;haris_th_t h=(haris_th_t)_beginthreadex(NULL,0,async_worker_win,t,0,NULL);if(!h){t->rc=1;t->done=1;return vi(-1);}t->thread=(HANDLE)h;
#else
int rc=pthread_create(&t->thread,NULL,async_worker,t);if(rc){t->rc=rc;t->done=1;return vi(-1);}
#endif
t->started=1;return (Value){.t=VHANDLE,.u.handle=t};}
static Value nasync_spawn(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR||!safe_path_arg(a[0].u.s))return vi(-1);AsyncTask*t=async_task_new();t->file=xdup(a[0].u.s);
#ifdef _WIN32
typedef uintptr_t haris_th_t;haris_th_t h=(haris_th_t)_beginthreadex(NULL,0,async_worker_win,t,0,NULL);if(!h){t->rc=1;t->done=1;return vi(-1);}t->thread=(HANDLE)h;
#else
int rc=pthread_create(&t->thread,NULL,async_worker,t);if(rc){t->rc=rc;t->done=1;return vi(-1);}
#endif
t->started=1;return vi(t->id);}
static Value nasync_await(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();if(a[0].t==VHANDLE&&heap_is_ptr(a[0].u.handle)&&*((int*)a[0].u.handle)==HK_ASYNC){AsyncTask*t=a[0].u.handle;if(!t->started)return vi(t->rc);if(!t->joined){EXEC_UNLOCK();
#ifdef _WIN32
WaitForSingleObject(t->thread,INFINITE);
#else
pthread_join(t->thread,NULL);
#endif
EXEC_LOCK();t->joined=1;}Value r=t->result;async_cleanup(t);return r;}if(a[0].t==VINT)return nasync_join(vm,n,a);return vn();}
static Value nasync_join(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VINT)return vi(-1);for(size_t i=0;i<g_task_n;i++)if(g_tasks[i]&&g_tasks[i]->id==a[0].u.i){AsyncTask*t=g_tasks[i];if(!t->started)return vi(t->rc);if(!t->joined){EXEC_UNLOCK();
#ifdef _WIN32
WaitForSingleObject(t->thread,INFINITE);
#else
pthread_join(t->thread,NULL);
#endif
EXEC_LOCK();t->joined=1;}int rc=t->rc;async_cleanup(t);return vi(rc);}return vi(-1);}
static Value nasync_done(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);for(size_t i=0;i<g_task_n;i++){AsyncTask*t=g_tasks[i];if(!t)continue;int hit=(a[0].t==VINT&&t->id==a[0].u.i)||(a[0].t==VHANDLE&&a[0].u.handle==t);if(hit)return vb(t->done);}return vb(0);}
static Value ngpu_backends(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_HW_GPU))return cap_error(vm,CAP_HW_GPU,"gpu.backends");(void)a;if(n!=0)return vn();Value o=va();
#ifdef __linux__
void*h=dlopen("libvulkan.so.1",RTLD_LAZY);if(h){ap(o.u.a,vs("vulkan"));dlclose(h);}h=dlopen("libOpenCL.so",RTLD_LAZY);if(h){ap(o.u.a,vs("opencl"));dlclose(h);}h=dlopen("libcuda.so.1",RTLD_LAZY);if(h){ap(o.u.a,vs("cuda"));dlclose(h);}
#else
ap(o.u.a,vs("cpu-fallback"));
#endif
return o;}
static Value ngpu_available(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_HW_GPU))return cap_error(vm,CAP_HW_GPU,"gpu.available");if(n!=1||a[0].t!=VSTR)return vb(0);Value b=ngpu_backends(vm,0,NULL);for(size_t i=0;i<b.u.a->n;i++)if(!strcmp(b.u.a->v[i].u.s,a[0].u.s))return vb(1);return vb(0);}
static Value board_info(VM*vm,int n,Value*a){(void)a;if(!cap_allowed(vm,CAP_HW_BOARD))return cap_error(vm,CAP_HW_BOARD,"board.info");if(n)return vn();Value o=vsobj();stput(o.u.st,"raw_access",vb(0));stput(o.u.st,"mode",vs("broker-only"));stput(o.u.st,"platform",vs(
#ifdef _WIN32
"windows"
#elif defined(__APPLE__)
"macos"
#elif defined(__linux__)
"linux"
#elif defined(__FreeBSD__)
"freebsd"
#elif defined(__OpenBSD__)
"openbsd"
#else
"other"
#endif
));return o;}
static Value nsimd_add(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR||a[1].t!=VARR||a[0].u.a->n!=a[1].u.a->n)return vn();Value o=va();for(size_t i=0;i<a[0].u.a->n;i++){if(a[0].u.a->v[i].t!=VINT||a[1].u.a->v[i].t!=VINT)return vn();ap(o.u.a,vi(a[0].u.a->v[i].u.i+a[1].u.a->v[i].u.i));}return o;}
static Value math_clamp2(VM*vm,int n,Value*a){(void)vm;if(n!=3||!isnum(a[0])||!isnum(a[1])||!isnum(a[2]))return vn();double x=dn(a[0]),lo=dn(a[1]),hi=dn(a[2]);if(x<lo)x=lo;if(x>hi)x=hi;return vf(x);}
static Value math_approx2(VM*vm,int n,Value*a){(void)vm;if(n!=2||!isnum(a[0])||!isnum(a[1]))return vb(0);double x=dn(a[0]),y=dn(a[1]);return vb(fabs(x-y)<=1e-6*(1.0+fmax(fabs(x),fabs(y))));}

/* ================= Haris v2.1 Game / AI / Cyber toolkit ================= */
static Value obj_put2(const char *a,const char *b,Value va_,Value vb_){
    Value o=vsobj(); stput(o.u.st,a,va_); stput(o.u.st,b,vb_); return o;
}
static Value games_vec3(VM*vm,int n,Value*a){(void)vm;if(n!=3||!isnum(a[0])||!isnum(a[1])||!isnum(a[2]))return vn();Value o=va();ap(o.u.a,vf(dn(a[0])));ap(o.u.a,vf(dn(a[1])));ap(o.u.a,vf(dn(a[2])));return o;}
static Value games_move3(VM*vm,int n,Value*a){(void)vm;if(n!=7)return vn();for(int i=0;i<7;i++)if(!isnum(a[i]))return vn();double x=dn(a[0]),y=dn(a[1]),z=dn(a[2]),vx=dn(a[3]),vy=dn(a[4]),vz=dn(a[5]),dt=dn(a[6]);Value o=va();ap(o.u.a,vf(x+vx*dt));ap(o.u.a,vf(y+vy*dt));ap(o.u.a,vf(z+vz*dt));return o;}
static Value games_distance3(VM*vm,int n,Value*a){(void)vm;if(n!=6)return vn();for(int i=0;i<6;i++)if(!isnum(a[i]))return vn();double x=dn(a[3])-dn(a[0]),y=dn(a[4])-dn(a[1]),z=dn(a[5])-dn(a[2]);return vf(sqrt(x*x+y*y+z*z));}
static Value games_lerp_vec(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VARR||a[1].t!=VARR||!isnum(a[2])||a[0].u.a->n!=a[1].u.a->n)return vn();double t=dn(a[2]);if(t<0)t=0;if(t>1)t=1;Value o=va();for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i])||!isnum(a[1].u.a->v[i]))return vn();ap(o.u.a,vf(dn(a[0].u.a->v[i])+(dn(a[1].u.a->v[i])-dn(a[0].u.a->v[i]))*t));}return o;}
static Value games_stats(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR)return vn();double s=0,mn=INFINITY,mx=-INFINITY;long long c=0;for(size_t i=0;i<a[0].u.a->n;i++)if(isnum(a[0].u.a->v[i])){double x=dn(a[0].u.a->v[i]);s+=x;if(x<mn)mn=x;if(x>mx)mx=x;c++;}Value o=vsobj();stput(o.u.st,"count",vi(c));stput(o.u.st,"sum",vf(s));stput(o.u.st,"mean",vf(c?s/c:0));stput(o.u.st,"min",vf(c?mn:0));stput(o.u.st,"max",vf(c?mx:0));return o;}
static Value ai_prompt_template(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTR||a[1].t!=VSTRUCT)return vn();char*src=xdup(a[0].u.s);for(size_t i=0;i<a[1].u.st->n;i++){const char*k=a[1].u.st->v[i].name;Value v=a[1].u.st->v[i].value;if(v.t!=VSTR)continue;char token[256];snprintf(token,sizeof token,"{{%s}}",k);Value ar[3]={vs(src),vs(token),v};Value rr=nreplace(NULL,3,ar);xfree(ar[0].u.s);xfree(ar[1].u.s);xfree(src);if(rr.t!=VSTR)return vn();src=xdup(rr.u.s);xfree(rr.u.s);}Value r=vs(src);xfree(src);return r;}
static Value ai_rag_topk(VM*vm,int n,Value*a){
    (void)vm;if(n!=3||a[0].t!=VARR||a[1].t!=VARR||a[2].t!=VINT)return vn();long long k=a[2].u.i;if(k<1||k>128||!a[1].u.a->n||k>(long long)a[1].u.a->n)return vn();
    size_t N=a[1].u.a->n;size_t dim=a[0].u.a->n;if(!dim)return vn();double qn=0;for(size_t j=0;j<dim;j++){if(!isnum(a[0].u.a->v[j]))return vn();double x=dn(a[0].u.a->v[j]);qn+=x*x;}qn=sqrt(qn);if(!(qn>0))return vn();
    typedef struct {double s;size_t i;} RHit;RHit *heap=(RHit*)xmalloc((size_t)k*sizeof(*heap));if(!heap)return vn();size_t hn=0;
    #define RAG_SWAP(x,y) do{RHit _t=(x);(x)=(y);(y)=_t;}while(0)
    for(size_t i=0;i<N;i++){
        Value row=a[1].u.a->v[i];if(row.t!=VARR||row.u.a->n!=dim){xfree(heap);return vn();}double dot=0,rn=0;
        for(size_t j=0;j<dim;j++){if(!isnum(row.u.a->v[j])){xfree(heap);return vn();}double x=dn(a[0].u.a->v[j]),y=dn(row.u.a->v[j]);dot+=x*y;rn+=y*y;}rn=sqrt(rn);double score=(rn>0)?dot/(qn*rn):0;
        if(hn<(size_t)k){heap[hn]=(RHit){score,i};size_t z=hn++;while(z){size_t par=(z-1)/2;if(heap[par].s<=heap[z].s)break;RAG_SWAP(heap[par],heap[z]);z=par;}}
        else if(score>heap[0].s){heap[0]=(RHit){score,i};size_t z=0;for(;;){size_t l=z*2+1,r=l+1,m=z;if(l<hn&&heap[l].s<heap[m].s)m=l;if(r<hn&&heap[r].s<heap[m].s)m=r;if(m==z)break;RAG_SWAP(heap[z],heap[m]);z=m;}}
    }
    for(size_t i=1;i<hn;i++){RHit x=heap[i];size_t j=i;while(j&&heap[j-1].s<x.s){heap[j]=heap[j-1];j--;}heap[j]=x;}
    Value out=va();for(size_t i=0;i<hn;i++){Value o=vsobj();stput(o.u.st,"index",vi((long long)heap[i].i));stput(o.u.st,"score",vf(heap[i].s));ap(out.u.a,o);}xfree(heap);return out;
    #undef RAG_SWAP
}
static Value ai_token_count(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();size_t c=0,in=0;for(const unsigned char*p=(const unsigned char*)a[0].u.s;*p;p++){if(isspace(*p)){in=0;}else if(!in){c++;in=1;}}return vi((long long)c);}
static int hexv(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static Value sec_hash_text(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();EVP_MD_CTX*c=EVP_MD_CTX_new();if(!c)return vn();unsigned char h[EVP_MAX_MD_SIZE];unsigned int hn=0;EVP_DigestInit_ex(c,EVP_sha256(),NULL);EVP_DigestUpdate(c,a[0].u.s,strlen(a[0].u.s));int ok=EVP_DigestFinal_ex(c,h,&hn)==1;EVP_MD_CTX_free(c);if(!ok)return vn();char out[65];for(unsigned int i=0;i<hn;i++)sprintf(out+i*2,"%02x",h[i]);out[64]=0;return vs(out);}
/* Defensive: estimate password strength from character-class variety and
   length (a Shannon-style pool-size entropy estimate, not a breach-list
   lookup). No capability needed - purely local computation, same as
   security.hash_text. */
static Value security_password_strength(VM*vm,int n,Value*a){
    (void)vm;if(n!=1||a[0].t!=VSTR)return vn();
    const char*s=a[0].u.s;size_t len=strlen(s);
    int has_lower=0,has_upper=0,has_digit=0,has_symbol=0;
    for(size_t i=0;i<len;i++){unsigned char c=(unsigned char)s[i];if(islower(c))has_lower=1;else if(isupper(c))has_upper=1;else if(isdigit(c))has_digit=1;else has_symbol=1;}
    double pool=0;if(has_lower)pool+=26;if(has_upper)pool+=26;if(has_digit)pool+=10;if(has_symbol)pool+=33;
    double entropy=(pool>0&&len>0)?((double)len*log2(pool)):0;
    const char*verdict=(len<8||entropy<28)?"very weak":(entropy<36)?"weak":(entropy<60)?"moderate":(entropy<80)?"strong":"very strong";
    Value o=vsobj();
    stput(o.u.st,"length",vi((long long)len));stput(o.u.st,"entropy_bits",vf(entropy));
    stput(o.u.st,"has_lower",vb(has_lower));stput(o.u.st,"has_upper",vb(has_upper));stput(o.u.st,"has_digit",vb(has_digit));stput(o.u.st,"has_symbol",vb(has_symbol));
    stput(o.u.st,"verdict",vs(verdict));
    return o;
}
/* Offensive/audit: dictionary attack against a hash you already hold (e.g.
   auditing your own exported password hashes for weak entries). Purely
   local hash comparisons against a caller-supplied wordlist - never
   touches a live system. Returns the matching candidate string, or null. */
static Value security_crack_hash(VM*vm,int n,Value*a){
    (void)vm;if(n!=3||a[0].t!=VSTR||a[1].t!=VARR||a[2].t!=VSTR)return vn();
    const EVP_MD*md;
    if(!strcasecmp(a[2].u.s,"sha256"))md=EVP_sha256();
    else if(!strcasecmp(a[2].u.s,"sha1"))md=EVP_sha1();
    else if(!strcasecmp(a[2].u.s,"md5"))md=EVP_md5();
    else return vn();
    if(a[1].u.a->n>200000)return vn();
    for(size_t i=0;i<a[1].u.a->n;i++){
        if(a[1].u.a->v[i].t!=VSTR)continue;
        const char*cand=a[1].u.a->v[i].u.s;
        EVP_MD_CTX*c=EVP_MD_CTX_new();if(!c)return vn();
        unsigned char h[EVP_MAX_MD_SIZE];unsigned int hn=0;
        EVP_DigestInit_ex(c,md,NULL);EVP_DigestUpdate(c,cand,strlen(cand));int ok=EVP_DigestFinal_ex(c,h,&hn)==1;EVP_MD_CTX_free(c);
        if(!ok)continue;
        char hex[EVP_MAX_MD_SIZE*2+1];for(unsigned int j=0;j<hn;j++)sprintf(hex+j*2,"%02x",h[j]);hex[hn*2]=0;
        if(!strcasecmp(hex,a[0].u.s))return vs(cand);
    }
    return vn();
}
static Value sec_file_hash(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"security.file_hash");if(n!=1||a[0].t!=VSTR||!safe_path_arg(a[0].u.s))return vn();char h[65];return sha256_file(a[0].u.s,h)?vs(h):vn();}
static Value sec_file_entropy(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"security.file_entropy");if(n!=1||a[0].t!=VSTR||!safe_path_arg(a[0].u.s))return vn();FILE*f=fopen(a[0].u.s,"rb");if(!f)return vn();unsigned long long freq[256]={0},total=0;unsigned char buf[8192];size_t z;while((z=fread(buf,1,sizeof buf,f))>0){for(size_t i=0;i<z;i++)freq[buf[i]]++;total+=z;if(total>8ULL*1024*1024)break;}fclose(f);if(!total)return vf(0);double H=0;for(int i=0;i<256;i++)if(freq[i]){double p=(double)freq[i]/(double)total;H-=p*(log(p)/log(2.0));}return vf(H);}
static Value sec_scan_source(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"security.scan_source");if(n!=1||a[0].t!=VSTR)return vn();char*src=readf_limit(a[0].u.s,4ULL*1024*1024);if(!src)return vn();char*norm=sec_normalize(src);xfree(src);Value hits=va();struct Sig{const char*pat;const char*id;const char*sev;}sig[]={{"strcpy[[:space:]]*[(]","unsafe-copy","high"},{"strcat[[:space:]]*[(]","unsafe-concat","high"},{"gets[[:space:]]*[(]","gets","critical"},{"system[[:space:]]*[(]","shell-exec","high"},{"popen[[:space:]]*[(]","process-pipe","high"},{"eval[[:space:]]*[(]","dynamic-eval","medium"},{"rm[[:space:]]*-[[:space:]]*rf","destructive-delete","critical"},{"powershell[[:space:]]+-enc","encoded-shell","high"},{"curl_easy_setopt[[:space:]]*[(]","network-client","info"}};for(size_t i=0;i<sizeof(sig)/sizeof(sig[0]);i++)if(sec_re(sig[i].pat,norm)){Value o=vsobj();stput(o.u.st,"id",vs(sig[i].id));stput(o.u.st,"severity",vs(sig[i].sev));stput(o.u.st,"pattern",vs(sig[i].pat));ap(hits.u.a,o);}xfree(norm);Value r=vsobj();stput(r.u.st,"findings",hits);stput(r.u.st,"count",vi((long long)hits.u.a->n));stput(r.u.st,"normalized",vb(1));return r;}
static Value sec_log_analyze(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"security.log_analyze");if(n!=1||a[0].t!=VSTR)return vn();char*src=readf_limit(a[0].u.s,8ULL*1024*1024);if(!src)return vn();long long lines=0,err=0,warn=0,auth=0;for(char*p=src;*p;){lines++;char*e=strchr(p,'\n');if(e)*e=0;char*q=xdup(p);for(char*t=q;*t;t++)*t=(char)tolower((unsigned char)*t);if(strstr(q,"error"))err++;if(strstr(q,"warn"))warn++;if(strstr(q,"failed")||strstr(q,"unauthorized")||strstr(q,"authentication"))auth++;xfree(q);if(!e)break;p=e+1;}xfree(src);Value r=vsobj();stput(r.u.st,"lines",vi(lines));stput(r.u.st,"errors",vi(err));stput(r.u.st,"warnings",vi(warn));stput(r.u.st,"auth_events",vi(auth));return r;}
static Value sec_private_target(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vb(0);const char*s=a[0].u.s;unsigned a1,a2,a3,a4; if(sscanf(s,"%u.%u.%u.%u",&a1,&a2,&a3,&a4)!=4||a1>255||a2>255||a3>255||a4>255)return vb(0);return vb(a1==10||a1==127||(a1==192&&a2==168)||(a1==172&&a2>=16&&a2<=31));}
static Value sec_lab_scenario(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();Value o=vsobj();stput(o.u.st,"mode",vs("simulation-only"));stput(o.u.st,"authorized",vb(1));stput(o.u.st,"scenario",a[0]);if(!strcmp(a[0].u.s,"phishing")){stput(o.u.st,"signal",vs("suspicious-link"));stput(o.u.st,"recommended_control",vs("URL filtering + user reporting"));}else if(!strcmp(a[0].u.s,"credential_stuffing")){stput(o.u.st,"signal",vs("repeated authentication failures"));stput(o.u.st,"recommended_control",vs("rate limits + MFA + lockout policy"));}else if(!strcmp(a[0].u.s,"ransomware")){stput(o.u.st,"signal",vs("rapid file modification"));stput(o.u.st,"recommended_control",vs("backup isolation + endpoint detection"));}else{stput(o.u.st,"signal",vs("unknown scenario"));stput(o.u.st,"recommended_control",vs("collect telemetry and investigate"));}return o;}
static Value sec_ioc_extract(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();const char*s=a[0].u.s;Value out=va();char tok[256];size_t z=0;for(const char*p=s;;p++){int end=!*p||isspace((unsigned char)*p)||strchr("\"'(),[]{}<>",*p)!=NULL;if(!end){if(z<sizeof(tok)-1)tok[z++]=*p;continue;}if(z){tok[z]=0;int dot=0,digit=1;for(size_t i=0;i<z;i++){if(tok[i]=='.')dot++;if(!isdigit((unsigned char)tok[i])&&tok[i]!='.')digit=0;}int ip=dot==3&&digit;int url=ci_prefix(tok,"http://")||ci_prefix(tok,"https://");if(ip||url)ap(out.u.a,vs(tok));z=0;}if(!*p)break;}return out;}


