/* -------------------------------------------------------------------------
   Cross-platform sandbox backends. No backend is treated as equivalent: when
   a native isolation primitive is unavailable, Haris fails closed for
   privileged filesystem/process/hardware capabilities instead of pretending
   that rlimits alone are a security boundary.
   ------------------------------------------------------------------------- */
static int sandbox_caps_are_privileged(unsigned caps){
    return (caps & (CAP_OS|CAP_SYSTEM|CAP_NET|CAP_WEB|CAP_SQL|CAP_CLOUD|CAP_GIT|CAP_FS|
                    CAP_NUCLEAR|CAP_REDTEAM|CAP_PENTEST|CAP_HW_ANY|CAP_GAME_ADMIN)) != 0;
}
static int sandbox_caps_safe(unsigned caps){ return (caps & (CAP_NUCLEAR|CAP_HW_ANY|CAP_GAME_ADMIN))==0; }

#if defined(__EMSCRIPTEN__)
static int sandbox_platform_harden(unsigned caps,const char *root,char *backend,size_t bsz){
    (void)caps;(void)root;if(backend&&bsz)snprintf(backend,bsz,"wasm-host-sandbox");return 1;
}
#elif defined(__linux__) && !defined(__ANDROID__)
static int sandbox_add_landlock_path(int rules_fd,const char *path,unsigned access){
    int fd=open(path,O_PATH|O_DIRECTORY|O_CLOEXEC);if(fd<0)return 0;
    struct landlock_path_beneath_attr pb={0};pb.allowed_access=access;pb.parent_fd=fd;
    int ok=(int)syscall(__NR_landlock_add_rule,rules_fd,LANDLOCK_RULE_PATH_BENEATH,&pb,0)==0;
    close(fd);return ok;
}
static int sandbox_set_landlock(const char *root,int allow_system_read,int allow_system_exec){
    int abi=(int)syscall(__NR_landlock_create_ruleset,NULL,0,LANDLOCK_CREATE_RULESET_VERSION);
    if(abi<1)return 0;
    struct landlock_ruleset_attr rs={0};
    rs.handled_access_fs=LANDLOCK_ACCESS_FS_EXECUTE|LANDLOCK_ACCESS_FS_WRITE_FILE|LANDLOCK_ACCESS_FS_READ_FILE|LANDLOCK_ACCESS_FS_READ_DIR|LANDLOCK_ACCESS_FS_REMOVE_DIR|LANDLOCK_ACCESS_FS_REMOVE_FILE|LANDLOCK_ACCESS_FS_MAKE_CHAR|LANDLOCK_ACCESS_FS_MAKE_DIR|LANDLOCK_ACCESS_FS_MAKE_REG|LANDLOCK_ACCESS_FS_MAKE_SOCK|LANDLOCK_ACCESS_FS_MAKE_FIFO|LANDLOCK_ACCESS_FS_MAKE_BLOCK|LANDLOCK_ACCESS_FS_MAKE_SYM;
    int rules=(int)syscall(__NR_landlock_create_ruleset,&rs,sizeof(rs),0);if(rules<0)return 0;
    if(!sandbox_add_landlock_path(rules,root,rs.handled_access_fs)){close(rules);return 0;}
    if(allow_system_read || allow_system_exec){
        const char*dirs[]={"/usr","/lib","/lib64","/etc"};
        unsigned sysacc=LANDLOCK_ACCESS_FS_READ_FILE|LANDLOCK_ACCESS_FS_READ_DIR;
        for(size_t i=0;i<sizeof(dirs)/sizeof(dirs[0]);i++)(void)sandbox_add_landlock_path(rules,dirs[i],sysacc);
    }
    if(allow_system_exec){
        const char*dirs[]={"/usr","/bin","/sbin","/lib","/lib64"};
        unsigned execacc=LANDLOCK_ACCESS_FS_EXECUTE;
        for(size_t i=0;i<sizeof(dirs)/sizeof(dirs[0]);i++)(void)sandbox_add_landlock_path(rules,dirs[i],execacc);
    }
    if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)!=0){close(rules);return 0;}
    int ok=(int)syscall(__NR_landlock_restrict_self,rules,0)==0;close(rules);return ok;
}
static int sandbox_set_seccomp(int allow_net,int allow_exec){
    unsigned long deny[] = {
#ifdef __NR_ptrace
        __NR_ptrace,
#endif
#ifdef __NR_process_vm_readv
        __NR_process_vm_readv,
#endif
#ifdef __NR_process_vm_writev
        __NR_process_vm_writev,
#endif
#ifdef __NR_mount
        __NR_mount,
#endif
#ifdef __NR_umount2
        __NR_umount2,
#endif
#ifdef __NR_pivot_root
        __NR_pivot_root,
#endif
#ifdef __NR_chroot
        __NR_chroot,
#endif
#ifdef __NR_setns
        __NR_setns,
#endif
#ifdef __NR_unshare
        __NR_unshare,
#endif
#ifdef __NR_reboot
        __NR_reboot,
#endif
#ifdef __NR_kexec_load
        __NR_kexec_load,
#endif
#ifdef __NR_init_module
        __NR_init_module,
#endif
#ifdef __NR_finit_module
        __NR_finit_module,
#endif
#ifdef __NR_delete_module
        __NR_delete_module,
#endif
#ifdef __NR_open_by_handle_at
        __NR_open_by_handle_at,
#endif
    };
    struct sock_filter f[160];size_t n=0;
    for(size_t i=0;i<sizeof(deny)/sizeof(deny[0]);i++){f[n++]=(struct sock_filter)BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,(unsigned)deny[i],0,1);f[n++]=(struct sock_filter)BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_KILL_PROCESS);}
    if(!allow_exec){
#ifdef __NR_execve
        f[n++]=(struct sock_filter)BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,__NR_execve,0,1);f[n++]=(struct sock_filter)BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_KILL_PROCESS);
