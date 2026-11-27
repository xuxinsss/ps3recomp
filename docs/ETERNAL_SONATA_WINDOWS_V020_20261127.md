# Eternal Sonata: Windows Vulkan on v0.20.0 (2026-11-27)

## Reproducible source baseline

This branch starts at v0.20.0, d81c393ee72db80ef89aa4464b23c466edda44b0.
It records the four verified Linux port patches (gem U wrap, FIFO 3D routing,
Vulkan point sprites, fused guest row copy) from port revision
4700c3bf43ab29c18e6ac293f7bf5dd988d35ae8, then minimal Windows adaptations.
The original SDK working tree and original Windows executable were preserved.
No game-generated sources, binaries, game data, firmware, saves or captures are
part of this branch. No upstream PR is implied by this record.

## Changes and boundaries

* Windows project template selects Vulkan when PS3RECOMP_RSX_VULKAN is enabled.
  Its default without that option remains D3D12. Vulkan's compiled present-thread
  hooks now have a QPC clock on Windows; this does not enable an async thread.
* Point sprites enable supported largePoints, emit PointSize, replace only the
  selected TC coordinates, handle non-square sample grids, and lower glslang's
  reserved PointCoord input to its SPIR-V built-in. The observed foreground
  flower POINTS use TC4. The rule is not hardcoded to game atlas addresses.
* FIFO mirroring excludes 2D methods from the 3D register file. Gem wrap is an
  opt-in material rule, enabled by RSX_WRAP_U_FP=FE01,1259C00 for this title.
* SPURS static/dynamic event port allocations are scoped to the owning instance.
  This game's second instance also requests static port 1. The global table
  returned BUSY and its audio initialization stopped before creating Multistream.
  With owner-scoped allocation the task starts and the audio ring becomes nonzero.
  The legacy owner-unspecified spurs_port_queue getter is retained; this is not
  a claim that every game's cross-instance event delivery is now validated.
* Runtime-loaded audio code still requires SPU_INTERP_UNLIFTED=1 for this title.
  The corrected run produced nonzero mixed PCM (observed peak 0.1398); WASAPI ran
  about 187.5 blocks/s without measured dropped frames or underruns in the short
  audio check. This is not a full audio synchronization/long-duration validation.

## Build and launch contract

Native Windows x64, clang-cl 20.1.8 + installed MSVC 14.44 headers, Ninja/CMake.
Vulkan-Headers SDK 1.4.328.1 and glslang 15.1.0 were privately built, without a
system SDK installation. FFmpeg runtime headers were from
73de9781b6319e0051249b73e7d9b4c09451edf2 (codec 63, util 61), matching the local DLLs.
The small generated avconfig.h sets little-endian and fast-unaligned on x86-64.

Regenerate PPU and SPU using this exact SDK and the port's prepare.py and
prepare_spu.py, in a new output tree. Do not reuse old Windows PPU/SPU output.
Build templates/project with PS3RECOMP_RSX_VULKAN=ON, PORT_ENABLE_SPU=ON,
PS3RECOMP_FFMPEG_RUNTIME=ON, and explicit RECOMP_DIR/SPU_DIR/dependency prefixes.

Launch with private HDD0/HDD1/gamedata, the game's VFS root, firmware root,
PS3RECOMP_VK_WINDOW=1, PS3RECOMP_VK_GUEST_PROGRAMS=1, VK_DRAW_BATCH=1,
VK_DRAW_BATCH_MAX=512, SPU_INTERP_UNLIFTED=1, and the gem rule above.
The measured baseline keeps direct present and async present disabled. No
AVX2/direct-LS/readback coalescing/barrier dedup/GPU-backing or old job lookahead
experiments were added. Fused row copy retains every guest store and readback.

## Validation

Windows CPU point-sprite assertions and real production glslang translation
passed: PointSize/PointCoord built-ins, ordinary varying isolation, selected TC,
non-square grids, capacity failure and idempotence. A real FIFO fixture passed
2D/3D collision isolation, subchannel 1/driver routing and USER_CMD callbacks;
its Windows-only display replay linker stub does not affect production code.

Run tools/test_spurs_owner_ports.py in a compiler environment (CC can select
clang-cl). It extracts the actual production port-table section and verifies
two owners on static port 1, same-owner duplicate rejection without insertion,
per-owner dynamic port 16, detach and owner-only cancellation. This passed on
Windows. Its non-Windows runner is provided but was not executed in this session.

## Local performance evidence and limitations

Machine: Intel i5-11400, NVIDIA RTX 3060, 1280x720, native Windows.
Original Release SHA256:
3e0a3afe51a8aae3b36041155245f4f2de8c6f0b103eb97a7695696449bd52cf.
Corrected no-probe executable SHA256:
2d438350c7baade72c442de303a799460912c4acbf4c523cc38b1c8e9002b7e5.

An audible Vulkan diagnostic build measured 15.466 FPS, median 64.463 ms,
p95 68.191 ms, on 81 distinct flips / 80 intervals, after NEW GAME input,
with guest_draws >=1000 (median 1478). Zero-draw presents were excluded.
An unmodified original Release run measured 21.766 FIFO flips/s, median
46.188 ms, p95 57.332 ms on 81 successive consumed guest flips / 80 intervals
at t=55.003..58.679 s, after scripted NEW GAME at 35 s. Its available built-in
trace does not carry a per-flip draw count, so its criterion is weaker than the
new engine's. Visual capture in a separate run confirmed the opening scene.
The old PAD_FILE input attempt remained at the title and was excluded; the
historically supported PAD_SCRIPT path entered NEW GAME correctly.

These are nearby opening views, not lockstep cameras or equal rendered loads:
the original lacks the foreground flower field and has strips; the Vulkan
version restores it. Screenshots and performance runs are separate. Shader
startup, scheduling and different diagnostic overhead limit causal comparisons.
No 30 FPS claim, full-game claim, combat/Load regression or upstream publication
is made. Raw captures and logs stay private and are not included here.

Final no-probe, audible 145-second Windows regression completed: title gem,
foreground flower field, canopy contours, strip-free foreground and subtitles
were captured. Audio remained nonzero through the ending sample, with zero
reported dropped frames/underruns in the sampled rate reports. Its private
HDD/save roots were separate. The test process was stopped at the time limit.
