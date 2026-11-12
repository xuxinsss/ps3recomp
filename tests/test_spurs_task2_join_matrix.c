/* Characterization, not a firmware oracle. Keep observed hazards visible. */
#define main task2_baseline_main
#include "test_spurs_task2_hle.c"
#undef main
struct observed_join { s32 rc; u32 code,out; };
static void* matrix_join(void* arg)
{
    struct observed_join* r=arg;
    vm_write32(r->out,0xdeadbeef);
    r->rc=cellSpursJoinTask2(TS,0,r->out);r->code=vm_read32(r->out);
    return NULL;
}
static FILE* trace;
static void observation(const char* name,s32 rc,u32 code,int hazard)
{
    fprintf(trace,"{\"case\":\"%s\",\"rc\":%d,\"code\":%u,\"hazard\":%s,\"oracle\":\"host-characterization\"}\n",name,rc,code,hazard?"true":"false");
}
int main(void)
{
    assert(task2_baseline_main(1,NULL)==0);
    trace=fopen(getenv("TASK2_JOIN_RESULTS"),"w");assert(trace);
    assert(create_taskset(TS)==0);
    vm_write32(CODE,0xdeadbeef);
    s32 rc=cellSpursJoinTask2(TS,0,CODE);
    assert(rc==CELL_SPURS_TASK_ERROR_SRCH && vm_read32(CODE)==0xdeadbeef);
    observation("never-created",rc,vm_read32(CODE),0);
    spu_taskset2_set_exit(TS,0,1,0x11223344);
    assert(cellSpursJoinTask2(TS,0,CODE)==0 && vm_read32(CODE)==0x11223344);
    assert(cellSpursJoinTask2(TS,0,CODE)==0 && vm_read32(CODE)==0x11223344);
    observation("sequential-rejoin",0,vm_read32(CODE),0);
    struct observed_join pair[2]={{.out=CODE},{.out=CODE+4}};pthread_t threads[2];
    for(unsigned i=0;i<2;i++)assert(!pthread_create(&threads[i],NULL,matrix_join,&pair[i]));
    for(unsigned i=0;i<2;i++) {
        pthread_join(threads[i],NULL);assert(pair[i].rc==0 && pair[i].code==0x11223344);
    }
    observation("concurrent-rejoin",pair[1].rc,pair[1].code,0);
    destroy_quiet(TS);
    assert(create_taskset(TS)==0);
    spu_taskset2_set_exit(TS,0,1,0x11223344); /* A exits, has not been joined */
    atomic_store(&tail_arrived,0);atomic_store(&tail_go,0);atomic_store(&tail_target,TS);
    assert(create(TS,OUT,BIN)==0 && vm_read32(OUT)==0);wait_for(&tail_arrived);
    assert(cellSpursJoinTask2(TS,0,CODE)==0 && vm_read32(CODE)==0);
    observation("reuse-before-first-join",0,vm_read32(CODE),1);
    atomic_store(&tail_go,1);destroy_quiet(TS);atomic_store(&tail_target,0);
    /* Pause a Join of incarnation A between polls. Finish A, actually create
     * B using the same ID, and hold B at its real host-thread cleanup tail. */
    assert(create_taskset(TS)==0);
    spu_taskset2_set_exit(TS,0,0,0);spurs_taskset_bitset_update(TS+CSTS_ENABLED,0,1);
    atomic_store(&join_wait_arrived,0);atomic_store(&join_wait_go,0);atomic_store(&join_wait_target,TS);
    struct observed_join old={.out=CODE};pthread_t joining;
    assert(!pthread_create(&joining,NULL,matrix_join,&old));wait_for(&join_wait_arrived);
    spu_taskset2_set_exit(TS,0,1,0x11223344);spurs_taskset_bitset_update(TS+CSTS_ENABLED,0,0);
    atomic_store(&tail_arrived,0);atomic_store(&tail_go,0);atomic_store(&tail_target,TS);
    assert(create(TS,OUT,BIN)==0 && vm_read32(OUT)==0);wait_for(&tail_arrived);
    atomic_store(&join_wait_go,1);pthread_join(joining,NULL);atomic_store(&join_wait_target,0);
    /* Current implementation follows ID 0 to B (exit code 0), losing A's code.
     * This assertion records a known hazard; it does not approve the behavior. */
    assert(old.rc==0 && old.code==0);
    observation("waiting-A-reused-by-B",old.rc,old.code,1);
    atomic_store(&tail_go,1);destroy_quiet(TS);atomic_store(&tail_target,0);
    assert(!atomic_load(&live_ls) && active_flags()==0);
    assert(!fclose(trace));
    puts("Task2 Join characterization: CAPTURED (firmware comparison pending)");
    return 0;
}
