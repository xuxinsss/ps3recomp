/* Manufactured policy modules exercise the real initializer, scheduler and
 * SPU policy execution. No game code, addresses or completion substitutions. */
#define spu_coh_notify_write fixture_eventflag_notify
#define spu_linelock_lock fixture_eventflag_lock
#define spu_taskset_signal_task fixture_signal_task
#define spu_workload_dispatch_async fixture_unbounded_dispatch
#include "../libs/spurs/cellSpurs.c"
#undef spu_workload_dispatch_async
#undef spu_coh_notify_write
#undef spu_linelock_lock
#undef spu_taskset_signal_task
#include <assert.h>
#include <stdatomic.h>
#include "../runtime/spu/spu_coherency.h"

void spu_indirect_branch(spu_context*);
void spu_halt(spu_context*);
uint8_t* vm_base;
uint32_t ppu_vm_size=0x20000;
int g_resv_store_active;
uint32_t g_ww_lo,g_ww_hi,g_barrier_sync_watch;
uint32_t g_spu_image_src_ea,g_spu_image_ls_start,g_spu_image_span;
unsigned long long ps3_ms_now(void) { LARGE_INTEGER n,f; QueryPerformanceCounter(&n); QueryPerformanceFrequency(&f); return n.QuadPart*1000/f.QuadPart; }
void ppu_resv_break_store(uint64_t ea) {(void)ea;}
void ps3_ww_report_inline(uint32_t a,uint64_t v,int w){(void)a;(void)v;(void)w;}
/* A commit to a registered taskset's first line starts its pending tasks,
 * which keeps spu_async_run's task start in the link. This fixture makes
 * no taskset, so none of these is called. */
uint64_t spurs_pm_build_context(uint8_t* ls,uint32_t ts,uint32_t id,uint32_t spu,uint32_t tag)
{(void)ls;(void)ts;(void)id;(void)spu;(void)tag;return 0;}
void spu_thread_publish_ctx(uint32_t tid,void* c){(void)tid;(void)c;}
void ydkj_wake_all_event_flags(void){}

static atomic_uint attempted, completed, signaled;
void spu_linelock_lock(uint32_t);
static atomic_int watching, reuse_slot;
void spu_coh_notify_write(uint32_t);
/* Force slot reuse after the guest transaction is published, before signal.
 * Notification is still performed by the production coherency implementation. */
