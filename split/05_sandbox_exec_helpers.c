/* -------------------------------------------------------------------------
   Sandbox-first execution helpers. These run untrusted/dangerous code in a
   short-lived worker process. On Linux the worker is confined with Landlock,
   no_new_privs, seccomp, CPU/RAM/file/process limits and explicit capabilities.
   The host process never executes the requested snippet itself.
   ------------------------------------------------------------------------- */
#ifndef HARIS_SANDBOX_MAX_OUTPUT
#define HARIS_SANDBOX_MAX_OUTPUT (8ULL*1024ULL*1024ULL)
#endif

static unsigned long long sandbox_clock_ms(void);
static int sandbox_read_stdin_source(char **out,size_t *outn){
    if(!out||!outn)return 0;*out=NULL;*outn=0;size_t cap=4096;char*b=(char*)xmalloc(cap);const unsigned long long deadline=sandbox_clock_ms()+5000ULL;
    for(;;){
        if(*outn+1>=cap){size_t nc=cap*2;if(nc>H_SOURCE_MAX+1)nc=H_SOURCE_MAX+1;if(nc<=cap){xfree(b);return 0;}b=(char*)xrealloc(b,nc);cap=nc;}
#ifndef _WIN32
        unsigned long long now=sandbox_clock_ms();if(now>=deadline){xfree(b);return 0;}int wait_ms=(int)((deadline-now)>100ULL?100ULL:(deadline-now));struct pollfd pfd={STDIN_FILENO,POLLIN|POLLHUP|POLLERR,0};int pr=poll(&pfd,1,wait_ms);if(pr<0&&errno==EINTR)continue;if(pr<0){xfree(b);return 0;}if(pr==0)continue;if(!(pfd.revents&(POLLIN|POLLHUP|POLLERR)))continue;
#else
        HANDLE hin=GetStdHandle(STD_INPUT_HANDLE);DWORD wr=WaitForSingleObject(hin,100);if(wr==WAIT_TIMEOUT){if(sandbox_clock_ms()>=deadline){xfree(b);return 0;}continue;}if(wr==WAIT_FAILED){xfree(b);return 0;}
#endif
#ifdef _WIN32
        int got=_read(_fileno(stdin),b+*outn,(unsigned)(cap-*outn-1));
#else
        ssize_t got=read(STDIN_FILENO,b+*outn,cap-*outn-1);
#endif
        if(got>0){*outn+=(size_t)got;if(*outn>H_SOURCE_MAX){xfree(b);return 0;}continue;}
        if(got==0)break;
        if(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK){if(sandbox_clock_ms()>=deadline){xfree(b);return 0;}continue;}
        xfree(b);return 0;
    }
    b[*outn]=0;*out=b;return 1;
}
static int run_sandbox_stdin_worker(unsigned caps){
    char*src=NULL;size_t n=0;if(!sandbox_read_stdin_source(&src,&n))return 2;
    VM vm;init(&vm);vm.module_sandboxed=1;vm.sandbox_worker=1;vm.sandbox_auto=1;vm.module_caps=caps & ~CAP_NUCLEAR;vm.host_module_caps=vm.module_caps;
    char cwd[PATH_MAX];if(!getcwd(cwd,sizeof cwd))snprintf(cwd,sizeof cwd,".");vm.sandbox_root=cwd;
    caps &= ~CAP_NUCLEAR;
    if(!sandbox_caps_safe(caps)){fprintf(stderr,"Haris security: hardware/raw capabilities are broker-only\n");xfree(src);return 3;}
    if(!sandbox_platform_harden(caps,cwd,vm.sandbox_backend,sizeof vm.sandbox_backend) && sandbox_caps_are_privileged(caps)){fprintf(stderr,"Haris security: no native sandbox backend available; privileged capability denied\n");xfree(src);return 3;}
    char why[160]={0};if(!module_scan(src,caps,why,sizeof why)){fprintf(stderr,"Haris sandbox: denied: %s\n",why);xfree(src);return 3;}
    Fn*f=compile(src,"<sandbox>");xfree(src);run(&vm,f,0,0);fflush(stdout);return vm.last_error?1:0;
}
static unsigned long long sandbox_clock_ms(void){
#ifdef _WIN32
    return (unsigned long long)clock()*1000ULL/(unsigned long long)(CLOCKS_PER_SEC?CLOCKS_PER_SEC:1);
#else
    struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (unsigned long long)ts.tv_sec*1000ULL+(unsigned long long)ts.tv_nsec/1000000ULL;
#endif
}
#ifdef _WIN32
static int win_utf8_to_wide(const char*s,wchar_t*out,int cap){if(!s||!out||cap<=0)return 0;int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,out,cap);return n>0;}
static int win_appcontainer_sid(PSID*out_sid){
    HRESULT hr=DeriveAppContainerSidFromAppContainerName(L"HarisForgeSandbox",out_sid);
    if(SUCCEEDED(hr)&&*out_sid)return 1;
    hr=CreateAppContainerProfile(L"HarisForgeSandbox",L"Haris Forge Sandbox",L"Haris Forge per-process sandbox",NULL,0,out_sid);
    return SUCCEEDED(hr)&&*out_sid;
}
static int win_cap_sid(LPCWSTR name,PSID*out_sid){PSID *gs=NULL,*cs=NULL;DWORD gn=0,cn=0;if(!DeriveCapabilitySidsFromName(name,&gs,&gn,&cs,&cn)||cn<1){if(gs)LocalFree(gs);if(cs)LocalFree(cs);return 0;}if(gn){for(DWORD i=0;i<gn;i++)if(gs[i])LocalFree(gs[i]);LocalFree(gs);}*out_sid=cs[0];for(DWORD i=1;i<cn;i++)LocalFree(cs[i]);LocalFree(cs);return 1;}
static int win_grant_dir_acl(const wchar_t*dir,PSID sid){
    PSECURITY_DESCRIPTOR sd=NULL;PACL oldacl=NULL,acl=NULL;DWORD rc=GetNamedSecurityInfoW((LPWSTR)dir,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,&oldacl,NULL,&sd);if(rc!=ERROR_SUCCESS&&rc!=ERROR_FILE_NOT_FOUND)return 0;
    EXPLICIT_ACCESSW ea;ZeroMemory(&ea,sizeof ea);ea.grfAccessPermissions=GENERIC_READ|GENERIC_WRITE|DELETE|FILE_LIST_DIRECTORY|FILE_ADD_FILE|FILE_ADD_SUBDIRECTORY;ea.grfAccessMode=GRANT_ACCESS;ea.grfInheritance=SUB_CONTAINERS_AND_OBJECTS_INHERIT;ea.Trustee.TrusteeForm=TRUSTEE_IS_SID;ea.Trustee.TrusteeType=TRUSTEE_IS_WELL_KNOWN_GROUP;ea.Trustee.ptstrName=(LPWSTR)sid;
    rc=SetEntriesInAclW(1,&ea,oldacl,&acl);if(rc!=ERROR_SUCCESS){if(sd)LocalFree(sd);return 0;}rc=SetNamedSecurityInfoW((LPWSTR)dir,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,acl,NULL);if(acl)LocalFree(acl);if(sd)LocalFree(sd);return rc==ERROR_SUCCESS;
}
static void win_remove_tree(const wchar_t*dir){WIN32_FIND_DATAW fd;wchar_t pat[32768];_snwprintf(pat,sizeof pat/sizeof(*pat),L"%s\\*",dir);HANDLE h=FindFirstFileW(pat,&fd);if(h!=INVALID_HANDLE_VALUE){do{if(!wcscmp(fd.cFileName,L".")||!wcscmp(fd.cFileName,L".."))continue;wchar_t pth[32768];_snwprintf(pth,sizeof pth/sizeof(*pth),L"%s\\%s",dir,fd.cFileName);if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){win_remove_tree(pth);RemoveDirectoryW(pth);}else{SetFileAttributesW(pth,FILE_ATTRIBUTE_NORMAL);DeleteFileW(pth);}}while(FindNextFileW(h,&fd));FindClose(h);}RemoveDirectoryW(dir);}
static int win_set_job(HANDLE process,HANDLE *out_job){HANDLE job=CreateJobObjectW(NULL,NULL);if(!job)return 0;JOBOBJECT_EXTENDED_LIMIT_INFORMATION eli;ZeroMemory(&eli,sizeof eli);eli.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS|JOB_OBJECT_LIMIT_PROCESS_MEMORY;eli.BasicLimitInformation.ActiveProcessLimit=4;eli.ProcessMemoryLimit=(SIZE_T)512ULL*1024ULL*1024ULL;if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&eli,sizeof eli)||!AssignProcessToJobObject(job,process)){CloseHandle(job);return 0;}*out_job=job;return 1;}
static int win_harden_current_process(void){PROCESS_MITIGATION_IMAGE_LOAD_POLICY ip;ZeroMemory(&ip,sizeof ip);ip.NoRemoteImages=1;ip.NoLowMandatoryLabelImages=1;(void)SetProcessMitigationPolicy(ProcessImageLoadPolicy,&ip,sizeof ip);PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY ep;ZeroMemory(&ep,sizeof ep);ep.DisableExtensionPoints=1;(void)SetProcessMitigationPolicy(ProcessExtensionPointDisablePolicy,&ep,sizeof ep);SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);return 1;}
static Value sandbox_run_code_windows(VM*vm,const char*src,unsigned caps){
    if(!src)return vn();if(strlen(src)>H_SOURCE_MAX)return vn();
    if(caps&CAP_NUCLEAR)caps&=~CAP_NUCLEAR;
    if((caps&(CAP_SYSTEM|CAP_HW_ANY))!=0){fprintf(stderr,"Haris security: Windows AppContainer sandbox does not grant system/hardware access; request denied\\n");return vn();}
    PSID appSid=NULL; if(!win_appcontainer_sid(&appSid)){fprintf(stderr,"Haris security: AppContainer unavailable\\n");return vn();}
    PSID netSid=NULL;SID_AND_ATTRIBUTES capAttr[1];DWORD capCount=0;if(caps&(CAP_NET|CAP_WEB|CAP_CLOUD)){if(!win_cap_sid(L"internetClient",&netSid)){LocalFree(appSid);return vn();}capAttr[0].Sid=netSid;capAttr[0].Attributes=SE_GROUP_ENABLED;capCount=1;}
    wchar_t root[32768];wchar_t temp[32768];DWORD tn=GetTempPathW((DWORD)(sizeof(temp)/sizeof(*temp)),temp);if(!tn||tn>=sizeof(temp)/sizeof(*temp)){if(netSid)LocalFree(netSid);LocalFree(appSid);return vn();}SYSTEMTIME st;GetSystemTime(&st);DWORD pid=GetCurrentProcessId();_snwprintf(root,sizeof root/sizeof(*root),L"%sHarisForge-%lu-%04u%02u%02u%02u%02u%02u",temp,(unsigned long)pid,st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);if(!CreateDirectoryW(root,NULL)){if(netSid)LocalFree(netSid);LocalFree(appSid);return vn();}if((caps&CAP_FS)&&!win_grant_dir_acl(root,appSid)){win_remove_tree(root);if(netSid)LocalFree(netSid);LocalFree(appSid);return vn();}
    SECURITY_CAPABILITIES sc;ZeroMemory(&sc,sizeof sc);sc.AppContainerSid=appSid;sc.Capabilities=capAttr;sc.CapabilityCount=capCount;
    SIZE_T attrSize=0;InitializeProcThreadAttributeList(NULL,1,0,&attrSize);LPPROC_THREAD_ATTRIBUTE_LIST al=(LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(),0,attrSize);if(!al||!InitializeProcThreadAttributeList(al,1,0,&attrSize)||!UpdateProcThreadAttribute(al,0,PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,&sc,sizeof sc,NULL)){if(al)HeapFree(GetProcessHeap(),0,al);win_remove_tree(root);if(netSid)LocalFree(netSid);LocalFree(appSid);return vn();}
    wchar_t exe[MAX_PATH*4];DWORD en=GetModuleFileNameW(NULL,exe,(DWORD)(sizeof(exe)/sizeof(*exe)));if(!en){DeleteProcThreadAttributeList(al);HeapFree(GetProcessHeap(),0,al);win_remove_tree(root);if(netSid)LocalFree(netSid);LocalFree(appSid);return vn();}
    wchar_t cmd[1024];_snwprintf(cmd,sizeof cmd/sizeof(*cmd),L"\"%s\" --sandbox-stdin-worker --module-caps %u",exe,caps);
    SECURITY_ATTRIBUTES sa;ZeroMemory(&sa,sizeof sa);sa.nLength=sizeof sa;sa.bInheritHandle=TRUE;HANDLE inR=NULL,inW=NULL,outR=NULL,outW=NULL;if(!CreatePipe(&inR,&inW,&sa,0)||!CreatePipe(&outR,&outW,&sa,0)){if(inR)CloseHandle(inR);if(inW)CloseHandle(inW);if(outR)CloseHandle(outR);if(outW)CloseHandle(outW);DeleteProcThreadAttributeList(al);HeapFree(GetProcessHeap(),0,al);win_remove_tree(root);if(netSid)LocalFree(netSid);LocalFree(appSid);return vn();}SetHandleInformation(inW,HANDLE_FLAG_INHERIT,0);SetHandleInformation(outR,HANDLE_FLAG_INHERIT,0);
    STARTUPINFOEXW si;ZeroMemory(&si,sizeof si);si.StartupInfo.cb=sizeof si;si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;si.StartupInfo.hStdInput=inR;si.StartupInfo.hStdOutput=outW;si.StartupInfo.hStdError=outW;si.lpAttributeList=al;PROCESS_INFORMATION pi;ZeroMemory(&pi,sizeof pi);wchar_t empty_env[2]={0,0};DWORD flags=CREATE_UNICODE_ENVIRONMENT|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT|CREATE_SUSPENDED;BOOL ok=CreateProcessW(exe,cmd,NULL,NULL,TRUE,flags,empty_env,root,&si.StartupInfo,&pi);CloseHandle(inR);CloseHandle(outW);if(!ok){CloseHandle(inW);CloseHandle(outR);DeleteProcThreadAttributeList(al);HeapFree(GetProcessHeap(),0,al);win_remove_tree(root);if(netSid)LocalFree(netSid);LocalFree(appSid);return vn();}
    HANDLE job=NULL;if(!win_set_job(pi.hProcess,&job)){TerminateProcess(pi.hProcess,124);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(inW);CloseHandle(outR);DeleteProcThreadAttributeList(al);HeapFree(GetProcessHeap(),0,al);win_remove_tree(root);if(netSid)LocalFree(netSid);LocalFree(appSid);return vn();}ResumeThread(pi.hThread);DWORD written=0;BOOL wr=WriteFile(inW,src,(DWORD)strlen(src),&written,NULL);CloseHandle(inW);
    char*buf=(char*)xmalloc(4096);size_t len=0,cap=4096;int timed=0,truncated=0;DWORD start=GetTickCount();for(;;){DWORD avail=0;if(PeekNamedPipe(outR,NULL,0,NULL,&avail,NULL)&&avail){if(len+(size_t)avail+1>HARIS_SANDBOX_MAX_OUTPUT){truncated=1;TerminateJobObject(job,124);break;}if(len+(size_t)avail+1>=cap){size_t nc=cap;while(nc<len+(size_t)avail+1)nc*=2;buf=(char*)xrealloc(buf,nc);cap=nc;}DWORD got=0;if(!ReadFile(outR,buf+len,avail,&got,NULL)||!got)break;len+=got;buf[len]=0;}DWORD w=WaitForSingleObject(pi.hProcess,25);if(w==WAIT_OBJECT_0)break;if(GetTickCount()-start>35000UL){timed=1;TerminateJobObject(job,125);break;}}
    for(;;){DWORD avail=0;if(!PeekNamedPipe(outR,NULL,0,NULL,&avail,NULL)||!avail)break;if(len+(size_t)avail+1>HARIS_SANDBOX_MAX_OUTPUT){truncated=1;break;}if(len+(size_t)avail+1>=cap){size_t nc=cap;while(nc<len+(size_t)avail+1)nc*=2;buf=(char*)xrealloc(buf,nc);cap=nc;}DWORD got=0;if(!ReadFile(outR,buf+len,avail,&got,NULL)||!got)break;len+=got;buf[len]=0;}
    DWORD exitc=1;GetExitCodeProcess(pi.hProcess,&exitc);CloseHandle(outR);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(job);DeleteProcThreadAttributeList(al);HeapFree(GetProcessHeap(),0,al);if(netSid)LocalFree(netSid);LocalFree(appSid);win_remove_tree(root);buf[len]=0;Value r=vsobj();stput(r.u.st,"ok",vb(wr&&exitc==0&&!timed&&!truncated));stput(r.u.st,"status",vi((long long)exitc));stput(r.u.st,"output",vs(buf));stput(r.u.st,"timed_out",vb(timed));stput(r.u.st,"output_truncated",vb(truncated));stput(r.u.st,"backend",vs("windows-appcontainer+job"));xfree(buf);(void)vm;return r;
}
#endif

