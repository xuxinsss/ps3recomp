from pathlib import Path
import os,sys,subprocess,tempfile,json,re,hashlib,time
root=Path(__file__).resolve().parent
profile=sys.argv[1] if len(sys.argv)>1 else 'default'
out=root/'prio-evidence'/profile;out.mkdir(parents=True,exist_ok=True)
header=r'''
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <stdatomic.h>
#include <string.h>
#include <sched.h>
enum {D_CREATE,D_WAIT,D_ENTRY,D_SLEEP_BEGIN,D_SLEEP_END,D_PUBLISH,D_RETURN,D_LOG_DONE,D_LOCK_BEGIN,D_LOCK_END,D_FREE,D_OBSERVE,D_CREATE_END,D_COUNT};
static _Atomic long long dt[65][D_COUNT];
static _Atomic long long max_nap[65],nap_sum[65],nap_n[65],nap_at[65];
static _Atomic int dr[65], dq[65];
static long long dns(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000000000LL+t.tv_nsec; }
static void mark(unsigned id,unsigned event) {if(id<65)atomic_store_explicit(&dt[id][event],dns(),memory_order_relaxed);}
static void reason(unsigned id,int why) {mark(id,D_OBSERVE);atomic_store(&dr[id],why);}
static void nap(unsigned id,long long begin) {long long elapsed=dns()-begin;if(id<65){atomic_fetch_add(&nap_sum[id],elapsed);atomic_fetch_add(&nap_n[id],1);if(elapsed>atomic_load(&max_nap[id])){atomic_store(&max_nap[id],elapsed);atomic_store(&nap_at[id],begin);}}}
static int get_qos(void) {qos_class_t q;int rel;int rc=pthread_get_qos_class_np(pthread_self(),&q,&rel);return rc ? -rc : (int)q;}
static void setup_profile(void) {
 const char *p=getenv("DIAG_QOS");int rc=0;
 if(p && !strcmp(p,"background"))rc=pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND,0);
 if(p && !strcmp(p,"interactive"))rc=pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0);
 printf("PROFILE qos=%u change_rc=%d\n",(unsigned)get_qos(),rc);
 if(rc)exit(88);
}
static void dump(unsigned id,unsigned long long body,long long begin,long long end) {
 if(id>=65)abort();
 atomic_store(&dt[id][D_CREATE],begin);atomic_store(&dt[id][D_CREATE_END],end);
 printf("TRACE {\"tid\":%u,\"body\":%llu,\"elapsed_ms\":%.3f,\"reason\":%d,\"qos\":%d,\"nap_max_ms\":%.3f,\"nap_sum_ms\":%.3f,\"nap_n\":%lld,\"nap_at_ms\":%.3f,\"events_ms\":[",id,body,(end-begin)/1e6,atomic_load(&dr[id]),atomic_load(&dq[id]),atomic_load(&max_nap[id])/1e6,atomic_load(&nap_sum[id])/1e6,atomic_load(&nap_n[id]),(atomic_load(&nap_at[id])-begin)/1e6);
 for(unsigned i=0;i<D_COUNT;i++){long long v=atomic_load(&dt[id][i]);printf("%s%.3f",i?",":"",v?(v-begin)/1e6:-1.0);}puts("]}");
}
'''
source=(root/'tests/test_ppu_prio_start.c').read_text();runtime=(root/'runtime/syscalls/sys_ppu_thread.c').read_text()
def once(s,a,b):
 assert s.count(a)==1,(a,s.count(a));return s.replace(a,b)
