#define H_SOURCE_MAX (32ULL*1024ULL*1024ULL)
static long long int_binop(VM*vm,Op op,long long a,long long b){
    long long out=0;
/* Native integer overflow is fatal outside sandbox.  Sandboxed code gets a
   deterministic 0 sentinel plus math.overflowed()==true; this is intentional so
   untrusted workers cannot terminate the host process.  Call
   math.clear_overflow() after handling the condition. */
#define INT_OVERFLOW(MSG) do{if(vm&&vm->module_sandboxed){vm->int_overflow=1;snprintf(vm->err_msg,sizeof vm->err_msg,"%s",MSG);return 0;}die("%s",MSG);return 0;}while(0)
    if(op==I_ADD){if((b>0&&a>LLONG_MAX-b)||(b<0&&a<LLONG_MIN-b))INT_OVERFLOW("Haris runtime: integer overflow");out=a+b;}
    else if(op==I_SUB){if((b<0&&a>LLONG_MAX+b)||(b>0&&a<LLONG_MIN+b))INT_OVERFLOW("Haris runtime: integer overflow");out=a-b;}
    else if(op==I_MUL){
#if defined(__SIZEOF_INT128__)
        __int128 z=(__int128)a*(__int128)b;if(z>LLONG_MAX||z<LLONG_MIN)INT_OVERFLOW("Haris runtime: integer overflow");out=(long long)z;
#else
        if(a==0||b==0)out=0;else if(a==-1&&b==LLONG_MIN)INT_OVERFLOW("Haris runtime: integer overflow");else if(b==-1&&a==LLONG_MIN)INT_OVERFLOW("Haris runtime: integer overflow");else if(a>0){if(b>0){if(a>LLONG_MAX/b)INT_OVERFLOW("Haris runtime: integer overflow");}else if(b<LLONG_MIN/a)INT_OVERFLOW("Haris runtime: integer overflow");}else{if(b>0){if(a<LLONG_MIN/b)INT_OVERFLOW("Haris runtime: integer overflow");}else if(a<LLONG_MAX/b)INT_OVERFLOW("Haris runtime: integer overflow");}out=a*b;
#endif
    } else if(op==I_DIV){if(b==0)die("division by zero");if(a==LLONG_MIN&&b==-1)INT_OVERFLOW("Haris runtime: integer overflow");out=a/b;}
    else if(op==I_MOD){if(b==0)die("division by zero");if(a==LLONG_MIN&&b==-1)return 0;out=a%b;}
    else die("invalid integer operator");
#undef INT_OVERFLOW
    return out;
}
static Value nmath_overflowed(VM*vm,int n,Value*a){(void)a;return n==0?vb(vm&&vm->int_overflow):vn();}
static Value nmath_clear_overflow(VM*vm,int n,Value*a){(void)a;if(n!=0)return vn();if(vm)vm->int_overflow=0;return vb(1);}