static Value sandbox_run_code(VM*vm,int n,Value*a){
    if(n<1||n>2||a[0].t!=VSTR)return vn();
    unsigned caps=CAP_SAFE_DEFAULT;
    if(n==2&&a[1].t==VARR){
        for(size_t i=0;i<a[1].u.a->n;i++){Value v=a[1].u.a->v[i];if(v.t!=VSTR)continue;unsigned c=cap_from_name(v.u.s);
            if(!c){fprintf(stderr,"Haris security: unknown sandbox capability '%s'\n",v.u.s);return vn();}caps|=c;
        }
    }
    char auth_reason[192]={0};
    if(!sandbox_authorize_caps(vm,caps,auth_reason,sizeof auth_reason)){fprintf(stderr,"Haris security: sandbox denied: %s\n",auth_reason);return vn();}
    /* Red-team requires the documented authorization above; an audit file is
       optional for redteam and mandatory for pentest, matching the capability
       contract. If a redteam audit path was supplied, record it opportunistically. */
    if((caps&CAP_REDTEAM)&&vm->audit_log_path&&*vm->audit_log_path){(void)audit_log_event(vm,"redteam",caps);}
    if((caps&CAP_PENTEST)&&!audit_log_event(vm,"pentest",caps))return cap_error(vm,CAP_PENTEST,"pentest.audit_log");
    caps &= ~CAP_NUCLEAR;
#ifdef _WIN32
    return sandbox_run_code_windows(vm,a[0].u.s,caps);
#else
    char self[PATH_MAX];if(!realpath(g_program_path,self))snprintf(self,sizeof self,"%s",g_program_path);
    char sandbox_root[PATH_MAX]="/tmp/haris-sbox-XXXXXX";
    if(!mkdtemp(sandbox_root))return vn();
    int pin[2]={-1,-1},pout[2]={-1,-1};
    if(pipe(pin)!=0){remove_tree(sandbox_root);return vn();}
    if(pipe(pout)!=0){close(pin[0]);close(pin[1]);remove_tree(sandbox_root);return vn();}
    pid_t pid=fork();
    if(pid<0){close(pin[0]);close(pin[1]);close(pout[0]);close(pout[1]);remove_tree(sandbox_root);return vn();}
    if(pid==0){
        close(pin[1]);close(pout[0]);
        if(dup2(pin[0],STDIN_FILENO)<0)_exit(125);
        if(dup2(pout[1],STDOUT_FILENO)<0)_exit(125);
        close(pin[0]);close(pout[1]);
        if(chdir(sandbox_root)!=0)_exit(125);
        /* Do not inherit API keys, proxy settings, LD_PRELOAD, or other host
           environment state into an untrusted worker. */
        extern char **environ;
        environ[0]=NULL;
        char capsbuf[32];snprintf(capsbuf,sizeof capsbuf,"%u",caps);
        char *const av[]={(char*)self,(char*)"--sandbox-stdin-worker",(char*)"--module-caps",capsbuf,NULL};
        execv(self,av);_exit(126);
    }
    close(pin[0]);close(pout[1]);
    int fl=fcntl(pout[0],F_GETFL,0);if(fl>=0)fcntl(pout[0],F_SETFL,fl|O_NONBLOCK);
    struct sigaction oldpipe,ignpipe;memset(&ignpipe,0,sizeof ignpipe);ignpipe.sa_handler=SIG_IGN;sigemptyset(&ignpipe.sa_mask);sigaction(SIGPIPE,&ignpipe,&oldpipe);
    const char*src=a[0].u.s;size_t sn=strlen(src),sent=0,cap=4096,len=0;char*buf=(char*)xmalloc(cap);
    int in_open=1,done=0,timed=0,truncated=0,st=0;unsigned long long deadline=sandbox_clock_ms()+35000ULL;
    while(!done){
        struct pollfd pf[2];nfds_t np=0;int out_idx=(int)np;pf[np].fd=pout[0];pf[np].events=POLLIN|POLLHUP|POLLERR;np++;int in_idx=-1;
        if(in_open&&sent<sn){in_idx=(int)np;pf[np].fd=pin[1];pf[np].events=POLLOUT|POLLHUP|POLLERR;np++;}
        int pr=poll(pf,np,100);
        if(pr<0&&errno==EINTR)continue;
        if(pf[out_idx].revents&(POLLIN|POLLHUP|POLLERR)){
            for(;;){
                if(len+1>=cap){size_t nc=cap<1048576?cap*2:cap+(cap>>1);if(nc>HARIS_SANDBOX_MAX_OUTPUT+1)nc=HARIS_SANDBOX_MAX_OUTPUT+1;if(nc<=cap){truncated=1;kill(pid,SIGKILL);break;}buf=(char*)xrealloc(buf,nc);cap=nc;}
                ssize_t got=read(pout[0],buf+len,cap-len-1);
                if(got>0){len+=(size_t)got;if(len>=HARIS_SANDBOX_MAX_OUTPUT){truncated=1;kill(pid,SIGKILL);break;}continue;}
                if(got<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))break;
                if(got==0)done=1;break;
            }
        }
        if(in_idx>=0&&(pf[in_idx].revents&(POLLOUT|POLLHUP|POLLERR))){
            if(sent<sn){ssize_t w=write(pin[1],src+sent,sn-sent>65536?65536:sn-sent);if(w>0)sent+=(size_t)w;else if(w<0&&(errno==EPIPE||errno==EBADF)){close(pin[1]);in_open=0;}}
            if(sent==sn){close(pin[1]);in_open=0;}
        }
        pid_t w=waitpid(pid,&st,WNOHANG);if(w==pid)done=1;
        if(sandbox_clock_ms()>=deadline){timed=1;kill(pid,SIGKILL);waitpid(pid,&st,0);done=1;}
        if(truncated){waitpid(pid,&st,0);done=1;}
    }
    if(in_open)close(pin[1]);
    for(;;){if(len+1>=cap)break;ssize_t got=read(pout[0],buf+len,cap-len-1);if(got>0){len+=(size_t)got;continue;}if(got<0&&errno==EINTR)continue;break;}
    close(pout[0]);if(!done)waitpid(pid,&st,0);
    (void)remove_tree(sandbox_root);
    sigaction(SIGPIPE,&oldpipe,NULL);buf[len]=0;
    Value out=vsobj();stput(out.u.st,"ok",vb(WIFEXITED(st)&&WEXITSTATUS(st)==0&&!timed&&!truncated));stput(out.u.st,"status",vi(WIFEXITED(st)?WEXITSTATUS(st):-1));stput(out.u.st,"output",vs(buf));stput(out.u.st,"timed_out",vb(timed));stput(out.u.st,"output_truncated",vb(truncated));xfree(buf);(void)vm;return out;
