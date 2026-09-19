/* Haris Forge v10
   Release: v10.2.0
   Single-file VM/runtime for fast scripting, games, data, AI, HTTP/HTTPS, HTTP/3, WebRTC, UDP and WebSocket.

   Host build:  cc -O3 -march=native Haris_v10.c -o haris $(pkg-config --cflags --libs libcurl sqlite3 openssl) -lm -lpthread
   Embedded:   cc -O3 -march=native -DHARIS_EMBEDDED -DHARIS_NO_MAIN -fPIC -c Haris_v10.c
   Android:     build with the NDK/CMake project under android/ (HARIS_NO_MAIN).
   Emscripten: emcc -O3 -DHARIS_EMBEDDED -DHARIS_NO_MAIN Haris_v10.c ...

   Memory policy: memory.alloc/resize/read/write are bounds-checked and GC-managed.
   Professional raw memory: memory.wrap/address are host/engine-only; CAP_NUCLEAR is never granted inside a sandbox.
   Raw memory is intentionally unsafe and must only wrap memory whose lifetime is
   owned by the host/engine and remains valid for the whole handle lifetime.
*/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <direct.h>
#include <io.h>
#include <sddl.h>
#include <processthreadsapi.h>
#include <sys/stat.h>
#include <time.h>
#else
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <sys/resource.h>
#if defined(__APPLE__)
#include <sandbox.h>
#include <mach-o/dyld.h>
#endif
#if defined(__FreeBSD__)
#include <sys/capsicum.h>
#endif
#if defined(__OpenBSD__)
#include <unistd.h>
#endif
#if defined(__linux__)
#include <sys/mman.h>
#if !defined(__ANDROID__)
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/landlock.h>
#endif
#endif
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#ifdef __linux__
#include <dlfcn.h>
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#ifndef _WIN32
#include <signal.h>
#endif
#include <strings.h>
#include <limits.h>
#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <sqlite3.h>
#include <setjmp.h>
#include <regex.h>
#include <stdbool.h>
#include <float.h>

/* Performance hints are compile-time only and do not change language semantics. */
#if defined(__GNUC__) || defined(__clang__)
#  define HARIS_HOT __attribute__((hot))
#  define HARIS_ALWAYS_INLINE __attribute__((always_inline)) inline
#  define HARIS_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define HARIS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define HARIS_HOT
#  define HARIS_ALWAYS_INLINE inline
#  define HARIS_LIKELY(x) (x)
#  define HARIS_UNLIKELY(x) (x)
#endif

#if defined(__ANDROID__)
#define HARIS_ANDROID 1
#include <android/log.h>
#endif

/* Optional protocol backends.  HTTP/3 uses the OpenSSL 3.5+ QUIC API plus
   nghttp3; WebRTC uses the libdatachannel C API.  Both remain optional so the
   core runtime still builds on machines without those libraries. */
#if defined(HARIS_USE_HTTP3) && defined(__has_include)
#  if __has_include(<openssl/quic.h>) && __has_include(<nghttp3/nghttp3.h>)
#    include <openssl/quic.h>
#    include <nghttp3/nghttp3.h>
#    define HARIS_HAVE_HTTP3 1
#  endif
#endif
#if defined(HARIS_USE_WEBRTC) && defined(__has_include)
#  if __has_include(<rtc/rtc.h>)
#    include <rtc/rtc.h>
#    define HARIS_HAVE_WEBRTC 1
#  endif
#endif

/* Optional native in-process backends.  They are compile-time gated so the
   single-file runtime still builds on hosts that do not ship the libraries. */
#if !defined(HARIS_NO_LIBGIT2) && defined(__has_include)
#  if __has_include(<git2.h>)
#    include <git2.h>
#    define HARIS_HAVE_LIBGIT2 1
#  endif
#endif
#if defined(HARIS_USE_LLAMA_CPP)
#  include "llama.h"
#  define HARIS_HAVE_LLAMA 1
#endif
#ifndef _WIN32
#include <pthread.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

