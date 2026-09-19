/* ---------------- Web-native site builder/server ----------------
   Safe-by-default helpers for building small static sites and a localhost
   development server. Dynamic application routing can be layered on top of
   the VM later; this layer deliberately does not execute filesystem paths or
   templates as code. */
static int web_mkdirs(const char *path){
    if(!path||!*path)return 0;
    char *buf=xdup(path); size_t n=strlen(buf);
    for(size_t i=1;i<n;i++){
        if(buf[i]=='/'||buf[i]=='\\'){
            char keep=buf[i]; buf[i]=0;
            if(*buf && access(buf,F_OK)!=0){
                if(mkdir(buf,0777)!=0 && errno!=EEXIST){buf[i]=keep;xfree(buf);return 0;}
            }
            buf[i]=keep;
        }
    }
    if(access(buf,F_OK)!=0 && mkdir(buf,0777)!=0 && errno!=EEXIST){xfree(buf);return 0;}
    xfree(buf); return 1;
}
static int web_write_text(const char *path,const char *text){
    if(!path||!text)return 0; FILE*f=fopen(path,"wb"); if(!f)return 0;
    size_t n=strlen(text),w=fwrite(text,1,n,f); int rc=fclose(f); return w==n && rc==0;
}
static int web_rel_safe(const char *rel){
    if(!rel||!*rel)return 0;
    const char *p=rel; while(*p){
        while(*p=='/'||*p=='\\')p++;
        const char *st=p; while(*p&&*p!='/'&&*p!='\\')p++; size_t n=(size_t)(p-st);
        if(n==2&&!strncmp(st,"..",2))return 0;
        if(n==0)continue;
    }
    return 1;
}
static char *web_url_decode(const char *s){
    size_t n=strlen(s),cap=n+1,pos=0; char*out=xmalloc(cap);
    for(size_t i=0;i<n;i++){
        unsigned char c=(unsigned char)s[i];
        if(c=='%'&&i+2<n&&isxdigit((unsigned char)s[i+1])&&isxdigit((unsigned char)s[i+2])){
            int h=isdigit((unsigned char)s[i+1])?s[i+1]-'0':tolower((unsigned char)s[i+1])-'a'+10;
            int l=isdigit((unsigned char)s[i+2])?s[i+2]-'0':tolower((unsigned char)s[i+2])-'a'+10;
            c=(unsigned char)(h*16+l); i+=2;
        } else if(c=='+') c=' ';
        if(c==0){xfree(out);return NULL;} out[pos++]=(char)c;
    }
    out[pos]=0; return out;
}
static const char *web_mime(const char *path){
    const char *d=strrchr(path,'.'); if(!d)return "application/octet-stream";
    if(!strcasecmp(d,".html")||!strcasecmp(d,".htm"))return "text/html; charset=utf-8";
    if(!strcasecmp(d,".css"))return "text/css; charset=utf-8";
    if(!strcasecmp(d,".js"))return "text/javascript; charset=utf-8";
    if(!strcasecmp(d,".json"))return "application/json; charset=utf-8";
    if(!strcasecmp(d,".txt"))return "text/plain; charset=utf-8";
    if(!strcasecmp(d,".svg"))return "image/svg+xml";
    if(!strcasecmp(d,".png"))return "image/png";
    if(!strcasecmp(d,".jpg")||!strcasecmp(d,".jpeg"))return "image/jpeg";
    if(!strcasecmp(d,".gif"))return "image/gif";
    if(!strcasecmp(d,".webp"))return "image/webp";
    if(!strcasecmp(d,".ico"))return "image/x-icon";
    if(!strcasecmp(d,".wasm"))return "application/wasm";
    return "application/octet-stream";
}
static char *web_join(const char *root,const char *rel){
    if(!root||!rel||!web_rel_safe(rel))return NULL;
    size_t nr=strlen(root),nn=strlen(rel); while(nn&&(*rel=='/'||*rel=='\\')){rel++;nn--;}
    char sep=(nr&&root[nr-1]!='/'&&root[nr-1]!='\\')?'/':0;
    size_t z=nr+(sep?1:0)+nn+1; char*out=xmalloc(z);
    snprintf(out,z,"%s%s%s",root,sep?"/":"",rel); return out;
}
static char *web_escape_html_attr(const char *s){
    size_t cap=strlen(s)*6+1,pos=0;char*out=xmalloc(cap);
    for(const unsigned char*p=(const unsigned char*)s;*p;p++){
        const char*r=NULL;switch(*p){case '&':r="&amp;";break;case '<':r="&lt;";break;case '>':r="&gt;";break;case '"':r="&quot;";break;case '\'':r="&#39;";break;default:if(pos+2<cap)out[pos++]=(char)*p;continue;}size_t z=strlen(r);memcpy(out+pos,r,z);pos+=z;}
    out[pos]=0;return out;
}
static Value web_html(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.html");
    if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VSTR)return vn(); const char*lang="en";
    if(n==3){if(a[2].t!=VSTR)return vn();lang=a[2].u.s;}
    char *et=web_escape_html_attr(a[0].u.s), *el=web_escape_html_attr(lang); size_t cap=strlen(et)+strlen(a[1].u.s)+strlen(el)+256; char*out=xmalloc(cap);
    snprintf(out,cap,"<!doctype html><html lang=\"%s\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>%s</title></head><body>%s</body></html>",el,et,a[1].u.s);
    Value v=vs(out);xfree(out);xfree(et);xfree(el);return v;
}
static Value web_render(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.render");
    if(n!=2||a[0].t!=VSTR||a[1].t!=VSTRUCT)return vn();
    char *out=xdup(a[0].u.s); size_t used=strlen(out)+1;
    for(size_t i=0;i<a[1].u.st->n;i++){
        const char *k=a[1].u.st->v[i].name; Value v=a[1].u.st->v[i].value;
        if(v.t!=VSTR&&v.t!=VINT&&v.t!=VFLOAT&&v.t!=VBOOL)continue;
        char val[128]; const char *rep=v.t==VSTR?v.u.s:((v.t==VINT?(snprintf(val,sizeof val,"%lld",v.u.i),val):(v.t==VFLOAT?(snprintf(val,sizeof val,"%.17g",v.u.f),val):(v.u.b?"true":"false"))));
        size_t kl=strlen(k)+4, vl=strlen(rep); char *tag=xmalloc(kl);snprintf(tag,kl,"{{%s}}",k);
        size_t nt=0; for(char*q=out;(q=strstr(q,tag));q+=kl)nt++;
        if(nt){size_t old=strlen(out); size_t nc = old + 1; if(vl>=kl) nc += nt*(vl-kl); else { size_t shrink=nt*(kl-vl); nc = shrink>old ? 1 : old-shrink+1; } char*neo=xmalloc(nc);size_t wp=0;char*q=out;while(1){char*h=strstr(q,tag);if(!h){size_t z=strlen(q);memcpy(neo+wp,q,z);wp+=z;break;}size_t z=(size_t)(h-q);memcpy(neo+wp,q,z);wp+=z;memcpy(neo+wp,rep,vl);wp+=vl;q=h+kl;}neo[wp]=0;xfree(out);out=neo;used=wp+1;}
        xfree(tag); (void)used;
    }
    Value v=vs(out);xfree(out);return v;
}
static Value web_build(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.build");
    if(n<2||n>5||a[0].t!=VSTR||a[1].t!=VSTR)return vb(0);
    const char *root=a[0].u.s,*title=a[1].u.s,*body=n>=3&&a[2].t==VSTR?a[2].u.s:"<h1>Hello from Haris</h1>";
    const char *css=n>=4&&a[3].t==VSTR?a[3].u.s:"body{font-family:sans-serif;max-width:900px;margin:40px auto;padding:0 16px}h1{font-size:2rem}";
    const char *js=n>=5&&a[4].t==VSTR?a[4].u.s:"console.log('Haris web app');";
    if(n>=3&&a[2].t!=VSTR)return vb(0); if(n>=4&&a[3].t!=VSTR)return vb(0); if(n>=5&&a[4].t!=VSTR)return vb(0);
    if(!web_mkdirs(root))return vb(0); char *p1=web_join(root,"index.html"),*p2=web_join(root,"style.css"),*p3=web_join(root,"app.js");if(!p1||!p2||!p3){xfree(p1);xfree(p2);xfree(p3);return vb(0);}
    char *safe_title=web_escape_html_attr(title); size_t cap=strlen(safe_title)+strlen(body)+strlen(css)+strlen(js)+512;char*h=xmalloc(cap);snprintf(h,cap,"<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>%s</title><link rel=\"stylesheet\" href=\"style.css\"></head><body>%s<script src=\"app.js\"></script></body></html>",title,body);
    int ok=web_write_text(p1,h)&&web_write_text(p2,css)&&web_write_text(p3,js);xfree(h);xfree(safe_title);xfree(p1);xfree(p2);xfree(p3);return vb(ok);
}

#ifdef _WIN32
typedef SOCKET haris_socket_t;
#define HARIS_INVALID_SOCKET INVALID_SOCKET
#define haris_socket_close closesocket
static int g_wsa_ready=0;
static int haris_socket_init(void){if(g_wsa_ready)return 1;WSADATA w;if(WSAStartup(MAKEWORD(2,2),&w)!=0)return 0;g_wsa_ready=1;return 1;}
#else
typedef int haris_socket_t;
#define HARIS_INVALID_SOCKET (-1)
#define haris_socket_close close
static int haris_socket_init(void){return 1;}
#endif
static int haris_socket_set_timeouts(haris_socket_t s,int recv_ms,int send_ms){
    if(s==HARIS_INVALID_SOCKET)return 0;
#ifdef _WIN32
    DWORD r=(DWORD)(recv_ms<0?0:recv_ms), w=(DWORD)(send_ms<0?0:send_ms);
    if(setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(const char*)&r,sizeof r)!=0)return 0;
    if(setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,(const char*)&w,sizeof w)!=0)return 0;
    return 1;
