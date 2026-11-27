# D3D12 depth-feedback snapshot: false contours on moving models

The legacy renderer did not refresh a depth texture's colour snapshot when
its source was still the currently bound DSV (`ds == cur_z`). Eternal Sonata's
forest contour pass samples that depth while writing the same colour target.
The old snapshot no longer describes the current geometry, producing narrow
false lines across the character's dress and the floating plant enemy.

Refresh a dirty sampled depth snapshot even when the source remains the current
DSV. `ds_convert` reads the depth/stencil planes into a separate colour resource,
returns the depth resource to DEPTH_WRITE, and the existing `force_rebind` path
restores guest root signature, heaps, targets, viewport and samplers before the
draw. No guest draw or contour shader is suppressed.

Cache the FP's actual texture-unit mask with its existing bytecode-identified
PSO and copy the mask onto each recorded draw during the prepass. Conditional
fetches are included conservatively. Enabled but unread texture registers are
not dependencies, avoiding unnecessary snapshots of stale bindings. A shader
lookup failure leaves a conservative all-unit mask. Depth/stencil generations
still deduplicate unchanged snapshots; no frame-specific addresses enter the fix.

## Evidence, Windows hardware, 2026-11-27

- `d3d12-lines-pso-baseline10`: the forest's polygon-offset enables and parameters
  were all zero. Missing polygon bias was investigated and excluded.
- `d3d12-lines-pso-isolate12`: same-process controlled comparisons kept camera
  position fixed. Suppressing the contour pass removed the lines; restoring it
  restored them. Refreshing the current depth with the contour pass retained
  removed the lines. This suppression exists only in the diagnostic artifact.
- `d3d12-lines-pso-fixed13`: the production fix, including actual-unit dependency
  masks, preserved the contour pass, shadows and opening white flowers. Later
  animation captures remained clean. Camera position matches the baseline;
  character/enemy poses are not synchronized, so images are not pixel diffs.
- The headless WARP regression executes the production depth-copy shaders on
  a still-bound DSV, copies and rebinds it across five depth/stencil generations,
  and checks 320 packed pixels. Run `python tools/test_d3d12_depth_feedback.py`.
- Existing sampler, command bridge, point-quad HLSL/WARP, pipeline/DXBC cache and
  bit-exact vertex-fetch regressions also pass.

The bounded game regression covers the opening and forest/save-point vicinity.
It does not establish correctness for every title, battle exit or save loading.
Game data, screenshots, caches and diagnostic probes stay local.

Final clean production EXE SHA256: `bd7e788be01ba40dc38d62d036bab6888ed3654b261052670f135c92e2cea8e7`.
A 35-second opening FIFO-consumption trace (screenshots off) recorded
784 flips / 783 intervals, 22.384 FPS,
median 44.999 ms, P95 60.278 ms, maximum
92.649 ms, 0 intervals above 100 ms; zero trace drops.
Previous production measured 22.877 FPS in its corresponding time window;
scene progress differs, so this is not an identical-frame speedup claim.
The fixed forest diagnostic recorded exactly one depth snapshot per game
frame over all 3051 frames from guest flip 3450 through 6500.
