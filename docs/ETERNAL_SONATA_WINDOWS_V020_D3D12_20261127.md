# Eternal Sonata: v0.20.0 runtime with preserved Windows Release D3D12

The owner requested the older Release rendering path on v0.20.0, with smoothness
as the main concern. This branch builds that comparison independently; it does
not replace the Vulkan delivery or the original Windows Release.

## Scope and provenance

Base: exact SDK v0.20.0 d81c393ee72db80ef89aa4464b23c466edda44b0, through the
previous es/windows-v020-vulkan-20261127 correctness branch at 75ee7a2.
The matching v0.20.0 PPU/SPU generation is reused as read-only build input.

Simply selecting the stock v0.20.0 D3D12 backend failed visual validation:
malformed title gem and black opening despite FIFO flips continuing. Its EXE
SHA256 b90442d3b3fbf34512d2aa51854ab438f79e6140902f94eb6cd48f52543340bb
and the rejected run d3d12-v020-perf-01 are preserved locally. Its 22.36 flips/s
is NOT a valid playable-game performance comparison.

The compatibility source comes from the saved v020-runtime-smoke.patch, excluding
its SPURS section because the reviewed per-instance port ownership fix is already
present. The saved rsx-vertex-active.patch follows it. The resulting D3D12 backend
Git blob is 55554549edb9d8d50fcf46a49015bb4e51de6858, exactly the preserved original
Windows Release source blob documented in the Windows backup.

This is explicitly a legacy D3D12 compatibility snapshot on the v0.20.0 runtime,
not a minimal forward port of all upstream v0.20.0 renderer improvements. The
backend snapshot replaces overlapping upstream D3D12 changes. It restores the
original render-record capacity, inline arrays, restart-index handling, fragment
constants/alpha state, texture handling, GPU surface copy ordering and active
attribute fetch path. Guest memory copies remain in addition to GPU copies.

The original Release's direct job LS staging and opt-in native lifted-SPU flags
are separate commits. Direct staging is the preserved Windows production code,
not the later experimental LS double buffer. The native flags are actually
present in Ninja: -O2 -march=native -ffp-contract=off. This EXE targets this PC;
rebuild without the native option for a different host CPU.

No three-job lookahead, batched SRV experiments, GPU-only guest backing, async
present, or frame-probe patch is introduced. The Vulkan binaries and source tree
remain unchanged. Linux-only point-sprite fixes are not a D3D12 flower fix.

## Build and launch

Local independent checkout: F:\ps3-revise\es-v020-win\sdk-d3d12
Build script: build-d3d12-release.cmd; build cache: build-d3d12-release.
Within the SDK template CMake project use Release, clang-cl, PORT_ENABLE_SPU=ON,
PS3RECOMP_NATIVE_SPU=ON, PS3RECOMP_RSX_VULKAN=OFF, and the existing FFmpeg runtime
headers. RECOMP_DIR and SPU_DIR point to the port's local/v020/ppu and spu.

Visible manual launcher: F:\ps3-revise\es-v020-win\run-d3d12.cmd
It uses release-d3d12/MyGameRecomp.exe and a new private play-d3d12 session each
time. Read-only game/ELF/firmware inputs remain on F. Z=CROSS, X=CIRCLE,
Enter=START, arrows=D-pad. Settings match the original launcher:
VP_NOFIXPROJ=1, RSX_FULL_TEXTURE_HASH=1, RSX_VERTEX_CAPACITY=1572864,
SPURS_APPEND_CHAIN=1, SPU_INTERP_UNLIFTED=1, RSX_WRAP_U_FP=FE01,1259C00.
Private FFmpeg DLL paths and audio remain enabled.

## White-flower correctness checkpoint

Non-indexed POINTS are expanded into six vertices after the original guest VP.
POINT_SIZE and POINT_SPRITE_CONTROL are recorded; the sprite mask selects only
the replaced texture coordinates (TC4 here). The original FP and alpha state
are unchanged. Independent viewport axes map pixel size on the legacy
single-sample target; point size is clamped to 1..64 like the Vulkan baseline.

The first candidate incorrectly subtracted StartVertexLocation from SV_VertexID.
D3D12 already supplies draw-relative IDs, so this rotated/degenerately covered
the two triangles as preceding geometry changed offsets. The owner caught
partial flowers in later shots. The corrected shader uses SV_VertexID % 6.

The three atlases 0A541500/0A542080/0A542C00 contain 137/171/160 points,
468 total. Diagnostic logs confirmed all submitted even when the old candidate
rendered them incompletely. These diagnostics are excluded from the final EXE.

Validation: command/FIFO/texture-offset tests, actual HLSL compilation, and real
D3D12 WARP pixel readback pass. Seven draw offsets (0,2,5,11,18,64031,102744)
produce identical pixels. Reintroducing the bad subtraction only in a temporary
negative-control header fails at offset 2. tools/test_d3d12_legacy_commands.py
runs the command, shader and GPU regressions on Windows.

A bounded 130-second opening run captured 160 frames every six FIFO flips.
The reviewed sequence maintains the flower field across wide shots, heroine
entrance, rear view and face close-up. This is not a full-game guarantee;
indexed points, combat and Load-black-screen remain outside validation scope.
Previous audio validation produced nonzero PCM with no sampled device drops.

Correctness checkpoint EXE SHA256:
d10e9cebd0decd8e7aaf1caace679a150c464156c8d5b7ffda95e9000481af5f

## Performance checkpoint and outstanding hitch

No-screenshot run d3d12-flowers-final-perf-01: 770 consumed FIFO flips between
55.045 and 89.987 seconds, 22.008 flips/s; median 44.156 ms, P95 58.813 ms.
First 80 intervals: 22.662 flips/s. Trace drops: zero. These are guest FIFO
flips, not repeat host presents; this legacy trace has no per-flip draw count.
A separate visual run confirms the opening scene, not the title menu.

Original Release at the same scripted time window: 21.282 flips/s, median
45.177 ms, P95 58.083 ms. It lacks the flower field and is not equivalent
rendering load. Previous Vulkan measurements around 15-17 FPS also used
different diagnostics/camera progression, so no exact speedup is claimed.

Two adjacent long intervals at 75.1835 seconds last 674.649 and 799.398 ms.
Original Release has the same pattern (683.232 and 828.484 ms at 76.2863).
Both report 18 PSO cache misses, taking 0.75/0.77 seconds in the PSO path.
After the hitch, misses are zero but SRV preparation and FP hashing increase.
The final checkpoint averages 24.827 flips/s in seconds 65-75, then 20.187
in seconds 79-89 (original: 23.518 then 20.560). The owner notices both the
stall and sustained slowdown. They are not solved by the flower correction.
Further performance investigation must preserve this correctness checkpoint.
