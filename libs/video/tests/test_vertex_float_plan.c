#include "rsx_vertex_float_plan.h"
#include <assert.h>
#include <stdio.h>
static u8 memory[0x40000];u8* vm_base=memory;
static rsx_state state;
u32 cellGcmResolveLocated(int local,u32 offset)
{return local?0x1000u+offset:0x10000u+((offset&0x1000u)?0x8000u:0)+(offset&0xfffu);}
u32 cellGcmResolveOffset(u32 x){return cellGcmResolveLocated(0,x);}
int main(void)
{
 for(unsigned i=0;i<sizeof memory;i++)memory[i]=(u8)(i*73u+(i>>5));
 unsigned checked=0,fast=0;const u32 ids[]={0,1,7,254,255,256,257,511,0x10000000u,0xffffffffu};
 for(unsigned local=0;local<2;local++)for(unsigned type=0;type<9;type++)
 for(unsigned n=0;n<6;n++)for(unsigned freq=0;freq<4;freq++)for(unsigned mod=0;mod<2;mod++){
  memset(&state,0,sizeof state);rsx_vertex_attrib* a=&state.vertex_attribs[3];
  a->enabled=1;a->offset=(local?0:0x80000000u)|0x100u;a->stride=16;a->type=type;a->size=n;a->frequency=freq;
  state.frequency_divider_op=mod<<3;rsx_vertex_float_plan plan;rsx_vertex_float_prepare(&state,3,&plan);
  if(!plan.enabled)continue;
  for(unsigned i=0;i<sizeof ids/sizeof ids[0];i++){
   float original[4]={7,8,9,10},prepared[4]={7,8,9,10};
   rsx_fetch_enabled_attrib(&state,3,ids[i],original);rsx_vertex_float_fetch(&plan,ids[i],prepared);
   assert(memcmp(original,prepared,sizeof original)==0);checked++;
  }fast++;
 }
 rsx_vertex_float_plan plan;state.vertex_attribs[3].type=2;state.vertex_attribs[3].frequency=0;
 state.inline_stride=16;rsx_vertex_float_prepare(&state,3,&plan);assert(!plan.enabled);
 state.inline_stride=0;state.vertex_attribs[3].stride=0;rsx_vertex_float_prepare(&state,3,&plan);assert(!plan.enabled);
 state.vertex_attribs[3].stride=16;state.vertex_attribs[3].enabled=0;rsx_vertex_float_prepare(&state,3,&plan);assert(!plan.enabled);
 printf("PASS %u bit-exact float fetches across %u layouts, MAIN page remapping, index wrap and fallback guards\n",checked,fast);return 0;
}
