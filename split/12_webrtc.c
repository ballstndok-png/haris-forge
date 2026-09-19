/* ---------------- WebRTC / libdatachannel ---------------- */
#define HK_RTC_PC 0x52544350
#define HK_RTC_DC 0x52544344
typedef struct HRtcPC { int kind; int pc; } HRtcPC;
typedef struct HRtcDC { int kind; int dc; } HRtcDC;
static HRtcPC*rtc_pc_handle(Value v){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==HK_RTC_PC?(HRtcPC*)v.u.handle:NULL;}
static HRtcDC*rtc_dc_handle(Value v){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==HK_RTC_DC?(HRtcDC*)v.u.handle:NULL;}
static void rtc_pc_dtor(void*p){HRtcPC*x=p;if(!x)return;
#ifdef HARIS_HAVE_WEBRTC
    if(x->pc>0){rtcClosePeerConnection(x->pc);rtcDeletePeerConnection(x->pc);}
#endif
    xfree(x);}
static void rtc_dc_dtor(void*p){HRtcDC*x=p;if(!x)return;
#ifdef HARIS_HAVE_WEBRTC
    if(x->dc>0)rtcDeleteDataChannel(x->dc);
#endif
    xfree(x);}
static Value webrtc_available(VM*vm,int n,Value*a){(void)vm;(void)a;if(n!=0)return vn();
#ifdef HARIS_HAVE_WEBRTC
    return vb(1);
#else
    return vb(0);
#endif
}
static Value webrtc_peer(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"webrtc.peer");if(n>1)return vn();
#ifndef HARIS_HAVE_WEBRTC
    return vn();
#else
    rtcConfiguration cfg;memset(&cfg,0,sizeof cfg);const char*ice[32];int ice_n=0;
    if(n==1&&a[0].t==VARR){for(size_t i=0;i<a[0].u.a->n&&ice_n<32;i++){if(a[0].u.a->v[i].t!=VSTR)continue;ice[ice_n++]=a[0].u.a->v[i].u.s;}}
    cfg.iceServers=ice_n?ice:NULL;cfg.iceServersCount=ice_n;cfg.disableAutoNegotiation=false;int pc=rtcCreatePeerConnection(&cfg);if(pc<0)return vn();HRtcPC*x=(HRtcPC*)xmalloc_dtor(sizeof(*x),rtc_pc_dtor);memset(x,0,sizeof*x);x->kind=HK_RTC_PC;x->pc=pc;return (Value){.t=VHANDLE,.u.handle=x};
#endif
}
static Value webrtc_desc(int n,Value*a,int answer){
    (void)answer;return vn();
}
static Value webrtc_offer(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();HRtcPC*x=rtc_pc_handle(a[0]);if(!x)return vn();
#ifdef HARIS_HAVE_WEBRTC
    if(rtcSetLocalDescription(x->pc,"offer")<0)return vn();int z=rtcGetLocalDescription(x->pc,NULL,0);if(z<=0||z>262144)return vn();char*b=(char*)xmalloc((size_t)z);if(rtcGetLocalDescription(x->pc,b,z)<0){xfree(b);return vn();}Value r=vs(b);xfree(b);return r;
#else
    return vn();
#endif
}
static Value webrtc_answer(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();HRtcPC*x=rtc_pc_handle(a[0]);if(!x)return vn();
#ifdef HARIS_HAVE_WEBRTC
    if(rtcSetLocalDescription(x->pc,"answer")<0)return vn();int z=rtcGetLocalDescription(x->pc,NULL,0);if(z<=0||z>262144)return vn();char*b=(char*)xmalloc((size_t)z);if(rtcGetLocalDescription(x->pc,b,z)<0){xfree(b);return vn();}Value r=vs(b);xfree(b);return r;
#else
    return vn();
#endif
}
static Value webrtc_set_remote(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3)return vb(0);HRtcPC*x=rtc_pc_handle(a[0]);if(!x||a[1].t!=VSTR)return vb(0);
#ifdef HARIS_HAVE_WEBRTC
    const char*ty=(n==3&&a[2].t==VSTR)?a[2].u.s:NULL;return vb(rtcSetRemoteDescription(x->pc,a[1].u.s,ty)==0);
#else
    return vb(0);
#endif
}
static Value webrtc_candidate(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3)return vb(0);HRtcPC*x=rtc_pc_handle(a[0]);if(!x||a[1].t!=VSTR)return vb(0);
#ifdef HARIS_HAVE_WEBRTC
    const char*mid=(n==3&&a[2].t==VSTR)?a[2].u.s:NULL;return vb(rtcAddRemoteCandidate(x->pc,a[1].u.s,mid)==0);
#else
    return vb(0);
#endif
}
static Value webrtc_data_channel(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[1].t!=VSTR)return vn();HRtcPC*x=rtc_pc_handle(a[0]);if(!x)return vn();
#ifdef HARIS_HAVE_WEBRTC
    int dc=rtcCreateDataChannel(x->pc,a[1].u.s);if(dc<0)return vn();HRtcDC*d=(HRtcDC*)xmalloc_dtor(sizeof(*d),rtc_dc_dtor);memset(d,0,sizeof*d);d->kind=HK_RTC_DC;d->dc=dc;return (Value){.t=VHANDLE,.u.handle=d};
#else
    return vn();
#endif
}
static Value webrtc_send(VM*vm,int n,Value*a){(void)vm;if(n!=2)return vb(0);HRtcDC*d=rtc_dc_handle(a[0]);if(!d)return vb(0);
#ifdef HARIS_HAVE_WEBRTC
    unsigned char*buf=NULL;size_t z=0;if(!net_value_bytes_copy(a[1],&buf,&z))return vb(0);if(z>INT_MAX){xfree(buf);return vb(0);}int rc=rtcSendMessage(d->dc,(const char*)buf,(int)z);xfree(buf);return vb(rc==0);
#else
    return vb(0);
#endif
}
static Value webrtc_recv(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();HRtcDC*d=rtc_dc_handle(a[0]);if(!d)return vn();
#ifdef HARIS_HAVE_WEBRTC
    int avail=rtcGetAvailableAmount(d->dc);if(avail<0||avail>16*1024*1024)return vn();int cap=avail>0?avail:1;char*b=(char*)xmalloc((size_t)cap+1);int sz=cap;int rc=rtcReceiveMessage(d->dc,b,&sz);if(rc!=0){xfree(b);return vn();}Value r;if(sz<0){int z=-sz;r=vs(b);(void)z;}else r=net_bytes_array((unsigned char*)b,(size_t)sz);xfree(b);return r;
#else
    return vn();
#endif
}
static Value webrtc_info(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();Value o=vsobj();HRtcPC*p=rtc_pc_handle(a[0]);HRtcDC*d=rtc_dc_handle(a[0]);
#ifdef HARIS_HAVE_WEBRTC
    stput(o.u.st,"available",vb(1));if(p){stput(o.u.st,"type",vs("peer"));stput(o.u.st,"id",vi(p->pc));stput(o.u.st,"negotiation_needed",vb(rtcIsNegotiationNeeded(p->pc)));}else if(d){stput(o.u.st,"type",vs("datachannel"));stput(o.u.st,"id",vi(d->dc));stput(o.u.st,"open",vb(rtcIsOpen(d->dc)));stput(o.u.st,"buffered",vi(rtcGetBufferedAmount(d->dc)));}else return vn();return o;
#else
    stput(o.u.st,"available",vb(0));stput(o.u.st,"backend",vs("libdatachannel not compiled in"));return o;
#endif
}
static Value webrtc_close(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HRtcDC*d=rtc_dc_handle(a[0]);HRtcPC*p=rtc_pc_handle(a[0]);
#ifdef HARIS_HAVE_WEBRTC
    if(d){rtcClose(d->dc);return vb(rtcDeleteDataChannel(d->dc)==0);}if(p){rtcClosePeerConnection(p->pc);return vb(1);}return vb(0);
#else
    return vb(0);
#endif
}