#else
    struct timeval rt={(long)(recv_ms/1000),(long)((recv_ms%1000)*1000)};
    struct timeval wt={(long)(send_ms/1000),(long)((send_ms%1000)*1000)};
    if(setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&rt,sizeof rt)!=0)return 0;
    if(setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&wt,sizeof wt)!=0)return 0;
    return 1;
#endif
}
static void haris_socket_tune_server(haris_socket_t s){
    int one=1;
    setsockopt(s,SOL_SOCKET,SO_KEEPALIVE,(const char*)&one,sizeof one);
#ifdef TCP_NODELAY
    setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(const char*)&one,sizeof one);
#endif
    haris_socket_set_timeouts(s,15000,15000);
}
static int http_header_value_safe(const char *s){
    if(!s||!*s)return 0;
    for(const unsigned char*p=(const unsigned char*)s;*p;p++)if(*p=='\r'||*p=='\n'||*p==0)return 0;
    return 1;
}
static const char *http_reason(int status){
    switch(status){case 100:return "Continue";case 101:return "Switching Protocols";case 200:return "OK";case 201:return "Created";case 202:return "Accepted";case 204:return "No Content";case 206:return "Partial Content";case 301:return "Moved Permanently";case 302:return "Found";case 304:return "Not Modified";case 307:return "Temporary Redirect";case 308:return "Permanent Redirect";case 400:return "Bad Request";case 401:return "Unauthorized";case 403:return "Forbidden";case 404:return "Not Found";case 405:return "Method Not Allowed";case 408:return "Request Timeout";case 409:return "Conflict";case 413:return "Payload Too Large";case 415:return "Unsupported Media Type";case 422:return "Unprocessable Content";case 429:return "Too Many Requests";case 500:return "Internal Server Error";case 501:return "Not Implemented";case 502:return "Bad Gateway";case 503:return "Service Unavailable";case 504:return "Gateway Timeout";default:return NULL;}
}
static int http_status_valid(int status){return status>=100&&status<=599&&http_reason(status)!=NULL;}
static int web_path_inside_root(const char *root,const char *full){
    if(!root||!full)return 0;
#ifdef _WIN32
    char rr[MAX_PATH],ff[MAX_PATH];
    if(!_fullpath(rr,root,sizeof rr)||!_fullpath(ff,full,sizeof ff))return 0;
#else
    char rr[PATH_MAX],ff[PATH_MAX];
    if(!realpath(root,rr)||!realpath(full,ff))return 0;
#endif
    size_t n=strlen(rr);while(n>1&&(rr[n-1]=='/'||rr[n-1]=='\\'))rr[--n]=0;
#ifdef _WIN32
    for(size_t i=0;i<n;i++)rr[i]=(char)tolower((unsigned char)rr[i]);
    for(size_t i=0;ff[i];i++)ff[i]=(char)tolower((unsigned char)ff[i]);
#endif
    if(strncmp(rr,ff,n)!=0)return 0;
    return ff[n]==0||ff[n]=='/'||ff[n]=='\\';
}
static int haris_socket_last_error(void){
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}
static int haris_socket_set_nonblock(haris_socket_t s,int on){
#ifdef _WIN32
    u_long v=on?1UL:0UL;return ioctlsocket(s,FIONBIO,&v)==0;
#else
    int f=fcntl(s,F_GETFL,0);if(f<0)return 0;if(on)f|=O_NONBLOCK;else f&=~O_NONBLOCK;return fcntl(s,F_SETFL,f)==0;
#endif
}
static int haris_socket_wait(haris_socket_t s,int rd,int wr,int timeout_ms){
    fd_set rf,wf;FD_ZERO(&rf);FD_ZERO(&wf);if(rd)FD_SET(s,&rf);if(wr)FD_SET(s,&wf);
    struct timeval tv={(long)(timeout_ms/1000),(long)((timeout_ms%1000)*1000)};
#ifdef _WIN32
    int rc=select(0,rd?&rf:NULL,wr?&wf:NULL,NULL,&tv);return rc>0;
#else
    int rc=select((int)s+1,rd?&rf:NULL,wr?&wf:NULL,NULL,&tv);return rc>0;
#endif
}
static int haris_socket_send_all(haris_socket_t s,const void*p,size_t n,SSL*ssl){
    const unsigned char*b=(const unsigned char*)p;while(n){int z;
#ifdef HARIS_SSL_DISABLED
    z=(int)send(s,(const char*)b,(int)(n>INT_MAX?INT_MAX:n),0);
#else
    z=ssl?SSL_write(ssl,b,(int)(n>INT_MAX?INT_MAX:n)):(int)send(s,(const char*)b,(int)(n>INT_MAX?INT_MAX:n),0);
#endif
    if(z<=0)return 0;b+=z;n-=(size_t)z;}return 1;
}
static int haris_socket_recv_exact(haris_socket_t s,void*p,size_t n,SSL*ssl,int timeout_ms){
    unsigned char*b=(unsigned char*)p;size_t got=0;if(timeout_ms>=0)haris_socket_set_nonblock(s,1);
    while(got<n){if(timeout_ms>=0&&!haris_socket_wait(s,1,0,timeout_ms)){haris_socket_set_nonblock(s,0);return 0;}int z;
#ifdef HARIS_SSL_DISABLED
    z=(int)recv(s,(char*)b,(int)(n-got),0);
#else
    if(ssl){z=SSL_read(ssl,b,(int)(n-got));}else z=(int)recv(s,(char*)b,(int)(n-got),0);
#endif
    if(z<=0){haris_socket_set_nonblock(s,0);return 0;}got+=(size_t)z;b+=z;
    }
    if(timeout_ms>=0)haris_socket_set_nonblock(s,0);return 1;
}

typedef struct HUDP { int kind; haris_socket_t s; int connected; int nonblock; char *host; int port; } HUDP;
typedef struct HWSListener { int kind; haris_socket_t s; int tls; SSL_CTX *ctx; char *host; int port; } HWSListener;
typedef struct HWS { int kind; haris_socket_t s; int is_server; int tls; SSL_CTX *ctx; SSL *ssl; char *url; int closed; } HWS;
#define HK_UDP 0x55445001
#define HK_WS_LISTENER 0x57534C31
#define HK_WS 0x57534331
#define HARIS_NET_MAX_FRAME (8ULL*1024ULL*1024ULL)
#define HARIS_HTTP_HANDSHAKE_MAX (16ULL*1024ULL)

static void hudp_dtor(void*p){HUDP*u=(HUDP*)p;if(!u)return;if(u->s!=HARIS_INVALID_SOCKET)haris_socket_close(u->s);if(u->host)xfree(u->host);u->s=HARIS_INVALID_SOCKET;u->host=NULL;}
static void hwslistener_dtor(void*p){HWSListener*l=(HWSListener*)p;if(!l)return;if(l->s!=HARIS_INVALID_SOCKET)haris_socket_close(l->s);if(l->ctx)SSL_CTX_free(l->ctx);if(l->host)xfree(l->host);l->s=HARIS_INVALID_SOCKET;l->ctx=NULL;l->host=NULL;}
static void hws_dtor(void*p){HWS*w=(HWS*)p;if(!w)return;if(w->ssl){SSL_shutdown(w->ssl);SSL_free(w->ssl);w->ssl=NULL;}if(w->s!=HARIS_INVALID_SOCKET)haris_socket_close(w->s);if(w->ctx&&!w->is_server)SSL_CTX_free(w->ctx);if(w->url)xfree(w->url);w->s=HARIS_INVALID_SOCKET;w->ctx=NULL;w->url=NULL;w->closed=1;}

static int net_value_bytes(Value v,const unsigned char **pp,size_t *np){
    if(v.t==VSTR){*pp=(const unsigned char*)v.u.s;*np=strlen(v.u.s);return 1;}
    return 0;
}
static int net_value_bytes_copy(Value v,unsigned char **out,size_t *n){
    if(v.t==VSTR){*n=strlen(v.u.s);*out=(unsigned char*)xmalloc(*n?*n:1);if(*n)memcpy(*out,v.u.s,*n);return 1;}
    if(v.t==VARR){if(v.u.a->n>HARIS_NET_MAX_FRAME)return 0;*n=v.u.a->n;*out=(unsigned char*)xmalloc(*n?*n:1);for(size_t i=0;i<*n;i++){Value x=v.u.a->v[i];if(x.t!=VINT||x.u.i<0||x.u.i>255){xfree(*out);*out=NULL;return 0;}(*out)[i]=(unsigned char)x.u.i;}return 1;}
    return 0;
}
static Value net_bytes_array(const unsigned char*p,size_t n){Value o=va();if(!arr_reserve(o.u.a,n))return vn();for(size_t i=0;i<n;i++)ap(o.u.a,vi(p[i]));return o;}