/* Public runtime version: define it before any function that uses it. */
#ifndef HARIS_VERSION
#define HARIS_VERSION "10.2.0"
#endif
#ifdef _WIN32
#define strcasecmp _stricmp
#define strtok_r strtok_s
#define access _access
#define mkdir(path,mode) _mkdir(path)
#define chdir _chdir
#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#endif

typedef struct Value Value; typedef struct Arr Arr; typedef struct Env Env; typedef struct Fn Fn; typedef struct VM VM; typedef struct StructObj StructObj; typedef struct BoundCall BoundCall; typedef struct HEntity HEntity; typedef struct HWorld HWorld; typedef struct HModel HModel; typedef struct HMLP HMLP; typedef struct HSceneNode HSceneNode; typedef struct HScene HScene; typedef struct HTensor HTensor; typedef struct HLLM HLLM;
typedef Value (*Native)(VM*, int, Value*);
typedef struct HMLP HMLP;
static void xfree(void *p);
#define HK_MLP 0x4D4C50
#define HK_REGEX 0x52454758
#define HK_CLOSED 0x434C5344
#define HK_MEMBUF 0x4D425546
#define HK_ASYNC 0x4153594E
#define HK_TENSOR 0x54454E53
#define HK_LLM 0x4C4C4D31
static char* b64_encode_bytes(const unsigned char*in,size_t n);
static int hkind(Value v,int k);
static double ai_xavier_weight(VM*,int,int);
static Value ai_mlp(VM*,int,Value*);
static Value udp_open(VM*,int,Value*);
static Value udp_bind(VM*,int,Value*);
static Value udp_send(VM*,int,Value*);
static int sha256_mem_hex(const unsigned char *data,size_t n,char out[65]);
static Value udp_recv(VM*,int,Value*);
static Value udp_send_batch(VM*,int,Value*);
static Value udp_connect(VM*,int,Value*);
static Value udp_set_nonblock(VM*,int,Value*);
static Value udp_set_buffer(VM*,int,Value*);
static Value udp_close(VM*,int,Value*);
static Value udp_info(VM*,int,Value*);
static Value ws_connect(VM*,int,Value*);
static Value ws_listen(VM*,int,Value*);
static Value ws_accept(VM*,int,Value*);
static Value ws_send_text(VM*,int,Value*);
static Value ws_send_binary(VM*,int,Value*);
static Value ws_recv(VM*,int,Value*);
static Value ws_ping(VM*,int,Value*);
static Value ws_close(VM*,int,Value*);
static Value ws_info(VM*,int,Value*);
static Value ai_game_brain(VM*,int,Value*);
static Value ai_game_act(VM*,int,Value*);
static Value ai_game_remember(VM*,int,Value*);
static Value ai_game_train_step(VM*,int,Value*);
static Value ai_game_target_update(VM*,int,Value*);
static Value ai_game_info(VM*,int,Value*);
static Value game_nav_astar(VM*,int,Value*);
static Value ai_mlp_train(VM*,int,Value*);
struct HMLP { int kind, in, hidden, out; double *w1,*b1,*w2,*b2;
    double *m1,*v1,*mb1,*vb1,*m2,*v2,*mb2,*vb2;
    uint64_t opt_step; double opt_b1_pow,opt_b2_pow;
};
static void hmlp_dtor(void*p){HMLP*m=(HMLP*)p;if(!m)return;
    if(m->w1){xfree(m->w1);m->w1=NULL;} if(m->b1){xfree(m->b1);m->b1=NULL;}
    if(m->w2){xfree(m->w2);m->w2=NULL;} if(m->b2){xfree(m->b2);m->b2=NULL;}
    if(m->m1){xfree(m->m1);m->m1=NULL;} if(m->v1){xfree(m->v1);m->v1=NULL;}
    if(m->mb1){xfree(m->mb1);m->mb1=NULL;} if(m->vb1){xfree(m->vb1);m->vb1=NULL;}
    if(m->m2){xfree(m->m2);m->m2=NULL;} if(m->v2){xfree(m->v2);m->v2=NULL;}
    if(m->mb2){xfree(m->mb2);m->mb2=NULL;} if(m->vb2){xfree(m->vb2);m->vb2=NULL;}
}
static void *xmalloc(size_t n);
static int hmlp_opt_init(HMLP*m){
    if(!m)return 0;
    if(m->m1&&m->v1&&m->mb1&&m->vb1&&m->m2&&m->v2&&m->mb2&&m->vb2)return 1;
    size_t n1=(size_t)m->in*(size_t)m->hidden,n2=(size_t)m->hidden*(size_t)m->out;
    m->m1=(double*)xmalloc(n1*sizeof(double));m->v1=(double*)xmalloc(n1*sizeof(double));
    m->mb1=(double*)xmalloc((size_t)m->hidden*sizeof(double));m->vb1=(double*)xmalloc((size_t)m->hidden*sizeof(double));
    m->m2=(double*)xmalloc(n2*sizeof(double));m->v2=(double*)xmalloc(n2*sizeof(double));
    m->mb2=(double*)xmalloc((size_t)m->out*sizeof(double));m->vb2=(double*)xmalloc((size_t)m->out*sizeof(double));
    memset(m->m1,0,n1*sizeof(double));memset(m->v1,0,n1*sizeof(double));
    memset(m->mb1,0,(size_t)m->hidden*sizeof(double));memset(m->vb1,0,(size_t)m->hidden*sizeof(double));
    memset(m->m2,0,n2*sizeof(double));memset(m->v2,0,n2*sizeof(double));
    memset(m->mb2,0,(size_t)m->out*sizeof(double));memset(m->vb2,0,(size_t)m->out*sizeof(double));
    m->opt_step=0;m->opt_b1_pow=1.0;m->opt_b2_pow=1.0;return 1;
}
typedef enum { VNULL,VINT,VFLOAT,VBOOL,VSTR,VARR,VFN,VNATIVE,VHANDLE,VSTRUCT,VBOUND } VT;
struct Arr { Value *v; size_t n,cap; };
struct Value { VT t; union { long long i; double f; int b; char *s; Arr *a; Fn *fn; Native native; void *handle; StructObj *st; BoundCall *bound; } u; };
typedef struct { char *name; Value value; } StructField;
struct BoundCall { Native native; Fn *fn; Value *self; };
typedef struct MethodReg { char *type; char *name; Fn *fn; struct MethodReg *next; } MethodReg;
struct StructObj { StructField *v; size_t n,cap; };
typedef enum { I_CONST,I_GET,I_GETL,I_SET,I_SETL,I_MAKE_CLOSURE,I_POP,I_ADD,I_SUB,I_MUL,I_DIV,I_MOD,I_NEG,I_NOT,I_EQ,I_NEQ,I_LT,I_LTE,I_GT,I_GTE,I_AND,I_OR,I_JMP,I_JMPF,I_CALL,I_RET,I_ARRAY,I_INDEX,I_SETINDEX,I_FIELD,I_SETFIELD,I_STRUCT,I_TYPECHECK,I_TRY,I_ENDTRY,I_JMPNULL,I_HALT, I_INC_LOCAL, I_DEC_LOCAL, I_LOCAL_CONST_ADD, I_LOCAL_CONST_SUB, I_LOCAL_CONST_MUL, I_LOCAL_CONST_DIV, I_LOCAL_CONST_MOD, I_LOCAL_BIN, I_LOCAL_CMP_JMPF, I_OP_COUNT } Op;
_Static_assert(I_OP_COUNT == I_LOCAL_CMP_JMPF + 1, "Haris: Op enum sentinel out of sync");
typedef struct { Op op; int a; int b; int line; void *cache; unsigned cache_kind; } Ins;
typedef struct { Ins *v; int n,cap; Value *c; int nc,cc; char **names; int nn,cn; } Chunk;
typedef long long (*JitI64Fn)(long long,long long,long long,long long,long long,long long);
typedef long long (*LoopJitFn)(long long,long long);
typedef long long (*LoopJitFn3)(long long,long long,long long);
struct Fn { char *name; char **params; char **param_types; int np; int is_async; int is_root; char **locals; int nlocals; char **gens; char **gen_constraints; int ngens; char **spec_types; int nspec; char **exports; int nexports; char **privates; int nprivates; Env *closure; Chunk ch; unsigned long long calls; JitI64Fn jit_i64; int rec_jit_kind; void *jit_mem; size_t jit_size;
  unsigned long long loop_hits; LoopJitFn loop_jit_i64; LoopJitFn3 loop_jit3_i64; void *loop_mem; size_t loop_size; int loop_start, loop_exit, loop_var_name; int loop_is_local; int loop_limit_is_local, loop_limit_local; long long loop_limit, loop_step; int loop_cmp;
  int loop_acc_active, loop_acc_start, loop_acc_exit, loop_acc_name, loop_acc_var, loop_acc_is_local, loop_acc_cmp, loop_acc_limit_is_local, loop_acc_limit_local; long long loop_acc_limit, loop_acc_step;
};
typedef struct Bind { char *k; Value v; } Bind;
struct Env { Bind *v; int n,cap; Env *p; };
typedef struct { int ip, base; Fn *fn; } Frame;
struct ErrFrame { int ip; int fp; int sp; };
struct VM { Value st[8192]; int sp; Frame fr[256]; int fp; Env g; char **mods; int nm,cm;
  long long cpu_limit_ms, start_cpu_ms, cpu_slice_wall_ms, cpu_slice_cpu_ms; size_t mem_limit_bytes; size_t mem_used, mem_peak; int cpu_cores, cpu_percent; int cpu_affinity_ok; int int_overflow; int last_error; char *profile; int memory_auto; unsigned module_caps; unsigned host_module_caps; int module_sandboxed; int sandbox_worker; int sandbox_auto; int redteam_authorized; int pentest_authorized; char legal_doc_hash[65]; char *audit_log_path; char *sandbox_root; char *sandbox_root_owned; char sandbox_backend[48]; char **trusted_modules; int ntrusted,ctrusted; int err_depth; int err_active; int err_ip; int err_fp; int err_sp; struct ErrFrame err_frames[256]; char err_msg[512]; int jmp_depth; jmp_buf jmp_stack[32]; uint64_t rng_state; };
