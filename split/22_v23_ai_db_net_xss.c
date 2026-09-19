/* ---------------- v2.3 AI training + DB + Network + XSS ---------------- */
static Value v23_bool(int b){return vb(b?1:0);}

/* Load a small CSV dataset for in-language training pipelines.
   The first row is treated as data too; callers can decide how to interpret it. */
static Value ai_dataset_csv(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"ai.dataset_csv");
    if(n<1||n>2||a[0].t!=VSTR)return vn();
    char*src=readf_limit(a[0].u.s,8ULL*1024*1024); if(!src)return vn();
    char delim=','; if(n==2&&a[1].t==VSTR&&a[1].u.s[0])delim=a[1].u.s[0];
    Value rows=va(); char*line=src;
    while(line&&*line){
        char*e=strchr(line,'\n'); if(e)*e=0;
        Value row=va(); char*cur=line; int quote=0; char field[8192]; size_t z=0;
        for(;;cur++){
            char c=*cur;
            if(c=='"'){ if(quote&&cur[1]=='"'){ if(z+1<sizeof field)field[z++]='"'; cur++; } else quote=!quote; }
            else if((c==delim&&!quote)||c==0){field[z]=0; ap(row.u.a,vs(field)); z=0; if(c==0)break;}
            else if(z+1<sizeof field)field[z++]=c;
        }
        if(row.u.a->n>0)ap(rows.u.a,row);
        if(!e)break; line=e+1;
    }
    xfree(src); return rows;
}
static Value ai_dataset_split(VM*vm,int n,Value*a){
    (void)vm; if(n!=2||a[0].t!=VARR||!isnum(a[1]))return vn();
    double ratio=dn(a[1]); if(ratio<0||ratio>1)return vn(); size_t cut=(size_t)floor((double)a[0].u.a->n*ratio);
    Value tr=va(),te=va(); for(size_t i=0;i<a[0].u.a->n;i++) ap((i<cut?tr:te).u.a,a[0].u.a->v[i]);
    Value out=va();ap(out.u.a,tr);ap(out.u.a,te);return out;
}
static Value ai_mlp_save(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"ai.mlp_save");
    if(n!=2||!hkind(a[0],HK_MLP)||a[1].t!=VSTR||!safe_path_arg(a[1].u.s))return vb(0);
    HMLP*m=a[0].u.handle;FILE*f=fopen(a[1].u.s,"wb");if(!f)return vb(0);
    uint32_t magic=0x48524D31u; int ok=1; ok&=fwrite(&magic,sizeof magic,1,f)==1; ok&=fwrite(&m->in,sizeof(int),1,f)==1;ok&=fwrite(&m->hidden,sizeof(int),1,f)==1;ok&=fwrite(&m->out,sizeof(int),1,f)==1;
    ok&=fwrite(m->w1,sizeof(double),(size_t)m->in*m->hidden,f)==(size_t)m->in*m->hidden;ok&=fwrite(m->b1,sizeof(double),(size_t)m->hidden,f)==(size_t)m->hidden;
    ok&=fwrite(m->w2,sizeof(double),(size_t)m->hidden*m->out,f)==(size_t)m->hidden*m->out;ok&=fwrite(m->b2,sizeof(double),(size_t)m->out,f)==(size_t)m->out;fclose(f);return vb(ok);
}
static Value ai_mlp_load(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"ai.mlp_load");
    if(n!=1||a[0].t!=VSTR||!safe_path_arg(a[0].u.s))return vn();FILE*f=fopen(a[0].u.s,"rb");if(!f)return vn();uint32_t magic=0;int in=0,hid=0,out=0;
    if(fread(&magic,sizeof magic,1,f)!=1||magic!=0x48524D31u||fread(&in,sizeof(int),1,f)!=1||fread(&hid,sizeof(int),1,f)!=1||fread(&out,sizeof(int),1,f)!=1||in<1||in>128||hid<1||hid>128||out<1||out>64){fclose(f);return vn();}
    HMLP*m=(HMLP*)xmalloc_dtor(sizeof(*m),hmlp_dtor);memset(m,0,sizeof(*m));m->kind=HK_MLP;m->in=in;m->hidden=hid;m->out=out;m->w1=xmalloc((size_t)in*hid*sizeof(double));m->b1=xmalloc((size_t)hid*sizeof(double));m->w2=xmalloc((size_t)hid*out*sizeof(double));m->b2=xmalloc((size_t)out*sizeof(double));
    size_t ok=fread(m->w1,sizeof(double),(size_t)in*hid,f)==(size_t)in*hid;ok&=fread(m->b1,sizeof(double),(size_t)hid,f)==(size_t)hid;ok&=fread(m->w2,sizeof(double),(size_t)hid*out,f)==(size_t)hid*out;ok&=fread(m->b2,sizeof(double),(size_t)out,f)==(size_t)out;fclose(f);if(!ok){xfree(m->w1);m->w1=NULL;xfree(m->b1);m->b1=NULL;xfree(m->w2);m->w2=NULL;xfree(m->b2);m->b2=NULL;xfree(m);return vn();}return (Value){.t=VHANDLE,.u.handle=m};
}
static Value ai_mlp_train_report(VM*vm,int n,Value*a){
    if(n!=5){return vn();}
    int ok=ai_mlp_train(vm,n,a).u.b; if(!ok)return vn();
    Value o=vsobj();stput(o.u.st,"trained",vb(1));stput(o.u.st,"epochs",a[4]);stput(o.u.st,"learning_rate",a[3]);stput(o.u.st,"samples",vi(a[1].t==VARR?(long long)a[1].u.a->n:0));return o;
}

