/* ---------- Fast Web / Cloud ---------- */
typedef struct { char *p; size_t n, cap; int overflow; } HWebBuf;
static void hweb_free(HWebBuf*b){if(!b)return;xfree(b->p);b->p=NULL;b->n=b->cap=0;b->overflow=0;}
static size_t hweb_write(char *ptr,size_t sz,size_t nm,void*ud){
    HWebBuf*b=(HWebBuf*)ud;
    if(!b||sz==0)return 0;
    if(nm>SIZE_MAX/sz){b->overflow=1;return 0;}
    size_t add=sz*nm;
    if(add>H_DATA_MAX || b->n>H_DATA_MAX-add){b->overflow=1;return 0;}
    size_t need=b->n+add+1;
    if(need>b->cap){size_t nc=b->cap?b->cap:4096;while(nc<need){size_t nn=nc+(nc>>1);if(nn<nc||nn>H_DATA_MAX+1){nc=need;break;}nc=nn;}b->p=(char*)xrealloc(b->p,nc);b->cap=nc;}
    memcpy(b->p+b->n,ptr,add);b->n+=add;b->p[b->n]=0;return nm;
}
static void curl_fast_defaults(CURL*c){
    curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");
    /* Never downgrade a redirect from HTTPS to plaintext HTTP. */
    curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"https");
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(c,CURLOPT_SSLVERSION,CURL_SSLVERSION_TLSv1_2);
    /* Optional explicit trust store for embedded/portable deployments.
       Haris never disables peer/hostname verification merely because a custom
       CA bundle is supplied. */
    { const char *ca=getenv("HARIS_CA_BUNDLE"); if(ca&&*ca) curl_easy_setopt(c,CURLOPT_CAINFO,ca); }
    curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(c,CURLOPT_MAXREDIRS,5L);
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT_MS,4000L);
    curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,30000L);
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(c,CURLOPT_TCP_KEEPALIVE,1L);
    curl_easy_setopt(c,CURLOPT_TCP_NODELAY,1L);
    curl_easy_setopt(c,CURLOPT_DNS_CACHE_TIMEOUT,120L);
#ifdef CURLOPT_DOH_URL
    { const char *doh=getenv("HARIS_DOH_URL");
      if(doh&&strncasecmp(doh,"https://",8)==0){
        curl_easy_setopt(c,CURLOPT_DOH_URL,doh);
#ifdef CURLOPT_DOH_SSL_VERIFYPEER
        curl_easy_setopt(c,CURLOPT_DOH_SSL_VERIFYPEER,1L);
#endif
#ifdef CURLOPT_DOH_SSL_VERIFYHOST
        curl_easy_setopt(c,CURLOPT_DOH_SSL_VERIFYHOST,2L);
#endif
      }
    }
#endif
    curl_easy_setopt(c,CURLOPT_BUFFERSIZE,256L*1024L);
    curl_easy_setopt(c,CURLOPT_ACCEPT_ENCODING,"");
    curl_easy_setopt(c,CURLOPT_FORBID_REUSE,0L);
#ifdef CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS
    curl_easy_setopt(c,CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS,100L);
#endif
#ifdef CURLOPT_TCP_FASTOPEN
    curl_easy_setopt(c,CURLOPT_TCP_FASTOPEN,1L);
#endif
#ifdef CURL_HTTP_VERSION_2TLS
    curl_easy_setopt(c,CURLOPT_HTTP_VERSION,CURL_HTTP_VERSION_2TLS);
#endif
#ifdef CURLOPT_PIPEWAIT
    curl_easy_setopt(c,CURLOPT_PIPEWAIT,1L);
#endif
}
static HARIS_TLS CURLM *g_web_multi=NULL;
static CURLM *web_multi_get(void){
    if(g_web_multi)return g_web_multi;
    CURLM*m=curl_multi_init();
    if(!m)return NULL;
#ifdef CURLMOPT_PIPELINING
    curl_multi_setopt(m,CURLMOPT_PIPELINING,CURLPIPE_MULTIPLEX);
#endif
#ifdef CURLMOPT_MAX_HOST_CONNECTIONS
    curl_multi_setopt(m,CURLMOPT_MAX_HOST_CONNECTIONS,8L);
#endif
#ifdef CURLMOPT_MAX_TOTAL_CONNECTIONS
    curl_multi_setopt(m,CURLMOPT_MAX_TOTAL_CONNECTIONS,64L);
#endif
#ifdef CURLMOPT_MAX_CONNECTS
    curl_multi_setopt(m,CURLMOPT_MAX_CONNECTS,64L);
#endif
#ifdef CURLMOPT_MAX_CONCURRENT_STREAMS
    curl_multi_setopt(m,CURLMOPT_MAX_CONCURRENT_STREAMS,100L);
#endif
    g_web_multi=m;
    return m;
}
static void web_multi_wait(CURLM*m,int *running){
    while(*running){
        int numfds=0;
#if LIBCURL_VERSION_NUM >= 0x074200
        curl_multi_poll(m,NULL,0,250,&numfds);
#else
        curl_multi_wait(m,NULL,0,250,&numfds);
#endif
        CURLMcode mc=curl_multi_perform(m,running);
        while(mc==CURLM_CALL_MULTI_PERFORM)mc=curl_multi_perform(m,running);
        if(mc!=CURLM_OK)break;
    }
}
static Value web_result_from_easy(CURL*c,CURLcode rc,HWebBuf*b){
    Value r=vsobj();long code=0;double total=0;
    curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);
    curl_easy_getinfo(c,CURLINFO_TOTAL_TIME,&total);
    stput(r.u.st,"status",vi(code));
    stput(r.u.st,"ok",vb(rc==CURLE_OK&&code>=200&&code<400&&!b->overflow));
    stput(r.u.st,"time_ms",vf(total*1000.0));
    if(rc!=CURLE_OK){stput(r.u.st,"error",vs(curl_easy_strerror(rc)));return r;}
    if(b->overflow){stput(r.u.st,"error",vs("response exceeds H_DATA_MAX"));return r;}
    stput(r.u.st,"body",vs(b->p?b->p:""));
    return r;
}
static Value http_request_fast_impl(VM*vm,int n,Value*a){
    if(n<2||n>5||a[0].t!=VSTR||a[1].t!=VSTR)return vn();
    const char*m=a[0].u.s,*url=a[1].u.s,*body=(n>=3&&a[2].t==VSTR)?a[2].u.s:NULL;
    CURLM*multi=web_multi_get();if(!multi)return vn();
    CURL*c=curl_easy_init();if(!c)return vn();
    HWebBuf b={0};struct curl_slist*h=NULL;
    if(n>=4&&a[3].t==VARR)for(size_t i=0;i<a[3].u.a->n;i++)if(a[3].u.a->v[i].t==VSTR)h=curl_slist_append(h,a[3].u.a->v[i].u.s);
    curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,hweb_write);curl_easy_setopt(c,CURLOPT_WRITEDATA,&b);
    if(h)curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);
    if(!strcasecmp(m,"GET")){}else if(!strcasecmp(m,"HEAD"))curl_easy_setopt(c,CURLOPT_NOBODY,1L);
    else{curl_easy_setopt(c,CURLOPT_CUSTOMREQUEST,m);if(body)curl_easy_setopt(c,CURLOPT_POSTFIELDS,body);}
    long timeout=n==5&&a[4].t==VINT?a[4].u.i:30000;if(timeout>0)curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,timeout);
    CURLMcode rc_add=curl_multi_add_handle(multi,c);
    if(rc_add!=CURLM_OK){if(h)curl_slist_free_all(h);curl_easy_cleanup(c);hweb_free(&b);return vn();}
    int running=0;CURLMcode mc=curl_multi_perform(multi,&running);while(mc==CURLM_CALL_MULTI_PERFORM)mc=curl_multi_perform(multi,&running);
    if(mc==CURLM_OK)web_multi_wait(multi,&running);
    CURLcode rc=CURLE_FAILED_INIT;int q=0;CURLMsg*msg=NULL;while((msg=curl_multi_info_read(multi,&q))){if(msg->msg==CURLMSG_DONE&&msg->easy_handle==c){rc=msg->data.result;break;}}
    Value r=web_result_from_easy(c,rc,&b);
    curl_multi_remove_handle(multi,c);if(h)curl_slist_free_all(h);curl_easy_cleanup(c);hweb_free(&b);return r;
}
static int api_transient_status(long code){return code==408||code==425||code==429||(code>=500&&code<=599);}
static void api_backoff_ms(int attempt){
    unsigned ms=25u<<((attempt>4)?4:attempt);
    if(ms>400u)ms=400u;
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;ts.tv_sec=(time_t)(ms/1000u);ts.tv_nsec=(long)(ms%1000u)*1000000L;nanosleep(&ts,NULL);
#endif
}