static int ci_prefix(const char*s,const char*p){while(*p&&*s&&tolower((unsigned char)*s)==tolower((unsigned char)*p)){s++;p++;}return !*p;}
static Value nwebsafe(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vb(0);const unsigned char*s=(const unsigned char*)a[0].u.s;while(*s&&isspace(*s))s++;for(const unsigned char*p=s;*p;p++)if(*p<32||*p==127)return vb(0);if(!(ci_prefix((const char*)s,"http://")||ci_prefix((const char*)s,"https://")))return vb(0);if(strstr((const char*)s,"..")!=NULL)return vb(0);return vb(1); }
static Value nput(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.upload");if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VSTR)return vb(0);CURL*c=curl_easy_init();if(!c)return vb(0);FILE*f=fopen(a[1].u.s,"rb");if(!f){curl_easy_cleanup(c);return vb(0);}struct curl_slist*h=0;if(n==3&&a[2].t==VSTR){char auth[1024];int aw=snprintf(auth,sizeof auth,"Authorization: Bearer %s",a[2].u.s);if(aw<0||(size_t)aw>=sizeof auth){fclose(f);curl_easy_cleanup(c);return vb(0);}h=curl_slist_append(h,auth);}curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_URL,a[0].u.s);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_UPLOAD,1L);curl_easy_setopt(c,CURLOPT_READDATA,f);curl_easy_setopt(c,CURLOPT_TIMEOUT,120L);if(h)curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);CURLcode rc=curl_easy_perform(c);if(h)curl_slist_free_all(h);fclose(f);curl_easy_cleanup(c);return vb(rc==CURLE_OK);}

/* Counterpart to cloud.upload: streams a URL straight to a local file
   instead of buffering the whole response in memory. */
static Value cloud_download(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.download");
    if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VSTR)return vb(0);
    CURL*c=curl_easy_init();if(!c)return vb(0);
    FILE*f=fopen(a[1].u.s,"wb");if(!f){curl_easy_cleanup(c);return vb(0);}
    struct curl_slist*h=0;
    if(n==3&&a[2].t==VSTR){char auth[1024];int aw=snprintf(auth,sizeof auth,"Authorization: Bearer %s",a[2].u.s);if(aw<0||(size_t)aw>=sizeof auth){fclose(f);curl_easy_cleanup(c);return vb(0);}h=curl_slist_append(h,auth);}
    CurlOut outbuf={f,0};
    curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_URL,a[0].u.s);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(c,CURLOPT_MAXREDIRS,5L);curl_easy_setopt(c,CURLOPT_TIMEOUT,120L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,cw);curl_easy_setopt(c,CURLOPT_WRITEDATA,&outbuf);
    if(h)curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);
    CURLcode rc=curl_easy_perform(c);
    if(h)curl_slist_free_all(h);
    fclose(f);curl_easy_cleanup(c);
    return vb(rc==CURLE_OK);
}
/* Generic HTTP verb client for cloud/REST APIs: any method, an optional
   string body, and an optional array of raw "Key: Value" header strings.
   Returns [status_code, body]. This is the primitive cloud.put_json and
   cloud.delete are built on. */
static Value cloud_request(VM*vm,int n,Value*a){if(n<2||n>5)return vn();return cloud_http_fast(vm,n,a);}
static Value cloud_put_json(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.put_json");
    if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VSTR)return vn();
    Value hdrs=va();ap(hdrs.u.a,vs("Content-Type: application/json"));
    if(n==3&&a[2].t==VSTR){char auth[1024];int aw=snprintf(auth,sizeof auth,"Authorization: Bearer %s",a[2].u.s);if(aw>0&&(size_t)aw<sizeof auth)ap(hdrs.u.a,vs(auth));}
    Value args[4]={vs("PUT"),a[0],a[1],hdrs};
    return cloud_request(vm,4,args);
}
static Value cloud_delete(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.delete");
    if(n<1||n>2||a[0].t!=VSTR)return vn();
    Value hdrs=va();
    if(n==2&&a[1].t==VSTR){char auth[1024];int aw=snprintf(auth,sizeof auth,"Authorization: Bearer %s",a[1].u.s);if(aw>0&&(size_t)aw<sizeof auth)ap(hdrs.u.a,vs(auth));}
    Value args[4]={vs("DELETE"),a[0],vn(),hdrs};
    return cloud_request(vm,4,args);
}

/* ===================== Haris v2.0 AI-native runtime ===================== */
#ifndef HARIS_MEMBUF_MAX
#define HARIS_MEMBUF_MAX (256ULL*1024ULL*1024ULL)
#endif
typedef struct { int kind; sqlite3 *db; char *path; } HMemory;
typedef struct { int kind; unsigned char *data; size_t size; int owned; } HMemBuf;
static void hmemory_dtor(void*p){HMemory*m=(HMemory*)p;if(m&&m->db)sqlite3_close_v2(m->db);if(m&&m->path){xfree(m->path);m->path=NULL;}}
static void membuf_dtor(void*p){HMemBuf*m=(HMemBuf*)p;if(m&&m->owned&&m->data){xfree(m->data);m->data=NULL;}}
typedef struct { int kind; char *endpoint; char *key; char *model; char *system; Value tools; int max_steps; } HAgent;
static void hagent_dtor(void*p){HAgent*g=(HAgent*)p;if(!g)return;if(g->endpoint){xfree(g->endpoint);g->endpoint=NULL;}if(g->key){xfree(g->key);g->key=NULL;}if(g->model){xfree(g->model);g->model=NULL;}if(g->system){xfree(g->system);g->system=NULL;}g->tools=vn();g->kind=HK_CLOSED;}
#define HK_MEMORY 0x4D454D
#define HK_AGENT  0x414745

static int handle_kind(Value v,int k){return v.t==VHANDLE&&v.u.handle&&heap_is_ptr(v.u.handle)&&*((int*)v.u.handle)==k;}

