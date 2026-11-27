/* Per-draw preparation for ordinary float arrays. This caches layout only,
 * never vertex data. MAIN addresses still resolve separately for each fetch. */
#ifndef RSX_VERTEX_FLOAT_PLAN_H
#define RSX_VERTEX_FLOAT_PLAN_H
#include "rsx_vertex_fetch.h"
#include <string.h>
typedef struct { u32 enabled,base,stride,components,local; } rsx_vertex_float_plan;
static inline void rsx_vertex_float_prepare(const rsx_state* st,unsigned slot,
                                           rsx_vertex_float_plan* p)
{
    memset(p,0,sizeof *p);
    const rsx_vertex_attrib* a=&st->vertex_attribs[slot];
    /* Unknown formats retain the scalar decoder's float fallback. */
    if(!a->enabled || !a->stride || !vm_base || st->inline_stride ||
       (a->type!=2 && a->type>=1 && a->type<=7) ||
       !(a->frequency==0 || (a->frequency==1 && !(st->frequency_divider_op&(1u<<slot)))))return;
    p->enabled=1;p->local=(a->offset&0x80000000u)==0;
    p->base=a->offset&0x7fffffffu;p->stride=a->stride;
    if(p->local)p->base=cellGcmResolveLocated(1,p->base);
    p->components=a->size?a->size:4;if(p->components>4)p->components=4;
}
static inline u32 rsx_vertex_float_word(const u8* p)
{
    u32 v;memcpy(&v,p,4);
    return (v>>24)|((v>>8)&0xff00u)|((v<<8)&0xff0000u)|(v<<24);
}
static inline void rsx_vertex_float_fetch(const rsx_vertex_float_plan* plan,u32 vi,float out[4])
{
    u32 address=plan->base+vi*plan->stride;
    if(!plan->local)address=cellGcmResolveLocated(0,address);
    const u8* p=vm_base+address;
    u32 bits[4]={0,0,0,0x3f800000u};
    switch(plan->components){
    case 4:bits[3]=rsx_vertex_float_word(p+12); /* fall through */
    case 3:bits[2]=rsx_vertex_float_word(p+8);  /* fall through */
    case 2:bits[1]=rsx_vertex_float_word(p+4);  /* fall through */
    case 1:bits[0]=rsx_vertex_float_word(p);break;
    }
    memcpy(out,bits,sizeof bits);
}
#endif