static Value web_request_fast(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.request");return http_request_fast_impl(vm,n,a);}
static Value cloud_http_fast(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.request");return http_request_fast_impl(vm,n,a);}
static Value api_request(VM*vm,int n,Value*a){
    if(n<2||n>6||a[0].t!=VSTR||a[1].t!=VSTR)return vn();
    if(!cap_allowed(vm,CAP_WEB)&&!cap_allowed(vm,CAP_CLOUD))return vn();
    int retries=n==6&&a[5].t==VINT?(int)a[5].u.i:0;if(retries<0)retries=0;if(retries>5)retries=5;
    Value last=vn();
    for(int k=0;k<=retries;k++){
        last=http_request_fast_impl(vm,n>=5?5:n,a);
        if(last.t==VSTRUCT){
            Value ok=stget(last.u.st,"ok");
            if(ok.t==VBOOL&&ok.u.b)return last;
            Value st=stget(last.u.st,"status");
            long code=(st.t==VINT)?(long)st.u.i:0;
            if(k<retries && (st.t!=VINT || code==0 || api_transient_status(code))){api_backoff_ms(k);continue;}
        } else if(k<retries){api_backoff_ms(k);continue;}
        break;
    }
    return last;
}
static Value api_get(VM*vm,int n,Value*a){if(n<1||n>2)return vn();Value x[5]={vs("GET"),a[0],vn(),n==2?a[1]:vn(),vn()};return api_request(vm,n==2?4:2,x);}
static Value api_post_json(VM*vm,int n,Value*a){if(n<2||n>3)return vn();Value h=va();ap(h.u.a,vs("Content-Type: application/json"));if(n==3&&a[2].t==VSTR)ap(h.u.a,vs(a[2].u.s));Value x[4]={vs("POST"),a[0],a[1],h};return api_request(vm,4,x);}
static Value api_put_json(VM*vm,int n,Value*a){if(n<2||n>3)return vn();Value h=va();ap(h.u.a,vs("Content-Type: application/json"));if(n==3&&a[2].t==VSTR)ap(h.u.a,vs(a[2].u.s));Value x[4]={vs("PUT"),a[0],a[1],h};return api_request(vm,4,x);}
static Value api_delete(VM*vm,int n,Value*a){if(n<1||n>2)return vn();Value h=n==2&&a[1].t==VARR?a[1]:va();Value x[4]={vs("DELETE"),a[0],vn(),h};return api_request(vm,4,x);}
static Value api_json(VM*vm,int n,Value*a){if(n<2||n>5)return vn();Value r=api_request(vm,n,a);if(r.t==VSTRUCT){Value b=stget(r.u.st,"body");if(b.t==VSTR){Value z[1]={b};Value j=njson_parse(vm,1,z);stput(r.u.st,"json",j);}}return r;}
typedef struct {CURL *easy;HWebBuf body;struct curl_slist *headers;CURLcode rc;int active;} HApiBatchJob;
static Value api_batch(VM*vm,int n,Value*a){
    if(n<1||n>2||a[0].t!=VARR)return vn();
    if(!cap_allowed(vm,CAP_WEB)&&!cap_allowed(vm,CAP_CLOUD))return vn();
    Value out=va();size_t N=a[0].u.a->n;int maxp=n==2&&a[1].t==VINT?(int)a[1].u.i:32;if(maxp<1)maxp=1;if(maxp>256)maxp=256;
    for(size_t base=0;base<N;){
        size_t chunk=N-base<(size_t)maxp?N-base:(size_t)maxp;HApiBatchJob*jobs=(HApiBatchJob*)calloc(chunk,sizeof(*jobs));if(!jobs){while(out.u.a->n<N)ap(out.u.a,vn());break;}
        CURLM*multi=web_multi_get();if(!multi){free(jobs);while(out.u.a->n<N)ap(out.u.a,vn());break;}
        for(size_t j=0;j<chunk;j++){
            Value q=a[0].u.a->v[base+j];jobs[j].rc=CURLE_FAILED_INIT;
            if(q.t!=VSTRUCT){jobs[j].active=0;continue;}
            Value m=stget(q.u.st,"method"),u=stget(q.u.st,"url"),b=stget(q.u.st,"body"),h=stget(q.u.st,"headers"),to=stget(q.u.st,"timeout");
            if(m.t!=VSTR||u.t!=VSTR){jobs[j].active=0;continue;}
            CURL*c=curl_easy_init();if(!c)continue;jobs[j].easy=c;jobs[j].active=1;
            if(b.t==VSTR)curl_easy_setopt(c,CURLOPT_POSTFIELDS,b.u.s);
            if(h.t==VARR)for(size_t k=0;k<h.u.a->n;k++)if(h.u.a->v[k].t==VSTR)jobs[j].headers=curl_slist_append(jobs[j].headers,h.u.a->v[k].u.s);
            curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_URL,u.u.s);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,hweb_write);curl_easy_setopt(c,CURLOPT_WRITEDATA,&jobs[j].body);
            if(jobs[j].headers)curl_easy_setopt(c,CURLOPT_HTTPHEADER,jobs[j].headers);
            if(!strcasecmp(m.u.s,"GET")){}else if(!strcasecmp(m.u.s,"HEAD"))curl_easy_setopt(c,CURLOPT_NOBODY,1L);else curl_easy_setopt(c,CURLOPT_CUSTOMREQUEST,m.u.s);
            if(to.t==VINT&&to.u.i>0)curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,(long)to.u.i);
            if(curl_multi_add_handle(multi,c)!=CURLM_OK){if(jobs[j].headers)curl_slist_free_all(jobs[j].headers);hweb_free(&jobs[j].body);curl_easy_cleanup(c);jobs[j].easy=NULL;jobs[j].active=0;continue;}
        }
        int running=0;CURLMcode mc=curl_multi_perform(multi,&running);while(mc==CURLM_CALL_MULTI_PERFORM)mc=curl_multi_perform(multi,&running);if(mc==CURLM_OK)web_multi_wait(multi,&running);
        int qn=0;CURLMsg*msg;while((msg=curl_multi_info_read(multi,&qn))){if(msg->msg!=CURLMSG_DONE)continue;for(size_t j=0;j<chunk;j++)if(jobs[j].active&&jobs[j].easy==msg->easy_handle){jobs[j].rc=msg->data.result;break;}}
        for(size_t j=0;j<chunk;j++){if(!jobs[j].active){ap(out.u.a,vn());continue;}Value r=web_result_from_easy(jobs[j].easy,jobs[j].rc,&jobs[j].body);ap(out.u.a,r);curl_multi_remove_handle(multi,jobs[j].easy);if(jobs[j].headers)curl_slist_free_all(jobs[j].headers);curl_easy_cleanup(jobs[j].easy);hweb_free(&jobs[j].body);}
        free(jobs);base+=chunk;
    }
    return out;
}
static Value web_secure_get(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.secure_get");
    if(n<1||n>2||a[0].t!=VSTR)return vn();
    if(strncasecmp(a[0].u.s,"https://",8)!=0)return vn();
    return web_request_fast(vm,2,(Value[]){vs("GET"),a[0]});
}
static Value web_secure_pin(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.secure_pin");
    if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vn();if(strncasecmp(a[0].u.s,"https://",8)!=0)return vn();
    CURL*c=curl_easy_init();if(!c)return vn();HWebBuf b={0};curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_URL,a[0].u.s);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,hweb_write);curl_easy_setopt(c,CURLOPT_WRITEDATA,&b);curl_easy_setopt(c,CURLOPT_PINNEDPUBLICKEY,a[1].u.s);CURLcode rc=curl_easy_perform(c);Value r=vsobj();long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);stput(r.u.st,"ok",vb(rc==CURLE_OK&&code>=200&&code<400&&!b.overflow));stput(r.u.st,"status",vi(code));stput(r.u.st,"body",vs(b.p?b.p:""));if(rc!=CURLE_OK)stput(r.u.st,"error",vs(curl_easy_strerror(rc)));hweb_free(&b);curl_easy_cleanup(c);return r;
}
static char *dns_url_encode(const char*s){CURL*c=curl_easy_init();if(!c)return NULL;char*e=curl_easy_escape(c,s,(int)strlen(s));char*out=e?xdup(e):NULL;if(e)curl_free(e);curl_easy_cleanup(c);return out;}
static Value net_dns_resolve(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.dns.resolve");if(n<1||n>2||a[0].t!=VSTR)return vn();const char*host=a[0].u.s;const char*type=(n==2&&a[1].t==VSTR)?a[1].u.s:"A";if(!*host)return vn();
    if(!strcasecmp(type,"A")||!strcasecmp(type,"AAAA")){
        struct addrinfo h={0},*res=NULL;h.ai_family=!strcasecmp(type,"AAAA")?AF_INET6:AF_INET;h.ai_socktype=SOCK_STREAM;int rc=getaddrinfo(host,NULL,&h,&res);Value out=va();if(rc)return out;
        for(struct addrinfo*p=res;p;p=p->ai_next){char ip[INET6_ADDRSTRLEN]={0};void*addr=NULL;if(p->ai_family==AF_INET)addr=&((struct sockaddr_in*)p->ai_addr)->sin_addr;else if(p->ai_family==AF_INET6)addr=&((struct sockaddr_in6*)p->ai_addr)->sin6_addr;if(addr&&inet_ntop(p->ai_family,addr,ip,sizeof ip))ap(out.u.a,vs(ip));}freeaddrinfo(res);return out;
    }
    return vn();
}
static Value net_dns_doh(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"net.dns.doh");if(n<1||n>3||a[0].t!=VSTR)return vn();const char*host=a[0].u.s;const char*rr=(n>=2&&a[1].t==VSTR)?a[1].u.s:"A";const char*ep=(n==3&&a[2].t==VSTR)?a[2].u.s:"https://cloudflare-dns.com/dns-query";if(strncasecmp(ep,"https://",8)!=0)return vn();char*q=dns_url_encode(host);if(!q)return vn();char *rte=dns_url_encode(rr);if(!rte){xfree(q);return vn();}size_t z=strlen(ep)+strlen(q)+strlen(rte)+32;char*u=xmalloc(z);snprintf(u,z,"%s?name=%s&type=%s",ep,q,rte);xfree(q);xfree(rte);
    CURL*c=curl_easy_init();if(!c){xfree(u);return vn();}HWebBuf b={0};struct curl_slist*h=NULL;h=curl_slist_append(h,"accept: application/dns-json");curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_URL,u);curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,hweb_write);curl_easy_setopt(c,CURLOPT_WRITEDATA,&b);curl_easy_setopt(c,CURLOPT_HTTP_VERSION,CURL_HTTP_VERSION_2TLS);CURLcode rc=curl_easy_perform(c);long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);Value out=va();if(rc==CURLE_OK&&code>=200&&code<300&&!b.overflow&&b.p){Value arg=vs(b.p);Value j=njson_parse(vm,1,&arg);if(j.t==VSTRUCT){Value ans=stget(j.u.st,"Answer");if(ans.t==VARR){for(size_t i=0;i<ans.u.a->n;i++)if(ans.u.a->v[i].t==VSTRUCT){Value d=stget(ans.u.a->v[i].u.st,"data");if(d.t==VSTR)ap(out.u.a,d);}}}}xfree(u);if(h)curl_slist_free_all(h);hweb_free(&b);curl_easy_cleanup(c);return out;
}
static Value web_url_decode_value(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();char*x=web_url_decode(a[0].u.s);Value r=x?vs(x):vn();if(x)xfree(x);return r;}
static Value web_get_json(VM*vm,int n,Value*a){if(n<1||n>2)return vn();Value r;if(n==1)r=web_request_fast(vm,2,(Value[]){vs("GET"),a[0]});else r=web_request_fast(vm,4,(Value[]){vs("GET"),a[0],vn(),a[1]});if(r.t!=VSTRUCT)return vn();Value b=stget(r.u.st,"body");if(b.t==VSTR){Value x[1]={b};Value j=njson_parse(vm,1,x);stput(r.u.st,"json",j);}return r;}
static Value web_batch(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.batch");
    if(n<1||n>2||a[0].t!=VARR)return vn();
    int maxp=n==2&&a[1].t==VINT?(int)a[1].u.i:32;
    if(maxp<1)maxp=1;if(maxp>256)maxp=256;
    Value out=va();size_t N=a[0].u.a->n;
    CURLM*multi=web_multi_get();if(!multi)return out;
    for(size_t base=0;base<N;){
        size_t cnt=N-base;if(cnt>(size_t)maxp)cnt=(size_t)maxp;
        CURL**es=(CURL**)calloc(cnt,sizeof(CURL*));HWebBuf*bs=(HWebBuf*)calloc(cnt,sizeof(HWebBuf));CURLcode*results=(CURLcode*)malloc(cnt*sizeof(CURLcode));
        if(!es||!bs||!results){free(es);free(bs);free(results);break;}
        for(size_t j=0;j<cnt;j++)results[j]=CURLE_FAILED_INIT;
                for(size_t j=0;j<cnt;j++){
            Value u=a[0].u.a->v[base+j];if(u.t!=VSTR)continue;
            CURL*c=curl_easy_init();if(!c)continue;es[j]=c;curl_fast_defaults(c);
            curl_easy_setopt(c,CURLOPT_URL,u.u.s);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,hweb_write);curl_easy_setopt(c,CURLOPT_WRITEDATA,&bs[j]);
            if(curl_multi_add_handle(multi,c)!=CURLM_OK){curl_easy_cleanup(c);es[j]=NULL;continue;}
        }
        int running=0;CURLMcode mc=curl_multi_perform(multi,&running);while(mc==CURLM_CALL_MULTI_PERFORM)mc=curl_multi_perform(multi,&running);
        if(mc==CURLM_OK)web_multi_wait(multi,&running);
        int q=0;CURLMsg*msg=NULL;while((msg=curl_multi_info_read(multi,&q))){
            if(msg->msg==CURLMSG_DONE){for(size_t j=0;j<cnt;j++)if(es[j]==msg->easy_handle){results[j]=msg->data.result;break;}}
        }
        for(size_t j=0;j<cnt;j++){
            Value r=vsobj();if(!es[j]){stput(r.u.st,"ok",vb(0));stput(r.u.st,"error",vs("request setup failed"));ap(out.u.a,r);continue;}
            r=web_result_from_easy(es[j],results[j],&bs[j]);ap(out.u.a,r);curl_multi_remove_handle(multi,es[j]);curl_easy_cleanup(es[j]);hweb_free(&bs[j]);
        }
        free(es);free(bs);free(results);base+=cnt;
    }
    return out;
}
static Value web_url_encode(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();CURL*c=curl_easy_init();if(!c)return vn();char*e=curl_easy_escape(c,a[0].u.s,(int)strlen(a[0].u.s));Value r=e?vs(e):vn();if(e)curl_free(e);curl_easy_cleanup(c);return r;}
static size_t web_header_capture(char*p,size_t z,size_t n,void*u){Value*out=(Value*)u;size_t L=z*n;if(L&&p){char*line=(char*)malloc(L+1);if(!line)return L;memcpy(line,p,L);line[L]=0;char*colon=strchr(line,':');if(colon){*colon=0;char*val=trim(colon+1);if(out->t==VARR){Value kv=va();ap(kv.u.a,vs(trim(line)));ap(kv.u.a,vs(val));ap(out->u.a,kv);}}free(line);}return L;}
static Value web_request_headers(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.response_headers");if(n!=1||a[0].t!=VSTR)return vn();CURL*c=curl_easy_init();if(!c)return vn();Value h=va();curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_URL,a[0].u.s);curl_easy_setopt(c,CURLOPT_NOBODY,1L);curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,web_header_capture);curl_easy_setopt(c,CURLOPT_HEADERDATA,&h);curl_easy_perform(c);curl_easy_cleanup(c);return h;}
static Value web_download(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.download");if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!fs_path_allowed(vm,a[1].u.s))return vb(0);CURL*c=curl_easy_init();FILE*f=fopen(a[1].u.s,"wb");if(!c||!f){if(c)curl_easy_cleanup(c);if(f)fclose(f);return vb(0);}curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_URL,a[0].u.s);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,NULL);curl_easy_setopt(c,CURLOPT_WRITEDATA,f);CURLcode rc=curl_easy_perform(c);fclose(f);curl_easy_cleanup(c);return vb(rc==CURLE_OK);}
static Value web_upload(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.upload");if(n<3||n>4||a[0].t!=VSTR||a[1].t!=VSTR||a[2].t!=VSTR)return vb(0);CURL*c=curl_easy_init();if(!c)return vb(0);curl_mime*m=curl_mime_init(c);curl_mimepart*p=curl_mime_addpart(m);curl_mime_name(p,a[2].u.s);curl_mime_filedata(p,a[1].u.s);curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_URL,a[0].u.s);curl_easy_setopt(c,CURLOPT_MIMEPOST,m);if(n==4&&a[3].t==VARR){struct curl_slist*h=0;for(size_t i=0;i<a[3].u.a->n;i++)if(a[3].u.a->v[i].t==VSTR)h=curl_slist_append(h,a[3].u.a->v[i].u.s);curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);CURLcode rc=curl_easy_perform(c);if(h)curl_slist_free_all(h);curl_mime_free(m);curl_easy_cleanup(c);return vb(rc==CURLE_OK);}CURLcode rc=curl_easy_perform(c);curl_mime_free(m);curl_easy_cleanup(c);return vb(rc==CURLE_OK);}
static Value web_server_json(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.server_json");if(n<2||n>4||a[0].t!=VSTR||a[1].t!=VINT)return vb(0);Value j[1]={a[0]};Value r=njson_stringify(vm,1,j);Value args[4]={a[1],r,vn(),vn()};(void)args;return web_serve_static(vm,0,NULL);}
static Value cloud_env(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.env");if(n!=0)return vn();Value o=vsobj();const char*e[4]={getenv("HARIS_CLOUD_ENDPOINT"),getenv("HARIS_CLOUD_REGION"),getenv("HARIS_CLOUD_SERVICE"),getenv("HARIS_CLOUD_TOKEN")};stput(o.u.st,"endpoint",e[0]?vs(e[0]):vn());stput(o.u.st,"region",e[1]?vs(e[1]):vn());stput(o.u.st,"service",e[2]?vs(e[2]):vn());stput(o.u.st,"token",e[3]?vs(e[3]):vn());return o;}
static Value cloud_get_json(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.get_json");if(n<1||n>2)return vn();Value h=n==2&&a[1].t==VARR?a[1]:va();Value r=cloud_request(vm,4,(Value[]){vs("GET"),a[0],vn(),h});if(r.t==VSTRUCT){Value b=stget(r.u.st,"body");if(b.t==VSTR){Value x[1]={b};Value j=njson_parse(vm,1,x);stput(r.u.st,"json",j);}}return r;}
static Value cloud_post_json(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.post_json");if(n<2||n>3)return vn();Value h=va();ap(h.u.a,vs("Content-Type: application/json"));Value r=cloud_request(vm,n==3?4:3,(Value[]){vs("POST"),a[0],a[1],h});return r;}
static Value cloud_kv_get(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.kv_get");const char*base=getenv("HARIS_CLOUD_ENDPOINT");if(!base||!*base||n!=1||a[0].t!=VSTR)return vn();char u[2048];snprintf(u,sizeof u,"%s/kv/%s",base,a[0].u.s);Value q[1]={vs(u)};return cloud_get_json(vm,1,q);}
static Value cloud_kv_set(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_CLOUD))return cap_error(vm,CAP_CLOUD,"cloud.kv_set");const char*base=getenv("HARIS_CLOUD_ENDPOINT");if(!base||!*base||n!=2||a[0].t!=VSTR)return vn();char u[2048];snprintf(u,sizeof u,"%s/kv/%s",base,a[0].u.s);Value body[1]={a[1]};Value js=njson_stringify(vm,1,body);Value q[2]={vs(u),js};return cloud_post_json(vm,2,q);}
static Value cloud_storage_upload(VM*vm,int n,Value*a){return nput(vm,n,a);} 
static Value cloud_storage_download(VM*vm,int n,Value*a){return cloud_download(vm,n,a);} 

static Value ngdistance(VM*vm,int n,Value*a){(void)vm;if(n!=4)return vn();double dx=dn(a[2])-dn(a[0]),dy=dn(a[3])-dn(a[1]);return vf(sqrt(dx*dx+dy*dy));}
static Value ngseek(VM*vm,int n,Value*a){(void)vm;if(n!=6)return vn();double x=dn(a[0]),y=dn(a[1]),tx=dn(a[2]),ty=dn(a[3]),speed=dn(a[4]),dt=dn(a[5]),dx=tx-x,dy=ty-y,d=sqrt(dx*dx+dy*dy),step=speed*dt;if(d<1e-9)return (Value){.t=VARR,.u.a=({Arr*t=xmalloc(sizeof(Arr));*t=(Arr){0};ap(t,vf(x));ap(t,vf(y));t;})};if(step>d)step=d;Value o=va();ap(o.u.a,vf(x+dx/d*step));ap(o.u.a,vf(y+dy/d*step));return o;}
static Value ngflee(VM*vm,int n,Value*a){(void)vm;if(n!=6)return vn();double x=dn(a[0]),y=dn(a[1]),tx=dn(a[2]),ty=dn(a[3]),speed=dn(a[4]),dt=dn(a[5]),dx=x-tx,dy=y-ty,d=sqrt(dx*dx+dy*dy),step=speed*dt;Value o=va();if(d<1e-9){ap(o.u.a,vf(x));ap(o.u.a,vf(y));return o;}ap(o.u.a,vf(x+dx/d*step));ap(o.u.a,vf(y+dy/d*step));return o;}

static Value ngvec2(VM*vm,int n,Value*a){(void)vm;if(n!=2||!isnum(a[0])||!isnum(a[1]))return vn();Value o=va();ap(o.u.a,vf(dn(a[0])));ap(o.u.a,vf(dn(a[1])));return o;}
static Value ngmove(VM*vm,int n,Value*a){(void)vm;if(n!=5)return vn();double x=dn(a[0]),y=dn(a[1]),vx=dn(a[2]),vy=dn(a[3]),dt=dn(a[4]);Value o=va();ap(o.u.a,vf(x+vx*dt));ap(o.u.a,vf(y+vy*dt));return o;}
static Value ngarrive(VM*vm,int n,Value*a){(void)vm;if(n!=7)return vn();double x=dn(a[0]),y=dn(a[1]),tx=dn(a[2]),ty=dn(a[3]),speed=dn(a[4]),slowing=dn(a[5]),dt=dn(a[6]);double dx=tx-x,dy=ty-y,d=sqrt(dx*dx+dy*dy);Value o=va();if(d<1e-9){ap(o.u.a,vf(x));ap(o.u.a,vf(y));return o;}double desired=speed*(slowing>0?(d/slowing>1?1:d/slowing):1);double step=desired*dt;if(step>d)step=d;ap(o.u.a,vf(x+dx/d*step));ap(o.u.a,vf(y+dy/d*step));return o;}
static Value nglerp(VM*vm,int n,Value*a){(void)vm;if(n!=3||!isnum(a[0])||!isnum(a[1])||!isnum(a[2]))return vn();double t=dn(a[2]);if(t<0)t=0;if(t>1)t=1;return vf(dn(a[0])+(dn(a[1])-dn(a[0]))*t);}
static Value ngcircle_hit(VM*vm,int n,Value*a){(void)vm;if(n!=6)return vn();double dx=dn(a[2])-dn(a[0]),dy=dn(a[3])-dn(a[1]),r=dn(a[4])+dn(a[5]);return vb(dx*dx+dy*dy<=r*r);}
static Value ngaabb_hit(VM*vm,int n,Value*a){(void)vm;if(n!=8)return vn();double ax=dn(a[0]),ay=dn(a[1]),aw=dn(a[2]),ah=dn(a[3]),bx=dn(a[4]),by=dn(a[5]),bw=dn(a[6]),bh=dn(a[7]);return vb(ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by);}
static Value ngdamage(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||!isnum(a[0])||!isnum(a[1]))return vn();double hp=dn(a[0]),d=dn(a[1]),armor=n==3&&isnum(a[2])?dn(a[2]):0;if(d<0)d=0;if(armor<0)armor=0;double dealt=d*(100.0/(100.0+armor));hp-=dealt;if(hp<0)hp=0;Value o=va();ap(o.u.a,vf(hp));ap(o.u.a,vf(dealt));ap(o.u.a,vb(hp<=0));return o;}
static Value nggrid_path(VM*vm,int n,Value*a){(void)vm;if(n!=5||a[0].t!=VARR||a[0].u.a->n==0||a[1].t!=VINT||a[2].t!=VINT||a[3].t!=VINT||a[4].t!=VINT)return vn();int h=(int)a[0].u.a->n,w=a[0].u.a->v[0].t==VARR?(int)a[0].u.a->v[0].u.a->n:0;if(w<1||w>128||h>128)return vn();int sx=(int)a[1].u.i,sy=(int)a[2].u.i,gx=(int)a[3].u.i,gy=(int)a[4].u.i;if(sx<0||sx>=w||gx<0||gx>=w||sy<0||sy>=h||gy<0||gy>=h)return vn();int cells=w*h;unsigned char*seen=(unsigned char*)calloc((size_t)cells,1);int*px=(int*)malloc((size_t)cells*sizeof(int));int*q=(int*)malloc((size_t)cells*sizeof(int));int*qx=(int*)malloc((size_t)cells*sizeof(int));int*qy=(int*)malloc((size_t)cells*sizeof(int));if(!seen||!px||!q||!qx||!qy){free(seen);free(px);free(q);free(qx);free(qy);return vn();}for(int i=0;i<cells;i++)px[i]=-1;int head=0,tail=0,start=sy*w+sx;q[tail]=start;qx[tail]=sx;qy[tail]=sy;tail++;seen[start]=1;int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};while(head<tail){int cx=qx[head],cy=qy[head];int ci=q[head++];if(cx==gx&&cy==gy)break;for(int d=0;d<4;d++){int nx=cx+dirs[d][0],ny=cy+dirs[d][1];if(nx<0||nx>=w||ny<0||ny>=h)continue;int ni=ny*w+nx;if(seen[ni])continue;if(a[0].u.a->v[ny].t!=VARR||nx>=(int)a[0].u.a->v[ny].u.a->n)continue;Value cell=a[0].u.a->v[ny].u.a->v[nx];if(!(cell.t==VINT&&cell.u.i==0))continue;seen[ni]=1;px[ni]=ci;q[tail]=ni;qx[tail]=nx;qy[tail]=ny;tail++;}}int goal=gy*w+gx;if(!seen[goal]){free(seen);free(px);free(q);free(qx);free(qy);return va();}int len=0;for(int cur=goal;cur!=-1;cur=px[cur])len++;Value out=va();int cur=goal;int *path=(int*)malloc((size_t)len*sizeof(int));for(int i=0;i<len;i++){path[len-1-i]=cur;cur=px[cur];}for(int i=0;i<len;i++){Value pt=va();ap(pt.u.a,vi(path[i]%w));ap(pt.u.a,vi(path[i]/w));ap(out.u.a,pt);}free(path);free(seen);free(px);free(q);free(qx);free(qy);return out;}
static Value ngsafe(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vb(0);const char*s=a[0].u.s;return vb(!strstr(s,"..")&&s[0]!='/'&&!strchr(s,'\\')&&!(strlen(s)>1&&s[1]==':'));}
static Value nlogread(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"logs.read");if(n!=1||a[0].t!=VSTR)return vn();char*b=readf(a[0].u.s);if(!b)return vn();Value v=vs(b);xfree(b);return v;} static Value nlogcount(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"logs.count");if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vn();char*b=readf(a[0].u.s);if(!b)return vn();long long c=0;char*p=b;size_t m=strlen(a[1].u.s);while((p=strstr(p,a[1].u.s))){c++;p+=m?m:1;}xfree(b);return vi(c);}
static Value nloglevels(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"logs.levels");if(n!=1||a[0].t!=VSTR)return vn();char*b=readf(a[0].u.s);if(!b)return vn();long long info=0,warn=0,error=0;for(char*p=b;*p;){char*e=strchr(p,'\n');if(e)*e=0;for(char*q=p;*q;q++)*q=(char)toupper((unsigned char)*q);if(strstr(p,"ERROR"))error++;else if(strstr(p,"WARN"))warn++;else if(strstr(p,"INFO"))info++;if(!e) break;
    p=e+1;}xfree(b);Value o=va();ap(o.u.a,vi(info));ap(o.u.a,vi(warn));ap(o.u.a,vi(error));return o;}
static Value ndata_csv(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"data.csv");if(n!=1||a[0].t!=VSTR)return vn();char*b=readf(a[0].u.s);if(!b)return vn();Value rows=va();char*save=0;for(char*line=strtok_r(b,"\n",&save);line;line=strtok_r(0,"\n",&save)){Value row=va();char*sv=0;for(char*cell=strtok_r(line,",",&sv);cell;cell=strtok_r(0,",",&sv)){while(*cell==' '||*cell=='\r'||*cell=='\t')cell++;char*q=cell+strlen(cell);while(q>cell&&(q[-1]=='\r'||q[-1]==' '||q[-1]=='\t'))*--q=0;ap(row.u.a,vs(cell));}ap(rows.u.a,row);}xfree(b);return rows;}
static Value nstats(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR)return vn();Value o=va();if(!a[0].u.a->n){for(int i=0;i<5;i++)ap(o.u.a,vf(0));return o;}double sum=0,min=INFINITY,max=-INFINITY;size_t c=0;for(size_t i=0;i<a[0].u.a->n;i++)if(isnum(a[0].u.a->v[i])){double x=dn(a[0].u.a->v[i]);sum+=x;if(x<min)min=x;if(x>max)max=x;c++;}ap(o.u.a,vi((long long)c));ap(o.u.a,vf(sum));ap(o.u.a,vf(c?sum/c:0));ap(o.u.a,vf(c?min:0));ap(o.u.a,vf(c?max:0));return o;}
typedef struct {int found[6];} HeaderState;
static size_t discard_cb(char *ptr,size_t sz,size_t nm,void*ud){(void)ptr;(void)ud;return sz*nm;}
static size_t header_cb(char *ptr,size_t sz,size_t nm,void*ud){size_t n=sz*nm;HeaderState*h=(HeaderState*)ud;static const char*keys[]={"content-security-policy:","strict-transport-security:","x-content-type-options:","x-frame-options:","referrer-policy:","permissions-policy:"};for(int i=0;i<6;i++){size_t L=strlen(keys[i]);if(n>=L&&strncasecmp(ptr,keys[i],L)==0)h->found[i]=1;}return n;}
static Value nwebheaders(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.security_headers");if(n!=1||a[0].t!=VSTR)return vn();CURL*c=curl_easy_init();if(!c)return vn();HeaderState hs={{0}};curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_URL,a[0].u.s);curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,&hs);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,discard_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,NULL);CURLcode rc=curl_easy_perform(c);curl_easy_cleanup(c);if(rc!=CURLE_OK)return vn();Value o=va();for(int i=0;i<6;i++)ap(o.u.a,vb(hs.found[i]));return o;}
/* Real TLS certificate inspection over an https:// URL: chain fields
   (Subject, Issuer, expiry, etc, as libcurl/OpenSSL report them) plus
   whether verification against the system trust store passed. */
static Value web_tls_info(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.tls_info");
    if(n!=1||a[0].t!=VSTR)return vn();
    if(strncasecmp(a[0].u.s,"https://",8)!=0)return vn();
    CURL*c=curl_easy_init();if(!c)return vn();
    curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"https");curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"https");curl_easy_setopt(c,CURLOPT_URL,a[0].u.s);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,15000L);curl_easy_setopt(c,CURLOPT_CERTINFO,1L);curl_easy_setopt(c,CURLOPT_NOBODY,1L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,discard_cb);
    CURLcode rc=curl_easy_perform(c);
    Value out=vsobj();
    if(rc!=CURLE_OK){stput(out.u.st,"ok",vb(0));stput(out.u.st,"error",vs(curl_easy_strerror(rc)));curl_easy_cleanup(c);return out;}
    long verify=-1;curl_easy_getinfo(c,CURLINFO_SSL_VERIFYRESULT,&verify);
    struct curl_certinfo*ci=NULL;curl_easy_getinfo(c,CURLINFO_CERTINFO,&ci);
    Value certs=va();
    if(ci){for(int i=0;i<ci->num_of_certs;i++){Value fields=va();for(struct curl_slist*sl=ci->certinfo[i];sl;sl=sl->next)ap(fields.u.a,vs(sl->data));ap(certs.u.a,fields);}}
    stput(out.u.st,"ok",vb(1));stput(out.u.st,"verify_ok",vb(verify==0));stput(out.u.st,"certificates",certs);
    curl_easy_cleanup(c);
    return out;
}
/* Follows redirects one hop at a time (instead of libcurl auto-follow) so
   the caller can see the full chain of URLs/status codes - useful for
   spotting open-redirect / phishing-relay patterns, or just debugging a
   deploy's redirect rules. Capped at max_hops (default 10, max 20). */
