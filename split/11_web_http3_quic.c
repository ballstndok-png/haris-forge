/* ---------------- HTTP/3 over QUIC + nghttp3 ---------------- */
#ifdef HARIS_HAVE_HTTP3
typedef struct HH3Conn HH3Conn;
typedef struct HH3Stream { SSL *ssl; int64_t id; int active; int local; } HH3Stream;
struct HH3Conn {
    SSL *conn;
    nghttp3_conn *h3;
    HH3Stream st[96]; int nst;
    char root[PATH_MAX];
    char path[4096]; char method[16];
    int64_t req_sid; int headers_done; int response_submitted; int response_done; int stop;
    unsigned char *body; size_t body_len,body_off;
    int maxreq,served;
};
static HH3Stream *h3_stream(HH3Conn*c,int64_t id){for(int i=0;i<c->nst;i++)if(c->st[i].active&&c->st[i].id==id)return &c->st[i];return NULL;}
static HH3Stream *h3_add_stream(HH3Conn*c,SSL*s){if(!c||!s||c->nst>=(int)(sizeof(c->st)/sizeof(c->st[0])))return NULL;HH3Stream*q=&c->st[c->nst++];memset(q,0,sizeof*q);q->ssl=s;q->id=(int64_t)SSL_get_stream_id(s);q->active=1;q->local=SSL_is_stream_local(s);return q;}
static int h3_alpn_cb(SSL *ssl,const unsigned char **out,unsigned char *outlen,const unsigned char *in,unsigned int inlen,void *arg){
    (void)ssl;(void)arg;static const unsigned char proto[]="h3";for(unsigned int i=0;i<inlen;){unsigned int n=in[i++];if(i+n>inlen)break;if(n==2&&!memcmp(in+i,proto,2)){*out=proto;*outlen=2;return SSL_TLSEXT_ERR_OK;}i+=n;}return SSL_TLSEXT_ERR_NOACK;
}
static nghttp3_ssize h3_read_data_cb(nghttp3_conn *conn,int64_t sid,nghttp3_vec *vec,size_t veccnt,uint32_t*pflags,void*cu,void*su){
    (void)conn;(void)sid;(void)su;HH3Conn*c=(HH3Conn*)cu;if(!c||veccnt==0)return NGHTTP3_ERR_CALLBACK_FAILURE;
    size_t rem=c->body_len-c->body_off;if(!rem){*pflags=NGHTTP3_DATA_FLAG_EOF;return 0;}size_t z=rem>65536?65536:rem;vec[0].base=c->body+c->body_off;vec[0].len=z;c->body_off+=z;if(c->body_off==c->body_len)*pflags=NGHTTP3_DATA_FLAG_EOF;return 1;
}
static int h3_begin_headers(nghttp3_conn*c,int64_t sid,void*cu,void*su){(void)c;(void)su;HH3Conn*x=cu;if(!x||sid<0)return NGHTTP3_ERR_CALLBACK_FAILURE;if(x->req_sid!=-1&&x->req_sid!=sid)return NGHTTP3_ERR_CALLBACK_FAILURE;x->req_sid=sid;x->path[0]=0;x->method[0]=0;x->headers_done=0;return 0;}
static int h3_recv_header(nghttp3_conn*c,int64_t sid,int32_t token,nghttp3_rcbuf*n,nghttp3_rcbuf*v,uint8_t flags,void*cu,void*su){
    (void)c;(void)token;(void)flags;(void)su;HH3Conn*x=cu;nghttp3_vec nb=nghttp3_rcbuf_get_buf(n),vbv=nghttp3_rcbuf_get_buf(v);
    if(!x||sid!=x->req_sid)return NGHTTP3_ERR_CALLBACK_FAILURE;
    if(nb.len==5&&!memcmp(nb.base,":path",5)){if(vbv.len>=sizeof(x->path))return NGHTTP3_ERR_CALLBACK_FAILURE;memcpy(x->path,vbv.base,vbv.len);x->path[vbv.len]=0;}
    else if(nb.len==7&&!memcmp(nb.base,":method",7)){size_t z=vbv.len<sizeof(x->method)-1?vbv.len:sizeof(x->method)-1;memcpy(x->method,vbv.base,z);x->method[z]=0;}
    return 0;
}
static int h3_end_headers(nghttp3_conn*c,int64_t sid,int fin,void*cu,void*su){(void)c;(void)fin;(void)su;HH3Conn*x=cu;if(!x||sid!=x->req_sid)return NGHTTP3_ERR_CALLBACK_FAILURE;x->headers_done=1;return 0;}
static int h3_recv_data(nghttp3_conn*c,int64_t sid,const uint8_t*d,size_t n,void*cu,void*su){(void)c;(void)sid;(void)d;(void)n;(void)cu;(void)su;return 0;}
static int h3_end_stream(nghttp3_conn*c,int64_t sid,void*cu,void*su);
static int h3_stream_close_cb(nghttp3_conn*c,int64_t sid,uint64_t code,void*cu,void*su){(void)c;(void)sid;(void)code;(void)su;HH3Conn*x=cu;HH3Stream*s=h3_stream(x,sid);if(s)s->active=0;return 0;}
static int h3_noop_ack(nghttp3_conn*c,int64_t sid,uint64_t n,void*cu,void*su){(void)c;(void)sid;(void)n;(void)cu;(void)su;return 0;}
static int h3_noop_consume(nghttp3_conn*c,int64_t sid,size_t n,void*cu,void*su){(void)c;(void)sid;(void)n;(void)cu;(void)su;return 0;}
static int h3_noop_stop(nghttp3_conn*c,int64_t sid,uint64_t code,void*cu,void*su){(void)c;(void)su;HH3Conn*x=cu;HH3Stream*s=h3_stream(x,sid);if(s){SSL_STREAM_RESET_ARGS a={code};SSL_stream_reset(s->ssl,&a,sizeof a);}return 0;}
static int h3_noop_reset(nghttp3_conn*c,int64_t sid,uint64_t code,void*cu,void*su){(void)c;(void)sid;(void)code;(void)cu;(void)su;return 0;}
static int h3_noop_shutdown(nghttp3_conn*c,int64_t id,void*cu){(void)c;(void)id;(void)cu;return 0;}
static int h3_noop_settings(nghttp3_conn*c,const nghttp3_settings*s,void*cu){(void)c;(void)s;(void)cu;return 0;}
static int h3_submit_static_response(HH3Conn*x){
    if(!x||x->response_submitted)return 1;x->response_submitted=1;
    const char*method=x->method[0]?x->method:"GET";int head=!strcmp(method,"HEAD"),get=!strcmp(method,"GET");int status=200;
    char *decoded=web_url_decode(x->path[0]?x->path:"/");if(!decoded)return 0;if(decoded[0]=='/')memmove(decoded,decoded+1,strlen(decoded));if(!*decoded){xfree(decoded);decoded=xdup("index.html");}
    char *full=web_join(x->root,decoded);xfree(decoded);struct stat st={0};int ok=full&&stat(full,&st)==0&&S_ISREG(st.st_mode)&&st.st_size<=16*1024*1024;
    if(!get&&!head)ok=0;if(ok){char*d=readf_limit(full,16*1024*1024);if(!d)ok=0;else{x->body=(unsigned char*)d;x->body_len=head?0:(size_t)st.st_size;x->body_off=0;}}
    if(full)xfree(full);
    if(!ok){status=(!get&&!head)?405:404;x->body=(unsigned char*)xdup(status==405?"Method Not Allowed":"Not Found");x->body_len=strlen((char*)x->body);x->body_off=0;}
    char clen[32];snprintf(clen,sizeof clen,"%zu",x->body_len);
    const char*ctype=ok?web_mime(x->path):"text/plain; charset=utf-8";
    char status_s[4];snprintf(status_s,sizeof status_s,"%d",status);
    nghttp3_nv hdrs[4];memset(hdrs,0,sizeof hdrs);
    hdrs[0]=(nghttp3_nv){(uint8_t*)":status",(uint8_t*)status_s,7,strlen(status_s),NGHTTP3_NV_FLAG_NONE};
    hdrs[1]=(nghttp3_nv){(uint8_t*)"content-type",(uint8_t*)ctype,12,strlen(ctype),NGHTTP3_NV_FLAG_NONE};
    hdrs[2]=(nghttp3_nv){(uint8_t*)"content-length",(uint8_t*)clen,14,strlen(clen),NGHTTP3_NV_FLAG_NONE};
    hdrs[3]=(nghttp3_nv){(uint8_t*)"x-content-type-options",(uint8_t*)"nosniff",22,7,NGHTTP3_NV_FLAG_NONE};
    nghttp3_data_reader dr={h3_read_data_cb};
    if(head){x->body_len=0;x->body_off=0;}
    int rc=nghttp3_conn_submit_response(x->h3,x->req_sid,hdrs,4,head?NULL:&dr);
    return rc==0;
}
static int h3_end_stream(nghttp3_conn*c,int64_t sid,void*cu,void*su){(void)c;(void)su;HH3Conn*x=cu;if(!x||sid!=x->req_sid)return NGHTTP3_ERR_CALLBACK_FAILURE;return h3_submit_static_response(x)?0:NGHTTP3_ERR_CALLBACK_FAILURE;}
static int h3_write_pending(HH3Conn*x){
    for(int guard=0;guard<512;guard++){
        int64_t sid=-1;int fin=0;nghttp3_vec vec[8];nghttp3_ssize nv=nghttp3_conn_writev_stream(x->h3,&sid,&fin,vec,8);if(nv<0)return 0;if(nv==0){if(sid<0)return 1;}
        HH3Stream*s=h3_stream(x,sid);if(!s||!s->ssl)return 0;
        size_t total=0;for(nghttp3_ssize i=0;i<nv;i++){size_t off=0;while(off<vec[i].len){size_t w=0;if(SSL_write_ex(s->ssl,vec[i].base+off,vec[i].len-off,&w)!=1||w==0)return 0;off+=w;total+=w;}}
        if(nghttp3_conn_add_write_offset(x->h3,sid,total)!=0)return 0;
        if(fin){if(SSL_stream_conclude(s->ssl,0)!=1)return 0;if(sid==x->req_sid)x->response_done=1;}
    }
    return 1;
}
static int h3_process_stream(HH3Conn*x,HH3Stream*s){
    unsigned char buf[16384];
    for(int guard=0;guard<64;guard++){
        size_t n=0;int ok=SSL_read_ex(s->ssl,buf,sizeof buf,&n);
        if(ok==1){int64_t consumed=nghttp3_conn_read_stream(x->h3,s->id,buf,n,0);if(consumed<0)return 0;continue;}
        int e=SSL_get_error(s->ssl,0);if(e==SSL_ERROR_WANT_READ||e==SSL_ERROR_WANT_WRITE)return 1;
        if(e==SSL_ERROR_ZERO_RETURN){int64_t rc=nghttp3_conn_read_stream(x->h3,s->id,NULL,0,1);return rc>=0;}
        return 0;
    }
    return 1;
}
static int h3_server_conn(HH3Conn*x){
    nghttp3_callbacks cb;memset(&cb,0,sizeof cb);cb.acked_stream_data=h3_noop_ack;cb.stream_close=h3_stream_close_cb;cb.recv_data=h3_recv_data;cb.deferred_consume=h3_noop_consume;cb.begin_headers=h3_begin_headers;cb.recv_header=h3_recv_header;cb.end_headers=h3_end_headers;cb.stop_sending=h3_noop_stop;cb.end_stream=h3_end_stream;cb.reset_stream=h3_noop_reset;cb.shutdown=h3_noop_shutdown;cb.recv_settings=h3_noop_settings;
    nghttp3_settings settings;nghttp3_settings_default(&settings);
    if(nghttp3_conn_server_new(&x->h3,&cb,&settings,NULL,x)<0)return 0;
    nghttp3_conn_set_max_concurrent_streams(x->h3,64);
    if(SSL_set_incoming_stream_policy(x->conn,SSL_INCOMING_STREAM_POLICY_ACCEPT,0)!=1)return 0;
    SSL_set_blocking_mode(x->conn,0);
    SSL *ctl=SSL_new_stream(x->conn,SSL_STREAM_FLAG_UNI|SSL_STREAM_FLAG_NO_BLOCK);SSL *enc=SSL_new_stream(x->conn,SSL_STREAM_FLAG_UNI|SSL_STREAM_FLAG_NO_BLOCK);SSL *dec=SSL_new_stream(x->conn,SSL_STREAM_FLAG_UNI|SSL_STREAM_FLAG_NO_BLOCK);
    if(!ctl||!enc||!dec)return 0;SSL_set_blocking_mode(ctl,0);SSL_set_blocking_mode(enc,0);SSL_set_blocking_mode(dec,0);if(!h3_add_stream(x,ctl)||!h3_add_stream(x,enc)||!h3_add_stream(x,dec))return 0;
    if(nghttp3_conn_bind_control_stream(x->h3,(int64_t)SSL_get_stream_id(ctl))!=0)return 0;
    if(nghttp3_conn_bind_qpack_streams(x->h3,(int64_t)SSL_get_stream_id(enc),(int64_t)SSL_get_stream_id(dec))!=0)return 0;
    for(int guard=0;(!x->stop) && (x->maxreq==0 || guard<120000);guard++){
        SSL_handle_events(x->conn);
        for(;;){SSL *ns=SSL_accept_stream(x->conn,SSL_ACCEPT_STREAM_NO_BLOCK);if(!ns)break;SSL_set_blocking_mode(ns,0);if(!h3_add_stream(x,ns)){SSL_free(ns);return 0;}}
        for(int i=0;i<x->nst;i++)if(x->st[i].active&&!x->st[i].local)if(!h3_process_stream(x,&x->st[i]))x->st[i].active=0;
        if(!h3_write_pending(x))return 0;
        if(x->response_done){x->served++;if(x->maxreq>0&&x->served>=x->maxreq)x->stop=1;else x->response_done=0;x->req_sid=-1;x->response_submitted=0;x->headers_done=0;if(x->body){xfree(x->body);x->body=NULL;}x->body_len=x->body_off=0;}
        struct timespec ts={0,1000000L};nanosleep(&ts,NULL);
    }
    return 1;
}
static Value web_http3_serve_static(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB)||!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_WEB|CAP_FS,"web.http3_serve_static");
    if(n<3||n>6||a[0].t!=VSTR||a[1].t!=VSTR||a[2].t!=VSTR)return vb(0);
