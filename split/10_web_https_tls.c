/* ---------------- Hardened HTTPS server + TLS profile ---------------- */
static int tls_version_value(const Value *v, int *out){
    if(!v||!out)return 0;
    if(v->t==VINT){ if(v->u.i==12){*out=TLS1_2_VERSION;return 1;} if(v->u.i==13){
#ifdef TLS1_3_VERSION
        *out=TLS1_3_VERSION;return 1;
#else
        return 0;
#endif
    }}
    if(v->t!=VSTR)return 0;
    if(!strcmp(v->u.s,"1.2")||!strcmp(v->u.s,"tls1.2")||!strcmp(v->u.s,"TLS1.2")){*out=TLS1_2_VERSION;return 1;}
#ifdef TLS1_3_VERSION
    if(!strcmp(v->u.s,"1.3")||!strcmp(v->u.s,"tls1.3")||!strcmp(v->u.s,"TLS1.3")){*out=TLS1_3_VERSION;return 1;}
#endif
    return 0;
}
static const char *tls_default12(void){
    return "ECDHE+AESGCM:ECDHE+CHACHA20:!aNULL:!eNULL:!MD5:!RC4";
}
static const char *tls_default13(void){
#ifdef TLS1_3_VERSION
    return "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256";
#else
    return "";
#endif
}
static int https_alpn_cb(SSL *ssl,const unsigned char **out,unsigned char *outlen,
                         const unsigned char *in,unsigned int inlen,void *arg){
    (void)ssl;(void)arg;
    static const unsigned char proto[]="http/1.1";
    unsigned int i=0;
    while(i+1<inlen){unsigned int n=in[i++];if(i+n>inlen)break;if(n==8&&!memcmp(in+i,proto+0,8)){*out=proto;*outlen=8;return SSL_TLSEXT_ERR_OK;}i+=n;}
    return SSL_TLSEXT_ERR_NOACK;
}
static int tls_configure_server(SSL_CTX *ctx,const char *cert,const char *key,
                                int minv,int maxv,const char *c12,const char *c13){
    if(!ctx||!cert||!key)return 0;
    if(SSL_CTX_set_min_proto_version(ctx,minv)!=1)return 0;
    if(SSL_CTX_set_max_proto_version(ctx,maxv)!=1)return 0;
    if(c12&&*c12&&SSL_CTX_set_cipher_list(ctx,c12)!=1)return 0;
#ifdef TLS1_3_VERSION
    if(maxv>=TLS1_3_VERSION && minv<=TLS1_3_VERSION && c13&&*c13){
        if(SSL_CTX_set_ciphersuites(ctx,c13)!=1)return 0;
    }
#endif
    SSL_CTX_set_options(ctx,SSL_OP_NO_COMPRESSION|SSL_OP_CIPHER_SERVER_PREFERENCE);
    SSL_CTX_set_mode(ctx,SSL_MODE_RELEASE_BUFFERS);
    SSL_CTX_set_num_tickets(ctx,2);
    SSL_CTX_set_alpn_select_cb(ctx,https_alpn_cb,NULL);
    if(SSL_CTX_use_certificate_chain_file(ctx,cert)!=1)return 0;
    if(SSL_CTX_use_PrivateKey_file(ctx,key,SSL_FILETYPE_PEM)!=1)return 0;
    if(SSL_CTX_check_private_key(ctx)!=1)return 0;
    return 1;
}
static int https_send_all(SSL*ssl,const void*buf,size_t n){
    const unsigned char*p=(const unsigned char*)buf;size_t off=0;
    while(off<n){size_t w=0;if(SSL_write_ex(ssl,p+off,n-off,&w)!=1||w==0)return 0;off+=w;}
    return 1;
}
static ssize_t https_recv_req(SSL*ssl,char*buf,size_t cap,int timeout_ms){
    size_t n=0;unsigned long long deadline=sandbox_clock_ms()+(unsigned long long)(timeout_ms>0?timeout_ms:10000);
    for(;;){
        if(n+1>=cap)return -2;
        if(sandbox_clock_ms()>=deadline)return -3;
        size_t r=0;
        int ok=SSL_read_ex(ssl,buf+n,cap-n-1,&r);
        if(ok==1){n+=r;buf[n]=0;if(strstr(buf,"\r\n\r\n"))return (ssize_t)n;continue;}
        int e=SSL_get_error(ssl,0);
        if(e==SSL_ERROR_WANT_READ||e==SSL_ERROR_WANT_WRITE){
            struct timespec ts={0,10000000L};
            nanosleep(&ts,NULL);
            continue;
        }
        return n?(ssize_t)n:-1;
    }
}
static void http_path_for_static(char *target,size_t cap){
    char*q=strchr(target,'?');if(q)*q=0;
}
static Value web_https_serve_static(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB)||!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_WEB|CAP_FS,"web.https_serve_static");
    if(n<3||n>10||a[0].t!=VSTR||a[1].t!=VSTR||a[2].t!=VSTR)return vb(0);
    const char*root=a[0].u.s,*cert=a[1].u.s,*key=a[2].u.s;
    const char*host=(n>=4&&a[3].t==VSTR)?a[3].u.s:"0.0.0.0";
    int port=(n>=5&&a[4].t==VINT)?(int)a[4].u.i:8443;
    int maxreq=(n>=6&&a[5].t==VINT)?(int)a[5].u.i:0;
    int minv=TLS1_2_VERSION,maxv=0;