static int resolve_and_socket(const char*host,int port,int socktype,haris_socket_t*out){char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo h={0},*r=NULL;h.ai_socktype=socktype;h.ai_family=AF_UNSPEC;h.ai_flags=0;if(getaddrinfo(host,ps,&h,&r)!=0)return 0;haris_socket_t s=HARIS_INVALID_SOCKET;for(struct addrinfo*p=r;p;p=p->ai_next){s=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(s==HARIS_INVALID_SOCKET)continue;if(connect(s,p->ai_addr,(socklen_t)p->ai_addrlen)==0){*out=s;freeaddrinfo(r);return 1;}haris_socket_close(s);s=HARIS_INVALID_SOCKET;}freeaddrinfo(r);return 0;}
static int parse_host_port(const char*text,char*host,size_t hcap,int*port,int defport){
    if(!text||!*text)return 0;const char*s=text;const char*colon=strrchr(s,':');int p=defport;
    if(colon&&strchr(s,':')==colon){char num[16];size_t L=(size_t)(colon-s);if(L==0||L>=hcap)return 0;memcpy(host,s,L);host[L]=0;snprintf(num,sizeof num,"%s",colon+1);char*e=NULL;long x=strtol(num,&e,10);if(!e||*e||x<1||x>65535)return 0;p=(int)x;}
    else {if(strlen(s)>=hcap)return 0;snprintf(host,hcap,"%s",s);}*port=p;return 1;
}
static int parse_ws_url(const char*url,int*secure,char*host,size_t hcap,int*port,char*path,size_t pcap){
    const char*rest=NULL;*secure=0;if(!strncasecmp(url,"ws://",5)){rest=url+5;*port=80;}else if(!strncasecmp(url,"wss://",6)){rest=url+6;*secure=1;*port=443;}else return 0;
    const char*slash=strchr(rest,'/');const char*end=slash?slash:rest+strlen(rest);const char*colon=NULL;for(const char*p=rest;p<end;p++)if(*p==':')colon=p;
    size_t hl=(size_t)((colon?colon:end)-rest);if(hl==0||hl>=hcap)return 0;memcpy(host,rest,hl);host[hl]=0;if(colon){char num[16];size_t L=(size_t)(end-colon-1);if(L==0||L>=sizeof num)return 0;memcpy(num,colon+1,L);num[L]=0;char*e=NULL;long x=strtol(num,&e,10);if(!e||*e||x<1||x>65535)return 0;*port=(int)x;}
    if(slash){if(strlen(slash)>=pcap)return 0;snprintf(path,pcap,"%s",*slash?slash:"/");}else snprintf(path,pcap,"/");return 1;
}
static int random_bytes(unsigned char*p,size_t n){return n==0||(p&&n<=INT_MAX&&RAND_bytes(p,(int)n)==1);}
static char* ws_accept_key(const char*key){static const char suffix[]="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";size_t z=strlen(key)+36;char*in=(char*)xmalloc(z+1);snprintf(in,z+1,"%s%s",key,suffix);unsigned char d[EVP_MAX_MD_SIZE];unsigned int dn=0;EVP_MD_CTX*c=EVP_MD_CTX_new();if(!c){xfree(in);return NULL;}int ok=EVP_DigestInit_ex(c,EVP_sha1(),NULL)==1&&EVP_DigestUpdate(c,in,strlen(in))==1&&EVP_DigestFinal_ex(c,d,&dn)==1;EVP_MD_CTX_free(c);xfree(in);if(!ok||dn!=20)return NULL;return b64_encode_bytes(d,20);}
static int ws_read_http(haris_socket_t s,SSL*ssl,char*buf,size_t cap){size_t n=0;buf[0]=0;while(n+1<cap){if(!haris_socket_recv_exact(s,buf+n,1,ssl,5000))return 0;n++;buf[n]=0;if(n>=4&&!memcmp(buf+n-4,"\r\n\r\n",4))return 1;}return 0;}
static const char* http_header_value(const char*req,const char*name,char*out,size_t cap){const char*p=req;size_t nl=strlen(name);while(*p){const char*e=strstr(p,"\r\n");if(!e)break;if((size_t)(e-p)>=nl&&strncasecmp(p,name,nl)==0&&p[nl]==':'){const char*v=p+nl+1;while(*v==' '||*v=='\t')v++;size_t L=(size_t)(e-v);if(L>=cap)L=cap-1;memcpy(out,v,L);out[L]=0;return out;}p=e+2;if(e==req)break;}return NULL;}
static int ws_has_token_ci(const char*value,const char*token){const char*p=value;size_t n=strlen(token);while(*p){while(*p==','||isspace((unsigned char)*p))p++;const char*e=p;while(*e&&*e!=',')e++;size_t L=(size_t)(e-p);while(L&&isspace((unsigned char)p[L-1]))L--;if(L==n&&!strncasecmp(p,token,n))return 1;p=*e?e+1:e;}return 0;}
static int ws_client_handshake(HWS*w,const char*host,const char*path){unsigned char rnd[16];if(!random_bytes(rnd,sizeof rnd))return 0;char*key=b64_encode_bytes(rnd,sizeof rnd);if(!key)return 0;char*expect=ws_accept_key(key);if(!expect){xfree(key);return 0;}size_t cap=strlen(host)+strlen(path)+strlen(key)+512;char*req=(char*)xmalloc(cap);snprintf(req,cap,"GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",path,host,key);int ok=haris_socket_send_all(w->s,req,strlen(req),w->ssl);xfree(req);if(!ok){xfree(key);xfree(expect);return 0;}char hdr[HARIS_HTTP_HANDSHAKE_MAX];if(!ws_read_http(w->s,w->ssl,hdr,sizeof hdr)){xfree(key);xfree(expect);return 0;}char status[64]={0},acc[128]={0};int code=0;sscanf(hdr,"HTTP/%*s %d %63[^\r\n]",&code,status);char up[128]={0},con[128]={0};http_header_value(hdr,"Upgrade",up,sizeof up);http_header_value(hdr,"Connection",con,sizeof con);http_header_value(hdr,"Sec-WebSocket-Accept",acc,sizeof acc);ok=(code==101&&!strcasecmp(up,"websocket")&&ws_has_token_ci(con,"Upgrade")&&!strcasecmp(acc,expect));xfree(key);xfree(expect);return ok;}
static int ws_server_handshake(HWS*w){char hdr[HARIS_HTTP_HANDSHAKE_MAX];if(!ws_read_http(w->s,w->ssl,hdr,sizeof hdr))return 0;if(strncmp(hdr,"GET ",4)!=0)return 0;char key[256]={0},up[128]={0},con[128]={0},ver[64]={0};if(!http_header_value(hdr,"Sec-WebSocket-Key",key,sizeof key))return 0;if(!http_header_value(hdr,"Upgrade",up,sizeof up)||strcasecmp(up,"websocket"))return 0;if(!http_header_value(hdr,"Connection",con,sizeof con)||!ws_has_token_ci(con,"Upgrade"))return 0;if(!http_header_value(hdr,"Sec-WebSocket-Version",ver,sizeof ver)||strcmp(ver,"13"))return 0;char*acc=ws_accept_key(key);if(!acc)return 0;char resp[512];snprintf(resp,sizeof resp,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",acc);xfree(acc);return haris_socket_send_all(w->s,resp,strlen(resp),w->ssl);}
static int ws_send_frame(HWS*w,int opcode,const unsigned char*data,size_t n,int control){
    if(!w||w->closed||(!data&&n)||n>HARIS_NET_MAX_FRAME||(control&&n>125))return 0; unsigned char head[14];size_t hn=2;head[0]=0x80|(unsigned char)opcode;int mask=!w->is_server;
    if(n<126)head[1]=(unsigned char)(mask?0x80|n:n);
    else if(n<=65535){head[1]=(unsigned char)(mask?0x80|126:126);head[2]=(unsigned char)(n>>8);head[3]=(unsigned char)n;hn=4;}
    else {head[1]=(unsigned char)(mask?0x80|127:127);uint64_t z=(uint64_t)n;for(int i=0;i<8;i++)head[2+i]=(unsigned char)(z>>(56-8*i));hn=10;}
    unsigned char key[4];if(mask){if(!random_bytes(key,4))return 0;memcpy(head+hn,key,4);hn+=4;}if(n>SIZE_MAX-hn)return 0;size_t total=hn+n;unsigned char*buf=(unsigned char*)xmalloc(total?total:1);memcpy(buf,head,hn);
    if(mask){for(size_t i=0;i<n;i++)buf[hn+i]=data[i]^key[i&3];}else if(n)memcpy(buf+hn,data,n); int ok=haris_socket_send_all(w->s,buf,total,w->ssl);xfree(buf);return ok;
}
static int ws_read_frame(HWS*w,int*fin,int*opcode,unsigned char**payload,size_t*plen){unsigned char h[2];if(!haris_socket_recv_exact(w->s,h,2,w->ssl,-1))return 0;*fin=(h[0]&0x80)!=0;int rsv=h[0]&0x70;*opcode=h[0]&0x0f;int masked=(h[1]&0x80)!=0;uint64_t n=h[1]&0x7f;if(rsv||*opcode>0xA)return 0;if(n==126){unsigned char b[2];if(!haris_socket_recv_exact(w->s,b,2,w->ssl,-1))return 0;n=((uint64_t)b[0]<<8)|b[1];if(n<126)return 0;}else if(n==127){unsigned char b[8];if(!haris_socket_recv_exact(w->s,b,8,w->ssl,-1))return 0;if(b[0]&0x80)return 0;n=0;for(int i=0;i<8;i++)n=(n<<8)|b[i];if(n<65536)return 0;}if(n>HARIS_NET_MAX_FRAME)return 0;if((*opcode&8)&&(!*fin||n>125))return 0;if((w->is_server&&!masked)||(!w->is_server&&masked))return 0;unsigned char key[4];if(masked&&!haris_socket_recv_exact(w->s,key,4,w->ssl,-1))return 0;unsigned char*p=(unsigned char*)xmalloc((size_t)n?n:1);if(n&&!haris_socket_recv_exact(w->s,p,(size_t)n,w->ssl,-1)){xfree(p);return 0;}if(masked)for(size_t i=0;i<(size_t)n;i++)p[i]^=key[i&3];*payload=p;*plen=(size_t)n;return 1;}
static int utf8_valid(const unsigned char*s,size_t n){size_t i=0;while(i<n){unsigned c=s[i++];if(c<0x80)continue;if(c<0xC2)return 0;if(c<0xE0){if(i>=n||(s[i]&0xC0)!=0x80)return 0;i++;}else if(c<0xF0){if(i+1>=n||(s[i]&0xC0)!=0x80||(s[i+1]&0xC0)!=0x80)return 0;if(c==0xE0&&(s[i]<0xA0))return 0;if(c==0xED&&(s[i]>=0xA0))return 0;i+=2;}else if(c<=0xF4){if(i+2>=n||(s[i]&0xC0)!=0x80||(s[i+1]&0xC0)!=0x80||(s[i+2]&0xC0)!=0x80)return 0;if(c==0xF0&&s[i]<0x90)return 0;if(c==0xF4&&s[i]>=0x90)return 0;i+=3;}else return 0;}return 1;}

static Value udp_new_common(VM*vm,int n,Value*a,int force_bind){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,force_bind?"udp.bind":"udp.open");if(n<1||n>3)return vn();
    const char*host="0.0.0.0";int port=0;if(n>=1&&a[0].t==VINT)port=(int)a[0].u.i;else if(a[0].t==VSTR){host=a[0].u.s;if(n<2||a[1].t!=VINT)return vn();port=(int)a[1].u.i;}else return vn();if(port<0||port>65535)return vn();if(n==3&&a[2].t!=VBOOL)return vn();int connected=(n==3&&a[2].u.b);if(!haris_socket_init())return vn();HUDP*u=(HUDP*)xmalloc_dtor(sizeof(*u),hudp_dtor);memset(u,0,sizeof *u);u->kind=HK_UDP;u->s=HARIS_INVALID_SOCKET;u->port=port;u->host=xdup(host);
    if(connected){if(!resolve_and_socket(host,port,SOCK_DGRAM,&u->s)){xfree(u->host);u->host=NULL;u->kind=HK_CLOSED;return vn();}u->connected=1;}
    else {struct addrinfo h={0},*r=NULL;char ps[16];snprintf(ps,sizeof ps,"%d",port);h.ai_family=AF_UNSPEC;h.ai_socktype=SOCK_DGRAM;h.ai_flags=AI_PASSIVE;if(getaddrinfo((!strcmp(host,"*")?NULL:host),ps,&h,&r)!=0){u->kind=HK_CLOSED;return vn();}int one=1;for(struct addrinfo*p=r;p;p=p->ai_next){u->s=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(u->s==HARIS_INVALID_SOCKET)continue;setsockopt(u->s,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof one);if(bind(u->s,p->ai_addr,(socklen_t)p->ai_addrlen)==0)break;haris_socket_close(u->s);u->s=HARIS_INVALID_SOCKET;}freeaddrinfo(r);if(u->s==HARIS_INVALID_SOCKET){u->kind=HK_CLOSED;return vn();}
        if(u->port==0){struct sockaddr_storage actual;socklen_t alen=sizeof actual;if(getsockname(u->s,(struct sockaddr*)&actual,&alen)==0){if(actual.ss_family==AF_INET)u->port=(int)ntohs(((struct sockaddr_in*)&actual)->sin_port);else if(actual.ss_family==AF_INET6)u->port=(int)ntohs(((struct sockaddr_in6*)&actual)->sin6_port);}}
    }
    return (Value){.t=VHANDLE,.u.handle=u};
}
static Value udp_open(VM*vm,int n,Value*a){return udp_new_common(vm,n,a,0);} 
static Value udp_bind(VM*vm,int n,Value*a){return udp_new_common(vm,n,a,1);} 
static HUDP*udp_handle(Value v){if(v.t!=VHANDLE||!v.u.handle||!heap_is_ptr(v.u.handle)||*((int*)v.u.handle)!=HK_UDP)return NULL;return (HUDP*)v.u.handle;}
static Value udp_connect(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"udp.connect");if(n!=3||!udp_handle(a[0])||a[1].t!=VSTR||a[2].t!=VINT)return vb(0);HUDP*u=udp_handle(a[0]);if(a[2].u.i<1||a[2].u.i>65535)return vb(0);if(u->s!=HARIS_INVALID_SOCKET){haris_socket_close(u->s);u->s=HARIS_INVALID_SOCKET;}if(!resolve_and_socket(a[1].u.s,(int)a[2].u.i,SOCK_DGRAM,&u->s))return vb(0);u->connected=1;if(u->host)xfree(u->host);u->host=xdup(a[1].u.s);u->port=(int)a[2].u.i;return vb(1);}
static Value udp_send(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"udp.send");
    if(n<2||!udp_handle(a[0]))return vb(0); HUDP*u=udp_handle(a[0]); Value data; const char*host=NULL; int port=0;
    if(u->connected){if(n!=2)return vb(0);data=a[1];}
    else {if(n!=4||a[1].t!=VSTR||a[2].t!=VINT)return vb(0);host=a[1].u.s;port=(int)a[2].u.i;data=a[3];if(port<1||port>65535)return vb(0);}
    if(u->s==HARIS_INVALID_SOCKET)return vb(0); const unsigned char*p=NULL;size_t z=0;unsigned char*cp=NULL;
    if(!net_value_bytes(data,&p,&z)){if(!net_value_bytes_copy(data,&cp,&z))return vb(0);p=cp;}
    if(z>HARIS_NET_MAX_FRAME||z>INT_MAX){if(cp)xfree(cp);return vb(0);} int rc=-1;
    if(u->connected)rc=(int)send(u->s,(const char*)p,(int)z,0);
    else {char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo h={0},*r=NULL;h.ai_socktype=SOCK_DGRAM;h.ai_family=AF_UNSPEC;if(getaddrinfo(host,ps,&h,&r)==0){for(struct addrinfo*q=r;q;q=q->ai_next){rc=(int)sendto(u->s,(const char*)p,(int)z,0,q->ai_addr,(socklen_t)q->ai_addrlen);if(rc==(int)z)break;}freeaddrinfo(r);}}
    if(cp)xfree(cp);return vb(rc==(int)z);
}
static Value udp_recv(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"udp.recv");if(n<1||n>3||!udp_handle(a[0]))return vn();HUDP*u=udp_handle(a[0]);size_t maxn=(n>=2&&a[1].t==VINT&&a[1].u.i>0)?(size_t)a[1].u.i:1400;if(maxn>HARIS_NET_MAX_FRAME)return vn();int timeout=(n==3&&a[2].t==VINT)?(int)a[2].u.i:100;if(timeout<0)timeout=0;unsigned char*b=(unsigned char*)xmalloc(maxn?maxn:1);if(timeout&&!haris_socket_wait(u->s,1,0,timeout)){xfree(b);return vn();}struct sockaddr_storage ss;socklen_t sl=sizeof ss;int rc=(int)recvfrom(u->s,(char*)b,(int)maxn,0,(struct sockaddr*)&ss,&sl);if(rc<=0){xfree(b);return vn();}char host[INET6_ADDRSTRLEN]={0};int port=0;if(ss.ss_family==AF_INET){struct sockaddr_in*x=(struct sockaddr_in*)&ss;inet_ntop(AF_INET,&x->sin_addr,host,sizeof host);port=ntohs(x->sin_port);}else if(ss.ss_family==AF_INET6){struct sockaddr_in6*x=(struct sockaddr_in6*)&ss;inet_ntop(AF_INET6,&x->sin6_addr,host,sizeof host);port=ntohs(x->sin6_port);}Value out=vsobj();stput(out.u.st,"bytes",net_bytes_array(b,(size_t)rc));stput(out.u.st,"size",vi(rc));stput(out.u.st,"host",vs(host));stput(out.u.st,"port",vi(port));xfree(b);return out;}
static Value udp_send_batch(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"udp.send_batch");
    if(n!=2||!udp_handle(a[0])||a[1].t!=VARR||a[1].u.a->n>4096)return vb(0); HUDP*u=udp_handle(a[0]);long long ok=0;
    for(size_t i=0;i<a[1].u.a->n;i++){Value q=a[1].u.a->v[i];
        if(u->connected){if(q.t!=VARR||q.u.a->n!=1)continue;Value tmp[2]={a[0],q.u.a->v[0]};if(udp_send(vm,2,tmp).u.b)ok++;}
        else {if(q.t!=VARR||q.u.a->n!=3)continue;Value tmp[4]={a[0],q.u.a->v[0],q.u.a->v[1],q.u.a->v[2]};if(udp_send(vm,4,tmp).u.b)ok++;}
    } return vi(ok);
}
static Value udp_set_nonblock(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"udp.set_nonblock");if(n!=2||!udp_handle(a[0])||a[1].t!=VBOOL)return vb(0);HUDP*u=udp_handle(a[0]);if(!haris_socket_set_nonblock(u->s,a[1].u.b))return vb(0);u->nonblock=a[1].u.b;return vb(1);}
static Value udp_set_buffer(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"udp.set_buffer");if(n!=3||!udp_handle(a[0])||a[1].t!=VSTR||a[2].t!=VINT)return vb(0);int bytes=(int)a[2].u.i;if(bytes<4096||bytes>16*1024*1024)return vb(0);int opt=!strcasecmp(a[1].u.s,"recv")?SO_RCVBUF:(!strcasecmp(a[1].u.s,"send")?SO_SNDBUF:0);if(!opt)return vb(0);return vb(setsockopt(udp_handle(a[0])->s,SOL_SOCKET,opt,(const char*)&bytes,sizeof bytes)==0);}
static Value udp_close(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HUDP*u=udp_handle(a[0]);if(!u)return vb(0);if(u->s!=HARIS_INVALID_SOCKET)haris_socket_close(u->s);u->s=HARIS_INVALID_SOCKET;u->kind=HK_CLOSED;return vb(1);}
static Value udp_info(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();HUDP*u=udp_handle(a[0]);if(!u)return vn();Value o=vsobj();stput(o.u.st,"connected",vb(u->connected));stput(o.u.st,"nonblock",vb(u->nonblock));stput(o.u.st,"host",vs(u->host?u->host:""));stput(o.u.st,"port",vi(u->port));return o;}

