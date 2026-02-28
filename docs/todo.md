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
12. [x] Refactor `optimize_batch` into explicit pipeline stages (scan, binning, cache build, render-tile selection) with clear interfaces.
13. [x] Add skipped-command diagnostics counters (by command type and reason) into `RendererProfile` for runtime observability.
14. [x] Add optimizer-side skip diagnostics reasons so culling/invalid-command drops are split from render-stage skips.
15. [x] Add tile-stream/premerge optimizer diagnostics so malformed tile-command drops are attributed before render dispatch.
16. [x] Add dedicated optimizer skip reasons for tile-local bounds sanitization vs command-data sanitization in tile streams.
17. [x] Add a small `SkippedCommandReason` name formatter for diagnostics output/tests so reason buckets can be inspected without numeric decoding.
18. [x] Add a compact `RendererProfile` diagnostics dump helper that prints non-zero skip buckets with `skippedCommandReasonName` labels.
19. [x] Add an opt-in verbose diagnostics dump helper that also emits non-zero `byType` and `byTypeAndReason` skip matrices.
20. [x] Add a parse-friendly diagnostics format option (single-line key/value) for `RendererProfile` skip dump helpers.
21. [x] Add `RendererProfile` diagnostics parsing helpers that reconstruct structured buckets from key/value dump output.
22. [x] Add parse diagnostics error reporting (field index + reason) for key/value decode failures.
23. [x] Add strict parser consistency checks that can optionally reject payloads where totals conflict with bucket sums.
24. [x] Add an optional diagnostics parse mode that validates `byTypeAndReason` rows/columns against `byType` and `byReason` independently.
25. [x] Add parser options to choose strict section targets (`optimizer`, `renderer`, or both) for consistency/marginal checks.
26. [x] Add parser options to configure strict failure precedence when multiple consistency/marginal violations exist in one payload.
27. [x] Add parse mode to collect and return all strict consistency/marginal violations instead of failing on the first mismatch.
28. [x] Add a helper to format collected strict parse violations into readable/key-value summaries for diagnostics logs.
29. [x] Add parser helpers to decode strict-violation key/value summaries back into structured violation entries.
30. [x] Add parser option to enforce contiguous strict-violation indices in key/value decode (reject sparse index payloads early).
31. [x] Add strict-violation decode mode that normalizes out-of-order contiguous indices instead of rejecting sparse arrival order.
32. [x] Add strict-violation decode option to reject duplicate index entries with conflicting field/reason values.
33. [x] Add strict-violation decode option to fail on duplicate index entries even when duplicate values are identical.
34. [x] Add strict-violation decode option to reject unknown-reason fallback tokens (`UnknownParseErrorReason`) in strict parse mode.
35. Add strict-violation decode mode to cap maximum allowed violation count for defensive parse limits.
