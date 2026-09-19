/* ---------------- AI-friendly data/JSON layer ----------------
   Haris already has arrays and structs. These helpers make them usable as
   native JSON objects so AI/API code can be written as data instead of
   manually assembled JSON strings.
*/
typedef struct { char *d; size_t n, cap; } HJsonBuf;

static int hjson_grow(HJsonBuf *b, size_t extra){
    if(extra > H_DATA_MAX || b->n > H_DATA_MAX - extra) return 0;
    size_t need=b->n+extra+1;
    if(need<=b->cap) return 1;
    size_t cap=b->cap?b->cap:256;
    while(cap<need){
        if(cap>H_DATA_MAX/2){cap=H_DATA_MAX+1;break;}
        cap*=2;
    }
    if(cap>H_DATA_MAX+1) return 0;
    b->d=(char*)xrealloc(b->d,cap); b->cap=cap; return 1;
}
static int hjson_putc(HJsonBuf*b,char c){
    if(!hjson_grow(b,1)) return 0; b->d[b->n++]=c; b->d[b->n]=0; return 1;
}
static int hjson_puts(HJsonBuf*b,const char*s){
    size_t n=strlen(s); if(!hjson_grow(b,n)) return 0;
    memcpy(b->d+b->n,s,n); b->n+=n; b->d[b->n]=0; return 1;
}
static int hjson_string(HJsonBuf*b,const char*s){
    if(!hjson_putc(b,'"')) return 0;
    for(const unsigned char*p=(const unsigned char*)s;*p;p++){
        char esc[7]={0}; const char*r=NULL;
        switch(*p){
            case '"':r="\\\"";break; case '\\':r="\\\\";break;
            case '\b':r="\\b";break; case '\f':r="\\f";break;
            case '\n':r="\\n";break; case '\r':r="\\r";break;
            case '\t':r="\\t";break;
            default:
                if(*p<0x20){snprintf(esc,sizeof esc,"\\u%04x",(unsigned)*p);r=esc;}
                break;
        }
        if(r){if(!hjson_puts(b,r))return 0;}
        else if(!hjson_putc(b,(char)*p))return 0;
    }
    return hjson_putc(b,'"');
}
static int hjson_value(HJsonBuf*b,Value v,int depth);
static int hjson_value(HJsonBuf*b,Value v,int depth){
    if(depth>32) return 0;
    char num[64];
    switch(v.t){
        case VNULL:return hjson_puts(b,"null");
        case VBOOL:return hjson_puts(b,v.u.b?"true":"false");
        case VINT:snprintf(num,sizeof num,"%lld",v.u.i);return hjson_puts(b,num);
        case VFLOAT:
            if(!isfinite(v.u.f)) return hjson_puts(b,"null");
            snprintf(num,sizeof num,"%.17g",v.u.f);return hjson_puts(b,num);
        case VSTR:return hjson_string(b,v.u.s?v.u.s:"");
        case VARR:
            if(!hjson_putc(b,'['))return 0;
            for(size_t i=0;i<v.u.a->n;i++){
                if(i&&!hjson_putc(b,','))return 0;
                if(!hjson_value(b,v.u.a->v[i],depth+1))return 0;
            }
            return hjson_putc(b,']');
        case VSTRUCT: {
            Value mty=stget(v.u.st,"__type");
            if(mty.t==VSTR && !strcmp(mty.u.s,"map")){
                Value bv=stget(v.u.st,"__buckets"); if(!hjson_putc(b,'{'))return 0; int wrote=0;
                if(bv.t==VARR){
                    for(size_t bi=0;bi<bv.u.a->n;bi++){ Value bucket=bv.u.a->v[bi]; if(bucket.t!=VARR)continue;
                        for(size_t bj=0;bj<bucket.u.a->n;bj++){ Value pair=bucket.u.a->v[bj]; if(pair.t!=VARR||pair.u.a->n<2)continue;
                            Value k=pair.u.a->v[0],val=pair.u.a->v[1]; char kbuf[64]; const char*kstr;
                            if(k.t==VSTR)kstr=k.u.s; else if(k.t==VINT){snprintf(kbuf,sizeof kbuf,"%lld",k.u.i);kstr=kbuf;}
                            else if(k.t==VBOOL)kstr=k.u.b?"true":"false"; else if(k.t==VFLOAT){snprintf(kbuf,sizeof kbuf,"%.17g",k.u.f);kstr=kbuf;} else kstr="null";
                            if(wrote && !hjson_putc(b,','))return 0;
                            if(!hjson_string(b,kstr))return 0; if(!hjson_putc(b,':'))return 0;
                            if(!hjson_value(b,val,depth+1))return 0; wrote=1;
                        }
                    }
                }
                return hjson_putc(b,'}');
            }
            if(!hjson_putc(b,'{'))return 0;
            for(size_t i=0;i<v.u.st->n;i++){
                if(i&&!hjson_putc(b,','))return 0;
                if(!hjson_string(b,v.u.st->v[i].name?v.u.st->v[i].name:""))return 0;
                if(!hjson_putc(b,':'))return 0;
                if(!hjson_value(b,v.u.st->v[i].value,depth+1))return 0;
            }
            return hjson_putc(b,'}');
        }
        default:return 0;
    }
}
typedef struct { const char*s; size_t i,n; int ok; } JParser;
static void jskip(JParser*p){ while(p->i<p->n && (p->s[p->i]==' '||p->s[p->i]=='\t'||p->s[p->i]=='\n'||p->s[p->i]=='\r')) p->i++; }
static Value jparse_value(JParser*p);
static Value jparse_string(JParser*p){
    p->i++; DBuf b; dbuf_init(&b);
    while(p->i<p->n && p->s[p->i]!='"'){
        char c=p->s[p->i];
        if(c=='\\' && p->i+1<p->n){
            char e=p->s[p->i+1];
            if(e=='"'){dbuf_putc(&b,'"');p->i+=2;}
            else if(e=='\\'){dbuf_putc(&b,'\\');p->i+=2;}
            else if(e=='/'){dbuf_putc(&b,'/');p->i+=2;}
            else if(e=='n'){dbuf_putc(&b,'\n');p->i+=2;}
            else if(e=='t'){dbuf_putc(&b,'\t');p->i+=2;}
            else if(e=='r'){dbuf_putc(&b,'\r');p->i+=2;}
            else if(e=='b'){dbuf_putc(&b,'\b');p->i+=2;}
            else if(e=='f'){dbuf_putc(&b,'\f');p->i+=2;}
            else if(e=='u' && p->i+5<p->n){
                char hex[5]={p->s[p->i+2],p->s[p->i+3],p->s[p->i+4],p->s[p->i+5],0};
                unsigned cp=(unsigned)strtoul(hex,NULL,16); p->i+=6;
                if(cp<=0x7F) dbuf_putc(&b,(char)cp);
                else if(cp<=0x7FF){ dbuf_putc(&b,(char)(0xC0|(cp>>6))); dbuf_putc(&b,(char)(0x80|(cp&0x3F))); }
                else { dbuf_putc(&b,(char)(0xE0|(cp>>12))); dbuf_putc(&b,(char)(0x80|((cp>>6)&0x3F))); dbuf_putc(&b,(char)(0x80|(cp&0x3F))); }
            } else { dbuf_putc(&b,e); p->i+=2; }
        } else { dbuf_putc(&b,c); p->i++; }
    }
    if(p->i<p->n) p->i++;
    dbuf_putc(&b,0); Value v=vs(b.d); free(b.d); return v;
}
static Value jparse_number(JParser*p){
    size_t start=p->i; int isfloat=0;
    if(p->i<p->n && p->s[p->i]=='-') p->i++;
    while(p->i<p->n && isdigit((unsigned char)p->s[p->i])) p->i++;
    if(p->i<p->n && p->s[p->i]=='.'){ isfloat=1; p->i++; while(p->i<p->n && isdigit((unsigned char)p->s[p->i])) p->i++; }
    if(p->i<p->n && (p->s[p->i]=='e'||p->s[p->i]=='E')){ isfloat=1; p->i++; if(p->i<p->n&&(p->s[p->i]=='+'||p->s[p->i]=='-'))p->i++; while(p->i<p->n && isdigit((unsigned char)p->s[p->i])) p->i++; }
    size_t len=p->i-start; char buf[64]; if(len>=sizeof buf) len=sizeof buf-1; memcpy(buf,p->s+start,len); buf[len]=0;
    return isfloat? vf(atof(buf)) : vi(atoll(buf));
}
static Value jparse_array(JParser*p){
    p->i++; Value arr=va(); jskip(p);
    if(p->i<p->n && p->s[p->i]==']'){ p->i++; return arr; }
    for(;;){ jskip(p); Value v=jparse_value(p); if(!p->ok) return arr; ap(arr.u.a,v); jskip(p);
        if(p->i<p->n && p->s[p->i]==','){ p->i++; continue; }
        if(p->i<p->n && p->s[p->i]==']'){ p->i++; break; }
        p->ok=0; break; }
    return arr;
}
static Value jparse_object(JParser*p){
    p->i++; Value obj=vsobj(); jskip(p);
    if(p->i<p->n && p->s[p->i]=='}'){ p->i++; return obj; }
    for(;;){ jskip(p);
        if(p->i>=p->n || p->s[p->i]!='"'){ p->ok=0; break; }
        Value key=jparse_string(p); jskip(p);
        if(p->i>=p->n || p->s[p->i]!=':'){ p->ok=0; break; }
        p->i++; jskip(p); Value val=jparse_value(p); if(!p->ok) break;
        stput(obj.u.st,key.u.s,val); jskip(p);
        if(p->i<p->n && p->s[p->i]==','){ p->i++; continue; }
        if(p->i<p->n && p->s[p->i]=='}'){ p->i++; break; }
        p->ok=0; break; }
    return obj;
}
static Value jparse_value(JParser*p){
    jskip(p); if(p->i>=p->n){ p->ok=0; return vn(); }
    char c=p->s[p->i];
    if(c=='"') return jparse_string(p);
    if(c=='{') return jparse_object(p);
    if(c=='[') return jparse_array(p);
    if(c=='-'||isdigit((unsigned char)c)) return jparse_number(p);
    if(p->i+4<=p->n && !strncmp(p->s+p->i,"true",4)){ p->i+=4; return vb(1); }
    if(p->i+5<=p->n && !strncmp(p->s+p->i,"false",5)){ p->i+=5; return vb(0); }
    if(p->i+4<=p->n && !strncmp(p->s+p->i,"null",4)){ p->i+=4; return vn(); }
    p->ok=0; return vn();
}
static Value njson_parse(VM*vm,int n,Value*a){
    (void)vm; if(n!=1||a[0].t!=VSTR) return vn();
    JParser jp={a[0].u.s,0,strlen(a[0].u.s),1};
    Value v=jparse_value(&jp); jskip(&jp);
    if(!jp.ok) return vn();
    return v;
}
static Value njson_stringify(VM*vm,int n,Value*a){
    (void)vm; if(n!=1)return vn();
    HJsonBuf b={0}; if(!hjson_value(&b,a[0],0)){xfree(b.d);return vn();}
    Value out=vs(b.d?b.d:""); xfree(b.d); return out;
}
static Value ai_message_common(int n,Value*a,const char*role){
    if(n!=1||a[0].t!=VSTR)return vn();
    Value o=vsobj();
    stput(o.u.st,"role",vs(role));
    stput(o.u.st,"content",a[0]);
    return o;
}
static Value nai_system(VM*vm,int n,Value*a){(void)vm;return ai_message_common(n,a,"system");}
static Value nai_user(VM*vm,int n,Value*a){(void)vm;return ai_message_common(n,a,"user");}
static Value nai_assistant(VM*vm,int n,Value*a){(void)vm;return ai_message_common(n,a,"assistant");}
static Value nai_tool(VM*vm,int n,Value*a){
    (void)vm; if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VSTR)return vn();
    Value o=vsobj();stput(o.u.st,"type",vs("function"));
    Value f=vsobj();stput(f.u.st,"name",a[0]);stput(f.u.st,"description",a[1]);
    if(n==3)stput(f.u.st,"parameters",a[2]);else stput(f.u.st,"parameters",vsobj());
    stput(o.u.st,"function",f);return o;
}
static Value nai_messages(VM*vm,int n,Value*a){
    (void)vm;if(n<1||n>128)return vn();Value o=va();
    for(int i=0;i<n;i++)if(a[i].t!=VSTRUCT){return vn();}else ap(o.u.a,a[i]);
    return o;
}