#endif
}

static char *sandbox_source_call(const char *fn,const char *arg){
    size_t alen=arg?strlen(arg):0;
    if(alen>H_SOURCE_MAX/2)return NULL;
    size_t cap=alen*2+128;
    char *out=(char*)xmalloc(cap);size_t pos=0;
    int w=snprintf(out,cap,"%s(\"",fn);if(w<0||(size_t)w>=cap){xfree(out);return NULL;}pos=(size_t)w;
    if(!json_append_escaped(out,cap,&pos,arg?arg:"")){xfree(out);return NULL;}
    if(pos+3>=cap){xfree(out);return NULL;}out[pos++]='"';out[pos++]=')';out[pos++]='\n';out[pos]=0;return out;
}
static Value sandbox_system(VM*vm,int n,Value*a){
    if(n!=1||a[0].t!=VSTR)return vn();
    char *code=sandbox_source_call("system.run",a[0].u.s);if(!code)return vn();
    Value ca[2]={vs(code),vn()};Value r;
    /* sandbox.run's capability argument is optional; supply a one-item array explicitly. */
    ca[1]=va();ap(ca[1].u.a,vs("system"));r=sandbox_run_code(vm,2,ca);xfree(code);return r;
}
static Value sandbox_read(VM*vm,int n,Value*a){
    if(n!=1||a[0].t!=VSTR)return vn();
    if(!safe_path_arg(a[0].u.s))return vn();
    char *code=sandbox_source_call("file.read",a[0].u.s);if(!code)return vn();
    /* Ask the worker to print a compact result; stdout is the safe IPC channel. */
    size_t z=strlen(code);char *wrap=(char*)xmalloc(z+8);memcpy(wrap,"print(",6);memcpy(wrap+6,code, z);wrap[z+6]=')';wrap[z+7]=0;xfree(code);
    Value ca[2]={vs(wrap),va()};xfree(wrap);ap(ca[1].u.a,vs("fs"));Value r=sandbox_run_code(vm,2,ca);return r;
}
static Value sandbox_write(VM*vm,int n,Value*a){
    if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR)return vn();
    if(!safe_path_arg(a[0].u.s))return vn();
    size_t plen=strlen(a[0].u.s),vlen=strlen(a[1].u.s);if(vlen>H_DATA_MAX/2)return vn();
    size_t cap=plen*2+vlen*2+256;char *code=(char*)xmalloc(cap);size_t pos=0;int w=snprintf(code,cap,"file.write(\"");if(w<0||(size_t)w>=cap){xfree(code);return vn();}pos=(size_t)w;
    if(!json_append_escaped(code,cap,&pos,a[0].u.s)){xfree(code);return vn();}if(pos+4>=cap){xfree(code);return vn();}code[pos++]=',';code[pos++]='"';
    if(!json_append_escaped(code,cap,&pos,a[1].u.s)){xfree(code);return vn();}if(pos+3>=cap){xfree(code);return vn();}code[pos++]='"';code[pos++]=')';code[pos++]='\n';code[pos]=0;
    Value ca[2]={vs(code),va()};xfree(code);ap(ca[1].u.a,vs("fs"));return sandbox_run_code(vm,2,ca);
}

