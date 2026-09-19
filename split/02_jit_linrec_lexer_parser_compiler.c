/* ---------------- Linear-recursion JIT (x86-64) ----------------
   Compiles a narrow, exactly-matched bytecode shape: a single-parameter
   function that (1) checks the parameter against a base case and returns
   either the parameter or a constant, otherwise (2) calls itself twice with
   the parameter reduced by two constants and combines the two results with
   +, -, or *. This is the classic linear-recursive shape (Fibonacci and
   relatives). The match is purely structural over opcodes/operands, exactly
   like jit_compile_loop_i64 above: any deviation bails out to the ordinary
   VM path, so this never guesses or reimplements an algorithm - it only
   gives the proven shape a native, non-recursive-VM-frame implementation.
   The two recursive calls are native `call` instructions to this same
   compiled block (self-call via a rel32 displacement computed at
   generation time, since the target is this very buffer's start), so deep
   recursion runs on the real CPU stack with the ordinary C calling
   convention - no Haris call-frame growth at all for the recursive part. */
static int jit_compile_linrec_i64(Fn*f){
#if defined(__x86_64__) && !defined(_WIN32)
    if(!f||f->np!=1||f->ch.n<19)return 0;
    Ins*c=f->ch.v;
    if(c[0].op!=I_GETL||c[0].a!=0)return 0;
    if(c[1].op!=I_CONST||c[1].a<0||c[1].a>=f->ch.nc||f->ch.c[c[1].a].t!=VINT)return 0;
    long long base_k=f->ch.c[c[1].a].u.i;
    Op cmp=c[2].op;
    if(cmp!=I_EQ&&cmp!=I_NEQ&&cmp!=I_LT&&cmp!=I_LTE&&cmp!=I_GT&&cmp!=I_GTE)return 0;
    if(c[3].op!=I_JMPF)return 0;
    int L=c[3].a;
    int base_is_param;long long base_const=0;int base_next=-1;
    if(c[4].op==I_GETL&&c[4].a==0){base_is_param=1;
        if(c[5].op==I_RET&&c[6].op==I_JMP&&c[6].a==L)base_next=7;
        else if(c[5].op==I_SETL&&c[6].op==I_GETL&&c[7].op==I_RET&&c[8].op==I_JMP&&c[8].a==L)base_next=9;
        else return 0;
    } else if(c[4].op==I_CONST&&c[4].a>=0&&c[4].a<f->ch.nc&&f->ch.c[c[4].a].t==VINT){base_is_param=0;base_const=f->ch.c[c[4].a].u.i;
        if(c[5].op==I_RET&&c[6].op==I_JMP&&c[6].a==L)base_next=7;
        else if(c[5].op==I_SETL&&c[6].op==I_GETL&&c[7].op==I_RET&&c[8].op==I_JMP&&c[8].a==L)base_next=9;
        else return 0;
    } else return 0;
    if(L!=base_next)return 0;
    int nameA=-1;long long c1v=0,c2v=0;Op comb;int endpos=L;
    if(L+10>=f->ch.n)return 0;
    /* call 1: GET self-name, GETL 0, CONST c1, SUB, CALL 1 */
    if(c[L+0].op!=I_GET)return 0; nameA=c[L+0].a;
    if(c[L+1].op!=I_GETL||c[L+1].a!=0)return 0;
    if(c[L+2].op!=I_CONST||c[L+2].a<0||c[L+2].a>=f->ch.nc||f->ch.c[c[L+2].a].t!=VINT)return 0;
    c1v=f->ch.c[c[L+2].a].u.i;
    if(c[L+3].op!=I_SUB||c[L+4].op!=I_CALL||c[L+4].a!=1)return 0;
    /* call 2: same function, GETL 0, CONST c2, SUB, CALL 1 */
    if(c[L+5].op!=I_GET||c[L+5].a!=nameA||c[L+6].op!=I_GETL||c[L+6].a!=0)return 0;
    if(c[L+7].op!=I_CONST||c[L+7].a<0||c[L+7].a>=f->ch.nc||f->ch.c[c[L+7].a].t!=VINT)return 0;
    c2v=f->ch.c[c[L+7].a].u.i;
    if(c[L+8].op!=I_SUB||c[L+9].op!=I_CALL||c[L+9].a!=1)return 0;
    comb=c[L+10].op;
    if(comb!=I_ADD&&comb!=I_SUB&&comb!=I_MUL)return 0;
    endpos=L+11;
    if(endpos<f->ch.n&&c[endpos].op==I_RET){endpos++;}
    else if(endpos+2<f->ch.n&&c[endpos].op==I_SETL&&c[endpos+1].op==I_GETL&&c[endpos+2].op==I_RET){endpos+=3;}
    else return 0;
    /* The compiler appends a final implicit return after the explicit return. */
    if(endpos+4==f->ch.n){
        if(c[endpos].op!=I_CONST||c[endpos+1].op!=I_SETL||c[endpos+2].op!=I_GETL||c[endpos+3].op!=I_RET)return 0;
        endpos+=4;
    }
    if(endpos!=f->ch.n)return 0;
    if(nameA<0||nameA>=f->ch.nn||!f->name||strcmp(f->ch.names[nameA],f->name))return 0;

    unsigned char code[192];size_t o=0;
    /* cmp rdi, imm64 via rsi scratch: movabs rsi, base_k; cmp rdi, rsi */
    {unsigned char imm[10]={0x48,0xbe};memcpy(imm+2,&base_k,8);jit_emit(code,&o,imm,10);}
    jit_emit(code,&o,(const unsigned char[]){0x48,0x39,0xf7},3); /* cmp rdi,rsi */
    /* Jcc to base_case: placeholder, patched below once base_case offset is known */
    unsigned char cc_true; /* condition-code that jumps to the BASE case:
        the base block runs when (param cmp base_k) is TRUE - exactly the
        case where I_JMPF, in the original bytecode, does NOT jump (falls
        through into the base-return code). Native mirrors that by jumping
        to base_at on a true comparison and falling through to the
        recursive block otherwise. */
    switch(cmp){
        case I_LT: cc_true=0x8c; break;  /* JL  */
        case I_LTE:cc_true=0x8e; break;  /* JLE */
        case I_GT: cc_true=0x8f; break;  /* JG  */
        case I_GTE:cc_true=0x8d; break;  /* JGE */
        case I_EQ: cc_true=0x84; break;  /* JE  */
        default:   cc_true=0x85; break;  /* I_NEQ: JNE */
    }
    size_t jcc_at=o;
    jit_emit(code,&o,(const unsigned char[]){0x0f,cc_true,0,0,0,0},6); /* jcc rel32, patched */
    /* recursive path */
    jit_emit(code,&o,(const unsigned char[]){0x48,0x83,0xec,0x18},4); /* sub rsp,24 */
    jit_emit(code,&o,(const unsigned char[]){0x48,0x89,0x3c,0x24},4); /* mov [rsp],rdi */
    if(c1v>=-(long long)INT32_MAX&&c1v<=(long long)INT32_MAX){int32_t v=(int32_t)(-c1v);unsigned char b[7]={0x48,0x8d,0xbf};memcpy(b+3,&v,4);jit_emit(code,&o,b,7);} /* lea rdi,[rdi-c1] */
    else{unsigned char imm[10]={0x48,0xbe};memcpy(imm+2,&c1v,8);jit_emit(code,&o,imm,10);jit_emit(code,&o,(const unsigned char[]){0x48,0x29,0xf7},3);} /* movabs rsi,c1; sub rdi,rsi */
    size_t call1_at=o;
    jit_emit(code,&o,(const unsigned char[]){0xe8,0,0,0,0},5); /* call rel32, patched */
    jit_emit(code,&o,(const unsigned char[]){0x48,0x89,0x44,0x24,0x08},5); /* mov [rsp+8],rax  (save r1) */
    jit_emit(code,&o,(const unsigned char[]){0x48,0x8b,0x3c,0x24},4); /* mov rdi,[rsp] (reload n) */
    if(c2v>=-(long long)INT32_MAX&&c2v<=(long long)INT32_MAX){int32_t v=(int32_t)(-c2v);unsigned char b[7]={0x48,0x8d,0xbf};memcpy(b+3,&v,4);jit_emit(code,&o,b,7);} /* lea rdi,[rdi-c2] */
    else{unsigned char imm[10]={0x48,0xbe};memcpy(imm+2,&c2v,8);jit_emit(code,&o,imm,10);jit_emit(code,&o,(const unsigned char[]){0x48,0x29,0xf7},3);}
    size_t call2_at=o;
    jit_emit(code,&o,(const unsigned char[]){0xe8,0,0,0,0},5); /* call rel32, patched (r2 -> rax) */
    jit_emit(code,&o,(const unsigned char[]){0x48,0x8b,0x4c,0x24,0x08},5); /* mov rcx,[rsp+8] (r1) */
    if(comb==I_ADD)jit_emit(code,&o,(const unsigned char[]){0x48,0x01,0xc8},3); /* add rax,rcx */
    else if(comb==I_MUL)jit_emit(code,&o,(const unsigned char[]){0x48,0x0f,0xaf,0xc1},4); /* imul rax,rcx */
    else{jit_emit(code,&o,(const unsigned char[]){0x48,0x29,0xc1},3);jit_emit(code,&o,(const unsigned char[]){0x48,0x89,0xc8},3);} /* sub rcx,rax; mov rax,rcx  (=> r1-r2) */
    jit_emit(code,&o,(const unsigned char[]){0x48,0x83,0xc4,0x18},4); /* add rsp,24 */
    jit_emit(code,&o,(const unsigned char[]){0xc3},1); /* ret */
    /* base case (falls-through target of the patched jcc) */
    size_t base_at=o;
    if(base_is_param)jit_emit(code,&o,(const unsigned char[]){0x48,0x89,0xf8},3); /* mov rax,rdi */
    else{unsigned char imm[10]={0x48,0xb8};memcpy(imm+2,&base_const,8);jit_emit(code,&o,imm,10);} /* movabs rax,base_const */
    jit_emit(code,&o,(const unsigned char[]){0xc3},1); /* ret */
    if(o>sizeof code)return 0; /* safety net, should never trigger given fixed-size emission */
    /* Patch the relative displacements now that every offset is known. */
    {int32_t rel=(int32_t)(base_at-(jcc_at+6));memcpy(code+jcc_at+2,&rel,4);}
    {int32_t rel=(int32_t)(0-(call1_at+5));memcpy(code+call1_at+1,&rel,4);}
    {int32_t rel=(int32_t)(0-(call2_at+5));memcpy(code+call2_at+1,&rel,4);}
    void*mem=jit_alloc(o);if(!mem)return 0;memcpy(mem,code,o);if(!jit_make_rx(mem,o)){jit_free(mem,o);return 0;}
    f->jit_mem=mem;f->jit_size=o;f->jit_i64=(JitI64Fn)mem;f->rec_jit_kind=1;
    JitBlock*jb=(JitBlock*)malloc(sizeof(*jb));if(jb){jb->p=mem;jb->n=o;jb->next=g_jit_blocks;g_jit_blocks=jb;}
    return 1;
#else
    return 0;
#endif
}

static long long jit_loop_eval(long long cur,long long limit,long long step,int cmp){
    if(step==0)return cur;
    if(cmp==I_LT){
        if(step>0){ if(cur>=limit)return cur; unsigned long long dist=(unsigned long long)(limit-cur-1); unsigned long long q=dist/(unsigned long long)step; unsigned long long inc=(q+1ULL)*(unsigned long long)step; if(inc>(unsigned long long)LLONG_MAX || cur>LLONG_MAX-(long long)inc) return limit; long long out=cur+(long long)inc; return out>=limit?out:limit; }
        return cur;
    }
    if(cmp==I_LTE){
        if(step>0){ if(cur>limit)return cur; unsigned long long dist=(unsigned long long)(limit-cur); unsigned long long q=dist/(unsigned long long)step; unsigned long long inc=(q+1ULL)*(unsigned long long)step; if(inc>(unsigned long long)LLONG_MAX || cur>LLONG_MAX-(long long)inc) return limit; long long out=cur+(long long)inc; return out>limit?out:out+step; }
        return cur;
    }
    return cur;
}

static long long jit_loop_eval_lt(long long cur,long long limit,long long step){return jit_loop_eval(cur,limit,step,I_LT);}
static long long jit_loop_eval_lte(long long cur,long long limit,long long step){return jit_loop_eval(cur,limit,step,I_LTE);}
static long long jit_loop_eval_lt_i64(long long cur,long long limit){return jit_loop_eval(cur,limit,1,I_LT);}
static long long jit_loop_eval_lte_i64(long long cur,long long limit){return jit_loop_eval(cur,limit,1,I_LTE);}

static int jit_accum_add_remaining(long long cur,long long limit,long long step,int cmp,long long acc,long long*out_acc,long long*out_var){
#if defined(__SIZEOF_INT128__)
    if(!out_acc||!out_var||step<=0||cur<0||acc<0||limit<0)return 0;
    __int128 n;
    if(cmp==I_LT){ if(cur>=limit){*out_acc=acc;*out_var=cur;return 1;} n=((__int128)limit-cur-1)/step+1; }
    else if(cmp==I_LTE){ if(cur>limit){*out_acc=acc;*out_var=cur;return 1;} n=((__int128)limit-cur)/step+1; }
    else return 0;
    __int128 c=cur,s=step;
    __int128 last=c+(n-1)*s;
    __int128 finalv=c+n*s;
    __int128 sum=n*c+s*n*(n-1)/2;
    __int128 finala=(__int128)acc+sum;
    if(last<0||last>LLONG_MAX||finalv>LLONG_MAX||finala>LLONG_MAX)return 0;
    *out_acc=(long long)finala;*out_var=(long long)finalv;return 1;
#else
    (void)cur;(void)limit;(void)step;(void)cmp;(void)acc;(void)out_acc;(void)out_var;return 0;
#endif
}