/* Defensive XSS analysis. It detects common dangerous sinks/patterns and
   provides a safe HTML encoder. It never emits an executable payload. */
static int sec_hexbyte(char a,char b){int x=(a>='0'&&a<='9')?a-'0':(a>='a'&&a<='f'?a-'a'+10:(a>='A'&&a<='F'?a-'A'+10:-1));int y=(b>='0'&&b<='9')?b-'0':(b>='a'&&b<='f'?b-'a'+10:(b>='A'&&b<='F'?b-'A'+10:-1));return(x<0||y<0)?-1:(x<<4)|y;}
static int sec_entity_decode(const char*p,size_t n,char*out){struct E{const char*n;char c;}e[]={{"amp;",'&'},{"lt;",'<'},{"gt;",'>'},{"quot;",'"'},{"apos;",'\''},{"colon;",':'}};for(size_t i=0;i<sizeof(e)/sizeof(e[0]);i++)if(n==strlen(e[i].n)&&!strncasecmp(p,e[i].n,n)){*out=e[i].c;return 1;}if(n>3&&p[0]=='#'){unsigned long v=0;int hex=0;size_t i=1;if(i<n&&(p[i]=='x'||p[i]=='X')){hex=1;i++;}for(;i<n;i++){int d=hex?(isdigit((unsigned char)p[i])?p[i]-'0':(tolower((unsigned char)p[i])>='a'&&tolower((unsigned char)p[i])<='f'?tolower((unsigned char)p[i])-'a'+10:-1)):(isdigit((unsigned char)p[i])?p[i]-'0':-1);if(d<0||v>255)return 0;v=v*(hex?16:10)+(unsigned)d;}if(v<=255){*out=(char)v;return 1;}}return 0;}
static char* sec_normalize(const char*src){size_t cap=strlen(src)*2+32,n=0;char*d=xmalloc(cap);for(size_t i=0;src[i];){if(src[i]=='%'&&src[i+1]&&src[i+2]){int v=sec_hexbyte(src[i+1],src[i+2]);if(v>=0){d[n++]=(char)v;i+=3;continue;}}if(src[i]=='&'){const char*e=strchr(src+i,';');if(e&&(size_t)(e-(src+i))<=64){char c;size_t z=(size_t)(e-(src+i));if(sec_entity_decode(src+i+1,z,&c)){d[n++]=c;i+=z+1;continue;}}}if(n+2>=cap){cap*=2;d=xrealloc(d,cap);}d[n++]=(char)tolower((unsigned char)src[i++]);}size_t o=0;for(size_t i=0;i<n;i++)if(!isspace((unsigned char)d[i]))d[o++]=d[i];d[o]=0;return d;}
static int sec_re(const char*pat,const char*text){regex_t r;if(regcomp(&r,pat,REG_EXTENDED|REG_ICASE|REG_NOSUB)!=0)return 0;int ok=regexec(&r,text,0,NULL,0)==0;regfree(&r);return ok;}
static Value security_xss_scan(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();char*s=sec_normalize(a[0].u.s);Value hits=va();struct Sig{const char*pat;const char*id;const char*sev;}sig[]={{"<script[[:space:]>]","script-tag","high"},{"javascript[[:space:]]*:","javascript-uri","high"},{"on[a-z]{3,}[[:space:]]*=","inline-event","high"},{"<iframe[[:space:]>]","iframe","medium"},{"<object[[:space:]>]","object","medium"},{"document[[:space:]]*[.][[:space:]]*cookie","cookie-access","high"},{"innerhtml[[:space:]]*=","html-sink","medium"},{"eval[[:space:]]*[(]","dynamic-eval","high"}};for(size_t i=0;i<sizeof(sig)/sizeof(sig[0]);i++)if(sec_re(sig[i].pat,s)){Value o=vsobj();stput(o.u.st,"id",vs(sig[i].id));stput(o.u.st,"severity",vs(sig[i].sev));stput(o.u.st,"pattern",vs(sig[i].pat));stput(o.u.st,"confidence",vs("pattern"));ap(hits.u.a,o);}xfree(s);Value r=vsobj();stput(r.u.st,"findings",hits);stput(r.u.st,"count",vi((long long)hits.u.a->n));stput(r.u.st,"normalized",vb(1));stput(r.u.st,"recommendation",vs("Pattern-based defense only: use context-aware output encoding, CSP, and a real HTML sanitizer."));return r;}
static Value security_html_escape(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();const char*s=a[0].u.s;size_t cap=strlen(s)*6+1,pos=0;char*out=xmalloc(cap);for(const char*p=s;*p;p++){const char*r=NULL;switch(*p){case '&':r="&amp;";break;case '<':r="&lt;";break;case '>':r="&gt;";break;case '\"':r="&quot;";break;case '\'':r="&#39;";break;default:if(pos+2<cap)out[pos++]=*p;continue;}size_t z=strlen(r);memcpy(out+pos,r,z);pos+=z;}out[pos]=0;Value v=vs(out);xfree(out);return v;}
static Value security_sqlite_schema(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_SQL))return cap_error(vm,CAP_SQL,"sql.schema"); if(n!=1||a[0].t!=VHANDLE)return vn();
    Value args[2]={a[0],vs("SELECT name, type, sql FROM sqlite_master WHERE type IN ('table','index','view','trigger') ORDER BY type,name")}; return nsqlquery(vm,2,args);
}
static Value security_sqlite_tables(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_SQL))return cap_error(vm,CAP_SQL,"sql.tables"); if(n!=1||a[0].t!=VHANDLE)return vn();
    Value args[2]={a[0],vs("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")}; return nsqlquery(vm,2,args);
}
static Value sql_begin(VM*vm,int n,Value*a){Value x[2];if(n!=1||a[0].t!=VHANDLE)return vb(0);x[0]=a[0];x[1]=vs("BEGIN");return nsqlexec(vm,2,x);}
static Value sql_commit(VM*vm,int n,Value*a){Value x[2];if(n!=1||a[0].t!=VHANDLE)return vb(0);x[0]=a[0];x[1]=vs("COMMIT");return nsqlexec(vm,2,x);}
static Value sql_rollback(VM*vm,int n,Value*a){Value x[2];if(n!=1||a[0].t!=VHANDLE)return vb(0);x[0]=a[0];x[1]=vs("ROLLBACK");return nsqlexec(vm,2,x);}
static Value sql_exec_bind(VM*vm,int n,Value*a){return nsqlbind(vm,n,a);}
static Value sql_quote_identifier(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();const char*s=a[0].u.s;size_t cap=strlen(s)*2+3,pos=0;char*out=xmalloc(cap);out[pos++]='\"';for(const char*p=s;*p;p++){if(*p=='\"')out[pos++]='\"';out[pos++]=*p;}out[pos++]='\"';out[pos]=0;Value r=vs(out);xfree(out);return r;}

