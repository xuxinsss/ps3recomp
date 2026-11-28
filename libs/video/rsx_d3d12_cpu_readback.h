/* Opt-in local, linear A8R8G8B8 surface readback. No game-specific dimensions. */
#ifndef RSX_D3D12_CPU_READBACK_H
#define RSX_D3D12_CPU_READBACK_H
#include <stdint.h>
#include <stdio.h>
typedef struct { uint32_t width, height; } rsx_cpu_readback_size;
static inline rsx_cpu_readback_size rsx_cpu_readback_parse(const char* text)
{
    rsx_cpu_readback_size size = {0, 0};
    unsigned w, h; char extra;
    if (text && sscanf(text, "%ux%u%c", &w, &h, &extra) == 2 &&
        w >= 16 && w <= 2048 && h >= 16 && h <= 2048) {
        size.width = w; size.height = h;
    }
    return size;
}
static inline int rsx_cpu_readback_matches(rsx_cpu_readback_size size,
    uint32_t w, uint32_t h, uint32_t format, uint32_t pitch)
{
    return size.width && w == size.width && h == size.height &&
        (format & 31u) == 8u && ((format >> 8) & 15u) == 1u &&
        w <= UINT32_MAX / 4u && pitch >= w * 4u;
}
/* GPU RGBA bytes -> guest big-endian A8R8G8B8. Preserve row padding. */
static inline int rsx_cpu_readback_argb(uint8_t* dst, uint32_t dst_pitch,
    const uint8_t* src, uint32_t src_pitch, uint32_t w, uint32_t h)
{
    if (!dst || !src || !w || !h || w > UINT32_MAX / 4u ||
        dst_pitch < w * 4u || src_pitch < w * 4u) return 0;
    for (uint32_t y = 0; y < h; ++y) {
        uint8_t* out = dst + (size_t)y * dst_pitch;
        const uint8_t* in = src + (size_t)y * src_pitch;
        for (uint32_t x = 0; x < w; ++x) {
            out[x*4] = in[x*4+3]; out[x*4+1] = in[x*4];
            out[x*4+2] = in[x*4+1]; out[x*4+3] = in[x*4+2];
        }
    }
    return 1;
}
#endif