static int jit_match_accum_loop(Fn*f,int start,int exit,int acc,int var,int acc_local,int cmp,long long limit,long long step,int*limit_is_local,int*limit_local){
    if(!f||start<0||exit<=start||exit>f->ch.n||acc<0||var<0||step<=0)return 0;
    if(acc==var)return 0;
    if(limit_is_local)*limit_is_local=0;
    if(limit_local)*limit_local=-1;
    Ins*c=f->ch.v;
    if(acc_local){
        /* Exact optimized local form: test, acc += i, i++, backedge. */
        if(start+6>=f->ch.n||exit!=start+7)return 0;
        if(c[start].op!=I_GETL||c[start].a!=var)return 0;
        if(c[start+1].op==I_CONST){
            if(c[start+1].a<0||c[start+1].a>=f->ch.nc||f->ch.c[c[start+1].a].t!=VINT||f->ch.c[c[start+1].a].u.i!=limit)return 0;
        } else if(c[start+1].op==I_GETL){
            if(c[start+1].a<0||c[start+1].a==acc||c[start+1].a==var)return 0;
            if(limit_is_local)*limit_is_local=1;
            if(limit_local)*limit_local=c[start+1].a;
        } else return 0;
        if(c[start+2].op!=cmp||c[start+3].op!=I_JMPF||c[start+3].a!=exit)return 0;
        if(c[start+4].op!=I_LOCAL_BIN||c[start+4].a!=acc||c[start+4].b!=var||(Op)(uintptr_t)c[start+4].cache!=I_ADD)return 0;
        if(c[start+5].op!=I_INC_LOCAL||c[start+5].a!=var)return 0;
        if(c[start+6].op!=I_JMP||c[start+6].a!=start)return 0;
        return 1;
    }
    if(exit!=start+13)return 0;
    if(c[start].op!=I_GET||c[start].a!=var)return 0;
    if(c[start+1].op!=I_CONST||c[start+1].a<0||c[start+1].a>=f->ch.nc||f->ch.c[c[start+1].a].t!=VINT||f->ch.c[c[start+1].a].u.i!=limit)return 0;
    if(c[start+2].op!=cmp||c[start+3].op!=I_JMPF||c[start+3].a!=exit)return 0;
    if(c[start+4].op!=I_GET||c[start+5].op!=I_GET||c[start+5].a!=var||c[start+6].op!=I_ADD||c[start+7].op!=I_SET||c[start+7].a!=acc)return 0;
    if(c[start+8].op!=I_GET||c[start+8].a!=var||c[start+9].op!=I_CONST||c[start+9].a<0||c[start+9].a>=f->ch.nc||f->ch.c[c[start+9].a].t!=VINT||f->ch.c[c[start+9].a].u.i!=step)return 0;
    if(c[start+10].op!=I_ADD||c[start+11].op!=I_SET||c[start+11].a!=var||c[start+12].op!=I_JMP||c[start+12].a!=start)return 0;
    return 1;
}

static int jit_compile_accum_loop(Fn*f,int start,int exit,int acc,int var,int acc_local,int cmp,long long limit,long long step,int limit_is_local,int limit_local){
    if(!jit_match_accum_loop(f,start,exit,acc,var,acc_local,cmp,limit,step,NULL,NULL))return 0;
    if(acc_local){
        if(!jit_match_accum_loop(f,start,exit,acc,var,acc_local,cmp,limit,step,&limit_is_local,&limit_local))return 0;
    }
    f->loop_acc_active=1;f->loop_acc_start=start;f->loop_acc_exit=exit;f->loop_acc_name=acc;f->loop_acc_var=var;f->loop_acc_is_local=acc_local;f->loop_acc_cmp=cmp;f->loop_acc_limit=limit;f->loop_acc_step=step;f->loop_acc_limit_is_local=limit_is_local;f->loop_acc_limit_local=limit_local;return 1;
}

static int jit_compile_loop_i64(Fn*f,int start,int exit,int var_name,long long limit,long long step,int cmp){
#if defined(__x86_64__) && !defined(_WIN32)
    if(!f||f->loop_jit_i64||start<0||exit<=start||exit>f->ch.n)return 0;
    int j=exit-1,k=start;
    if(j<0||f->ch.v[j].op!=I_JMP||f->ch.v[j].a!=start)return 0;
    if(k+8!=j)return 0;
    if(f->ch.v[k].op!=I_GET||f->ch.v[k].a!=var_name)return 0;
    if(f->ch.v[k+1].op!=I_CONST||f->ch.v[k+1].a<0||f->ch.v[k+1].a>=f->ch.nc||f->ch.c[f->ch.v[k+1].a].t!=VINT)return 0;
    if(f->ch.c[f->ch.v[k+1].a].u.i!=limit)return 0;
    if((int)f->ch.v[k+2].op!=cmp)return 0;
    if(f->ch.v[k+3].op!=I_JMPF||f->ch.v[k+3].a!=exit)return 0;
    if(f->ch.v[k+4].op!=I_GET||f->ch.v[k+4].a!=var_name)return 0;
    if(f->ch.v[k+5].op!=I_CONST||f->ch.v[k+5].a<0||f->ch.v[k+5].a>=f->ch.nc||f->ch.c[f->ch.v[k+5].a].t!=VINT)return 0;
    long long step2=f->ch.c[f->ch.v[k+5].a].u.i;
    if(f->ch.v[k+6].op==I_ADD){if(step2!=step)return 0;} else return 0;
    if(step<=0)return 0;
    if(f->ch.v[k+7].op!=I_SET||f->ch.v[k+7].a!=var_name)return 0;
    if(cmp==I_LTE && limit==LLONG_MAX)return 0; /* preserve checked-integer semantics */
    /* Native specialization for the proven monotonic loop. The generated
       native block computes the same fixed point as the loop and therefore
       removes the repeated bytecode dispatch entirely. */
    if(step!=1){ f->loop_jit3_i64=(cmp==I_LT)?jit_loop_eval_lt:jit_loop_eval_lte; f->loop_start=start; f->loop_exit=exit; f->loop_var_name=var_name; f->loop_limit=limit; f->loop_step=step; f->loop_cmp=cmp; return 1; }
    unsigned char code[32];size_t o=0;
    if(cmp==I_LT){
        const unsigned char x[]={0x48,0x39,0xf7,0x48,0x89,0xf0,0x48,0x0f,0x4d,0xc7,0xc3};
        memcpy(code,x,sizeof x);o=sizeof x;
    } else if(cmp==I_LTE){
        const unsigned char x[]={0x48,0x8d,0x46,0x01,0x48,0x39,0xc7,0x48,0x0f,0x4f,0xc7,0xc3};
        memcpy(code,x,sizeof x);o=sizeof x;
    } else return 0;
    void*mem=jit_alloc(o);if(!mem)return 0;memcpy(mem,code,o);if(!jit_make_rx(mem,o)){jit_free(mem,o);return 0;}
    f->loop_mem=mem;f->loop_size=o;f->loop_jit_i64=(LoopJitFn)mem;f->loop_start=start;f->loop_exit=exit;f->loop_var_name=var_name;f->loop_is_local=0;f->loop_limit_is_local=0;f->loop_limit_local=-1;f->loop_limit=limit;f->loop_step=step;f->loop_cmp=cmp;
    JitBlock*jb=(JitBlock*)malloc(sizeof(*jb));if(jb){jb->p=mem;jb->n=o;jb->next=g_jit_blocks;g_jit_blocks=jb;}return 1;
#else
    (void)f;(void)start;(void)exit;(void)var_name;(void)limit;(void)step;(void)cmp;return 0;
#endif
}
/* Tier-2 local-loop JIT: matches only the exact loop produced by the
   optimizer for a single numeric induction variable. */
static int jit_compile_local_loop_i64(Fn*f,int start,int exit,int local,long long limit,int step,int cmp,int limit_is_local,int limit_local){
#if defined(__x86_64__) && !defined(_WIN32)
    if(!f||f->loop_jit_i64||f->loop_jit3_i64||start<0||exit<=start||exit>f->ch.n||step<=0)return 0;
    if(start+6!=exit)return 0;
    Ins*c=f->ch.v;
    if(c[start].op!=I_GETL||c[start].a!=local)return 0;
    if(limit_is_local){
        if(c[start+1].op!=I_GETL||c[start+1].a!=limit_local||limit_local<0||limit_local==local)return 0;
    }else{
        if(c[start+1].op!=I_CONST||c[start+1].a<0||c[start+1].a>=f->ch.nc||f->ch.c[c[start+1].a].t!=VINT||f->ch.c[c[start+1].a].u.i!=limit)return 0;
    }
    if(c[start+2].op!=I_LT&&c[start+2].op!=I_LTE)return 0;
    if(c[start+3].op!=I_JMPF||c[start+3].a!=exit)return 0;
    if(c[start+4].op!=I_INC_LOCAL||c[start+4].a!=local)return 0;
    if(c[start+5].op!=I_JMP||c[start+5].a!=start)return 0;
    if(cmp!=c[start+2].op)return 0;
    if(step!=1){
        f->loop_jit3_i64=(cmp==I_LT)?jit_loop_eval_lt:jit_loop_eval_lte;
        f->loop_jit_i64=NULL;
    }else{
        /* Real 2-argument native loop JIT. The local limit is NOT baked into
           the generated code: the VM loads it from the current frame and
           passes it as arg2. This is what makes `while i < n` inside a
           function genuinely native rather than merely pattern-detected. */
        unsigned char code[32];size_t o=0;
        if(cmp==I_LT){
            const unsigned char x[]={0x48,0x39,0xf7,0x48,0x89,0xf0,0x48,0x0f,0x4d,0xc7,0xc3};
            memcpy(code,x,sizeof x);o=sizeof x;
        }else if(cmp==I_LTE){
            if(limit_is_local && limit==LLONG_MAX)return 0;
            const unsigned char x[]={0x48,0x8d,0x46,0x01,0x48,0x39,0xc7,0x48,0x0f,0x4f,0xc7,0xc3};
            memcpy(code,x,sizeof x);o=sizeof x;
        }else return 0;
        void*mem=jit_alloc(o);if(!mem)return 0;memcpy(mem,code,o);
        if(!jit_make_rx(mem,o)){jit_free(mem,o);return 0;}
        f->loop_mem=mem;f->loop_size=o;f->loop_jit_i64=(LoopJitFn)mem;f->loop_jit3_i64=NULL;
        JitBlock*jb=(JitBlock*)malloc(sizeof(*jb));if(jb){jb->p=mem;jb->n=o;jb->next=g_jit_blocks;g_jit_blocks=jb;}
    }
    f->loop_start=start;f->loop_exit=exit;f->loop_var_name=local;f->loop_is_local=1;
    f->loop_limit_is_local=limit_is_local;f->loop_limit_local=limit_local;
    f->loop_limit=limit;f->loop_step=step;f->loop_cmp=cmp;
    return 1;
#else
    if(!f||f->loop_jit_i64||f->loop_jit3_i64||start<0||exit<=start||exit>f->ch.n||step<=0)return 0;
    if(start+6!=exit)return 0;
    Ins*c=f->ch.v;
    if(c[start].op!=I_GETL||c[start].a!=local)return 0;
    if(limit_is_local){
        if(c[start+1].op!=I_GETL||c[start+1].a!=limit_local||limit_local<0||limit_local==local)return 0;
    }else{
        if(c[start+1].op!=I_CONST||c[start+1].a<0||c[start+1].a>=f->ch.nc||f->ch.c[c[start+1].a].t!=VINT||f->ch.c[c[start+1].a].u.i!=limit)return 0;
    }
    if(c[start+2].op!=I_LT&&c[start+2].op!=I_LTE)return 0;
    if(c[start+3].op!=I_JMPF||c[start+3].a!=exit)return 0;
    if(c[start+4].op!=I_INC_LOCAL||c[start+4].a!=local)return 0;
    if(c[start+5].op!=I_JMP||c[start+5].a!=start)return 0;
    if(cmp!=c[start+2].op)return 0;
    if(step==1){
        /* Portable native helper; unlike the old cast, this has the exact
           LoopJitFn ABI and therefore is safe on ARM/Windows too. */
        f->loop_jit_i64=(LoopJitFn)jit_loop_eval_lt_i64;
        f->loop_jit3_i64=NULL;
    }else{
        f->loop_jit3_i64=(cmp==I_LT)?jit_loop_eval_lt:jit_loop_eval_lte;
        f->loop_jit_i64=NULL;
    }
    f->loop_start=start;f->loop_exit=exit;f->loop_var_name=local;f->loop_is_local=1;
    f->loop_limit_is_local=limit_is_local;f->loop_limit_local=limit_local;
    f->loop_limit=limit;f->loop_step=step;f->loop_cmp=cmp;
    return 1;
#endif
}