/* Defensive network diagnostics, not a port scanner. */
static Value net_connect_check(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.connect_check"); if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VINT)return vn();int port=(int)a[1].u.i;if(port<1||port>65535)return vb(0);int timeout=n==3&&a[2].t==VINT?(int)a[2].u.i:1500;if(timeout<50||timeout>10000)timeout=1500;
    char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo hints={0},*res=0;hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;int rc=getaddrinfo(a[0].u.s,ps,&hints,&res);if(rc)return vb(0);int ok=0;
    for(struct addrinfo*p=res;p&&!ok;p=p->ai_next){int fd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(fd<0)continue;
#ifdef _WIN32
        u_long nb=1;ioctlsocket(fd,FIONBIO,&nb);connect(fd,p->ai_addr,(int)p->ai_addrlen);fd_set wf;FD_ZERO(&wf);FD_SET(fd,&wf);struct timeval tv={(long)(timeout/1000),(long)((timeout%1000)*1000)};int sr=select(0,NULL,&wf,NULL,NULL,&tv);if(sr>0){int err=0;int el=sizeof(err);getsockopt(fd,SOL_SOCKET,SO_ERROR,(char*)&err,&el);ok=(err==0);}closesocket(fd);
#else
        int fl=fcntl(fd,F_GETFL,0);fcntl(fd,F_SETFL,fl|O_NONBLOCK);connect(fd,p->ai_addr,p->ai_addrlen);fd_set wf;FD_ZERO(&wf);FD_SET(fd,&wf);struct timeval tv={(long)(timeout/1000),(long)((timeout%1000)*1000)};int sr=select(fd+1,NULL,&wf,NULL,&tv);if(sr>0){int err=0;socklen_t el=sizeof(err);getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&el);ok=(err==0);}close(fd);
#endif
    }freeaddrinfo(res);return vb(ok);
}
static Value net_parse_host(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();const char*u=a[0].u.s;const char*p=strstr(u,"://");p=p?p+3:u;const char*e=strpbrk(p,"/?#:");size_t z=e?(size_t)(e-p):strlen(p);if(z==0||z>255)return vn();char h[256];memcpy(h,p,z);h[z]=0;return vs(h);}
static Value net_status(VM*vm,int n,Value*a){(void)vm;if(n!=0)return vn();Value o=vsobj();stput(o.u.st,"dns",vb(1));stput(o.u.st,"http",vb(1));stput(o.u.st,"tcp_diagnostics",vb(1));stput(o.u.st,"port_scanning",vb(1));return o;}

