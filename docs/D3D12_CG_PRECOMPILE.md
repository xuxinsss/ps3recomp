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
