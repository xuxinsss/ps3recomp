#define spu_linelock_lock fixture_task2_line_lock
#define spu_workload_dispatch_task fixture_dispatch_task
#include "../libs/spurs/cellSpurs.c"

#undef spu_workload_dispatch_task
#undef spu_linelock_lock
#include <assert.h>
#include <stdatomic.h>
#include "../runtime/spu/spu_coherency.h"
#include "../runtime/syscalls/sys_event.h"

void spu_indirect_branch(spu_context*);
void spu_halt(spu_context*);
uint8_t* vm_base;
uint32_t ppu_vm_size=0x80000;
int g_resv_store_active;
uint32_t g_ww_lo,g_ww_hi,g_barrier_sync_watch;
uint32_t g_spu_image_src_ea,g_spu_image_ls_start,g_spu_image_span;
unsigned long long ps3_ms_now(void) { LARGE_INTEGER n,f; QueryPerformanceCounter(&n); QueryPerformanceFrequency(&f); return n.QuadPart*1000/f.QuadPart; }
void ppu_resv_break_store(uint64_t ea) {(void)ea;}
void ps3_ww_report_inline(uint32_t a,uint64_t v,int w){(void)a;(void)v;(void)w;}
/* Manufacture an entry/context rather than executing proprietary guest code. */
uint64_t spurs_pm_build_context(uint8_t* ls,uint32_t ts,uint32_t id,uint32_t spu,uint32_t tag)
{(void)ls;(void)ts;(void)id;(void)spu;(void)tag;return 0;}
void spu_thread_publish_ctx(uint32_t tid,void* c){(void)tid;(void)c;}

int spu_workload_dispatch_task(const uint8_t*,uint32_t,uint32_t,uint32_t,uint32_t);
void spu_linelock_lock(uint32_t);
static atomic_uint watched_record,join_arrived,join_done;
void fixture_task2_line_lock(uint32_t ea)
{
    if(ea==atomic_load(&watched_record))atomic_store(&join_arrived,1);
    spu_linelock_lock(ea);
}
static int dispatch_fail;
static unsigned dispatch_calls;
int fixture_dispatch_task(const uint8_t* image,uint32_t size,uint32_t args,uint32_t ts,uint32_t id)
{
    dispatch_calls++;
    if(dispatch_fail)return 0;
    return spu_workload_dispatch_task(image,size,args,ts,id);
}
#define TS 0x10000u
#define TS_B 0x18000u
#define BIN 0x30000u
#define ELF 0x40000u
#define OUT 0x31000u
#define CODE 0x31010u
static void init_ts(u32 ea)
{
    memset(vm_base+ea,0,TS2_STRUCT_END);
    spurs_taskset_init(ea,0,0,0,TS2_STRUCT_END,0,0);
    extern void spu_taskset2_register(uint32_t);
    spu_taskset2_register(ea);
}
static void manufactured_task(spu_context* c){(void)c;}
static s32 create(u64 ts,u64 out,u64 bin)
{ return cellSpursCreateTask2WithBinInfo(ts,out,bin,0,0); }
static void unchanged_failure(s32 expected,u64 ts,u64 out,u64 bin)
{
    vm_write32(OUT,0xdeadbeef);
    u8 before[128];memcpy(before,vm_base+TS,128);
    s32 rc=create(ts,out,bin);
    if(rc!=expected) fprintf(stderr,"create expected=%08x got=%08x ts=%llx out=%llx bin=%llx\n",(u32)expected,(u32)rc,(unsigned long long)ts,(unsigned long long)out,(unsigned long long)bin);
    assert(rc==expected);
    assert(vm_read32(OUT)==0xdeadbeef);
    assert(memcmp(before,vm_base+TS,128)==0);
}
static void* publish(void* unused)
{
    (void)unused;
    extern void spu_taskset2_set_exit(uint32_t,uint32_t,int,uint32_t);
    spu_taskset2_set_exit(TS,3,1,0xaabbccdd);
    return NULL;
}
static void* join_worker(void* unused)
{ (void)unused;assert(cellSpursJoinTask2(TS,3,CODE)==0);atomic_store(&join_done,1);return NULL; }
struct creation { u32 ts,out; };
static void* create_worker(void* arg)
{
    struct creation* p=arg;
    for(unsigned i=0;i<8;i++) {
        assert(create(p->ts,p->out,BIN)==0);
        assert(cellSpursJoinTask2(p->ts,vm_read32(p->out),p->out+4)==0);
        assert(vm_read32(p->out+4)==0);
    }
    return NULL;
}
static s32 create_taskset(u64 ea)
{ return cellSpursCreateTaskset2((CellSpurs*)(uintptr_t)0x1000,
                               (CellSpursTaskset*)(uintptr_t)ea, 0); }