/* Same non-blocking connect+select technique as net.connect_check, factored
   out so net.port_scan can probe many ports without repeating the socket code. */
static int tcp_connect_probe(const char*host,int port,int timeout){
    char ps[16];snprintf(ps,sizeof ps,"%d",port);struct addrinfo hints={0},*res=0;hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;
    if(getaddrinfo(host,ps,&hints,&res))return 0;
    int ok=0;
    for(struct addrinfo*p=res;p&&!ok;p=p->ai_next){
        int fd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(fd<0)continue;
#ifdef _WIN32
        u_long nb=1;ioctlsocket(fd,FIONBIO,&nb);connect(fd,p->ai_addr,(int)p->ai_addrlen);fd_set wf;FD_ZERO(&wf);FD_SET(fd,&wf);struct timeval tv={(long)(timeout/1000),(long)((timeout%1000)*1000)};int sr=select(0,NULL,&wf,NULL,&tv);if(sr>0){int err=0;int el=sizeof(err);getsockopt(fd,SOL_SOCKET,SO_ERROR,(char*)&err,&el);ok=(err==0);}closesocket(fd);
#else
        int fl=fcntl(fd,F_GETFL,0);fcntl(fd,F_SETFL,fl|O_NONBLOCK);connect(fd,p->ai_addr,p->ai_addrlen);fd_set wf;FD_ZERO(&wf);FD_SET(fd,&wf);struct timeval tv={(long)(timeout/1000),(long)((timeout%1000)*1000)};int sr=select(fd+1,NULL,&wf,NULL,&tv);if(sr>0){int err=0;socklen_t el=sizeof(err);getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&el);ok=(err==0);}close(fd);
#endif
    }
    freeaddrinfo(res);return ok;
}
/* Ranged TCP connect-scan, for testing hosts/networks the caller is
   authorized to assess. Bounded to <=1024 ports per call and a sane
   per-port timeout so it can't be turned into an unbounded sweep. */