static Value ai_prompt(VM*vm,int n,Value*a){
    (void)vm; if(n!=1||a[0].t!=VSTRUCT)return vn();
    Value out=vsobj(), msgs=va();
    Value system=stget(a[0].u.st,"system");
    Value user=stget(a[0].u.st,"user");
    Value assistant=stget(a[0].u.st,"assistant");
    if(system.t==VSTR)ap(msgs.u.a,ai_message_common(1,&system,"system"));
    if(user.t==VSTR)ap(msgs.u.a,ai_message_common(1,&user,"user"));
    if(assistant.t==VSTR)ap(msgs.u.a,ai_message_common(1,&assistant,"assistant"));
    stput(out.u.st,"messages",msgs); stput(out.u.st,"temperature",vf(0.7));
    return out;
}
static Value ai_cosine(VM*vm,int n,Value*a){
    (void)vm; if(n!=2||a[0].t!=VARR||a[1].t!=VARR||a[0].u.a->n!=a[1].u.a->n||!a[0].u.a->n)return vn();
    double dot=0,aa=0,bb=0; for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i])||!isnum(a[1].u.a->v[i]))return vn();double x=dn(a[0].u.a->v[i]),y=dn(a[1].u.a->v[i]);dot+=x*y;aa+=x*x;bb+=y*y;}return vf((aa>0&&bb>0)?dot/(sqrt(aa)*sqrt(bb)):0);
}
static uint64_t ai_fnv1a(const unsigned char *s,size_t n){uint64_t h=1469598103934665603ULL;for(size_t i=0;i<n;i++){h^=s[i];h*=1099511628211ULL;}return h;}
static uint64_t ai_mix64(uint64_t x){
    x+=UINT64_C(0x9e3779b97f4a7c15);x=(x^(x>>30))*UINT64_C(0xbf58476d1ce4e5b9);x=(x^(x>>27))*UINT64_C(0x94d049bb133111eb);return x^(x>>31);
}
static void ai_hash_add(double *v,int d,uint64_t h,double w){size_t i=(size_t)(h%(uint64_t)d);v[i]+=((h>>63)?-w:w);}
static Value ai_hash_embed(VM*vm,int n,Value*a){
    (void)vm;if(n<1||n>2||a[0].t!=VSTR)return vn();int d=n==2&&a[1].t==VINT?(int)a[1].u.i:128;if(d<8||d>8192)return vn();
    Value out=va();for(int i=0;i<d;i++)ap(out.u.a,vf(0));
    double *v=(double*)xmalloc((size_t)d*sizeof(double));if(!v)return vn();memset(v,0,(size_t)d*sizeof(double));
    const unsigned char *s=(const unsigned char*)a[0].u.s;size_t len=strlen(a[0].u.s);size_t i=0;uint64_t prev=0;int have_prev=0,features=0;
    while(i<len){
        while(i<len && isspace(s[i]))i++; if(i>=len)break; size_t st=i;
        while(i<len && !isspace(s[i]))i++; size_t wl=i-st; if(!wl)continue;
        uint64_t h=1469598103934665603ULL;size_t al=0;
        for(size_t j=0;j<wl;j++){unsigned char c=s[st+j];if(c>='A'&&c<='Z')c=(unsigned char)(c-'A'+'a');if(isalnum(c)||c>=128){h^=c;h*=1099511628211ULL;al++;}}
        if(al){
            uint64_t word_ns=h^UINT64_C(0x6a09e667f3bcc909);
            ai_hash_add(v,d,ai_mix64(word_ns),1.0);features++;
            if(have_prev){uint64_t bigram_ns=prev^h^UINT64_C(0x3c6ef372fe94f82b);ai_hash_add(v,d,ai_mix64(bigram_ns),0.65);}
        }
        for(size_t j=0;j+1<wl;j++){unsigned char c1=s[st+j],c2=s[st+j+1];if(c1>='A'&&c1<='Z')c1=(unsigned char)(c1-'A'+'a');if(c2>='A'&&c2<='Z')c2=(unsigned char)(c2-'A'+'a');uint64_t bh=((uint64_t)c1<<8)|c2;uint64_t char_ns=bh^h^UINT64_C(0xbb67ae8584caa73b);ai_hash_add(v,d,ai_mix64(char_ns),0.35);}
        prev=h;have_prev=1;
    }
    if(!features && len){ai_hash_add(v,d,ai_mix64(ai_fnv1a(s,len)),1.0);features=1;}
    double ss=0;for(int j=0;j<d;j++)ss+=v[j]*v[j];double inv=ss>1e-30?1.0/sqrt(ss):0;
    for(int j=0;j<d;j++)out.u.a->v[j]=vf(v[j]*inv);xfree(v);return out;
}
static char* ai_json_body_messages(Value msgs,const char*model){HJsonBuf b={0};if(!hjson_puts(&b,"{\"messages\":")){xfree(b.d);return NULL;}if(!hjson_value(&b,msgs,0)){xfree(b.d);return NULL;}if(model&&*model){if(!hjson_puts(&b,",\"model\":")){xfree(b.d);return NULL;}if(!hjson_string(&b,model)){xfree(b.d);return NULL;}}if(!hjson_putc(&b,'}')){xfree(b.d);return NULL;}return b.d;}
static char* ai_http_post_json(const char*url,const char*key,const char*body,long *status){
    CURLM*multi=web_multi_get();if(!multi)return NULL;
    CURL*c=curl_easy_init();if(!c)return NULL;HWebBuf hb={0};struct curl_slist*h=NULL;char auth[1024];
    if(key&&*key){int aw=snprintf(auth,sizeof auth,"Authorization: Bearer %s",key);if(aw<0||(size_t)aw>=sizeof auth){curl_easy_cleanup(c);return NULL;}h=curl_slist_append(h,auth);}
    h=curl_slist_append(h,"Content-Type: application/json");
    curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_POST,1L);curl_easy_setopt(c,CURLOPT_POSTFIELDS,body);curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,hweb_write);curl_easy_setopt(c,CURLOPT_WRITEDATA,&hb);curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,180000L);
    CURLMcode add=curl_multi_add_handle(multi,c);if(add!=CURLM_OK){curl_slist_free_all(h);curl_easy_cleanup(c);hweb_free(&hb);return NULL;}
    int running=0;CURLMcode mc=curl_multi_perform(multi,&running);while(mc==CURLM_CALL_MULTI_PERFORM)mc=curl_multi_perform(multi,&running);if(mc==CURLM_OK)web_multi_wait(multi,&running);
    CURLcode rc=CURLE_FAILED_INIT;int q=0;CURLMsg*msg=NULL;while((msg=curl_multi_info_read(multi,&q))){if(msg->msg==CURLMSG_DONE&&msg->easy_handle==c){rc=msg->data.result;break;}}
    long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);if(status)*status=code;
    char*r=NULL;if(rc==CURLE_OK&&!hb.overflow)r=xdup(hb.p?hb.p:"");
    else if(rc!=CURLE_OK)r=xdup(curl_easy_strerror(rc));
    curl_multi_remove_handle(multi,c);curl_slist_free_all(h);curl_easy_cleanup(c);hweb_free(&hb);return r;
}
static Value ai_embed_server(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"ai.embed");if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VSTR||(n==3&&a[2].t!=VSTR))return vn();
    HJsonBuf b={0};hjson_puts(&b,"{\"input\":");hjson_string(&b,a[1].u.s);if(n==3){hjson_puts(&b,",\"model\":");hjson_string(&b,a[2].u.s);}hjson_putc(&b,'}');long st=0;char*r=ai_http_post_json(a[0].u.s,NULL,b.d,&st);xfree(b.d);Value o=vsobj();stput(o.u.st,"status",vi(st));stput(o.u.st,"body",vs(r?r:""));if(r)xfree(r);return o;
}
static Value memory_open(VM*vm,int n,Value*a){
    (void)vm;if(n!=1||a[0].t!=VSTR)return vn();sqlite3*db=NULL;if(sqlite3_open(a[0].u.s,&db)!=SQLITE_OK){if(db)sqlite3_close(db);return vn();}
    const char*sql="CREATE TABLE IF NOT EXISTS memories (key TEXT PRIMARY KEY, value TEXT NOT NULL, tags TEXT DEFAULT '', updated INTEGER NOT NULL)";if(sqlite3_exec(db,sql,NULL,NULL,NULL)!=SQLITE_OK){sqlite3_close(db);return vn();}
    HMemory*m=xmalloc_dtor(sizeof(*m),hmemory_dtor);m->kind=HK_MEMORY;m->db=db;m->path=xdup(a[0].u.s);return (Value){.t=VHANDLE,.u.handle=m};
}
static Value memory_put(VM*vm,int n,Value*a){(void)vm;if(n<3||n>4||!handle_kind(a[0],HK_MEMORY)||a[1].t!=VSTR||a[2].t!=VSTR)return vb(0);HMemory*m=a[0].u.handle;const char*tags=(n==4&&a[3].t==VSTR)?a[3].u.s:"";sqlite3_stmt*st=NULL;const char*sql="INSERT INTO memories(key,value,tags,updated) VALUES(?1,?2,?3,strftime('%s','now')) ON CONFLICT(key) DO UPDATE SET value=excluded.value,tags=excluded.tags,updated=excluded.updated";if(sqlite3_prepare_v2(m->db,sql,-1,&st,NULL)!=SQLITE_OK)return vb(0);sqlite3_bind_text(st,1,a[1].u.s,-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,a[2].u.s,-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,3,tags,-1,SQLITE_TRANSIENT);int ok=sqlite3_step(st)==SQLITE_DONE;sqlite3_finalize(st);return vb(ok);}
static Value memory_get(VM*vm,int n,Value*a){(void)vm;if(n!=2||!handle_kind(a[0],HK_MEMORY)||a[1].t!=VSTR)return vn();HMemory*m=a[0].u.handle;sqlite3_stmt*st=NULL;if(sqlite3_prepare_v2(m->db,"SELECT value FROM memories WHERE key=?1",-1,&st,NULL)!=SQLITE_OK)return vn();sqlite3_bind_text(st,1,a[1].u.s,-1,SQLITE_TRANSIENT);Value r=vn();if(sqlite3_step(st)==SQLITE_ROW)r=vs((const char*)sqlite3_column_text(st,0));sqlite3_finalize(st);return r;}
static Value memory_search(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||!handle_kind(a[0],HK_MEMORY)||a[1].t!=VSTR)return vn();int lim=n==3&&a[2].t==VINT?(int)a[2].u.i:10;if(lim<1||lim>100)return vn();HMemory*m=a[0].u.handle;sqlite3_stmt*st=NULL;if(sqlite3_prepare_v2(m->db,"SELECT key,value,tags FROM memories WHERE key LIKE ?1 OR value LIKE ?1 OR tags LIKE ?1 ORDER BY updated DESC LIMIT ?2",-1,&st,NULL)!=SQLITE_OK)return vn();char pat[1024];snprintf(pat,sizeof pat,"%%%s%%",a[1].u.s);sqlite3_bind_text(st,1,pat,-1,SQLITE_TRANSIENT);sqlite3_bind_int(st,2,lim);Value out=va();while(sqlite3_step(st)==SQLITE_ROW){Value o=vsobj();stput(o.u.st,"key",vs((const char*)sqlite3_column_text(st,0)));stput(o.u.st,"value",vs((const char*)sqlite3_column_text(st,1)));stput(o.u.st,"tags",vs((const char*)sqlite3_column_text(st,2)));ap(out.u.a,o);}sqlite3_finalize(st);return out;}
static Value memory_close(VM*vm,int n,Value*a){(void)vm;if(n!=1||!handle_kind(a[0],HK_MEMORY))return vb(0);HMemory*m=a[0].u.handle;if(!m->db)return vb(1);int ok=sqlite3_close_v2(m->db)==SQLITE_OK;if(ok){m->db=NULL;if(m->path)xfree(m->path);m->path=NULL;m->kind=HK_CLOSED;}return vb(ok);}