static unsigned active_flags(void)
{
    unsigned n=0;
    for(unsigned i=0;i<SYS_EVENT_FLAG_MAX;i++)n+=!!g_sys_event_flags[i].active;
    return n;
}
static atomic_uint reserve_ready,reserve_go;
static void* reserve_worker(void* result)
{
    atomic_fetch_add(&reserve_ready,1);
    while(!atomic_load(&reserve_go))Sleep(0);
    *(int*)result=spu_taskset2_prepare(0x70000);
    return NULL;
}
static void unpublished_taskset(void)
{
    pthread_t threads[8];int results[8];
    for(unsigned i=0;i<8;i++)
        assert(pthread_create(&threads[i],NULL,reserve_worker,&results[i])==0);
    for(unsigned i=0;i<2000 && atomic_load(&reserve_ready)!=8;i++)Sleep(1);
    assert(atomic_load(&reserve_ready)==8);atomic_store(&reserve_go,1);
    unsigned winners=0;
    for(unsigned i=0;i<8;i++) {
        pthread_join(threads[i],NULL);
        assert(results[i]==1 || results[i]==-1);winners+=results[i]==1;
    }
    assert(winners==1 && !spu_taskset2_is_registered(0x70000));
    /* Even an eligible guest task cannot run while flags/metadata are still
     * being initialized. Exercise the real commit callback and scheduler. */
    spurs_taskset_init(0x70000,0x1000,0,0,TS2_STRUCT_END,0,0);
    const u32 zero[4]={0};
    spurs_taskset_add_task(0x70000,0,ELF,0,zero,zero);
    spurs_taskset_bitset_update(0x70000+CSTS_PENDING_READY,0,1);
    extern void spu_taskset_line_committed(uint32_t);
    spu_taskset_line_committed(0x70000);
    assert(vm_base[0x70000+CSTS_PENDING_READY]&0x80);
    assert(vm_base[0x70000+CSTS_ENABLED]&0x80);
    spu_taskset2_abort(0x70000);
    memset(vm_base+0x70000,0xa5,TS2_STRUCT_END);
    extern void spu_taskset_start_pending(uint32_t);
    spu_taskset_start_pending(0x70000); /* delayed callback after abort */
    for(unsigned i=0;i<TS2_STRUCT_END;i++)assert(vm_base[0x70000+i]==0xa5);
    assert(spu_taskset2_prepare(0x70000)==1);
    spu_taskset2_abort(0x70000);
    puts("Taskset2 concurrent reservation and unpublished scheduling guard: PASS");
}
static void assert_failed_taskset(u32 ea)
{
    memset(vm_base+ea,0xa5,TS2_STRUCT_END);
    u32 old_ts=g_ydkj_real_taskset_ea,old_spurs=g_ydkj_real_spurs_ea;
    unsigned before=active_flags();
    assert(create_taskset(ea)==CELL_SPURS_TASK_ERROR_NOMEM);
    for(unsigned i=0;i<TS2_STRUCT_END;i++)assert(vm_base[ea+i]==0xa5);
    assert(active_flags()==before && !spu_taskset2_is_registered(ea));
    assert(g_ydkj_real_taskset_ea==old_ts && g_ydkj_real_spurs_ea==old_spurs);
}
static void taskset_creation_failures(void)
{
    /* Actual lv2 allocation exhaustion: first flag fails, then only the
     * second fails. Repetition catches leaked flags and prepared slots. */
    u32 flags[SYS_EVENT_FLAG_MAX];
    for(unsigned i=0;i<SYS_EVENT_FLAG_MAX;i++) {
        assert(sys_event_flag_create_host(OUT,0)==0);flags[i]=vm_read32(OUT);
    }
    for(unsigned i=0;i<20;i++)assert_failed_taskset(0x20000);
    assert(sys_event_flag_destroy_host(flags[SYS_EVENT_FLAG_MAX-1])==0);
    for(unsigned i=0;i<40;i++)assert_failed_taskset(0x20000);
    for(unsigned i=0;i<SYS_EVENT_FLAG_MAX-1;i++)
        assert(sys_event_flag_destroy_host(flags[i])==0);
    assert(active_flags()==0);
    assert(cellSpursCreateTaskset2((CellSpurs*)(uintptr_t)0x1000,
        (CellSpursTaskset*)(uintptr_t)0x100020000ull,0)==CELL_SPURS_TASK_ERROR_FAULT);
    assert(cellSpursCreateTaskset2((CellSpurs*)(uintptr_t)0x1000,
        (CellSpursTaskset*)(uintptr_t)0x20000,0x100030000ull)==CELL_SPURS_TASK_ERROR_FAULT);
    assert(_cellSpursTasksetAttribute2Initialize(0x100030000ull,1)==CELL_SPURS_TASK_ERROR_FAULT);
    /* A committed object cannot be initialized again over its tasks/flags. */
    assert(create_taskset(0x20000)==0 && active_flags()==2);
    assert(spu_taskset2_is_registered(0x20000));
    assert(create(0x20000,OUT,BIN)==0);
    assert(cellSpursJoinTask2(0x20000,vm_read32(OUT),CODE)==0);
    u8 saved[TS2_STRUCT_END];memcpy(saved,vm_base+0x20000,sizeof saved);
    assert(create_taskset(0x20000)==CELL_SPURS_TASK_ERROR_BUSY);
    assert(!memcmp(saved,vm_base+0x20000,sizeof saved) && active_flags()==2);
    /* Keep the existing capacities: the eight-entry running directory is
     * now checked at creation, rather than silently losing duplicate guards. */
    const u32 addresses[]={0x28000,0x48000,0x50000,0x58000,0x60000};
    for(unsigned i=0;i<sizeof addresses/sizeof addresses[0];i++)
        assert(create_taskset(addresses[i])==0);
    assert(active_flags()==12); /* two manufactured tasksets already reserved */
    assert_failed_taskset(0x68000);
    puts("Taskset2 allocation rollback and bounded registration: PASS");
}
int main(int argc,char** argv)
{
    (void)argv;
    vm_base=calloc(1,ppu_vm_size);assert(vm_base);
    s_inst[0].ea=0x1000;
    if(argc>1) {
        extern void spu_taskset_register(uint32_t);
        assert(spu_taskset2_prepare(0x70000)==1);
        assert(spu_taskset2_prepare(0x68000)==1);
        spu_taskset2_abort(0x70000); /* a hole before an existing key */
        spu_taskset_register(0x68000); /* must not duplicate the key into it */
        for(unsigned i=0;i<31;i++)spu_taskset_register(0x100000+i*0x8000);
        spu_taskset2_abort(0x68000);
        spu_taskset_register(0x300000);
        assert_failed_taskset(TS);
        puts("Taskset2 full scheduling directory: PASS");return 0;
    }
    init_ts(TS);init_ts(TS_B);
    unchanged_failure(CELL_SPURS_TASK_ERROR_NULL_POINTER,TS,OUT,0);
    unchanged_failure(CELL_SPURS_TASK_ERROR_FAULT,0x100010000ull,OUT,BIN);
    unchanged_failure(CELL_SPURS_TASK_ERROR_FAULT,TS,0x100031000ull,BIN);
    unchanged_failure(CELL_SPURS_TASK_ERROR_FAULT,TS,OUT,0x100030000ull);
    vm_write64(BIN,ELF);
    unchanged_failure(CELL_SPURS_TASK_ERROR_INVAL,TS,OUT,BIN);
    /* A minimal manufactured ELF, no proprietary guest instructions. */
    memcpy(vm_base+ELF,"\177ELF\1\2",6);
    vm_write16(ELF+18,23);
    vm_write32(ELF+0x18,0x3000);vm_write32(ELF+0x1c,0x34);
    vm_write16(ELF+0x2a,0x20);vm_write16(ELF+0x2c,1);
    vm_write32(ELF+0x34,1);vm_write32(ELF+0x38,0x80);
    vm_write32(ELF+0x3c,0x3000);vm_write32(ELF+0x44,16);vm_write32(ELF+0x48,16);
    vm_write64(BIN,ELF);
    vm_write32(0x20000+CSTS_SIZE_FIELD,TS2_STRUCT_END);
    assert(create(0x20000,OUT,BIN)==CELL_SPURS_TASK_ERROR_STAT);
    assert(cellSpursDestroyTaskset2(0x20000)==CELL_SPURS_TASK_ERROR_STAT);
    /* Missing lift: real dispatch failure must not publish an output id,
     * leave enabled/ready bits, or destroy the previous exit record. */
    u32 rec=TS+TS2_EXITCODE;vm_base[rec+15]=4;vm_write32(rec+8,0x12345678);
    unchanged_failure(CELL_SPURS_TASK_ERROR_NOMEM,TS,OUT,BIN);
    assert(vm_base[rec+15]==4 && vm_read32(rec+8)==0x12345678);
    memset(vm_base+TS+0x80,0xa5,0x30);
    dispatch_fail=1;
    unchanged_failure(CELL_SPURS_TASK_ERROR_NOMEM,TS,OUT,BIN);
    for(unsigned i=0;i<0x30;i++) assert(vm_base[TS+0x80+i]==0xa5);
    dispatch_fail=0;
    spu_workload_register_img(spu_workload_fingerprint(vm_base+ELF,0x90),manufactured_task,77,"task2-test");
    /* Poison legacy globals: task identity must come from explicit arguments. */
    g_ydkj_real_taskset_ea=TS_B;g_ydkj_real_taskid=127;
    assert(create(TS,OUT,BIN)==0);u32 id=vm_read32(OUT);
    assert(cellSpursJoinTask2(TS,id,CODE)==0);assert(vm_read32(CODE)==0);
    assert(vm_base[TS_B+TS2_EXITCODE+127*16+15]==0);
    assert(cellSpursJoinTask2(TS,id,CODE)==0); /* documented local cached rejoin */
    assert(create(TS,OUT,BIN)==0 && vm_read32(OUT)==id);
    assert(cellSpursJoinTask2(TS,id,CODE)==0);
    struct creation requests[2]={{TS,OUT},{TS_B,OUT+0x100}};
    pthread_t creators[2];
    for(unsigned i=0;i<2;i++)assert(pthread_create(&creators[i],NULL,create_worker,&requests[i])==0);
    for(unsigned i=0;i<2;i++)pthread_join(creators[i],NULL);
    assert(cellSpursJoinTask2(TS,90,CODE)==CELL_SPURS_TASK_ERROR_SRCH);
    /* Hold the actual exit-record lock until Join has reached it. */
    u32 join_rec=TS+TS2_EXITCODE+3*16;
    spurs_taskset_bitset_update(TS+CSTS_ENABLED,3,1);
    spu_linelock_lock(join_rec);
    atomic_store(&watched_record,join_rec);
    pthread_t joining;assert(pthread_create(&joining,NULL,join_worker,NULL)==0);
    for(unsigned i=0;i<2000&&!atomic_load(&join_arrived);i++)Sleep(1);
    assert(atomic_load(&join_arrived)&&!atomic_load(&join_done));
    vm_base[join_rec+8]=0x12;vm_base[join_rec+9]=0x34;
    vm_base[join_rec+10]=0x56;vm_base[join_rec+11]=0x78;vm_base[join_rec+15]=2;
    spu_linelock_release(join_rec);pthread_join(joining,NULL);
    atomic_store(&watched_record,0);assert(vm_read32(CODE)==0x12345678);
    /* Concurrent real exit publisher / HLE join, including a patterned code. */
    for(unsigned i=0;i<64;i++) {
        extern void spu_taskset2_set_exit(uint32_t,uint32_t,int,uint32_t);
        spu_taskset2_set_exit(TS,3,0,0);
        pthread_t t;assert(pthread_create(&t,NULL,publish,NULL)==0);
        assert(cellSpursJoinTask2(TS,3,CODE)==0);
        assert(vm_read32(CODE)==0xaabbccdd);pthread_join(t,NULL);
    }
    assert(cellSpursJoinTask2(TS,128,CODE)==CELL_SPURS_TASK_ERROR_INVAL);
    assert(cellSpursJoinTask2(TS,3,0x100031010ull)==CELL_SPURS_TASK_ERROR_FAULT);
    vm_base[TS+TS2_EXITCODE+3*16+14]=1;
    assert(cellSpursJoinTask2(TS,3,CODE)==CELL_SPURS_TASK_ERROR_SRCH);
    spurs_taskset_bitset_update(TS+CSTS_ENABLED,3,0);
    spurs_taskset_bitset_update(TS+CSTS_ENABLED,5,1);
    assert(cellSpursDestroyTaskset2(TS)==CELL_SPURS_TASK_ERROR_BUSY);
    spurs_taskset_bitset_update(TS+CSTS_ENABLED,5,0);
    assert(cellSpursDestroyTaskset2(TS)==CELL_SPURS_TASK_ERROR_NOSYS);
    assert(dispatch_calls>=4);
    unpublished_taskset();
    taskset_creation_failures();
    puts("Task2 HLE safety and explicit dispatch: PASS");return 0;
}