static Value net_port_scan(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.port_scan");
    if(n<3||n>4||a[0].t!=VSTR||a[1].t!=VINT||a[2].t!=VINT)return vn();
    int start=(int)a[1].u.i,end=(int)a[2].u.i;
    if(start<1||start>65535||end<1||end>65535||end<start)return vn();
    if(end-start+1>1024)return vn();
    int timeout=n==4&&a[3].t==VINT?(int)a[3].u.i:300;
    if(timeout<50||timeout>5000)timeout=300;
    Value out=va();
    for(int port=start;port<=end;port++)
        if(tcp_connect_probe(a[0].u.s,port,timeout))ap(out.u.a,vi(port));
    return out;
}
/* Connects and reads whatever the service sends first (SSH/FTP/SMTP/etc
   greet with a banner unprompted; HTTP-like services typically won't until
   you send a request, so this returns null for those). Classic first step
   of service-version recon. */
static Value net_banner_grab(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.banner_grab");
    if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VINT)return vn();
    int port=(int)a[1].u.i;if(port<1||port>65535)return vn();
    int timeout=(n==3&&a[2].t==VINT)?(int)a[2].u.i:1500;
    if(timeout<50||timeout>10000)timeout=1500;
    char ps[16];snprintf(ps,sizeof ps,"%d",port);
    struct addrinfo hints={0},*res=0;hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;
    if(getaddrinfo(a[0].u.s,ps,&hints,&res))return vn();
    Value out=vn();
    for(struct addrinfo*p=res;p&&out.t!=VSTR;p=p->ai_next){
        int fd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(fd<0)continue;
        int connected=0;
#ifdef _WIN32
        u_long nb=1;ioctlsocket(fd,FIONBIO,&nb);connect(fd,p->ai_addr,(int)p->ai_addrlen);fd_set wf;FD_ZERO(&wf);FD_SET(fd,&wf);struct timeval tv={(long)(timeout/1000),(long)((timeout%1000)*1000)};int sr=select(0,NULL,&wf,NULL,&tv);if(sr>0){int err=0;int el=sizeof(err);getsockopt(fd,SOL_SOCKET,SO_ERROR,(char*)&err,&el);connected=(err==0);}
        if(connected){struct timeval rtv={(long)(timeout/1000),(long)((timeout%1000)*1000)};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,(char*)&rtv,sizeof rtv);char buf[512];int rn=recv(fd,buf,sizeof buf-1,0);if(rn>0){buf[rn]=0;out=vs(buf);}}
        closesocket(fd);
