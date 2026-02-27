# TODO

1. [x] Add a shared `AnalyzedCommand` IR pass to normalize bounds, clip, alpha, and tile span before optimize/render.
2. Replace duplicated primitive-bound logic in `src/renderer/Optimizer2D.cpp` and `src/renderer/Renderer2D.cpp` with shared helpers.
3. Implement strict `RenderBatch` validation mode that reports invariant violations (store-size mismatches, bad indices) before rendering.
4. Add typed append/build APIs for rect/circle/line/image/pixel (not just text) and mark direct raw-store writes as advanced/unsafe.
5. Add cross-path equivalence tests: same scene output for `tileStream on/off`, `autoTileStream on/off`, and premerged/manual streams.
6. Add malformed-batch robustness tests to verify deterministic behavior (fail-fast in strict mode, safe skip in permissive mode).
7. Refactor `RenderOptimizedImpl` into per-primitive kernels plus a small tile scheduler/orchestrator layer.
8. Refactor `optimize_batch` into explicit pipeline stages (scan, binning, cache build, render-tile selection) with clear interfaces.
9. Add skipped-command diagnostics counters (by command type and reason) into `RendererProfile` for runtime observability.