#define H_STACK_MAX  8192
#define H_CALL_MAX   256
static char*readf_limit(const char*path,size_t maxn){FILE*f=fopen(path,"rb");if(!f)return 0;if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}long ln=ftell(f);if(ln<0||(unsigned long long)ln>maxn){fclose(f);return 0;}if(fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}size_t n=(size_t)ln;char*b=xmalloc(n+1);size_t got=fread(b,1,n,f);fclose(f);if(got!=n){xfree(b);return 0;}b[n]=0;return b;}
static char*readf(const char*path){return readf_limit(path,H_SOURCE_MAX);}
enum {
  CAP_OS=1u<<0, CAP_SYSTEM=1u<<1, CAP_NET=1u<<2, CAP_WEB=1u<<3,
  CAP_SQL=1u<<4, CAP_CLOUD=1u<<5, CAP_GIT=1u<<6, CAP_FS=1u<<7,
  CAP_NUCLEAR=1u<<8,
  CAP_DEFENSE=1u<<9, CAP_AUDIT=1u<<10, CAP_REDTEAM=1u<<11, CAP_PENTEST=1u<<12,
  CAP_COMPLIANCE=1u<<13, CAP_REPORT=1u<<14, CAP_HW_CPU=1u<<15, CAP_HW_GPU=1u<<16, CAP_HW_BOARD=1u<<17, CAP_GAME_ADMIN=1u<<18
};
#define CAP_SAFE_DEFAULT (CAP_DEFENSE)
#define CAP_HW_ANY (CAP_HW_CPU|CAP_HW_GPU|CAP_HW_BOARD)
static const char*cap_name(unsigned cap){
  if(cap==CAP_OS)return "os"; if(cap==CAP_SYSTEM)return "system"; if(cap==CAP_NET)return "net"; if(cap==CAP_WEB)return "web";
  if(cap==CAP_SQL)return "sql"; if(cap==CAP_CLOUD)return "cloud"; if(cap==CAP_GIT)return "git"; if(cap==CAP_FS)return "fs"; if(cap==CAP_NUCLEAR)return "nuclear";
  if(cap==CAP_DEFENSE)return "defense"; if(cap==CAP_AUDIT)return "audit"; if(cap==CAP_REDTEAM)return "redteam"; if(cap==CAP_PENTEST)return "pentest";
  if(cap==CAP_COMPLIANCE)return "compliance"; if(cap==CAP_REPORT)return "report"; if(cap==CAP_HW_CPU)return "hw.cpu"; if(cap==CAP_HW_GPU)return "hw.gpu"; if(cap==CAP_HW_BOARD)return "hw.board"; if(cap==CAP_GAME_ADMIN)return "game.admin";
  return "unknown";
}
static int cap_allowed(VM*vm,unsigned cap){return !vm->module_sandboxed || (vm->module_caps & cap)!=0;}
static Value cap_error(VM*vm,unsigned cap,const char*name){(void)vm;fprintf(stderr,"Haris security: module capability '%s' required for %s\n",cap_name(cap),name);return vn();}
static unsigned requested_caps(const char*src){
  unsigned c=0; const char*p=src; int lines=0;
  while(*p && lines++<64){const char*e=strchr(p,'\n');size_t n=e?(size_t)(e-p):strlen(p);if(n>=15 && strstr(p,"@capabilities:") && strstr(p,"@capabilities:")<p+n){const char*q=strstr(p,"@capabilities:")+14;while(q<p+n){while(q<p+n&&(isspace((unsigned char)*q)||*q==','))q++;const char*b=q;while(q<p+n&&!isspace((unsigned char)*q)&&*q!=',')q++;size_t L=(size_t)(q-b);if(L==2&&!strncmp(b,"os",2))c|=CAP_OS;else if(L==6&&!strncmp(b,"system",6))c|=CAP_SYSTEM;else if(L==3&&!strncmp(b,"net",3))c|=CAP_NET;else if(L==3&&!strncmp(b,"web",3))c|=CAP_WEB;else if(L==3&&!strncmp(b,"sql",3))c|=CAP_SQL;else if(L==5&&!strncmp(b,"cloud",5))c|=CAP_CLOUD;else if(L==3&&!strncmp(b,"git",3))c|=CAP_GIT;else if(L==2&&!strncmp(b,"fs",2))c|=CAP_FS;else if(L==7&&!strncmp(b,"nuclear",7))c|=CAP_NUCLEAR;else if(L==7&&!strncmp(b,"defense",7))c|=CAP_DEFENSE;else if(L==5&&!strncmp(b,"audit",5))c|=CAP_DEFENSE;else if(L==7&&!strncmp(b,"redteam",7))c|=CAP_REDTEAM;else if(L==7&&!strncmp(b,"pentest",7))c|=CAP_PENTEST;else if(L==10&&!strncmp(b,"compliance",10))c|=CAP_DEFENSE;else if(L==6&&!strncmp(b,"report",6))c|=CAP_DEFENSE;else if(L==6&&!strncmp(b,"hw.cpu",6))c|=CAP_HW_CPU;else if(L==6&&!strncmp(b,"hw.gpu",6))c|=CAP_HW_GPU;else if(L==8&&!strncmp(b,"hw.board",8))c|=CAP_HW_BOARD;else if(L==10&&!strncmp(b,"game.admin",10))c|=CAP_GAME_ADMIN;}}
    if(!e) break;
    p=e+1;
  }return c;
}
static char* sec_normalize(const char*src);
static int module_scan(const char*src,unsigned caps,char*reason,size_t rs){
  struct Rule{const char*pat;unsigned cap;const char*name;};
  static const struct Rule r[]={
    {"system.run",CAP_SYSTEM,"system.run"},{"nuclear.",CAP_NUCLEAR,"nuclear API"},{"cloud.",CAP_CLOUD,"cloud"},{"api.",CAP_WEB,"api/web"},{"git.",CAP_GIT,"git"},{"sql.",CAP_SQL,"sql"},{"net.",CAP_NET,"net"},{"udp.",CAP_NET,"net"},{"ws.",CAP_NET,"net"},{"web.",CAP_WEB,"web"},{"os.",CAP_OS,"os"},
    {"defense.",CAP_DEFENSE,"defense.*"},{"audit.",CAP_DEFENSE,"audit.*"},{"redteam.",CAP_REDTEAM,"redteam.*"},{"pentest.",CAP_PENTEST,"pentest.*"},{"compliance.",CAP_DEFENSE,"compliance.*"},{"report.",CAP_DEFENSE,"report.*"},
    {"cpu.affinity",CAP_HW_CPU,"hw.cpu"},{"gpu.",CAP_HW_GPU,"hw.gpu"},{"board.",CAP_HW_BOARD,"hw.board"},{"game.server.cmd",CAP_GAME_ADMIN,"game.admin"}
  };
  char*norm=sec_normalize(src?src:"");
  for(size_t i=0;i<sizeof(r)/sizeof(r[0]);i++){
    if(strstr(norm,r[i].pat) && !(caps&r[i].cap)){
      snprintf(reason,rs,"requires capability '%s'",r[i].name);
      xfree(norm);
      return 0;
    }
  }
  xfree(norm);
  return 1;
}