static int jit_compile_fused_local_loop_i64(Fn*f,int start,int exit,int local,long long limit,int cmp){
#if defined(__x86_64__) && !defined(_WIN32)
    if(!f||f->loop_jit_i64||f->loop_jit3_i64||start<0||exit<=start||exit>f->ch.n||start+3!=exit)return 0;
    Ins*c=f->ch.v;
    if(c[start].op!=I_LOCAL_CMP_JMPF||c[start].a!=local)return 0;
    if((int)c[start].cache_kind<=0||c[start].cache!=(void*)(uintptr_t)exit)return 0;
    int cmp2=I_EQ+(int)c[start].cache_kind-1;
    if(cmp2!=cmp||(cmp2!=I_LT&&cmp2!=I_LTE))return 0;
    if(c[start].b<0||c[start].b>=f->ch.nc||f->ch.c[c[start].b].t!=VINT||f->ch.c[c[start].b].u.i!=limit)return 0;
    if(c[start+1].op!=I_INC_LOCAL||c[start+1].a!=local)return 0;
    if(c[start+2].op!=I_JMP||c[start+2].a!=start)return 0;
    unsigned char code[32];size_t o=0;
    if(cmp==I_LT){
        const unsigned char x[]={0x48,0x39,0xf7,0x48,0x89,0xf0,0x48,0x0f,0x4d,0xc7,0xc3};
        memcpy(code,x,sizeof x);o=sizeof x;
    }else{
        if(limit==LLONG_MAX)return 0;
        const unsigned char x[]={0x48,0x8d,0x46,0x01,0x48,0x39,0xc7,0x48,0x0f,0x4f,0xc7,0xc3};
        memcpy(code,x,sizeof x);o=sizeof x;
    }
    void*mem=jit_alloc(o);if(!mem)return 0;memcpy(mem,code,o);if(!jit_make_rx(mem,o)){jit_free(mem,o);return 0;}
    f->loop_mem=mem;f->loop_size=o;f->loop_jit_i64=(LoopJitFn)mem;f->loop_jit3_i64=NULL;f->loop_start=start;f->loop_exit=exit;f->loop_var_name=local;f->loop_is_local=1;f->loop_limit_is_local=0;f->loop_limit_local=-1;f->loop_limit=limit;f->loop_step=1;f->loop_cmp=cmp;
    JitBlock*jb=(JitBlock*)malloc(sizeof(*jb));if(jb){jb->p=mem;jb->n=o;jb->next=g_jit_blocks;g_jit_blocks=jb;}return 1;
#else
    if(!f||f->loop_jit_i64||f->loop_jit3_i64||start<0||exit<=start||exit>f->ch.n||start+3!=exit)return 0;
    Ins*c=f->ch.v;
    if(c[start].op!=I_LOCAL_CMP_JMPF||c[start].a!=local)return 0;
    if((int)c[start].cache_kind<=0||c[start].cache!=(void*)(uintptr_t)exit)return 0;
    int cmp2=I_EQ+(int)c[start].cache_kind-1;
    if(cmp2!=cmp||(cmp2!=I_LT&&cmp2!=I_LTE))return 0;
    if(c[start].b<0||c[start].b>=f->ch.nc||f->ch.c[c[start].b].t!=VINT||f->ch.c[c[start].b].u.i!=limit)return 0;
    if(c[start+1].op!=I_INC_LOCAL||c[start+1].a!=local)return 0;
    if(c[start+2].op!=I_JMP||c[start+2].a!=start)return 0;
    f->loop_jit_i64=(LoopJitFn)(cmp==I_LT?jit_loop_eval_lt_i64:jit_loop_eval_lte_i64);f->loop_jit3_i64=NULL;f->loop_start=start;f->loop_exit=exit;f->loop_var_name=local;f->loop_is_local=1;f->loop_limit_is_local=0;f->loop_limit_local=-1;f->loop_limit=limit;f->loop_step=1;f->loop_cmp=cmp;return 1;
#endif
}

/* Tier-2 function optimization: only proven, straight-line integer functions are lowered
   to native code. Complex control-flow/recursive functions stay on the VM, while hot
   call sites use a monomorphic inline cache and the VM reuses frames for tail calls. */
static Value vi(long long);
static int jit_maybe_compile(Fn*f){
    if(!f||f->jit_i64||f->calls<8)return 0;
    if(jit_compile_i64(f))return 1;
    return jit_compile_linrec_i64(f);
}

static inline int jit_call_i64(Fn *f, int n, Value *a, Value *out){
    if(!f||!f->jit_i64||n!=f->np||n<1||n>6)return 0;
    long long x[6]={0,0,0,0,0,0};
    for(int i=0;i<n;i++){ if(a[i].t!=VINT) return 0; x[i]=a[i].u.i; }
    *out=vi(f->jit_i64(x[0],x[1],x[2],x[3],x[4],x[5]));
    return 1;
}
static int jit_try_loop(Fn*f,int start,int exit,int var_name,long long limit,long long step,int cmp) __attribute__((unused));
static int jit_try_loop(Fn*f,int start,int exit,int var_name,long long limit,long long step,int cmp){
    if(!f||f->loop_jit_i64)return 1;
    if(++f->loop_hits<32)return 0;
    return jit_compile_loop_i64(f,start,exit,var_name,limit,step,cmp);
}

static void xfree(void*p){if(!p)return;HEAP_LOCK();Hdr*h=NULL;if(!heap_find_ptr(p,&h)){HEAP_UNLOCK();die("Haris: invalid managed pointer passed to free");}heap_del(h);g_mem_used-=h->n;void(*dtor)(void*)=h->dtor;HEAP_UNLOCK();if(dtor)dtor(p);free(h);}
static char *xdup(const char*s){if(!s)s="";size_t n=strlen(s);if(n==SIZE_MAX)oom();n++;char*p=xmalloc(n);memcpy(p,s,n);return p;}
#ifndef H_DATA_MAX
#define H_DATA_MAX   (64ULL*1024ULL*1024ULL)
#endif
typedef struct { char*d; size_t n,cap; } DBuf;
static void dbuf_init(DBuf*b){b->cap=64;b->n=0;b->d=(char*)malloc(b->cap);}
static void dbuf_putc(DBuf*b,char c){
    if(!b)return;
    if(b->n>=H_DATA_MAX-1)die("Haris: DBuf size limit exceeded");
    size_t need=b->n+2;
    if(!b->d||b->cap<64){b->cap=64;b->d=(char*)realloc(b->d,b->cap);if(!b->d)die("Haris: DBuf allocation failed");}
    if(need>b->cap){size_t nc=b->cap;while(nc<need){if(nc>=H_DATA_MAX/2){nc=H_DATA_MAX;break;}nc*=2;}if(nc>H_DATA_MAX){die("Haris: DBuf size limit exceeded");}b->cap=nc;b->d=(char*)realloc(b->d,b->cap);if(!b->d)die("Haris: DBuf allocation failed");}
    b->d[b->n++]=c;b->d[b->n]=0; }
static void dbuf_puts(DBuf*b,const char*s){ while(*s) dbuf_putc(b,*s++); }
static void dbuf_putn(DBuf*b,const char*s,size_t n){ for(size_t k=0;k<n;k++) dbuf_putc(b,s[k]); }
/* String interpolation: "a ${expr} b" -> (string.format("a {} b", expr)). Runs once, on raw
   source text, before lex() ever sees it. Strings with no ${ inside are copied through byte
   for byte, so this cannot change the meaning of any existing Haris program. */
static char *preprocess_interpolation(const char *src){
    size_t n=strlen(src); DBuf out; dbuf_init(&out); size_t i=0;
    while(i<n){
        char c=src[i];
        if(c=='"'){
            size_t start=i; size_t j=i+1; int has_interp=0;
            while(j<n && src[j]!='"'){ if(src[j]=='\\'&&j+1<n){j+=2;continue;} if(src[j]=='$'&&j+1<n&&src[j+1]=='{')has_interp=1; j++; }
            size_t end=j;
            if(!has_interp){ dbuf_putn(&out,src+start,((end<n)?end+1:end)-start); i=(end<n)?end+1:end; continue; }
            DBuf fmt; dbuf_init(&fmt); char**exprs=NULL; size_t ecount=0,ecap=0; size_t k=start+1;
            while(k<end){
                if(src[k]=='\\'&&k+1<end){ dbuf_putc(&fmt,src[k]); dbuf_putc(&fmt,src[k+1]); k+=2; continue; }
                if(src[k]=='{'){ dbuf_puts(&fmt,"{{"); k++; continue; }
                if(src[k]=='}'){ dbuf_puts(&fmt,"}}"); k++; continue; }
                if(src[k]=='$'&&k+1<end&&src[k+1]=='{'){
                    size_t m=k+2,depth=1; int instr=0;
                    while(m<end&&depth>0){
                        char mc=src[m];
                        if(instr){ if(mc=='\\'&&m+1<end){m+=2;continue;} if(mc=='"')instr=0; m++; continue; }
                        if(mc=='"'){instr=1;m++;continue;} if(mc=='{'){depth++;m++;continue;}
                        if(mc=='}'){depth--; if(depth==0)break; m++; continue;} m++;
                    }
                    size_t exs=k+2,exe=m; size_t exlen=exe>exs?exe-exs:0;
                    char*ex=(char*)malloc(exlen+1); memcpy(ex,src+exs,exlen); ex[exlen]=0;
                    if(ecount==ecap){ecap=ecap?ecap*2:4;exprs=(char**)realloc(exprs,ecap*sizeof(char*));}
                    exprs[ecount++]=ex; dbuf_puts(&fmt,"{}"); k=(m<end)?m+1:end; continue;
                }
                dbuf_putc(&fmt,src[k]); k++;
            }
            dbuf_putc(&fmt,0);
            dbuf_puts(&out,"(string.format(\""); dbuf_putn(&out,fmt.d,strlen(fmt.d)); dbuf_putc(&out,'"');
            for(size_t z=0;z<ecount;z++){ dbuf_putc(&out,','); dbuf_puts(&out,exprs[z]); free(exprs[z]); }
            free(exprs); free(fmt.d); dbuf_puts(&out,"))");
            i=(end<n)?end+1:end; continue;
        }
        if(c=='#'){ dbuf_putc(&out,c); i++; while(i<n&&src[i]!='\n'){dbuf_putc(&out,src[i]);i++;} continue; }
        dbuf_putc(&out,c); i++;
    }
    dbuf_putc(&out,0); return out.d;
}
static MethodReg *g_methods=NULL; static char **g_struct_names=NULL; static size_t g_struct_count=0,g_struct_cap=0;
static void register_struct_name(const char*n){for(size_t i=0;i<g_struct_count;i++)if(!strcmp(g_struct_names[i],n))return;if(g_struct_count==g_struct_cap){g_struct_cap=g_struct_cap?g_struct_cap*2:16;g_struct_names=xrealloc(g_struct_names,g_struct_cap*sizeof(char*));}g_struct_names[g_struct_count++]=xdup(n);}
static int known_struct_name(const char*n){for(size_t i=0;i<g_struct_count;i++)if(!strcmp(g_struct_names[i],n))return 1;return 0;}
static void register_method(const char*type,const char*name,Fn*fn){MethodReg*m=xmalloc(sizeof(*m));m->type=xdup(type);m->name=xdup(name);m->fn=fn;m->next=g_methods;g_methods=m;}
static Fn *find_method(const char*type,const char*name){for(MethodReg*m=g_methods;m;m=m->next)if(!strcmp(m->type,type)&&!strcmp(m->name,name))return m->fn;return NULL;}
static Value vbound(Native n,Fn*f,Value self){Value v={.t=VBOUND};v.u.bound=xmalloc(sizeof(BoundCall));v.u.bound->native=n;v.u.bound->fn=f;v.u.bound->self=xmalloc(sizeof(Value));*v.u.bound->self=self;return v;}

static Value vn(void){return (Value){.t=VNULL};} static Value vi(long long x){return (Value){.t=VINT,.u.i=x};} static Value vf(double x){return (Value){.t=VFLOAT,.u.f=x};} static Value vb(int x){return (Value){.t=VBOOL,.u.b=!!x};} static Value vs(const char*s){Value v={.t=VSTR};v.u.s=xdup(s);return v;} static Value va(void){Value v={.t=VARR};v.u.a=xmalloc(sizeof(Arr));*v.u.a=(Arr){0};return v;} static Value vsobj(void){Value v={.t=VSTRUCT};v.u.st=xmalloc(sizeof(StructObj));*v.u.st=(StructObj){0};return v;} static void stput(StructObj*s,const char*k,Value v){for(size_t i=0;i<s->n;i++)if(!strcmp(s->v[i].name,k)){s->v[i].value=v;return;}if(s->n==s->cap){s->cap=s->cap?s->cap*2:8;s->v=xrealloc(s->v,s->cap*sizeof(StructField));}s->v[s->n++]=(StructField){xdup(k),v};} static Value stget(StructObj*s,const char*k){for(size_t i=0;i<s->n;i++)if(!strcmp(s->v[i].name,k))return s->v[i].value;return vn();} static int arr_reserve(Arr*a,size_t need);
static void ap(Arr*a,Value v){if(!arr_reserve(a,a->n+1))oom();a->v[a->n++]=v;}
static int truth(Value v){switch(v.t){case VNULL:return 0;case VBOOL:return v.u.b;case VINT:return v.u.i!=0;case VFLOAT:return v.u.f!=0;case VSTR:return v.u.s[0]!=0;case VARR:return v.u.a->n!=0;case VSTRUCT:return v.u.st&&v.u.st->n!=0;default:return 1;}}
static const char *type_name(Value v){switch(v.t){case VNULL:return "null";case VINT:return "int";case VFLOAT:return "float";case VBOOL:return "bool";case VSTR:return "string";case VARR:return "array";case VFN:return "function";case VNATIVE:return "native";case VHANDLE:return "handle";case VSTRUCT:return "struct";case VBOUND:return "bound-method";default:return "unknown";}}