static Value sandbox_info(VM*vm,int n,Value*a){(void)a;if(n)return vn();Value o=vsobj();stput(o.u.st,"worker",vb(vm->sandbox_worker));stput(o.u.st,"auto",vb(vm->sandbox_auto));stput(o.u.st,"platform",vs(
#ifdef __EMSCRIPTEN__
"wasm"
#elif defined(_WIN32)
"windows"
#elif defined(__APPLE__)
"macos"
#elif defined(__linux__)
"linux"
#elif defined(__FreeBSD__)
"freebsd"
#elif defined(__OpenBSD__)
"openbsd"
#else
"other"
#endif
));stput(o.u.st,"backend",vs(vm->sandbox_backend[0]?vm->sandbox_backend:"unconfigured"));stput(o.u.st,"limits",vs("CPU/RAM/files/processes + OS sandbox when available"));stput(o.u.st,"hardware",vs("CPU/GPU/board raw access denied in sandbox; broker-only"));return o;}


#define HARIS_PACK_MAX_TOTAL (256ULL*1024ULL*1024ULL)
#define HARIS_PACK_MAX_ARCHIVE HARIS_PACK_MAX_TOTAL
#define HARIS_PACK_MAX HARIS_PACK_MAX_ARCHIVE
#define HARIS_PACK_MAX_ENTRIES 512u

/* Pack v2: [native executable][pack header][archive][pack footer].
   Header/footer are mirrored so the loader can distinguish a real package
   from an ordinary executable that merely ends with the magic bytes. */
#define HARIS_PACK_MAGIC "HARISPK10"
#define HARIS_PACK_MAGIC_LEN 9u
#define HARIS_PACK_VERSION 2u
#define HARIS_PACK_FLAGS 0u
#define HARIS_PACK_FLAG_ZSTD (1u<<0)
#define HARIS_PACK_FLAG_NATIVE_CACHE (1u<<1)
#define HARIS_PACK_FLAG_METADATA (1u<<2)
#define HARIS_PACK_KNOWN_FLAGS (HARIS_PACK_FLAG_ZSTD|HARIS_PACK_FLAG_NATIVE_CACHE|HARIS_PACK_FLAG_METADATA)
#define HARIS_PACK_INVALID (-1)
#define HARIS_PACK_VERSION_OFF HARIS_PACK_MAGIC_LEN
#define HARIS_PACK_FLAGS_OFF (HARIS_PACK_VERSION_OFF + 4u)
#define HARIS_PACK_ARCHIVE_SIZE_OFF (HARIS_PACK_FLAGS_OFF + 4u)
#define HARIS_PACK_SHA_OFF (HARIS_PACK_ARCHIVE_SIZE_OFF + 8u)
#define HARIS_PACK_HEADER_SIZE (HARIS_PACK_SHA_OFF + 64u)
#define HARIS_PACK_FOOTER_SIZE HARIS_PACK_HEADER_SIZE

#define HARIS_ARCH_MAGIC "HARISAR10"
#define HARIS_ARCH_MAGIC_LEN 9u
#define HARIS_ARCH_VERSION 1u
#define HARIS_ARCH_HEADER_SIZE (HARIS_ARCH_MAGIC_LEN + 4u + 4u)

typedef struct {
    char *path;
    char *src;
    size_t n;
} HPackEntry;
static HPackEntry *g_pack_entries=NULL;
static size_t g_pack_entry_n=0;
static int g_pack_loaded=0;
static int haris_pack_path_safe(const char*p){
    if(!p||!*p||p[0]=='/'||p[0]=='\\')return 0;
    if(isalpha((unsigned char)p[0])&&p[1]==':')return 0;
    const char*q=p;while(*q){const char*e=strpbrk(q,"/\\");size_t n=e?(size_t)(e-q):strlen(q);if(n==2&&q[0]=='.'&&q[1]=='.')return 0;q=e?e+1:q+strlen(q);}
    return 1;
}
static void haris_pack_runtime_clear(void){
    for(size_t i=0;i<g_pack_entry_n;i++){free(g_pack_entries[i].path);free(g_pack_entries[i].src);}free(g_pack_entries);g_pack_entries=NULL;g_pack_entry_n=0;g_pack_loaded=0;
}
static int haris_pack_find(const char*p){if(!p)return -1;for(size_t i=0;i<g_pack_entry_n;i++)if(!strcmp(g_pack_entries[i].path,p))return (int)i;return -1;}
static int haris_pack_lookup_module(VM*vm,const char*m,char*out,size_t outcap,char**src){
    (void)vm;if(!g_pack_loaded||!m||!*m||!out||!outcap||!src)return 0;*src=NULL;out[0]=0;if(!haris_pack_path_safe(m))return 0;
    char cands[5][PATH_MAX];int n=0;
    if(strstr(m,".hr")){snprintf(cands[n++],sizeof cands[0],"%s",m);}else{
        snprintf(cands[n++],sizeof cands[0],"%s.hr",m);
        snprintf(cands[n++],sizeof cands[0],"modules/%s.hr",m);
        snprintf(cands[n++],sizeof cands[0],"modules/%s/__init__.hr",m);
        snprintf(cands[n++],sizeof cands[0],"packages/%s/__init__.hr",m);
    }
    for(int k=0;k<n;k++){if(!haris_pack_path_safe(cands[k]))continue;int i=haris_pack_find(cands[k]);if(i>=0){if(strlen(cands[k])+1>outcap)return 0;snprintf(out,outcap,"%s",cands[k]);*src=xmalloc(g_pack_entries[i].n+1);memcpy(*src,g_pack_entries[i].src,g_pack_entries[i].n+1);return 1;}}
    return 0;
}