static int size_mul_ok(size_t a,size_t b,size_t*out){if(a&&b>SIZE_MAX/a)return 0;*out=a*b;return 1;}
static size_t next_capacity(size_t cap,size_t need){size_t n=cap?cap:8;while(n<need){if(n>SIZE_MAX/2)return 0;n*=2;}return n;}
static int arr_reserve(Arr*a,size_t need){if(need<=a->cap)return 1;size_t nc=next_capacity(a->cap,need);if(!nc||nc>SIZE_MAX/sizeof(Value))return 0;a->v=xrealloc(a->v,nc*sizeof(Value));a->cap=nc;return 1;}

static Value nappend(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR)return vn();ap(a[0].u.a,a[1]);return a[0];}
static Value nlist_set(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VARR||a[1].t!=VINT)return vb(0);long long k=a[1].u.i;if(k<0||(size_t)k>=a[0].u.a->n)return vb(0);a[0].u.a->v[k]=a[2];return a[2];}
static Value npop(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR||a[0].u.a->n==0)return vn();return a[0].u.a->v[--a[0].u.a->n];}
static Value ninsert(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VARR||a[1].t!=VINT)return vb(0);long long k=a[1].u.i;if(k<0)k=0;if(k>(long long)a[0].u.a->n)k=(long long)a[0].u.a->n;if(!arr_reserve(a[0].u.a,a[0].u.a->n+1))return vb(0);for(size_t i=a[0].u.a->n;i>(size_t)k;i--)a[0].u.a->v[i]=a[0].u.a->v[i-1];a[0].u.a->v[k]=a[2];a[0].u.a->n++;return vb(1);}
static Value nsubstr(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VSTR||a[1].t!=VINT||a[2].t!=VINT)return vn();long long st=a[1].u.i,len=a[2].u.i,sl=(long long)strlen(a[0].u.s);if(st<0)st=0;if(st>sl)st=sl;if(len<0)len=0;if(len>sl-st)len=sl-st;char*out=xmalloc((size_t)len+1);memcpy(out,a[0].u.s+st,(size_t)len);out[len]=0;Value v=vs(out);xfree(out);return v;}
static Value nreplace(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VSTR||a[1].t!=VSTR||a[2].t!=VSTR)return vn();const char*s=a[0].u.s,*old=a[1].u.s,*rep=a[2].u.s;if(!*old)return vs(s);size_t count=0,oldn=strlen(old),repn=strlen(rep);for(const char*p=s;(p=strstr(p,old));p+=oldn)count++;size_t base_len=strlen(s);size_t total;if(repn>=oldn){size_t add=repn-oldn;if(count && add>SIZE_MAX/count)die("Haris runtime: string replace too large");size_t extra=count*add;if(extra>SIZE_MAX-base_len-1)die("Haris runtime: string replace too large");total=base_len+extra;}else{size_t sub=oldn-repn;if(count>base_len/sub)die("Haris runtime: string replace underflow");total=base_len-count*sub;}char*out=xmalloc(total+1);size_t pos=0;const char*p=s;while(1){const char*q=strstr(p,old);if(!q)break;size_t z=(size_t)(q-p);memcpy(out+pos,p,z);pos+=z;memcpy(out+pos,rep,repn);pos+=repn;p=q+oldn;}strcpy(out+pos,p);Value v=vs(out);xfree(out);return v;}
static Value nupper(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();size_t z=strlen(a[0].u.s);char*out=xmalloc(z+1);for(size_t i=0;i<z;i++)out[i]=(char)toupper((unsigned char)a[0].u.s[i]);out[z]=0;Value v=vs(out);xfree(out);return v;}
static Value nlower(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();size_t z=strlen(a[0].u.s);char*out=xmalloc(z+1);for(size_t i=0;i<z;i++)out[i]=(char)tolower((unsigned char)a[0].u.s[i]);out[z]=0;Value v=vs(out);xfree(out);return v;}
static Value nabs(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0]))return vn();if(a[0].t==VINT){if(a[0].u.i==LLONG_MIN)return vn();return vi(a[0].u.i<0?-a[0].u.i:a[0].u.i);}return vf(fabs(a[0].u.f));}
/* Fast per-VM PRNG for non-secret randomness. Security-sensitive identifiers
   (conn_id, tokens, keys) MUST continue to use OpenSSL RAND_bytes().
   xorshift64* is deliberately chosen for low overhead in hot AI/data paths. */
