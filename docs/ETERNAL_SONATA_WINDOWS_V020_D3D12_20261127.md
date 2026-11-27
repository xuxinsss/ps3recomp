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

## Performance before the hitch fix

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


## Hitch diagnosis and retained fixes

A diagnostic build timed actual D3DCompile calls and CreateGraphicsPipelineState
separately. At the camera transition, two vertex shader compiles took about
302/298 ms. Many fragment shaders were compiled repeatedly under different
complete PSO state keys: 122 compiler calls contained only 73 distinct HLSL
sources. PSO creation itself was about 0.1-0.8 ms per measured call. Diagnostic
sampling, timers, shader dumps and the stage CSV writer are not in production.

The new DXBC cache retains the original complete PSO keys and generated HLSL.
It reuses bytecode only after matching the exact source, entry point, target,
compile flags and compiler DLL identity. On disk, bounded lengths, exact source,
DXBC magic and payload checksum reject truncated/corrupt/mismatched entries;
failed writes fall back to compiling. Includes/macros bypass the cache.
The in-memory cache is bounded to 128 entries and 32 MiB. Disk writes use an
atomic per-process/per-thread temporary file. It stores DXBC, not driver PSOs.

PS3RECOMP_D3D12_SHADER_CACHE selects a local persistent directory. The local
launcher uses F:\ps3-revise\es-v020-win\dxbc-cache-v2. No game-generated HLSL
or DXBC is committed. Without this variable only the process-memory cache is
used. PS3RECOMP_D3D12_SHADER_CACHE_DISABLE=1 restores the compiler path.
The identity is taken from d3dcompiler_47.dll, not the EXE import thunk, so
rebuilding the game does not invalidate otherwise identical shaders. A cold
cache can still hitch when a shader is encountered for the first time.

Two smaller CPU changes are retained. The per-draw descriptor window remembers
identical null SRVs; every real resource write invalidates that slot. No real
resource pointers or batched-SRV scheme are cached. Plain float vertex fetches
prepare layout once per draw, retain MAIN address resolution per vertex, and
fall back for inline data, nonplain frequency, zero stride and other formats.
Vertex data is never cached across draws. The opt-outs are
RSX_D3D12_NULL_SRV_CACHE_DISABLE and RSX_VERTEX_FLOAT_PLAN_DISABLE (presence).

Tests run the actual Windows compiler and D3D12 WARP, cache memory/disk reuse,
flags/profile/source separation, corrupted/truncated fallback, compiler errors,
null-to-resource-to-null invalidation, and 1,080 bit-exact scalar-vs-prepared
float fetches across 108 layouts including MAIN page remapping/index wrap.
A temporary in-game sampled scalar comparison also checks the optimized float
fetch against the original decoder; this audit is removed from production.

## Sustained frame-rate limit and rejected experiments

The hitch and sustained slowdown have different causes. Once shaders are warm,
the heroine camera still increases submitted draws from roughly 2,400 to
3,500-4,000. Native CPU sampling identifies vertex decoding/upload, texture and
fragment-program hashing, descriptor setup and driver calls; compile misses
have already stopped. This work does not claim to solve the remaining CPU cost.

Diagnostic camera-aligned windows count only nonempty draws and changing guest
flip IDs, not repeated host presents. The first >=3,300-draw frame anchors the
heroine entrance. Each comparison uses 200 intervals starting 10 and 210 frames
after that anchor, with similar draw counts rather than wall-clock alignment.
Cache/null-view baseline: 21.589 / 19.643 FPS. Adding prepared float fetch:
21.983 / 20.416 FPS. The small change should be treated as modest, not a large
speedup; all compared diagnostic builds use the same sampler/stage writer.

Strip deduplication added only about 1% beyond the float path in one run and
was removed. An additional input-mask experiment had no stable gain and was
removed. D3D12_VSYNC=0 measured 21.990 / 20.606 FPS versus the corresponding
strip candidate's 22.185 / 20.628 FPS, so production keeps VSync on. These local
experiments and their diagnostic source snapshots are not production patches.