static int module_trusted(VM*vm,const char*m){for(int i=0;i<vm->ntrusted;i++)if(!strcmp(vm->trusted_modules[i],m))return 1;return 0;}
static void add_trusted_module(VM*vm,const char*m){if(module_trusted(vm,m))return;if(vm->ntrusted==vm->ctrusted){vm->ctrusted=vm->ctrusted?vm->ctrusted*2:8;vm->trusted_modules=xrealloc(vm->trusted_modules,(size_t)vm->ctrusted*sizeof(char*));}vm->trusted_modules[vm->ntrusted++]=xdup(m);}

/* User .hr libraries are never shared across capability sets.  The cache key
   includes the canonical source path and the exact effective capability mask,
   preventing a library loaded with broader privileges from being reused by a
   caller that has fewer permissions. */
typedef struct { char *key; int state; Value module; } HModuleCache;
static HModuleCache *g_modules=NULL; static size_t g_module_n=0,g_module_cap=0;
static void gc_mark_global_roots(void){
    for(ActiveVM*n=g_active_vms;n;n=n->next)gc_mark_vm_roots(n->vm);
    if(g_modules){gc_mark_ptr(g_modules);for(size_t i=0;i<g_module_n;i++){if(g_modules[i].key)gc_mark_ptr(g_modules[i].key);gc_mark_value(g_modules[i].module);}}
    if(g_tasks){gc_mark_ptr(g_tasks);for(size_t i=0;i<g_task_n;i++)if(g_tasks[i])gc_mark_handle(g_tasks[i]);}
    for(MethodReg*m=g_methods;m;m=m->next){gc_mark_ptr(m);if(m->type)gc_mark_ptr(m->type);if(m->name)gc_mark_ptr(m->name);if(m->fn)gc_mark_fn(m->fn);}
    if(g_struct_names){gc_mark_ptr(g_struct_names);for(size_t i=0;i<g_struct_count;i++)if(g_struct_names[i])gc_mark_ptr(g_struct_names[i]);}
}
static int module_cache_idx(const char*key){if(!key)return -1;for(size_t i=0;i<g_module_n;i++)if(g_modules[i].key&&!strcmp(g_modules[i].key,key))return (int)i;return -1;}
static Value make_module_value(const char*name, VM*child, Fn*root){
    Value mod=vsobj(); stput(mod.u.st,"__module",vs(name)); Value ex=vsobj();
    Env*menv=xmalloc(sizeof(Env));memset(menv,0,sizeof*menv); for(int i=0;i<child->g.n;i++)en(menv,child->g.v[i].k,child->g.v[i].v);
    if(root&&root->nexports){for(int i=0;i<root->nexports;i++){Value v=stget((StructObj*)ex.u.st,root->exports[i]);(void)v;int gi=egi(&child->g,root->exports[i]);if(gi>=0){Value mv=child->g.v[gi].v;if(mv.t==VFN&&mv.u.fn&&mv.u.fn->closure==NULL)mv.u.fn->closure=menv;stput(ex.u.st,root->exports[i],mv);}}}
    stput(mod.u.st,"exports",ex);
    if(root&&root->nexports){
        for(int i=0;i<root->nexports;i++){
            int gi=egi(&child->g,root->exports[i]);
            if(gi>=0) stput(mod.u.st,root->exports[i],child->g.v[gi].v);
        }
    }
    return mod;
}
static int module_join_path(char*out,size_t cap,const char*base,const char*rel){
    if(!out||!cap||!base||!rel||!*rel)return 0;
    size_t nb=strlen(base),nr=strlen(rel); if(nb+nr+2>cap)return 0;
    const char*rr=rel; while(*rr=='/'||*rr=='\\')rr++;
    int sep=(nb&&base[nb-1]!='/'&&base[nb-1]!='\\');
    snprintf(out,cap,"%s%s%s",base,sep?"/":"",rr); return 1;
}
static int module_path_safe_for_vm(VM*vm,const char*path){
    if(!path||!*path||!safe_path_arg(path)||strstr(path,".."))return 0;
    if(vm&&vm->module_sandboxed && vm->sandbox_root&&*vm->sandbox_root)return fs_path_allowed(vm,path);
    return 1;
}
static int module_read_candidate(VM*vm,const char*path,char*out,size_t outcap,char**src){
    if(!out||!outcap||!src||!path||!*path)return 0;
    *src=NULL; out[0]=0;
    if(!module_path_safe_for_vm(vm,path))return 0;
    char candidate[PATH_MAX]; snprintf(candidate,sizeof candidate,"%s",path);
    char realp[PATH_MAX]={0};
    if(path_real_abs(candidate,realp,sizeof realp) && !module_path_safe_for_vm(vm,realp))return 0;
    char*text=readf_limit(candidate,H_SOURCE_MAX);
    if(!text)return 0;
    const char*stored=realp[0]?realp:candidate;
    if(strlen(stored)+1>outcap){xfree(text);return 0;}
    snprintf(out,outcap,"%s",stored); *src=text; return 1;
}
static int module_resolve(VM*vm,const char*m,char*out,size_t outcap,char**src){
    if(!vm||!m||!*m||!out||!outcap||!src)return 0;
    *src=NULL; out[0]=0;
    if(!safe_path_arg(m)||strstr(m,".."))return 0;
    if(haris_pack_lookup_module(vm,m,out,outcap,src))return 1;
    const char*base=(vm->module_sandboxed&&vm->sandbox_root&&*vm->sandbox_root)?vm->sandbox_root:NULL;
    char path[PATH_MAX];
    const char*variants[8]; char v0[PATH_MAX],v1[PATH_MAX],v2[PATH_MAX],v3[PATH_MAX],v4[PATH_MAX]; int vn=0;
    if(strstr(m,".hr")){variants[vn++]=m;}
    else{snprintf(v0,sizeof v0,"%s.hr",m);variants[vn++]=v0;}
    if(!base){
        snprintf(v1,sizeof v1,"modules/%s.hr",m);variants[vn++]=v1;
        snprintf(v2,sizeof v2,"modules/%s/__init__.hr",m);variants[vn++]=v2;
        snprintf(v3,sizeof v3,"packages/%s/__init__.hr",m);variants[vn++]=v3;
    } else {
        snprintf(v1,sizeof v1,"%s.hr",m); /* same module-directory root */
        snprintf(v2,sizeof v2,"modules/%s.hr",m);
        snprintf(v3,sizeof v3,"modules/%s/__init__.hr",m);
        snprintf(v4,sizeof v4,"packages/%s/__init__.hr",m);
        variants[vn++]=v2; variants[vn++]=v3; variants[vn++]=v4;
    }
    for(int i=0;i<vn;i++){
        const char*q=variants[i];
        if(base && (i==0 || q==v1 || q==v2 || q==v3 || q==v4)){
            if(module_join_path(path,sizeof path,base,q) && module_read_candidate(vm,path,out,outcap,src))return 1;
        } else if(module_read_candidate(vm,q,out,outcap,src)) return 1;
    }
    /* Project-local modules/packages are useful for a top-level import. */
    if(base){
        snprintf(path,sizeof path,"%s/%s",base,m); if(strstr(m,".hr")==NULL){char tmp[PATH_MAX];snprintf(tmp,sizeof tmp,"%s.hr",path);if(module_read_candidate(vm,tmp,out,outcap,src))return 1;} else if(module_read_candidate(vm,path,out,outcap,src))return 1;
    }
    return 0;
}
static unsigned module_effective_caps(VM*vm,const char*m,unsigned requested,int trusted){
    unsigned granted=(vm?vm->host_module_caps:0u);
    unsigned dangerous=CAP_SYSTEM|CAP_NUCLEAR|CAP_HW_ANY|CAP_REDTEAM|CAP_PENTEST|CAP_GAME_ADMIN;
    if(!trusted)granted &= ~dangerous;
    if((requested & dangerous) && !trusted) return requested & granted & ~dangerous;
    return requested & granted;
}
static Value nimport(VM*vm,int n,Value*a){
    if(n!=1||a[0].t!=VSTR)return vn(); const char*m=a[0].u.s;
    const char*builtin[]={"os","system","ai","sql","games","game","net","web","cloud","git","defense","logs","data","nuclear","engine","async","gpu","cpu","fs","file","dir","path","map","set","queue","stack","tuple","range","random","string","math","memory","regex","datetime","gfx","json","security","api"};
    for(size_t i=0;i<sizeof builtin/sizeof*builtin;i++)if(!strcmp(m,builtin[i]))return vb(1);
    char path[PATH_MAX]={0};char*src=NULL;
    if(!module_resolve(vm,m,path,sizeof path,&src))return vb(0);
    unsigned requested= requested_caps(src);
    int trusted=module_trusted(vm,m)||module_trusted(vm,path);
    unsigned effective=module_effective_caps(vm,m,requested,trusted);
    char why[192]={0};
    if(!sandbox_authorize_caps(vm,effective,why,sizeof why)){fprintf(stderr,"Haris security: module '%s' denied: %s\n",m,why);xfree(src);return vn();}
    if((requested & ~effective)!=0){
        unsigned missing=requested & ~effective; unsigned one=missing & (~missing+1u);
        snprintf(why,sizeof why,"capability '%s' is not granted to untrusted module",cap_name(one));
        fprintf(stderr,"Haris security: module '%s' denied: %s\n",m,why);xfree(src);return vn();
    }
    if(!module_scan(src,effective,why,sizeof why)){fprintf(stderr,"Haris security: module '%s' denied: %s\n",m,why);xfree(src);return vn();}
    char key[PATH_MAX+64];snprintf(key,sizeof key,"%s|caps=%u",path,effective);
    int ci=module_cache_idx(key);
    if(ci>=0){if(g_modules[ci].state==1)die("Haris module: circular dependency involving '%s'",m);return g_modules[ci].module;}
    Fn*root=compile(src,path);xfree(src);
    if(g_module_n==g_module_cap){g_module_cap=g_module_cap?g_module_cap*2:16;g_modules=xrealloc(g_modules,g_module_cap*sizeof(HModuleCache));}
    g_modules[g_module_n]=(HModuleCache){xdup(key),1,vn()};ci=(int)g_module_n++;

    VM child;init(&child);
    child.module_sandboxed=1; child.sandbox_auto=1; child.sandbox_worker=0;
    child.module_caps=effective; child.host_module_caps=effective;
    child.cpu_limit_ms=vm->cpu_limit_ms; child.mem_limit_bytes=vm->mem_limit_bytes;
    child.cpu_cores=vm->cpu_cores; child.cpu_percent=vm->cpu_percent;
    child.cpu_slice_wall_ms=vm->cpu_slice_wall_ms; child.cpu_slice_cpu_ms=vm->cpu_slice_cpu_ms;
    child.memory_auto=vm->memory_auto;
    child.redteam_authorized=trusted && vm->redteam_authorized;
    child.pentest_authorized=trusted && vm->pentest_authorized;
    if(vm->sandbox_root&&*vm->sandbox_root)child.sandbox_root=vm->sandbox_root;
    else {
        static HARIS_TLS char rootbuf[PATH_MAX];
        char*slash=strrchr(path,'/');
#ifdef _WIN32
        char*bs=strrchr(path,'\\'); if(bs&&(!slash||bs>slash))slash=bs;
#endif
        if(slash){size_t z=(size_t)(slash-path);if(z==0)z=1;if(z>=sizeof rootbuf)z=sizeof rootbuf-1;memcpy(rootbuf,path,z);rootbuf[z]=0;child.sandbox_root=rootbuf;}
    }
    /* Library imports stay in-process so exported Haris functions remain real
       callable values. Isolation therefore uses a separate VM environment,
       explicit capabilities, root-confined filesystem access and inherited
       resource budgets. Native OS sandboxing is reserved for process workers
       (calling seccomp/Landlock here would affect the host process too). */
    snprintf(child.sandbox_backend,sizeof child.sandbox_backend,"library:capability-vm");
    Value rr=run(&child,root,0,0);
    if(child.int_overflow){fprintf(stderr,"Haris security: module '%s' hit integer overflow\n",m);}
    (void)rr;
    Value mod=make_module_value(m,&child,root);g_modules[ci].module=mod;g_modules[ci].state=2;return mod;
}
typedef struct {FILE *f; size_t bytes;} CurlOut;
static size_t cw(void*p,size_t z,size_t n,void*u){size_t k=z*n;CurlOut*o=(CurlOut*)u;if(k>H_DATA_MAX-o->bytes)return 0;size_t w=fwrite(p,1,k,o->f);o->bytes+=w;return w;}
static Value web_req(VM*vm,int n,Value*a,int post){(void)vm;if(n<1||a[0].t!=VSTR)return vn();CURL*c=curl_easy_init();if(!c)return vn();FILE*tmp=tmpfile();if(!tmp){curl_easy_cleanup(c);return vn();}CurlOut outbuf={tmp,0};curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_URL,a[0].u.s);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(c,CURLOPT_MAXREDIRS,5L);curl_easy_setopt(c,CURLOPT_TIMEOUT,30L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,cw);curl_easy_setopt(c,CURLOPT_WRITEDATA,&outbuf);CURLcode rc=CURLE_OK;struct curl_slist*h=0;if(post){if(n<2||a[1].t!=VSTR){curl_easy_cleanup(c);fclose(tmp);return vn();}curl_easy_setopt(c,CURLOPT_POST,1L);curl_easy_setopt(c,CURLOPT_POSTFIELDS,a[1].u.s);h=curl_slist_append(0,"Content-Type: application/json");curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);rc=curl_easy_perform(c);}else rc=curl_easy_perform(c);long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);if(h)curl_slist_free_all(h);curl_easy_cleanup(c);fflush(tmp);if(rc!=CURLE_OK){fclose(tmp);Value r=va();ap(r.u.a,vi(0));ap(r.u.a,vs(curl_easy_strerror(rc)));return r;}if(fseek(tmp,0,SEEK_END)!=0){fclose(tmp);return vn();}long sz=ftell(tmp);if(sz<0||sz>(long)H_DATA_MAX){fclose(tmp);return vn();}if(fseek(tmp,0,SEEK_SET)!=0){fclose(tmp);return vn();}char*b=xmalloc((size_t)sz+1);size_t got=fread(b,1,(size_t)sz,tmp);b[got]=0;fclose(tmp);Value r=va();ap(r.u.a,vi(code));ap(r.u.a,vs(b));xfree(b);return r;}
static Value nwebget(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.get");return web_req(vm,n,a,0);} static Value nwebpost(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_WEB))return cap_error(vm,CAP_WEB,"web.post_json");return web_req(vm,n,a,1);}
static Value nosenv(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_OS))return cap_error(vm,CAP_OS,"os.getenv");if(n!=1||a[0].t!=VSTR)return vn();char*p=getenv(a[0].u.s);return p?vs(p):vn();} static Value nosexists(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_FS))return cap_error(vm,CAP_FS,"os.exists");if(n!=1||a[0].t!=VSTR)return vb(0);return vb(access(a[0].u.s,F_OK)==0);} static Value nostime(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_OS))return cap_error(vm,CAP_OS,"os.time_ms");(void)a;if(n)return vn();return vi((long long)time(0)*1000);}
static int system_command_allowed(const char *cmd){
#ifdef _WIN32
    const char*ok[]={"echo.exe","printf.exe","true.exe","false.exe","date.exe","whoami.exe",NULL};for(size_t i=0;ok[i];i++)if(!_stricmp(cmd,ok[i]))return 1;
#else
    const char*ok[]={"echo","printf","true","false","date","uname","whoami",NULL};for(size_t i=0;ok[i];i++)if(!strcmp(cmd,ok[i]))return 1;
#endif
    return 0;
}
static Value nsystem(VM*vm,int n,Value*a){
#ifdef HARIS_ANDROID
    (void)vm;(void)n;(void)a;
    return vn();
#else
    if(!cap_allowed(vm,CAP_SYSTEM))return cap_error(vm,CAP_SYSTEM,"system.run");if(n!=1||a[0].t!=VSTR||!safe_path_arg(a[0].u.s))return vb(0);
    char*buf=xdup(a[0].u.s);char*argv[65]={0};int argc=0;char*p=buf;
    while(*p){while(*p&&isspace((unsigned char)*p))p++;if(!*p)break;if(argc>=64){xfree(buf);return vb(0);}argv[argc++]=p;while(*p&&!isspace((unsigned char)*p))p++;if(*p)*p++='\0';}
    if(argc<1||!system_command_allowed(argv[0])){xfree(buf);return vb(0);}
#ifdef _WIN32
    int rc=_spawnvp(_P_WAIT,argv[0],(const char* const*)argv);xfree(buf);return vi((long long)rc);
#else
    pid_t pid=fork();if(pid<0){xfree(buf);return vi(-1);}if(pid==0){static const char*dirs[]={"/usr/bin","/bin",NULL};for(int i=0;dirs[i];i++){char exe[PATH_MAX];int w=snprintf(exe,sizeof exe,"%s/%s",dirs[i],argv[0]);if(w>0&&(size_t)w<sizeof exe)execv(exe,argv);} _exit(127);}int st=0;while(waitpid(pid,&st,0)<0&&errno==EINTR){}xfree(buf);if(WIFEXITED(st))return vi((long long)WEXITSTATUS(st));if(WIFSIGNALED(st))return vi(128LL+(long long)WTERMSIG(st));return vi(-1);
#endif
#endif
}
static Value ndns(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.dns");if(n!=1||a[0].t!=VSTR)return vn();struct addrinfo h={0},*r=0;h.ai_socktype=SOCK_STREAM;int rc=getaddrinfo(a[0].u.s,0,&h,&r);Value out=va();if(rc)return out;char ip[INET6_ADDRSTRLEN];int count=0;for(struct addrinfo*p=r;p&&count<16;p=p->ai_next){void*addr=p->ai_family==AF_INET?(void*)&((struct sockaddr_in*)p->ai_addr)->sin_addr:(void*)&((struct sockaddr_in6*)p->ai_addr)->sin6_addr;if(inet_ntop(p->ai_family,addr,ip,sizeof ip)){ap(out.u.a,vs(ip));count++;}}freeaddrinfo(r);return out;}
static Value nip(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.ip");(void)a;if(n!=0)return vn();char host[256];if(gethostname(host,sizeof host)!=0)return vn();host[sizeof host-1]=0;Value ds[1]={vs(host)};Value x=ndns(vm,1,ds);if(x.t==VARR&&x.u.a->n)return x.u.a->v[0];return vn();}
static Value nmac(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.mac");(void)a;if(n!=0)return vn();
#ifdef _WIN32
IP_ADAPTER_INFO info[16];ULONG len=sizeof(info);if(GetAdaptersInfo(info,&len)!=NO_ERROR)return vn();Value o=va();for(PIP_ADAPTER_INFO x=info;x;x=x->Next){if(x->AddressLength>=6){char b[32];snprintf(b,sizeof b,"%02X:%02X:%02X:%02X:%02X:%02X",x->Address[0],x->Address[1],x->Address[2],x->Address[3],x->Address[4],x->Address[5]);ap(o.u.a,vs(b));break;}}return o;
#else
DIR*d=opendir("/sys/class/net");if(!d)return vn();struct dirent*e;while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")||!strcmp(e->d_name,"lo"))continue;char path[512];snprintf(path,sizeof path,"/sys/class/net/%s/address",e->d_name);FILE*f=fopen(path,"r");if(!f)continue;char b[64];if(fgets(b,sizeof b,f)){fclose(f);closedir(d);b[strcspn(b,"\\r\\n")]=0;return vs(b);}fclose(f);}closedir(d);return vn();
#endif
}
static Value nfw(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_NET))return cap_error(vm,CAP_NET,"net.firewall_status");(void)a;if(n!=0)return vn();Value o=vsobj();stput(o.u.st,"supported",vb(0));stput(o.u.st,"safe",vb(1));stput(o.u.st,"message",vs("Native firewall inspection is disabled here because it would require host subprocesses; use an OS broker integration."));return o;}
static Value nsqlopen(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_SQL))return cap_error(vm,CAP_SQL,"sql.open");if(n!=1||a[0].t!=VSTR)return vn();sqlite3*db=0;if(sqlite3_open(a[0].u.s,&db)!=SQLITE_OK){if(db)sqlite3_close(db);return vn();}return (Value){.t=VHANDLE,.u.handle=db};}
static Value nsqlexec(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_SQL))return cap_error(vm,CAP_SQL,"sql.exec");if(n!=2||a[0].t!=VHANDLE||!a[0].u.handle||a[1].t!=VSTR)return vb(0);char*e=0;int rc=sqlite3_exec((sqlite3*)a[0].u.handle,a[1].u.s,0,0,&e);if(e)sqlite3_free(e);return vb(rc==SQLITE_OK);} 
static int qcb(void*u,int n,char**v,char**c){(void)c;Value*out=u;Value r=va();for(int i=0;i<n;i++)ap(r.u.a,v&&v[i]?vs(v[i]):vn());ap(out->u.a,r);return 0;} static Value nsqlquery(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_SQL))return cap_error(vm,CAP_SQL,"sql.query");if(n!=2||a[0].t!=VHANDLE||a[1].t!=VSTR)return vn();Value o=va();char*e=0;int rc=sqlite3_exec((sqlite3*)a[0].u.handle,a[1].u.s,qcb,&o,&e);if(e)sqlite3_free(e);return rc==SQLITE_OK?o:vn();} static Value nsqlclose(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_SQL))return cap_error(vm,CAP_SQL,"sql.close");if(n!=1||a[0].t!=VHANDLE||!a[0].u.handle)return vb(0);int rc=sqlite3_close((sqlite3*)a[0].u.handle);if(rc==SQLITE_OK)a[0].u.handle=0;return vb(rc==SQLITE_OK);}
static Value nsqlbind(VM*vm,int n,Value*a){if(!cap_allowed(vm,CAP_SQL))return cap_error(vm,CAP_SQL,"sql.prepare_bind");if(n<2||a[0].t!=VHANDLE||!a[0].u.handle||a[1].t!=VSTR)return vb(0);sqlite3_stmt*st=0;if(sqlite3_prepare_v2((sqlite3*)a[0].u.handle,a[1].u.s,-1,&st,0)!=SQLITE_OK)return vb(0);for(int i=2;i<n;i++){int rc=SQLITE_OK;if(a[i].t==VINT)rc=sqlite3_bind_int64(st,i-1,a[i].u.i);else if(a[i].t==VFLOAT)rc=sqlite3_bind_double(st,i-1,a[i].u.f);else if(a[i].t==VSTR)rc=sqlite3_bind_text(st,i-1,a[i].u.s,-1,SQLITE_TRANSIENT);else if(a[i].t==VNULL)rc=sqlite3_bind_null(st,i-1);else rc=SQLITE_MISUSE;if(rc!=SQLITE_OK){sqlite3_finalize(st);return vb(0);}}int rc=sqlite3_step(st);sqlite3_finalize(st);return vb(rc==SQLITE_DONE||rc==SQLITE_ROW);}