static uint64_t hr_rng_next(VM*vm){
    uint64_t x=vm?vm->rng_state:0x9E3779B97F4A7C15ULL;
    if(!x)x=0x9E3779B97F4A7C15ULL;
    x^=x>>12; x^=x<<25; x^=x>>27;
    uint64_t r=x*2685821657736338717ULL;
    if(vm)vm->rng_state=x;
    return r;
}
static void hr_rng_seed(VM*vm,uint64_t seed){
    if(!vm)return;
    if(!seed)seed=0x9E3779B97F4A7C15ULL;
    vm->rng_state=seed;
}
static double hr_rng_unit(VM*vm){return (double)(hr_rng_next(vm)>>11)*(1.0/9007199254740992.0);}
static Value hr_rand_int(VM*vm,int n,Value*a){(void)a;if(n!=0)return vn();return vi((long long)hr_rng_next(vm));}
static Value hr_rand_float(VM*vm,int n,Value*a){(void)a;if(n!=0)return vn();return vf(hr_rng_unit(vm));}
static Value hr_rand_int_range(VM*vm,int n,Value*a){
    if(n!=2||a[0].t!=VINT||a[1].t!=VINT)return vn();
    long long lo=a[0].u.i,hi=a[1].u.i;if(hi<lo){long long t=lo;lo=hi;hi=t;}
    uint64_t span=(uint64_t)hi-(uint64_t)lo+1ULL;uint64_t r=hr_rng_next(vm);
    if(!span)return vi((long long)r);
    return vi((long long)((uint64_t)lo+(r%span)));
}
static Value hr_rand_float_range(VM*vm,int n,Value*a){(void)vm;if(n!=2||!isnum(a[0])||!isnum(a[1]))return vn();double lo=dn(a[0]),hi=dn(a[1]);return vf(lo+(hi-lo)*hr_rng_unit(vm));}
static Value hr_deg_to_rad(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0]))return vn();return vf(dn(a[0])*(M_PI/180.0));}
static Value hr_rad_to_deg(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0]))return vn();return vf(dn(a[0])*(180.0/M_PI));}
static Value hr_sign(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0]))return vn();double x=dn(a[0]);int s=x>0?1:(x<0?-1:0);return a[0].t==VINT?vi(s):vf((double)s);}
static Value hr_min(VM*vm,int n,Value*a){(void)vm;if(n!=2||!isnum(a[0])||!isnum(a[1]))return vn();if(a[0].t==VINT&&a[1].t==VINT)return vi(a[0].u.i<a[1].u.i?a[0].u.i:a[1].u.i);double x=dn(a[0]),y=dn(a[1]);return vf(x<y?x:y);}
static Value hr_max(VM*vm,int n,Value*a){(void)vm;if(n!=2||!isnum(a[0])||!isnum(a[1]))return vn();if(a[0].t==VINT&&a[1].t==VINT)return vi(a[0].u.i>a[1].u.i?a[0].u.i:a[1].u.i);double x=dn(a[0]),y=dn(a[1]);return vf(x>y?x:y);}
static Value nsqrt(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0])||dn(a[0])<0)return vn();return vf(sqrt(dn(a[0])));}
static Value npow(VM*vm,int n,Value*a){(void)vm;if(n!=2||!isnum(a[0])||!isnum(a[1]))return vn();return vf(pow(dn(a[0]),dn(a[1])));}
static Value nfloor(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0]))return vn();return vf(floor(dn(a[0])));}
static Value nceil(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0]))return vn();return vf(ceil(dn(a[0])));}
static Value nsplit(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vn();Value out=va();char*copy=xdup(a[0].u.s);char*save=0;for(char*q=strtok_r(copy,a[1].u.s,&save);q;q=strtok_r(NULL,a[1].u.s,&save))ap(out.u.a,vs(q));xfree(copy);return out;}
static Value nassert(VM*vm,int n,Value*a){(void)vm;if(n<1)return vn();if(!truth(a[0]))die("Haris assertion failed");return n>1?a[1]:vn();}
static const char *value_error_text(Value v,char *buf,size_t cap){
    if(v.t==VSTR){snprintf(buf,cap,"%s",v.u.s?v.u.s:"");return buf;}
    if(v.t==VINT){snprintf(buf,cap,"%lld",v.u.i);return buf;}
    if(v.t==VBOOL){snprintf(buf,cap,"%s",v.u.b?"true":"false");return buf;}
    if(v.t==VNULL){snprintf(buf,cap,"null");return buf;}
    snprintf(buf,cap,"<%s>",type_name(v)); return buf;
}
static Value nerror(VM*vm,int n,Value*a){
    (void)vm; char msg[512]={0};
    if(n>=2){char code[128]={0},text[384]={0}; value_error_text(a[0],code,sizeof code); value_error_text(a[1],text,sizeof text); die("Haris error [%s]: %s",code,text);}
    if(n>=1){value_error_text(a[0],msg,sizeof msg); die("Haris error: %s",msg);} 
    die("Haris error"); return vn();
}
static Value nthrow(VM*vm,int n,Value*a){
    (void)vm; char msg[512]={0};
    if(n>=1) value_error_text(a[0],msg,sizeof msg); else snprintf(msg,sizeof msg,"throw");
    die("Haris throw: %s",msg); return vn();
}
static Value nprint(VM*vm,int n,Value*a){(void)vm;
    for(int i=0;i<n;i++){if(i)putchar(' ');pv(a[i]);}putchar('\n');
#ifdef HARIS_ANDROID
    __android_log_print(ANDROID_LOG_INFO,"Haris","print emitted (%d args)",n);
#endif
    return vn();
}
static Value nlen(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();if(a[0].t==VSTR)return vi((long long)strlen(a[0].u.s));if(a[0].t==VARR)return vi((long long)a[0].u.a->n);return vn();}
static int safe_path_arg(const char*s){if(!s||!*s)return 0;for(const unsigned char*p=(const unsigned char*)s;*p;p++){if(*p=='\"'||*p=='\''||*p==';'||*p=='&'||*p=='|'||*p=='<'||*p=='>'||*p=='$'||*p=='`'||*p=='\n'||*p=='\r'||*p=='%')return 0;}return 1;}

