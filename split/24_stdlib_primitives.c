/* ---------------- Haris 2.5 standard-library primitives ---------------- */
static Value nstr_find(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vi(-1);char*p=strstr(a[0].u.s,a[1].u.s);return vi(p?(long long)(p-a[0].u.s):-1);}
static Value nstr_starts(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vb(0);return vb(!strncmp(a[0].u.s,a[1].u.s,strlen(a[1].u.s)));}
static Value nstr_ends(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vb(0);size_t A=strlen(a[0].u.s),B=strlen(a[1].u.s);return vb(B<=A&&!strcmp(a[0].u.s+A-B,a[1].u.s));}
static Value nstr_trim(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();const char*s=a[0].u.s;while(isspace((unsigned char)*s))s++;const char*e=a[0].u.s+strlen(a[0].u.s);while(e>s&&isspace((unsigned char)e[-1]))e--;size_t z=(size_t)(e-s);char*b=xmalloc(z+1);memcpy(b,s,z);b[z]=0;Value r=vs(b);xfree(b);return r;}
static Value nstr_char_at(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTR||a[1].t!=VINT)return vn();long long i=a[1].u.i;if(i<0||(size_t)i>=strlen(a[0].u.s))return vn();char b[2]={a[0].u.s[i],0};return vs(b);}
static Value nstr_ord(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR||!a[0].u.s[0])return vi(-1);return vi((unsigned char)a[0].u.s[0]);}
static Value nstr_chr(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VINT||a[0].u.i<0||a[0].u.i>255)return vn();char b[2]={(char)a[0].u.i,0};return vs(b);}
static Value nfile_read(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS)||n!=1||a[0].t!=VSTR||!fs_path_allowed(vm,a[0].u.s))return vn();char*b=readf_limit(a[0].u.s,H_DATA_MAX);if(!b)return vn();Value r=vs(b);xfree(b);return r;}
static Value nfile_write(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS)||n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!fs_path_allowed(vm,a[0].u.s))return vb(0);FILE*f=fopen(a[0].u.s,"wb");if(!f)return vb(0);size_t z=strlen(a[1].u.s);int ok=fwrite(a[1].u.s,1,z,f)==z&&fclose(f)==0;return vb(ok);}
static Value nfile_append(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS)||n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!fs_path_allowed(vm,a[0].u.s))return vb(0);FILE*f=fopen(a[0].u.s,"ab");if(!f)return vb(0);size_t z=strlen(a[1].u.s);int ok=fwrite(a[1].u.s,1,z,f)==z&&fclose(f)==0;return vb(ok);}
static Value ndir_exists(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS)||n!=1||a[0].t!=VSTR||!fs_path_allowed(vm,a[0].u.s))return vb(0);struct stat st;return vb(stat(a[0].u.s,&st)==0&&S_ISDIR(st.st_mode));}
static Value npath_join(VM*vm,int n,Value*a){(void)vm;if(n<1||n>8)return vn();size_t cap=1;for(int i=0;i<n;i++)if(a[i].t==VSTR)cap+=strlen(a[i].u.s)+2;else return vn();char*b=xmalloc(cap);b[0]=0;for(int i=0;i<n;i++){if(i&&b[strlen(b)-1]!='/')strcat(b,"/");const char*s=a[i].u.s;while(*s=='/'&&i) s++;strcat(b,s);}Value r=vs(b);xfree(b);return r;}
static Value npath_basename(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();const char*p=a[0].u.s+strlen(a[0].u.s);while(p>a[0].u.s&&p[-1]!='/'&&p[-1]!='\\')p--;return vs(p);}
static Value npath_dirname(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();const char*p=strrchr(a[0].u.s,'/');if(!p)p=strrchr(a[0].u.s,'\\');if(!p)return vs(".");size_t z=(size_t)(p-a[0].u.s);if(!z)z=1;char*b=xmalloc(z+1);memcpy(b,a[0].u.s,z);b[z]=0;Value r=vs(b);xfree(b);return r;}
static Value npath_extension(VM*vm,int n,Value*a){
    (void)vm;
    if(n!=1||a[0].t!=VSTR)return vn();
    const char*s=a[0].u.s; const char*base=s;
    for(const char*p=s;*p;p++) if(*p=='/'||*p=='\\') base=p+1;
    const char*dot=strrchr(base,'.');
    if(!dot||dot==base)return vs("");
    return vs(dot);
}

/* Native filesystem API. All paths are capability-gated and reject shell metacharacters.
   Operations act on files/directories directly; no shell is invoked. */
