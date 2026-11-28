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
# Check the shared #882 correction against its deliberately reversed ordering.
with tempfile.TemporaryDirectory(prefix='pm-counter-proof-') as directory:
 temp=Path(directory);bench=(root/'tests/bench_spurs_pm_hop.c').read_text()
 start=bench.index('static void pm_run(');end=bench.index('static void pm_a(',start);original=bench[start:end]
 store='        __atomic_store_n(&g_hops, h + 1, __ATOMIC_RELEASE);\n'
 assert original.count(store)==1
 reversed_order=original.replace(store,'').replace('    }\n    c->pc = SPURS_PM_EXIT_TO_KERNEL_LS;',store+'    }\n    c->pc = SPURS_PM_EXIT_TO_KERNEL_LS;')
 assert reversed_order.count(store)==1
 for name,source,wanted in [('fixed',original,0),('wake-before-count',reversed_order,1)]:
  (temp/'spurs_hop_under_test.inc').write_text(source)
  binary=temp/name
  subprocess.run(['cc','-std=gnu17','-O1','-UNDEBUG','-I'+str(temp),str(root/'tests/test_spurs_hop_counter.c'),'-o',str(binary)],check=True)
  result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=10)
  print(name,result.stdout,result.stderr,flush=True)
  assert result.returncode==wanted,(name,result.returncode)
  if wanted:assert 'ran=2, counted=1' in result.stderr
  else:assert 'PASS: overlapping benchmark hops retain both completions' in result.stdout
 print('PASS: shared PM hop correction and reversed-order negative control',flush=True)
print('PASS: candidate negative controls and delayed-host contracts '+expected,flush=True)