A warm-cache preliminary no-screenshot run reduced the former 674/799-ms stalls
to a maximum interval of 86.084 ms, with zero intervals above 100 ms in the
35-second gameplay trace. This is cache-warm opening evidence, not a guarantee
against first-use compilation or a whole-game stutter-free claim.


## Final production verification (2026-11-27)

Production EXE SHA256:
25a83a34f14886e42a0e68ed2ffd63e048b3ae690ed1912d646057d3644eda69

The clean Release build removes the temporary sampler call, linker sampling
map, stage CSV writer and scalar audit. Run d3d12-production-final-perf-01 uses
this exact EXE with no screenshots and no native sampling. Existing opt-in FIFO
tracing counts 791 consumed flips / 790 intervals between 55.000 and 89.982 s:
22.583 FPS, median 44.855 ms, P95 58.142 ms, maximum 72.449 ms, zero intervals
above 100 ms, zero trace drops. The earlier flower-correct build measured
22.008 FPS with maximum 799.398 ms; original Release measured 21.282 FPS with
maximum 828.484 ms. Original lacks flowers, so its load is not equivalent.
Wall-clock-aligned averages also advance through the camera at different rates;
use the camera-stage comparison above for sustained-cost interpretation.

This final run has a warm persistent cache. All 75 existing cache files retain
their sizes and modification times across the final rebuild and gameplay run,
confirming reuse across EXE changes. New scenes can still cause first-use
compilation. This is a bounded opening regression, not a 30 FPS claim.

The matching optimized renderer was tested in a separate 150-second visual/
audio/audit run: d3d12-final-float-audio-sequence-01. It captured 160 frames every
six guest flips, including title gem, tree canopy, flower field, heroine entry,
rear view, face close-up and subtitles. Flowers remained complete in the
reviewed later shots. A 1-in-64 sample of eligible attribute fetches compared
11,900,000 vectors bit-for-bit against the original scalar decoder without a
mismatch. Nonzero PCM peaks reached 0.3604; sampled audio-rate records report
zero dropped frames and zero underruns. This diagnostic run is not an FPS test.

All test-run returncode=1 entries accompanied by stopped_own_process=true are
intentional deadline terminations of processes created for the test. Original
hung windows were neither controlled nor terminated. Original Release SHA256
3e0a3afe51a8aae3b36041155245f4f2de8c6f0b103eb97a7695696449bd52cf and the
independent Vulkan EXE SHA256
5f04c142aed162add960901aa4ca255df438df9d1ec80d3bbe69ec2b51286b8a remain unchanged.

## Forest vegetation: respect guest sampler LOD bounds

User gameplay revealed thick black fringes around forest grass, flowers and
leaves. Independent forest-entry captures reproduced them. The legacy D3D12
sampler decoded bias and filtering but discarded TEXTURE_CONTROL0's minimum
and maximum LOD. In this scene, unit 1 is a separate vegetation mask with
control0=0x80000000 and filter=0x01032000: mip filtering is selected but both
guest LOD bounds are zero. Allowing arbitrary mip levels misaligned that mask
with the colour texture. For example, the 512x512 leaf colour atlas instead
uses control0=0x80048000 (levels 0..9) and filter=0x02063F00 (bias -1).

The sampler now decodes both unsigned 8-fractional-bit bounds and clamps an
inverted interval. Non-mipmapped filters use the minimum level. This agrees
with the existing Vulkan sampler's register interpretation. Alpha test,
blending, shader code and textures remain unchanged. Forcing alpha reference
0.5 was only an earlier diagnostic and is NOT part of the fix; it removed
too much foliage. Captured anti-alias control was FFFF0000/FFFF0001, with no
alpha-to-coverage enable, so that hypothesis was rejected too.

Local evidence: es-v020-win/runs/d3d12-field-diagnostic-03/rsxcap/frame_9000
(before) and d3d12-field-diagnostic-04/rsxcap/frame_9000 (after), same static
forest-entry camera. Thick black outlines are visibly removed by the LOD fix.
These captures include diagnostic instrumentation and are not FPS benchmarks.
The standalone sampler regression covers this exact mask/colour pair,
fractional bounds, non-mip filtering and inverted bounds. All legacy command,
point-quad compiler, WARP, shader-cache and bit-exact vertex tests also pass.