static Value nai_dot(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR||a[1].t!=VARR||a[0].u.a->n!=a[1].u.a->n)return vn();double x=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i])||!isnum(a[1].u.a->v[i]))return vn();x+=dn(a[0].u.a->v[i])*dn(a[1].u.a->v[i]);}return vf(x);}
static Value nai_cosine(VM*vm,int n,Value*a){Value d=nai_dot(vm,n,a);if(d.t!=VFLOAT)return d;double aa=0,bb=0;for(size_t i=0;i<a[0].u.a->n;i++){double x=dn(a[0].u.a->v[i]),y=dn(a[1].u.a->v[i]);aa+=x*x;bb+=y*y;}return vf((aa&&bb)?d.u.f/(sqrt(aa)*sqrt(bb)):0);}
static Value nai_sigmoid(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0]))return vn();double x=dn(a[0]);return vf(1.0/(1.0+exp(-x)));}
static Value nai_argmax(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR||!a[0].u.a->n)return vn();size_t k=0;for(size_t i=1;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();if(dn(a[0].u.a->v[i])>dn(a[0].u.a->v[k]))k=i;}return vi((long long)k);}
static Value nai_softmax(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR)return vn();Value o=va();double mx=-INFINITY,sum=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();if(dn(a[0].u.a->v[i])>mx)mx=dn(a[0].u.a->v[i]);}for(size_t i=0;i<a[0].u.a->n;i++){double e=exp(dn(a[0].u.a->v[i])-mx);ap(o.u.a,vf(e));sum+=e;}for(size_t i=0;i<o.u.a->n;i++)o.u.a->v[i].u.f/=sum;return o;}
static Value nai_linear(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VARR||a[1].t!=VARR||a[0].u.a->n!=a[1].u.a->n||!isnum(a[2]))return vn();double y=dn(a[2]);for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i])||!isnum(a[1].u.a->v[i]))return vn();y+=dn(a[0].u.a->v[i])*dn(a[1].u.a->v[i]);}return vf(y);}