static int isnum(Value v){return v.t==VINT||v.t==VFLOAT;} static double dn(Value v){return v.t==VINT?(double)v.u.i:v.u.f;}
static void pv(Value v){switch(v.t){case VNULL:printf("null");break;case VINT:printf("%lld",v.u.i);break;case VFLOAT:printf("%.12g",v.u.f);break;case VBOOL:printf(v.u.b?"true":"false");break;case VSTR:printf("%s",v.u.s);break;case VARR:putchar('[');for(size_t i=0;i<v.u.a->n;i++){if(i)printf(", ");pv(v.u.a->v[i]);}putchar(']');break;case VFN:printf("<function %s>",v.u.fn->name);break;case VNATIVE:printf("<native>");break;case VHANDLE:printf("<handle>");break;case VSTRUCT:printf("<struct>");break;case VBOUND:printf("<bound-method>");break;}}
static void en(Env*e,const char*k,Value v){for(int i=0;i<e->n;i++)if(!strcmp(e->v[i].k,k)){e->v[i].v=v;return;}if(e->n==e->cap){e->cap=e->cap?e->cap*2:32;e->v=xrealloc(e->v,e->cap*sizeof(Bind));}e->v[e->n]=(Bind){xdup(k),v};e->n++;}
static int egi(Env*e,const char*k){for(int i=0;i<e->n;i++)if(!strcmp(e->v[i].k,k))return i;return -1;}
static int fn_param_index(Fn*f,const char*k){if(!f)return -1;for(int i=0;i<f->np;i++)if(!strcmp(f->params[i],k))return i;return -1;}
static int fn_local_index(Fn*f,const char*k){if(!f)return -1;for(int i=0;i<f->nlocals;i++)if(!strcmp(f->locals[i],k))return f->np+i;return -1;}
static void root_visibility(Fn*f,const char*k,int vis){if(!f||!k||vis==0)return;char ***dst=vis==1?&f->exports:&f->privates;int *n=vis==1?&f->nexports:&f->nprivates;*dst=xrealloc(*dst,(size_t)(*n+1)*sizeof(char*));(*dst)[(*n)++]=xdup(k);}
static void fn_mark_export(Fn*f,const char*k){root_visibility(f,k,1);}

static int type_matches(const char *want, Value v){
    if(!want||!*want||!strcmp(want,"any"))return 1;
    if(!strcmp(want,"number"))return isnum(v);
    if(!strcmp(want,"float"))return v.t==VFLOAT||v.t==VINT;
    if(!strcmp(want,"int"))return v.t==VINT;
    if(!strcmp(want,"string"))return v.t==VSTR;
    if(!strcmp(want,"bool"))return v.t==VBOOL;
    if(!strcmp(want,"array"))return v.t==VARR;
    if(!strcmp(want,"struct"))return v.t==VSTRUCT;
    return !strcmp(type_name(v),want);
}
static const char *generic_base_type(Fn*f,const char*ty){
    if(!f||!ty)return NULL; for(int i=0;i<f->ngens;i++)if(!strcmp(f->gens[i],ty))return f->gens[i]; return NULL;
}
static Fn *generic_specialize(Fn*f,const char **specs,int ns){
    if(!f||f->ngens!=ns)return NULL;
    for(int i=0;i<ns;i++)if(!specs[i]||!*specs[i])return NULL;
    size_t name_cap=strlen(f->name)+3;for(int i=0;i<ns;i++){size_t z=strlen(specs[i]);if(z>SIZE_MAX-name_cap-2)oom();name_cap+=z+1;}if(name_cap>H_DATA_MAX)die("Haris generic: specialization name too large");Fn *c=xmalloc(sizeof(*c)); memcpy(c,f,sizeof(*c)); c->name=xmalloc(name_cap); c->params=NULL; c->param_types=NULL; c->locals=NULL; c->gens=NULL; c->gen_constraints=NULL; c->spec_types=NULL; c->closure=f->closure;
    size_t name_len=(size_t)snprintf(c->name,name_cap,"%s<",f->name);for(int i=0;i<ns;i++){if(i)c->name[name_len++]=',';size_t z=strlen(specs[i]);memcpy(c->name+name_len,specs[i],z);name_len+=z;}c->name[name_len++]='>';c->name[name_len]=0;
    if(f->np){c->params=xmalloc((size_t)f->np*sizeof(char*));c->param_types=xmalloc((size_t)f->np*sizeof(char*));for(int i=0;i<f->np;i++){c->params[i]=f->params[i];c->param_types[i]=f->param_types?f->param_types[i]:NULL;}}
    if(f->nlocals){c->locals=xmalloc((size_t)f->nlocals*sizeof(char*));memcpy(c->locals,f->locals,(size_t)f->nlocals*sizeof(char*));}
    c->gens=f->gens;c->gen_constraints=f->gen_constraints;c->spec_types=xmalloc((size_t)ns*sizeof(char*));for(int i=0;i<ns;i++)c->spec_types[i]=xdup(specs[i]);c->nspec=ns; return c;
}
static int generic_infer_call(Fn*f,int n,Value*a,const char **out,int max){
    if(!f||f->ngens<=0||f->ngens>max)return 0;
    for(int i=0;i<f->ngens;i++)out[i]=NULL;
    for(int pi=0;pi<f->np&&pi<n;pi++){
        const char *pty=f->param_types?f->param_types[pi]:NULL;
        if(!pty)continue;
        for(int gi=0;gi<f->ngens;gi++) if(!strcmp(pty,f->gens[gi])){
            const char *actual=type_name(a[pi]);
            if(out[gi] && strcmp(out[gi],actual)){
                for(int k=0;k<f->ngens;k++)if(out[k])xfree((void*)out[k]);
                return 0;
            }
            if(!out[gi])out[gi]=xdup(actual);
        }
    }
    for(int gi=0;gi<f->ngens;gi++) if(!out[gi]){
        for(int k=0;k<f->ngens;k++)if(out[k])xfree((void*)out[k]);
        return 0;
    }
    return 1;
}

static int generic_validate_call(Fn*f,int n,Value*a){
    if(!f||f->ngens<=0)return 1;
    if(f->nspec!=f->ngens)return 0;
    for(int gi=0;gi<f->ngens;gi++){
        const char *spec=f->spec_types[gi]; if(f->gen_constraints&&f->gen_constraints[gi]&&*f->gen_constraints[gi]){int found=0;for(int pi=0;pi<f->np;pi++)if(f->param_types&&f->param_types[pi]&&!strcmp(f->param_types[pi],f->gens[gi])){if(pi<n&&type_matches(f->gen_constraints[gi],a[pi]))found=1;else return 0;}if(!found)return 0;}
        for(int pi=0;pi<f->np&&pi<n;pi++)if(f->param_types&&f->param_types[pi]&&!strcmp(f->param_types[pi],f->gens[gi])){if(!type_matches(spec,a[pi]))return 0;}
    } return 1;
}

static int fn_ensure_local(Fn*f,const char*k){int i=fn_local_index(f,k);if(i>=0)return i;int p=fn_param_index(f,k);if(p>=0)return p;f->locals=xrealloc(f->locals,(size_t)(f->nlocals+1)*sizeof(char*));f->locals[f->nlocals]=xdup(k);return f->np+f->nlocals++;}
static Value env_get_chain(Env*e,const char*k){for(Env*q=e;q;q=q->p){int i=egi(q,k);if(i>=0)return q->v[i].v;}return vn();}
static int env_lookup_chain(Env*e,const char*k,Value*out){if(out)*out=vn();for(Env*q=e;q;q=q->p){int i=egi(q,k);if(i>=0){if(out)*out=q->v[i].v;return 1;}}return 0;}
static int env_has_chain(Env*e,const char*k){return env_lookup_chain(e,k,NULL);}
static void tpush(TV*t,Tok x){if(t->n==t->cap){t->cap=t->cap?t->cap*2:128;t->v=xrealloc(t->v,t->cap*sizeof(Tok));}t->v[t->n++]=x;}
static TT kw(const char*s){const char*k[]={"if","else","while","fn","async","await","struct","enum","match","case","return","let","true","false","null","and","or","not","import","trait","try","catch","for","in","break","continue","throw","finally","export","private","from","defer"};TT tv[]={TIF,TELSE,TWHILE,TFN,TASYNC,TAWAIT,TSTRUCT,TENUM,TMATCH,TCASE,TRETURN,TLET,TTRUE,TFALSE,TNULL,TAND,TOR,TNOT,TIMPORT,TTRAIT,TTRY,TCATCH,TFOR,TIN,TBREAK,TCONT,TTHROW,TFINALLY,TEXPORT,TPRIVATE,TFROM,TDEFER};for(int i=0;i<32;i++)if(!strcmp(s,k[i]))return tv[i];return TID;}
static TV lex(const char*s){TV t={0};int line=1;size_t i=0;int bdepth=0;while(s[i]){char c=s[i];if(c==' '||c=='\t'||c=='\r'){i++;continue;}if(c=='\n'){i++;line++;if(bdepth<=0)tpush(&t,(Tok){TNL,xdup("\\n"),line-1});continue;}if(c=='#'){while(s[i]&&s[i]!='\n')i++;continue;}if(isalpha((unsigned char)c)||c=='_'){size_t st=i++;while(isalnum((unsigned char)s[i])||s[i]=='_')i++;char*w=xdup(" ");xfree(w);w=xmalloc(i-st+1);memcpy(w,s+st,i-st);w[i-st]=0;tpush(&t,(Tok){kw(w),w,line});continue;}if(isdigit((unsigned char)c)){size_t st=i++;int dot=0;while(isdigit((unsigned char)s[i])||(!dot&&s[i]=='.')){if(s[i]=='.')dot=1;i++;}char*w=xmalloc(i-st+1);memcpy(w,s+st,i-st);w[i-st]=0;tpush(&t,(Tok){dot?TFLOAT:TINT,w,line});continue;}if(c=='"'){int ln=line;i++;size_t cap=64,n=0;char*b=xmalloc(cap);while(s[i]&&s[i]!='"'){char q=s[i++];if(q=='\\'&&s[i]){char e=s[i++];q=e=='n'?'\n':e=='t'?'\t':e=='r'?'\r':e=='"'?'"':'\\';}if(n+2>cap){cap*=2;b=xrealloc(b,cap);}b[n++]=q;}if(s[i]!='"')die("Haris: unterminated string at line %d",ln);i++;b[n]=0;tpush(&t,(Tok){TSTR,b,ln});continue;}TT z=TEOF;int two=0;switch(c){case '+':z=TPLUS;break;case '-':z=TMINUS;break;case '*':z=TSTAR;break;case '/':z=TSLASH;break;case '%':z=TPCT;break;case '(':z=TLP;break;case ')':z=TRP;break;case '{':z=TLB;break;case '}':z=TRB;break;case '[':z=TLS;break;case ']':z=TRS;break;case ',':z=TCOM;break;case ';':z=TSEM;break;case ':':z=TCOLON;break;case '.':z=TDOT;break;case '=':z=TEQ;if(s[i+1]=='='){z=TEQEQ;two=1;}break;case '!':z=TNEQ;if(s[i+1]=='=')two=1;break;case '<':z=TLT;if(s[i+1]=='='){z=TLTE;two=1;}break;case '>':z=TGT;if(s[i+1]=='='){z=TGTE;two=1;}break;case '?':z=TQUESTION;if(s[i+1]=='.'){z=TQDOT;two=1;}break;default:die("Haris: unexpected '%c' line %d",c,line);}if(z==TLP||z==TLS)bdepth++;else if((z==TRP||z==TRS)&&bdepth>0)bdepth--;size_t ln=two?2:1;char*w=xmalloc(ln+1);memcpy(w,s+i,ln);w[ln]=0;tpush(&t,(Tok){z,w,line});i+=ln;}tpush(&t,(Tok){TEOF,xdup(""),line});return t;}
static Tok*cur(P*p){return&p->tv->v[p->p];} static int mt(P*p,TT t){if(cur(p)->t==t){p->p++;return 1;}return 0;} static void need(P*p,TT t,const char*s){if(!mt(p,t))die("Haris parse error line %d: expected %s",cur(p)->line,s);}
static void emit(P*p,Op op,int a,int line){Chunk*c=p->ch;if(c->n==c->cap){c->cap=c->cap?c->cap*2:64;c->v=xrealloc(c->v,c->cap*sizeof(Ins));}c->v[c->n++]=(Ins){.op=op,.a=a,.b=0,.line=line,.cache=NULL,.cache_kind=0};}
static void emit_b(P*p,Op op,int a,int b,int line){Chunk*c=p->ch;if(c->n==c->cap){c->cap=c->cap?c->cap*2:64;c->v=xrealloc(c->v,c->cap*sizeof(Ins));}c->v[c->n++]=(Ins){.op=op,.a=a,.b=b,.line=line,.cache=NULL,.cache_kind=0};} static int con(P*p,Value v){Chunk*c=p->ch;if(c->nc==c->cc){c->cc=c->cc?c->cc*2:64;c->c=xrealloc(c->c,c->cc*sizeof(Value));}c->c[c->nc]=v;return c->nc++;} static int name(P*p,const char*s){for(int i=0;i<p->ch->nn;i++)if(!strcmp(p->ch->names[i],s))return i;if(p->ch->nn==p->ch->cn){p->ch->cn=p->ch->cn?p->ch->cn*2:32;p->ch->names=xrealloc(p->ch->names,p->ch->cn*sizeof(char*));}p->ch->names[p->ch->nn]=xdup(s);return p->ch->nn++;}
static void match_value(P*p){Tok*t=cur(p);if(mt(p,TINT)){emit(p,I_CONST,con(p,vi(strtoll(t->s,0,10))),t->line);return;}if(mt(p,TSTR)){emit(p,I_CONST,con(p,vs(t->s)),t->line);return;}if(mt(p,TTRUE)){emit(p,I_CONST,con(p,vb(1)),t->line);return;}if(mt(p,TFALSE)){emit(p,I_CONST,con(p,vb(0)),t->line);return;}if(mt(p,TNULL)){emit(p,I_CONST,con(p,vn()),t->line);return;}if(mt(p,TID)){emit(p,I_GET,name(p,t->s),t->line);return;}die("Haris parse error line %d: invalid match pattern",t->line);}
static void skip_nl(P*p); static void expr(P*p,int min);
/* An identifier followed by `{` can mean either an object literal or a block
   belonging to the surrounding statement, e.g. `while x < n { ... }`.
   Only treat it as an object literal when the contents have object-field
   syntax (`name: value`) (or the object is explicitly empty). This removes
   the ambiguity that previously broke while/if conditions ending in an
   identifier or function parameter. */
