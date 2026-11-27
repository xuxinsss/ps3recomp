#include "../cellSpurs.h"
#include "../../../runtime/platform/win32_compat.h"
#include <stdio.h>
#include <stdlib.h>
extern uint8_t* vm_base;
#define GUEST_PTR(p,T) ((T)((p)?(void*)(vm_base+(uint32_t)(uintptr_t)(p)):(void*)0))
#include "spurs_ports_under_test.inc"
#include <assert.h>
uint8_t* vm_base;
void sys_event_queue_cancel_by_id(uint32_t q) {(void)q;}
void sys_event_queue_uncancel_by_id(uint32_t q) {(void)q;}
int main(void) {
 vm_base=calloc(1,0x10000); assert(vm_base);
 CellSpurs* a=(CellSpurs*)(uintptr_t)0x2000;
 CellSpurs* b=(CellSpurs*)(uintptr_t)0x4000;
 u8* port=(u8*)(uintptr_t)0x100;
 vm_base[0x100]=1;
 assert(cellSpursAttachLv2EventQueue(a,11,port,0)==CELL_OK);
 assert(cellSpursAttachLv2EventQueue(b,22,port,0)==CELL_OK);
 assert(s_spurs_event_queue_n==2);
 assert(cellSpursAttachLv2EventQueue(a,33,port,0)==CELL_SPURS_CORE_ERROR_BUSY);
 assert(s_spurs_event_queue_n==2);
 assert(cellSpursDetachLv2EventQueue(a,1)==CELL_OK);
 assert(port_binding(0x4000,1)>=0 && port_binding(0x2000,1)<0);
 vm_base[0x100]=0;
 assert(cellSpursAttachLv2EventQueue(a,33,port,1)==CELL_OK && vm_base[0x100]==16);
 assert(cellSpursAttachLv2EventQueue(b,44,port,1)==CELL_OK && vm_base[0x100]==16);
 spurs_cancel_attached_queues(0x2000);
 assert(port_binding(0x4000,1)>=0 && port_binding(0x4000,16)>=0 && s_spurs_event_queue_n==2);
 puts("PASS: per-instance static/dynamic ports, duplicate rejection, detach and owner cancellation");
 free(vm_base); return 0;
}

