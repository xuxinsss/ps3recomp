Review aid for SPURS owner routing and PR #848

Reviewed upstream master: e85647a33ce5f9391788f0e3f52ab395a7fe6a89
Reviewed PR #848: 98f536f355ee2a430861cffd4659c8f503d3c281
Standalone owner candidate: faf8584f88918f4793620c4dda33b83db8ea964e

owner-after-848.patch is the incremental tree change after applying the exact
reviewed #848 diff to master. It reuses the explicit taskset/task-id API rather
than adding a second implementation. Applying it produced the same 23 changed
files as the standalone owner candidate; actual asynchronous owner capture and
18 real-LV2-queue routing regressions passed on that reconstructed tree.

Deliberate behavior difference: #848 preserves global task identity fallback
in the generic async wrapper; this increment makes the generic wrapper unowned
with zero taskset/task id. SPURS tasks use the explicit dispatch entry. Worker
paths use their captured identity, including the CRI and policy-probe paths.

This patch is for review against those exact snapshots, not an instruction to
apply both full proposals. If #848 or master changes, reconcile against the new
version and validate the new final SHA. No #848 commit was adopted into the
standalone owner or lifecycle proposal history.

This branch only publishes the review artifact. Runtime implementation and CI
remain on the three proposal branches; this documentation commit is not an
additional runtime candidate. Developed with Codex assistance.
