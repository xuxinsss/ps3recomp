# D3D12 CPU surface readback

`PS3RECOMP_D3D12_CPU_READBACK=WIDTHxHEIGHT` opts the legacy D3D12 backend into
copying matching primary **local, linear A8R8G8B8** render targets and their 1:1 GPU copy destinations to guest memory
after their batch executes. It is disabled by default. Dimensions must be between
16 and 2048. Floating-point, swizzled, depth-only and differently sized targets
are excluded. This is a scoped compatibility mechanism, not general GPU/CPU
memory coherence or mid-batch semaphore synchronization.

Eternal Sonata (BLUS30161) renders its save preview into a 320x160 target and then
copies those pixels to PREVIEW0. Without GPU readback, the file contains zeros or
stale VRAM allocation contents. Its Windows launcher uses
`PS3RECOMP_D3D12_CPU_READBACK=320x160`. Existing files are never rewritten by the
backend; the player must save again in the game to replace an old preview.

The backend retains the guest row pitch, converts RGBA GPU bytes to guest ARGB
bytes, restores the GPU resource state and maintains SPU reservation bookkeeping.
Only targets queued by a draw or GPU copy in the submitted batch are selected, once per
offset. Cached targets used only for sampling are excluded. Ordinary forest
frames do not match and perform no readback or GPU wait for this feature. Matching
batches do incur synchronous readback; do not select a continuously rendered
scene target unless that cost is intentional.

Validation: `tools/test_d3d12_cpu_readback.py` checks selection, pixel byte order,
row padding and invalid pitches. A Windows Release game replay must additionally
cover save, overwrite, process restart, load and continued movement, with
`[D3D12] CPU surface readback` absent during the steady-state performance window.