static int fs_valid_path(VM*vm,const char*s,const char*name){
    if(!cap_allowed(vm,CAP_FS))return 0;
    if(!fs_path_allowed(vm,s))return 0;
    (void)name; return 1;
}
static Value nfs_read(VM*vm,int n,Value*a){
    if(n!=1||a[0].t!=VSTR||!fs_valid_path(vm,a[0].u.s,"fs.read"))return vn();
    char*b=readf_limit(a[0].u.s,H_DATA_MAX); if(!b)return vn();
    Value r=vs(b); xfree(b); return r;
}
static Value nfs_write(VM*vm,int n,Value*a){
    if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!fs_valid_path(vm,a[0].u.s,"fs.write"))return vb(0);
    FILE*f=fopen(a[0].u.s,"wb"); if(!f)return vb(0);
    size_t z=strlen(a[1].u.s); int ok=(fwrite(a[1].u.s,1,z,f)==z && fclose(f)==0); return vb(ok);
}
static Value nfs_delete(VM*vm,int n,Value*a){
    if(n!=1||a[0].t!=VSTR||!fs_valid_path(vm,a[0].u.s,"fs.delete"))return vb(0);
    if(unlink(a[0].u.s)==0)return vb(1);
    #ifdef _WIN32
    if(_rmdir(a[0].u.s)==0)return vb(1);
    #else
    if(rmdir(a[0].u.s)==0)return vb(1);
    #endif
    return vb(0);
}
static int fs_copy_file(const char*src,const char*dst){
    FILE*in=fopen(src,"rb"); if(!in)return 0;
    FILE*out=fopen(dst,"wb"); if(!out){fclose(in);return 0;}
    unsigned char buf[64*1024]; size_t z; int ok=1;
    while((z=fread(buf,1,sizeof buf,in))>0){ if(fwrite(buf,1,z,out)!=z){ok=0;break;} }
    if(ferror(in))ok=0;
    if(fclose(in)!=0)ok=0; if(fclose(out)!=0)ok=0;
    return ok;
}
static Value nfs_copy(VM*vm,int n,Value*a){
    if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!fs_valid_path(vm,a[0].u.s,"fs.copy")||!fs_valid_path(vm,a[1].u.s,"fs.copy"))return vb(0);
    return vb(fs_copy_file(a[0].u.s,a[1].u.s));
}
static Value nfs_move(VM*vm,int n,Value*a){
    if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!fs_valid_path(vm,a[0].u.s,"fs.move")||!fs_valid_path(vm,a[1].u.s,"fs.move"))return vb(0);
    #ifdef _WIN32
    if(MoveFileExA(a[0].u.s,a[1].u.s,MOVEFILE_REPLACE_EXISTING|MOVEFILE_COPY_ALLOWED))return vb(1);
    #else
    if(rename(a[0].u.s,a[1].u.s)==0)return vb(1);
    #endif
    return vb(0);
}
static Value nfs_list_dir(VM*vm,int n,Value*a){
    if(n!=1||a[0].t!=VSTR||!fs_valid_path(vm,a[0].u.s,"fs.list_dir"))return vn();
    Value out=va();
    #ifdef _WIN32
    char pattern[PATH_MAX*2]; int w=snprintf(pattern,sizeof pattern,"%s%s*",a[0].u.s,((*a[0].u.s && a[0].u.s[strlen(a[0].u.s)-1]=='\\')?"":"\\"));
    if(w<0||(size_t)w>=sizeof pattern)return vn();
    WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA(pattern,&fd); if(h==INVALID_HANDLE_VALUE)return out;
    do { if(strcmp(fd.cFileName,".")&&strcmp(fd.cFileName,"..")){Value o=vsobj();stput(o.u.st,"name",vs(fd.cFileName));Value pp[2]={(Value){.t=VSTR,.u.s=a[0].u.s},(Value){.t=VSTR,.u.s=fd.cFileName}};stput(o.u.st,"path",npath_join(vm,2,pp));stput(o.u.st,"directory",vb((fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0));ap(out.u.a,o);} } while(FindNextFileA(h,&fd));
    FindClose(h);
    #else
    DIR*d=opendir(a[0].u.s); if(!d)return out; struct dirent*e;
    while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;Value o=vsobj();stput(o.u.st,"name",vs(e->d_name));Value pp[2]={(Value){.t=VSTR,.u.s=a[0].u.s},vs(e->d_name)};stput(o.u.st,"path",npath_join(vm,2,pp));
        char full[PATH_MAX];int w=snprintf(full,sizeof full,"%s/%s",a[0].u.s,e->d_name);struct stat st={0};int isdir=(w>0&&w<(int)sizeof full&&stat(full,&st)==0&&S_ISDIR(st.st_mode));stput(o.u.st,"directory",vb(isdir));ap(out.u.a,o);
    } closedir(d);
    #endif
    return out;
}
static unsigned long long map_key_hash(Value k){
    unsigned long long h=1469598103934665603ULL;
    #define HR_HMIX(byte) do{ h^=(unsigned char)(byte); h*=1099511628211ULL; }while(0)
    HR_HMIX((int)k.t);
    switch(k.t){
        case VNULL: break;
        case VBOOL: HR_HMIX(k.u.b); break;
        case VINT: { unsigned long long v=(unsigned long long)k.u.i; for(int i=0;i<8;i++){HR_HMIX(v&0xFF); v>>=8;} break; }
        case VFLOAT: { unsigned long long v; memcpy(&v,&k.u.f,sizeof v); for(int i=0;i<8;i++){HR_HMIX(v&0xFF); v>>=8;} break; }
        case VSTR: { for(const char*s=k.u.s; *s; s++) HR_HMIX(*s); break; }
        case VARR: { HR_HMIX(k.u.a->n); for(size_t i=0;i<k.u.a->n;i++){ unsigned long long sub=map_key_hash(k.u.a->v[i]); for(int b=0;b<8;b++){HR_HMIX(sub&0xFF); sub>>=8;} } break; }
        default: { unsigned long long v=(unsigned long long)(size_t)k.u.st; for(int i=0;i<8;i++){HR_HMIX(v&0xFF); v>>=8;} break; }
    }
    #undef HR_HMIX
    return h;
}
static int map_key_eq(Value a,Value b){
    if(a.t!=b.t) return (isnum(a)&&isnum(b))?dn(a)==dn(b):0;
    switch(a.t){
        case VNULL: return 1;
        case VBOOL: return a.u.b==b.u.b;
        case VINT: return a.u.i==b.u.i;
        case VFLOAT: return a.u.f==b.u.f;
        case VSTR: return !strcmp(a.u.s,b.u.s);
        case VARR: { if(a.u.a->n!=b.u.a->n) return 0; for(size_t i=0;i<a.u.a->n;i++) if(!map_key_eq(a.u.a->v[i],b.u.a->v[i])) return 0; return 1; }
        default: return a.u.st==b.u.st;
    }
}
static Value map_new_sized(size_t nbuckets){
    Value m=vsobj(); stput(m.u.st,"__type",vs("map")); stput(m.u.st,"__size",vi(0));
    Value buckets=va(); for(size_t i=0;i<nbuckets;i++) ap(buckets.u.a,va());
    stput(m.u.st,"__buckets",buckets); return m;
}
static Value nmap_new(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();return map_new_sized(16);}
static void map_maybe_grow(StructObj*st){
    Value bv=stget(st,"__buckets"),szv=stget(st,"__size");
    if(bv.t!=VARR||szv.t!=VINT||bv.u.a->n==0) return;
    size_t cap=bv.u.a->n; long long size=szv.u.i;
    if((double)size/(double)cap<0.75) return;
    size_t newcap=cap*2; Value newb=va(); for(size_t i=0;i<newcap;i++) ap(newb.u.a,va());
    for(size_t i=0;i<cap;i++){ Value bucket=bv.u.a->v[i]; if(bucket.t!=VARR) continue;
        for(size_t j=0;j<bucket.u.a->n;j++){ Value pair=bucket.u.a->v[j]; if(pair.t!=VARR||pair.u.a->n<2) continue;
            size_t idx=(size_t)(map_key_hash(pair.u.a->v[0])%newcap); ap(newb.u.a->v[idx].u.a,pair); } }
    stput(st,"__buckets",newb);
}
static Value nmap_set(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VSTRUCT)return vb(0);
    StructObj*st=a[0].u.st; Value bv=stget(st,"__buckets"); if(bv.t!=VARR||bv.u.a->n==0)return vb(0);
    size_t cap=bv.u.a->n,idx=(size_t)(map_key_hash(a[1])%cap); Value bucket=bv.u.a->v[idx]; if(bucket.t!=VARR)return vb(0);
    for(size_t i=0;i<bucket.u.a->n;i++){ Value pair=bucket.u.a->v[i]; if(pair.t==VARR&&pair.u.a->n>=2&&map_key_eq(pair.u.a->v[0],a[1])){ pair.u.a->v[1]=a[2]; return vb(1); } }
    Value pair=va(); ap(pair.u.a,a[1]); ap(pair.u.a,a[2]); ap(bucket.u.a,pair);
    Value szv=stget(st,"__size"); stput(st,"__size",vi((szv.t==VINT?szv.u.i:0)+1)); map_maybe_grow(st); return vb(1);
}
static Value nmap_get(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT)return vn();
    Value bv=stget(a[0].u.st,"__buckets"); if(bv.t!=VARR||bv.u.a->n==0)return vn();
    size_t cap=bv.u.a->n,idx=(size_t)(map_key_hash(a[1])%cap); Value bucket=bv.u.a->v[idx]; if(bucket.t!=VARR)return vn();
    for(size_t i=0;i<bucket.u.a->n;i++){ Value pair=bucket.u.a->v[i]; if(pair.t==VARR&&pair.u.a->n>=2&&map_key_eq(pair.u.a->v[0],a[1])) return pair.u.a->v[1]; }
    return vn();
}
static Value nmap_has(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT)return vb(0);
    Value bv=stget(a[0].u.st,"__buckets"); if(bv.t!=VARR||bv.u.a->n==0)return vb(0);
    size_t cap=bv.u.a->n,idx=(size_t)(map_key_hash(a[1])%cap); Value bucket=bv.u.a->v[idx]; if(bucket.t!=VARR)return vb(0);
    for(size_t i=0;i<bucket.u.a->n;i++){ Value pair=bucket.u.a->v[i]; if(pair.t==VARR&&pair.u.a->n>=2&&map_key_eq(pair.u.a->v[0],a[1])) return vb(1); }
    return vb(0);
}
static Value nmap_delete(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT)return vb(0);
    StructObj*st=a[0].u.st; Value bv=stget(st,"__buckets"); if(bv.t!=VARR||bv.u.a->n==0)return vb(0);
    size_t cap=bv.u.a->n,idx=(size_t)(map_key_hash(a[1])%cap); Value bucket=bv.u.a->v[idx]; if(bucket.t!=VARR)return vb(0);
    for(size_t i=0;i<bucket.u.a->n;i++){ Value pair=bucket.u.a->v[i]; if(pair.t==VARR&&pair.u.a->n>=2&&map_key_eq(pair.u.a->v[0],a[1])){
        for(size_t j=i;j+1<bucket.u.a->n;j++) bucket.u.a->v[j]=bucket.u.a->v[j+1]; bucket.u.a->n--;
        Value szv=stget(st,"__size"); long long ns=(szv.t==VINT?szv.u.i:1)-1; if(ns<0)ns=0; stput(st,"__size",vi(ns)); return vb(1); } }
    return vb(0);
}
static Value nmap_items(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTRUCT)return va();
    Value bv=stget(a[0].u.st,"__buckets"),out=va(); if(bv.t!=VARR)return out;
    for(size_t i=0;i<bv.u.a->n;i++){ Value bucket=bv.u.a->v[i]; if(bucket.t!=VARR)continue; for(size_t j=0;j<bucket.u.a->n;j++) ap(out.u.a,bucket.u.a->v[j]); }
    return out;
}
static Value nmap_keys(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTRUCT)return va();
    Value bv=stget(a[0].u.st,"__buckets"),out=va(); if(bv.t!=VARR)return out;
    for(size_t i=0;i<bv.u.a->n;i++){ Value bucket=bv.u.a->v[i]; if(bucket.t!=VARR)continue; for(size_t j=0;j<bucket.u.a->n;j++){ Value pair=bucket.u.a->v[j]; if(pair.t==VARR&&pair.u.a->n>=1) ap(out.u.a,pair.u.a->v[0]); } }
    return out;
}
static Value nmap_values(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTRUCT)return va();
    Value bv=stget(a[0].u.st,"__buckets"),out=va(); if(bv.t!=VARR)return out;
    for(size_t i=0;i<bv.u.a->n;i++){ Value bucket=bv.u.a->v[i]; if(bucket.t!=VARR)continue; for(size_t j=0;j<bucket.u.a->n;j++){ Value pair=bucket.u.a->v[j]; if(pair.t==VARR&&pair.u.a->n>=2) ap(out.u.a,pair.u.a->v[1]); } }
    return out;
}
static Value nmap_size(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTRUCT)return vi(0);Value szv=stget(a[0].u.st,"__size");return szv.t==VINT?szv:vi(0);}