#else
        int fl=fcntl(fd,F_GETFL,0);fcntl(fd,F_SETFL,fl|O_NONBLOCK);connect(fd,p->ai_addr,p->ai_addrlen);fd_set wf;FD_ZERO(&wf);FD_SET(fd,&wf);struct timeval tv={(long)(timeout/1000),(long)((timeout%1000)*1000)};int sr=select(fd+1,NULL,&wf,NULL,&tv);if(sr>0){int err=0;socklen_t el=sizeof(err);getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&el);connected=(err==0);}
        if(connected){struct timeval rtv={(long)(timeout/1000),(long)((timeout%1000)*1000)};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&rtv,sizeof rtv);char buf[512];int rn=recv(fd,buf,sizeof buf-1,0);if(rn>0){buf[rn]=0;out=vs(buf);}}
        close(fd);
#endif
    }
    freeaddrinfo(res);
    return out;
}
/* Heuristic weight per port: services that commonly indicate exposure risk
   (legacy plaintext protocols, exposed databases, remote admin) score higher. */
static double port_risk_weight(int port){
    switch(port){
        case 21: return 2.0;
        case 23: return 3.0;
        case 25: return 1.0;
        case 135: case 139: case 445: return 2.5;
        case 1433: case 3306: case 5432: case 6379: case 27017: return 2.0;
        case 3389: return 2.5;
        case 5900: return 2.0;
        case 22: case 80: case 443: return 0.3;
        default: return 0.7;
    }
}
/* AI-assisted triage over a list of open ports (as returned by
   net.port_scan): sigmoid-squashes a weighted sum into a 0..1 risk score.
   A starting signal for a human analyst, not a verdict. */
static Value security_port_risk_score(VM*vm,int n,Value*a){
    (void)vm;if(n!=1||a[0].t!=VARR)return vn();
    double sum=0;
    for(size_t i=0;i<a[0].u.a->n;i++){
        if(a[0].u.a->v[i].t!=VINT)return vn();
        int port=(int)a[0].u.a->v[i].u.i;if(port<1||port>65535)return vn();
        sum+=port_risk_weight(port);
    }
    double score=1.0/(1.0+exp(-(sum-2.0)));
    Value o=vsobj();
    stput(o.u.st,"score",vf(score));
    stput(o.u.st,"ports_seen",vi((long long)a[0].u.a->n));
    return o;
}

static Value ngeneric_info(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VFN||!a[0].u.fn)return vn();Fn*f=a[0].u.fn;Value o=vsobj();stput(o.u.st,"name",vs(f->name));stput(o.u.st,"generic_count",vi(f->ngens));Value g=va();for(int i=0;i<f->ngens;i++)ap(g.u.a,vs(f->gens[i]));stput(o.u.st,"parameters",g);return o;}
static void bindn(Env*e,const char*k,Native f){en(e,k,(Value){.t=VNATIVE,.u.native=f});}
static int apply_cpu_affinity(VM*vm);
static Value nnuc_set_profile(VM*vm,int n,Value*a){if(n!=1||a[0].t!=VSTR)return vb(0);vm->profile=xdup(a[0].u.s);return vb(1);}
static Value nnuc_profile(VM*vm,int n,Value*a){(void)a;if(n!=0)return vn();Value o=va();ap(o.u.a,vs(vm->profile?vm->profile:"default"));ap(o.u.a,vs(vm->memory_auto?"auto":"manual"));ap(o.u.a,vi((long long)g_mem_used));ap(o.u.a,vi((long long)g_mem_peak));ap(o.u.a,vi(vm->cpu_limit_ms));ap(o.u.a,vi((long long)(vm->mem_limit_bytes/(1024*1024))));ap(o.u.a,vi(vm->cpu_cores));ap(o.u.a,vi(vm->cpu_percent));return o;}