static int ws_make_client_tls(HWS*w,const char*host){w->ctx=SSL_CTX_new(TLS_client_method());if(!w->ctx)return 0;SSL_CTX_set_min_proto_version(w->ctx,TLS1_2_VERSION);SSL_CTX_set_verify(w->ctx,SSL_VERIFY_PEER,NULL);if(SSL_CTX_set_default_verify_paths(w->ctx)!=1)return 0;w->ssl=SSL_new(w->ctx);if(!w->ssl)return 0;SSL_set_tlsext_host_name(w->ssl,host);X509_VERIFY_PARAM*vp=SSL_get0_param(w->ssl);if(X509_VERIFY_PARAM_set1_host(vp,host,0)!=1)return 0;if(SSL_set_fd(w->ssl,(int)w->s)!=1)return 0;return SSL_connect(w->ssl)==1;}
static int ws_make_server_tls(HWS* w,SSL_CTX*ctx){w->ctx=ctx;w->ssl=SSL_new(ctx);if(!w->ssl)return 0;if(SSL_set_fd(w->ssl,(int)w->s)!=1)return 0;return SSL_accept(w->ssl)==1;}
static HWS*ws_handle(Value v){if(v.t!=VHANDLE||!v.u.handle||!heap_is_ptr(v.u.handle)||*((int*)v.u.handle)!=HK_WS)return NULL;return (HWS*)v.u.handle;}
static HWSListener*ws_listener_handle(Value v){if(v.t!=VHANDLE||!v.u.handle||!heap_is_ptr(v.u.handle)||*((int*)v.u.handle)!=HK_WS_LISTENER)return NULL;return (HWSListener*)v.u.handle;}
static Value ws_connect(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"ws.connect");if(n!=1||a[0].t!=VSTR)return vn();char host[256],path[2048];int secure,port;if(!parse_ws_url(a[0].u.s,&secure,host,sizeof host,&port,path,sizeof path))return vn();if(!haris_socket_init())return vn();HWS*w=(HWS*)xmalloc_dtor(sizeof(*w),hws_dtor);memset(w,0,sizeof *w);w->kind=HK_WS;w->s=HARIS_INVALID_SOCKET;w->url=xdup(a[0].u.s);if(!resolve_and_socket(host,port,SOCK_STREAM,&w->s)){w->kind=HK_CLOSED;return vn();}w->tls=secure;if(secure&&!ws_make_client_tls(w,host)){w->kind=HK_CLOSED;return vn();}if(!ws_client_handshake(w,host,path)){w->kind=HK_CLOSED;return vn();}return (Value){.t=VHANDLE,.u.handle=w};}
static Value ws_listen(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"ws.listen");if(n<1||n>3||a[0].t!=VINT)return vn();int port=(int)a[0].u.i;if(port<1||port>65535)return vn();const char*cert=n>=2&&a[1].t==VSTR?a[1].u.s:NULL;const char*key=n>=3&&a[2].t==VSTR?a[2].u.s:NULL;if((cert&&!key)||(!cert&&key))return vn();if(cert&&!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"ws.listen TLS certificate");if(!haris_socket_init())return vn();HWSListener*l=(HWSListener*)xmalloc_dtor(sizeof(*l),hwslistener_dtor);memset(l,0,sizeof*l);l->kind=HK_WS_LISTENER;l->s=HARIS_INVALID_SOCKET;l->port=port;l->host=xdup("0.0.0.0");l->tls=cert!=NULL;if(l->tls){l->ctx=SSL_CTX_new(TLS_server_method());if(!l->ctx||SSL_CTX_set_min_proto_version(l->ctx,TLS1_2_VERSION)!=1||SSL_CTX_use_certificate_file(l->ctx,cert,SSL_FILETYPE_PEM)!=1||SSL_CTX_use_PrivateKey_file(l->ctx,key,SSL_FILETYPE_PEM)!=1||SSL_CTX_check_private_key(l->ctx)!=1){l->kind=HK_CLOSED;return vn();}}
    struct addrinfo h={0},*r=NULL;char ps[16];snprintf(ps,sizeof ps,"%d",port);h.ai_family=AF_UNSPEC;h.ai_socktype=SOCK_STREAM;h.ai_flags=AI_PASSIVE;if(getaddrinfo(NULL,ps,&h,&r)!=0){l->kind=HK_CLOSED;return vn();}int one=1;for(struct addrinfo*p=r;p;p=p->ai_next){l->s=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(l->s==HARIS_INVALID_SOCKET)continue;setsockopt(l->s,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof one);if(bind(l->s,p->ai_addr,(socklen_t)p->ai_addrlen)==0&&listen(l->s,128)==0)break;haris_socket_close(l->s);l->s=HARIS_INVALID_SOCKET;}freeaddrinfo(r);if(l->s==HARIS_INVALID_SOCKET){l->kind=HK_CLOSED;return vn();}return (Value){.t=VHANDLE,.u.handle=l};}