static Value nset_new(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();return va();}
static int val_eq_simple(Value a,Value b){if(isnum(a)&&isnum(b))return dn(a)==dn(b);if(a.t==VSTR&&b.t==VSTR)return !strcmp(a.u.s,b.u.s);if(a.t==VBOOL&&b.t==VBOOL)return a.u.b==b.u.b;return a.t==VNULL&&b.t==VNULL;}
static Value nset_add(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR)return vb(0);for(size_t i=0;i<a[0].u.a->n;i++)if(val_eq_simple(a[0].u.a->v[i],a[1]))return vb(1);ap(a[0].u.a,a[1]);return vb(1);}
static Value nset_has(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR)return vb(0);for(size_t i=0;i<a[0].u.a->n;i++)if(val_eq_simple(a[0].u.a->v[i],a[1]))return vb(1);return vb(0);}
static Value nset_size(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR)return vi(0);return vi((long long)a[0].u.a->n);}
static Value nqueue_new(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();return va();}
static Value nqueue_push(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR)return vb(0);ap(a[0].u.a,a[1]);return vb(1);}
static Value nqueue_pop(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR||!a[0].u.a->n)return vn();Value r=a[0].u.a->v[0];if(a[0].u.a->n>1)memmove(a[0].u.a->v,a[0].u.a->v+1,(a[0].u.a->n-1)*sizeof(Value));a[0].u.a->n--;return r;}


