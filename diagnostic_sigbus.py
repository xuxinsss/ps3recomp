from pathlib import Path
import os,sys,subprocess,json,hashlib,time,resource
R=Path(__file__).resolve().parent;mode=sys.argv[1];O=R/'sigbus-evidence'/mode;O.mkdir(parents=True,exist_ok=True)
if mode in ['paired','paired-signal']:
 source=(R/'tests/test_ppu_prio_start.c').read_text();summary=[]
 prefix='''#include <sys/mman.h>
#include <stddef.h>
static int observe_protect(void*,size_t,int);
#define mprotect observe_protect
'''
 suffix=r'''
#undef mprotect
#include <errno.h>
static int observe_protect(void* a,size_t n,int prot) {
 int rc=mprotect(a,n,prot),err=errno;char b[512];
 int len=snprintf(b,sizeof b,"PROTECT vm=%p addr=%p bytes=%zu prot=%d rc=%d errno=%d\n",vm_base,a,n,prot,rc,err);write(2,b,len);errno=err;return rc;
}
'''
 if mode=='paired-signal':
  suffix+=r'''
#include <signal.h>
#include <sys/ucontext.h>
#include <execinfo.h>
static void fault(int sig,siginfo_t* si,void* v) {
 ucontext_t* u=v;uintptr_t pc=u->uc_mcontext->__ss.__pc;char b[1024];
 int n=snprintf(b,sizeof b,"SIGNAL sig=%d code=%d addr=%p pc=%llx x8=%llx x9=%llx INSN=%08x,%08x,%08x,%08x\n",sig,si->si_code,si->si_addr,(unsigned long long)pc,u->uc_mcontext->__ss.__x[8],u->uc_mcontext->__ss.__x[9],((unsigned*)pc)[0],((unsigned*)pc)[1],((unsigned*)pc)[2],((unsigned*)pc)[3]);write(2,b,n);
 void* bt[32];n=backtrace(bt,32);backtrace_symbols_fd(bt,n,2);_exit(128+sig);
}
__attribute__((constructor)) static void install(void) {struct sigaction sa={0};sa.sa_sigaction=fault;sa.sa_flags=SA_SIGINFO;sigaction(SIGBUS,&sa,0);sigaction(SIGSEGV,&sa,0);}
'''
 fixed=source.replace('vm_base = calloc(1, 0x10000); assert(vm_base);','vm_base = mmap(NULL, 0x200000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0); assert(vm_base != MAP_FAILED);').replace('next_addr = 0x10000000u; g_vm_stack_alloc.region_end = 0xA0000000u;','next_addr = 0x10000u; g_vm_stack_alloc.region_end = 0x200000u;').replace('free(vm_base);','assert(munmap(vm_base, 0x200000) == 0);')
 for label,content in [('original',source),('owned',fixed)]:
  # The copied fixture retains its relative runtime include by using an absolute path.
  content=content.replace('../runtime/',str(R/'runtime')+'/')
  f=O/(label+'.c');f.write_text(prefix+content+suffix)
  b=O/label;cmd=['clang','-std=gnu17','-D_GNU_SOURCE','-O1','-UNDEBUG','-ffunction-sections','-fdata-sections','-Iinclude','-Iruntime','-Iruntime/syscalls',str(f),'-Wl,-dead_strip','-o',str(b)]
  subprocess.run(cmd,cwd=R,check=True)
  for i in range(100):
   env=dict(os.environ);env.pop('PS3_PRIO_START',None)
   if i%2:env['PS3_PRIO_START']='1'
   p=subprocess.run([str(b)],env=env,capture_output=True,text=True,timeout=60);(O/f'{label}-{i:03d}.log').write_text(p.stdout+p.stderr);summary.append(dict(variant=label,case=i,on=bool(i%2),exit=p.returncode));print(label,i,p.returncode,flush=True)
 if mode=='paired-signal':
  import re
  pcs=sorted(set(re.findall(r'pc=([0-9a-f]+)', ''.join(p.read_text() for p in O.glob('original-*.log')))))
  for pc in pcs:
   cmd=['lldb','--batch',str(O/'original'),'-o','settings set target.disable-aslr false','-o','b main','-o','run','-o','image list','-o','disassemble --start-address 0x'+pc+' --count 16']
   q=subprocess.run(cmd,capture_output=True,text=True,timeout=60);(O/('instruction-'+pc+'.log')).write_text(q.stdout+q.stderr)
 (O/'results.json').write_text(json.dumps(summary,indent=2));print('COLLECTION ONLY; original faults preserved');raise SystemExit(0)
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
