# Eternal Sonata Windows SDK preservation — 2026-11-23

This branch preserves the uncommitted SDK source used by the local Eternal Sonata port work. It is a migration checkpoint for the owner's fork, based on `3c3e8827bf02b4480538dfe5f898e0b7debb1b30` (`spurs-main`). Commits were reconstructed on 2026-11-23 from the existing patches and current local source; they are not recovered historical commits.

## Source provenance and commit groups

| Commit | Source | Scope |
| --- | --- | --- |
| a5d3d08 | runtime.patch | SPURS queue ownership and opt-in append-chain continuation |
| 2c3ac06 | runtime.patch | Seven RSX files: existing Windows rendering/texture/inline/index/FIFO compatibility snapshot and reuse paths |
| e1af93d | runtime.patch | Optional native lifted-SPU compiler settings |
| 4f3874d | spurs-job-direct-ls.patch | Stage directly into the initialized job context LS |
| 2e0d5bb | rsx-vertex-active.patch | Active attribute list and shared enabled-array fetch helper |
| 0cd0484 | Remaining local diff | Watched-EA PUT source diagnostic in spu_dma.h |

`runtime.patch` applies to the base; the direct-LS and active-vertex patches apply on top. After reconstruction, all 12 source files with actual Git diffs have exactly the same Git blob IDs as the original working tree. The previously reported 14 dirty status entries included `runtime/spu/spu_channels.c` and `runtime/syscalls/sys_timer.c`; both were verified byte-for-byte identical to the original HEAD, so neither receives a fabricated change.

This records existing code without fixing or revalidating it. In particular, the opening white flowers and aliasing remain unresolved. The render snapshot combines several changes whose original edit order is not recoverable from Git. The diagnostic header remains a separate commit so a later clean baseline can omit it deliberately.

## Validation and boundaries

- Verified source-blob equivalence against the original working tree; no new compile, game execution or performance test was run.
- Original `spurs-main` HEAD and working files remain untouched. Commits were created in a separate F-drive worktree.
- No new rendering fix, performance conclusion or Steam Deck/Linux/Vulkan compatibility claim is made.
- The known Windows comparison executable remains SHA-256 `3E0A3AFE51A8AAE3B36041155245F4F2DE8C6F0B103EB97A7695696449BD52CF`. Source preservation does not establish a reproducible rebuild of that binary.
- Three-job lookahead, experimental LS double buffering, batched SRV patches and detailed performance probes remain in the separate experimental worktrees and local evidence directories. This branch does not import them from those worktrees.
- Game data, ELF, lifted PPU/SPU game code, firmware, saves, binaries and captures are not included. Copy these separately when migrating.
- Upstream PRs remain paused. Backup commits use `[skip ci]`; no workflow was manually triggered.

## Local patch fingerprints

- `runtime.patch` SHA-256 `f46f5ff2d829782d3c5a19f55442a1a896e39c73b59ed236a9bf9524157e399b`
- `spurs-job-direct-ls.patch` SHA-256 `7138f341a7aeab8612df7ff039458e9fae5d9dc564e015b1f377910e471e0ce4`
- `rsx-vertex-active.patch` SHA-256 `34053bcff596044473d5c5f6cf845a3d2ebfa325f0e68e65ee7bfe958e094ff8`

## Preserved source blobs

- `libs/spurs/cellSpurs.c`: `f3128816b67297e39a8481bab018a0f927614fd0`
- `libs/video/cellGcmSys.c`: `5702b15318ddabe481eec02e5cc61980a000a701`
- `libs/video/rsx_commands.c`: `5e9ef9d50754d03d0c95b51c8fd9f7653c8d6087`
- `libs/video/rsx_commands.h`: `341cfb1a05b5883ca630d3055d7947c25d754518`
- `libs/video/rsx_d3d12_backend.c`: `55554549edb9d8d50fcf46a49015bb4e51de6858`
- `libs/video/rsx_fp_decompiler.c`: `7d41cd4168b5c61b7afb6f012672dc731727f18f`
- `libs/video/rsx_fp_decompiler.h`: `820831e5c47049d0069e695d0bdce4b288b6b26d`
- `libs/video/rsx_vertex_fetch.c`: `32c1804fe4ea578ed7f1cf5dd388e2964d6ea8a1`
- `libs/video/rsx_vertex_fetch.h`: `fdd45011f9dda69a7604144765ce0fc6c634deee`
- `runtime/spu/spu_dma.h`: `daa72820af9dee6962902189a6190b5511340952`
- `runtime/spu/spurs_job.c`: `74462f2df1dd8691d0b535d0796b81cae9f3a7b6`
- `templates/project/CMakeLists.txt`: `63ea0d52eef66893eee6b835c83f469efd43c35a`
