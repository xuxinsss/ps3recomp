from pathlib import Path
import os,sys,subprocess,tempfile
root=Path(sys.argv[1]).resolve();expected=sys.argv[2]
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()==expected
subprocess.run([sys.executable,str(root/'tools/test_ppu_prio_start.py'),'--negative-controls'],cwd=root,check=True)
subprocess.run([sys.executable,str(root/'tools/test_ppu_prio_start.py'),'--host-delay-ms','250'],cwd=root,check=True)
if sys.platform=='darwin':
 with tempfile.TemporaryDirectory(prefix='prio-background-proof-') as d:
  d=Path(d);src=d/'background.c';binary=d/'background'
  src.write_text('#include <pthread.h>\n#include <pthread/qos.h>\n#define main fixture_main\n#include "'+(root/'tests/test_ppu_prio_start.c').as_posix()+'"\n#undef main\nint main(void){if(pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND,0))return 99;return fixture_main();}\n')
  subprocess.run(['clang','-std=gnu17','-D_GNU_SOURCE','-O1','-UNDEBUG','-ffunction-sections','-fdata-sections','-I'+str(root/'include'),'-I'+str(root/'runtime'),'-I'+str(root/'runtime/syscalls'),str(src),'-Wl,-dead_strip','-o',str(binary)],check=True)
  for repeat in range(2):
   for on in [False,True]:
    env={k:v for k,v in os.environ.items() if k!='PS3_PRIO_START'};env['PS3_PRIO_TEST_HOST_DELAY_MS']='250'
    if on:env['PS3_PRIO_START']='1'
    p=subprocess.run([str(binary)],env=env,capture_output=True,text=True,timeout=60);print(p.stdout,flush=True)
    if p.returncode:print(p.stderr,flush=True)
    p.check_returncode()
  print('PASS: macOS background QoS and 250ms host delay, 4 real-thread executions',flush=True)
print('PASS: candidate negative controls and delayed-host contracts '+expected,flush=True)