static HMemBuf *membuf_from(Value v){if(!hkind(v,HK_MEMBUF))return NULL;return (HMemBuf*)v.u.handle;}
static int membuf_bounds(HMemBuf*m,size_t off,size_t len){return m&&off<=m->size&&len<=m->size-off;}
static int membuf_size_arg(Value v,size_t*out){
    if(v.t!=VINT||v.u.i<0||!out)return 0;
    unsigned long long x=(unsigned long long)v.u.i;
    if(x>(unsigned long long)SIZE_MAX)return 0;
    *out=(size_t)x;return 1;
}
static Value memory_alloc(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();size_t z;if(!membuf_size_arg(a[0],&z)||z>HARIS_MEMBUF_MAX)return vn();HMemBuf*m=xmalloc_dtor(sizeof(*m),membuf_dtor);m->kind=HK_MEMBUF;m->size=z;m->owned=1;m->data=(unsigned char*)xmalloc(z?z:1);return (Value){.t=VHANDLE,.u.handle=m};}
static Value memory_resize(VM*vm,int n,Value*a){(void)vm;if(n!=2)return vb(0);HMemBuf*m=membuf_from(a[0]);size_t z;if(!m||!m->owned||!membuf_size_arg(a[1],&z)||z>HARIS_MEMBUF_MAX)return vb(0);m->data=(unsigned char*)xrealloc(m->data,z);m->size=z;return vb(1);}
static Value memory_size(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();HMemBuf*m=membuf_from(a[0]);return m?vi((long long)m->size):vn();}
static Value memory_read_u8(VM*vm,int n,Value*a){(void)vm;if(n!=2)return vn();HMemBuf*m=membuf_from(a[0]);size_t off;if(!m||!membuf_size_arg(a[1],&off)||!membuf_bounds(m,off,1))return vn();return vi((long long)m->data[off]);}
static Value memory_write_u8(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[2].t!=VINT||a[2].u.i<0||a[2].u.i>255)return vb(0);HMemBuf*m=membuf_from(a[0]);size_t off;if(!m||!membuf_size_arg(a[1],&off)||!membuf_bounds(m,off,1))return vb(0);m->data[off]=(unsigned char)a[2].u.i;return vb(1);}
#define MEM_READ_SCALAR(NAME,TYPE,MAKE) \
static Value NAME(VM*vm,int n,Value*a){(void)vm;if(n!=2)return vn();HMemBuf*m=membuf_from(a[0]);size_t off;if(!m||!membuf_size_arg(a[1],&off)||!membuf_bounds(m,off,sizeof(TYPE)))return vn();TYPE x;memcpy(&x,m->data+off,sizeof x);return MAKE;}
#define MEM_WRITE_SCALAR(NAME,TYPE,VALIDATE,CONVERT) \
static Value NAME(VM*vm,int n,Value*a){(void)vm;if(n!=3||!(VALIDATE))return vb(0);HMemBuf*m=membuf_from(a[0]);size_t off;if(!m||!membuf_size_arg(a[1],&off)||!membuf_bounds(m,off,sizeof(TYPE)))return vb(0);TYPE x=(TYPE)(CONVERT);memcpy(m->data+off,&x,sizeof x);return vb(1);}
MEM_READ_SCALAR(memory_read_u16,uint16_t,vi((long long)x))
MEM_READ_SCALAR(memory_read_u32,uint32_t,vi((long long)x))
MEM_READ_SCALAR(memory_read_u64,uint64_t,vi((long long)x))
MEM_READ_SCALAR(memory_read_i32,int32_t,vi((long long)x))
MEM_READ_SCALAR(memory_read_i64,int64_t,vi((long long)x))
MEM_READ_SCALAR(memory_read_f32,float,vf((double)x))
MEM_READ_SCALAR(memory_read_f64,double,vf(x))
MEM_WRITE_SCALAR(memory_write_u16,uint16_t,(a[2].t==VINT&&a[2].u.i>=0&&a[2].u.i<=65535),a[2].u.i)
MEM_WRITE_SCALAR(memory_write_u32,uint32_t,(a[2].t==VINT&&a[2].u.i>=0&& (unsigned long long)a[2].u.i<=UINT32_MAX),(unsigned long long)a[2].u.i)
MEM_WRITE_SCALAR(memory_write_u64,uint64_t,(a[2].t==VINT&&a[2].u.i>=0),(unsigned long long)a[2].u.i)
MEM_WRITE_SCALAR(memory_write_i32,int32_t,(a[2].t==VINT&&a[2].u.i>=INT32_MIN&&a[2].u.i<=INT32_MAX),a[2].u.i)
MEM_WRITE_SCALAR(memory_write_i64,int64_t,(a[2].t==VINT),a[2].u.i)
MEM_WRITE_SCALAR(memory_write_f32,float,isnum(a[2]),dn(a[2]))
MEM_WRITE_SCALAR(memory_write_f64,double,isnum(a[2]),dn(a[2]))
#undef MEM_READ_SCALAR
#undef MEM_WRITE_SCALAR
static Value memory_fill(VM*vm,int n,Value*a){(void)vm;if(n!=4||a[2].t!=VINT||a[2].u.i<0||a[2].u.i>255)return vb(0);HMemBuf*m=membuf_from(a[0]);size_t off,len;if(!m||!membuf_size_arg(a[1],&off)||!membuf_size_arg(a[3],&len)||!membuf_bounds(m,off,len))return vb(0);memset(m->data+off,(unsigned char)a[2].u.i,len);return vb(1);}
static Value memory_copy(VM*vm,int n,Value*a){(void)vm;if(n!=5)return vb(0);HMemBuf*d=membuf_from(a[0]),*src=membuf_from(a[2]);size_t doff,soff,len;if(!d||!src||!membuf_size_arg(a[1],&doff)||!membuf_size_arg(a[3],&soff)||!membuf_size_arg(a[4],&len)||!membuf_bounds(d,doff,len)||!membuf_bounds(src,soff,len))return vb(0);memmove(d->data+doff,src->data+soff,len);return vb(1);}
static Value memory_to_array(VM*vm,int n,Value*a){(void)vm;if(n!=3)return vn();HMemBuf*m=membuf_from(a[0]);size_t off,len;if(!m||!membuf_size_arg(a[1],&off)||!membuf_size_arg(a[2],&len)||len>4ULL*1024ULL*1024ULL||!membuf_bounds(m,off,len))return vn();Value o=va();if(!arr_reserve(o.u.a,len))return vn();for(size_t i=0;i<len;i++)ap(o.u.a,vi(m->data[off+i]));return o;}
static Value memory_free(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HMemBuf*m=membuf_from(a[0]);if(!m)return vb(0);if(m->owned&&m->data)xfree(m->data);m->data=NULL;m->size=0;m->owned=0;m->kind=HK_CLOSED;return vb(1);}
static Value memory_wrap(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NUCLEAR))return cap_error(vm,CAP_NUCLEAR,"memory.wrap");if(n!=2||a[0].t!=VINT||a[1].t!=VINT||a[0].u.i<=0||a[1].u.i<0)return vn();uintptr_t addr=(uintptr_t)(unsigned long long)a[0].u.i;size_t z;if(!membuf_size_arg(a[1],&z)||z>HARIS_MEMBUF_MAX)return vn();HMemBuf*m=xmalloc_dtor(sizeof(*m),membuf_dtor);m->kind=HK_MEMBUF;m->data=(unsigned char*)addr;m->size=z;m->owned=0;return (Value){.t=VHANDLE,.u.handle=m};}
static Value memory_address(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NUCLEAR))return cap_error(vm,CAP_NUCLEAR,"memory.address");if(n!=1)return vn();HMemBuf*m=membuf_from(a[0]);if(!m)return vn();return vi((long long)(uintptr_t)m->data);}

static Value ai_tool_call(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTR)return vn();Value o=vsobj();stput(o.u.st,"name",a[0]);stput(o.u.st,"arguments",a[1]);stput(o.u.st,"type",vs("function"));return o;}
/* AI/CPU/GPU/OOP expansion. OOP here uses prototype objects so it fits Haris's
   existing struct/value model without introducing closures into the VM. */
static Value ncpu_info(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_HW_CPU))return cap_error(vm,CAP_HW_CPU,"cpu.info");
    (void)vm;(void)a;if(n)return vn(); Value o=vsobj();
#ifdef _WIN32
    SYSTEM_INFO si; GetSystemInfo(&si); stput(o.u.st,"cores",vi((long long)si.dwNumberOfProcessors)); stput(o.u.st,"arch",vs(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64?"x86_64":"unknown"));
#else
    long c=sysconf(_SC_NPROCESSORS_ONLN); if(c<1)c=1; stput(o.u.st,"cores",vi(c));
#if defined(__x86_64__)
    stput(o.u.st,"arch",vs("x86_64"));
#elif defined(__aarch64__)
    stput(o.u.st,"arch",vs("aarch64"));
#elif defined(__i386__)
    stput(o.u.st,"arch",vs("x86"));
#else
    stput(o.u.st,"arch",vs("unknown"));
#endif
#endif
#if defined(__AVX2__)
    stput(o.u.st,"avx2",vb(1));
#else
    stput(o.u.st,"avx2",vb(0));
#endif
    return o;
}
static Value ncpu_cores(VM*vm,int n,Value*a){if(n)return vn();Value o=ncpu_info(vm,0,NULL);return stget(o.u.st,"cores");}
static Value ncpu_arch(VM*vm,int n,Value*a){if(n)return vn();Value o=ncpu_info(vm,0,NULL);return stget(o.u.st,"arch");}
static Value ncpu_affinity(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_HW_CPU))return cap_error(vm,CAP_HW_CPU,"cpu.affinity");if(n!=1||a[0].t!=VINT||a[0].u.i<1)return vb(0);
#ifdef _WIN32
    long long c=a[0].u.i; DWORD_PTR mask=c>=(long long)(sizeof(DWORD_PTR)*8)?~(DWORD_PTR)0:(((DWORD_PTR)1<<c)-1); return vb(SetProcessAffinityMask(GetCurrentProcess(),mask)!=0);