void fixture_eventflag_notify(uint32_t ea)
{
    spu_coh_notify_write(ea);
    if (ea==0x10000u && atomic_exchange(&reuse_slot,0)) {
        spu_linelock_lock(ea);
        vm_base[ea+EF_WAITING_TASK_ID]=42;
        spu_linelock_release(ea);
    }
}
void spu_linelock_lock(uint32_t);
void fixture_eventflag_lock(uint32_t ea)
{
    if (atomic_load(&watching)) atomic_store(&attempted, 1);
    spu_linelock_lock(ea);
}
void fixture_signal_task(uint32_t ts, uint32_t id)
{
    assert(ts == 0x18000 && id == 7);
    atomic_fetch_add(&signaled, 1);
}
void sys_event_queue_cancel_by_id(u32 id) { (void)id; }
void sys_event_queue_uncancel_by_id(u32 id) { (void)id; }
int fixture_unbounded_dispatch(const uint8_t* p,uint32_t s,uint32_t d)
{ (void)p;(void)s;(void)d;return 0; }
#define FLAG 0x10000u
#define OUT  0x11000u
static unsigned operation;
static s32 result;
static void wait_until(atomic_uint* p)
{
    for (unsigned i=0;i<2000;i++) { if(atomic_load(p)) return; Sleep(1); }
    assert(!"EventFlag operation did not enter the guest line transaction");
}
static void* receiver(void* unused)
{
    (void)unused;
    CellSpursEventFlag* flag=(CellSpursEventFlag*)(uintptr_t)FLAG;
    if(operation==0) result=cellSpursEventFlagSet(flag, 1);
    if(operation==1) result=cellSpursEventFlagTryWait(flag,(u16*)(uintptr_t)OUT,CELL_SPURS_EVENT_FLAG_AND);
    if(operation==2) result=cellSpursEventFlagWait(flag,(u16*)(uintptr_t)OUT,CELL_SPURS_EVENT_FLAG_AND);
    if(operation==3) result=cellSpursEventFlagClear(flag,1);
    atomic_store(&completed,1);return NULL;
}
static void run_transaction(unsigned op, unsigned clear_mode)
{
    assert(cellSpursEventFlagInitialize((CellSpursTaskset*)(uintptr_t)0x18000,
        (CellSpursEventFlag*)(uintptr_t)FLAG,clear_mode,2)==0);
    vm_write16(OUT,1);
    operation=op;atomic_store(&attempted,0);atomic_store(&completed,0);
    /* The synthetic SPU writer pauses halfway through one atomic line update.
     * The wrapper only announces entry; the production lock still arbitrates.
     * This forces the API to arrive before the writer's final fields. */
    spu_linelock_lock(FLAG);
    ef_w16(FLAG+EF_EVENTS,1);
    atomic_store(&watching,1);
    pthread_t t;assert(pthread_create(&t,NULL,receiver,NULL)==0);
    wait_until(&attempted);
    assert(!atomic_load(&completed));
    if(op==0) {
        ef_w16(FLAG+EF_SPU_USED_SLOTS,0x8000);
        ef_w16(FLAG+EF_SPU_WAIT_MASK_ARR,1);
        vm_base[FLAG+EF_WAITING_TASK_ID]=7;
        atomic_store(&reuse_slot,1);
    }
    /* A distinct event posted by the writer must survive consuming bit 1. */
    ef_w16(FLAG+EF_EVENTS,3);
    spu_linelock_release(FLAG);
    assert(pthread_join(t,NULL)==0);atomic_store(&watching,0);
    assert(result==0);
    if(op==0) {
        assert(atomic_load(&signaled)==1);
        assert(vm_base[FLAG+EF_WAITING_TASK_ID]==42);
        assert(ef_r16(FLAG+EF_PENDING_RECV_EVT)==1);
        assert(ef_r16(FLAG+EF_SPU_PENDING_RECV)==0x8000);
    }
    if(op==1 || op==2) assert(vm_read16(OUT)==3);
    assert(ef_r16(FLAG+EF_EVENTS)==
        ((op==3 || clear_mode==CELL_SPURS_EVENT_FLAG_CLEAR_AUTO)?2:3));
}
#include "../runtime/spu/spu_dma.h"
#include "../runtime/spu/spu_helpers.h"
extern void spu_wrch(spu_context*,uint32_t,u128);
static void atomic_command(spu_context* c,uint32_t cmd)
{
    spu_wrch(c,MFC_LSA,spu_make_preferred_u32(0x1000));
    spu_wrch(c,MFC_EAH,spu_make_preferred_u32(0));
    spu_wrch(c,MFC_EAL,spu_make_preferred_u32(FLAG));
    spu_wrch(c,MFC_Cmd,spu_make_preferred_u32(cmd));
}
static void test_real_putllc_interleaving(void)
{
    static spu_context c;spu_context_init(&c,0);
    CellSpursEventFlag* f=(CellSpursEventFlag*)(uintptr_t)FLAG;
    assert(cellSpursEventFlagInitialize((CellSpursTaskset*)(uintptr_t)0x18000,
        f,CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,2)==0);
    assert(cellSpursEventFlagSet(f,1)==0);
    atomic_command(&c,MFC_GETLLAR_CMD);
    /* The host transaction lands between the guest's read and commit. */
    assert(cellSpursEventFlagSet(f,2)==0);
    c.ls[0x1001]|=4;
    atomic_command(&c,MFC_PUTLLC_CMD);
    assert(c.atomic_stat==1 && ef_r16(FLAG)==3);
    atomic_command(&c,MFC_GETLLAR_CMD);c.ls[0x1001]|=4;
    atomic_command(&c,MFC_PUTLLC_CMD);
    assert(c.atomic_stat==0 && ef_r16(FLAG)==7);
    vm_write16(OUT,1);
    assert(cellSpursEventFlagTryWait(f,(u16*)(uintptr_t)OUT,CELL_SPURS_EVENT_FLAG_AND)==0);
    assert(vm_read16(OUT)==7 && ef_r16(FLAG)==6);
    spu_coh_unregister(&c);
}
int main(void)
{
    vm_base=calloc(1,ppu_vm_size);assert(vm_base);
    run_transaction(0,CELL_SPURS_EVENT_FLAG_CLEAR_AUTO);
    run_transaction(1,CELL_SPURS_EVENT_FLAG_CLEAR_AUTO);
    run_transaction(2,CELL_SPURS_EVENT_FLAG_CLEAR_AUTO);
    run_transaction(3,CELL_SPURS_EVENT_FLAG_CLEAR_AUTO);
    run_transaction(1,CELL_SPURS_EVENT_FLAG_CLEAR_MANUAL);
    run_transaction(2,CELL_SPURS_EVENT_FLAG_CLEAR_MANUAL);
    vm_write16(OUT,4);
    assert((u32)cellSpursEventFlagTryWait((CellSpursEventFlag*)(uintptr_t)FLAG,
        (u16*)(uintptr_t)OUT,CELL_SPURS_EVENT_FLAG_AND)==CELL_SPURS_TASK_ERROR_BUSY);
    assert(vm_read16(OUT)==4 && ef_r16(FLAG+EF_EVENTS)==3);
    test_real_putllc_interleaving();
    puts("EventFlag concurrent transactions: PASS");return 0;
}