static Value web_redirect_chain(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.redirect_chain");
    if(n<1||n>2||a[0].t!=VSTR)return vn();
    int max_hops=(n==2&&a[1].t==VINT)?(int)a[1].u.i:10;
    if(max_hops<1||max_hops>20)max_hops=10;
    Value chain=va();
    char cur[2048];snprintf(cur,sizeof cur,"%s",a[0].u.s);
    for(int hop=0;hop<max_hops;hop++){
        CURL*c=curl_easy_init();if(!c)break;
        curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_URL,cur);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_NOBODY,1L);curl_easy_setopt(c,CURLOPT_TIMEOUT,15L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,discard_cb);
        CURLcode rc=curl_easy_perform(c);
        if(rc!=CURLE_OK){curl_easy_cleanup(c);break;}
        long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);
        char*loc=NULL;curl_easy_getinfo(c,CURLINFO_REDIRECT_URL,&loc);
        Value hopv=vsobj();stput(hopv.u.st,"url",vs(cur));stput(hopv.u.st,"status",vi(code));ap(chain.u.a,hopv);
        int stop=!(code>=300&&code<400&&loc);
        if(loc&&strlen(loc)<sizeof cur)snprintf(cur,sizeof cur,"%s",loc);
        curl_easy_cleanup(c);
        if(stop)break;
    }
    return chain;
}
/* Content-discovery helper (like dirb/gobuster): tries each candidate path
   against base_url and reports the ones that don't 404. Bounded to 300
   paths per call - this is a lookup helper for paths you supply, not a
   flooding tool. */