#endif
#ifdef __NR_execveat
        f[n++]=(struct sock_filter)BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,__NR_execveat,0,1);f[n++]=(struct sock_filter)BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_KILL_PROCESS);
#endif
    }
    if(!allow_net){
        unsigned long nd[] = {
#ifdef __NR_socket
            __NR_socket,
#endif
#ifdef __NR_socketpair
            __NR_socketpair,
#endif
#ifdef __NR_connect
            __NR_connect,
#endif
#ifdef __NR_accept
            __NR_accept,
#endif
#ifdef __NR_accept4
            __NR_accept4,
#endif
#ifdef __NR_bind
            __NR_bind,
#endif
#ifdef __NR_listen
            __NR_listen,
#endif
#ifdef __NR_sendto
            __NR_sendto,
#endif
#ifdef __NR_recvfrom
            __NR_recvfrom,
#endif
#ifdef __NR_sendmsg
            __NR_sendmsg,
#endif
#ifdef __NR_recvmsg
            __NR_recvmsg,
#endif
        };
        for(size_t i=0;i<sizeof(nd)/sizeof(nd[0]);i++){f[n++]=(struct sock_filter)BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,(unsigned)nd[i],0,1);f[n++]=(struct sock_filter)BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_KILL_PROCESS);}
    }
    f[n++]=(struct sock_filter)BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ALLOW);
    struct sock_fprog p={.len=(unsigned short)n,.filter=f};
    if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)!=0)return 0;
    return prctl(PR_SET_SECCOMP,SECCOMP_MODE_FILTER,&p)==0;
}
static void sandbox_limits(unsigned caps){
    struct rlimit r;
    r.rlim_cur=r.rlim_max=512ULL*1024ULL*1024ULL;(void)setrlimit(RLIMIT_AS,&r);
    r.rlim_cur=r.rlim_max=30;(void)setrlimit(RLIMIT_CPU,&r);
    r.rlim_cur=r.rlim_max=128;(void)setrlimit(RLIMIT_NOFILE,&r);
#ifdef RLIMIT_NPROC
    r.rlim_cur=r.rlim_max=32;(void)setrlimit(RLIMIT_NPROC,&r);
#endif
    r.rlim_cur=r.rlim_max=64ULL*1024ULL*1024ULL;(void)setrlimit(RLIMIT_FSIZE,&r);
    (void)prctl(PR_SET_DUMPABLE,0,0,0,0);
    int allow_net=(caps&(CAP_NET|CAP_WEB|CAP_CLOUD))!=0;
    int allow_exec=(caps&CAP_SYSTEM)!=0;
    (void)sandbox_set_seccomp(allow_net,allow_exec);
}
static int sandbox_platform_harden(unsigned caps,const char *root,char *backend,size_t bsz){
    if(backend&&bsz)snprintf(backend,bsz,"linux-landlock-seccomp");
    sandbox_limits(caps);
    int allow_net=(caps&(CAP_NET|CAP_WEB|CAP_CLOUD))!=0;
    int allow_exec=(caps&CAP_SYSTEM)!=0;
    if(!sandbox_set_landlock(root,allow_net,allow_exec))return 0;
    return 1;
}
#elif defined(__APPLE__)
static int sandbox_platform_harden(unsigned caps,const char *root,char *backend,size_t bsz){
    if(backend&&bsz)snprintf(backend,bsz,"macos-seatbelt");
    (void)setrlimit(RLIMIT_AS,&(struct rlimit){.rlim_cur=512ULL*1024ULL*1024ULL,.rlim_max=512ULL*1024ULL*1024ULL});
    (void)setrlimit(RLIMIT_CPU,&(struct rlimit){.rlim_cur=30,.rlim_max=30});
    char profile[8192];
    const int allow_net=(caps&(CAP_NET|CAP_WEB|CAP_CLOUD))!=0;
    const int allow_exec=(caps&CAP_SYSTEM)!=0;
    int w=snprintf(profile,sizeof profile,
      "(version 1)\n"
      "(deny default)\n"
      "(allow process*)\n"
      "(allow sysctl-read)\n"
      "(allow mach-lookup)\n"
      "(allow file-read* (subpath /System))\n"
      "(allow file-read* (subpath /usr))\n"
      "(allow file-read* (subpath /Library))\n"
      "(allow file-read* (subpath /private/etc))\n"
      "(allow file-read* (subpath \"%s\"))\n"
      "(allow file-write* (subpath \"%s\"))\n"
      "%s%s%s",
      root,root,
      allow_net?"(allow network-outbound)\n":"",
      allow_exec?"(allow process-exec)\n":"",
      "");
    if(w<0||(size_t)w>=sizeof(profile))return 0;
    char *err=NULL;int rc=sandbox_init(profile,SANDBOX_NAMED, &err);if(rc!=0){if(err)sandbox_free_error(err);return 0;}return 1;
}
#elif defined(__OpenBSD__)
static int sandbox_platform_harden(unsigned caps,const char *root,char *backend,size_t bsz){
    if(backend&&bsz)snprintf(backend,bsz,"openbsd-pledge-unveil");
    if(unveil(root,"rwc")!=0)return 0;
    (void)unveil("/usr/lib","r");(void)unveil("/usr/local/lib","r");(void)unveil("/etc","r");(void)unveil(NULL,NULL);
    char p[256]="stdio rpath wpath cpath";if(caps&(CAP_NET|CAP_WEB|CAP_CLOUD))strcat(p," inet dns");if(caps&CAP_SYSTEM)strcat(p," proc exec");
    if(pledge(p,NULL)!=0)return 0;return 1;
}
#elif defined(__FreeBSD__)
static int sandbox_platform_harden(unsigned caps,const char *root,char *backend,size_t bsz){
    (void)root;if(backend&&bsz)snprintf(backend,bsz,"freebsd-capsicum");
    (void)setrlimit(RLIMIT_AS,&(struct rlimit){.rlim_cur=512ULL*1024ULL*1024ULL,.rlim_max=512ULL*1024ULL*1024ULL});
    if((caps&(CAP_NET|CAP_WEB|CAP_CLOUD))!=0)return 0; /* network needs a broker/Casper setup */
    return cap_enter()==0;
}
#elif defined(_WIN32)
static int sandbox_windows_job(HANDLE process,HANDLE *job_out,unsigned caps){
    HANDLE job=CreateJobObjectW(NULL,NULL);if(!job)return 0;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION eli;ZeroMemory(&eli,sizeof eli);
    eli.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS|JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    eli.BasicLimitInformation.ActiveProcessLimit=4;eli.ProcessMemoryLimit=(SIZE_T)512ULL*1024ULL*1024ULL;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&eli,sizeof eli)){CloseHandle(job);return 0;}
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY ip;ZeroMemory(&ip,sizeof ip);ip.NoRemoteImages=1;ip.NoLowMandatoryLabelImages=1;(void)SetProcessMitigationPolicy(ProcessImageLoadPolicy,&ip,sizeof ip);
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY ep;ZeroMemory(&ep,sizeof ep);ep.DisableExtensionPoints=1;(void)SetProcessMitigationPolicy(ProcessExtensionPointDisablePolicy,&ep,sizeof ep);
    (void)caps;
    if(!AssignProcessToJobObject(job,process)){CloseHandle(job);return 0;}if(job_out)*job_out=job;else CloseHandle(job);return 1;
}
static int sandbox_platform_harden(unsigned caps,const char *root,char *backend,size_t bsz){
    (void)root;(void)caps;if(backend&&bsz)snprintf(backend,bsz,"windows-job-object");
    (void)SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);return 1;
}
#else
static int sandbox_platform_harden(unsigned caps,const char *root,char *backend,size_t bsz){
    (void)caps;(void)root;if(backend&&bsz)snprintf(backend,bsz,"unsupported-fail-closed");return 0;
}
#endif