typedef enum { TEOF,TID,TINT,TFLOAT,TSTR, TPLUS,TMINUS,TSTAR,TSLASH,TPCT,TEQ,TEQEQ,TNEQ,TLT,TLTE,TGT,TGTE,TLP,TRP,TLB,TRB,TLS,TRS,TCOM,TSEM,TDOT,TCOLON,TIF,TELSE,TWHILE,TFN,TASYNC,TAWAIT,TSTRUCT,TENUM,TMATCH,TCASE,TRETURN,TLET,TTRUE,TFALSE,TNULL,TAND,TOR,TNOT,TIMPORT,TTRAIT,TTRY,TCATCH,TFOR,TIN,TBREAK,TCONT,TTHROW,TFINALLY,TEXPORT,TPRIVATE,TFROM,TDEFER,TQUESTION,TQDOT,TNL } TT;
typedef struct { TT t; char *s; int line; } Tok; typedef struct { Tok *v; int n,p,cap; } TV; typedef struct { TV *tv; int p; Chunk *ch; Fn *fn; int is_root; int break_sp, continue_sp; int break_jumps[64], continue_jumps[64]; const char *struct_name; int visibility_pending; Fn *root_fn; int defer_n, defer_cap; int *defer_slots; int try_depth; } P;
static void die(const char *f,...);
static void *xmalloc(size_t n); static void *xrealloc(void *p,size_t n); static char *xdup(const char*s);
static void xfree(void *p); static int sha256_file(const char*path,char out[65]); static unsigned cap_from_name(const char*s);