static int looks_like_object_literal(P*p){
    int i=p->p;
    if(i>=p->tv->n || p->tv->v[i].t!=TLB) return 0;
    i++;
    while(i<p->tv->n && (p->tv->v[i].t==TNL || p->tv->v[i].t==TSEM)) i++;
    if(i<p->tv->n && p->tv->v[i].t==TRB) return 1;
    if(i+1<p->tv->n && p->tv->v[i].t==TID && p->tv->v[i+1].t==TCOLON) return 1;
    return 0;
}
static void optimize_bytecode(Fn*f); static void stmt(P*p); static Fn *parse_fn_literal(P *p, int line){
    Fn *f=xmalloc(sizeof(Fn)); memset(f,0,sizeof *f); f->name=xdup("<lambda>");
    need(p,TLP,"(");
    if(cur(p)->t!=TRP){ do { Tok*u=cur(p); need(p,TID,"parameter"); f->params=xrealloc(f->params,(f->np+1)*sizeof(char*)); f->param_types=xrealloc(f->param_types,(f->np+1)*sizeof(char*)); f->params[f->np]=xdup(u->s); f->param_types[f->np]=NULL; f->np++; if(mt(p,TCOLON)){Tok*ty=cur(p);need(p,TID,"parameter type");f->param_types[f->np-1]=xdup(ty->s);} } while(mt(p,TCOM)); }
    need(p,TRP,")");
    P q=*p; q.ch=&f->ch; q.fn=f; q.is_root=0; q.break_sp=q.continue_sp=0; skip_nl(&q); need(&q,TLB,"{");
    while(cur(&q)->t!=TRB&&cur(&q)->t!=TEOF) stmt(&q);
    need(&q,TRB,"}"); emit(&q,I_CONST,con(&q,vn()),line); emit(&q,I_RET,0,line); optimize_bytecode(f); p->p=q.p; return f;
}
static Value regex_compile(VM*,int,Value*); static Value regex_find_all(VM*,int,Value*); static Value regex_match(VM*,int,Value*); static Value regex_search(VM*,int,Value*); static Value datetime_now(VM*,int,Value*); static Value datetime_format(VM*,int,Value*); static Value datetime_add_days(VM*,int,Value*); static Value datetime_add_seconds(VM*,int,Value*); static Value datetime_diff(VM*,int,Value*); static Value gfx_window(VM*,int,Value*); static Value gfx_draw_circle(VM*,int,Value*); static Value gfx_draw_text(VM*,int,Value*); static Value gfx_is_open(VM*,int,Value*); static Value gfx_clear(VM*,int,Value*); static Value gfx_display(VM*,int,Value*); static Value gfx_close(VM*,int,Value*); static Value sec_regex_ioc(VM*,int,Value*); static Value sec_secret_scan(VM*,int,Value*);
static void stmt(P*p); static Fn *parse_fn_literal(P *p,int line); static void primary(P*p){Tok*t=cur(p);if(mt(p,TLB)){skip_nl(p);int fields=0;while(cur(p)->t!=TRB&&cur(p)->t!=TEOF){Tok*fnm=cur(p);need(p,TID,"field name");need(p,TCOLON,":");emit(p,I_CONST,con(p,vs(fnm->s)),fnm->line);expr(p,0);fields++;skip_nl(p);if(!mt(p,TCOM))break;skip_nl(p);}need(p,TRB,"}");emit(p,I_STRUCT,fields,t->line);return;}if(mt(p,TASYNC)){emit(p,I_GET,name(p,"async"),t->line);return;}if(mt(p,TAWAIT)){emit(p,I_GET,name(p,"async.await"),t->line);expr(p,7);emit(p,I_CALL,1,t->line);return;}if(mt(p,TFN)){ Fn*f=parse_fn_literal(p,t->line); emit(p,I_CONST,con(p,(Value){.t=VFN,.u.fn=f}),t->line); emit(p,I_MAKE_CLOSURE,0,t->line); return; } if(mt(p,TINT)){emit(p,I_CONST,con(p,vi(strtoll(t->s,0,10))),t->line);return;}if(mt(p,TFLOAT)){emit(p,I_CONST,con(p,vf(strtod(t->s,0))),t->line);return;}if(mt(p,TSTR)){emit(p,I_CONST,con(p,vs(t->s)),t->line);return;}if(mt(p,TTRUE)){emit(p,I_CONST,con(p,vb(1)),t->line);return;}if(mt(p,TFALSE)){emit(p,I_CONST,con(p,vb(0)),t->line);return;}if(mt(p,TNULL)){emit(p,I_CONST,con(p,vn()),t->line);return;}if(mt(p,TID)){char q[512];snprintf(q,sizeof q,"%s",t->s);if(cur(p)->t==TLB && looks_like_object_literal(p)){int is_prompt=!strcmp(q,"prompt");if(is_prompt)emit(p,I_GET,name(p,"ai.prompt"),t->line);p->p++;skip_nl(p);int fields=0;if(known_struct_name(q)){emit(p,I_CONST,con(p,vs("__type")),t->line);emit(p,I_CONST,con(p,vs(q)),t->line);fields++;}while(cur(p)->t!=TRB&&cur(p)->t!=TEOF){Tok*fnm=cur(p);need(p,TID,"field name");need(p,TCOLON,":");emit(p,I_CONST,con(p,vs(fnm->s)),fnm->line);expr(p,0);fields++;skip_nl(p);if(!mt(p,TCOM))break;skip_nl(p);}need(p,TRB,"}");emit(p,I_STRUCT,fields,t->line);if(!strcmp(q,"prompt")){emit(p,I_CALL,1,t->line);}return;}if(p->fn&&!p->is_root){int li=fn_local_index(p->fn,q);if(li>=0){emit(p,I_GETL,li,t->line);return;}int pi=fn_param_index(p->fn,q);if(pi>=0){emit(p,I_GETL,pi,t->line);return;}}emit(p,I_GET,name(p,q),t->line);return;}if(mt(p,TLP)){expr(p,0);need(p,TRP,")");return;}if(mt(p,TLS)){int n=0;skip_nl(p);while(cur(p)->t!=TRS&&cur(p)->t!=TEOF){expr(p,0);n++;skip_nl(p);if(mt(p,TCOM)){skip_nl(p);if(cur(p)->t==TRS)break;continue;}break;}need(p,TRS,"]");emit(p,I_ARRAY,n,t->line);return;}die("Haris parse error line %d: expected expression",t->line);}
static void skip_nl(P*p){while(cur(p)->t==TNL||cur(p)->t==TSEM)p->p++;}
static int stmt_end(P*p){return cur(p)->t==TNL||cur(p)->t==TSEM||cur(p)->t==TRB||cur(p)->t==TEOF;}
static int bp(TT t){switch(t){case TDOT:case TQDOT:return 8;case TOR:return 1;case TAND:return 2;case TEQEQ:case TNEQ:return 3;case TLT:case TLTE:case TGT:case TGTE:return 4;case TPLUS:case TMINUS:return 5;case TSTAR:case TSLASH:case TPCT:return 6;case TLP:case TLS:return 8;default:return 0;}}static Op bop(TT t){switch(t){case TPLUS:return I_ADD;case TMINUS:return I_SUB;case TSTAR:return I_MUL;case TSLASH:return I_DIV;case TPCT:return I_MOD;case TEQEQ:return I_EQ;case TNEQ:return I_NEQ;case TLT:return I_LT;case TLTE:return I_LTE;case TGT:return I_GT;case TGTE:return I_GTE;case TAND:return I_AND;case TOR:return I_OR;default:return I_HALT;}}
static void expr(P*p,int min){
    Tok*t=cur(p);
    if(mt(p,TMINUS)){expr(p,7);emit(p,I_NEG,0,t->line);}
    else if(mt(p,TNOT)){expr(p,7);emit(p,I_NOT,0,t->line);}
    else primary(p);
    while(1){
        TT tt=cur(p)->t;int b=bp(tt);if(b<=min)break;
        if(tt==TAND){
            int ln=cur(p)->line; p->p++;
            int jf=p->ch->n; emit(p,I_JMPF,0,ln);
            expr(p,b);
            int je=p->ch->n; emit(p,I_JMP,0,ln);
            int false_ip=p->ch->n; emit(p,I_CONST,con(p,vb(0)),ln);
            p->ch->v[jf].a=false_ip; p->ch->v[je].a=p->ch->n;
            continue;
        }
        if(tt==TOR){
            int ln=cur(p)->line; p->p++;
            int jf=p->ch->n; emit(p,I_JMPF,0,ln);
            emit(p,I_CONST,con(p,vb(1)),ln);
            int je=p->ch->n; emit(p,I_JMP,0,ln);
            int rhs_ip=p->ch->n; expr(p,b);
            p->ch->v[jf].a=rhs_ip; p->ch->v[je].a=p->ch->n;
            continue;
        }
        if(tt==TLT){
            int save=p->p; p->p++; const char*specs[16]; int ns=0; int ok=1;
            do{ if(cur(p)->t!=TID){ok=0;break;} if(ns<16)specs[ns++]=cur(p)->s; p->p++; }while(mt(p,TCOM));
            if(!mt(p,TGT)||cur(p)->t!=TLP){
                p->p=save; p->p++; expr(p,b); emit(p,I_LT,0,t->line); continue;
            }
            int cv=con(p,vs(specs[0])); (void)cv; for(int si=1;si<ns;si++){}
            DBuf specbuf;dbuf_init(&specbuf);for(int si=0;si<ns;si++){if(si)dbuf_putc(&specbuf,',');dbuf_puts(&specbuf,specs[si]);}int sc=con(p,vs(specbuf.d));free(specbuf.d); p->p++; int ln= t->line; int n=0;if(cur(p)->t!=TRP){do{expr(p,0);n++;}while(mt(p,TCOM));}need(p,TRP,")");emit_b(p,I_CALL,n,sc+1,ln);continue;
        }
        if(tt==TLP){int ln=cur(p)->line;p->p++;int n=0;if(cur(p)->t!=TRP){do{expr(p,0);n++;}while(mt(p,TCOM));}need(p,TRP,")");emit(p,I_CALL,n,ln);continue;}
        if(tt==TLS){int ln=cur(p)->line;p->p++;expr(p,0);need(p,TRS,"]");emit(p,I_INDEX,0,ln);continue;}
        if(tt==TDOT){int ln=cur(p)->line;p->p++;Tok*u=cur(p);if(u->t!=TID && !(u->t>=TIF&&u->t<=TFROM))die("Haris parse error line %d: expected field name",u->line);p->p++;emit(p,I_CONST,con(p,vs(u->s)),ln);emit(p,I_FIELD,0,ln);continue;}
        if(tt==TQDOT){
            /* obj?.field : if obj is null, short-circuits to null without touching the field (also safely
               propagates through further chained accesses, since each ?. independently re-checks its own
               left-hand value for null). Uses I_JMPNULL, which peeks the stack instead of popping, so the
               null case needs nothing pushed and the non-null case falls through to a normal field read. */
            int ln=cur(p)->line;p->p++;Tok*u=cur(p);need(p,TID,"field name");
            int jn=p->ch->n;emit(p,I_JMPNULL,0,ln);
            emit(p,I_CONST,con(p,vs(u->s)),ln);emit(p,I_FIELD,0,ln);
            p->ch->v[jn].a=p->ch->n;
            continue;
        }
        p->p++;expr(p,b);emit(p,bop(tt),0,t->line);
    }
    if(min==0 && cur(p)->t==TQUESTION){
        /* Ternary `cond ? a : b`, compiled the same way as if/else: only the taken branch executes. */
        int ln=cur(p)->line; p->p++;
        int jf=p->ch->n; emit(p,I_JMPF,0,ln);
        expr(p,0);
        int je=p->ch->n; emit(p,I_JMP,0,ln);
        need(p,TCOLON,":");
        p->ch->v[jf].a=p->ch->n;
        expr(p,0);
        p->ch->v[je].a=p->ch->n;
    }
}