static Value ai_vec_binary(VM*vm,int n,Value*a,int mode){(void)vm;if(n!=2||a[0].t!=VARR||a[1].t!=VARR||a[0].u.a->n!=a[1].u.a->n)return vn();Value o=va();for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i])||!isnum(a[1].u.a->v[i]))return vn();double x=dn(a[0].u.a->v[i]),y=dn(a[1].u.a->v[i]);ap(o.u.a,vf(mode==0?x+y:mode==1?x-y:x*y));}return o;}
static Value nai_vec_add(VM*vm,int n,Value*a){return ai_vec_binary(vm,n,a,0);} 
static Value nai_vec_sub(VM*vm,int n,Value*a){return ai_vec_binary(vm,n,a,1);} 
static Value nai_vec_mul(VM*vm,int n,Value*a){return ai_vec_binary(vm,n,a,2);} 
static Value nai_relu(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vn();if(a[0].t==VINT)return vi(a[0].u.i>0?a[0].u.i:0);if(a[0].t==VFLOAT)return vf(a[0].u.f>0?a[0].u.f:0);if(a[0].t!=VARR)return vn();Value o=va();for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();double x=dn(a[0].u.a->v[i]);ap(o.u.a,vf(x>0?x:0));}return o;}
static Value nai_tanh(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0]))return vn();return vf(tanh(dn(a[0])));}
static Value nai_mse(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR||a[1].t!=VARR||a[0].u.a->n!=a[1].u.a->n||!a[0].u.a->n)return vn();double s=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i])||!isnum(a[1].u.a->v[i]))return vn();double d=dn(a[0].u.a->v[i])-dn(a[1].u.a->v[i]);s+=d*d;}return vf(s/(double)a[0].u.a->n);}
static Value nai_normalize(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VARR)return vn();Value o=va();double norm=0;for(size_t i=0;i<a[0].u.a->n;i++){if(!isnum(a[0].u.a->v[i]))return vn();double x=dn(a[0].u.a->v[i]);norm+=x*x;}norm=sqrt(norm);for(size_t i=0;i<a[0].u.a->n;i++)ap(o.u.a,vf(norm?dn(a[0].u.a->v[i])/norm:0));return o;}
static Value nai_matmul(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VARR||a[1].t!=VARR||!a[0].u.a->n||!a[1].u.a->n)return vn();size_t r=a[0].u.a->n,k=a[0].u.a->v[0].t==VARR?a[0].u.a->v[0].u.a->n:0,c=a[1].u.a->v[0].t==VARR?a[1].u.a->v[0].u.a->n:0;if(!k||!c||r>512||c>512||k>512||a[1].u.a->n!=k)return vn();for(size_t i=0;i<r;i++)if(a[0].u.a->v[i].t!=VARR||a[0].u.a->v[i].u.a->n!=k)return vn();for(size_t j=0;j<k;j++)if(a[1].u.a->v[j].t!=VARR||a[1].u.a->v[j].u.a->n!=c)return vn();Value out=va();for(size_t i=0;i<r;i++){Value row=va();for(size_t j=0;j<c;j++){double sum=0;for(size_t q=0;q<k;q++){if(!isnum(a[0].u.a->v[i].u.a->v[q])||!isnum(a[1].u.a->v[q].u.a->v[j]))return vn();sum+=dn(a[0].u.a->v[i].u.a->v[q])*dn(a[1].u.a->v[q].u.a->v[j]);}ap(row.u.a,vf(sum));}ap(out.u.a,row);}return out;}
static Value nai_train_linear(VM*vm,int n,Value*a){(void)vm;
 if(n!=5||a[0].t!=VARR||a[1].t!=VARR||a[2].t!=VARR||!isnum(a[3])||!isnum(a[4]))return vn();
 size_t samples=a[0].u.a->n;if(!samples||a[1].u.a->n!=samples||a[0].u.a->v[0].t!=VARR)return vn();
 size_t d=a[0].u.a->v[0].u.a->n;if(!d||d>256||a[2].u.a->n!=d)return vn();
 for(size_t i=0;i<samples;i++)if(a[0].u.a->v[i].t!=VARR||a[0].u.a->v[i].u.a->n!=d||!isnum(a[1].u.a->v[i]))return vn();
 double lr=dn(a[3]);int epochs=(int)dn(a[4]);if(epochs<1||epochs>10000||!isfinite(lr))return vn();
 double*w=(double*)malloc(d*sizeof(double));if(!w)return vn();
 for(size_t j=0;j<d;j++){if(!isnum(a[2].u.a->v[j])){free(w);return vn();}w[j]=dn(a[2].u.a->v[j]);}
 double b=0;
 for(int ep=0;ep<epochs;ep++){
  double*gw=(double*)calloc(d,sizeof(double));if(!gw){free(w);return vn();}double gb=0;
  for(size_t i=0;i<samples;i++){double pred=b;for(size_t j=0;j<d;j++)pred+=dn(a[0].u.a->v[i].u.a->v[j])*w[j];double e=pred-dn(a[1].u.a->v[i]);for(size_t j=0;j<d;j++)gw[j]+=2*e*dn(a[0].u.a->v[i].u.a->v[j]);gb+=2*e;}
  double inv=1.0/(double)samples;for(size_t j=0;j<d;j++)w[j]-=lr*gw[j]*inv;b-=lr*gb*inv;free(gw);
 }
 Value out=va();for(size_t j=0;j<d;j++)ap(out.u.a,vf(w[j]));ap(out.u.a,vf(b));free(w);return out;
}