static Value ws_accept(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"ws.accept");if(n<1||n>2||!ws_listener_handle(a[0]))return vn();HWSListener*l=ws_listener_handle(a[0]);int timeout=n==2&&a[1].t==VINT?(int)a[1].u.i:0;if(timeout>0&&!haris_socket_wait(l->s,1,0,timeout))return vn();haris_socket_t c=accept(l->s,NULL,NULL);if(c==HARIS_INVALID_SOCKET)return vn();HWS*w=(HWS*)xmalloc_dtor(sizeof(*w),hws_dtor);memset(w,0,sizeof*w);w->kind=HK_WS;w->s=c;w->is_server=1;w->tls=l->tls;if(l->tls&&!ws_make_server_tls(w,l->ctx)){w->kind=HK_CLOSED;return vn();}if(!ws_server_handshake(w)){w->kind=HK_CLOSED;return vn();}return (Value){.t=VHANDLE,.u.handle=w};}
static Value ws_send_common(VM*vm,int n,Value*a,int opcode){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,opcode==1?"ws.send_text":"ws.send_binary");if(n!=2||!ws_handle(a[0]))return vb(0);const unsigned char*p=NULL;size_t z=0;unsigned char*cp=NULL;if(opcode==1){if(a[1].t!=VSTR)return vb(0);p=(const unsigned char*)a[1].u.s;z=strlen(a[1].u.s);if(!utf8_valid(p,z))return vb(0);}else{if(!net_value_bytes_copy(a[1],&cp,&z))return vb(0);p=cp;}int ok=ws_send_frame(ws_handle(a[0]),opcode,p,z,0);if(cp)xfree(cp);return vb(ok);} 
static Value ws_send_text(VM*vm,int n,Value*a){return ws_send_common(vm,n,a,1);} 
static Value ws_send_binary(VM*vm,int n,Value*a){return ws_send_common(vm,n,a,2);} 
static Value ws_recv(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"ws.recv");if(n<1||n>2||!ws_handle(a[0]))return vn();HWS*w=ws_handle(a[0]);int timeout=n==2&&a[1].t==VINT?(int)a[1].u.i:-1;if(timeout>=0&&timeout<1)timeout=1;if(timeout>=0&&!haris_socket_wait(w->s,1,0,timeout))return vn();Value acc=vsobj();unsigned char*msg=NULL;size_t mn=0;int msgop=0;int started=0;for(;;){int fin,op;unsigned char*p=NULL;size_t z=0;if(!ws_read_frame(w,&fin,&op,&p,&z))return vn();if(op==8){ws_send_frame(w,8,p,z,1);xfree(p);w->closed=1;stput(acc.u.st,"type",vs("close"));return acc;}if(op==9){ws_send_frame(w,10,p,z,1);xfree(p);continue;}if(op==10){xfree(p);continue;}if(op==0){if(!started){xfree(p);return vn();}if(!mn&&z){msg=(unsigned char*)xmalloc(z);memcpy(msg,p,z);mn=z;}else if(z){if(mn>HARIS_NET_MAX_FRAME-z){xfree(p);if(msg)xfree(msg);return vn();}msg=(unsigned char*)xrealloc(msg,mn+z);memcpy(msg+mn,p,z);mn+=z;}xfree(p);if(!fin)continue;}else if(op==1||op==2){if(started){xfree(p);return vn();}started=!fin;msgop=op;if(z){msg=(unsigned char*)xmalloc(z);memcpy(msg,p,z);mn=z;}xfree(p);if(!fin)continue;}else return vn();if(fin)break;}if(msgop==1&&!utf8_valid(msg,mn)){if(msg)xfree(msg);return vn();}stput(acc.u.st,"type",vs(msgop==1?"text":"binary"));stput(acc.u.st,"data",msgop==1?vs((const char*)msg):net_bytes_array(msg,mn));stput(acc.u.st,"size",vi((long long)mn));if(msg)xfree(msg);return acc;}
static Value ws_ping(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"ws.ping");if(n!=2||!ws_handle(a[0])||a[1].t!=VSTR||strlen(a[1].u.s)>125)return vb(0);return vb(ws_send_frame(ws_handle(a[0]),9,(unsigned char*)a[1].u.s,strlen(a[1].u.s),1));}
static Value ws_close(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"ws.close");if(n!=1&&! (n==2&&a[1].t==VSTR))return vb(0);if(!ws_handle(a[0]))return vb(0);HWS*w=ws_handle(a[0]);if(w->closed)return vb(1);unsigned char c[2]={0x03,0xE8};if(n==2&&a[1].t==VSTR&&strlen(a[1].u.s)>0){size_t z=strlen(a[1].u.s);if(z>123)z=123;unsigned char*buf=(unsigned char*)xmalloc(z+2);buf[0]=3;buf[1]=232;memcpy(buf+2,a[1].u.s,z);int ok=ws_send_frame(w,8,buf,z+2,1);xfree(buf);w->closed=1;return vb(ok);}int ok=ws_send_frame(w,8,c,2,1);w->closed=1;return vb(ok);}
static Value ws_info(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();if(ws_handle(a[0])){HWS*w=ws_handle(a[0]);Value o=vsobj();stput(o.u.st,"secure",vb(w->tls));stput(o.u.st,"server",vb(w->is_server));stput(o.u.st,"closed",vb(w->closed));stput(o.u.st,"url",vs(w->url?w->url:""));return o;}if(ws_listener_handle(a[0])){HWSListener*l=ws_listener_handle(a[0]);Value o=vsobj();stput(o.u.st,"secure",vb(l->tls));stput(o.u.st,"port",vi(l->port));return o;}return vn();}

