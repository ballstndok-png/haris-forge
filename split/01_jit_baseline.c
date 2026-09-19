/* ---------------- Baseline JIT (x86-64 integer arithmetic) ----------------
   Hot functions that match a deliberately small, safe bytecode shape are
   compiled to native machine code. All other functions keep using the VM. */
static void *jit_alloc(size_t n){
#ifdef __linux__
    size_t ps=(size_t)sysconf(_SC_PAGESIZE); size_t z=(n+ps-1)/ps*ps; void*p=mmap(NULL,z,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0); return p==MAP_FAILED?NULL:p;
#elif defined(_WIN32)
    return VirtualAlloc(NULL,n,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
#else
    (void)n; return NULL;
#endif
}
static int jit_make_rx(void*p,size_t n){
#ifdef __linux__
    size_t ps=(size_t)sysconf(_SC_PAGESIZE);size_t z=(n+ps-1)/ps*ps;return mprotect(p,z,PROT_READ|PROT_EXEC)==0;
#elif defined(_WIN32)
    DWORD old=0;return VirtualProtect(p,n,PAGE_EXECUTE_READ,&old)!=0;
#else
    (void)p;(void)n;return 0;
#endif
}
static void jit_free(void*p,size_t n){
    if(!p)return;
#ifdef __linux__
    size_t ps=(size_t)sysconf(_SC_PAGESIZE);size_t z=(n+ps-1)/ps*ps;munmap(p,z);
#elif defined(_WIN32)
    (void)n;VirtualFree(p,0,MEM_RELEASE);
#else
    (void)n;
#endif
}
static int jit_emit(unsigned char*b,size_t*o,const unsigned char*x,size_t n){memcpy(b+*o,x,n);*o+=n;return 1;}
static int jit_compile_i64(Fn*f){
    /* Semantic straight-line integer-expression JIT.
       The compiler normalizes commutative operands and reversible comparisons,
       so equivalent expressions do not depend on source/bytecode operand order.
       It never recognizes a function by name and never replaces an algorithm. */
    if(!f||f->np<1||f->np>6||f->ch.n<3)return 0;
    Ins *c=f->ch.v;int ret=-1;
    for(int i=0;i<f->ch.n;i++)if(c[i].op==I_RET){ret=i;break;}
    if(ret<2)return 0;
    int lk=0,rk=0,l=-1,r=-1;long long lv=0,rv=0;Op op=I_HALT;int pos=0;
    if(c[pos].op==I_GETL){lk=1;l=c[pos].a;pos++;}
    else if(c[pos].op==I_CONST&&c[pos].a>=0&&c[pos].a<f->ch.nc&&f->ch.c[c[pos].a].t==VINT){lk=2;lv=f->ch.c[c[pos].a].u.i;pos++;}
    else return 0;
    if(pos<ret&&c[pos].op==I_NEG&&lk==1&&pos+1==ret){op=I_NEG;pos++;}
    else{
        if(pos>=ret)return 0;
        if(c[pos].op==I_GETL){rk=1;r=c[pos].a;pos++;}
        else if(c[pos].op==I_CONST&&c[pos].a>=0&&c[pos].a<f->ch.nc&&f->ch.c[c[pos].a].t==VINT){rk=2;rv=f->ch.c[c[pos].a].u.i;pos++;}
        else return 0;
        if(pos>=ret)return 0;
        op=c[pos++].op;
    }
    if(op==I_NEG){return 0;}else{
        if(pos!=ret)return 0;
        if(op!=I_ADD&&op!=I_SUB&&op!=I_MUL&&op!=I_EQ&&op!=I_NEQ&&op!=I_LT&&op!=I_LTE&&op!=I_GT&&op!=I_GTE)return 0;
        /* Normalize left-constant cases without changing semantics. */
        if(lk==2&&rk==1){
            if(op==I_ADD||op==I_MUL||op==I_EQ||op==I_NEQ){int ti=l;l=r;r=ti;long long tv=lv;lv=rv;rv=tv;int tk=lk;lk=rk;rk=tk;}
            else if(op==I_LT||op==I_LTE||op==I_GT||op==I_GTE){int ti=l;l=r;r=ti;long long tv=lv;lv=rv;rv=tv;int tk=lk;lk=rk;rk=tk;Op oi=op;if(op==I_LT)oi=I_GT;else if(op==I_LTE)oi=I_GTE;else if(op==I_GT)oi=I_LT;else oi=I_LTE;op=oi;}
            else return 0;
        }
        if(lk!=1||l<0||l>=6)return 0;
        if(rk==1&&(r<0||r>=6))return 0;
    }
#if defined(__x86_64__) && !defined(_WIN32)
    unsigned char code[128];size_t o=0;
    static const unsigned char mov_from[6][3]={{0x48,0x89,0xf8},{0x48,0x89,0xf0},{0x48,0x89,0xd0},{0x48,0x89,0xc8},{0x4c,0x89,0xc0},{0x4c,0x89,0xc8}};
    static const unsigned char add_from[6][3]={{0x48,0x01,0xf8},{0x48,0x01,0xf0},{0x48,0x01,0xd0},{0x48,0x01,0xc8},{0x4c,0x01,0xc0},{0x4c,0x01,0xc8}};
    static const unsigned char sub_from[6][3]={{0x48,0x29,0xf8},{0x48,0x29,0xf0},{0x48,0x29,0xd0},{0x48,0x29,0xc8},{0x4c,0x29,0xc0},{0x4c,0x29,0xc8}};
    static const unsigned char mul_from[6][4]={{0x48,0x0f,0xaf,0xc7},{0x48,0x0f,0xaf,0xc6},{0x48,0x0f,0xaf,0xc2},{0x48,0x0f,0xaf,0xc1},{0x49,0x0f,0xaf,0xc0},{0x49,0x0f,0xaf,0xc1}};
    jit_emit(code,&o,mov_from[l],3);
    if(op==I_NEG)jit_emit(code,&o,(const unsigned char[]){0x48,0xf7,0xd8},3);
    else if(rk==2){unsigned char imm[10]={0x48,0xb9};memcpy(imm+2,&rv,8);jit_emit(code,&o,imm,10);if(op==I_ADD)jit_emit(code,&o,(const unsigned char[]){0x48,0x01,0xc8},3);else if(op==I_SUB)jit_emit(code,&o,(const unsigned char[]){0x48,0x29,0xc8},3);else if(op==I_MUL)jit_emit(code,&o,(const unsigned char[]){0x48,0x0f,0xaf,0xc1},4);else{jit_emit(code,&o,(const unsigned char[]){0x48,0x39,0xc8},3);unsigned char cc=(op==I_EQ)?0x94:(op==I_NEQ)?0x95:(op==I_LT)?0x9c:(op==I_LTE)?0x9e:(op==I_GT)?0x9f:0x9d;unsigned char sc[]={0x0f,cc,0xc0,0x48,0x0f,0xb6,0xc0};jit_emit(code,&o,sc,sizeof sc);}}
    else if(op==I_ADD)jit_emit(code,&o,add_from[r],3);
    else if(op==I_SUB)jit_emit(code,&o,sub_from[r],3);
    else if(op==I_MUL)jit_emit(code,&o,mul_from[r],4);
    else{static const unsigned char cmp_from[6][3]={{0x48,0x39,0xf8},{0x48,0x39,0xf0},{0x48,0x39,0xd0},{0x48,0x39,0xc8},{0x4c,0x39,0xc0},{0x4c,0x39,0xc8}};jit_emit(code,&o,cmp_from[r],3);unsigned char cc=(op==I_EQ)?0x94:(op==I_NEQ)?0x95:(op==I_LT)?0x9c:(op==I_LTE)?0x9e:(op==I_GT)?0x9f:0x9d;unsigned char sc[]={0x0f,cc,0xc0,0x48,0x0f,0xb6,0xc0};jit_emit(code,&o,sc,sizeof sc);}
    code[o++]=0xc3;
    void*mem=jit_alloc(o);if(!mem)return 0;memcpy(mem,code,o);if(!jit_make_rx(mem,o)){jit_free(mem,o);return 0;}f->jit_mem=mem;f->jit_size=o;f->jit_i64=(JitI64Fn)mem;JitBlock*jb=(JitBlock*)malloc(sizeof(*jb));if(jb){jb->p=mem;jb->n=o;jb->next=g_jit_blocks;g_jit_blocks=jb;}return 1;
#else
    return 0;
#endif
}