static void emit_defers(P*p){ if(!p||!p->fn)return; for(int i=p->defer_n-1;i>=0;i--){int s=p->defer_slots[i];emit(p,I_GETL,s,0);int jf=p->ch->n;emit(p,I_JMPF,0,0);emit(p,I_GETL,s,0);emit(p,I_CALL,0,0);p->ch->v[jf].a=p->ch->n;emit(p,I_POP,0,0);} }
static int looks_like_assignment(P*p){
  int i=p->p; if(i>=p->tv->n||p->tv->v[i].t!=TID)return 0; i++;
  for(;;){
    if(i<p->tv->n&&p->tv->v[i].t==TDOT){i++;if(i>=p->tv->n||p->tv->v[i].t!=TID)return 0;i++;continue;}
    if(i<p->tv->n&&p->tv->v[i].t==TLS){int d=1;i++;while(i<p->tv->n&&d){if(p->tv->v[i].t==TLS)d++;else if(p->tv->v[i].t==TRS)d--;i++;}if(d)return 0;continue;}
    return i<p->tv->n&&p->tv->v[i].t==TEQ;
  }
}
static void emit_lvalue_load(P*p){
  Tok*n=cur(p); need(p,TID,"assignment target");
  if(p->fn&&!p->is_root){int li=fn_local_index(p->fn,n->s);int pi=fn_param_index(p->fn,n->s);if(li>=0)emit(p,I_GETL,li,n->line);else if(pi>=0)emit(p,I_GETL,pi,n->line);else emit(p,I_GET,name(p,n->s),n->line);}else emit(p,I_GET,name(p,n->s),n->line);
  while(1){
    if(mt(p,TLS)){int ln=cur(p-1)->line;expr(p,0);need(p,TRS,"]");emit(p,I_INDEX,0,ln);continue;}
    if(mt(p,TDOT)){Tok*u=cur(p);need(p,TID,"field name");emit(p,I_CONST,con(p,vs(u->s)),u->line);emit(p,I_FIELD,0,u->line);continue;}
    break;
  }
}
static void emit_lvalue_set(P*p){
  /* The final lvalue is left as (container,key) by recompiling the last access. */
  (void)p;
}
static void stmt(P*p){
  skip_nl(p);
  Tok*t=cur(p);
  if(mt(p,TEXPORT)){ if(!p->is_root){} p->visibility_pending=1; t=cur(p); } else if(mt(p,TPRIVATE)){ p->visibility_pending=2; t=cur(p); }
  if(mt(p,TDEFER)){
    if(!p->fn)die("Haris parse error line %d: defer outside function",t->line);
    Fn*d=xmalloc(sizeof(Fn));memset(d,0,sizeof*d);d->name=xdup("<defer>");d->is_root=0;P q=*p;q.ch=&d->ch;q.fn=d;q.is_root=0;q.root_fn=p->root_fn;skip_nl(&q);need(&q,TLB,"{");while(cur(&q)->t!=TRB&&cur(&q)->t!=TEOF)stmt(&q);need(&q,TRB,"}");emit(&q,I_CONST,con(&q,vn()),t->line);emit(&q,I_RET,0,t->line);p->p=q.p;
    char nm[64];snprintf(nm,sizeof nm,"__defer_%d",p->defer_n);int slot=fn_ensure_local(p->fn,nm);if(p->defer_n==p->defer_cap){p->defer_cap=p->defer_cap?p->defer_cap*2:8;p->defer_slots=xrealloc(p->defer_slots,(size_t)p->defer_cap*sizeof(int));}p->defer_slots[p->defer_n++]=slot;emit(p,I_CONST,con(p,(Value){.t=VFN,.u.fn=d}),t->line);emit(p,I_MAKE_CLOSURE,0,t->line);emit(p,I_SETL,slot,t->line);skip_nl(p);return;
  }
  if(mt(p,TIMPORT)){
    Tok*m=cur(p); if(m->t!=TID&&m->t!=TSTR)die("expected module after import"); p->p++; emit(p,I_GET,name(p,"__import"),t->line); emit(p,I_CONST,con(p,vs(m->s)),t->line); emit(p,I_CALL,1,t->line);
    if(m->t==TSTR){const char*base=strrchr(m->s,'/');base=base?base+1:m->s;char alias[256];snprintf(alias,sizeof alias,"%s",base);char*dot=strrchr(alias,'.');if(dot&&!strcmp(dot,".hr"))*dot=0;emit(p,I_SET,name(p,alias),t->line);} else emit(p,I_SET,name(p,m->s),t->line); skip_nl(p); return;
  }
  if(mt(p,TFROM)){
    Tok*m=cur(p); if(m->t!=TID&&m->t!=TSTR)die("expected module after from"); p->p++; if(cur(p)->t==TIMPORT)p->p++; else die("expected import after from module");
    char tmpn[64];static unsigned impid=0;snprintf(tmpn,sizeof tmpn,"__import_mod_%u",impid++);emit(p,I_GET,name(p,"__import"),t->line);emit(p,I_CONST,con(p,vs(m->s)),t->line);emit(p,I_CALL,1,t->line);emit(p,I_SET,name(p,tmpn),t->line);
    while(1){Tok*x=cur(p);need(p,TID,"import name");emit(p,I_GET,name(p,tmpn),x->line);emit(p,I_CONST,con(p,vs(x->s)),x->line);emit(p,I_INDEX,0,x->line);emit(p,I_SET,name(p,x->s),x->line);skip_nl(p);if(!mt(p,TCOM))break;skip_nl(p);}skip_nl(p);return;
  }
  if(looks_like_assignment(p)){
    Tok*base=cur(p); int base_line=base->line; need(p,TID,"assignment target");
    int li=-1,pi=-1;
    if(p->fn&&!p->is_root){li=fn_local_index(p->fn,base->s);pi=fn_param_index(p->fn,base->s);}
    int last_op=0,last_line=base_line;
    /* BUGFIX (stack leak): only push the base value onto the operand stack
       when there is an actual [..] or .field chain to resolve for an
       indexed/field assignment target. A plain `x = expr` never needs the
       old value of x on the stack, so emitting I_GET/I_GETL unconditionally
       here left one orphaned slot on every simple assignment, silently
       filling the fixed-size value stack (VM.st[8192]) until it overflowed
       after ~8192 assignments -- e.g. any loop with `i = i + 1` in its body. */
    if(cur(p)->t==TLS||cur(p)->t==TDOT){
      if(p->fn&&!p->is_root){if(li>=0)emit(p,I_GETL,li,base_line);else if(pi>=0)emit(p,I_GETL,pi,base_line);else emit(p,I_GET,name(p,base->s),base_line);}else emit(p,I_GET,name(p,base->s),base_line);
      while(cur(p)->t==TLS||cur(p)->t==TDOT){
        if(cur(p)->t==TLS){last_op=1;last_line=cur(p)->line;p->p++;expr(p,0);need(p,TRS,"]");}
        else {last_op=2;last_line=cur(p)->line;p->p++;Tok*u=cur(p);need(p,TID,"field name");emit(p,I_CONST,con(p,vs(u->s)),u->line);}
        if(cur(p)->t==TLS||cur(p)->t==TDOT)emit(p,last_op==1?I_INDEX:I_FIELD,0,last_line);
      }
    }
    if(cur(p)->t!=TEQ)die("Haris parse error line %d: invalid assignment target",base_line);
    need(p,TEQ,"="); expr(p,0);
    if(last_op==0){ if(p->fn&&!p->is_root){if(li>=0)emit(p,I_SETL,li,base_line);else if(pi>=0)emit(p,I_SETL,pi,base_line);else die("Haris parse error line %d: assignment to undefined local '%s'",base_line,base->s);}else emit(p,I_SET,name(p,base->s),base_line); }
    else emit(p,last_op==1?I_SETINDEX:I_SETFIELD,0,last_line);
    skip_nl(p); return;
  }
  if(mt(p,TLET)){
    Tok*n=cur(p); need(p,TID,"identifier"); const char*ann=0;
    if(mt(p,TCOLON)){Tok*ty=cur(p);need(p,TID,"type name");ann=ty->s;}
    need(p,TEQ,"="); expr(p,0);
    if(p->fn&&!p->is_root){int li=fn_ensure_local(p->fn,n->s);emit(p,I_SETL,li,n->line);if(ann){emit(p,I_GETL,li,n->line);emit(p,I_TYPECHECK,con(p,vs(ann)),n->line);emit(p,I_POP,0,n->line);}}
    else {emit(p,I_SET,name(p,n->s),n->line);if(ann){emit(p,I_GET,name(p,n->s),n->line);emit(p,I_TYPECHECK,con(p,vs(ann)),n->line);emit(p,I_POP,0,n->line);} root_visibility(p->root_fn,n->s,p->visibility_pending); p->visibility_pending=0;}
    skip_nl(p); return;
  }
  if(mt(p,TSTRUCT)){
    Tok*sn=cur(p); need(p,TID,"struct name"); register_struct_name(sn->s); const char *prev=p->struct_name; p->struct_name=sn->s; skip_nl(p); need(p,TLB,"{"); skip_nl(p);
    while(cur(p)->t!=TRB&&cur(p)->t!=TEOF){
      if(cur(p)->t==TFN){
        Tok*tfn=cur(p); p->p++; Tok*mn=cur(p); need(p,TID,"method name"); Fn*m=xmalloc(sizeof(Fn)); memset(m,0,sizeof *m); m->name=xdup(mn->s); m->is_root=0;
        need(p,TLP,"("); if(cur(p)->t!=TRP){do{Tok*u=cur(p);need(p,TID,"parameter");m->params=xrealloc(m->params,(m->np+1)*sizeof(char*));m->params[m->np++]=xdup(u->s);if(mt(p,TCOLON))need(p,TID,"parameter type");}while(mt(p,TCOM));} need(p,TRP,")");
        if(m->np<1||strcmp(m->params[0],"self"))die("Haris parse error line %d: struct method must take self",tfn->line); if(mt(p,TMINUS)){need(p,TGT,"->");need(p,TID,"return type");} P q=*p; q.ch=&m->ch;q.fn=m;q.is_root=0;q.struct_name=sn->s;q.break_sp=q.continue_sp=0;skip_nl(&q);need(&q,TLB,"{");while(cur(&q)->t!=TRB&&cur(&q)->t!=TEOF)stmt(&q);need(&q,TRB,"}");emit(&q,I_CONST,con(&q,vn()),tfn->line);emit(&q,I_RET,0,tfn->line);register_method(sn->s,mn->s,m);p->p=q.p;skip_nl(p);if(mt(p,TCOM))skip_nl(p);continue;
      }
      need(p,TID,"field name");if(mt(p,TCOLON))need(p,TID,"field type");skip_nl(p);if(mt(p,TCOM))skip_nl(p);
    }
    need(p,TRB,"}");p->struct_name=prev;skip_nl(p);return;
  }
  if(mt(p,TENUM)){
    Tok*en=cur(p); need(p,TID,"enum name"); skip_nl(p); need(p,TLB,"{"); skip_nl(p);
    long long val=0;
    while(cur(p)->t!=TRB&&cur(p)->t!=TEOF){Tok*item=cur(p);need(p,TID,"enum member");emit(p,I_CONST,con(p,vi(val++)),item->line);emit(p,I_SET,name(p,item->s),item->line);skip_nl(p);if(!mt(p,TCOM))break;skip_nl(p);}
    need(p,TRB,"}"); skip_nl(p); (void)en; return;
  }
  if(mt(p,TTRAIT)){
    need(p,TID,"trait name"); skip_nl(p); need(p,TLB,"{");
    while(cur(p)->t!=TRB&&cur(p)->t!=TEOF){ if(mt(p,TFN)){need(p,TID,"trait method");if(mt(p,TLP)){while(cur(p)->t!=TRP&&cur(p)->t!=TEOF)p->p++;need(p,TRP,")");} if(cur(p)->t==TNL)skip_nl(p); mt(p,TSEM);} else p->p++; }
    need(p,TRB,"}"); skip_nl(p); return;
  }
  if(mt(p,TMATCH)){
    /* Match is lowered to ordinary equality checks using a private global. */
    static unsigned match_id=0; char hidden[64]; snprintf(hidden,sizeof hidden,"__match_%u",match_id++);
    expr(p,0); emit(p,I_SET,name(p,hidden),t->line); skip_nl(p); need(p,TLB,"{"); skip_nl(p);
    int end_jumps[64]; int nj=0;
    while(cur(p)->t!=TRB&&cur(p)->t!=TEOF){ mt(p,TCASE); skip_nl(p); match_value(p); emit(p,I_GET,name(p,hidden),t->line); emit(p,I_EQ,0,t->line); int jf=p->ch->n; emit(p,I_JMPF,0,t->line); skip_nl(p);
      if(mt(p,TLB)){while(cur(p)->t!=TRB&&cur(p)->t!=TEOF)stmt(p);need(p,TRB,"}");}else{stmt(p);} int je=p->ch->n;emit(p,I_JMP,0,t->line);if(nj<64)end_jumps[nj++]=je;p->ch->v[jf].a=p->ch->n;skip_nl(p);
    }
    need(p,TRB,"}");for(int j=0;j<nj;j++)p->ch->v[end_jumps[j]].a=p->ch->n;skip_nl(p);return;
  }
  int async_flag=mt(p,TASYNC); if(async_flag) need(p,TFN,"fn");
  if(async_flag || cur(p)->t==TFN){ if(!async_flag) mt(p,TFN); {
    Tok*n=cur(p); need(p,TID,"function name");
    Fn*f=xmalloc(sizeof(Fn)); memset(f,0,sizeof *f); f->name=xdup(n->s); f->is_async=async_flag; f->is_root=0;
    if(mt(p,TLT)){ do { Tok*g=cur(p); need(p,TID,"generic parameter"); for(int gi=0;gi<f->ngens;gi++) if(!strcmp(f->gens[gi],g->s)) die("Haris parse error line %d: duplicate generic %s",g->line,g->s); f->gens=xrealloc(f->gens,(f->ngens+1)*sizeof(char*)); f->gen_constraints=xrealloc(f->gen_constraints,(f->ngens+1)*sizeof(char*)); f->gens[f->ngens]=xdup(g->s); f->gen_constraints[f->ngens]=NULL; f->ngens++; if(mt(p,TCOLON)){Tok*co=cur(p);need(p,TID,"generic constraint");f->gen_constraints[f->ngens-1]=xdup(co->s);} } while(mt(p,TCOM)); need(p,TGT,">" ); }
    need(p,TLP,"(");
    if(cur(p)->t!=TRP){do{Tok*u=cur(p);need(p,TID,"parameter");f->params=xrealloc(f->params,(f->np+1)*sizeof(char*));f->param_types=xrealloc(f->param_types,(f->np+1)*sizeof(char*));f->params[f->np]=xdup(u->s);f->param_types[f->np]=NULL;f->np++;if(mt(p,TCOLON)){Tok*ty=cur(p);need(p,TID,"parameter type");f->param_types[f->np-1]=xdup(ty->s);}}while(mt(p,TCOM));}
    need(p,TRP,")"); if(mt(p,TMINUS)){need(p,TGT,">");need(p,TID,"return type");} P q=*p; q.ch=&f->ch; q.fn=f; q.is_root=0; q.break_sp=q.continue_sp=0;
    skip_nl(&q); need(&q,TLB,"{");
    while(cur(&q)->t!=TRB&&cur(&q)->t!=TEOF)stmt(&q);
    need(&q,TRB,"}"); p->p=q.p;
    /* Every function chunk must end in a halting instruction. A body with no
       explicit `return` (e.g. a function that only mutates state) would
       otherwise fall off the end of its bytecode array, and the interpreter
       would dispatch on whatever garbage happens to sit past it. This is
       harmless dead code when the body already returns on every path. */
    emit(&q,I_CONST,con(&q,vn()),t->line); int dr=fn_ensure_local(f,"__return_value"); emit(&q,I_SETL,dr,t->line); for(int di=q.defer_n-1;di>=0;di--){int ds=q.defer_slots[di];emit(&q,I_GETL,ds,t->line);int jf=q.ch->n;emit(&q,I_JMPF,0,t->line);emit(&q,I_GETL,ds,t->line);emit(&q,I_CALL,0,t->line);emit(&q,I_POP,0,t->line);q.ch->v[jf].a=q.ch->n;} emit(&q,I_GETL,dr,t->line); emit(&q,I_RET,0,t->line); optimize_bytecode(f);
    emit(p,I_CONST,con(p,(Value){.t=VFN,.u.fn=f}),t->line);
    emit(p,I_MAKE_CLOSURE,0,t->line);
    emit(p,I_SET,name(p,n->s),t->line); root_visibility(p->root_fn,n->s,p->visibility_pending); p->visibility_pending=0; skip_nl(p); return;
  }}
  if(mt(p,TRETURN)){
    if(!p->fn)die("return outside function");
    int rs=fn_ensure_local(p->fn,"__return_value");
    if(stmt_end(p)) emit(p,I_CONST,con(p,vn()),t->line); else expr(p,0);
    emit(p,I_SETL,rs,t->line); emit_defers(p); emit(p,I_GETL,rs,t->line); emit(p,I_RET,0,t->line); skip_nl(p); return;
  }
  if(mt(p,TFOR)){
    int destructure=0; char*dnames[8]; int dn=0; Tok*vn=NULL;
    if(cur(p)->t==TLS){
      destructure=1; p->p++;
      if(cur(p)->t!=TRS){ do{ Tok*nm=cur(p); need(p,TID,"loop variable"); if(dn<8)dnames[dn++]=xdup(nm->s); }while(mt(p,TCOM)); }
      need(p,TRS,"]");
    } else { vn=cur(p); need(p,TID,"loop variable"); }
    need(p,TIN,"in");
    static unsigned fid=0; char arrn[64],idxn[64],lenn[64]; snprintf(arrn,sizeof arrn,"__for_arr_%u",fid); snprintf(idxn,sizeof idxn,"__for_i_%u",fid); snprintf(lenn,sizeof lenn,"__for_len_%u",fid++);
    expr(p,0); if(p->fn&&!p->is_root){int al=fn_ensure_local(p->fn,arrn);emit(p,I_SETL,al,t->line);}else emit(p,I_SET,name(p,arrn),t->line);
    emit(p,I_GET,name(p,"len"),t->line); if(p->fn&&!p->is_root){int al=fn_local_index(p->fn,arrn);emit(p,I_GETL,al,t->line);}else emit(p,I_GET,name(p,arrn),t->line); emit(p,I_CALL,1,t->line); if(p->fn&&!p->is_root){int ll=fn_ensure_local(p->fn,lenn);emit(p,I_SETL,ll,t->line);}else emit(p,I_SET,name(p,lenn),t->line);
    emit(p,I_CONST,con(p,vi(0)),t->line); if(p->fn&&!p->is_root){int il=fn_ensure_local(p->fn,idxn);emit(p,I_SETL,il,t->line);}else emit(p,I_SET,name(p,idxn),t->line);
    int loop_start=p->ch->n; if(p->fn&&!p->is_root){emit(p,I_GETL,fn_local_index(p->fn,idxn),t->line);emit(p,I_GETL,fn_local_index(p->fn,lenn),t->line);}else{emit(p,I_GET,name(p,idxn),t->line);emit(p,I_GET,name(p,lenn),t->line);} emit(p,I_LT,0,t->line); int jf=p->ch->n; emit(p,I_JMPF,0,t->line);
    if(!destructure){
      if(p->fn&&!p->is_root){emit(p,I_GETL,fn_local_index(p->fn,arrn),t->line);emit(p,I_GETL,fn_local_index(p->fn,idxn),t->line);emit(p,I_INDEX,0,t->line);int viidx=fn_ensure_local(p->fn,vn->s);emit(p,I_SETL,viidx,vn->line);}else{emit(p,I_GET,name(p,arrn),t->line);emit(p,I_GET,name(p,idxn),t->line);emit(p,I_INDEX,0,t->line);emit(p,I_SET,name(p,vn->s),vn->line);}
    } else {
      static unsigned did=0; char tmpn[64]; snprintf(tmpn,sizeof tmpn,"__for_elem_%u",did++);
      if(p->fn&&!p->is_root){
        emit(p,I_GETL,fn_local_index(p->fn,arrn),t->line);emit(p,I_GETL,fn_local_index(p->fn,idxn),t->line);emit(p,I_INDEX,0,t->line);
        int ti=fn_ensure_local(p->fn,tmpn);emit(p,I_SETL,ti,t->line);
        for(int di=0;di<dn;di++){emit(p,I_GETL,fn_local_index(p->fn,tmpn),t->line);emit(p,I_CONST,con(p,vi(di)),t->line);emit(p,I_INDEX,0,t->line);int vidx=fn_ensure_local(p->fn,dnames[di]);emit(p,I_SETL,vidx,t->line);}
      } else {
        emit(p,I_GET,name(p,arrn),t->line);emit(p,I_GET,name(p,idxn),t->line);emit(p,I_INDEX,0,t->line);emit(p,I_SET,name(p,tmpn),t->line);
        for(int di=0;di<dn;di++){emit(p,I_GET,name(p,tmpn),t->line);emit(p,I_CONST,con(p,vi(di)),t->line);emit(p,I_INDEX,0,t->line);emit(p,I_SET,name(p,dnames[di]),t->line);}
      }
      for(int di=0;di<dn;di++)xfree(dnames[di]);
    }
    p->break_sp++; p->continue_sp++; int bsp=p->break_sp-1,csp=p->continue_sp-1; p->break_jumps[bsp]=p->continue_jumps[csp]=-1;
    skip_nl(p); need(p,TLB,"{"); while(cur(p)->t!=TRB&&cur(p)->t!=TEOF)stmt(p); need(p,TRB,"}");
    int cont_target=p->ch->n; if(p->continue_jumps[csp]>=0)p->ch->v[p->continue_jumps[csp]].a=cont_target;
    if(p->fn&&!p->is_root){emit(p,I_GETL,fn_local_index(p->fn,idxn),t->line);emit(p,I_CONST,con(p,vi(1)),t->line);emit(p,I_ADD,0,t->line);emit(p,I_SETL,fn_local_index(p->fn,idxn),t->line);}else{emit(p,I_GET,name(p,idxn),t->line);emit(p,I_CONST,con(p,vi(1)),t->line);emit(p,I_ADD,0,t->line);emit(p,I_SET,name(p,idxn),t->line);} emit(p,I_JMP,loop_start,t->line); int end=p->ch->n; p->ch->v[jf].a=end; if(p->break_jumps[bsp]>=0)p->ch->v[p->break_jumps[bsp]].a=end; p->break_sp--; p->continue_sp--; skip_nl(p); return;
  }
  if(mt(p,TBREAK)){ if(p->break_sp<=0)die("Haris parse error line %d: break outside loop",t->line); emit(p,I_JMP,0,t->line); p->break_jumps[p->break_sp-1]=p->ch->n-1; skip_nl(p); return; }
  if(mt(p,TCONT)){ if(p->continue_sp<=0)die("Haris parse error line %d: continue outside loop",t->line); emit(p,I_JMP,0,t->line); p->continue_jumps[p->continue_sp-1]=p->ch->n-1; skip_nl(p); return; }
  if(mt(p,TTHROW)){ if(p->fn&&p->try_depth==0) emit_defers(p); emit(p,I_GET,name(p,"throw"),t->line); expr(p,0); emit(p,I_CALL,1,t->line); emit(p,I_POP,0,t->line); skip_nl(p); return; }
  if(mt(p,TTRY)){
    skip_nl(p); need(p,TLB,"{");
    int tr=p->ch->n; emit_b(p,I_TRY,0,0,t->line); p->try_depth++;
    while(cur(p)->t!=TRB&&cur(p)->t!=TEOF)stmt(p);
    p->try_depth--; need(p,TRB,"}"); emit(p,I_ENDTRY,0,t->line);
    int jend=p->ch->n; emit(p,I_JMP,0,t->line);
    skip_nl(p); need(p,TCATCH,"catch");
    Tok*ce=cur(p); if(mt(p,TLP)){ce=cur(p);need(p,TID,"error variable");need(p,TRP,")");}else{need(p,TID,"error variable");}
    skip_nl(p); need(p,TLB,"{");
    int handler=p->ch->n; p->ch->v[tr].a=handler; p->ch->v[tr].b=name(p,ce->s);
    emit(p,I_SET,p->ch->v[tr].b,t->line);
    while(cur(p)->t!=TRB&&cur(p)->t!=TEOF)stmt(p);
    need(p,TRB,"}"); skip_nl(p); if(mt(p,TFINALLY)){ p->ch->v[jend].a=p->ch->n; skip_nl(p); need(p,TLB,"{"); while(cur(p)->t!=TRB&&cur(p)->t!=TEOF)stmt(p); need(p,TRB,"}"); skip_nl(p); } else { p->ch->v[jend].a=p->ch->n; } return;
  }
  if(mt(p,TIF)){
    expr(p,0); int jf=p->ch->n; emit(p,I_JMPF,0,t->line);
    skip_nl(p); need(p,TLB,"{");
    while(cur(p)->t!=TRB&&cur(p)->t!=TEOF)stmt(p);
    need(p,TRB,"}"); int je=p->ch->n; emit(p,I_JMP,0,t->line);
    p->ch->v[jf].a=p->ch->n;
    skip_nl(p);
    if(mt(p,TELSE)){
      skip_nl(p); need(p,TLB,"{");
      while(cur(p)->t!=TRB&&cur(p)->t!=TEOF)stmt(p);
      need(p,TRB,"}");
    }
    p->ch->v[je].a=p->ch->n; skip_nl(p); return;
  }
  if(mt(p,TWHILE)){
    int st=p->ch->n; expr(p,0); int jf=p->ch->n; emit(p,I_JMPF,0,t->line);
    skip_nl(p); need(p,TLB,"{");
    while(cur(p)->t!=TRB&&cur(p)->t!=TEOF)stmt(p);
    need(p,TRB,"}"); emit(p,I_JMP,st,t->line); p->ch->v[jf].a=p->ch->n;
    skip_nl(p); return;
  }
  if(cur(p)->t==TRB||cur(p)->t==TEOF){return;}
  /* assignment: locals/parameters stay in the frame; unresolved names are globals. */
  if(cur(p)->t==TID && p->p+1<p->tv->n && p->tv->v[p->p+1].t==TEQ){
    Tok*n=cur(p); p->p+=2; expr(p,0);
    if(p->fn&&!p->is_root){int li=fn_local_index(p->fn,n->s); if(li>=0)emit(p,I_SETL,li,n->line); else emit(p,I_SET,name(p,n->s),n->line);}
    else emit(p,I_SET,name(p,n->s),n->line);
    skip_nl(p); return;
  }
  expr(p,0); emit(p,I_POP,0,t->line); skip_nl(p);
}
static Value app_get_route(VM*vm,int n,Value*a); static Value app_post_route(VM*vm,int n,Value*a); static Value app_put_route(VM*vm,int n,Value*a); static Value app_delete_route(VM*vm,int n,Value*a); static Value app_listen(VM*vm,int n,Value*a); static Value app_https_listen(VM*vm,int n,Value*a);