static void bootstrap_stdlib(VM *vm);
static Value map_new_sized(size_t nbuckets);
static Value nmap_set(VM*vm,int n,Value*a);
static const char* find_ci(const char*hay,const char*needle){
    size_t nl=strlen(needle); if(!nl)return hay;
    for(const char*p=hay; *p; p++){ size_t i=0; while(i<nl && p[i] && tolower((unsigned char)p[i])==tolower((unsigned char)needle[i])) i++; if(i==nl) return p; }
    return NULL;
}
static int route_match(const char*pattern,const char*path,Value paramsMap){
    char pbuf[512],hbuf[4096]; snprintf(pbuf,sizeof pbuf,"%s",pattern); snprintf(hbuf,sizeof hbuf,"%s",path);
    char*psave=0,*hsave=0;
    char*pt=strtok_r(pbuf,"/",&psave); char*ht=strtok_r(hbuf,"/",&hsave);
    for(;;){
        if(!pt && !ht) return 1;
        if(!pt || !ht) return 0;
        if(pt[0]==':'){ Value args[3]={paramsMap,vs(pt+1),vs(ht)}; nmap_set(NULL,3,args); }
        else if(strcmp(pt,ht)!=0) return 0;
        pt=strtok_r(NULL,"/",&psave); ht=strtok_r(NULL,"/",&hsave);
    }
}
static Value web_response(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.response");
    if(n<2||n>3||a[0].t!=VINT||a[1].t!=VSTR)return vn();
    Value r=vsobj(); stput(r.u.st,"__type",vs("http_response")); stput(r.u.st,"status",a[0]); stput(r.u.st,"body",a[1]);
    stput(r.u.st,"headers",(n==3&&a[2].t==VSTRUCT)?a[2]:vn());
    return r;
}
static Value app_new(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.app");
    (void)a; if(n)return vn();
    Value app=vsobj(); stput(app.u.st,"__type",vs("app")); stput(app.u.st,"__routes",va());
    return app;
}
static Value app_add_route(VM*vm,int n,Value*a,const char*method){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.app route");
    if(n!=3||a[0].t!=VSTRUCT||a[1].t!=VSTR||(a[2].t!=VFN&&a[2].t!=VNATIVE))return vb(0);
    Value routes=stget(a[0].u.st,"__routes"); if(routes.t!=VARR)return vb(0);
    Value entry=va(); ap(entry.u.a,vs(method)); ap(entry.u.a,a[1]); ap(entry.u.a,a[2]); ap(routes.u.a,entry);
    return vb(1);
}
static Value app_get_route(VM*vm,int n,Value*a){return app_add_route(vm,n,a,"GET");}
static Value app_post_route(VM*vm,int n,Value*a){return app_add_route(vm,n,a,"POST");}
static Value app_put_route(VM*vm,int n,Value*a){return app_add_route(vm,n,a,"PUT");}
static Value app_delete_route(VM*vm,int n,Value*a){return app_add_route(vm,n,a,"DELETE");}
static Value app_listen(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.app listen");
    if(n<2||n>3||a[0].t!=VSTRUCT||a[1].t!=VINT)return vb(0);
    int port=(int)a[1].u.i; const char*host=(n==3&&a[2].t==VSTR)?a[2].u.s:"127.0.0.1";
    if(port<1||port>65535)return vb(0);
    Value routes=stget(a[0].u.st,"__routes"); if(routes.t!=VARR)return vb(0);
    if(!haris_socket_init())return vb(0);
    char ps[16];snprintf(ps,sizeof ps,"%d",port);
    struct addrinfo h={0},*res=0;h.ai_family=AF_UNSPEC;h.ai_socktype=SOCK_STREAM;h.ai_flags=AI_PASSIVE;
    if(getaddrinfo(host,ps,&h,&res)){
#ifdef _WIN32
        WSACleanup();
#endif
        return vb(0);
    }
    haris_socket_t srv=HARIS_INVALID_SOCKET;int one=1;
    for(struct addrinfo*p=res;p;p=p->ai_next){
        srv=socket(p->ai_family,p->ai_socktype,p->ai_protocol); if(srv==HARIS_INVALID_SOCKET)continue;
#ifdef _WIN32
        setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof one);
#else
        setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
#endif
        if(bind(srv,p->ai_addr,(int)p->ai_addrlen)==0 && listen(srv,32)==0)break;
        haris_socket_close(srv); srv=HARIS_INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if(srv==HARIS_INVALID_SOCKET){
#ifdef _WIN32
        WSACleanup();
#endif
        return vb(0);
    }
    for(;;){
        haris_socket_t c=accept(srv,NULL,NULL); if(c==HARIS_INVALID_SOCKET)break; haris_socket_tune_server(c);
        char *buf=(char*)malloc(65536); size_t got=0; long header_end=-1;
        for(;;){
#ifdef _WIN32
            int r=recv(c,buf+got,(int)(65535-got),0);
#else
            ssize_t r=recv(c,buf+got,65535-got,0);
#endif
            if(r<=0)break; got+=(size_t)r; buf[got]=0;
            const char*he=strstr(buf,"\r\n\r\n"); if(he){header_end=(long)(he-buf)+4;break;}
            if(got>=65535)break;
        }
        if(header_end<0){free(buf);haris_socket_close(c);continue;}
        char method[8]={0},target[4096]={0};
        if(sscanf(buf,"%7s %4095s",method,target)!=2){free(buf);haris_socket_close(c);continue;}
        char*q=strchr(target,'?'); Value querymap=map_new_sized(8);
        if(q){ *q=0; char*qs=xdup(q+1); char*qsave=0; for(char*pair=strtok_r(qs,"&",&qsave);pair;pair=strtok_r(NULL,"&",&qsave)){ char*eq=strchr(pair,'='); if(eq){*eq=0; char*kk=web_url_decode(pair); char*vv=web_url_decode(eq+1); Value args3[3]={querymap,vs(kk),vs(vv)}; nmap_set(NULL,3,args3); xfree(kk); xfree(vv);} else { char*kk=web_url_decode(pair); Value args3[3]={querymap,vs(kk),vs("")}; nmap_set(NULL,3,args3); xfree(kk);} } xfree(qs); }
        char*path=web_url_decode(target); if(!path){free(buf);haris_socket_close(c);continue;}
        long long content_length=0; const char*clh=find_ci(buf,"\r\nContent-Length:");
        if(clh && clh<buf+header_end){ clh+=strlen("\r\nContent-Length:"); while(*clh==' '||*clh=='\t')clh++; char*ce=NULL; errno=0; content_length=strtoll(clh,&ce,10); if(errno||ce==clh||content_length<0||content_length>1024*1024){free(buf);haris_socket_close(c);continue;} while(*ce==' '||*ce=='\t')ce++; if(*ce!='\r'&&*ce!='\n'&&*ce){free(buf);haris_socket_close(c);continue;} }
        if(find_ci(buf,"Transfer-Encoding:")){free(buf);haris_socket_close(c);continue;}
        size_t body_have=got-(size_t)header_end;
        while((long)body_have<content_length && got<65535){
#ifdef _WIN32
            int r=recv(c,buf+got,(int)(65535-got),0);
#else
            ssize_t r=recv(c,buf+got,65535-got,0);
#endif
            if(r<=0)break; got+=(size_t)r; body_have=got-(size_t)header_end;
        }
        if((long long)body_have<content_length){free(buf);haris_socket_close(c);continue;} char*body=(char*)xmalloc((size_t)content_length+1);if(content_length)memcpy(body,buf+header_end,(size_t)content_length);body[content_length]=0;
        Value headersmap=map_new_sized(8);
        {   char*hcopy=(char*)malloc((size_t)header_end+1); memcpy(hcopy,buf,(size_t)header_end); hcopy[header_end]=0;
            char*line_save=0; char*ln=strtok_r(hcopy,"\r\n",&line_save); int first=1;
            for(; ln; ln=strtok_r(NULL,"\r\n",&line_save)){
                if(first){first=0;continue;}
                char*colon=strchr(ln,':'); if(!colon)continue; *colon=0; char*val=colon+1; while(*val==' ')val++;
                for(char*pc=ln;*pc;pc++)*pc=(char)tolower((unsigned char)*pc);
                Value args3[3]={headersmap,vs(ln),vs(val)}; nmap_set(NULL,3,args3);
            }
            free(hcopy);
        }
        Value handler=vn(); int found=0; Value paramsmap=map_new_sized(8);
        for(size_t i=0;i<routes.u.a->n;i++){
            Value entry=routes.u.a->v[i]; if(entry.t!=VARR||entry.u.a->n<3)continue;
            Value me=entry.u.a->v[0],pe=entry.u.a->v[1];
            if(me.t==VSTR&&pe.t==VSTR&&!strcmp(me.u.s,method)){
                Value tryparams=map_new_sized(8);
                if(route_match(pe.u.s,path,tryparams)){ handler=entry.u.a->v[2]; found=1; paramsmap=tryparams; break; }
            }
        }
        if(found && handler.t==VFN){
            Value req=vsobj(); stput(req.u.st,"method",vs(method)); stput(req.u.st,"path",vs(path)); stput(req.u.st,"body",vs(body));
            stput(req.u.st,"query",querymap); stput(req.u.st,"headers",headersmap); stput(req.u.st,"params",paramsmap);
            VM child; init(&child); bootstrap_stdlib(&child);
            for(int gi=0; gi<vm->g.n; gi++) en(&child.g, vm->g.v[gi].k, vm->g.v[gi].v);
            Value result=run(&child, handler.u.fn, 1, &req);
            int status=200; const char*rb=""; Value custom_headers=vn();
            if(result.t==VSTRUCT){ Value ty=stget(result.u.st,"__type");
                if(ty.t==VSTR && !strcmp(ty.u.s,"http_response")){
                    Value sv=stget(result.u.st,"status"); if(sv.t==VINT)status=(int)sv.u.i;
                    Value bv=stget(result.u.st,"body"); if(bv.t==VSTR)rb=bv.u.s;
                    custom_headers=stget(result.u.st,"headers");
                }
            } else if(result.t==VSTR) rb=result.u.s;
            const char*ctype="text/plain; charset=utf-8";
            if(rb[0]=='<') ctype="text/html; charset=utf-8"; else if(rb[0]=='{'||rb[0]=='[') ctype="application/json";
            if(!http_status_valid(status))status=500; const char*statustext=http_reason(status);
            DBuf hdrb; dbuf_init(&hdrb); char first_line[128]; snprintf(first_line,sizeof first_line,"HTTP/1.1 %d %s\r\n",status,statustext); dbuf_puts(&hdrb,first_line);
            char ctline[256]; snprintf(ctline,sizeof ctline,"Content-Type: %s\r\n",ctype); dbuf_puts(&hdrb,ctline);
            char clline[64]; snprintf(clline,sizeof clline,"Content-Length: %zu\r\n",strlen(rb)); dbuf_puts(&hdrb,clline);
            if(custom_headers.t==VSTRUCT){ Value hbv=stget(custom_headers.u.st,"__buckets"); if(hbv.t==VARR){ for(size_t bi=0;bi<hbv.u.a->n;bi++){ Value bucket=hbv.u.a->v[bi]; if(bucket.t!=VARR)continue; for(size_t bj=0;bj<bucket.u.a->n;bj++){ Value pair=bucket.u.a->v[bj]; if(pair.t==VARR&&pair.u.a->n>=2&&pair.u.a->v[0].t==VSTR&&pair.u.a->v[1].t==VSTR&&http_header_value_safe(pair.u.a->v[0].u.s)&&http_header_value_safe(pair.u.a->v[1].u.s)){ char hl[512]; snprintf(hl,sizeof hl,"%s: %s\r\n",pair.u.a->v[0].u.s,pair.u.a->v[1].u.s); dbuf_puts(&hdrb,hl); } } } } }
            dbuf_puts(&hdrb,"Connection: close\r\n\r\n");
            send(c,hdrb.d,(int)hdrb.n,0); send(c,rb,(int)strlen(rb),0); free(hdrb.d);
        } else {
            const char*b404="Not Found"; char hdr[256]; int hn=snprintf(hdr,sizeof hdr,"HTTP/1.1 404 Not Found\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",strlen(b404));
            send(c,hdr,hn,0); send(c,b404,(int)strlen(b404),0);
        }
        xfree(path); xfree(body); free(buf); haris_socket_close(c);
    }
    haris_socket_close(srv);
#ifdef _WIN32
    WSACleanup();
#endif
    return vb(1);
} 
static Value app_https_listen(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB)||!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_WEB|CAP_FS,"web.app.https_listen");
    if(n<4||n>6||a[0].t!=VSTRUCT||a[1].t!=VSTR||a[2].t!=VSTR||a[3].t!=VINT)return vb(0);
    const char*cert=a[1].u.s,*key=a[2].u.s,*host=(n>=5&&a[4].t==VSTR)?a[4].u.s:"127.0.0.1";int port=(int)a[3].u.i,maxreq=(n>=6&&a[5].t==VINT)?(int)a[5].u.i:0;
    if(port<1||port>65535||maxreq<0||!fs_path_allowed(vm,cert)||!fs_path_allowed(vm,key))return vb(0);
    Value routes=stget(a[0].u.st,"__routes");if(routes.t!=VARR)return vb(0);
    if(!haris_socket_init())return vb(0);
    SSL_CTX*ctx=SSL_CTX_new(TLS_server_method());if(!ctx)return vb(0);int maxv=TLS1_2_VERSION;