#ifdef TLS1_3_VERSION
    maxv=TLS1_3_VERSION;
#else
    maxv=TLS1_2_VERSION;
#endif
    if(n>=7 && !tls_version_value(&a[6],&minv))return vb(0);
    if(n>=8 && !tls_version_value(&a[7],&maxv))return vb(0);
    const char*c12=(n>=9&&a[8].t==VSTR&&a[8].u.s[0])?a[8].u.s:tls_default12();
    const char*c13=(n>=10&&a[9].t==VSTR&&a[9].u.s[0])?a[9].u.s:tls_default13();
    /* TLS 1.3 uses its own cipher-suite configuration path in OpenSSL. */
    if(port<1||port>65535||maxreq<0||minv>maxv||maxv<TLS1_2_VERSION)return vb(0);
#ifdef TLS1_3_VERSION
    if(maxv==TLS1_3_VERSION && minv==TLS1_3_VERSION && !tls_default13()[0])return vb(0);
#else
    if(maxv>maxv)return vb(0);
#endif
    if(!fs_path_allowed(vm,root)||!fs_path_allowed(vm,cert)||!fs_path_allowed(vm,key))return vb(0);
    if(!haris_socket_init())return vb(0);
    SSL_CTX*ctx=SSL_CTX_new(TLS_server_method());if(!ctx)return vb(0);
    if(!tls_configure_server(ctx,cert,key,minv,maxv,c12,c13)){SSL_CTX_free(ctx);return vb(0);}
    char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo h={0},*res=0;h.ai_family=AF_UNSPEC;h.ai_socktype=SOCK_STREAM;h.ai_flags=AI_PASSIVE;
    if(getaddrinfo(host,ps,&h,&res)){SSL_CTX_free(ctx);return vb(0);}
    haris_socket_t srv=HARIS_INVALID_SOCKET;int one=1;
    for(struct addrinfo*p=res;p;p=p->ai_next){srv=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(srv==HARIS_INVALID_SOCKET)continue;
#ifdef _WIN32
        setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof one);
#else
        setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
#endif
        if(bind(srv,p->ai_addr,(socklen_t)p->ai_addrlen)==0&&listen(srv,64)==0)break;haris_socket_close(srv);srv=HARIS_INVALID_SOCKET;}
    freeaddrinfo(res);if(srv==HARIS_INVALID_SOCKET){SSL_CTX_free(ctx);return vb(0);}
    int served=0;
    while(maxreq==0||served<maxreq){
        haris_socket_t c=accept(srv,NULL,NULL);if(c==HARIS_INVALID_SOCKET)break;served++;
        SSL*ssl=SSL_new(ctx);if(!ssl){haris_socket_close(c);continue;}SSL_set_fd(ssl,c);SSL_set_blocking_mode(ssl,1);
        if(SSL_accept(ssl)!=1){SSL_free(ssl);haris_socket_close(c);continue;}
        char req[8192];ssize_t got=https_recv_req(ssl,req,sizeof req,10000);
        if(got>0){char method[8]={0},target[4096]={0};if(sscanf(req,"%7s %4095s",method,target)==2){
            int head=!strcmp(method,"HEAD");http_path_for_static(target,sizeof target);
            char*path=web_url_decode(target);if(path&&path[0]=='/')memmove(path,path+1,strlen(path));if(path&&!*path){xfree(path);path=xdup("index.html");}
            char*full=path?web_join(root,path):NULL;struct stat st={0};int ok=full&&stat(full,&st)==0&&S_ISREG(st.st_mode)&&st.st_size<=16*1024*1024;
            if(ok){char*data=readf_limit(full,16*1024*1024);if(data){char hdr[768];int hn=snprintf(hdr,sizeof hdr,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %lld\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nStrict-Transport-Security: max-age=31536000; includeSubDomains\r\nConnection: close\r\n\r\n",web_mime(full),(long long)st.st_size);https_send_all(ssl,hdr,(size_t)hn);if(!head)https_send_all(ssl,data,(size_t)st.st_size);xfree(data);}else ok=0;}
            if(!ok){const char*body="Not Found";char hdr[512];int hn=snprintf(hdr,sizeof hdr,"HTTP/1.1 404 Not Found\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: 9\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n%s",body);https_send_all(ssl,hdr,(size_t)hn);}if(full)xfree(full);if(path)xfree(path);
        }}
        SSL_shutdown(ssl);SSL_free(ssl);haris_socket_close(c);
    }
    haris_socket_close(srv);SSL_CTX_free(ctx);return vb(1);
}
static Value web_tls_profile(VM*vm,int n,Value*a){
    (void)vm;(void)a;if(n!=0)return vn();Value o=vsobj();
    stput(o.u.st,"openssl",vs(OpenSSL_version(OPENSSL_VERSION)));
    stput(o.u.st,"min_default",vs("TLS1.2"));
#ifdef TLS1_3_VERSION
    stput(o.u.st,"tls13",vb(1));stput(o.u.st,"max_default",vs("TLS1.3"));
#else
    stput(o.u.st,"tls13",vb(0));stput(o.u.st,"max_default",vs("TLS1.2"));
#endif
    stput(o.u.st,"tls12_ciphers",vs(tls_default12()));stput(o.u.st,"tls13_ciphers",vs(tls_default13()));
    return o;
}

