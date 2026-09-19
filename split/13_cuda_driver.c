/* -------- CUDA Driver API: real GPU execution when NVIDIA driver exists -------- */
typedef int HCUresult; typedef unsigned long long HCUdeviceptr; typedef int HCUdevice;
typedef void* HCUcontext; typedef void* HCUmodule; typedef void* HCUfunction;
typedef HCUresult (*hcuInitFn)(unsigned); typedef HCUresult (*hcuDeviceGetFn)(HCUdevice*,int);
typedef HCUresult (*hcuDeviceGetNameFn)(char*,int,HCUdevice); typedef HCUresult (*hcuCtxCreateFn)(HCUcontext*,unsigned,HCUdevice);
typedef HCUresult (*hcuCtxDestroyFn)(HCUcontext); typedef HCUresult (*hcuModuleLoadDataFn)(HCUmodule*,const void*);
typedef HCUresult (*hcuModuleGetFunctionFn)(HCUfunction*,HCUmodule,const char*); typedef HCUresult (*hcuModuleUnloadFn)(HCUmodule);
typedef HCUresult (*hcuMemAllocFn)(HCUdeviceptr*,size_t); typedef HCUresult (*hcuMemFreeFn)(HCUdeviceptr);
typedef HCUresult (*hcuMemcpyHtoDFn)(HCUdeviceptr,const void*,size_t); typedef HCUresult (*hcuMemcpyDtoHFn)(void*,HCUdeviceptr,size_t);
typedef HCUresult (*hcuLaunchKernelFn)(HCUfunction,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,void*,void**,void**);
typedef const char* (*hcuGetErrorNameFn)(HCUresult); typedef const char* (*hcuGetErrorStringFn)(HCUresult);
typedef struct { void *lib; hcuInitFn init; hcuDeviceGetFn device_get; hcuDeviceGetNameFn device_name; hcuCtxCreateFn ctx_create; hcuCtxDestroyFn ctx_destroy; hcuModuleLoadDataFn module_load; hcuModuleGetFunctionFn module_get; hcuModuleUnloadFn module_unload; hcuMemAllocFn mem_alloc; hcuMemFreeFn mem_free; hcuMemcpyHtoDFn htoD; hcuMemcpyDtoHFn dtoH; hcuLaunchKernelFn launch; hcuGetErrorNameFn err_name; hcuGetErrorStringFn err_string; int ready; } HCUDA;
static HARIS_TLS HCUDA g_hcuda={0};
static void* hcuda_sym(void*h,const char*n){
#ifdef _WIN32
    return h?(void*)GetProcAddress((HMODULE)h,n):NULL;
#else
    return h?dlsym(h,n):NULL;
#endif
}
static void hcuda_close_lib(void*h){
#ifdef _WIN32
    if(h)FreeLibrary((HMODULE)h);
#else
    if(h)dlclose(h);
#endif
}
static int hcuda_load_req(void*h,void **dst,const char*name){
    *dst=hcuda_sym(h,name);
    if(!*dst){ hcuda_close_lib(h); memset(&g_hcuda,0,sizeof g_hcuda); return 0; }
    return 1;
}
static int hcuda_load(void){
    if(g_hcuda.ready)return 1;
    void*h=NULL;
#ifdef _WIN32
    h=(void*)LoadLibraryA("nvcuda.dll");
#else
    h=dlopen("libcuda.so.1",RTLD_LAZY|RTLD_LOCAL);
    if(!h)h=dlopen("libcuda.so",RTLD_LAZY|RTLD_LOCAL);
#endif
    if(!h)return 0;
    g_hcuda.lib=h;
    if(!hcuda_load_req(h,(void**)&g_hcuda.init,"cuInit"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.device_get,"cuDeviceGet"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.device_name,"cuDeviceGetName"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.ctx_create,"cuCtxCreate_v2"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.ctx_destroy,"cuCtxDestroy_v2"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.module_load,"cuModuleLoadData"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.module_get,"cuModuleGetFunction"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.module_unload,"cuModuleUnload"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.mem_alloc,"cuMemAlloc_v2"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.mem_free,"cuMemFree_v2"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.htoD,"cuMemcpyHtoD_v2"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.dtoH,"cuMemcpyDtoH_v2"))return 0;
    if(!hcuda_load_req(h,(void**)&g_hcuda.launch,"cuLaunchKernel"))return 0;
    g_hcuda.err_name=(hcuGetErrorNameFn)hcuda_sym(h,"cuGetErrorName");g_hcuda.err_string=(hcuGetErrorStringFn)hcuda_sym(h,"cuGetErrorString");
    if(g_hcuda.init(0)!=0){ hcuda_close_lib(h); memset(&g_hcuda,0,sizeof g_hcuda); return 0; }
    g_hcuda.ready=1;return 1;
}
static Value ngpu_cuda_info(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_HW_GPU))return cap_error(vm,CAP_HW_GPU,"gpu.cuda_info");(void)a;if(n)return vn();
    Value o=vsobj();if(!hcuda_load()){stput(o.u.st,"available",vb(0));stput(o.u.st,"backend",vs("cuda-driver-unavailable"));return o;}
    HCUdevice d=0;if(g_hcuda.device_get(&d,0)!=0){stput(o.u.st,"available",vb(0));return o;}char name[256]={0};g_hcuda.device_name(name,sizeof name,d);stput(o.u.st,"available",vb(1));stput(o.u.st,"backend",vs("cuda-driver"));stput(o.u.st,"device",vs(name));return o;
}
/* UNTESTED: needs NVIDIA GPU + compatible driver.
   The CI portability suite can validate the CUDA driver API/fallback path,
   but cannot claim real GPU execution unless a self-hosted NVIDIA runner runs it. */