#ifdef TLS1_3_VERSION
    maxv=TLS1_3_VERSION;
#endif
    if(!tls_configure_server(ctx,cert,key,TLS1_2_VERSION,maxv,tls_default12(),tls_default13())){SSL_CTX_free(ctx);return vb(0);}
    char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo h={0},*res=0;h.ai_family=AF_UNSPEC;h.ai_socktype=SOCK_STREAM;h.ai_flags=AI_PASSIVE;
    if(getaddrinfo(host,ps,&h,&res)){SSL_CTX_free(ctx);return vb(0);}haris_socket_t srv=HARIS_INVALID_SOCKET;int one=1;
    for(struct addrinfo*p=res;p;p=p->ai_next){srv=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(srv==HARIS_INVALID_SOCKET)continue;setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof one);if(bind(srv,p->ai_addr,(socklen_t)p->ai_addrlen)==0&&listen(srv,64)==0)break;haris_socket_close(srv);srv=HARIS_INVALID_SOCKET;}
    freeaddrinfo(res);if(srv==HARIS_INVALID_SOCKET){SSL_CTX_free(ctx);return vb(0);}int served=0;
    while(maxreq==0||served<maxreq){haris_socket_t c=accept(srv,NULL,NULL);if(c==HARIS_INVALID_SOCKET)break;haris_socket_tune_server(c);SSL*ssl=SSL_new(ctx);if(!ssl){haris_socket_close(c);continue;}if(SSL_set_fd(ssl,(int)c)!=1||SSL_accept(ssl)!=1){SSL_free(ssl);haris_socket_close(c);continue;}served++;
        char req[65536];ssize_t got=https_recv_req(ssl,req,sizeof req,10000);if(got<=0){SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);continue;}
        char method[8]={0},target[4096]={0};if(sscanf(req,"%7s %4095s",method,target)!=2){SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);continue;}
        char*q=strchr(target,'?');Value querymap=map_new_sized(8);if(q){*q=0;char*qs=xdup(q+1);char*save=0;for(char*pair=strtok_r(qs,"&",&save);pair;pair=strtok_r(NULL,"&",&save)){char*eq=strchr(pair,'=');if(eq){*eq=0;char*kk=web_url_decode(pair);char*vv=web_url_decode(eq+1);Value aa[3]={querymap,vs(kk),vs(vv)};nmap_set(NULL,3,aa);xfree(kk);xfree(vv);}else{char*kk=web_url_decode(pair);Value aa[3]={querymap,vs(kk),vs("")};nmap_set(NULL,3,aa);xfree(kk);}}xfree(qs);}
        char*path=web_url_decode(target);if(!path){SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);continue;}
        long long content_length=0;const char*clh=find_ci(req,"\r\nContent-Length:");if(clh&&clh<req+got){clh+=strlen("\r\nContent-Length:");while(*clh==' '||*clh=='\t')clh++;char*ce=NULL;errno=0;content_length=strtoll(clh,&ce,10);if(errno||ce==clh||content_length<0||content_length>1024*1024){xfree(path);SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);continue;}while(*ce==' '||*ce=='\t')ce++;if(*ce!='\r'&&*ce!='\n'&&*ce){xfree(path);SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);continue;}}if(find_ci(req,"Transfer-Encoding:")){xfree(path);SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);continue;}
        const char*he=strstr(req,"\r\n\r\n");if(!he){xfree(path);SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);continue;}size_t header_end=(size_t)(he-req)+4,body_have=(size_t)got-header_end;while((long)body_have<content_length){size_t rn=0;int ro=SSL_read_ex(ssl,req+got,sizeof(req)-got-1,&rn);if(ro!=1){int ee=SSL_get_error(ssl,0);if(ee==SSL_ERROR_WANT_READ||ee==SSL_ERROR_WANT_WRITE){struct timespec ts={0,10000000L};nanosleep(&ts,NULL);continue;}break;}if(!rn)break;got+=rn;body_have=got-header_end;req[got]=0;if(got>=sizeof(req)-1)break;}
        if((long long)body_have<content_length){xfree(path);SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);continue;} char*body=(char*)xmalloc((size_t)content_length+1);if(content_length)memcpy(body,req+header_end,(size_t)content_length);body[content_length]=0;
        Value headersmap=map_new_sized(8);char*hcopy=(char*)xmalloc(header_end+1);memcpy(hcopy,req,header_end);hcopy[header_end]=0;char*hs=0;char*ln=strtok_r(hcopy,"\r\n",&hs);int first=1;for(;ln;ln=strtok_r(NULL,"\r\n",&hs)){if(first){first=0;continue;}char*co=strchr(ln,':');if(!co)continue;*co=0;char*val=co+1;while(*val==' ')val++;for(char*z=ln;*z;z++)*z=(char)tolower((unsigned char)*z);Value aa[3]={headersmap,vs(ln),vs(val)};nmap_set(NULL,3,aa);}xfree(hcopy);
        Value handler=vn(),paramsmap=map_new_sized(8);int found=0;for(size_t ri=0;ri<routes.u.a->n;ri++){Value e=routes.u.a->v[ri];if(e.t!=VARR||e.u.a->n<3)continue;Value me=e.u.a->v[0],pe=e.u.a->v[1];if(me.t==VSTR&&pe.t==VSTR&&!strcmp(me.u.s,method)){Value tp=map_new_sized(8);if(route_match(pe.u.s,path,tp)){handler=e.u.a->v[2];paramsmap=tp;found=1;break;}}}
        int status=404;const char*rb="Not Found";Value custom=vn();if(found&&(handler.t==VFN||handler.t==VNATIVE)){Value rv=vsobj();stput(rv.u.st,"method",vs(method));stput(rv.u.st,"path",vs(path));stput(rv.u.st,"body",vs(body));stput(rv.u.st,"query",querymap);stput(rv.u.st,"headers",headersmap);stput(rv.u.st,"params",paramsmap);VM child;init(&child);bootstrap_stdlib(&child);if(handler.t==VFN){for(int gi=0;gi<vm->g.n;gi++)en(&child.g,vm->g.v[gi].k,vm->g.v[gi].v);Value rr=run(&child,handler.u.fn,1,&rv);if(rr.t==VSTRUCT){Value ty=stget(rr.u.st,"__type"),sv=stget(rr.u.st,"status"),bv=stget(rr.u.st,"body");if(ty.t==VSTR&&!strcmp(ty.u.s,"http_response")){if(sv.t==VINT)status=(int)sv.u.i;if(bv.t==VSTR)rb=bv.u.s;custom=stget(rr.u.st,"headers");}}else if(rr.t==VSTR){status=200;rb=rr.u.s;}}else{Value aa[1]={rv};Value rr=handler.u.native(&child,1,aa);if(rr.t==VSTR){status=200;rb=rr.u.s;}}}
        if(!http_status_valid(status))status=500; const char*stxt=http_reason(status);const char*ctype=rb[0]=='<'?"text/html; charset=utf-8":(rb[0]=='{'||rb[0]=='[')?"application/json":"text/plain; charset=utf-8";DBuf hb;dbuf_init(&hb);char fl[128];snprintf(fl,sizeof fl,"HTTP/1.1 %d %s\r\n",status,stxt);dbuf_puts(&hb,fl);char cl[64];snprintf(cl,sizeof cl,"Content-Length: %zu\r\n",strlen(rb));dbuf_puts(&hb,cl);char ct[256];snprintf(ct,sizeof ct,"Content-Type: %s\r\n",ctype);dbuf_puts(&hb,ct);dbuf_puts(&hb,"X-Content-Type-Options: nosniff\r\nStrict-Transport-Security: max-age=31536000; includeSubDomains\r\nConnection: close\r\n");if(custom.t==VSTRUCT){Value bs=stget(custom.u.st,"__buckets");if(bs.t==VARR)for(size_t bi=0;bi<bs.u.a->n;bi++){Value bucket=bs.u.a->v[bi];if(bucket.t!=VARR)continue;for(size_t bj=0;bj<bucket.u.a->n;bj++){Value pair=bucket.u.a->v[bj];if(pair.t==VARR&&pair.u.a->n>=2&&pair.u.a->v[0].t==VSTR&&pair.u.a->v[1].t==VSTR&&http_header_value_safe(pair.u.a->v[0].u.s)&&http_header_value_safe(pair.u.a->v[1].u.s)){char line[512];snprintf(line,sizeof line,"%s: %s\r\n",pair.u.a->v[0].u.s,pair.u.a->v[1].u.s);dbuf_puts(&hb,line);}}}}
        dbuf_puts(&hb,"\r\n");https_send_all(ssl,hb.d,hb.n);https_send_all(ssl,rb,strlen(rb));free(hb.d);xfree(body);xfree(path);SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);
    }
    haris_socket_close(srv);SSL_CTX_free(ctx);