static Value web_dir_bruteforce(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.dir_bruteforce");
    if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VARR)return vn();
    if(a[1].u.a->n>300)return vn();
    int timeout=(n==3&&a[2].t==VINT)?(int)a[2].u.i:2000;
    if(timeout<200||timeout>10000)timeout=2000;
    size_t blen=strlen(a[0].u.s);
    int base_has_slash=blen>0&&a[0].u.s[blen-1]=='/';
    Value found=va();
    for(size_t i=0;i<a[1].u.a->n;i++){
        if(a[1].u.a->v[i].t!=VSTR)continue;
        const char*path=a[1].u.a->v[i].u.s;
        int path_has_slash=path[0]=='/';
        char url[2048];int w=snprintf(url,sizeof url,"%s%s%s",a[0].u.s,(base_has_slash||path_has_slash)?"":"/",path);
        if(w<0||(size_t)w>=sizeof url)continue;
        CURL*c=curl_easy_init();if(!c)continue;
        curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_NOBODY,1L);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,(long)timeout);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,discard_cb);
        CURLcode rc=curl_easy_perform(c);
        if(rc==CURLE_OK){
            long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);
            if(code>0&&code<400){Value hv=vsobj();stput(hv.u.st,"path",vs(path));stput(hv.u.st,"status",vi(code));ap(found.u.a,hv);}
        }
        curl_easy_cleanup(c);
    }
    return found;
}
#ifdef HARIS_HAVE_LIBGIT2
static int git2_init_once(void){ static int done=0; if(!done){ if(git_libgit2_init()<0)return 0; done=1; } return 1; }
static int git2_open(const char *path, git_repository **out){ if(!path||!out||!safe_path_arg(path)||!git2_init_once())return -1; *out=NULL; return git_repository_open(out,path); }
static const char *git2_status_index_char(unsigned st){
    if(st&GIT_STATUS_INDEX_NEW)return "A"; if(st&GIT_STATUS_INDEX_MODIFIED)return "M"; if(st&GIT_STATUS_INDEX_DELETED)return "D";
    if(st&GIT_STATUS_INDEX_RENAMED)return "R"; if(st&GIT_STATUS_INDEX_TYPECHANGE)return "T"; if(st&GIT_STATUS_INDEX_CONFLICTED)return "U"; return " ";
}
static const char *git2_status_worktree_char(unsigned st){
    if(st&GIT_STATUS_WT_NEW)return "?"; if(st&GIT_STATUS_WT_MODIFIED)return "M"; if(st&GIT_STATUS_WT_DELETED)return "D";
    if(st&GIT_STATUS_WT_RENAMED)return "R"; if(st&GIT_STATUS_WT_TYPECHANGE)return "T"; if(st&GIT_STATUS_WT_CONFLICTED)return "U"; return " ";
}
static const char *git2_entry_path(const git_status_entry *e){
    if(!e)return "";
    if(e->head_to_index.new_file.path)return e->head_to_index.new_file.path;
    if(e->head_to_index.old_file.path)return e->head_to_index.old_file.path;
    if(e->index_to_workdir.new_file.path)return e->index_to_workdir.new_file.path;
    if(e->index_to_workdir.old_file.path)return e->index_to_workdir.old_file.path;
    return "";
}
static Value git_status_value(VM*vm,git_repository*repo,int as_objects){
    (void)vm; git_status_list *list=NULL; git_status_options o=GIT_STATUS_OPTIONS_INIT;
    o.show=GIT_STATUS_SHOW_INDEX_AND_WORKDIR; o.flags=GIT_STATUS_OPT_INCLUDE_UNTRACKED|GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS|GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
    if(git_status_list_new(&list,repo,&o)<0)return vn(); size_t n=git_status_list_entrycount(list); Value out=as_objects?va():vn();
    if(!as_objects){
        DBuf b; dbuf_init(&b);
        for(size_t i=0;i<n;i++){const git_status_entry*e=git_status_byindex(list,i);if(!e)continue;char line[8];snprintf(line,sizeof line,"%s%s %s\n",git2_status_index_char(e->status),git2_status_worktree_char(e->status),git2_entry_path(e));dbuf_puts(&b,line);}
        Value r=vs(b.d?b.d:""); if(b.d)xfree(b.d); git_status_list_free(list); return r;
    }
    for(size_t i=0;i<n;i++){const git_status_entry*e=git_status_byindex(list,i);if(!e)continue;Value z=vsobj();stput(z.u.st,"index",vs(git2_status_index_char(e->status)));stput(z.u.st,"worktree",vs(git2_status_worktree_char(e->status)));stput(z.u.st,"path",vs(git2_entry_path(e)));ap(out.u.a,z);} git_status_list_free(list);return out;
}
static Value ngitver(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.version");(void)a;if(n)return vn();if(!git2_init_once())return vs("libgit2 unavailable");int ma,mi,rv;git_libgit2_version(&ma,&mi,&rv);char z[64];snprintf(z,sizeof z,"libgit2 %d.%d.%d",ma,mi,rv);return vs(z);}
static Value ngitstatus(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.status");if(n!=1||a[0].t!=VSTR)return vn();git_repository*r=NULL;int rc=git2_open(a[0].u.s,&r);if(rc<0)return vs("libgit2: repository unavailable");Value out=git_status_value(vm,r,0);git_repository_free(r);return out;}
static Value ngitlog(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.log");if(n!=1||a[0].t!=VSTR)return vn();git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vn();git_revwalk*w=NULL;if(git_revwalk_new(&w,r)<0||git_revwalk_push_head(w)<0){if(w)git_revwalk_free(w);git_repository_free(r);return vn();}git_revwalk_sorting(w,GIT_SORT_TIME|GIT_SORT_TOPOLOGICAL);DBuf b;dbuf_init(&b);git_oid oid;int cnt=0;while(cnt<10&&git_revwalk_next(&oid,w)==0){git_commit*c=NULL;if(git_commit_lookup(&c,r,&oid)==0){char oh[9];git_oid_fmt(oh,&oid);oh[8]=0;const char*sum=git_commit_summary(c);dbuf_puts(&b,oh);dbuf_puts(&b," ");dbuf_puts(&b,sum?sum:"");dbuf_putc(&b,'\n');git_commit_free(c);cnt++;}}git_revwalk_free(w);git_repository_free(r);Value out=vs(b.d?b.d:"");if(b.d)xfree(b.d);return out;}
static Value git_info(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.info"); if(n!=1||a[0].t!=VSTR)return vn(); git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vn();
    Value o=vsobj();const char*root=git_repository_workdir(r);stput(o.u.st,"root",vs(root?root:""));int detached=git_repository_head_detached(r);stput(o.u.st,"detached",vb(detached));
    char branch[256]={0},head[128]={0},upstream[512]={0};long long ahead=0,behind=0;git_reference*ref=NULL;
    if(git_repository_head(&ref,r)==0){const char*rn=git_reference_shorthand(ref);if(rn)snprintf(branch,sizeof branch,"%s",rn);const git_oid*oid=git_reference_target(ref);if(oid)git_oid_tostr(head,sizeof head,oid);stput(o.u.st,"branch",vs(branch));stput(o.u.st,"head",vs(head));
        git_reference*up=NULL;if(!detached&&git_branch_upstream(&up,ref)==0){const char*un=git_reference_shorthand(up);if(un)snprintf(upstream,sizeof upstream,"%s",un);stput(o.u.st,"upstream",vs(upstream));const git_oid*lo=oid,*uo=git_reference_target(up);if(lo&&uo){size_t aa=0,bb=0;if(git_graph_ahead_behind(&aa,&bb,r,lo,uo)==0){ahead=(long long)aa;behind=(long long)bb;}}git_reference_free(up);}else stput(o.u.st,"upstream",vn());git_reference_free(ref);
    }else{stput(o.u.st,"branch",vs("HEAD"));stput(o.u.st,"head",vn());stput(o.u.st,"upstream",vn());}
    stput(o.u.st,"ahead",vi(ahead));stput(o.u.st,"behind",vi(behind));Value st=git_status_value(vm,r,0);int dirty=(st.t==VSTR&&st.u.s&&*st.u.s);stput(o.u.st,"dirty",vb(dirty));git_repository_free(r);return o;
}
static Value git_is_clean(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.is_clean");if(n!=1||a[0].t!=VSTR)return vb(0);git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vb(0);Value st=git_status_value(vm,r,0);int clean=st.t==VSTR&&(!st.u.s||!*st.u.s);git_repository_free(r);return vb(clean);}
static Value git_head(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.head");if(n!=1||a[0].t!=VSTR)return vn();git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vn();git_reference*ref=NULL;Value out=vn();if(git_repository_head(&ref,r)==0){const git_oid*oid=git_reference_target(ref);if(oid){char z[GIT_OID_HEXSIZE+1];git_oid_tostr(z,sizeof z,oid);out=vs(z);}git_reference_free(ref);}git_repository_free(r);return out;}
static Value git_status_files(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.status_files");if(n!=1||a[0].t!=VSTR)return vn();git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vn();Value out=git_status_value(vm,r,1);git_repository_free(r);return out;}
static Value git_diff_stat(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.diff_stat");if(n!=1||a[0].t!=VSTR)return vn();git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vn();git_diff*d=NULL;git_diff_options o=GIT_DIFF_OPTIONS_INIT;if(git_diff_tree_to_workdir_with_index(&d,r,NULL,&o)<0){git_repository_free(r);return vn();}size_t nd=git_diff_num_deltas(d);Value out=vsobj();stput(out.u.st,"files",vi((long long)nd));long long ins=0,del=0;for(size_t i=0;i<nd;i++){const git_diff_delta*de=git_diff_get_delta(d,i);if(de){ins+=(long long)de->old_file.size?0:0;del+=(long long)de->new_file.size?0:0;}}stput(out.u.st,"insertions",vi(ins));stput(out.u.st,"deletions",vi(del));git_diff_free(d);git_repository_free(r);return out;}
static Value ngitadd(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.add");if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vb(0);git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vb(0);git_index*idx=NULL;int rc=git_repository_index(&idx,r);if(rc==0)rc=git_index_add_bypath(idx,a[1].u.s);if(rc==0)rc=git_index_write(idx);if(idx)git_index_free(idx);git_repository_free(r);return vb(rc==0);}
static Value git_stage_all(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.stage_all");if(n!=1||a[0].t!=VSTR)return vb(0);git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vb(0);git_index*idx=NULL;int rc=git_repository_index(&idx,r);if(rc==0){git_strarray ps={0};rc=git_index_add_all(idx,&ps,GIT_INDEX_ADD_DEFAULT,NULL,NULL);}if(rc==0)rc=git_index_write(idx);if(idx)git_index_free(idx);git_repository_free(r);return vb(rc==0);}
static Value ngitclone(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.clone");if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vn();if(!safe_path_arg(a[0].u.s)||!safe_path_arg(a[1].u.s))return vn();git_repository*r=NULL;git_clone_options o=GIT_CLONE_OPTIONS_INIT;int rc=git2_init_once()?git_clone(&r,a[0].u.s,a[1].u.s,&o):GIT_ERROR;if(r)git_repository_free(r);return vb(rc==0);}
static Value ngitpull(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.pull");if(n!=1||a[0].t!=VSTR)return vb(0);git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vb(0);git_remote*rem=NULL;int rc=git_remote_lookup(&rem,r,"origin");if(rc==0){git_fetch_options fo=GIT_FETCH_OPTIONS_INIT;rc=git_remote_fetch(rem,NULL,&fo,NULL);}if(rem)git_remote_free(rem);git_repository_free(r);return vb(rc==0);}
static Value ngitpush(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.push");if(n<1||n>2||a[0].t!=VSTR)return vb(0);const char*rn=n==2&&a[1].t==VSTR?a[1].u.s:"origin";git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vb(0);git_remote*rem=NULL;int rc=git_remote_lookup(&rem,r,rn);if(rc==0){git_push_options po=GIT_PUSH_OPTIONS_INIT;rc=git_remote_push(rem,NULL,&po);}if(rem)git_remote_free(rem);git_repository_free(r);return vb(rc==0);}
static Value ngitcommit(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.commit");if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vb(0);git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vb(0);git_index*idx=NULL;git_tree*tree=NULL;git_reference*hr=NULL;git_commit*parent=NULL;git_signature*sig=NULL;git_oid toid,coid;int rc=git_repository_index(&idx,r);if(rc==0)rc=git_index_write_tree(&toid,idx);if(rc==0)rc=git_tree_lookup(&tree,r,&toid);if(rc==0)rc=git_signature_default(&sig,r);if(rc==0)rc=git_repository_head(&hr,r);if(rc==0)rc=git_commit_lookup(&parent,r,git_reference_target(hr));if(rc==0){const git_commit*parents[1]={parent};rc=git_commit_create(&coid,r,"HEAD",sig,sig,NULL,a[1].u.s,tree,1,parents);}if(sig)git_signature_free(sig);if(parent)git_commit_free(parent);if(hr)git_reference_free(hr);if(tree)git_tree_free(tree);if(idx)git_index_free(idx);git_repository_free(r);return vb(rc==0);}
static Value git_commit_if_dirty(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.commit_if_dirty");if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vn();if(git_is_clean(vm,1,(Value[]){a[0]}).u.b)return vs("clean");if(!git_stage_all(vm,1,(Value[]){a[0]}).u.b)return vs("stage failed");return ngitcommit(vm,2,(Value[]){a[0],a[1]});}
static Value ngitdiff(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.diff");return git_diff_stat(vm,n,a);}
static Value ngitbranch(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.branch");if(n!=1||a[0].t!=VSTR)return vn();git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vn();git_branch_iterator*it=NULL;int rc=git_branch_iterator_new(&it,r,GIT_BRANCH_LOCAL|GIT_BRANCH_REMOTE);Value out=va();if(rc==0){git_reference*ref=NULL;git_branch_t ty;const char*name=NULL;while(git_branch_next(&ref,&ty,it)==0){if(git_branch_name(&name,ref)==0&&name)ap(out.u.a,vs(name));git_reference_free(ref);ref=NULL;}}if(it)git_branch_iterator_free(it);git_repository_free(r);return out;}
static Value ngitcheckout(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.checkout");if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!safe_path_arg(a[0].u.s)||!safe_path_arg(a[1].u.s))return vb(0);git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vb(0);char refname[1024];int w=snprintf(refname,sizeof refname,"refs/heads/%s",a[1].u.s);if(w<0||(size_t)w>=sizeof refname){git_repository_free(r);return vb(0);}int rc=git_repository_set_head(r,refname);if(rc==0){git_checkout_options co=GIT_CHECKOUT_OPTIONS_INIT;co.checkout_strategy=GIT_CHECKOUT_SAFE;rc=git_checkout_head(r,&co);}git_repository_free(r);return vb(rc==0);}
static Value git_create_branch(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.create_branch");if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!safe_path_arg(a[0].u.s)||!safe_path_arg(a[1].u.s))return vb(0);git_repository*r=NULL;if(git2_open(a[0].u.s,&r)<0)return vb(0);git_reference*hr=NULL;git_commit*c=NULL;git_reference*br=NULL;int rc=git_repository_head(&hr,r);if(rc==0)rc=git_commit_lookup(&c,r,git_reference_target(hr));if(rc==0)rc=git_branch_create(&br,r,a[1].u.s,c,0);if(rc==0){char rn[1024];snprintf(rn,sizeof rn,"refs/heads/%s",a[1].u.s);rc=git_repository_set_head(r,rn);}if(br)git_reference_free(br);if(c)git_commit_free(c);if(hr)git_reference_free(hr);git_repository_free(r);return vb(rc==0);}
#else
static const char* git2_missing(void){return "libgit2 backend not available; rebuild with libgit2";}
static Value ngitver(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.version");(void)a;if(n)return vn();return vs(git2_missing());}
static Value ngitstatus(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.status");(void)a;if(n!=1)return vn();return vs(git2_missing());}
static Value ngitlog(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.log");(void)a;if(n!=1)return vn();return vs(git2_missing());}
static Value git_info(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.info");(void)a;if(n!=1)return vn();return vn();}
static Value git_is_clean(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.is_clean");(void)a;return vb(0);}
static Value git_head(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.head");(void)a;return vn();}
static Value git_status_files(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.status_files");(void)a;return va();}
static Value git_diff_stat(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.diff_stat");(void)a;return vn();}
static Value git_stage_all(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.stage_all");(void)a;return vb(0);}
static Value ngitclone(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.clone");(void)a;return vb(0);}
static Value ngitpull(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.pull");(void)a;return vb(0);}
static Value ngitpush(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.push");(void)a;return vb(0);}
static Value ngitadd(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.add");(void)a;return vb(0);}
static Value ngitcommit(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.commit");(void)a;return vb(0);}
static Value ngitdiff(VM*vm,int n,Value*a){return git_diff_stat(vm,n,a);}
static Value ngitbranch(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.branch");(void)a;return va();}
static Value ngitcheckout(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.checkout");(void)a;return vb(0);}
static Value git_commit_if_dirty(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.commit_if_dirty");(void)a;return vs(git2_missing());}
static Value git_create_branch(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_GIT))return cap_error(vm,CAP_GIT,"git.create_branch");(void)a;return vb(0);}
#endif
/* Machine-readable parser kept as a compatibility helper for old tests/tools. */
static int git_info_parse_status(const char*text,char*branch,size_t branch_n,char*head,size_t head_n,
                                 long long*ahead,long long*behind,int*dirty,int*detached,char*upstream,size_t upstream_n){
    if(!text||!branch||!head||!ahead||!behind||!dirty||!detached)return 0;branch[0]=0;head[0]=0;if(upstream&&upstream_n)upstream[0]=0;*ahead=*behind=0;*dirty=0;*detached=0;
    char*buf=xdup(text);char*save=NULL;for(char*line=strtok_r(buf,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){while(*line=='\r')line++;if(!strncmp(line,"# branch.head ",14)){snprintf(branch,branch_n,"%s",line+14);}else if(!strncmp(line,"# branch.oid ",13)){snprintf(head,head_n,"%s",line+13);}else if(!strncmp(line,"# branch.upstream ",18)&&upstream&&upstream_n){snprintf(upstream,upstream_n,"%s",line+18);}else if(!strncmp(line,"# branch.ab ",12)){char sa[64]={0},sb[64]={0};if(sscanf(line+12,"%63s %63s",sa,sb)==2){*ahead=atoll(sa);*behind=atoll(sb);}}else if(line[0]=='1'||line[0]=='2'||line[0]=='u'||line[0]=='?'){*dirty=1;}}
    *detached=(!strcmp(branch,"(detached)")||!strcmp(branch,"HEAD"));xfree(buf);return head[0]!=0;
}