static int spawn_sandboxed_module(const char *path,unsigned caps){
#ifdef HARIS_ANDROID
    (void)path;(void)caps;
    fprintf(stderr,"Haris Android: process sandbox workers are unavailable; use the app process sandbox.\n");
    return 3;
#else
#ifdef _WIN32
    if(!path||!*path)return -1;
    char*src=readf_limit(path,H_SOURCE_MAX);if(!src)return -1;
    caps &= ~CAP_NUCLEAR;
    if(!sandbox_caps_safe(caps)){xfree(src);fprintf(stderr,"Haris security: hardware/raw capabilities are broker-only\n");return 3;}
    Value r=sandbox_run_code_windows(NULL,src,caps);xfree(src);
    if(r.t!=VSTRUCT)return 3;
    Value out=stget(r.u.st,"output");if(out.t==VSTR&&out.u.s)fputs(out.u.s,stdout);
    Value ok=stget(r.u.st,"ok");return (ok.t==VBOOL&&ok.u.b)?0:1;
#elif defined(__unix__) || defined(__APPLE__)
    char self[PATH_MAX];if(g_program_path&&*g_program_path&&!realpath(g_program_path,self))snprintf(self,sizeof self,"%s",g_program_path);else if(g_program_path&&*g_program_path)snprintf(self,sizeof self,"%s",g_program_path);else return -1;
    char abs[PATH_MAX];if(!realpath(path,abs))return -1;char capsbuf[64];snprintf(capsbuf,sizeof capsbuf,"%u",caps & ~CAP_NUCLEAR);
    pid_t pid=fork();if(pid<0)return -1;if(pid==0){extern char **environ;environ[0]=NULL;char *const av[]={(char*)self,(char*)"--sandbox-worker",(char*)abs,(char*)"--module-caps",capsbuf,NULL};execv(self,av);_exit(126);}int st=0;if(waitpid(pid,&st,0)<0)return -1;return WIFEXITED(st)?WEXITSTATUS(st):128+(WIFSIGNALED(st)?WTERMSIG(st):1);
#else
    (void)path; (void)caps; return -2;
#endif
#endif
}