#else
#if defined(__linux__) && !defined(__ANDROID__)
    long long c=a[0].u.i; cpu_set_t set; CPU_ZERO(&set); for(long i=0,lim=sysconf(_SC_NPROCESSORS_CONF);i<lim&&i<c;i++)CPU_SET((int)i,&set); return vb(sched_setaffinity(0,sizeof(set),&set)==0);
#else
    return vb(0);
#endif
#endif
}
static Value ncpu_yield(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();
#ifdef _WIN32
    Sleep(0);
#else
    sched_yield();
#endif
    return vn();
}
static Value ngpu_info(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_HW_GPU))return cap_error(vm,CAP_HW_GPU,"gpu.info");
    (void)a;if(n)return vn();Value o=vsobj();const char*e=getenv("HARIS_GPU");stput(o.u.st,"device",vs(e&&*e?e:"auto"));
    int cuda=0;
#ifdef __linux__
    void*h=dlopen("libcuda.so.1",RTLD_LAZY);if(h){cuda=1;dlclose(h);}
    stput(o.u.st,"backend",vs("linux"));
#elif defined(_WIN32)
    HMODULE h=LoadLibraryA("nvcuda.dll");if(h){cuda=1;FreeLibrary(h);}
    stput(o.u.st,"backend",vs("windows"));
#else
    stput(o.u.st,"backend",vs("portable"));
#endif
    /* Kept for API compatibility: it now means an NVIDIA CUDA driver is
       detectable, without spawning a shell or trusting PATH. */
    stput(o.u.st,"nvidia_smi",vb(cuda));stput(o.u.st,"nvidia_driver",vb(cuda));stput(o.u.st,"cuda",vb(cuda));stput(o.u.st,"portable",vb(1));return o;
}
static Value ngpu_backend(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_HW_GPU))return cap_error(vm,CAP_HW_GPU,"gpu.backend");if(n!=1||a[0].t!=VSTR)return vb(0);Value o=ngpu_info(vm,0,NULL);Value v=stget(o.u.st,a[0].u.s);return v.t==VBOOL?v:vb(0);}
static Value ngpu_compute(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_HW_GPU))return cap_error(vm,CAP_HW_GPU,"gpu.compute");
    if(n!=3||a[0].t!=VSTR||a[1].t!=VARR||a[2].t!=VARR)return vn();
    if(!strcmp(a[0].u.s,"add"))return ai_vec_binary(vm,2,(Value[]){a[1],a[2]},0);
    if(!strcmp(a[0].u.s,"mul"))return ai_vec_binary(vm,2,(Value[]){a[1],a[2]},2);
    if(!strcmp(a[0].u.s,"matmul"))return nai_matmul(vm,2,(Value[]){a[1],a[2]});
    return vn();
}
/* ===================== Real Tensor + Autograd ===================== */
typedef enum { TOP_LEAF,TOP_ADD,TOP_SUB,TOP_MUL,TOP_NEG,TOP_RELU,TOP_TANH,TOP_SIGMOID,TOP_GELU,TOP_SUM,TOP_MEAN,TOP_MATMUL,TOP_LINEAR,TOP_SOFTMAX,TOP_LAYERNORM,TOP_ATTENTION,TOP_CE,TOP_FUSED_LINEAR_RELU } HTOp;
/* Tensor ownership is GC-based, not VM-stack-only: tensor handles and every
   autograd edge are traced by gc_mark_handle().  This makes long training runs
   reclaim unreachable graphs, including cycles.  tensor.clear releases the large
   numeric buffers early while preserving the handle and graph metadata. */
