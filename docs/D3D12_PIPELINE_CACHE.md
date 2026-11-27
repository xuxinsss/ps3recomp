# Optional D3D12 driver pipeline cache

DXBC preparation removes HLSL compilation stalls but does not eliminate
`CreateGraphicsPipelineState`. Eternal Sonata's first forest-position pipeline
took about 108 ms even with prepared DXBC, and one opening frame created 18
new pipelines in about 0.21 seconds. This cost reproduced in two production
runs, with maximum FIFO flip intervals of 244.448 and 244.236 ms.

Set `PS3RECOMP_D3D12_PIPELINE_CACHE` to a local writable directory to retain
`ID3D12PipelineState::GetCachedBlob` results for guest vertex/fragment PSOs.
The existing in-memory cache and full draw state remain authoritative.

Disk identity compares the complete normalized graphics description, all
shader bytecode, serialized root signature, input layout values and semantic
strings. Host addresses and cached-blob pointers are excluded. Stream output
is not cached. A filename hash is only a lookup index: the stored identity
must also match byte-for-byte. Files have bounded lengths, a content hash and
atomic replacement; corrupt or truncated files do not reach the driver.

The driver always receives the original full description. A cached blob is
only a compilation hint. Any driver rejection retries without it and replaces
the stale cache after successful creation. Failure to read/write a cache or
retrieve a driver blob leaves ordinary creation available. No draw is skipped,
no shader placeholder is rendered, and no graphics state is relaxed.

Driver blobs are specific to the machine, adapter and driver. They are local
artifacts and must not be distributed as portable shader binaries. See the
Microsoft API contract:
https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_cached_pipeline_state

The standalone regression substitutes the driver boundary to verify exact
identity despite pointer relocation, shader/root/blend-state changes, rejected
cached blobs, corrupted content and oversized file fallback. Hardware replay
is additionally required to establish performance and rendering behavior.

An entirely new pipeline still needs its first creation. This cache reuses
observed combinations across launches; ELF shader preparation alone cannot
predict every future combination of vertex shader, fragment shader and state.

Hardware replay on the same Windows adapter (2026-11-27) retained 141 blobs,
6,623,154 bytes. A fresh process reused all 141 files without rewriting any.
First forest-path PSO creation fell from 107.995 ms to 7.671 ms; the first
save-point PSOs took 3.456 and 5.357 ms instead of 41.112 and 68.685 ms.
Maximum nonempty-draw/new-guest-flip intervals on the same controlled route:

| Window | Cold shaders | Prepared DXBC | Prepared DXBC + retained PSO |
| --- | ---: | ---: | ---: |
| First forest path | 1315.611 ms | 161.842 ms | 71.784 ms |
| First save contact | 529.353 ms | 125.560 ms | 68.402 ms |

The final windows contained 278/228 intervals, medians 44.717/45.797 ms,
and zero intervals above 100 ms. Screenshots taken after measurement retained
opening white flowers and correct forest foliage. These are repeatable route
windows, not identical animated frames or claims about all future pipelines.
The cache is learned from a previous run; unlike embedded Cg preparation,
it does not predict unseen shader/state combinations.