# Keep the runtime state machine and timeout unchanged. Record timestamps without logging inside its polling loop.
runtime=once(runtime,'    const ULONGLONG start = GetTickCount64();','    mark(tid,D_WAIT);\n    const ULONGLONG start = GetTickCount64();')
runtime=once(runtime,'if (__atomic_load_n(&t->blocked, __ATOMIC_ACQUIRE)) return;','if (__atomic_load_n(&t->blocked, __ATOMIC_ACQUIRE)) {reason(tid,1); return;}')
runtime=once(runtime,'if (state == PPU_THREAD_STATE_FINISHED || state == PPU_THREAD_STATE_FREE) return;','if (state == PPU_THREAD_STATE_FINISHED || state == PPU_THREAD_STATE_FREE) {reason(tid,2); return;}')
runtime=once(runtime,'if (waited >= PRIO_START_TIMEOUT_MS) break;','if (waited >= PRIO_START_TIMEOUT_MS) {reason(tid,3); break;}')
runtime=once(runtime,'else                             Sleep(1);','else {long long nb=dns();Sleep(1);nap(tid,nb);}')
runtime=once(runtime,'        s_exit_armed = 0;\n        fprintf(stderr, "[THREAD','        mark(info->ctx.thread_id,D_RETURN);\n        s_exit_armed = 0;\n        fprintf(stderr, "[THREAD')
runtime=once(runtime,'        /* An established interrupt handler','        mark(info->ctx.thread_id,D_LOG_DONE);\n        /* An established interrupt handler')
runtime=once(runtime,'    /* Mark as finished */\n    table_lock();','    /* Mark as finished */\n    mark(info->ctx.thread_id,D_LOCK_BEGIN);\n    table_lock();\n    mark(info->ctx.thread_id,D_LOCK_END);')
runtime=once(runtime,'        info->state = PPU_THREAD_STATE_FREE;','        mark(info->ctx.thread_id,D_FREE);\n        info->state = PPU_THREAD_STATE_FREE;')
# Resolve quoted local includes from the copied runtime's original directory.
runtime=re.sub(r'#include "([^"]+)"',lambda m:'#include "'+(root/'runtime/syscalls'/m[1]).resolve().as_posix()+'"' if (root/'runtime/syscalls'/m[1]).exists() else m[0],runtime)
source=once(source,'    switch (ctx->gpr[3]) {','    mark(ctx->thread_id,D_ENTRY);atomic_store(&dq[ctx->thread_id],(int)get_qos());\n    switch (ctx->gpr[3]) {')
assert source.count('        sleep_ms(PUBLISH_DELAY_MS);')==2
source=source.replace('        sleep_ms(PUBLISH_DELAY_MS);','        mark(ctx->thread_id,D_SLEEP_BEGIN);sleep_ms(PUBLISH_DELAY_MS);mark(ctx->thread_id,D_SLEEP_END);')
source=source.replace('        __atomic_store_n(&g_published, 1, __ATOMIC_RELAXED);','        __atomic_store_n(&g_published, 1, __ATOMIC_RELAXED);mark(ctx->thread_id,D_PUBLISH);')
source=once(source,'    const long long t0 = now_ms();','    const long long diag_begin = dns();\n    const long long t0 = now_ms();')
source=once(source,'    return now_ms() - t0;','    const long long elapsed = now_ms() - t0;\n    dump(__builtin_bswap64(*(uint64_t*)(vm_base+0x100)),body,diag_begin,dns());\n    return elapsed;')
source=once(source,'    const int on = getenv("PS3_PRIO_START") != NULL;','    setup_profile();\n    const int on = getenv("PS3_PRIO_START") != NULL;')
loads=[];results=[]
try:
 with tempfile.TemporaryDirectory(prefix='prio-rootcause-') as tmp:
  tmp=Path(tmp);(tmp/'diag.h').write_text(header);(tmp/'runtime.c').write_text(runtime)
  source=source.replace('#include "../runtime/syscalls/sys_ppu_thread.c"','#include "'+(tmp/'diag.h').as_posix()+'"\n#include "'+(tmp/'runtime.c').as_posix()+'"')
  (tmp/'trace.c').write_text(source)
  # Wrapper sets only the host QoS. Original fixture and runtime files stay byte-for-byte untouched.
  wrapper='#include "'+(tmp/'diag.h').as_posix()+'"\n#define main original_main\n#include "'+(root/'tests/test_ppu_prio_start.c').as_posix()+'"\n#undef main\nint main(void){setup_profile();return original_main();}\n'
  (tmp/'original.c').write_text(wrapper)
  binaries={}
  for name,src in [('original',root/'tests/test_ppu_prio_start.c' if profile in ('default','load') else tmp/'original.c'),('trace',tmp/'trace.c')]:
   binary=tmp/name;cmd=['clang','-std=gnu17','-D_GNU_SOURCE','-O1','-UNDEBUG','-ffunction-sections','-fdata-sections','-I'+str(root/'include'),'-I'+str(root/'runtime'),'-I'+str(root/'runtime/syscalls'),str(src),'-Wl,-dead_strip','-o',str(binary)]
   subprocess.run(cmd,check=True);binaries[name]=binary
  (out/'instrumented-runtime.c').write_text(runtime);(out/'instrumented-fixture.c').write_text(source);(out/'diag.h').write_text(header)
  if profile=='load':
   for _ in range(min(12,2*(os.cpu_count() or 2))):loads.append(subprocess.Popen([sys.executable,'-c','while True: pass'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL))
   time.sleep(1)
  for variant,count in [('original',20),('trace',60)]:
   for i in range(count):
    env={**os.environ,'PS3_PRIO_START':'1','DIAG_QOS':profile}
    p=subprocess.run([str(binaries[variant])],env=env,capture_output=True,text=True,timeout=30)
    (out/f'{variant}-{i:03}.stdout').write_text(p.stdout);(out/f'{variant}-{i:03}.stderr').write_text(p.stderr)
    traces=[json.loads(l[6:]) for l in p.stdout.splitlines() if l.startswith('TRACE ')]
    rec=dict(profile=profile,variant=variant,iteration=i,exit=p.returncode,failures=[l for l in p.stderr.splitlines() if l.startswith('FAIL')],body2=[t for t in traces if t['body']==2],all_traces=traces)
    results.append(rec);print('CASE '+json.dumps({k:v for k,v in rec.items() if k!='all_traces'}),flush=True)
  (out/'results.json').write_text(json.dumps(results,indent=2))
  print('RESULT_SUMMARY '+json.dumps(dict(profile=profile,total=len(results),failed=sum(r['exit']!=0 for r in results),traced_body2_failures=[r for r in results if r['variant']=='trace' and any('PRIO_START_TIMEOUT_MS - 20' in f for f in r['failures'])])),flush=True)
finally:
 for p in loads:p.terminate()
 for p in loads:p.wait(timeout=10)

