/* -------- Distributed all-reduce (TCP full-mesh, POSIX/Windows socket wrappers) -------- */
typedef struct {int kind;int rank,world;haris_socket_t *peer;int peer_n;} HDist;
static void hdist_dtor(void*p){HDist*d=(HDist*)p;if(!d)return;for(int i=0;i<d->world;i++)if(i!=d->rank&&d->peer&&d->peer[i]!=HARIS_INVALID_SOCKET)haris_socket_close(d->peer[i]);if(d->peer)xfree(d->peer);d->kind=HK_CLOSED;}
static int hdist_sendall(haris_socket_t s,const void*p,size_t n){const unsigned char*b=p;while(n){int z=(int)(n>65536?65536:n);int r=send(s,(const char*)b,z,0);if(r<=0)return 0;b+=r;n-=(size_t)r;}return 1;}
static int hdist_recvall(haris_socket_t s,void*p,size_t n){unsigned char*b=p;while(n){int z=(int)(n>65536?65536:n);int r=recv(s,(char*)b,z,0);if(r<=0)return 0;b+=r;n-=(size_t)r;}return 1;}
static int hdist_set_keepalive(haris_socket_t s){
    int one=1;
    if(setsockopt(s,SOL_SOCKET,SO_KEEPALIVE,(const char*)&one,(int)sizeof one)!=0)return 0;
#ifdef _WIN32
    /* Windows uses system TCP keepalive defaults; the 30s application timeout
       below remains the authoritative failure bound. */
    return 1;
#else
#ifdef TCP_KEEPIDLE
    int idle=5;setsockopt(s,IPPROTO_TCP,TCP_KEEPIDLE,&idle,sizeof idle);
#endif
#ifdef TCP_KEEPINTVL
    int intvl=5;setsockopt(s,IPPROTO_TCP,TCP_KEEPINTVL,&intvl,sizeof intvl);
#endif
#ifdef TCP_KEEPCNT
    int cnt=6;setsockopt(s,IPPROTO_TCP,TCP_KEEPCNT,&cnt,sizeof cnt);
#endif
    return 1;
#endif
}
static int hdist_set_timeout(haris_socket_t s,unsigned ms){
#ifdef _WIN32
    DWORD v=(DWORD)ms;return setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(const char*)&v,(int)sizeof v)==0 && setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,(const char*)&v,(int)sizeof v)==0;
#else
    struct timeval tv={(time_t)(ms/1000u),(suseconds_t)((ms%1000u)*1000u)};return setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv)==0 && setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv)==0;
#endif
}
#define HDIST_HEARTBEAT_MAGIC 0x48425254u
static int hdist_heartbeat(HDist*d,unsigned timeout_ms){
    if(!d||d->world<=1)return 1;
    uint32_t ping=HDIST_HEARTBEAT_MAGIC, pong=0;
    for(int r=0;r<d->world;r++) if(r!=d->rank){
        if(!hdist_set_timeout(d->peer[r],timeout_ms)||!hdist_sendall(d->peer[r],&ping,4)||!hdist_recvall(d->peer[r],&pong,4)||pong!=HDIST_HEARTBEAT_MAGIC)return 0;
    }
    return 1;
}
static int hdist_heartbeat_collective(HDist*d,unsigned timeout_ms){
    if(!d||d->world<=1)return 1;
    uint32_t ping=HDIST_HEARTBEAT_MAGIC,pong=0;
    for(int r=0;r<d->world;r++) if(r!=d->rank){
        if(!hdist_set_timeout(d->peer[r],timeout_ms))return 0;
        if(d->rank<r){
            if(!hdist_sendall(d->peer[r],&ping,4)||!hdist_recvall(d->peer[r],&pong,4)||pong!=HDIST_HEARTBEAT_MAGIC)return 0;
        } else {
            if(!hdist_recvall(d->peer[r],&pong,4)||pong!=HDIST_HEARTBEAT_MAGIC||!hdist_sendall(d->peer[r],&ping,4))return 0;
        }
    }
    return 1;
}

