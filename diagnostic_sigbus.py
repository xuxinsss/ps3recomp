from pathlib import Path
import os,sys,subprocess,json,hashlib,time
R=Path(__file__).resolve().parent;mode=sys.argv[1];O=R/'sigbus-evidence'/mode;O.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items() if k not in ['PS3_PRIO_START','PS3_PRIO_TEST_HOST_DELAY_MS']}
flags=['clang','-std=gnu17','-D_GNU_SOURCE','-O1','-UNDEBUG','-ffunction-sections','-fdata-sections','-Iinclude','-Iruntime','-Iruntime/syscalls']
binary=O/'thread-integration'
cmd=flags+['tests/test_ppu_prio_start.c','-Wl,-dead_strip','-o',str(binary)]
p=subprocess.run(cmd,cwd=R,capture_output=True,text=True);(O/'compile.log').write_text(p.stdout+p.stderr);p.check_returncode()
manifest={f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in ['tests/test_ppu_prio_start.c','tools/test_ppu_prio_start.py','runtime/syscalls/sys_ppu_thread.c','runtime/syscalls/sys_ppu_thread.h']}
(O/'source-hashes.json').write_text(json.dumps(manifest,indent=2));results=[]
if mode=='debugger':
 commands=['settings set target.disable-aslr false','run','thread backtrace all','register read','disassemble --pc --count 20','image list','memory region $pc','memory region $x0','quit']
 commandfile=O/'commands.lldb';commandfile.write_text('\n'.join(commands)+'\n')
 command=['lldb','--batch','--source',str(commandfile),'--',str(binary)];count=24
else:command=[str(binary)];count=100
for i in range(count):
 start=time.monotonic()
 try:p=subprocess.run(command,cwd=R,env=env,capture_output=True,text=True,timeout=60);out=p.stdout+p.stderr;rc=p.returncode
 except subprocess.TimeoutExpired as e:out=str(e);rc=999
 (O/f'case-{i:03d}.log').write_text(out)
 fault=('EXC_BAD_ACCESS' in out or 'SIGBUS' in out or 'SIGSEGV' in out or rc<0)
 results.append(dict(case=i,exit=rc,fault=fault,seconds=time.monotonic()-start))
 print(mode,i,'exit',rc,'fault',fault,flush=True)
(O/'results.json').write_text(json.dumps(results,indent=2))
print('COLLECTION ONLY:',sum(x['fault'] for x in results),'faults of',count,'not a passing correctness gate')