static Value nstr_format(VM*vm,int n,Value*a){(void)vm;if(n<1||a[0].t!=VSTR||n>9)return vn();const char*fmt=a[0].u.s;size_t cap=strlen(fmt)+64;char*out=xmalloc(cap);size_t pos=0;int arg=1;
    for(size_t i=0;fmt[i];){
        if(fmt[i]=='{' && fmt[i+1]=='{'){if(pos+1>=cap){cap*=2;out=xrealloc(out,cap);}out[pos++]='{';i+=2;continue;}
        if(fmt[i]=='}' && fmt[i+1]=='}'){if(pos+1>=cap){cap*=2;out=xrealloc(out,cap);}out[pos++]='}';i+=2;continue;}
        if(fmt[i]=='{' && fmt[i+1]=='}'){if(arg>=n)die("Haris format error: missing argument for {} placeholder");char tmp[256];Value v=a[arg++];value_error_text(v,tmp,sizeof tmp);size_t z=strlen(tmp);while(pos+z+1>=cap){cap*=2;out=xrealloc(out,cap);}memcpy(out+pos,tmp,z);pos+=z;i+=2;continue;}
        if(pos+1>=cap){cap*=2;out=xrealloc(out,cap);}out[pos++]=fmt[i++];
    }
    out[pos]=0;Value r=vs(out);xfree(out);return r;
}
static size_t utf8_next(const unsigned char *s,size_t n,size_t i,unsigned *cp){if(i>=n)return i;unsigned c=s[i];if(c<0x80){*cp=c;return i+1;}if((c&0xE0)==0xC0&&i+1<n&&(s[i+1]&0xC0)==0x80){*cp=((c&0x1F)<<6)|(s[i+1]&0x3F);return i+2;}if((c&0xF0)==0xE0&&i+2<n&&(s[i+1]&0xC0)==0x80&&(s[i+2]&0xC0)==0x80){*cp=((c&0x0F)<<12)|((s[i+1]&0x3F)<<6)|(s[i+2]&0x3F);return i+3;}if((c&0xF8)==0xF0&&i+3<n&&(s[i+1]&0xC0)==0x80&&(s[i+2]&0xC0)==0x80&&(s[i+3]&0xC0)==0x80){*cp=((c&7)<<18)|((s[i+1]&0x3F)<<12)|((s[i+2]&0x3F)<<6)|(s[i+3]&0x3F);return i+4;}*cp=0xFFFD;return i+1;}
static Value nstr_utf8_len(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();size_t z=0,N=strlen(a[0].u.s);for(size_t i=0;i<N;){unsigned cp;size_t j=utf8_next((const unsigned char*)a[0].u.s,N,i,&cp);if(j<=i)break;i=j;z++;}return vi((long long)z);}
static Value nstr_utf8_at(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTR||a[1].t!=VINT||a[1].u.i<0)return vn();long long idx=a[1].u.i;size_t N=strlen(a[0].u.s),i=0,k=0;while(i<N){unsigned cp;size_t j=utf8_next((const unsigned char*)a[0].u.s,N,i,&cp);if((long long)k==idx){char b[5];size_t z=0;if(cp<=0x7F)b[z++]=(char)cp;else if(cp<=0x7FF){b[z++]=(char)(0xC0|(cp>>6));b[z++]=(char)(0x80|(cp&63));}else if(cp<=0xFFFF){b[z++]=(char)(0xE0|(cp>>12));b[z++]=(char)(0x80|((cp>>6)&63));b[z++]=(char)(0x80|(cp&63));}else{b[z++]=(char)(0xF0|(cp>>18));b[z++]=(char)(0x80|((cp>>12)&63));b[z++]=(char)(0x80|((cp>>6)&63));b[z++]=(char)(0x80|(cp&63));}b[z]=0;return vs(b);}i=j;k++;}return vn();}
static Value nstr_utf8_chr(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VINT||a[0].u.i<0||a[0].u.i>0x10FFFF)return vn();unsigned cp=(unsigned)a[0].u.i;char b[5];size_t z=0;if(cp<=0x7F)b[z++]=(char)cp;else if(cp<=0x7FF){b[z++]=(char)(0xC0|(cp>>6));b[z++]=(char)(0x80|(cp&63));}else if(cp<=0xFFFF){if(cp>=0xD800&&cp<=0xDFFF)return vn();b[z++]=(char)(0xE0|(cp>>12));b[z++]=(char)(0x80|((cp>>6)&63));b[z++]=(char)(0x80|(cp&63));}else{b[z++]=(char)(0xF0|(cp>>18));b[z++]=(char)(0x80|((cp>>12)&63));b[z++]=(char)(0x80|((cp>>6)&63));b[z++]=(char)(0x80|(cp&63));}b[z]=0;return vs(b);}
static Value nrange(VM*vm,int n,Value*a){(void)vm;if(n<1||n>3)return vn();long long start=0,end=0,step=1;if(n==1){if(a[0].t!=VINT)return vn();end=a[0].u.i;}else{if(a[0].t!=VINT||a[1].t!=VINT)return vn();start=a[0].u.i;end=a[1].u.i;if(n==3){if(a[2].t!=VINT)return vn();step=a[2].u.i;}}if(step==0)return vn();unsigned long long count=0,mag=0;if(step>0&&start<end){unsigned long long dist=(unsigned long long)end-(unsigned long long)start-1ULL;count=dist/(unsigned long long)step+1ULL;}else if(step<0&&start>end){unsigned long long dist=(unsigned long long)start-(unsigned long long)end-1ULL;mag=(unsigned long long)(-(step+1))+1ULL;count=dist/mag+1ULL;}if(count>1000000ULL)die("Haris range too large: %llu elements; use bounded iteration",count);Value r=va();if(!arr_reserve(r.u.a,(size_t)count))return vn();for(unsigned long long i=0;i<count;i++){unsigned long long off=i*(step>0?(unsigned long long)step:mag);long long x=step>0?(long long)((unsigned long long)start+off):(long long)((unsigned long long)start-off);ap(r.u.a,vi(x));}return r;}
static Value ntuple(VM*vm,int n,Value*a){(void)vm;Value r=va();for(int i=0;i<n;i++)ap(r.u.a,a[i]);return r;}
static Value nstack_new(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();return va();}
static Value nstack_push(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR)return vb(0);ap(a[0].u.a,a[1]);return vb(1);}
static Value nstack_pop(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR||!a[0].u.a->n)return vn();return a[0].u.a->v[--a[0].u.a->n];}
static Value nstack_size(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR)return vi(-1);return vi((long long)a[0].u.a->n);}

typedef struct { int kind; FILE *f; } HFile;
static void hfile_dtor(void*p){HFile*h=(HFile*)p;if(h&&h->f){fclose(h->f);h->f=NULL;}}
#define HK_FILE 0x46494C
static Value nfile_open(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"file.open");if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!fs_path_allowed(vm,a[0].u.s))return vn();const char*mode=a[1].u.s;if(!*mode||strpbrk(mode,"rwbax+" )==NULL)return vn();FILE*f=fopen(a[0].u.s,mode);if(!f)return vn();HFile*h=xmalloc_dtor(sizeof *h,hfile_dtor);h->kind=HK_FILE;h->f=f;return (Value){.t=VHANDLE,.u.handle=h};}
static Value nfile_read_any(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"file.read");if(n!=1)return vn();if(a[0].t==VSTR)return nfile_read(vm,n,a);if(a[0].t!=VHANDLE||!hkind(a[0],HK_FILE))return vn();HFile*h=a[0].u.handle;if(!h->f)return vn();size_t cap=8192;long here=ftell(h->f);if(here>=0&&fseek(h->f,0,SEEK_END)==0){long end=ftell(h->f);if(end>=here){unsigned long long rem=(unsigned long long)(end-here);cap=(size_t)(rem>H_DATA_MAX?H_DATA_MAX:rem);}fseek(h->f,here,SEEK_SET);}if(cap>H_DATA_MAX)cap=H_DATA_MAX;char*b=xmalloc(cap+1);size_t z=fread(b,1,cap,h->f);b[z]=0;Value r=vs(b);xfree(b);return r;}
static Value nfile_write_any(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"file.write");if(n!=2||a[1].t!=VSTR)return vb(0);if(a[0].t==VSTR)return nfile_write(vm,n,a);if(a[0].t!=VHANDLE||!hkind(a[0],HK_FILE))return vb(0);HFile*h=a[0].u.handle;if(!h->f)return vb(0);size_t z=strlen(a[1].u.s);return vb(fwrite(a[1].u.s,1,z,h->f)==z);}
static Value nfile_close(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"file.close");if(n!=1||!hkind(a[0],HK_FILE))return vb(0);HFile*h=a[0].u.handle;if(!h->f){h->kind=HK_CLOSED;return vb(1);}int ok=fclose(h->f)==0;h->f=NULL;if(ok)h->kind=HK_CLOSED;return vb(ok);}
static Value ndir_list(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"dir.list");if(n!=1||a[0].t!=VSTR||!fs_path_allowed(vm,a[0].u.s))return vn();Value out=va();
#ifdef _WIN32
    char pat[PATH_MAX];snprintf(pat,sizeof pat,"%s\\*",a[0].u.s);WIN32_FIND_DATAA d;HANDLE h=FindFirstFileA(pat,&d);if(h==INVALID_HANDLE_VALUE)return out;do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,".."))ap(out.u.a,vs(d.cFileName));}while(FindNextFileA(h,&d));FindClose(h);
