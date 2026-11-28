from pathlib import Path
import os,sys,subprocess,json,hashlib,time,resource
R=Path(__file__).resolve().parent;mode=sys.argv[1];O=R/'sigbus-evidence'/mode;O.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items() if k not in ['PS3_PRIO_START','PS3_PRIO_TEST_HOST_DELAY_MS']}
flags=['clang','-std=gnu17','-D_GNU_SOURCE','-O1','-UNDEBUG','-ffunction-sections','-fdata-sections','-Iinclude','-Iruntime','-Iruntime/syscalls']
binary=O/'thread-integration'
extra=[]
if mode=='signal':
 signal_source=O/'signal.c';signal_source.write_text(r'''
#include <signal.h>
#include <unistd.h>
#include <sys/ucontext.h>
#include <execinfo.h>
#include <stdio.h>
static void fault(int sig, siginfo_t* si, void* v) {
 ucontext_t* u=v; char b[4096];
 int n=snprintf(b,sizeof b,"SIGNAL sig=%d code=%d addr=%p pc=%llx lr=%llx sp=%llx fp=%llx x0=%llx x1=%llx x8=%llx x9=%llx\n",sig,si->si_code,si->si_addr,u->uc_mcontext->__ss.__pc,u->uc_mcontext->__ss.__lr,u->uc_mcontext->__ss.__sp,u->uc_mcontext->__ss.__fp,u->uc_mcontext->__ss.__x[0],u->uc_mcontext->__ss.__x[1],u->uc_mcontext->__ss.__x[8],u->uc_mcontext->__ss.__x[9]);
 write(2,b,n); void* bt[32]; n=backtrace(bt,32);backtrace_symbols_fd(bt,n,2);_exit(128+sig);
}
__attribute__((constructor)) static void install(void) {struct sigaction sa={0};sa.sa_sigaction=fault;sa.sa_flags=SA_SIGINFO;sigaction(SIGBUS,&sa,0);sigaction(SIGSEGV,&sa,0);}
''');extra=[str(signal_source)]
cmd=flags+['tests/test_ppu_prio_start.c',*extra,'-Wl,-dead_strip','-o',str(binary)]
p=subprocess.run(cmd,cwd=R,capture_output=True,text=True);(O/'compile.log').write_text(p.stdout+p.stderr);p.check_returncode()
manifest={f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in ['tests/test_ppu_prio_start.c','tools/test_ppu_prio_start.py','runtime/syscalls/sys_ppu_thread.c','runtime/syscalls/sys_ppu_thread.h']}
(O/'source-hashes.json').write_text(json.dumps(manifest,indent=2));results=[]
if mode=='debugger':
 commands=['settings set target.disable-aslr false','run','thread backtrace all','register read','disassemble --pc --count 20','image list','memory region $pc','memory region $x0','quit']
 commandfile=O/'commands.lldb';commandfile.write_text('\n'.join(commands)+'\n')
 command=['lldb','--batch','--source',str(commandfile),'--',str(binary)];count=24
else:command=[str(binary)];count=100
if mode=='core':
 resource.setrlimit(resource.RLIMIT_CORE,(resource.RLIM_INFINITY,resource.RLIM_INFINITY))
 subprocess.run(['sudo','sysctl','-w','kern.coredump=1'],check=True)
 subprocess.run(['sudo','sysctl','-w','kern.corefile='+str(O/'core.%P')],check=True)
for i in range(count):
 start=time.monotonic()
 try:p=subprocess.run(command,cwd=R,env=env,capture_output=True,text=True,timeout=60);out=p.stdout+p.stderr;rc=p.returncode
 except subprocess.TimeoutExpired as e:out=str(e);rc=999
 (O/f'case-{i:03d}.log').write_text(out)
 fault=('EXC_BAD_ACCESS' in out or 'SIGBUS' in out or 'SIGSEGV' in out or 'SIGNAL sig=' in out or rc<0)
 if mode=='core' and fault:
  for core in O.glob('core.*'):
   cmds=['thread backtrace all','register read','disassemble --pc --count 20','image list','memory region $pc','memory region $x0']
   lldb=['lldb','--batch','-c',str(core),str(binary)]
   for c in cmds:lldb+=['-o',c]
   q=subprocess.run(lldb,capture_output=True,text=True,timeout=60)
   (O/f'core-{i:03d}.lldb.log').write_text(q.stdout+q.stderr)
   (O/f'core-{i:03d}.metadata.json').write_text(json.dumps(dict(size=core.stat().st_size)))
   core.unlink() # only this experiment's exact core files, after extracting debugger evidence
 results.append(dict(case=i,exit=rc,fault=fault,seconds=time.monotonic()-start))
 print(mode,i,'exit',rc,'fault',fault,flush=True)
(O/'results.json').write_text(json.dumps(results,indent=2))
print('COLLECTION ONLY:',sum(x['fault'] for x in results),'faults of',count,'not a passing correctness gate')
