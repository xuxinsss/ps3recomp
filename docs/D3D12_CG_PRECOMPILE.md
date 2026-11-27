# Optional legacy D3D12 shader preparation

`tools/d3d12_precompile_cg.c` prepares the existing exact-source DXBC cache
before gameplay from a **locally supplied** ELF or Cg-containing binary.
It scans bounded revision-6 Cg headers, translates the microcode using this
SDK's own vertex/fragment decompilers, and invokes the same compiler/cache
implementation and flags as the legacy D3D12 renderer. It does not change
guest code, draw ordering or shaders selected at runtime.

Build on Windows with the repository's configured MSVC environment:

```
python tools/build_d3d12_precompile.py path/to/output-directory
python tools/precompile_d3d12.py path/to/output-directory/d3d12-precompile-cg.exe input.elf cache-directory log-directory --jobs 4
python tools/test_d3d12_precompile.py path/to/output-directory/d3d12-precompile-cg.exe
```

The wrapper starts up to eight independent workers (default four). Each owns
its compiler and decompiler state; it terminates only its own children on
interruption. Workers share atomic cache writes. The first preparation takes
time; subsequent runs validate/reuse the cache. Cache source, target, flags,
compiler identity, bytecode hash and contents are checked by the runtime's
existing cache implementation. Truncated/corrupt entries compile again.

Coverage is deliberately bounded. Vertex programs enumerate only branch bits
actually referenced by BRB/CLB, with an eight-bit expansion ceiling. Variants
include ordinary triangles, fixed points and TC4 point sprites used by Eternal
Sonata. Other point-coordinate masks and shader programs outside the supplied
file retain normal runtime compilation. Fragment metadata supplies output
register/depth control and cube sampler units. The 4096-byte fragment decoder
bound matches the current legacy renderer; changing that renderer bound must
also update this utility. This utility does not prepare driver-specific PSOs,
upload textures, or eliminate costs of first-time resource loading.

For the local Eternal Sonata USA ELF, 546 validated programs (186 vertex,
360 fragment) produce 1554 variant requests and 1348 unique cache entries,
with zero compiler failures. All 101 game-shader cache identities previously
observed through the user's forest gameplay are present. The eight small
renderer-built shaders are not Cg programs and still compile during renderer
initialization. No binary, generated shader, raw microcode, cache, game asset,
or environment manifest is included in this repository.

The RPCS3 comparison is conceptual: its Vulkan initialization calls
`preload_shaders()` and loads its shader cache before drawing:
https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/RSX/VK/VKGSRender.cpp
This tool additionally uses the static port's access to embedded program
metadata to prepare some shaders before their first encounter.

## Local forest regression (2026-11-27)

Independent HDD directories, identical renderer/instrumentation, 1280x720,
the same analog movement segments from a new game. `d3d12-field-cold07` keeps
only the 75 opening-cache entries; `d3d12-field-precompiled-06` starts with
the 1348 entries generated from the ELF, without copying learned gameplay
cache entries. Movement starts at different idle flips, so these are matched
route/stage comparisons, not identical animated frames or a steady-FPS claim.

| First encounter | Opening-only cache | ELF-prepared cache |
| --- | ---: | ---: |
| Forest position, maximum game-frame interval | 1315.611 ms | 161.842 ms |
| Forest position, vertex shader request | 1034.167 ms | 0.528 ms |
| Forest position, fragment shader request | 122.988 ms | 0.277 ms |
| Forest position, driver PSO creation | 107.995 ms | 106.715 ms |
| Save-point contact, maximum game-frame interval | 529.353 ms | 125.560 ms |
| Save-point contact, two new vertex shader requests | 307.648 / 349.741 ms | 0.414 / 0.419 ms |
| Save-point contact, new fragment shader request | 58.804 ms | 0.262 ms |

QPC records include only nonempty rendered frames with a new guest flip,
excluding repeated presents. The first route window is event-1 to event+12
seconds; the save contact window is event-1 to event+10. No screenshots or
other compiler jobs run during these windows. Renderer logs/probes are
identical in the two test binaries and are removed from the delivered EXE.
Save-point graphics and the save prompt were reached in the independent test;
actual save/load and battle exit are outside this bounded regression.

The long shader stalls are removed for these observed variants. The remaining
126-162 ms first-use intervals are real: PSO creation still occurs on the
rendering thread. This is not a claim of hitch-free gameplay or 30 FPS.
The warm preparation wrapper completed in 2.039 s (workers 0.125-0.141 s).
The earlier 138.279 s preparation measurement reused a partially populated
experimental cache and must not be reported as an empty-cache startup time.