/* ===================== Haris Forge 3.9 high-performance data/web/game layer ===================== */
static Value data_read_csv(VM*,int,Value*);
static Value data_from_rows(VM*,int,Value*);
static Value data_rows(VM*,int,Value*);
static Value data_shape(VM*,int,Value*);
static Value data_column(VM*,int,Value*);
static Value data_filter(VM*,int,Value*);
static Value data_sort(VM*,int,Value*);
static Value data_head(VM*,int,Value*);
static Value data_tail(VM*,int,Value*);
static Value data_describe(VM*,int,Value*);
static Value data_group_count(VM*,int,Value*);
static Value data_drop_missing(VM*,int,Value*);
static Value data_fill_missing(VM*,int,Value*);
static Value data_sample(VM*,int,Value*);
static Value data_write_csv(VM*,int,Value*);
static Value data_corr(VM*,int,Value*);
static Value data_cov(VM*,int,Value*);
static Value data_regression(VM*,int,Value*);
static Value data_zscore(VM*,int,Value*);
static Value data_minmax(VM*,int,Value*);
static Value data_jsonl(VM*,int,Value*);
static Value web_request_fast(VM*,int,Value*);
static Value web_get_json(VM*,int,Value*);
static Value web_batch(VM*,int,Value*);
static Value web_url_encode(VM*,int,Value*);
static Value web_request_headers(VM*,int,Value*);
static Value web_download(VM*,int,Value*);
static Value web_upload(VM*,int,Value*);
static Value web_server_json(VM*,int,Value*);
static Value cloud_env(VM*,int,Value*);
static Value cloud_get_json(VM*,int,Value*);
static Value cloud_post_json(VM*,int,Value*);
static Value cloud_kv_get(VM*,int,Value*);
static Value cloud_kv_set(VM*,int,Value*);
static Value cloud_storage_upload(VM*,int,Value*);
static Value cloud_storage_download(VM*,int,Value*);
static Value engine_query_radius(VM*,int,Value*);
static Value engine_stats(VM*,int,Value*);
static char *trim(char*);
static char *web_url_decode(const char*);
static Value web_serve_static(VM*,int,Value*);
static Value web_https_serve_static(VM*,int,Value*); static Value web_tls_profile(VM*,int,Value*); static int https_send_all(SSL*,const void*,size_t); static ssize_t https_recv_req(SSL*,char*,size_t,int); static const char *tls_default12(void); static const char *tls_default13(void); static int tls_configure_server(SSL_CTX*,const char*,const char*,int,int,const char*,const char*);
static Value web_tls_profile(VM*,int,Value*);
static Value web_http3_serve_static(VM*,int,Value*);
static Value web_http3_info(VM*,int,Value*);
static Value webrtc_available(VM*,int,Value*);
static Value webrtc_peer(VM*,int,Value*);
static Value webrtc_offer(VM*,int,Value*);
static Value webrtc_answer(VM*,int,Value*);
static Value webrtc_set_remote(VM*,int,Value*);
static Value webrtc_candidate(VM*,int,Value*);
static Value webrtc_data_channel(VM*,int,Value*);
static Value webrtc_send(VM*,int,Value*);
static Value webrtc_recv(VM*,int,Value*);
static Value webrtc_info(VM*,int,Value*);
static Value webrtc_close(VM*,int,Value*);
static Value nput(VM*,int,Value*);
static Value cloud_download(VM*,int,Value*);
static Value cloud_request(VM*,int,Value*);
#ifdef _WIN32
static Value sandbox_run_code_windows(VM*,const char*,unsigned);
#endif
static int remove_tree(const char*);

