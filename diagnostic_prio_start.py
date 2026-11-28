from pathlib import Path
import os,subprocess,json,sys,tempfile
root=Path(__file__).resolve().parent
source=(root/'tests/test_ppu_prio_start.c').read_text()
source=source.replace('#include "../runtime/syscalls/sys_ppu_thread.c"','#include "'+str(root/'runtime/syscalls/sys_ppu_thread.c')+'"')
source=source.replace('static void guest_entry(ppu_context* ctx)','static long long diag_enter_ms, diag_publish_ms;\nstatic void guest_entry(ppu_context* ctx)')
source=source.replace('    switch (ctx->gpr[3]) {','    __atomic_store_n(&diag_enter_ms, now_ms(), __ATOMIC_RELEASE);\n    switch (ctx->gpr[3]) {')
source=source.replace('        __atomic_store_n(&g_published, 1, __ATOMIC_RELAXED);','        __atomic_store_n(&diag_publish_ms, now_ms(), __ATOMIC_RELEASE);\n        __atomic_store_n(&g_published, 1, __ATOMIC_RELAXED);')
source=source.replace('    const long long t0 = now_ms();','    __atomic_store_n(&diag_enter_ms, 0, __ATOMIC_RELEASE);\n    __atomic_store_n(&diag_publish_ms, 0, __ATOMIC_RELEASE);\n    const long long t0 = now_ms();')
source=source.replace('    return now_ms() - t0;','    const long long elapsed = now_ms() - t0;\n    printf("TIMING on=%d priority=%d body=%llu elapsed=%lld enter_delay=%lld publish_delay=%lld published=%d\\n", getenv("PS3_PRIO_START") != NULL, priority, (unsigned long long)body, elapsed, __atomic_load_n(&diag_enter_ms, __ATOMIC_ACQUIRE)-t0, __atomic_load_n(&diag_publish_ms, __ATOMIC_ACQUIRE)-t0, __atomic_load_n(&g_published, __ATOMIC_RELAXED));\n    return elapsed;')
results=[]
with tempfile.TemporaryDirectory(prefix='prio-start-audit-') as tmp:
 tmp=Path(tmp);diag=tmp/'instrumented.c';diag.write_text(source)
 for variant,src,count in [('upstream-original',root/'tests/test_ppu_prio_start.c',12),('upstream-timing',diag,24)]:
  binary=tmp/variant
  cmd=['clang','-std=gnu17','-D_GNU_SOURCE','-O1','-UNDEBUG','-ffunction-sections','-fdata-sections','-I'+str(root/'include'),'-I'+str(root/'runtime'),'-I'+str(root/'runtime/syscalls'),str(src),'-Wl,-dead_strip','-o',str(binary)]
  subprocess.run(cmd,check=True)
  for i in range(count):
   env={k:v for k,v in os.environ.items() if k!='PS3_PRIO_START'};env['PS3_PRIO_START']='1'
   p=subprocess.run([str(binary)],capture_output=True,text=True,timeout=60,env=env)
   item=dict(variant=variant,iteration=i,exit=p.returncode,timing=[x for x in p.stdout.splitlines() if x.startswith('TIMING')],failures=[x for x in p.stderr.splitlines() if 'FAIL' in x or 'did not block within' in x]);results.append(item);print(json.dumps(item),flush=True)
print('DIAGNOSTIC_SUMMARY',json.dumps(dict(total=len(results),failed=sum(r['exit']!=0 for r in results),results=results)),flush=True)
print('Diagnostic workflow records original assertion failures as data; its exit status is not a passing result for those cases.',flush=True)