struct HTensor {int kind,ndim,requires_grad;size_t n,shape[8];double *data,*grad,*opt_m,*opt_v;unsigned long long opt_step;double opt_b1_pow,opt_b2_pow;uint64_t visit;HTOp op;HTensor *a,*b,*c;double aux;size_t aux_n;};
static void htensor_dtor(void*p){HTensor*t=(HTensor*)p;if(!t)return;if(t->data)xfree(t->data);if(t->grad)xfree(t->grad);if(t->opt_m)xfree(t->opt_m);if(t->opt_v)xfree(t->opt_v);t->data=t->grad=t->opt_m=t->opt_v=NULL;t->kind=HK_CLOSED;}
static HARIS_TLS uint64_t g_tensor_visit_epoch=0;
static int tensor_handle(Value v){return hkind(v,HK_TENSOR);}
static size_t tensor_numel_shape(const size_t*s,int nd){if(nd<0||nd>8)return 0;size_t n=1;for(int i=0;i<nd;i++){if(!s[i]||n>SIZE_MAX/s[i])return 0;n*=s[i];}return n;}
static HTensor* tensor_new_raw(int nd,const size_t*shape,int rg){if(nd<0||nd>8)return NULL;size_t n=tensor_numel_shape(shape,nd);if(!n||n>H_DATA_MAX/sizeof(double))return NULL;HTensor*t=(HTensor*)xmalloc_dtor(sizeof(*t),htensor_dtor);memset(t,0,sizeof*t);t->kind=HK_TENSOR;t->ndim=nd;t->n=n;t->requires_grad=!!rg;memcpy(t->shape,shape,(size_t)nd*sizeof(size_t));t->data=(double*)xmalloc(n*sizeof(double));return t;}
static Value tensor_v(HTensor*t){return (Value){.t=VHANDLE,.u.handle=t};}
static int tensor_same(const HTensor*a,const HTensor*b){if(!a||!b||a->ndim!=b->ndim||a->n!=b->n)return 0;for(int i=0;i<a->ndim;i++)if(a->shape[i]!=b->shape[i])return 0;return 1;}
static int tensor_binary_shape(const HTensor*a,const HTensor*b){return tensor_same(a,b)||(b&&b->n==1)||(a&&a->n==1);}
static int tensor_need_grad(HTensor*t){return t&&t->requires_grad;}
static HTensor*tensor_make_op(HTOp op,HTensor*a,HTensor*b,HTensor*c){int nd=a?a->ndim:(b?b->ndim:(c?c->ndim:0));const size_t*sh=a?a->shape:(b?b->shape:c->shape);HTensor*t=tensor_new_raw(nd,sh,(a&&a->requires_grad)||(b&&b->requires_grad)||(c&&c->requires_grad));if(!t)return NULL;t->op=op;t->a=a;t->b=b;t->c=c;return t;}
static Value tensor_from_array(VM*vm,int n,Value*a){(void)vm;if(n<1||n>3)return vn();if(a[0].t!=VARR&&a[0].t!=VFLOAT&&a[0].t!=VINT)return vn();int rg=0;if(n>=3&&a[2].t==VBOOL)rg=a[2].u.b;int nd=1;size_t sh[8]={0};Value src=a[0];if(src.t==VARR){size_t m=src.u.a->n;if(n>=2&&a[1].t==VARR){if(a[1].u.a->n<1||a[1].u.a->n>8)return vn();nd=(int)a[1].u.a->n;for(int i=0;i<nd;i++){Value q=a[1].u.a->v[i];if(q.t!=VINT||q.u.i<=0)return vn();sh[i]=(size_t)q.u.i;}if(tensor_numel_shape(sh,nd)!=m)return vn();}else{sh[0]=m;}}else sh[0]=1;HTensor*t=tensor_new_raw(nd,sh,rg);if(!t)return vn();if(src.t==VARR){for(size_t i=0;i<t->n;i++){if(!isnum(src.u.a->v[i])){htensor_dtor(t);return vn();}t->data[i]=dn(src.u.a->v[i]);}}else t->data[0]=dn(src);return tensor_v(t);}
static Value ntensor_zeros(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2)return vn();if(a[0].t==VINT){size_t sh[1]={(size_t)a[0].u.i};if(sh[0]>1000000)return vn();HTensor*x=tensor_new_raw(1,sh,0);if(!x)return vn();memset(x->data,0,x->n*sizeof(double));return tensor_v(x);}if(a[0].t==VARR){Value q[3]={vn(),a[0],vb(0)};Value temp=va();for(size_t i=0;i<a[0].u.a->n;i++)ap(temp.u.a,vi(0));q[0]=temp;return tensor_from_array(vm,3,q);}return vn();}
static Value ntensor_range(VM*vm,int n,Value*a){(void)vm;if(n<1||n>3)return vn();long long st=0,en=0,step=1;if(n==1){if(a[0].t!=VINT)return vn();en=a[0].u.i;}else{if(a[0].t!=VINT||a[1].t!=VINT)return vn();st=a[0].u.i;en=a[1].u.i;if(n==3){if(a[2].t!=VINT)return vn();step=a[2].u.i;}}if(!step)return vn();size_t cnt=0;long long x=st;if(step>0){if(en>st)cnt=(size_t)((en-st+step-1)/step);}else{if(en<st)cnt=(size_t)((st-en+(-step)-1)/(-step));}if(!cnt||cnt>1000000)return vn();size_t sh[1]={cnt};HTensor*t=tensor_new_raw(1,sh,0);if(!t)return vn();for(size_t i=0;i<cnt;i++){t->data[i]=(double)x;x+=step;}return tensor_v(t);}
static Value tensor_ones(VM*vm,int n,Value*a){Value z=ntensor_zeros(vm,n,a);if(!tensor_handle(z))return z;HTensor*t=z.u.handle;for(size_t i=0;i<t->n;i++)t->data[i]=1;return z;}
static Value tensor_parameter(VM*vm,int n,Value*a){if(n<1||n>2)return vn();Value q[3];q[0]=a[0];q[1]=n>=2?a[1]:vn();q[2]=vb(1);if(q[1].t==VNULL){Value sh=va();ap(sh.u.a,vi(1));q[1]=sh;}return tensor_from_array(vm,3,q);}
static Value tensor_binary(VM*vm,int n,Value*a,HTOp op){(void)vm;if(n!=2||!tensor_handle(a[0])||!tensor_handle(a[1]))return vn();HTensor*x=a[0].u.handle,*y=a[1].u.handle;if(!tensor_binary_shape(x,y)||x->n==1&&y->n>1)return vn();HTensor*t=tensor_make_op(op,x,y,NULL);if(!t)return vn();size_t N=t->n;for(size_t i=0;i<N;i++){double xv=x->n==1?x->data[0]:x->data[i],yv=y->n==1?y->data[0]:y->data[i];t->data[i]=op==TOP_ADD?xv+yv:op==TOP_SUB?xv-yv:xv*yv;}return tensor_v(t);}
static Value tensor_neg(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*x=a[0].u.handle,*t=tensor_make_op(TOP_NEG,x,NULL,NULL);if(!t)return vn();for(size_t i=0;i<t->n;i++)t->data[i]=-x->data[i];return tensor_v(t);}
static Value tensor_unary(VM*vm,int n,Value*a,HTOp op){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*x=a[0].u.handle,*t=tensor_make_op(op,x,NULL,NULL);if(!t)return vn();for(size_t i=0;i<t->n;i++){double v=x->data[i];t->data[i]=op==TOP_RELU?(v>0?v:0):op==TOP_TANH?tanh(v):op==TOP_SIGMOID?1.0/(1.0+exp(-fmax(-700,fmin(700,v)))):.5*v*(1+tanh(sqrt(2.0/M_PI)*(v+.044715*v*v*v)));}return tensor_v(t);}
static Value tensor_reduce(VM*vm,int n,Value*a,int mean){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*x=a[0].u.handle;size_t sh[1]={1};HTensor*t=tensor_new_raw(1,sh,x->requires_grad);if(!t)return vn();t->op=mean?TOP_MEAN:TOP_SUM;t->a=x;t->data[0]=0;for(size_t i=0;i<x->n;i++)t->data[0]+=x->data[i];if(mean)t->data[0]/=(double)x->n;return tensor_v(t);}
static Value tensor_matmul(VM*vm,int n,Value*a){(void)vm;if(n!=2||!tensor_handle(a[0])||!tensor_handle(a[1]))return vn();HTensor*A=a[0].u.handle,*B=a[1].u.handle;if(A->ndim!=2||B->ndim!=2||A->shape[1]!=B->shape[0])return vn();size_t m=A->shape[0],kdim=A->shape[1],q=B->shape[1],sh[2]={m,q};HTensor*t=tensor_new_raw(2,sh,A->requires_grad||B->requires_grad);if(!t)return vn();t->op=TOP_MATMUL;t->a=A;t->b=B;memset(t->data,0,t->n*sizeof(double));for(size_t i=0;i<m;i++){double*row=t->data+i*q;const double*ar=A->data+i*kdim;for(size_t k=0;k<kdim;k++){double av=ar[k];const double*br=B->data+k*q;for(size_t j=0;j<q;j++)row[j]+=av*br[j];}}return tensor_v(t);}
static Value tensor_linear(VM*vm,int n,Value*a){if(n!=3||!tensor_handle(a[0])||!tensor_handle(a[1])||!tensor_handle(a[2]))return vn();HTensor*x=a[0].u.handle,*w=a[1].u.handle,*b=a[2].u.handle;if(x->ndim!=2||w->ndim!=2||b->ndim!=1||x->shape[1]!=w->shape[1]||b->n!=w->shape[0])return vn();size_t m=x->shape[0],in=x->shape[1],outn=w->shape[0],sh[2]={m,outn};HTensor*t=tensor_new_raw(2,sh,x->requires_grad||w->requires_grad||b->requires_grad);if(!t)return vn();t->op=TOP_LINEAR;t->a=x;t->b=w;t->c=b;for(size_t i=0;i<m;i++){double*row=t->data+i*outn;const double*xr=x->data+i*in;for(size_t j=0;j<outn;j++)row[j]=b->data[j];for(size_t k=0;k<in;k++){double xv=xr[k];const double*wr=w->data+k;for(size_t j=0;j<outn;j++)row[j]+=xv*wr[j*in];}}return tensor_v(t);}
static Value tensor_softmax(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*x=a[0].u.handle,*t=tensor_make_op(TOP_SOFTMAX,x,NULL,NULL);if(!t)return vn();if(x->ndim!=1)return vn();double m=-HUGE_VAL,s=0;for(size_t i=0;i<x->n;i++)m=fmax(m,x->data[i]);for(size_t i=0;i<x->n;i++){t->data[i]=exp(x->data[i]-m);s+=t->data[i];}for(size_t i=0;i<x->n;i++)t->data[i]/=s;return tensor_v(t);}
static Value tensor_layernorm(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||!tensor_handle(a[0]))return vn();HTensor*x=a[0].u.handle;if(x->ndim!=1&&x->ndim!=2)return vn();double eps=n==2&&isnum(a[1])?dn(a[1]):1e-5;HTensor*t=tensor_make_op(TOP_LAYERNORM,x,NULL,NULL);if(!t||eps<=0)return vn();size_t rows=x->ndim==1?1:x->shape[0],cols=x->ndim==1?x->n:x->shape[1];for(size_t r=0;r<rows;r++){double m=0,v=0;for(size_t j=0;j<cols;j++)m+=x->data[r*cols+j];m/=cols;for(size_t j=0;j<cols;j++){double d=x->data[r*cols+j]-m;v+=d*d;}v/=cols;double inv=1/sqrt(v+eps);for(size_t j=0;j<cols;j++)t->data[r*cols+j]=(x->data[r*cols+j]-m)*inv;}t->aux=eps;return tensor_v(t);}
static int tensor_mark_visit(HTensor**seen,size_t*n,HTensor*t){if(!t)return 1;for(size_t i=0;i<*n;i++)if(seen[i]==t)return 1;if(*n>=65536)return 0;seen[(*n)++]=t;return 1;}
static void tensor_zero_grad_tree(HTensor*t){if(!t)return;if(t->grad)memset(t->grad,0,t->n*sizeof(double));tensor_zero_grad_tree(t->a);tensor_zero_grad_tree(t->b);tensor_zero_grad_tree(t->c);}
static int tensor_ensure_grad(HTensor*t){if(!t||!t->requires_grad)return 1;if(!t->grad)t->grad=(double*)xmalloc(t->n*sizeof(double));return 1;}
static void tensor_add_grad(HTensor*t,size_t i,double v){if(!t||!t->requires_grad)return;if(!tensor_ensure_grad(t))return;t->grad[i]+=v;}
static Value tensor_backward(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||!tensor_handle(a[0]))return vb(0);HTensor*out=a[0].u.handle;if(out->n!=1&&n==1)return vb(0);if(n==2&&a[1].t!=VARR)return vb(0);size_t cap=4096,count=0;HTensor**nodes=(HTensor**)xmalloc(cap*sizeof(*nodes));HTensor**stack=(HTensor**)xmalloc(cap*sizeof(*stack));size_t sp=0;uint64_t stamp=++g_tensor_visit_epoch;if(!stamp)stamp=++g_tensor_visit_epoch;stack[sp++]=out;while(sp){HTensor*t=stack[--sp];if(!t||t->visit==stamp)continue;t->visit=stamp;if(count==cap){if(cap>65536){xfree(nodes);xfree(stack);return vb(0);}cap*=2;nodes=(HTensor**)xrealloc(nodes,cap*sizeof(*nodes));stack=(HTensor**)xrealloc(stack,cap*sizeof(*stack));}nodes[count++]=t;HTensor*kids[3]={t->a,t->b,t->c};for(int ki=0;ki<3;ki++)if(kids[ki]){if(sp==cap){if(cap>65536){xfree(nodes);xfree(stack);return vb(0);}cap*=2;stack=(HTensor**)xrealloc(stack,cap*sizeof(*stack));}stack[sp++]=kids[ki];}}
    if(!tensor_ensure_grad(out)){xfree(nodes);xfree(stack);return vb(0);}if(n==1)out->grad[0]=1.0;else{Value g=a[1];if(g.u.a->n!=out->n){xfree(nodes);xfree(stack);return vb(0);}for(size_t i=0;i<out->n;i++){if(!isnum(g.u.a->v[i])){xfree(nodes);xfree(stack);return vb(0);}out->grad[i]=dn(g.u.a->v[i]);}}
    for(size_t ri=0;ri<count;ri++){HTensor*t=nodes[ri];if(!t->grad)continue;size_t N=t->n;switch(t->op){case TOP_ADD:case TOP_SUB:case TOP_MUL:for(size_t i=0;i<N;i++){size_t ia=t->a->n==1?0:i,ib=t->b->n==1?0:i;double ga=t->grad[i],gb=t->grad[i];if(t->op==TOP_MUL){ga*=t->b->data[ib];gb*=t->a->data[ia];}if(t->op==TOP_SUB)gb=-gb;tensor_add_grad(t->a,ia,ga);tensor_add_grad(t->b,ib,gb);}break;case TOP_NEG:for(size_t i=0;i<N;i++)tensor_add_grad(t->a,i,-t->grad[i]);break;case TOP_RELU:for(size_t i=0;i<N;i++)tensor_add_grad(t->a,i,t->grad[i]*(t->a->data[i]>0));break;case TOP_TANH:for(size_t i=0;i<N;i++)tensor_add_grad(t->a,i,t->grad[i]*(1-t->data[i]*t->data[i]));break;case TOP_SIGMOID:for(size_t i=0;i<N;i++)tensor_add_grad(t->a,i,t->grad[i]*t->data[i]*(1-t->data[i]));break;case TOP_GELU:for(size_t i=0;i<N;i++){double x=t->a->data[i],u=sqrt(2.0/M_PI)*(x+.044715*x*x*x),th=tanh(u),du=sqrt(2.0/M_PI)*(1+.134145*x*x);double d=.5*(1+th)+.5*x*(1-th*th)*du;tensor_add_grad(t->a,i,t->grad[i]*d);}break;case TOP_SUM:for(size_t i=0;i<t->a->n;i++)tensor_add_grad(t->a,i,t->grad[0]);break;case TOP_MEAN:for(size_t i=0;i<t->a->n;i++)tensor_add_grad(t->a,i,t->grad[0]/(double)t->a->n);break;case TOP_MATMUL:{size_t m=t->a->shape[0],k=t->a->shape[1],q=t->b->shape[1];for(size_t i=0;i<m;i++)for(size_t j=0;j<q;j++){double g=t->grad[i*q+j];for(size_t z=0;z<k;z++){tensor_add_grad(t->a,i*k+z,g*t->b->data[z*q+j]);tensor_add_grad(t->b,z*q+j,g*t->a->data[i*k+z]);}}break;}case TOP_FUSED_LINEAR_RELU:{size_t m=t->a->shape[0],k=t->a->shape[1],q=t->b->shape[0];for(size_t i=0;i<m;i++)for(size_t j=0;j<q;j++){double g=(t->data[i*q+j]>0)?t->grad[i*q+j]:0;tensor_add_grad(t->c,j,g);for(size_t z=0;z<k;z++){tensor_add_grad(t->a,i*k+z,g*t->b->data[j*k+z]);tensor_add_grad(t->b,j*k+z,g*t->a->data[i*k+z]);}}break;}
        case TOP_LINEAR:{size_t m=t->a->shape[0],k=t->a->shape[1],q=t->b->shape[0];for(size_t i=0;i<m;i++)for(size_t j=0;j<q;j++){double g=t->grad[i*q+j];tensor_add_grad(t->c,j,g);for(size_t z=0;z<k;z++){tensor_add_grad(t->a,i*k+z,g*t->b->data[j*k+z]);tensor_add_grad(t->b,j*k+z,g*t->a->data[i*k+z]);}}break;}case TOP_SOFTMAX:{double dot=0;for(size_t i=0;i<N;i++)dot+=t->grad[i]*t->data[i];for(size_t i=0;i<N;i++)tensor_add_grad(t->a,i,t->data[i]*(t->grad[i]-dot));break;}case TOP_CE:{size_t cls=t->aux_n;double m=-HUGE_VAL,s=0;for(size_t i=0;i<t->a->n;i++)m=fmax(m,t->a->data[i]);for(size_t i=0;i<t->a->n;i++)s+=exp(t->a->data[i]-m);for(size_t i=0;i<t->a->n;i++){double p=exp(t->a->data[i]-m)/s;tensor_add_grad(t->a,i,t->grad[0]*(p-((i==cls)?1.0:0.0)));}break;}case TOP_LAYERNORM:{size_t rows=t->a->ndim==1?1:t->a->shape[0],cols=t->a->ndim==1?t->a->n:t->a->shape[1];double eps=t->aux;for(size_t r=0;r<rows;r++){double mean=0,var=0;for(size_t j=0;j<cols;j++)mean+=t->a->data[r*cols+j];mean/=cols;for(size_t j=0;j<cols;j++){double d=t->a->data[r*cols+j]-mean;var+=d*d;}var/=cols;double inv=1/sqrt(var+eps),sumg=0,sumgx=0;for(size_t j=0;j<cols;j++){double xd=t->a->data[r*cols+j]-mean;sumg+=t->grad[r*cols+j];sumgx+=t->grad[r*cols+j]*xd;}for(size_t j=0;j<cols;j++){double xhat=(t->a->data[r*cols+j]-mean)*inv;double g=(inv/cols)*(cols*t->grad[r*cols+j]-sumg-xhat*sumgx);tensor_add_grad(t->a,r*cols+j,g);}}break;}default:break;}}
    xfree(nodes);xfree(stack);return vb(1);}
