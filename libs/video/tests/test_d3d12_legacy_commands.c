/* Regression for the legacy D3D12 command bridge: inline packets, constant
 * attribute defaults, overflow rejection and primitive-restart registers. */
#include "rsx_commands.h"
#include "rsx_vertex_fetch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
u8* vm_base;
u32 ppu_hle_inject_base;
void vm_write32(u32 a,u32 v) {(void)a;(void)v;}
void ps3_ms(const char* s) {(void)s;}
unsigned long long ps3_ms_now(void) {return 0;}
u32 gcm_io_offset_to_ea(u32 x) {return x;}
u32 cellGcmResolveLocated(int l,u32 x) {(void)l;return x;}
static rsx_state state;
static int draws;
static void draw(void* opaque,u32 topology,u32 first,u32 count) {
 (void)opaque; (void)topology;
 assert(first==0 && count==3 && state.inline_stride==8);
 assert(state.inline_bytes==24);
 for(u32 i=0;i<3;i++) {
  float v[4]; rsx_fetch_attrib(&state,0,i,v);
  assert(v[0]==(float)(i*2+1) && v[1]==(float)(i*2+2));
  assert(v[2]==0.0f && v[3]==1.0f);
 }
 draws++;
}
int main(void) {
 rsx_backend backend={0}; backend.supports_inline_arrays=1; backend.draw_arrays=draw;
 rsx_state_init(&state); rsx_set_backend(&backend);
 state.vertex_attribs[0].enabled=1; state.vertex_attribs[0].type=2;
 state.vertex_attribs[0].size=2; state.vertex_attribs[0].stride=8;
 state.vertex_data4f[0][2]=9.0f;
 rsx_process_method(&state,0x1808,5);
 for(int i=1;i<=6;i++) {
  float f=(float)i; u32 word; memcpy(&word,&f,4);
  rsx_process_method(&state,0x1818,word);
  assert(draws==0);
 }
 rsx_process_method(&state,0x1808,0);
 assert(draws==1 && state.inline_bytes==0 && state.inline_stride==0);
 state.vertex_attribs[0].enabled=0;
 float v[4]; rsx_fetch_attrib(&state,0,999,v);
 assert(v[0]==0 && v[1]==0 && v[2]==9 && v[3]==1);
 rsx_process_method(&state,0x1DAC,1); rsx_process_method(&state,0x1DB0,0xffff);
 assert(state.restart_enabled==1 && state.restart_index==0xffff);
 rsx_process_method(&state,0x1808,5);
 state.inline_bytes=sizeof(state.inline_data);
 rsx_process_method(&state,0x1818,0);
 rsx_process_method(&state,0x1808,0);
 assert(draws==1 && state.inline_bytes==0);
 puts("PASS legacy inline packets, fetch defaults, overflow and restart state");
 return 0;
}