#else
    DIR*d=opendir(a[0].u.s);if(!d)return out;struct dirent*e;while((e=readdir(d)))if(strcmp(e->d_name,".")&&strcmp(e->d_name,".."))ap(out.u.a,vs(e->d_name));closedir(d);
#endif
    return out;
}


static char *regex_expand_shorthand(const char *src){
    size_t n=strlen(src),cap=n+32,pos=0;char*out=xmalloc(cap);int in_class=0,esc=0;
    for(size_t i=0;i<n;i++){
        char c=src[i];
        if(!esc && c=='[') in_class=1;
        if(c=='\\' && i+1<n){
            const char*rep=NULL;
            if(src[i+1]=='d') rep=in_class ? "[:digit:]" : "[0-9]";
            else if(src[i+1]=='w') rep=in_class ? "[:alnum:]_" : "[[:alnum:]_]";
            else if(src[i+1]=='s') rep=in_class ? "[:space:]" : "[[:space:]]";
            if(rep){size_t L=strlen(rep);while(pos+L+1>=cap){cap*=2;out=xrealloc(out,cap);}memcpy(out+pos,rep,L);pos+=L;i++;esc=0;continue;}
            esc=1;
        } else esc=0;
        if(pos+2>=cap){cap*=2;out=xrealloc(out,cap);}out[pos++]=c;
        if(!esc && c==']') in_class=0;
    }
    out[pos]=0;return out;
}
typedef struct HRegex { int kind; regex_t re; int flags; int compiled; } HRegex;
static void regex_dtor(void*p){HRegex*r=(HRegex*)p;if(r&&r->kind==HK_REGEX&&r->compiled)regfree(&r->re);}
static HRegex*regex_handle(Value v){if(v.t!=VSTRUCT)return NULL;Value h=stget(v.u.st,"handle");return hkind(h,HK_REGEX)?(HRegex*)h.u.handle:NULL;}
static Value regex_compile(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||a[0].t!=VSTR||(n==2&&a[1].t!=VSTR))return vn();int flags=(n==2&&strchr(a[1].u.s,'i'))?(REG_EXTENDED|REG_ICASE):REG_EXTENDED;char*x=regex_expand_shorthand(a[0].u.s);HRegex*r=xmalloc_dtor(sizeof(*r),regex_dtor);memset(r,0,sizeof*r);r->kind=HK_REGEX;r->flags=flags;if(regcomp(&r->re,x,flags)!=0){xfree(r);xfree(x);return vn();}r->compiled=1;xfree(x);Value o=vsobj();stput(o.u.st,"__type",vs("regex"));stput(o.u.st,"pattern",a[0]);stput(o.u.st,"flags",n==2?a[1]:vs(""));stput(o.u.st,"handle",(Value){.t=VHANDLE,.u.handle=r});return o;}
static Value regex_find_all(VM*vm,int n,Value*a){(void)vm;if(n!=2)return vn();const char*txt=NULL;HRegex*r=NULL;regex_t local;int owns=0;if(a[0].t==VSTRUCT&&a[1].t==VSTR){r=regex_handle(a[0]);if(!r)return vn();txt=a[1].u.s;}else if(a[0].t==VSTR&&a[1].t==VSTR){char*x=regex_expand_shorthand(a[0].u.s);if(regcomp(&local,x,REG_EXTENDED)!=0){xfree(x);return vn();}xfree(x);owns=1;txt=a[1].u.s;}else return vn();Value out=va();const char*cur=txt;size_t guard=0;regex_t*re=r?&r->re:&local;while(*cur&&guard++<100000){regmatch_t m;if(regexec(re,cur,1,&m,0)!=0||m.rm_so<0||m.rm_eo<=m.rm_so)break;size_t L=(size_t)(m.rm_eo-m.rm_so);char*b=xmalloc(L+1);memcpy(b,cur+m.rm_so,L);b[L]=0;ap(out.u.a,vs(b));xfree(b);cur+=m.rm_eo;}if(owns)regfree(&local);return out;}
static Value regex_match(VM*vm,int n,Value*a){(void)vm;if(n!=2)return vb(0);const char*txt=NULL;HRegex*r=NULL;regex_t local;int owns=0;if(a[0].t==VSTRUCT&&a[1].t==VSTR){r=regex_handle(a[0]);if(!r)return vb(0);txt=a[1].u.s;}else if(a[0].t==VSTR&&a[1].t==VSTR){char*x=regex_expand_shorthand(a[0].u.s);if(regcomp(&local,x,REG_EXTENDED)!=0){xfree(x);return vb(0);}xfree(x);owns=1;txt=a[1].u.s;}else return vb(0);int rc=regexec(r?&r->re:&local,txt,0,NULL,0);if(owns)regfree(&local);return vb(rc==0);}
static Value regex_search(VM*vm,int n,Value*a){return regex_match(vm,n,a);}
static Value datetime_make(time_t tt){struct tm t;memset(&t,0,sizeof t);
#ifdef _WIN32
    localtime_s(&t,&tt);
#else
    localtime_r(&tt,&t);
#endif
    Value o=vsobj();stput(o.u.st,"__type",vs("datetime"));stput(o.u.st,"epoch",vi((long long)tt));stput(o.u.st,"year",vi(t.tm_year+1900));stput(o.u.st,"month",vi(t.tm_mon+1));stput(o.u.st,"day",vi(t.tm_mday));stput(o.u.st,"hour",vi(t.tm_hour));stput(o.u.st,"minute",vi(t.tm_min));stput(o.u.st,"second",vi(t.tm_sec));stput(o.u.st,"weekday",vi(t.tm_wday));return o;}
static Value datetime_now(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();return datetime_make(time(NULL));}
static Value datetime_format(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT||a[1].t!=VSTR)return vn();Value e=stget(a[0].u.st,"epoch");if(e.t!=VINT)return vn();time_t tt=(time_t)e.u.i;struct tm t;memset(&t,0,sizeof t);
#ifdef _WIN32
    localtime_s(&t,&tt);
#else
    localtime_r(&tt,&t);