static char *trim(char*);
static Value gmethod_get(Value obj,const char*name){
    if(obj.t!=VSTRUCT||!name)return vn();
    Value ty=stget(obj.u.st,"__type");if(ty.t!=VSTR)return vn();
    Fn*f=find_method(ty.u.s,name);if(f)return vbound(NULL,f,obj);
    if(!strcmp(ty.u.s,"regex")){if(!strcmp(name,"find_all"))return vbound(regex_find_all,NULL,obj);if(!strcmp(name,"match"))return vbound(regex_match,NULL,obj);if(!strcmp(name,"search"))return vbound(regex_search,NULL,obj);}
    if(!strcmp(ty.u.s,"datetime")){if(!strcmp(name,"format"))return vbound(datetime_format,NULL,obj);if(!strcmp(name,"add_days"))return vbound(datetime_add_days,NULL,obj);if(!strcmp(name,"add_seconds"))return vbound(datetime_add_seconds,NULL,obj);}
    if(!strcmp(ty.u.s,"gfx_window")){if(!strcmp(name,"is_open"))return vbound(gfx_is_open,NULL,obj);if(!strcmp(name,"clear"))return vbound(gfx_clear,NULL,obj);if(!strcmp(name,"display"))return vbound(gfx_display,NULL,obj);if(!strcmp(name,"close"))return vbound(gfx_close,NULL,obj);}
    if(!strcmp(ty.u.s,"app")){if(!strcmp(name,"get"))return vbound(app_get_route,NULL,obj);if(!strcmp(name,"post"))return vbound(app_post_route,NULL,obj);if(!strcmp(name,"put"))return vbound(app_put_route,NULL,obj);if(!strcmp(name,"delete"))return vbound(app_delete_route,NULL,obj);if(!strcmp(name,"listen"))return vbound(app_listen,NULL,obj);if(!strcmp(name,"https_listen"))return vbound(app_https_listen,NULL,obj);}
    if(!strcmp(ty.u.s,"dataframe")){
        if(!strcmp(name,"head"))return vbound(data_head,NULL,obj);
        if(!strcmp(name,"tail"))return vbound(data_tail,NULL,obj);
        if(!strcmp(name,"shape"))return vbound(data_shape,NULL,obj);
        if(!strcmp(name,"rows"))return vbound(data_rows,NULL,obj);
        if(!strcmp(name,"column"))return vbound(data_column,NULL,obj);
        if(!strcmp(name,"filter"))return vbound(data_filter,NULL,obj);
        if(!strcmp(name,"sort"))return vbound(data_sort,NULL,obj);
        if(!strcmp(name,"describe"))return vbound(data_describe,NULL,obj);
        if(!strcmp(name,"groupby_count"))return vbound(data_group_count,NULL,obj);
        if(!strcmp(name,"drop_missing"))return vbound(data_drop_missing,NULL,obj);
        if(!strcmp(name,"fill_missing"))return vbound(data_fill_missing,NULL,obj);
        if(!strcmp(name,"sample"))return vbound(data_sample,NULL,obj);
        if(!strcmp(name,"to_csv"))return vbound(data_write_csv,NULL,obj);
    }
    return vn();
}

static void optimize_bytecode(Fn*f){
    if(!f||f->ch.n<2)return;
    Chunk*c=&f->ch; int n=c->n;
    Ins*old=c->v;
    Ins*nv=(Ins*)xmalloc((size_t)n*sizeof(Ins));
    int*map=(int*)malloc((size_t)(n+1)*sizeof(int));
    if(!nv||!map){if(nv)xfree(nv);if(map)free(map);return;}
    int i=0,m=0;
    while(i<n){
        map[i]=m;
        /* Safe loop induction update: GETL x; CONST 1; ADD; SETL x. */
        if(i+3<n&&old[i].op==I_GETL&&old[i+1].op==I_CONST&&old[i+1].a>=0&&old[i+1].a<c->nc&&c->c[old[i+1].a].t==VINT&&c->c[old[i+1].a].u.i==1&&old[i+2].op==I_ADD&&old[i+3].op==I_SETL&&old[i+3].a==old[i].a){
            nv[m]=(Ins){.op=I_INC_LOCAL,.a=old[i].a,.b=0,.line=old[i].line,.cache=NULL,.cache_kind=0};
            map[i+1]=m;map[i+2]=m;map[i+3]=m;m++;i+=4;continue;
        }
        if(i+3<n&&old[i].op==I_GETL&&old[i+1].op==I_CONST&&old[i+1].a>=0&&old[i+1].a<c->nc&&c->c[old[i+1].a].t==VINT&&c->c[old[i+1].a].u.i==1&&old[i+2].op==I_SUB&&old[i+3].op==I_SETL&&old[i+3].a==old[i].a){
            nv[m]=(Ins){.op=I_DEC_LOCAL,.a=old[i].a,.b=0,.line=old[i].line,.cache=NULL,.cache_kind=0};
            map[i+1]=m;map[i+2]=m;map[i+3]=m;m++;i+=4;continue;
        }
        /* Fuse local comparison + conditional jump. The source sequence has no
           side effects between the four instructions, and the comparison's bool
           is immediately consumed by JMPF, so the net stack effect remains zero. */
        if(i+3<n&&old[i].op==I_GETL&&old[i+1].op==I_CONST&&old[i+1].a>=0&&old[i+1].a<c->nc&&c->c[old[i+1].a].t==VINT&&old[i+2].op>=I_EQ&&old[i+2].op<=I_GTE&&old[i+3].op==I_JMPF){
            Op co=old[i+2].op;
            if(co==I_EQ||co==I_NEQ||co==I_LT||co==I_LTE||co==I_GT||co==I_GTE){
                nv[m]=(Ins){.op=I_LOCAL_CMP_JMPF,.a=old[i].a,.b=old[i+1].a,.line=old[i].line,.cache=(void*)(uintptr_t)old[i+3].a,.cache_kind=(unsigned)(co-I_EQ+1)};
                map[i+1]=m;map[i+2]=m;map[i+3]=m;m++;i+=4;continue;
            }
        }
        /* Fuse local = local <op> local. The original evaluates both operands
           before applying the arithmetic operator, so this is semantics-safe. */
        if(i+3<n&&old[i].op==I_GETL&&old[i+1].op==I_GETL&&old[i+2].op>=I_ADD&&old[i+2].op<=I_MOD&&old[i+3].op==I_SETL&&old[i+3].a==old[i].a){
            nv[m]=(Ins){.op=I_LOCAL_BIN,.a=old[i].a,.b=old[i+1].a,.line=old[i].line,.cache=(void*)(uintptr_t)old[i+2].op,.cache_kind=3};
            map[i+1]=m;map[i+2]=m;map[i+3]=m;m++;i+=4;continue;
        }
        /* Fuse local = local <op> integer-constant for all arithmetic ops. */
        if(i+3<n&&old[i].op==I_GETL&&old[i+1].op==I_CONST&&old[i+1].a>=0&&old[i+1].a<c->nc&&c->c[old[i+1].a].t==VINT&&old[i+2].op>=I_ADD&&old[i+2].op<=I_MOD&&old[i+3].op==I_SETL&&old[i+3].a==old[i].a){
            Op bo=old[i+2].op;
            Op sop=bo==I_ADD?I_LOCAL_CONST_ADD:bo==I_SUB?I_LOCAL_CONST_SUB:bo==I_MUL?I_LOCAL_CONST_MUL:bo==I_DIV?I_LOCAL_CONST_DIV:bo==I_MOD?I_LOCAL_CONST_MOD:I_HALT;
            if(sop!=I_HALT){
                nv[m]=(Ins){.op=sop,.a=old[i].a,.b=old[i+1].a,.line=old[i].line,.cache=NULL,.cache_kind=0};
                map[i+1]=m;map[i+2]=m;map[i+3]=m;m++;i+=4;continue;
            }
        }
        nv[m++]=old[i++];
    }
    map[n]=m;
    /* Rewrite control-flow targets after compaction. */
    for(int j=0;j<m;j++){
        switch(nv[j].op){
            case I_JMP: case I_JMPF: case I_TRY: case I_JMPNULL:
                if(nv[j].a>=0&&nv[j].a<=n)nv[j].a=map[nv[j].a];
                break;
            case I_LOCAL_CMP_JMPF:
                {int target=(int)(uintptr_t)nv[j].cache;if(target>=0&&target<=n)nv[j].cache=(void*)(uintptr_t)map[target];}
                break;
            default: break;
        }
    }
    xfree(old); c->v=nv; c->n=m; c->cap=n;
    free(map);
}

static Fn*compile(const char*src,const char*name){char*psrc=preprocess_interpolation(src);TV tv=lex(psrc);free(psrc);Fn*f=xmalloc(sizeof(Fn));memset(f,0,sizeof* f);f->name=xdup(name);f->is_root=1;P p={0}; p.tv=&tv; p.p=0; p.ch=&f->ch; p.fn=f; p.is_root=1; p.root_fn=f; p.break_sp=p.continue_sp=0;while(cur(&p)->t!=TEOF)stmt(&p);emit(&p,I_HALT,0,cur(&p)->line);optimize_bytecode(f);return f;}
