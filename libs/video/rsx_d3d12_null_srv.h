/* Remember only null views. Real resources are deliberately never cached:
 * releasing/reusing a resource pointer cannot make a stale descriptor hit. */
#ifndef RSX_D3D12_NULL_SRV_H
#define RSX_D3D12_NULL_SRV_H
#include <stdint.h>
typedef struct { uint32_t valid,format,mapping,cube; } rsx_d3d12_null_srv;
static inline int rsx_d3d12_null_srv_reuse(rsx_d3d12_null_srv* slot,
                                         int has_resource,uint32_t format,
                                         uint32_t mapping,int cube)
{
    if(has_resource){slot->valid=0;return 0;}
    uint32_t c=cube!=0;
    if(slot->valid && slot->format==format && slot->mapping==mapping && slot->cube==c)return 1;
    slot->valid=1;slot->format=format;slot->mapping=mapping;slot->cube=c;return 0;
}
#endif