static Value ngpu_cuda_add(VM*vm,int n,Value*a){
    if(!cap_allowed(vm,CAP_HW_GPU))return cap_error(vm,CAP_HW_GPU,"gpu.cuda_add");if(n!=2||a[0].t!=VARR||a[1].t!=VARR||a[0].u.a->n!=a[1].u.a->n)return vn();size_t N=a[0].u.a->n;if(!N||N>1<<24)return vn();
    Value out=va();double*ha=(double*)xmalloc(N*sizeof(double));double*hb=(double*)xmalloc(N*sizeof(double));double*hc=(double*)xmalloc(N*sizeof(double));if(!ha||!hb||!hc){if(ha)xfree(ha);if(hb)xfree(hb);if(hc)xfree(hc);return vn();}
    for(size_t i=0;i<N;i++){if(!isnum(a[0].u.a->v[i])||!isnum(a[1].u.a->v[i])){xfree(ha);xfree(hb);xfree(hc);return vn();}ha[i]=dn(a[0].u.a->v[i]);hb[i]=dn(a[1].u.a->v[i]);}
    int ok=0;
    if(hcuda_load()){
        HCUdevice d=0;HCUcontext c=NULL;HCUmodule m=NULL;HCUfunction f=NULL;HCUdeviceptr da=0,db=0,dc=0;
        static const char ptx[]=".version 6.4\n.target sm_52\n.address_size 64\n.visible .entry add64(.param .u64 A,.param .u64 B,.param .u64 C,.param .u32 N){.reg .pred %p; .reg .b32 %r<6>; .reg .b64 %rd<10>; ld.param.u64 %rd1,[A]; ld.param.u64 %rd2,[B]; ld.param.u64 %rd3,[C]; ld.param.u32 %r1,[N]; mov.u32 %r2,%tid.x; mov.u32 %r3,%ctaid.x; mov.u32 %r4,%ntid.x; mad.lo.u32 %r5,%r3,%r4,%r2; setp.ge.u32 %p,%r5,%r1; @%p bra DONE; mul.wide.u32 %rd4,%r5,8; add.s64 %rd5,%rd1,%rd4; add.s64 %rd6,%rd2,%rd4; add.s64 %rd7,%rd3,%rd4; ld.global.f64 %fd1,[%rd5]; ld.global.f64 %fd2,[%rd6]; add.f64 %fd3,%fd1,%fd2; st.global.f64 [%rd7],%fd3; DONE: ret;}\n";
        if(g_hcuda.device_get(&d,0)==0 && g_hcuda.ctx_create(&c,0,d)==0 && g_hcuda.module_load(&m,ptx)==0 && g_hcuda.module_get(&f,m,"add64")==0 &&
           g_hcuda.mem_alloc(&da,N*sizeof(double))==0 && g_hcuda.mem_alloc(&db,N*sizeof(double))==0 && g_hcuda.mem_alloc(&dc,N*sizeof(double))==0 &&
           g_hcuda.htoD(da,ha,N*sizeof(double))==0 && g_hcuda.htoD(db,hb,N*sizeof(double))==0){
            unsigned blocks=(unsigned)((N+255)/256),threads=256;unsigned nn=(unsigned)N;void*args[]={&da,&db,&dc,&nn};
            if(g_hcuda.launch(f,blocks,1,1,threads,1,1,0,NULL,args,NULL)==0 && g_hcuda.dtoH(hc,dc,N*sizeof(double))==0)ok=1;
        }
        if(dc)g_hcuda.mem_free(dc);if(db)g_hcuda.mem_free(db);if(da)g_hcuda.mem_free(da);if(m)g_hcuda.module_unload(m);if(c)g_hcuda.ctx_destroy(c);
    }
    if(ok)for(size_t i=0;i<N;i++)ap(out.u.a,vf(hc[i]));else for(size_t i=0;i<N;i++)ap(out.u.a,vf(ha[i]+hb[i]));
    xfree(ha);xfree(hb);xfree(hc);return out;
}