static int hdist_parse_endpoint(const char*s,char*host,size_t hs,int*port){if(!s||!host||!port)return 0;const char*c=strrchr(s,':');if(!c||c==s||!c[1])return 0;size_t hn=(size_t)(c-s);if(hn>=hs)return 0;memcpy(host,s,hn);host[hn]=0;*port=atoi(c+1);return *port>0&&*port<65536;}
static void hdist_sleep_ms(unsigned ms){
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;ts.tv_sec=(time_t)(ms/1000u);ts.tv_nsec=(long)(ms%1000u)*1000000L;nanosleep(&ts,NULL);
#endif
}
static haris_socket_t hdist_listen(const char*host,int port){
    haris_socket_t bad=HARIS_INVALID_SOCKET;if(!haris_socket_init()||!host||port<1||port>65535)return bad;
    char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo h={0},*res=NULL;h.ai_family=AF_UNSPEC;h.ai_socktype=SOCK_STREAM;h.ai_flags=0;
    if(!strcmp(host,"0.0.0.0")||!strcmp(host,"::"))h.ai_flags=AI_PASSIVE;
    if(getaddrinfo((h.ai_flags?NULL:host),ps,&h,&res)!=0)return bad;
    haris_socket_t out=bad;
    for(struct addrinfo*rp=res;rp;rp=rp->ai_next){
        haris_socket_t s=socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);if(s==HARIS_INVALID_SOCKET)continue;
        int one=1;setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,(int)sizeof one);
        if(bind(s,rp->ai_addr,(socklen_t)rp->ai_addrlen)==0&&listen(s,32)==0){out=s;break;}
        haris_socket_close(s);
    }
    freeaddrinfo(res);return out;
}
static haris_socket_t hdist_connect(const char*host,int port){haris_socket_t s=HARIS_INVALID_SOCKET;if(!resolve_and_socket(host,port,SOCK_STREAM,&s))return HARIS_INVALID_SOCKET;return s;}
static Value distributed_init(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"distributed_init");if(n<3||n>4||a[0].t!=VSTR||a[1].t!=VINT||a[2].t!=VINT)return vn();int rank=(int)a[1].u.i,world=(int)a[2].u.i,timeout=n==4&&a[3].t==VINT?(int)a[3].u.i:10000;if(world<1||world>32||rank<0||rank>=world||timeout<100||timeout>120000)return vn();Value eps=a[0];char**hosts=(char**)xmalloc((size_t)world*sizeof(char*));int*ports=(int*)xmalloc((size_t)world*sizeof(int));if(!hosts||!ports){if(hosts)xfree(hosts);if(ports)xfree(ports);return vn();}for(int i=0;i<world;i++)hosts[i]=NULL;char tmp[4096];snprintf(tmp,sizeof tmp,"%s",eps.u.s);char*save=0;int ec=0;for(char*t=strtok_r(tmp,",",&save);t&&ec<world;t=strtok_r(NULL,",",&save),ec++){char hb[256];if(!hdist_parse_endpoint(t,hb,sizeof hb,&ports[ec])){ec=-1;break;}hosts[ec]=xdup(hb);}if(ec!=world){for(int i=0;i<world;i++)if(hosts[i])xfree(hosts[i]);xfree(hosts);xfree(ports);return vn();}
    HDist*d=(HDist*)xmalloc_dtor(sizeof*d,hdist_dtor);memset(d,0,sizeof*d);d->kind=HK_DIST;d->rank=rank;d->world=world;d->peer=(haris_socket_t*)xmalloc((size_t)world*sizeof(*d->peer));for(int i=0;i<world;i++)d->peer[i]=HARIS_INVALID_SOCKET;haris_socket_t ls=hdist_listen(hosts[rank],ports[rank]);if(world>1&&ls==HARIS_INVALID_SOCKET){for(int i=0;i<world;i++)if(hosts[i])xfree(hosts[i]);xfree(hosts);xfree(ports);hdist_dtor(d);return vn();}
    for(int r=0;r<rank;r++){unsigned long long dl=sandbox_clock_ms()+(unsigned long long)timeout;haris_socket_t s=HARIS_INVALID_SOCKET;while(s==HARIS_INVALID_SOCKET&&sandbox_clock_ms()<dl){s=hdist_connect(hosts[r],ports[r]);if(s==HARIS_INVALID_SOCKET)hdist_sleep_ms(50);}if(s==HARIS_INVALID_SOCKET){if(ls!=HARIS_INVALID_SOCKET)haris_socket_close(ls);for(int i=0;i<world;i++)if(hosts[i])xfree(hosts[i]);xfree(hosts);xfree(ports);hdist_dtor(d);return vn();}uint32_t rr=(uint32_t)rank;if(!hdist_sendall(s,&rr,sizeof rr)){haris_socket_close(s);hdist_dtor(d);return vn();}d->peer[r]=s;hdist_set_keepalive(s);}
    for(int got=rank+1;got<world;got++){if(!haris_socket_wait(ls,1,0,timeout)){if(ls!=HARIS_INVALID_SOCKET)haris_socket_close(ls);for(int i=0;i<world;i++)if(hosts[i])xfree(hosts[i]);xfree(hosts);xfree(ports);hdist_dtor(d);return vn();}haris_socket_t s=accept(ls,NULL,NULL);if(s==HARIS_INVALID_SOCKET){got--;continue;}uint32_t rr=0;if(!hdist_recvall(s,&rr,sizeof rr)||rr>=world||rr==(uint32_t)rank){haris_socket_close(s);got--;continue;}d->peer[rr]=s;hdist_set_keepalive(s);}
    if(ls!=HARIS_INVALID_SOCKET)haris_socket_close(ls);
    for(int i=0;i<world;i++)if(i!=rank)if(!hdist_set_timeout(d->peer[i],30000)){for(int j=0;j<world;j++)if(hosts[j])xfree(hosts[j]);xfree(hosts);xfree(ports);hdist_dtor(d);return vn();}
    if(!hdist_heartbeat_collective(d,30000)){for(int i=0;i<world;i++)if(hosts[i])xfree(hosts[i]);xfree(hosts);xfree(ports);hdist_dtor(d);return vn();}
    for(int i=0;i<world;i++)if(hosts[i])xfree(hosts[i]);xfree(hosts);xfree(ports);Value out=(Value){.t=VHANDLE,.u.handle=d};return out;}