static int path_real_abs(const char *path,char *out,size_t cap){
    if(!path||!out||cap==0)return 0;
#ifdef _WIN32
    char full[PATH_MAX]={0};
    if(!_fullpath(full,path,sizeof full))return 0;
    HANDLE h=CreateFileA(full,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,
                         OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(h!=INVALID_HANDLE_VALUE){
        char finalp[PATH_MAX]={0};
        DWORD n=GetFinalPathNameByHandleA(h,finalp,(DWORD)sizeof finalp,
                                          FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
        CloseHandle(h);
        if(n>0&&n<sizeof finalp){
            const char*src=finalp;
            if(src[0]=='\\'&&src[1]=='\\'&&src[2]=='?'&&src[3]=='\\'){
                src+=4;
                if(src[0]=='U'&&src[1]=='N'&&src[2]=='C'&&src[3]=='\\'){
                    /* \\?\\UNC\\server\\share -> \\server\\share */
                    static char unc[PATH_MAX];
                    size_t sn=strlen(src+4);
                    if(sn+3>=sizeof unc)return 0;
                    unc[0]='\\';unc[1]='\\';memcpy(unc+2,src+4,sn+1);
                    src=unc;
                }
            }
            if(strlen(src)+1>cap)return 0;
            snprintf(out,cap,"%s",src);
            return 1;
        }
    }
    if(strlen(full)+1>cap)return 0;
    snprintf(out,cap,"%s",full);
    return 1;
#else
    return realpath(path,out)!=NULL;
#endif
}
static int path_is_within_root(const char *root,const char *candidate){
    if(!root||!candidate||!*root||!*candidate)return 0;
    size_t rn=strlen(root);while(rn>1&&(root[rn-1]=='/'||root[rn-1]=='\\'))rn--;
#ifdef _WIN32
    if(_strnicmp(root,candidate,rn)!=0)return 0;
#else
    if(strncmp(root,candidate,rn)!=0)return 0;
#endif
    return candidate[rn]=='\0'||candidate[rn]=='/'||candidate[rn]=='\\';
}
static int fs_path_allowed(const VM *vm,const char *path){
    if(!safe_path_arg(path))return 0;
    if(!vm||!vm->sandbox_root||!*vm->sandbox_root)return 1;
    char root[PATH_MAX],target[PATH_MAX],parent[PATH_MAX];
    if(!path_real_abs(vm->sandbox_root,root,sizeof root))return 0;
    if(path_real_abs(path,target,sizeof target))return path_is_within_root(root,target);
    if(snprintf(parent,sizeof parent,"%s",path)<0)return 0;
    char *slash=strrchr(parent,'/');
#ifdef _WIN32
    char *bslash=strrchr(parent,'\\');if(bslash&&(!slash||bslash>slash))slash=bslash;
#endif
    if(slash){if(slash==parent)slash[1]='\0';else *slash='\0';}
    else snprintf(parent,sizeof parent,".");
    if(!path_real_abs(parent,target,sizeof target))return 0;
    return path_is_within_root(root,target);
}
static int json_append_escaped(char*out,size_t cap,size_t*pos,const char*s){for(const unsigned char*p=(const unsigned char*)s;*p;p++){const char*esc=0;char one[3]={0};switch(*p){case '\"':esc="\\\"";break;case '\\':esc="\\\\";break;case '\n':esc="\\n";break;case '\r':esc="\\r";break;case '\t':esc="\\t";break;default:one[0]=(char)*p;esc=one;break;}size_t n=strlen(esc);if(*pos+n>=cap)return 0;memcpy(out+*pos,esc,n);*pos+=n;}return 1;}


static int sandbox_authorize_caps(VM*vm,unsigned requested,char*reason,size_t rs){
    if(!vm)return 0;
    if(requested&CAP_NUCLEAR){snprintf(reason,rs,"CAP_NUCLEAR/raw host memory is never granted inside a sandbox");return 0;}
    if(requested&CAP_HW_ANY){snprintf(reason,rs,"raw CPU/GPU/board capabilities are broker-only and never granted inside a sandbox");return 0;}
    unsigned missing=requested & ~vm->host_module_caps;
    if(missing){unsigned one=missing & (~missing+1u);snprintf(reason,rs,"host has not explicitly granted capability '%s'",cap_name(one));return 0;}
    if((requested&CAP_REDTEAM)&&!vm->redteam_authorized){snprintf(reason,rs,"redteam.* requires --legal-doc authorization");return 0;}
    if((requested&CAP_PENTEST)&&!vm->pentest_authorized){snprintf(reason,rs,"pentest.* requires --audit-log authorization");return 0;}
    return 1;
}
static int audit_log_event(VM*vm,const char*kind,unsigned caps){
    if(!vm||!vm->audit_log_path||!*vm->audit_log_path)return 0; FILE*f=fopen(vm->audit_log_path,"ab"); if(!f)return 0;
    time_t t=time(NULL); struct tm tmv; memset(&tmv,0,sizeof tmv);
#ifdef _WIN32
    localtime_s(&tmv,&t); unsigned long long pid=(unsigned long long)GetCurrentProcessId();
#else
    localtime_r(&t,&tmv); unsigned long long pid=(unsigned long long)getpid();
#endif
    fprintf(f,"{\"ts\":\"%04d-%02d-%02dT%02d:%02d:%02d\",\"pid\":%llu,\"kind\":\"%s\",\"caps\":%u,\"legal_doc_sha256\":\"%s\"}\n",tmv.tm_year+1900,tmv.tm_mon+1,tmv.tm_mday,tmv.tm_hour,tmv.tm_min,tmv.tm_sec,pid,kind?kind:"sandbox",caps,vm->legal_doc_hash);
    fclose(f);return 1;
}
static int authorize_redteam_document(VM*vm,const char*path){ if(!vm||!path||!*path)return 0; char h[65]; if(!sha256_file(path,h))return 0; snprintf(vm->legal_doc_hash,sizeof vm->legal_doc_hash,"%s",h); vm->redteam_authorized=1; return 1; }