/* Standalone pack format: the native Haris executable is copied verbatim and
   a bounded, SHA-256-checked Haris source archive is appended. Pack v2 uses a
   mirrored header/footer with reserved flags so future compression, native-cache,
   and metadata features can be introduced without ambiguous decoding. */
static void init(VM*vm);
Value run(VM*vm,Fn*f,int argc,Value*args);
#if defined(_MSC_VER)
#define HARIS_TLS __declspec(thread)
#else
#define HARIS_TLS _Thread_local
#endif
static HARIS_TLS VM *g_runtime_vm=NULL;
#ifdef _WIN32
static CRITICAL_SECTION g_heap_mutex; static int g_heap_mutex_init=0;
static void heap_lock_init(void){if(!g_heap_mutex_init){InitializeCriticalSection(&g_heap_mutex);g_heap_mutex_init=1;}}
#define HEAP_LOCK() do{heap_lock_init();EnterCriticalSection(&g_heap_mutex);}while(0)
#define HEAP_UNLOCK() LeaveCriticalSection(&g_heap_mutex)
static CRITICAL_SECTION g_exec_mutex; static int g_exec_mutex_init=0;
static void exec_lock_init(void){if(!g_exec_mutex_init){InitializeCriticalSection(&g_exec_mutex);g_exec_mutex_init=1;}}
#define EXEC_LOCK() do{exec_lock_init();EnterCriticalSection(&g_exec_mutex);}while(0)
#define EXEC_UNLOCK() LeaveCriticalSection(&g_exec_mutex)
#else
static pthread_mutex_t g_heap_mutex=PTHREAD_MUTEX_INITIALIZER;
#define HEAP_LOCK() pthread_mutex_lock(&g_heap_mutex)
#define HEAP_UNLOCK() pthread_mutex_unlock(&g_heap_mutex)
static pthread_once_t g_exec_once=PTHREAD_ONCE_INIT; static pthread_mutex_t g_exec_mutex;
static void exec_mutex_init(void){pthread_mutexattr_t a;pthread_mutexattr_init(&a);pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE);pthread_mutex_init(&g_exec_mutex,&a);pthread_mutexattr_destroy(&a);}
#define EXEC_LOCK() do{pthread_once(&g_exec_once,exec_mutex_init);pthread_mutex_lock(&g_exec_mutex);}while(0)
#define EXEC_UNLOCK() pthread_mutex_unlock(&g_exec_mutex)
#endif
static void die(const char *f,...){va_list a;va_start(a,f);if(g_runtime_vm&&g_runtime_vm->jmp_depth>0){vsnprintf(g_runtime_vm->err_msg,sizeof(g_runtime_vm->err_msg),f,a);g_runtime_vm->err_active=1;va_end(a);longjmp(g_runtime_vm->jmp_stack[g_runtime_vm->jmp_depth-1],1);}vfprintf(stderr,f,a);va_end(a);fputc('\n',stderr);exit(1);} 
static void oom(void){die("Haris: out of memory");}
typedef struct Hdr Hdr;
struct Hdr { size_t n; unsigned marked; unsigned char generation; unsigned char age; void (*dtor)(void*); Hdr *next; Hdr *hnext; };
static size_t g_mem_used=0, g_mem_peak=0; static HARIS_TLS size_t g_mem_limit=0;
static Hdr *g_heap_head=NULL;
#define HEAP_BUCKETS 65536u
static Hdr *g_heap_index[HEAP_BUCKETS];
static size_t g_gc_next=2*1024*1024;
typedef struct JitBlock { void *p; size_t n; struct JitBlock *next; } JitBlock;
static JitBlock *g_jit_blocks=NULL;
#ifdef _WIN32
typedef struct AsyncTask { int kind; long id; HANDLE thread; int done; int rc; int started; int joined; char *file; Fn *fn; int argc; Value *args; Value result; } AsyncTask;
#else
#include <pthread.h>
typedef struct AsyncTask { int kind; long id; pthread_t thread; int done; int rc; int started; int joined; char *file; Fn *fn; int argc; Value *args; Value result; } AsyncTask;
#endif
static AsyncTask **g_tasks=NULL; static size_t g_task_n=0,g_task_cap=0; static long g_next_task=1;
static const char *g_program_path=NULL;