static Value distributed_allreduce(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"distributed_allreduce");if(n!=2||!handle_kind(a[0],HK_DIST)||a[1].t!=VARR)return vn();HDist*d=a[0].u.handle;if(!hdist_heartbeat_collective(d,30000))return vn();size_t N=a[1].u.a->n;if(N>1<<20)return vn();double*buf=(double*)xmalloc(N*sizeof(double));if(!buf)return vn();for(size_t i=0;i<N;i++){if(!isnum(a[1].u.a->v[i])){xfree(buf);return vn();}buf[i]=dn(a[1].u.a->v[i]);}uint64_t nn=(uint64_t)N;for(int r=0;r<d->world;r++)if(r!=d->rank){if(!hdist_sendall(d->peer[r],&nn,sizeof nn)||!hdist_sendall(d->peer[r],buf,N*sizeof(double))){xfree(buf);return vn();}}for(int r=0;r<d->world;r++)if(r!=d->rank){uint64_t rn=0;double*tmp=(double*)xmalloc(N*sizeof(double));if(!tmp||!hdist_recvall(d->peer[r],&rn,sizeof rn)||rn!=nn||!hdist_recvall(d->peer[r],tmp,N*sizeof(double))){if(tmp)xfree(tmp);xfree(buf);return vn();}for(size_t i=0;i<N;i++)buf[i]+=tmp[i];xfree(tmp);}Value out=va();for(size_t i=0;i<N;i++)ap(out.u.a,vf(buf[i]));xfree(buf);return out;}
static Value distributed_barrier(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"distributed_barrier");if(n!=1||!handle_kind(a[0],HK_DIST))return vb(0);HDist*d=a[0].u.handle;if(!hdist_heartbeat_collective(d,30000))return vb(0);uint32_t token=0x48424152u;for(int r=0;r<d->world;r++)if(r!=d->rank)if(!hdist_sendall(d->peer[r],&token,4))return vb(0);for(int r=0;r<d->world;r++)if(r!=d->rank){uint32_t x;if(!hdist_recvall(d->peer[r],&x,4)||x!=token)return vb(0);}return vb(1);}
static Value distributed_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||!handle_kind(a[0],HK_DIST))return vn();HDist*d=a[0].u.handle;Value o=vsobj();stput(o.u.st,"rank",vi(d->rank));stput(o.u.st,"world",vi(d->world));stput(o.u.st,"backend",vs("tcp-full-mesh"));return o;}
static Value distributed_close(VM*vm,int n,Value*a){(void)vm;if(n!=1||!handle_kind(a[0],HK_DIST))return vb(0);hdist_dtor(a[0].u.handle);return vb(1);}

