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
35. [x] Add strict-violation decode mode to cap maximum allowed violation count for defensive parse limits.
36. [x] Add strict-violation decode mode to cap total parsed key/value fields before rejecting oversized payloads.
37. [x] Add strict-violation decode option to cap maximum decoded violation index before pending-vector resize to harden against sparse-index memory blowups.
38. [x] Add strict-violation decode option to cap maximum decoded `fieldIndex` values for defensive parse bounds in untrusted payloads.
39. [x] Add strict-violation decode option to reject duplicate `strictViolations.count` fields in defensive mode to prevent ambiguous count overrides.
40. [x] Add strict-violation decode option to require `strictViolations.count` appear before indexed entries in defensive mode for earlier bound validation.
41. [x] Add strict-violation decode option to reject zero-padded numeric tokens (`strictViolations.count`, index, and `fieldIndex`) in defensive mode.
42. [x] Add strict-violation decode option to reject explicit plus-prefixed numeric tokens (`+1`) in defensive mode for canonical token enforcement.
43. [x] Add strict-violation decode option to emit dedicated diagnostics for minus-prefixed numeric tokens in defensive canonical-token mode.
44. [x] Add strict-violation decode option to reject non-canonical positive zero (`-0`) tokens in defensive canonical-token mode with dedicated diagnostics.
45. [x] Add strict-violation decode option to reject numeric tokens with leading ASCII whitespace in defensive canonical-token mode.
46. [x] Add strict-violation decode option to reject numeric tokens with trailing ASCII whitespace in defensive canonical-token mode.
47. [x] Add strict-violation decode option to reject reason-name tokens with leading/trailing ASCII whitespace in defensive canonical-token mode.
48. [x] Add strict-violation decode option to reject reason-name tokens with embedded ASCII whitespace in defensive canonical-token mode.
49. [x] Add strict-violation decode option to reject reason-name tokens with non-ASCII whitespace in defensive canonical-token mode.
50. [x] Add strict-violation decode option to reject malformed UTF-8 reason-name tokens in defensive canonical-token mode.
51. [x] Add strict-violation decode option to emit dedicated diagnostics for non-whitespace non-ASCII reason-name tokens in defensive canonical-token mode.
52. [x] Add strict-violation decode option to reject reason-name tokens containing ASCII control characters in defensive canonical-token mode.
53. [x] Add strict-violation decode option to reject reason-name tokens containing non-ASCII Unicode control characters in defensive canonical-token mode.
54. [x] Add strict-violation decode option to reject reason-name tokens containing Unicode noncharacters in defensive canonical-token mode.
55. [x] Add strict-violation decode option to reject reason-name tokens containing lone UTF-16 surrogate code points encoded via CESU-8 in defensive canonical-token mode.
56. [x] Add strict-violation decode option to emit dedicated diagnostics for paired UTF-16 surrogate sequences encoded via CESU-8 in defensive canonical-token mode.
57. [x] Add strict-violation decode option to emit dedicated diagnostics for mixed-order UTF-16 surrogate sequences (low-before-high) encoded via CESU-8 in defensive canonical-token mode.
58. [x] Add strict-violation decode option to emit dedicated diagnostics for same-order UTF-16 surrogate sequences (high-before-high and low-before-low) encoded via CESU-8 in defensive canonical-token mode.
59. [x] Add strict-violation surrogate-token tests covering precedence when lone/paired/mixed-order/same-order CESU-8 rejection modes are all enabled together.
60. [x] Add strict-violation surrogate-token tests verifying canonical reason-name tokens are accepted when all lone/paired/mixed-order/same-order CESU-8 rejection modes are enabled together.
61. [x] Add strict-violation surrogate-token tests verifying `UnknownParseErrorReason` fallback-token acceptance/rejection behavior when all lone/paired/mixed-order/same-order CESU-8 rejection modes are enabled together.
62. [x] Add strict-violation surrogate-token tests verifying malformed UTF-8 reason tokens still report malformed-UTF-8 diagnostics when all lone/paired/mixed-order/same-order CESU-8 rejection modes are enabled.
63. [x] Add strict-violation reason-token tests verifying malformed UTF-8 diagnostics take precedence over `UnknownParseErrorReason` fallback-token rejection when both checks are enabled.
64. [x] Add strict-violation reason-token tests verifying surrogate diagnostics take precedence over malformed-UTF-8 diagnostics when both checks are enabled and both conditions occur in one token.
65. [x] Add strict-violation reason-token tests verifying `rejectUnknownReasonFallbackToken` only rejects exact `UnknownParseErrorReason` matches (not fallback-like prefixes/suffixes).
66. [x] Add strict-violation reason-token tests verifying fallback-like prefix tokens are treated as unknown names (not fallback tokens) when `rejectUnknownReasonFallbackToken` is enabled.
67. [x] Add strict-violation reason-token tests verifying ASCII-whitespace diagnostics take precedence over fallback-token rejection when both checks are enabled.
68. [x] Add strict-violation reason-token tests verifying trailing ASCII-whitespace diagnostics take precedence over fallback-token rejection when both checks are enabled.
69. [x] Add strict-violation reason-token tests verifying non-ASCII-whitespace diagnostics take precedence over fallback-token rejection when both checks are enabled.
70. [x] Add strict-violation reason-token tests verifying leading non-ASCII-whitespace diagnostics take precedence over fallback-token rejection when both checks are enabled.
71. [x] Add strict-violation reason-token tests verifying non-whitespace non-ASCII diagnostics take precedence over fallback-token rejection when both checks are enabled.