#ifndef _WIN32
    if(!fs_path_allowed(vm,a[0].u.s)||!fs_path_allowed(vm,a[1].u.s)||!fs_path_allowed(vm,a[2].u.s))return vb(0);
#endif
    int port=(n>=4&&a[3].t==VINT)?(int)a[3].u.i:8443;const char*host=(n>=5&&a[4].t==VSTR)?a[4].u.s:"0.0.0.0";int maxreq=(n>=6&&a[5].t==VINT)?(int)a[5].u.i:0;if(port<1||port>65535||maxreq<0)return vb(0);if(!haris_socket_init())return vb(0);
    SSL_CTX*ctx=SSL_CTX_new(OSSL_QUIC_server_method());if(!ctx)return vb(0);SSL_CTX_set_min_proto_version(ctx,TLS1_3_VERSION);SSL_CTX_set_max_proto_version(ctx,TLS1_3_VERSION);SSL_CTX_set_alpn_select_cb(ctx,h3_alpn_cb,NULL);if(SSL_CTX_use_certificate_chain_file(ctx,a[1].u.s)!=1||SSL_CTX_use_PrivateKey_file(ctx,a[2].u.s,SSL_FILETYPE_PEM)!=1||SSL_CTX_check_private_key(ctx)!=1){SSL_CTX_free(ctx);return vb(0);}
    struct addrinfo h={0},*res=0;char ps[16];snprintf(ps,sizeof ps,"%d",port);h.ai_family=AF_INET;h.ai_socktype=SOCK_DGRAM;h.ai_protocol=IPPROTO_UDP;if(getaddrinfo(host,ps,&h,&res)){SSL_CTX_free(ctx);return vb(0);}haris_socket_t fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd==HARIS_INVALID_SOCKET){freeaddrinfo(res);SSL_CTX_free(ctx);return vb(0);}int one=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof one);if(bind(fd,res->ai_addr,(socklen_t)res->ai_addrlen)!=0){freeaddrinfo(res);haris_socket_close(fd);SSL_CTX_free(ctx);return vb(0);}freeaddrinfo(res);
    BIO_socket_nbio(fd,1);SSL*listener=SSL_new_listener(ctx,0);if(!listener||SSL_set_fd(listener,fd)!=1||SSL_set_blocking_mode(listener,0)!=1||SSL_listen(listener)!=1){if(listener)SSL_free(listener);haris_socket_close(fd);SSL_CTX_free(ctx);return vb(0);}
    int served_total=0;while(maxreq==0||served_total<maxreq){SSL_handle_events(listener);SSL*c=SSL_accept_connection(listener,SSL_ACCEPT_CONNECTION_NO_BLOCK);if(!c){struct timespec ts={0,1000000L};nanosleep(&ts,NULL);continue;}HH3Conn x;memset(&x,0,sizeof x);x.conn=c;x.req_sid=-1;x.maxreq=maxreq>0?(maxreq-served_total):0;snprintf(x.root,sizeof x.root,"%s",a[0].u.s);x.stop=0;int ok=h3_server_conn(&x);served_total+=x.served;if(x.body)xfree(x.body);if(x.h3)nghttp3_conn_del(x.h3);SSL_shutdown_ex(c,SSL_SHUTDOWN_FLAG_NO_STREAM_FLUSH,NULL,0);SSL_free(c);if(!ok&&maxreq>0&&served_total<maxreq)break;}
    SSL_free(listener);haris_socket_close(fd);SSL_CTX_free(ctx);return vb(1);
}
#endif /* HARIS_HAVE_HTTP3 */
#ifndef HARIS_HAVE_HTTP3
static Value web_http3_serve_static(VM*vm,int n,Value*a){(void)a;return cap_error(vm,CAP_WEB,"web.http3_serve_static requires -DHARIS_USE_HTTP3 + OpenSSL QUIC + nghttp3");}
#endif
static Value web_http3_info(VM*vm,int n,Value*a){(void)vm;(void)a;if(n!=0)return vn();Value o=vsobj();
#ifdef HARIS_HAVE_HTTP3
    stput(o.u.st,"available",vb(1));stput(o.u.st,"transport",vs("OpenSSL QUIC"));stput(o.u.st,"http3",vs("h3"));
    stput(o.u.st,"nghttp3",vs(NGHTTP3_VERSION));stput(o.u.st,"tls",vs("1.3"));
#else
    stput(o.u.st,"available",vb(0));stput(o.u.st,"transport",vs("unavailable"));stput(o.u.st,"http3",vs("build with -DHARIS_USE_HTTP3 and libnghttp3/OpenSSL 3.5+"));
#endif
    return o;}