static unsigned heap_hash(const void *p){uintptr_t x=(uintptr_t)p;
#if UINTPTR_MAX > 0xffffffffu
x^=x>>33;x*=UINT64_C(0xff51afd7ed558ccd);x^=x>>33;x*=UINT64_C(0xc4ceb9fe1a85ec53);x^=x>>33;
#else
x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;
#endif
return (unsigned)(x&(HEAP_BUCKETS-1));}
static void heap_index_add(Hdr*h){unsigned b=heap_hash((void*)(h+1));h->hnext=g_heap_index[b];g_heap_index[b]=h;}
static void heap_index_del(Hdr*h){unsigned b=heap_hash((void*)(h+1));Hdr**pp=&g_heap_index[b];while(*pp&&*pp!=h)pp=&(*pp)->hnext;if(*pp)*pp=(*pp)->hnext;h->hnext=NULL;}
static int heap_is_ptr(void*p){if(!p)return 0;unsigned b=heap_hash(p);for(Hdr*h=g_heap_index[b];h;h=h->hnext)if((void*)(h+1)==p)return 1;return 0;}
static void heap_add(Hdr*h){h->next=g_heap_head;g_heap_head=h;heap_index_add(h);}
static void heap_del(Hdr*h){Hdr**pp=&g_heap_head;while(*pp&&*pp!=h)pp=&(*pp)->next;if(*pp)*pp=(*pp)->next;heap_index_del(h);}
static int heap_find_ptr(void*p,Hdr**out){if(!p)return 0;unsigned b=heap_hash(p);for(Hdr*h=g_heap_index[b];h;h=h->hnext)if((void*)(h+1)==p){if(out)*out=h;return 1;}return 0;}