#ifdef _WIN32
    WSACleanup();
#endif
    return vb(1);
}

static Value web_serve_static(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.serve_static");
    if(n<1||n>4||a[0].t!=VSTR)return vb(0); const char*root=a[0].u.s;const char*host=(n>=2&&a[1].t==VSTR)?a[1].u.s:"127.0.0.1";int port=(n>=3&&a[2].t==VINT)?(int)a[2].u.i:8080;int maxreq=(n>=4&&a[3].t==VINT)?(int)a[3].u.i:0;
    if(port<1||port>65535||maxreq<0||strlen(root)==0)return vb(0);if(!haris_socket_init())return vb(0);
    char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo h={0},*res=0;h.ai_family=AF_UNSPEC;h.ai_socktype=SOCK_STREAM;h.ai_flags=AI_PASSIVE;int rc=getaddrinfo(host,ps,&h,&res);if(rc){
#ifdef _WIN32
        WSACleanup();
#endif
        return vb(0);
    }
    haris_socket_t srv=HARIS_INVALID_SOCKET;int one=1;
    for(struct addrinfo*p=res;p;p=p->ai_next){srv=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(srv==HARIS_INVALID_SOCKET)continue;
#ifdef _WIN32
        setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof one);
#else
        setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
#endif
        if(bind(srv,p->ai_addr,(int)p->ai_addrlen)==0&&listen(srv,32)==0)break;haris_socket_close(srv);srv=HARIS_INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if(srv==HARIS_INVALID_SOCKET){
#ifdef _WIN32
        WSACleanup();
#endif
        return vb(0);
    }
    int served=0;while(maxreq==0||served<maxreq){
        haris_socket_t c=accept(srv,NULL,NULL);if(c==HARIS_INVALID_SOCKET)break;haris_socket_tune_server(c);served++;
        char req[8192];
#ifdef _WIN32
        int got=recv(c,req,(int)sizeof req-1,0);
#else
        ssize_t got=recv(c,req,sizeof req-1,0);
#endif
        if(got<=0){haris_socket_close(c);continue;}req[got]=0;char method[8]={0},target[4096]={0};if(sscanf(req,"%7s %4095s",method,target)!=2){haris_socket_close(c);continue;}
        int head=!strcmp(method,"HEAD"),get=!strcmp(method,"GET");if(!get&&!head){const char*r="HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";send(c,r,(int)strlen(r),0);haris_socket_close(c);continue;}
        char*q=strchr(target,'?');if(q)*q=0;char*path=web_url_decode(target);if(!path){haris_socket_close(c);continue;}if(path[0]=='/')memmove(path,path+1,strlen(path));if(!*path){xfree(path);path=xdup("index.html");}
        char*full=web_join(root,path);struct stat st={0};int ok=full&&web_path_inside_root(root,full)&&stat(full,&st)==0&&S_ISREG(st.st_mode)&&st.st_size<=16*1024*1024;
        if(ok){char*data=readf_limit(full,16*1024*1024);if(!data)ok=0;else{char hdr[512];int hn=snprintf(hdr,sizeof hdr,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %lld\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",web_mime(full),(long long)st.st_size);send(c,hdr,hn,0);if(!head)send(c,data,(int)st.st_size,0);xfree(data);}}
        if(!ok){const char*body="Not Found";char hdr[256];int hn=snprintf(hdr,sizeof hdr,"HTTP/1.1 404 Not Found\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: 9\r\nConnection: close\r\n\r\n%s",body);send(c,hdr,hn,0);}
        xfree(path);xfree(full);haris_socket_close(c);
    }
    haris_socket_close(srv);
#ifdef _WIN32
    WSACleanup();
#endif
    return vb(1);
}

