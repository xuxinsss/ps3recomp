/* Readback selection and pixel layout; GPU execution is covered by game replay. */
#include "../rsx_d3d12_cpu_readback.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    const char* bad[] = {0,"","0x0","320x0","320x160junk","-1x160","4096x160","320x160x1"};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i)
        assert(!rsx_cpu_readback_parse(bad[i]).width);
    rsx_cpu_readback_size size=rsx_cpu_readback_parse("320x160");
    assert(size.width==320 && size.height==160);
    assert(rsx_cpu_readback_matches(size,320,160,0x7080148,1280));
    assert(!rsx_cpu_readback_matches(size,640,360,0x7080148,2560));
    assert(!rsx_cpu_readback_matches(size,320,160,0x708014B,2560));
    assert(!rsx_cpu_readback_matches(size,320,160,0x7080248,1280));
    assert(!rsx_cpu_readback_matches(size,320,160,0x7080148,1279));
    const uint8_t src[24]={1,2,3,4,5,6,7,8,90,90,90,90,11,12,13,14,15,16,17,18,91,91,91,91};
    uint8_t dst[32];memset(dst,0xCC,sizeof(dst));
    assert(rsx_cpu_readback_argb(dst,16,src,12,2,2));
    const uint8_t want[8]={4,1,2,3,8,5,6,7},want2[8]={14,11,12,13,18,15,16,17};
    assert(!memcmp(dst,want,8) && !memcmp(dst+16,want2,8));
    for(unsigned i=8;i<16;++i)assert(dst[i]==0xCC && dst[i+16]==0xCC);
    uint8_t before[32];memcpy(before,dst,32);
    assert(!rsx_cpu_readback_argb(dst,7,src,12,2,2));
    assert(!rsx_cpu_readback_argb(dst,16,src,7,2,2));
    assert(!rsx_cpu_readback_argb(dst,16,src,12,UINT32_MAX,2));
    assert(!memcmp(before,dst,32));
    puts("PASS opt-in dimensions, linear RGBA8 selection, big-endian ARGB, padded rows and invalid pitches");
}
