# Complete legacy D3D12 fragment programs

The forest-entry bridge in Eternal Sonata used a 4304-byte fragment program
(206 instructions). The legacy D3D12 backend scanned only 4096 bytes for its
length, code hash, texture usage, constants and generated HLSL. The missing END
made the length scanner return zero, which the backend replaced with 64 bytes;
the decompiler still emitted the truncated prefix. The bridge face consequently
displayed flat green/brown regions and unrelated-looking intermediate shading.

The backend now requires a complete program. Ordinary programs retain the
existing 4 KiB scan. When that scan cannot find END, the backend queries the
committed host region and extends the scan up to 64 KiB, matching the live draw
engine's ceiling. This preserves the existing readable-input contract for the
initial scan and bounds the newly extended read. Length, hash, texture mask,
decompilation and inline constants all use the complete byte range. The scanner
also rejects an END instruction whose inline constant is truncated.

The D3D12 decompiler rejects incomplete programs instead of emitting their
prefix. Offline Cg preparation passes the complete, bounds-validated microcode
range to that same decompiler. The exact-source DXBC cache gives the corrected
shader a different identity; no cache deletion or game-specific shader rewrite
is necessary. This does not pre-create every possible runtime pipeline state.

An intermediate candidate queried Windows memory regions on every draw. Local
regression exposed a severe slowdown, so that candidate was rejected. The query
belongs only on the extended-scan path; it must not be reintroduced for ordinary
short programs as a routine per-draw operation.

## Validation

- Synthetic program with texture sampling, inline constants and END beyond
  4096 bytes: full length, code hash, texture mask, constant extraction and real
  Windows HLSL compilation pass. Truncated inputs are rejected.
- The same synthetic program passes offline Cg preparation and exact-source
  warm-cache reuse (`tools/test_d3d12_long_fp.py`). No game data is in the test.
- Existing fragment decompiler regression: 121 passed, zero failed (Windows
  test build supplies temporary POSIX environment-function adapters).
- Legacy command, sampler, vertex-fetch, point-quad HLSL, WARP point pixels,
  DXBC/pipeline cache and current-depth WARP regressions pass.
- Local ELF preparation: 546 programs, 1554 prepared variants, zero failures
  and zero skipped programs with four workers. This is a warm-cache result,
  not a cold-compilation benchmark or a promise of zero first-use stalls.
- Local D3D12 regression captured 60 images at requested guest flips
  1350 through 3120, stride 30, with nonzero draw activity. Opening flowers,
  subtitles, the forest introduction and the bridge were inspected. At the
  matching bridge camera (requested flip 2520), the metal rim and rivets render
  correctly; consecutive bridge cameras also retain the corrected material.
  The test-only executable hid its window but used the same renderer code.
  Production retains its normal visible window and has no color diagnostics.

This is a bounded visual regression, not a frame-rate benchmark or complete
game validation. GPU readback screenshots introduce overhead and must not be
used to claim gameplay FPS improvements.

Local game captures and extracted shader data remain outside the repository.
The correction does not alter game assets, guest geometry or gameplay logic.
