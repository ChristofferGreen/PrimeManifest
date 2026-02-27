# TODO

1. [x] Add a shared `AnalyzedCommand` IR pass to normalize bounds, clip, alpha, and tile span before optimize/render.
2. [x] Replace duplicated primitive-bound logic in `src/renderer/Optimizer2D.cpp` and `src/renderer/Renderer2D.cpp` with shared helpers.
3. [x] Implement strict `RenderBatch` validation mode that reports invariant violations (store-size mismatches, bad indices) before rendering.
4. [x] Add typed append/build APIs for rect/circle/line/image/pixel (not just text) and mark direct raw-store writes as advanced/unsafe.
5. [x] Add cross-path equivalence tests: same scene output for `tileStream on/off`, `autoTileStream on/off`, and premerged/manual streams.
6. [x] Add malformed-batch robustness tests to verify deterministic behavior (fail-fast in strict mode, safe skip in permissive mode).
7. [x] Refactor `RenderOptimizedImpl` phase 1 into a tile command scheduler/orchestrator layer plus extracted `SetPixel`/`SetPixelA` kernels.
8. [x] Refactor `RenderOptimizedImpl` phase 2 by extracting `Line`/`Image` kernels and routing them through scheduler-based dispatch.
9. [x] Complete `RenderOptimizedImpl` `Circle` kernel extraction and route `CommandType::Circle` through kernel dispatch.
10. [x] Complete `RenderOptimizedImpl` `Rect` kernel extraction and route `CommandType::Rect` through kernel dispatch.
11. [x] Complete `RenderOptimizedImpl` `Text` kernel extraction and finish dispatch cleanup.
12. Refactor `optimize_batch` into explicit pipeline stages (scan, binning, cache build, render-tile selection) with clear interfaces.
13. Add skipped-command diagnostics counters (by command type and reason) into `RendererProfile` for runtime observability.