static void gc_mark_ptr(void*p); static void gc_mark_handle(void*p); static void gc_mark_value(Value v); static void gc_mark_game_server_handle(void*p);
static void gc_mark_chunk(Chunk*c); static void gc_mark_fn(Fn*f); static void gc_mark_env(Env*e);
static void gc_mark_late_handle(void*p,int kind);
static void gc_mark_global_roots(void);
static void gc_mark_vm_roots(VM*vm);

typedef struct ActiveVM { VM*vm; struct ActiveVM*next; } ActiveVM;
static ActiveVM *g_active_vms=NULL;
static void active_vm_add(VM*vm){ActiveVM*n=(ActiveVM*)malloc(sizeof(*n));if(!n)oom();n->vm=vm;EXEC_LOCK();n->next=g_active_vms;g_active_vms=n;EXEC_UNLOCK();}
static void active_vm_del(VM*vm){EXEC_LOCK();ActiveVM**pp=&g_active_vms;while(*pp&&(*pp)->vm!=vm)pp=&(*pp)->next;if(*pp){ActiveVM*n=*pp;*pp=n->next;free(n);}EXEC_UNLOCK();}

static void gc_mark_ptr(void*p){if(!p||!heap_is_ptr(p))return;Hdr*h=((Hdr*)p)-1;if(h->marked)return;h->marked=1;}
static void gc_mark_value(Value v){
    if(v.t==VSTR){gc_mark_ptr(v.u.s);return;}
    if(v.t==VARR&&v.u.a){gc_mark_ptr(v.u.a);if(heap_is_ptr(v.u.a)){if(v.u.a->v)gc_mark_ptr(v.u.a->v);for(size_t i=0;i<v.u.a->n;i++)gc_mark_value(v.u.a->v[i]);}}
    else if(v.t==VFN&&v.u.fn){gc_mark_ptr(v.u.fn);if(heap_is_ptr(v.u.fn))gc_mark_fn(v.u.fn);}
    else if(v.t==VHANDLE)gc_mark_handle(v.u.handle);
    else if(v.t==VBOUND&&v.u.bound){gc_mark_ptr(v.u.bound);if(v.u.bound->self)gc_mark_value(*v.u.bound->self);if(v.u.bound->fn)gc_mark_fn(v.u.bound->fn);}
    else if(v.t==VSTRUCT&&v.u.st){gc_mark_ptr(v.u.st);if(heap_is_ptr(v.u.st)){if(v.u.st->v)gc_mark_ptr(v.u.st->v);for(size_t i=0;i<v.u.st->n;i++){gc_mark_ptr(v.u.st->v[i].name);gc_mark_value(v.u.st->v[i].value);}}}
}
static void gc_mark_chunk(Chunk*c){if(!c)return;if(c->v)gc_mark_ptr(c->v);if(c->c){gc_mark_ptr(c->c);for(int i=0;i<c->nc;i++)gc_mark_value(c->c[i]);}if(c->names){gc_mark_ptr(c->names);for(int i=0;i<c->nn;i++)gc_mark_ptr(c->names[i]);}}
static void gc_mark_fn(Fn*f){if(!f||!heap_is_ptr(f))return;gc_mark_ptr(f);if(f->name)gc_mark_ptr(f->name);if(f->params){gc_mark_ptr(f->params);for(int i=0;i<f->np;i++)gc_mark_ptr(f->params[i]);}if(f->param_types){gc_mark_ptr(f->param_types);for(int i=0;i<f->np;i++)if(f->param_types[i])gc_mark_ptr(f->param_types[i]);}if(f->locals){gc_mark_ptr(f->locals);for(int i=0;i<f->nlocals;i++)if(f->locals[i])gc_mark_ptr(f->locals[i]);}if(f->gens){gc_mark_ptr(f->gens);for(int i=0;i<f->ngens;i++)if(f->gens[i])gc_mark_ptr(f->gens[i]);}if(f->gen_constraints){gc_mark_ptr(f->gen_constraints);for(int i=0;i<f->ngens;i++)if(f->gen_constraints[i])gc_mark_ptr(f->gen_constraints[i]);}if(f->spec_types){gc_mark_ptr(f->spec_types);for(int i=0;i<f->nspec;i++)if(f->spec_types[i])gc_mark_ptr(f->spec_types[i]);}if(f->exports){gc_mark_ptr(f->exports);for(int i=0;i<f->nexports;i++)if(f->exports[i])gc_mark_ptr(f->exports[i]);}if(f->privates){gc_mark_ptr(f->privates);for(int i=0;i<f->nprivates;i++)if(f->privates[i])gc_mark_ptr(f->privates[i]);}if(f->closure){gc_mark_ptr(f->closure);gc_mark_env(f->closure);}gc_mark_chunk(&f->ch);}
static void gc_mark_env(Env*e){for(Env*p=e;p;p=p->p){if(p->v){gc_mark_ptr(p->v);for(int i=0;i<p->n;i++){if(p->v[i].k)gc_mark_ptr(p->v[i].k);gc_mark_value(p->v[i].v);}}}}
static void gc_mark_vm_roots(VM*vm){if(!vm)return;gc_mark_env(&vm->g);for(int i=0;i<vm->sp&&i<(int)(sizeof(vm->st)/sizeof(vm->st[0]));i++)gc_mark_value(vm->st[i]);for(int i=0;i<vm->fp&&i<(int)(sizeof(vm->fr)/sizeof(vm->fr[0]));i++)if(vm->fr[i].fn)gc_mark_fn(vm->fr[i].fn);if(vm->mods){gc_mark_ptr(vm->mods);for(int i=0;i<vm->nm;i++)if(vm->mods[i])gc_mark_ptr(vm->mods[i]);}if(vm->trusted_modules){gc_mark_ptr(vm->trusted_modules);for(int i=0;i<vm->ntrusted;i++)if(vm->trusted_modules[i])gc_mark_ptr(vm->trusted_modules[i]);}if(vm->profile)gc_mark_ptr(vm->profile);}
static void gc_collect(VM*vm){(void)vm;EXEC_LOCK();HEAP_LOCK();for(Hdr*h=g_heap_head;h;h=h->next)h->marked=0;gc_mark_global_roots();Hdr*dead=NULL;Hdr*h=g_heap_head,*prev=NULL;while(h){Hdr*n=h->next;if(!h->marked){if(prev)prev->next=n;else g_heap_head=n;g_mem_used-=h->n;heap_index_del(h);h->hnext=dead;dead=h;}else prev=h;h=n;}g_gc_next=g_mem_used<1024*1024?2*1024*1024:g_mem_used*2;HEAP_UNLOCK();while(dead){Hdr*n=dead->hnext;if(dead->dtor)dead->dtor((void*)(dead+1));free(dead);dead=n;}EXEC_UNLOCK();}
static size_t gc_minor(VM*vm){(void)vm;EXEC_LOCK();HEAP_LOCK();for(Hdr*h=g_heap_head;h;h=h->next)if(h->generation==0)h->marked=0;gc_mark_global_roots();Hdr*dead=NULL;Hdr*h=g_heap_head,*prev=NULL;size_t freed=0;while(h){Hdr*n=h->next;if(h->generation==0&&!h->marked){size_t z=h->n;if(prev)prev->next=n;else g_heap_head=n;g_mem_used-=z;freed+=z;heap_index_del(h);h->hnext=dead;dead=h;}else{if(h->generation==0&&h->marked&&h->age++>=1){h->generation=1;h->age=0;}prev=h;}h=n;}g_gc_next=g_mem_used*2>2*1024*1024?g_mem_used*2:2*1024*1024;HEAP_UNLOCK();while(dead){Hdr*n=dead->hnext;if(dead->dtor)dead->dtor((void*)(dead+1));free(dead);dead=n;}EXEC_UNLOCK();return freed;}