static Value tensor_cross_entropy(VM*vm,int n,Value*a){(void)vm;if(n!=2||!tensor_handle(a[0])||a[1].t!=VINT)return vn();HTensor*x=a[0].u.handle;long long cls=a[1].u.i;if(x->ndim!=1||cls<0||(size_t)cls>=x->n)return vn();size_t sh[1]={1};HTensor*t=tensor_new_raw(1,sh,x->requires_grad);if(!t)return vn();t->op=TOP_CE;t->a=x;t->aux_n=(size_t)cls;double m=-HUGE_VAL,s=0;for(size_t i=0;i<x->n;i++)m=fmax(m,x->data[i]);for(size_t i=0;i<x->n;i++)s+=exp(x->data[i]-m);t->data[0]=-(x->data[cls]-m-log(s));return tensor_v(t);} 
static Value tensor_zero_grad(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vb(0);tensor_zero_grad_tree(a[0].u.handle);return vb(1);}
static Value tensor_data(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*t=a[0].u.handle;Value o=va();for(size_t i=0;i<t->n;i++)ap(o.u.a,vf(t->data[i]));return o;}
static Value tensor_grad(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*t=a[0].u.handle;if(!t->grad)return va();Value o=va();for(size_t i=0;i<t->n;i++)ap(o.u.a,vf(t->grad[i]));return o;}
static Value tensor_shape(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*t=a[0].u.handle;Value o=va();for(int i=0;i<t->ndim;i++)ap(o.u.a,vi((long long)t->shape[i]));return o;}
static Value tensor_numel(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();return vi((long long)((HTensor*)a[0].u.handle)->n);}
static Value tensor_adam_step(VM*vm,int n,Value*a){(void)vm;if(n<2||n>5||!tensor_handle(a[0])||!isnum(a[1]))return vb(0);HTensor*t=a[0].u.handle;double lr=dn(a[1]),b1=n>=3&&isnum(a[2])?dn(a[2]):0.9,b2=n>=4&&isnum(a[3])?dn(a[3]):0.999,eps=n>=5&&isnum(a[4])?dn(a[4]):1e-8;if(lr<=0||b1<=0||b1>=1||b2<=0||b2>=1||eps<=0||!t->grad)return vb(0);if(!t->opt_m){t->opt_m=(double*)xmalloc(t->n*sizeof(double));t->opt_v=(double*)xmalloc(t->n*sizeof(double));memset(t->opt_m,0,t->n*sizeof(double));memset(t->opt_v,0,t->n*sizeof(double));t->opt_step=0;t->opt_b1_pow=1.0;t->opt_b2_pow=1.0;}t->opt_step++;t->opt_b1_pow*=b1;t->opt_b2_pow*=b2;double b1t=1.0-t->opt_b1_pow,b2t=1.0-t->opt_b2_pow;for(size_t i=0;i<t->n;i++){double g=t->grad[i];t->opt_m[i]=b1*t->opt_m[i]+(1-b1)*g;t->opt_v[i]=b2*t->opt_v[i]+(1-b2)*g*g;t->data[i]-=lr*(t->opt_m[i]/b1t)/(sqrt(t->opt_v[i]/b2t)+eps);}return vb(1);}
static Value tensor_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vn();HTensor*t=a[0].u.handle;Value o=vsobj();stput(o.u.st,"ndim",vi(t->ndim));stput(o.u.st,"numel",vi((long long)t->n));stput(o.u.st,"requires_grad",vb(t->requires_grad));stput(o.u.st,"op",vi((long long)t->op));return o;}
static Value tensor_clear(VM*vm,int n,Value*a){(void)vm;if(n!=1||!tensor_handle(a[0]))return vb(0);HTensor*t=a[0].u.handle;if(t->data){xfree(t->data);t->data=NULL;}if(t->grad){xfree(t->grad);t->grad=NULL;}if(t->opt_m){xfree(t->opt_m);t->opt_m=NULL;}if(t->opt_v){xfree(t->opt_v);t->opt_v=NULL;}return vb(1);}