#endif
    char f[256];size_t o=0;const char*fmt=a[1].u.s;for(size_t i=0;fmt[i]&&o+4<sizeof f;i++){if(!strncmp(fmt+i,"YYYY",4)){f[o++]='%';f[o++]='Y';i+=3;}else if(!strncmp(fmt+i,"MM",2)){f[o++]='%';f[o++]='m';i++;}else if(!strncmp(fmt+i,"DD",2)){f[o++]='%';f[o++]='d';i++;}else if(!strncmp(fmt+i,"HH",2)){f[o++]='%';f[o++]='H';i++;}else if(!strncmp(fmt+i,"mm",2)){f[o++]='%';f[o++]='M';i++;}else if(!strncmp(fmt+i,"ss",2)){f[o++]='%';f[o++]='S';i++;}else f[o++]=fmt[i];}f[o]=0;char out[512];return strftime(out,sizeof out,f,&t)?vs(out):vs("");}
static Value datetime_add_seconds(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT||a[1].t!=VINT)return vn();Value e=stget(a[0].u.st,"epoch");if(e.t!=VINT)return vn();return datetime_make((time_t)(e.u.i+a[1].u.i));}
static Value datetime_add_days(VM*vm,int n,Value*a){if(n!=2||a[0].t!=VSTRUCT||a[1].t!=VINT)return vn();return datetime_add_seconds(vm,2,(Value[]){a[0],vi(a[1].u.i*86400LL)});}
static Value datetime_diff(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT||a[1].t!=VSTRUCT)return vn();Value x=stget(a[0].u.st,"epoch"),y=stget(a[1].u.st,"epoch");if(x.t!=VINT||y.t!=VINT)return vn();return vi(y.u.i-x.u.i);}
#define HK_GFX 0x474658
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
#ifdef _WIN32
typedef HMODULE GfxLib;
#else
typedef void *GfxLib;
#endif
static GfxLib gfx_lib=NULL;
static SDL_Renderer *g_gfx_renderer=NULL;
static int (*p_SDL_Init)(unsigned int)=NULL;
static void (*p_SDL_Quit)(void)=NULL;
static SDL_Window *(*p_SDL_CreateWindow)(const char*,int,int,int,int,unsigned int)=NULL;
static void (*p_SDL_DestroyWindow)(SDL_Window*)=NULL;
static SDL_Renderer *(*p_SDL_CreateRenderer)(SDL_Window*,int,unsigned int)=NULL;
static void (*p_SDL_DestroyRenderer)(SDL_Renderer*)=NULL;
static int (*p_SDL_SetRenderDrawColor)(SDL_Renderer*,unsigned char,unsigned char,unsigned char,unsigned char)=NULL;
static int (*p_SDL_RenderClear)(SDL_Renderer*)=NULL;
static void (*p_SDL_RenderPresent)(SDL_Renderer*)=NULL;
static int (*p_SDL_RenderDrawPoint)(SDL_Renderer*,int,int)=NULL;
static int (*p_SDL_PollEvent)(void*)=NULL;
#ifdef _WIN32
#define GFX_LOAD(name) p_##name=(void*)GetProcAddress(gfx_lib,#name)
#else
#define GFX_LOAD(name) p_##name=dlsym(gfx_lib,#name)
#endif
static int gfx_load_api(void){
    if(gfx_lib) return p_SDL_CreateWindow!=NULL;
#ifdef _WIN32
    const char*names[]={"SDL2.dll","SDL2-2.0.dll"};
    for(size_t i=0;i<sizeof(names)/sizeof(names[0])&&!gfx_lib;i++) gfx_lib=LoadLibraryA(names[i]);
#else
    const char*names[]={"libSDL2-2.0.so.0","libSDL2.so"};
    for(size_t i=0;i<sizeof(names)/sizeof(names[0])&&!gfx_lib;i++) gfx_lib=dlopen(names[i],RTLD_NOW|RTLD_LOCAL);
#endif
    if(!gfx_lib)return 0;
    GFX_LOAD(SDL_Init); GFX_LOAD(SDL_Quit); GFX_LOAD(SDL_CreateWindow); GFX_LOAD(SDL_DestroyWindow);
    GFX_LOAD(SDL_CreateRenderer); GFX_LOAD(SDL_DestroyRenderer); GFX_LOAD(SDL_SetRenderDrawColor);
    GFX_LOAD(SDL_RenderClear); GFX_LOAD(SDL_RenderPresent); GFX_LOAD(SDL_RenderDrawPoint); GFX_LOAD(SDL_PollEvent);
    return p_SDL_Init&&p_SDL_CreateWindow&&p_SDL_DestroyWindow&&p_SDL_CreateRenderer&&p_SDL_DestroyRenderer&&p_SDL_SetRenderDrawColor&&p_SDL_RenderClear&&p_SDL_RenderPresent&&p_SDL_RenderDrawPoint;
}
static int gfx_parse_color(const char*s,unsigned char*r,unsigned char*g,unsigned char*b){*r=0;*g=0;*b=0;if(!s)return 0;if(!strcasecmp(s,"black"))return 1;if(!strcasecmp(s,"white")){*r=*g=*b=255;return 1;}if(!strcasecmp(s,"red")){*r=255;return 1;}if(!strcasecmp(s,"green")){*g=255;return 1;}if(!strcasecmp(s,"blue")){*b=255;return 1;}if(s[0]=='#'&&strlen(s)==7){unsigned int x=0;if(sscanf(s+1,"%06x",&x)==1){*r=(unsigned char)(x>>16);*g=(unsigned char)(x>>8);*b=(unsigned char)x;return 1;}}return 0;}
typedef struct HGfxWindow {int kind;int open;int w,h;char *title;SDL_Window*win;SDL_Renderer*ren;int real_sdl;} HGfxWindow;
static void hgfx_dtor(void*p){HGfxWindow*w=(HGfxWindow*)p;if(!w)return;if(w->real_sdl&&w->ren)p_SDL_DestroyRenderer(w->ren);if(w->real_sdl&&w->win)p_SDL_DestroyWindow(w->win);w->ren=NULL;w->win=NULL;if(w->title){xfree(w->title);w->title=NULL;}w->open=0;w->real_sdl=0;}
static HGfxWindow*gfx_handle(Value v){if(v.t!=VSTRUCT)return NULL;Value h=stget(v.u.st,"handle");return h.t==VHANDLE?(HGfxWindow*)h.u.handle:NULL;}
static Value gfx_window(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VINT||a[1].t!=VINT||a[2].t!=VSTR)return vn();HGfxWindow*w=xmalloc_dtor(sizeof* w,hgfx_dtor);memset(w,0,sizeof* w);w->kind=HK_GFX;w->open=1;w->w=(int)a[0].u.i;w->h=(int)a[1].u.i;w->title=xdup(a[2].u.s);
    if(gfx_load_api()&&p_SDL_Init(0x00000020u)==0){
        w->win=p_SDL_CreateWindow(w->title,0x1FFF0000,0x1FFF0000,w->w,w->h,0x00000004u);
        if(w->win) w->ren=p_SDL_CreateRenderer(w->win,-1,0x00000002u|0x00000004u);
        if(w->win&&!w->ren) w->ren=p_SDL_CreateRenderer(w->win,-1,0);
        if(w->win&&w->ren){w->real_sdl=1;g_gfx_renderer=w->ren;}
    }
    Value o=vsobj();stput(o.u.st,"__type",vs("gfx_window"));stput(o.u.st,"handle",(Value){.t=VHANDLE,.u.handle=w});stput(o.u.st,"width",a[0]);stput(o.u.st,"height",a[1]);stput(o.u.st,"headless",vb(!w->real_sdl));return o;}