/* Generic OpenAI-compatible JSON request. This deliberately accepts an
   already-structured body so Haris stays provider/model agnostic. */
static char *ai_http_post_json(const char*,const char*,const char*,long*);
static Value nai_request(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"ai.request");
    if(n<2||n>3||a[0].t!=VSTR||a[1].t!=VSTR||(n==3&&a[2].t!=VSTR))return vn();
    const char *body=(n==3?a[2].u.s:a[1].u.s),*key=(n==3?a[1].u.s:NULL);long code=0;char*r=ai_http_post_json(a[0].u.s,key,body,&code);Value o=va();ap(o.u.a,vi(code));if(!r){ap(o.u.a,vs("request failed"));return o;}ap(o.u.a,vs(r));xfree(r);return o;
}
static Value nai_chat_messages(VM*vm,int n,Value*a){
    if(n<3||n>4||a[0].t!=VSTR||a[1].t!=VSTR||a[2].t!=VARR)return vn();
    HJsonBuf b={0};
    if(!hjson_puts(&b,"{\"messages\":")){xfree(b.d);return vn();}
    if(!hjson_value(&b,a[2],0)){xfree(b.d);return vn();}
    if(n==4){
        if(a[3].t!=VSTR||!hjson_puts(&b,",\"model\":")||!hjson_string(&b,a[3].u.s)){xfree(b.d);return vn();}
    }
    if(!hjson_putc(&b,'}')){xfree(b.d);return vn();}
    Value args[3]={a[0],a[1],vs(b.d?b.d:"")};
    Value r=nai_request(vm,3,args);xfree(b.d);return r;
}

static Value nai_chat(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"ai.chat");
    if(n!=3||a[0].t!=VSTR||a[1].t!=VSTR||a[2].t!=VSTR)return vn();
    HJsonBuf b={0};
    if(!hjson_puts(&b,"{\"messages\":[{\"role\":\"user\",\"content\":" )||!hjson_string(&b,a[2].u.s)||!hjson_puts(&b,"}]}") ){xfree(b.d);return vn();}
    long code=0;char*r=ai_http_post_json(a[0].u.s,a[1].u.s,b.d,&code);xfree(b.d);
    Value out=va();ap(out.u.a,vi(code));ap(out.u.a,vs(r?r:""));if(r)xfree(r);return out;
}