static void *xmalloc_dtor(size_t n,void(*dtor)(void*)){size_t z=n?n:1;if(z>SIZE_MAX-sizeof(Hdr))oom();void*mem=malloc(sizeof(Hdr)+z);if(!mem)oom();Hdr*h=(Hdr*)mem;HEAP_LOCK();if(g_mem_limit&&z>g_mem_limit-g_mem_used){HEAP_UNLOCK();free(mem);oom();}g_mem_used+=z;if(g_mem_used>g_mem_peak)g_mem_peak=g_mem_used;h->n=z;h->marked=0;h->generation=0;h->age=0;h->dtor=dtor;h->next=NULL;h->hnext=NULL;heap_add(h);HEAP_UNLOCK();return(void*)(h+1);}
static void *xmalloc(size_t n){return xmalloc_dtor(n,NULL);}
static void *xrealloc(void*p,size_t n){if(!p)return xmalloc(n);size_t z=n?n:1;if(z>SIZE_MAX-sizeof(Hdr))oom();HEAP_LOCK();Hdr*h=NULL;if(!heap_find_ptr(p,&h)){HEAP_UNLOCK();die("Haris: invalid managed pointer passed to realloc");}size_t old=h->n;if(z>old){size_t d=z-old;if(g_mem_limit&&d>g_mem_limit-g_mem_used){HEAP_UNLOCK();oom();}g_mem_used+=d;}else g_mem_used-=old-z;Hdr**pp=&g_heap_head;while(*pp&&*pp!=h)pp=&(*pp)->next;if(!*pp){HEAP_UNLOCK();die("Haris: managed heap corruption");}Hdr*oldh=h;Hdr*next=h->next;void(*dtor)(void*)=h->dtor;heap_index_del(oldh);h=(Hdr*)realloc(h,sizeof(Hdr)+z);if(!h){g_mem_used+=old>z?old-z:0;g_mem_used-=z>old?z-old:0;heap_index_add(oldh);HEAP_UNLOCK();oom();}h->n=z;h->dtor=dtor;h->hnext=NULL;h->next=next;if(h!=oldh)*pp=h;if(h->generation==0&&z>old)h->age=0;heap_index_add(h);HEAP_UNLOCK();return(void*)(h+1);}