static Value gfx_is_open(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HGfxWindow*w=gfx_handle(a[0]);if(!w)return vb(0);if(w->real_sdl&&p_SDL_PollEvent){unsigned char ev[64];while(p_SDL_PollEvent(ev)){if(*(unsigned int*)ev==0x100u){w->open=0;break;}}}return vb(w->open);}
static Value gfx_clear(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[1].t!=VSTR)return vb(0);HGfxWindow*w=gfx_handle(a[0]);if(!w)return vb(0);if(w->real_sdl&&w->ren){unsigned char r,g,b;if(!gfx_parse_color(a[1].u.s,&r,&g,&b))return vb(0);p_SDL_SetRenderDrawColor(w->ren,r,g,b,255);p_SDL_RenderClear(w->ren);}return vb(1);}
static Value gfx_display(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HGfxWindow*w=gfx_handle(a[0]);if(w&&w->real_sdl&&w->ren)p_SDL_RenderPresent(w->ren);return vb(w!=NULL);}
static Value gfx_close(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);HGfxWindow*w=gfx_handle(a[0]);if(!w)return vb(0);if(w->real_sdl&&w->ren)p_SDL_DestroyRenderer(w->ren);if(w->real_sdl&&w->win)p_SDL_DestroyWindow(w->win);if(g_gfx_renderer==w->ren)g_gfx_renderer=NULL;w->ren=NULL;w->win=NULL;w->open=0;w->real_sdl=0;w->kind=HK_CLOSED;return vb(1);}
static Value gfx_draw_circle(VM*vm,int n,Value*a){(void)vm;if(n!=4||a[0].t!=VINT||a[1].t!=VINT||a[2].t!=VINT||a[3].t!=VSTR)return vb(0);if(!g_gfx_renderer)return vb(1);unsigned char r,g,b;if(!gfx_parse_color(a[3].u.s,&r,&g,&b))return vb(0);p_SDL_SetRenderDrawColor(g_gfx_renderer,r,g,b,255);int cx=(int)a[0].u.i,cy=(int)a[1].u.i,rad=(int)a[2].u.i;for(int y=-rad;y<=rad;y++)for(int x=-rad;x<=rad;x++)if((long long)x*x+(long long)y*y<=(long long)rad*rad)p_SDL_RenderDrawPoint(g_gfx_renderer,cx+x,cy+y);return vb(1);}
/* Built-in 5x7 bitmap font: no SDL_ttf dependency, deterministic in engine/headless builds. */
static const unsigned char gfx_font5x7[95][7]={
    {0,0,0,0,0,0,0},
    {16,16,16,16,16,0,16},
    {40,40,0,0,0,0,0},
    {40,124,40,40,124,40,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {16,16,0,0,0,0,0},
    {8,16,32,32,32,16,8},
    {32,16,8,8,8,16,32},
    {0,16,84,56,84,16,0},
    {0,16,16,124,16,16,0},
    {0,0,0,0,16,16,32},
    {0,0,0,124,0,0,0},
    {0,0,0,0,0,48,48},
    {4,8,16,32,64,0,0},
    {56,68,76,84,100,68,56},
    {16,48,16,16,16,16,56},
    {56,68,4,8,16,32,124},
    {120,4,4,56,4,4,120},
    {8,24,40,72,124,8,8},
    {124,64,64,120,4,4,120},
    {56,64,64,120,68,68,56},
    {124,4,8,16,32,32,32},
    {56,68,68,56,68,68,56},
    {56,68,68,60,4,4,56},
    {0,24,24,0,24,24,0},
    {0,24,24,0,24,16,32},
    {8,16,32,64,32,16,8},
    {0,124,0,124,0,0,0},
    {64,32,16,8,16,32,64},
    {56,68,4,8,16,0,16},
    {56,68,92,84,92,64,60},
    {56,68,68,124,68,68,68},
    {120,68,68,120,68,68,120},
    {60,64,64,64,64,64,60},
    {120,68,68,68,68,68,120},
    {124,64,64,120,64,64,124},
    {124,64,64,120,64,64,64},
    {60,64,64,92,68,68,60},
    {68,68,68,124,68,68,68},
    {124,16,16,16,16,16,124},
    {28,8,8,8,72,72,48},
    {68,72,80,96,80,72,68},
    {64,64,64,64,64,64,124},
    {68,108,84,84,68,68,68},
    {68,100,84,76,68,68,68},
    {56,68,68,68,68,68,56},
    {120,68,68,120,64,64,64},
    {56,68,68,68,84,72,52},
    {120,68,68,120,80,72,68},
    {60,64,64,56,4,4,120},
    {124,16,16,16,16,16,16},
    {68,68,68,68,68,68,56},
    {68,68,68,68,68,40,16},
    {68,68,68,84,84,108,68},
    {68,68,40,16,40,68,68},
    {68,68,40,16,16,16,16},
    {124,4,8,16,32,64,124},
    {56,32,32,32,32,32,56},
    {64,32,16,8,4,0,0},
    {56,4,4,4,4,4,56},
    {16,40,68,0,0,0,0},
    {0,0,0,0,0,0,124},
    {32,16,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0},
    {12,16,16,96,16,16,12},
    {16,16,16,16,16,16,16},
    {96,16,16,12,16,16,96},
    {0,36,88,0,0,0,0},
};
static const unsigned char*gfx_glyph(unsigned char ch){
    if(ch<32||ch>126)ch='?';
    return gfx_font5x7[ch-32];
}
static Value gfx_draw_text(VM*vm,int n,Value*a){
    (void)vm;
    if(n<4||n>5||a[0].t!=VINT||a[1].t!=VINT||a[2].t!=VSTR||a[3].t!=VSTR)return vb(0);
    if(!g_gfx_renderer)return vb(1); /* headless/no-SDL remains non-fatal for engine startup */
    unsigned char r,g,b;if(!gfx_parse_color(a[3].u.s,&r,&g,&b))return vb(0);
    int scale=(n==5&&a[4].t==VINT)?(int)a[4].u.i:1;if(scale<1)scale=1;if(scale>16)scale=16;
    int x=(int)a[0].u.i,y=(int)a[1].u.i;const char*txt=a[2].u.s;
    p_SDL_SetRenderDrawColor(g_gfx_renderer,r,g,b,255);
    for(size_t ci=0;txt[ci];ci++){
        unsigned char ch=(unsigned char)txt[ci];if(ch=='\n'){y+=8*scale;x=(int)a[0].u.i;continue;}
        const unsigned char*g=gfx_glyph(ch);
        for(int gy=0;gy<7;gy++)for(int gx=0;gx<5;gx++)if(g[gy]&(1u<<(4-gx)))
            for(int sy=0;sy<scale;sy++)for(int sx=0;sx<scale;sx++)p_SDL_RenderDrawPoint(g_gfx_renderer,x+gx*scale+sx,y+gy*scale+sy);
        x+=6*scale;
    }
    return vb(1);
}
static Value sec_regex_ioc(VM*vm,int n,Value*a){
    (void)vm; if(n!=1||a[0].t!=VSTR)return vn();
    Value out=vsobj(); Value ips=va(), urls=va();
    Value p1=regex_compile(NULL,1,(Value[]){vs("([0-9]{1,3}\\.){3}[0-9]{1,3}")});
    if(p1.t==VSTRUCT){Value z[2]={p1,a[0]};ips=regex_find_all(NULL,2,z);}
    Value p2=regex_compile(NULL,1,(Value[]){vs("https?://[^[:space:]]+")});
    if(p2.t==VSTRUCT){Value z[2]={p2,a[0]};urls=regex_find_all(NULL,2,z);}
    stput(out.u.st,"ips",ips); stput(out.u.st,"urls",urls);
    stput(out.u.st,"count",vi((long long)(ips.u.a->n+urls.u.a->n))); return out;
}
static Value sec_secret_scan(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vn();char*s=sec_normalize(a[0].u.s);Value hits=va();struct Sig{const char*pat;const char*id;const char*sev;}sig[]={{"[\"']?(password|passwd|pwd)[\"']?[[:space:]]*[:=][[:space:]]*[\"']?[^[:space:],;{}\"']+","password","high"},{"[\"']?(api[_-]?key|token|secret)[\"']?[[:space:]]*[:=][[:space:]]*[\"']?[^[:space:],;{}\"']+","generic-secret","high"},{"aws[_-]?secret[_-]?access[_-]?key[[:space:]]*[:=][[:space:]]*[^[:space:],;{}]+","aws-secret","critical"},{"(AKIA|ASIA)[0-9A-Z]{16}","aws-access-key-id","high"},{"gh[pousr]_[A-Za-z0-9_]{4,}","github-token","critical"},{"-----begin[a-z0-9]*privatekey-----","private-key","critical"}};for(size_t i=0;i<sizeof(sig)/sizeof(sig[0]);i++)if(sec_re(sig[i].pat,s)){Value o=vsobj();stput(o.u.st,"id",vs(sig[i].id));stput(o.u.st,"severity",vs(sig[i].sev));stput(o.u.st,"pattern",vs(sig[i].pat));stput(o.u.st,"redacted",vs("<secret-redacted>"));ap(hits.u.a,o);}xfree(s);Value r=vsobj();stput(r.u.st,"findings",hits);stput(r.u.st,"count",vi((long long)hits.u.a->n));stput(r.u.st,"normalized",vb(1));return r;}
static Value tensor_from_array(VM*,int,Value*);static Value tensor_ones(VM*,int,Value*);static Value tensor_parameter(VM*,int,Value*);static Value tensor_neg(VM*,int,Value*);static Value tensor_data(VM*,int,Value*);static Value tensor_grad(VM*,int,Value*);static Value tensor_shape(VM*,int,Value*);static Value tensor_numel(VM*,int,Value*);static Value tensor_adam_step(VM*,int,Value*);static Value tensor_info(VM*,int,Value*);static Value tensor_backward(VM*,int,Value*);static Value tensor_zero_grad(VM*,int,Value*);static Value tensor_cross_entropy(VM*,int,Value*);static Value tensor_matmul(VM*,int,Value*);static Value tensor_linear(VM*,int,Value*);static Value tensor_softmax(VM*,int,Value*);static Value tensor_layernorm(VM*,int,Value*);static Value ai_llm_info(VM*,int,Value*);static Value ai_llm_load(VM*,int,Value*);static Value ai_llm_generate(VM*,int,Value*);static Value ai_llm_close(VM*,int,Value*);static Value api_request(VM*,int,Value*);static Value api_get(VM*,int,Value*);static Value api_post_json(VM*,int,Value*);static Value api_put_json(VM*,int,Value*);static Value api_delete(VM*,int,Value*);static Value api_json(VM*,int,Value*);static Value api_batch(VM*,int,Value*);
static int namespace_put(Value ns,const char*path,Value v){
    if(ns.t!=VSTRUCT||!path||!*path)return 0;
    const char*dot=strchr(path,'.');
    if(!dot){stput(ns.u.st,path,v);return 1;}
    size_t n=(size_t)(dot-path);if(n==0||n>=128)return 0;
    char head[128];memcpy(head,path,n);head[n]=0;
    Value child=stget(ns.u.st,head);
    if(child.t==VNULL){child=vsobj();stput(ns.u.st,head,child);}
    if(child.t!=VSTRUCT)return 0;
    namespace_put(child,dot+1,v);
    stput(ns.u.st,head,child);
    return 1;
}
static void build_builtin_namespaces(VM*vm){
    size_t initial=vm->g.n;
    for(size_t i=0;i<initial;i++){
        const char*k=vm->g.v[i].k; const char*dot=strchr(k,'.'); if(!dot||dot==k) continue;
        size_t plen=(size_t)(dot-k); if(plen>=128) continue;
        char prefix[128];memcpy(prefix,k,plen);prefix[plen]=0;
        int gi=egi(&vm->g,prefix);
        Value ns;
        if(gi>=0){if(vm->g.v[gi].v.t!=VSTRUCT)continue;ns=vm->g.v[gi].v;}
        else{ns=vsobj();en(&vm->g,prefix,ns);gi=egi(&vm->g,prefix);if(gi<0)continue;}
        (void)namespace_put(ns,dot+1,vm->g.v[i].v);
        vm->g.v[gi].v=ns;
    }
}

static Value audit_event(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_DEFENSE))return cap_error(vm,CAP_DEFENSE,"audit.event");if(n<1||n>2||a[0].t!=VSTR)return vn();Value o=vsobj();stput(o.u.st,"ok",vb(1));stput(o.u.st,"event",vs(a[0].u.s));if(n==2&&a[1].t==VSTRUCT)stput(o.u.st,"data",noop_dup(a[1]));return o;}
static Value compliance_status(VM*vm,int n,Value*a){(void)a;if(!cap_allowed(vm,CAP_DEFENSE))return cap_error(vm,CAP_DEFENSE,"compliance.status");if(n)return vn();Value o=vsobj();stput(o.u.st,"sandbox_default",vb(1));stput(o.u.st,"hardware_raw_in_sandbox",vb(0));stput(o.u.st,"tls_peer_verification",vb(1));stput(o.u.st,"redteam_legal_doc_required",vb(1));stput(o.u.st,"pentest_audit_log_required",vb(1));return o;}
static Value report_json(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_DEFENSE))return cap_error(vm,CAP_DEFENSE,"report.json");if(n!=1)return vn();HJsonBuf b={0};if(!hjson_value(&b,a[0],0)){xfree(b.d);return vn();}Value r=vs(b.d?b.d:"");xfree(b.d);return r;}