static Value tensor_add(VM*v,int n,Value*a){return tensor_binary(v,n,a,TOP_ADD);}static Value tensor_sub(VM*v,int n,Value*a){return tensor_binary(v,n,a,TOP_SUB);}static Value tensor_mul(VM*v,int n,Value*a){return tensor_binary(v,n,a,TOP_MUL);}static Value tensor_relu(VM*v,int n,Value*a){return tensor_unary(v,n,a,TOP_RELU);}static Value tensor_tanh(VM*v,int n,Value*a){return tensor_unary(v,n,a,TOP_TANH);}static Value tensor_sigmoid(VM*v,int n,Value*a){return tensor_unary(v,n,a,TOP_SIGMOID);}static Value tensor_gelu(VM*v,int n,Value*a){return tensor_unary(v,n,a,TOP_GELU);}static Value tensor_sum(VM*v,int n,Value*a){return tensor_reduce(v,n,a,0);}static Value tensor_mean_real(VM*v,int n,Value*a){return tensor_reduce(v,n,a,1);}
static Value nai_gelu(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR)return vn();Value o=va();for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();double x=dn(a[0].u.a->v[i]);ap(o.u.a,vf(.5*x*(1.0+tanh(sqrt(2.0/M_PI)*(x+.044715*x*x*x)))));}return o;}
static Value nai_layernorm(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||a[0].t!=VARR||!a[0].u.a->n)return vn();double eps=n==2?dn(a[1]):1e-5,m=0,v=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();m+=dn(a[0].u.a->v[i]);}m/=a[0].u.a->n;for(size_t i=0;i<a[0].u.a->n;i++){double d=dn(a[0].u.a->v[i])-m;v+=d*d;}v/=a[0].u.a->n;double inv=1.0/sqrt(v+eps);Value o=va();for(size_t i=0;i<a[0].u.a->n;i++)ap(o.u.a,vf((dn(a[0].u.a->v[i])-m)*inv));return o;}
static Value nai_softmax_temperature(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||a[0].t!=VARR)return vn();double temp=n==2?dn(a[1]):1.0;if(temp<=0)return vn();double mx=-HUGE_VAL,sum=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();mx=fmax(mx,dn(a[0].u.a->v[i]));}for(size_t i=0;i<a[0].u.a->n;i++)sum+=exp((dn(a[0].u.a->v[i])-mx)/temp);Value o=va();for(size_t i=0;i<a[0].u.a->n;i++)ap(o.u.a,vf(exp((dn(a[0].u.a->v[i])-mx)/temp)/sum));return o;}
static Value nai_topk(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR||a[1].t!=VINT)return vn();long long k=a[1].u.i;if(k<1||k>(long long)a[0].u.a->n||k>1000)return vn();Value used=va(),out=va();for(long long z=0;z<k;z++){long long bi=-1;double bv=-HUGE_VAL;for(size_t i=0;i<a[0].u.a->n;i++){int seen=0;for(size_t j=0;j<used.u.a->n;j++)if(used.u.a->v[j].u.i==(long long)i)seen=1;if(seen)continue;if(!isnum(a[0].u.a->v[i]))return vn();double v=dn(a[0].u.a->v[i]);if(bi<0||v>bv){bi=i;bv=v;}}ap(used.u.a,vi(bi));Value o=vsobj();stput(o.u.st,"index",vi(bi));stput(o.u.st,"value",vf(bv));ap(out.u.a,o);}return out;}
static Value nai_sample_greedy(VM*vm,int n,Value*a){return nai_argmax(vm,n,a);}
static Value nai_attention(VM*vm,int n,Value*a){
    (void)vm;if(n<2||n>3||a[0].t!=VARR||a[1].t!=VARR)return vn();Value Q=a[0],K=a[1],V=n==3?a[2]:a[1];size_t nq=Q.u.a->n,nk=K.u.a->n;if(!nq||!nk||V.t!=VARR||V.u.a->n!=nk||Q.u.a->v[0].t!=VARR||K.u.a->v[0].t!=VARR)return vn();size_t d=Q.u.a->v[0].u.a->n,dv=V.u.a->v[0].u.a->n;if(!d||!dv||nq>1024||nk>1024||d>1024||dv>1024)return vn();
    for(size_t i=0;i<nq;i++)if(Q.u.a->v[i].t!=VARR||Q.u.a->v[i].u.a->n!=d)return vn();for(size_t j=0;j<nk;j++)if(K.u.a->v[j].t!=VARR||K.u.a->v[j].u.a->n!=d||V.u.a->v[j].t!=VARR||V.u.a->v[j].u.a->n!=dv)return vn();
    Value out=va();double scale=1.0/sqrt((double)d);double *scores=(double*)xmalloc(nk*sizeof(double));if(!scores)return vn();
    for(size_t i=0;i<nq;i++){double mx=-HUGE_VAL;for(size_t j=0;j<nk;j++){double ss=0;for(size_t z=0;z<d;z++)ss+=dn(Q.u.a->v[i].u.a->v[z])*dn(K.u.a->v[j].u.a->v[z]);scores[j]=ss*scale;if(scores[j]>mx)mx=scores[j];}double sum=0;for(size_t j=0;j<nk;j++){scores[j]=exp(scores[j]-mx);sum+=scores[j];}double inv=1.0/sum;Value row=va();for(size_t z=0;z<dv;z++){double ss=0;for(size_t j=0;j<nk;j++)ss+=scores[j]*dn(V.u.a->v[j].u.a->v[z]);ap(row.u.a,vf(ss*inv));}ap(out.u.a,row);}xfree(scores);return out;
}
static Value ntensor_mean(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR||!a[0].u.a->n)return vn();double sum=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();sum+=dn(a[0].u.a->v[i]);}return vf(sum/(double)a[0].u.a->n);}
static Value noop_dup(Value v){if(v.t==VARR){Value o=va();for(size_t i=0;i<v.u.a->n;i++)ap(o.u.a,noop_dup(v.u.a->v[i]));return o;}if(v.t==VSTRUCT){Value o=vsobj();for(size_t i=0;i<v.u.st->n;i++)stput(o.u.st,v.u.st->v[i].name,noop_dup(v.u.st->v[i].value));return o;}return v;}
static Value noop_new(VM*vm,int n,Value*a){
    (void)vm;
    if(n!=1||a[0].t!=VSTRUCT)return vn();
    Value o=noop_dup(a[0]);
    Value c=stget(o.u.st,"__class");
    if(c.t!=VSTR)stput(o.u.st,"__class",vs("Object"));
    return o;
}
static Value noop_get(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT||a[1].t!=VSTR)return vn();Value v=stget(a[0].u.st,a[1].u.s);return v;}
static Value noop_set(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VSTRUCT||a[1].t!=VSTR)return vb(0);stput(a[0].u.st,a[1].u.s,a[2]);return vb(1);}
static Value noop_has(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT||a[1].t!=VSTR)return vb(0);return vb(stget(a[0].u.st,a[1].u.s).t!=VNULL);}
static Value noop_type(VM*vm,int n,Value*a){
    (void)vm;
    if(n!=1)return vn();
    if(a[0].t==VSTRUCT){
        Value c=stget(a[0].u.st,"__class");
        if(c.t==VSTR)return c;
        Value ty=stget(a[0].u.st,"__type");
        if(ty.t==VSTR)return ty;
        return vs("Object");
    }
    return vs(type_name(a[0]));
}
static Value noop_class(VM*vm,int n,Value*a){
    (void)vm;
    /* Query form: oop.class(instance) -> class name.
       Factory form: oop.class("Name", fields) -> class descriptor. */
    if(n==1 && a[0].t==VSTRUCT){
        Value c=stget(a[0].u.st,"__class");
        if(c.t==VSTR)return c;
        c=stget(a[0].u.st,"__type");
        return c.t==VSTR?c:vn();
    }
    if(n<1||n>2||a[0].t!=VSTR)return vn();
    Value o=vsobj();
    stput(o.u.st,"__class",a[0]);
    stput(o.u.st,"__type",a[0]);
    if(n==2&&a[1].t==VSTRUCT){
        for(size_t i=0;i<a[1].u.st->n;i++)stput(o.u.st,a[1].u.st->v[i].name,a[1].u.st->v[i].value);
    }
    return o;
}

/* ===================== Haris v8.4 AI Systems ===================== */
#define HK_TRANSFORMER 0x54524E46
#define HK_RNN 0x524E4E31
#define HK_LSTM 0x4C53544D
#define HK_DIST 0x44535431

