# Renderer 2D Optimization Log

Date: 2026-02-14

## Protocol
- Build: Release only.
- Benchmark: `./renderer_bench --frames 600`.
- Runs: 20 per measurement.
- Report: mean FPS plus dispersion (median, min, max, stdev).

## Current Aggregate
- Mean: 294.07 FPS
- Median: 307.48 FPS
- Min: 189.39 FPS
- Max: 337.49 FPS
- Stdev: 37.40 FPS
- Build: Release
- Runs: 20
- Frames: 600
- Commit: working tree (uncommitted)

## Measurements
| Date | Build | Runs | Frames | Mean FPS | Median | Min | Max | Stdev | Commit | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-02-14 | Release | 20 | 600 | 384.66 | 391.71 | 337.18 | 409.73 | 22.48 | Working tree | Current aggregate (palette-only command colors, int16 geometry, tileSize=32). |
| 2026-02-14 | Release | 20 | 600 | 294.07 | 307.48 | 189.39 | 337.49 | 37.40 | Working tree | Current aggregate (palette-only command colors, int16 geometry, tileSize=32). |
| 2026-02-14 | Release | 20 | 600 | 512.51 | 511.24 | 401.41 | 591.72 | 50.39 | Working tree | Pre-merged per-tile command list (tile stream pre-merge) + front-to-back + opaque-count early-out + in-place tile buffer. |
| 2026-02-14 | Release | 20 | 600 | 358.65 | 360.61 | 289.31 | 378.50 | 19.56 | Working tree | Prior aggregate (palette-only command colors, int16 geometry, tileSize=32). |
| 2026-02-14 | Release | 20 | 600 | 573.13 | 573.56 | 537.40 | 614.24 | 25.73 | Working tree | Pre-merged per-tile command list (tile stream pre-merge) + front-to-back + opaque-count early-out + in-place tile buffer. |
| 2026-02-14 | Release | 20 | 600 | 341.19 | 346.28 | 307.91 | 358.25 | 13.40 | Working tree | Prior aggregate (palette-only command colors, int16 geometry, tileSize=32). |
| 2026-02-14 | Release | 20 | 600 | 358.37 | 358.05 | 302.32 | 411.67 | 32.41 | Working tree | Hierarchical tile stream (L0/L1/L2) + front-to-back tile buffer + opaque-count early-out + in-place tile buffer (no copy). |
| 2026-02-14 | Release | 20 | 600 | 370.26 | 380.50 | 322.01 | 389.73 | 22.03 | Working tree | Prior aggregate (palette-only command colors, int16 geometry, tileSize=32). |
| 2026-02-14 | Release | 20 | 600 | 371.37 | 377.06 | 272.70 | 414.78 | 42.03 | Working tree | Hierarchical tile stream (L0/L1/L2) + front-to-back tile buffer (alpha cutoff 0.98) + tile opaque-count early-out. |
| 2026-02-14 | Release | 20 | 600 | 390.10 | 392.99 | 354.86 | 396.39 | 10.66 | Working tree | Prior aggregate (palette-only command colors, int16 geometry, tileSize=32). |
| 2026-02-14 | Release | 20 | 600 | 169.33 | 169.92 | 163.64 | 170.40 | 1.60 | Working tree | Hierarchical tile stream (L0/L1/L2) + front-to-back tile buffer (alpha cutoff 0.98). |
| 2026-02-14 | Release | 20 | 600 | 307.06 | 310.60 | 266.73 | 332.29 | 18.63 | Working tree | Tile-local command stream (hierarchical traversal, uint8 coords). |
| 2026-02-14 | Release | 20 | 600 | 360.40 | 370.81 | 296.29 | 383.49 | 22.94 | Working tree | Tile-local command stream (flat traversal). |
| 2026-02-14 | Release | 20 | 600 | 362.76 | 365.04 | 332.64 | 384.53 | 15.27 | Working tree | Prior aggregate (palette-only command colors, int16 geometry, tileSize=32). |
| 2026-02-14 | Release | 20 | 600 | 368.89 | 372.51 | 322.04 | 386.42 | 15.66 | Working tree | Prior aggregate (palette optional, tileSize=32). |
| 2026-02-14 | Release | 20 | 600 | 384.39 | 391.63 | 351.49 | 399.05 | 14.76 | Working tree | Indexed palette colors (rainbow + grayscale). |
| 2026-02-14 | Release | 20 | 600 | 407.58 | 416.08 | 378.38 | 420.48 | 14.73 | Working tree | Previous aggregate (pre-palette experiment). |
| 2026-02-14 | Release | 20 | 600 | 160.46 | 158.62 | 125.85 | 203.87 | 16.02 | Working tree | 2x renderer threads vs cores (regression). |
| 2026-02-14 | Release | 20 | 600 | 342.34 | 332.16 | 298.48 | 385.59 | 29.08 | Working tree | Renderer threads = 0.5x hardware concurrency (5 vs base 11). |
| 2026-02-14 | Release | 20 | 600 | 395.39 | 401.02 | 329.85 | 422.95 | 27.76 | Working tree | Renderer threads = base-1 (10 vs base 11). |
| 2026-02-14 | Release | 20 | 600 | 328.01 | 333.41 | 246.67 | 384.76 | 38.08 | Working tree | Renderer threads = base+1 (12 vs base 11). |
| 2026-02-14 | Release | 20 | 600 | 205.84 | 204.13 | 167.38 | 249.89 | 20.13 | Working tree | Renderer threads = 1.5x (16 vs base 11). |
| 2026-02-14 | Release | 20 | 600 | 395.16 | 396.04 | 352.27 | 428.41 | 19.83 | Working tree | Fixed-point glyph positioning retry (regression). |
| 2026-02-14 | Release | 20 | 600 | 340.55 | 340.90 | 304.22 | 371.77 | 17.68 | Working tree | Precompute rect center/rotation/half-extents retry (regression). |
| 2026-02-14 | Release | 20 | 600 | 383.38 | 404.06 | 242.63 | 441.37 | 50.78 | Working tree | Chunking disabled (`ChunkSize=1`). |
| 2026-02-14 | Release | 20 | 600 | 388.10 | 393.19 | 353.73 | 407.45 | 12.30 | Working tree | Pow2 tile fast path disabled (always divide). |
| 2026-02-14 | Release | 20 | 600 | 378.83 | 378.61 | 358.04 | 394.89 | 7.39 | Working tree | Combined-alpha early-out disabled. |
| 2026-02-14 | Release | 20 | 600 | 410.06 | 416.12 | 365.27 | 424.16 | 15.18 | Working tree | Prepass clip bounds shrink disabled. |
| 2026-02-14 | Release | 20 | 600 | 380.27 | 381.67 | 364.52 | 392.03 | 7.74 | Working tree | Gradient color cache disabled. |
| 2026-02-14 | Release | 20 | 600 | 380.90 | 380.62 | 369.07 | 390.20 | 5.69 | Working tree | RGBA cache disabled. |
| 2026-02-14 | Release | 20 | 600 | 371.06 | 373.44 | 281.53 | 427.16 | 37.23 | Working tree | Clip cache disabled. |
| 2026-02-14 | Release | 20 | 600 | 385.71 | 408.56 | 167.23 | 437.68 | 65.12 | Working tree | Gradient params cache disabled. |
| 2026-02-14 | Release | 20 | 600 | 349.01 | 358.93 | 271.77 | 397.61 | 33.44 | Working tree | Text LUT disabled (per-pixel premultiplied). |

## Change Ledger
| Change | Status | Evidence |
| --- | --- | --- |
| Opaque glyph fast path uses `bitmapOpaque` and scan fallback | Kept | Release measurement pending. |
| Clear uses 32-bit fill when aligned/contiguous | Kept | Release measurement pending. |
| Fixed-point glyph positioning (remove `lround`) | Rejected | Release mean 395.16 FPS (regression). |
| Precompute text LUT per active command | Kept | Disabling LUT mean 349.01 FPS. |
| Precompute rect gradient params per active rect | Kept | Disabling cache mean 385.71 FPS. |
| Precompute rect center/rotation/half-extents | Rejected | Release mean 340.55 FPS (regression). |
| Opaque rect fast path (axis-aligned, no gradient) + rounded core fill | Kept | Default 149.33 FPS vs 94.53 off. Heavy 14.86 FPS vs 10.20 off. |
| Vertical gradient row fast path (axis-aligned) | Candidate | Default 162.01 FPS. Heavy 18.64 FPS. |
| Rounded gradient core fill (axis-aligned, vertical, opaque) | Candidate | Default 235.76 FPS. Heavy 22.67 FPS. |
| Gradient row increment (general) | Candidate | Default 236.55 FPS. Heavy 24.47 FPS. |
| Axis-aligned rect skip-rotation | Candidate | Default 261.78 FPS. Heavy 24.25 FPS. |
| Front-to-back early-out (assume sorted) | Candidate | Default render-only 300.78 FPS vs 285.59 FPS. Heavy mostly noise. |
| Tile-stream rendering | Candidate | Default 491.14 FPS vs 236.80 FPS. Heavy 106.82 FPS vs 23.80 FPS. |
| Auto tile-stream generation | Candidate | Default 457.67 FPS vs 208.20 FPS. Heavy 81.12 FPS vs 22.64 FPS. |
| Thread-local scratch vectors for prepass | Rejected | Segfault in release tests. |
| Cache per-command RGBA channels | Kept | Disabling cache mean 380.90 FPS. |
| Cache per-command clip rects | Kept | Disabling cache mean 371.06 FPS. |
| Cache gradient colors per rect | Kept | Disabling cache mean 380.27 FPS. |
| Prepass clip bounds shrink (rect/text) | Kept | Disabling shrink mean 410.06 FPS. |
| Prepass combined-alpha early-out (rect/text) | Kept | Disabling early-out mean 378.83 FPS. |
| Power-of-two tile index fast path | Kept | Disabling fast path mean 388.10 FPS. |
| Chunked tile pool work dispatch | Kept | Disabling chunking mean 383.38 FPS. |
| Renderer worker count = 2x hardware concurrency | Rejected | Release mean 160.46 FPS (regression). |
| Renderer worker count = 0.5x hardware concurrency | Rejected | Release mean 342.34 FPS (regression). |
| Renderer worker count = base-1 | Rejected | Release mean 395.39 FPS (regression). |
| Renderer worker count = base+1 | Rejected | Release mean 328.01 FPS (regression). |
| Renderer worker count = 1.5x hardware concurrency | Rejected | Release mean 205.84 FPS (regression). |
| Palette-indexed colors (256) | Kept | Release mean 384.39 FPS vs RGBA8 368.89 FPS (~4.2% win). |
| 16-bit command geometry (positions/clips, text sizes) | Candidate | Release mean 362.76 FPS (post-change baseline). |
| Tile-local command stream (uint8 coords) | Rejected | Release mean 360.40 FPS vs 384.66 FPS baseline (regression). Hierarchical traversal mean 307.06 FPS. |
| Hierarchical tile stream (L0/L1/L2) + front-to-back tile buffer | Rejected | Release mean 169.33 FPS vs 390.10 FPS baseline (regression). |
| Hierarchical tile stream + tile opaque-count early-out | Candidate | Release mean 371.37 FPS vs 370.26 FPS baseline (noise). |
| Hierarchical tile stream + in-place tile buffer | Candidate | Release mean 358.37 FPS vs 341.19 FPS baseline (~5% win, high variance). |
| Pre-merged per-tile tile stream | Kept | Release mean 573.13 FPS vs 358.65 FPS baseline (~60% win). |

## Profiling Sweeps (single-run, `--profile`)
| Date | Scenario | FPS | Render ms | Build ms | TileWork ms | Util % | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-02-14 | Default `--tile 8` (optimized, tile stream) | 741.71 | 1.620 | 0.907 | 13.529 | 69.59 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 16` (optimized, tile stream) | 664.81 | 1.656 | 1.029 | 14.514 | 73.04 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 24` (optimized, tile stream) | 522.80 | 2.573 | 0.768 | 19.843 | 64.27 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 32` (optimized, tile stream) | 491.70 | 1.959 | 0.738 | 19.779 | 84.14 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Heavy `--tile 8` (optimized, tile stream) | 675.49 | 1.608 | 8.978 | 14.556 | 75.44 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 16` (optimized, tile stream) | 629.50 | 2.403 | 8.322 | 22.937 | 79.54 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 24` (optimized, tile stream) | 416.95 | 2.346 | 7.892 | 22.325 | 79.30 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 32` (optimized, tile stream) | 294.24 | 3.338 | 7.770 | 28.508 | 71.17 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Default `--radius 0` (optimized, tile stream) | 543.81 | 1.714 | 0.760 | 16.653 | 80.97 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--radius 4` (optimized, tile stream) | 534.76 | 1.850 | 0.760 | 16.977 | 76.47 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--radius 8` (optimized, tile stream) | 495.14 | 1.992 | 0.741 | 19.688 | 82.36 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Heavy `--radius 0` (optimized, tile stream) | 436.82 | 2.290 | 7.855 | 22.824 | 83.06 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--radius 4` (optimized, tile stream) | 363.06 | 3.210 | 7.786 | 28.866 | 74.94 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--radius 8` (optimized, tile stream) | 332.95 | 3.118 | 8.152 | 30.862 | 82.48 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Default `--tile 32` (optimized, non-tile-stream) | 153.47 | 9.056 | 0.807 | 83.487 | 76.82 | `--optimized`, renderTiles fix; RenderedTiles 920, RenderedCommands 23341. |

## Tile Sweep (post non-tile-stream renderTiles fix)
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default `--tile 8` (combined) | 94.05 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 16` (combined) | 105.71 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 32` (combined) | 132.70 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 64` (combined) | 112.91 | 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 8` (render-only) | 130.40 | `--optimized`, 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 16` (render-only) | 108.98 | `--optimized`, 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 32` (render-only) | 128.56 | `--optimized`, 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Default `--tile 64` (render-only) | 110.09 | `--optimized`, 1280x720, 4000 rects, 200 texts. |
| 2026-02-14 | Heavy `--tile 8` (combined) | 11.37 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 16` (combined) | 12.53 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 32` (combined) | 12.94 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 64` (combined) | 12.19 | 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 8` (render-only) | 13.27 | `--optimized`, 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 16` (render-only) | 13.48 | `--optimized`, 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 32` (render-only) | 14.99 | `--optimized`, 1280x720, 40000 rects, 2000 texts. |
| 2026-02-14 | Heavy `--tile 64` (render-only) | 12.84 | `--optimized`, 1280x720, 40000 rects, 2000 texts. |

## Profiling Breakdown (phase timings)
| Date | Scenario | FPS | Render ms | Tiles ms | TileWork ms | Optimize ms | Binning ms | RectCache ms | TextCache ms | Util % | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-02-14 | Default (combined) | 130.42 | 6.181 | 6.112 | 61.320 | 0.833 | 0.079 | 0.696 | 0.043 | 82.67 | 1280x720, 4000 rects, 200 texts, tile 32. |
| 2026-02-14 | Default (render-only) | 154.03 | 7.020 | 6.954 | 72.375 | 0.000 | 0.000 | 0.000 | 0.000 | 85.92 | `--optimized`, 1280x720, 4000 rects, 200 texts, tile 32. |
| 2026-02-14 | Heavy (combined) | 13.17 | 61.434 | 61.371 | 719.545 | 8.625 | 1.156 | 6.967 | 0.424 | 97.60 | 1280x720, 40000 rects, 2000 texts, tile 32. |
| 2026-02-14 | Heavy (render-only) | 15.60 | 62.147 | 62.083 | 723.710 | 0.000 | 0.000 | 0.000 | 0.000 | 97.04 | `--optimized`, 1280x720, 40000 rects, 2000 texts, tile 32. |

## Opaque Rect Fast Path (A/B)
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default (fast path on) | 149.33 | 1280x720, 4000 rects, 200 texts, tile 32. |
| 2026-02-14 | Default (fast path off) | 94.53 | `--no-opaque-rect-fastpath`. |
| 2026-02-14 | Default render-only (fast path on) | 163.40 | `--optimized`. |
| 2026-02-14 | Default render-only (fast path off) | 112.16 | `--optimized --no-opaque-rect-fastpath`. |
| 2026-02-14 | Heavy (fast path on) | 14.86 | 1280x720, 40000 rects, 2000 texts, tile 32. |
| 2026-02-14 | Heavy (fast path off) | 10.20 | `--no-opaque-rect-fastpath`. |
| 2026-02-14 | Heavy render-only (fast path on) | 18.29 | `--optimized`. |
| 2026-02-14 | Heavy render-only (fast path off) | 10.91 | `--optimized --no-opaque-rect-fastpath`. |

## Vertical Gradient Row Fast Path
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default (combined) | 162.01 | 1280x720, 4000 rects, 200 texts, tile 32. |
| 2026-02-14 | Default (render-only) | 200.61 | `--optimized`. |
| 2026-02-14 | Heavy (combined) | 18.64 | 1280x720, 40000 rects, 2000 texts, tile 32. |
| 2026-02-14 | Heavy (render-only) | 22.68 | `--optimized`. |

## Rounded Gradient Core Fill (vertical)
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default (combined) | 235.76 | 1280x720, 4000 rects, 200 texts, tile 32. |
| 2026-02-14 | Default (render-only) | 312.72 | `--optimized`. |
| 2026-02-14 | Heavy (combined) | 22.67 | 1280x720, 40000 rects, 2000 texts, tile 32. |
| 2026-02-14 | Heavy (render-only) | 29.38 | `--optimized`. |

## Gradient Row Increment (general)
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default (combined) | 236.55 | 1280x720, 4000 rects, 200 texts, tile 32. |
| 2026-02-14 | Default (render-only) | 314.09 | `--optimized`. |
| 2026-02-14 | Heavy (combined) | 24.47 | 1280x720, 40000 rects, 2000 texts, tile 32. |
| 2026-02-14 | Heavy (render-only) | 31.01 | `--optimized`. |

## Axis-Aligned Rect Fast Path (skip rotation)
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default (combined) | 261.78 | 1280x720, 4000 rects, 200 texts, tile 32. |
| 2026-02-14 | Default (render-only) | 350.49 | `--optimized`. |
| 2026-02-14 | Heavy (combined) | 24.25 | 1280x720, 40000 rects, 2000 texts, tile 32. |
| 2026-02-14 | Heavy (render-only) | 30.51 | `--optimized`. |

## Front-to-Back Early-Out (assume sorted)
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default (combined, F2B on) | 236.29 | `--front-to-back`. |
| 2026-02-14 | Default (combined, F2B off) | 236.80 | `--no-front-to-back`. |
| 2026-02-14 | Default (render-only, F2B on) | 300.78 | `--optimized --front-to-back`. |
| 2026-02-14 | Default (render-only, F2B off) | 285.59 | `--optimized --no-front-to-back`. |
| 2026-02-14 | Heavy (combined, F2B on) | 23.35 | `--front-to-back`. |
| 2026-02-14 | Heavy (combined, F2B off) | 23.80 | `--no-front-to-back`. |
| 2026-02-14 | Heavy (render-only, F2B on) | 30.78 | `--optimized --front-to-back`. |
| 2026-02-14 | Heavy (render-only, F2B off) | 30.44 | `--optimized --no-front-to-back`. |

## Tile-Stream + Front-to-Back (assume sorted)
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default (combined, stream off) | 236.80 | `--tile-stream` disabled, `--no-auto-tile-stream`. |
| 2026-02-14 | Default (combined, stream on) | 491.14 | `--tile-stream`. |
| 2026-02-14 | Default (combined, stream+F2B) | 492.20 | `--tile-stream --front-to-back`. |
| 2026-02-14 | Default (render-only, stream off) | 285.59 | `--optimized --no-auto-tile-stream`. |
| 2026-02-14 | Default (render-only, stream on) | 933.21 | `--optimized --tile-stream`. |
| 2026-02-14 | Default (render-only, stream+F2B) | 861.30 | `--optimized --tile-stream --front-to-back`. |
| 2026-02-14 | Heavy (combined, stream off) | 23.80 | `--tile-stream` disabled, `--no-auto-tile-stream`. |
| 2026-02-14 | Heavy (combined, stream on) | 106.82 | `--tile-stream`. |
| 2026-02-14 | Heavy (combined, stream+F2B) | 105.71 | `--tile-stream --front-to-back`. |
| 2026-02-14 | Heavy (render-only, stream off) | 30.44 | `--optimized --no-auto-tile-stream`. |
| 2026-02-14 | Heavy (render-only, stream on) | 570.87 | `--optimized --tile-stream`. |
| 2026-02-14 | Heavy (render-only, stream+F2B) | 619.46 | `--optimized --tile-stream --front-to-back`. |

## Auto Tile-Stream (generated)
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default (combined, auto off) | 208.20 | `--no-auto-tile-stream`. |
| 2026-02-14 | Default (combined, auto on) | 457.67 | `--auto-tile-stream`. |
| 2026-02-14 | Default (render-only, auto off) | 319.01 | `--optimized --no-auto-tile-stream`. |
| 2026-02-14 | Default (render-only, auto on) | 816.38 | `--optimized --auto-tile-stream`. |
| 2026-02-14 | Heavy (combined, auto off) | 22.64 | `--no-auto-tile-stream`. |
| 2026-02-14 | Heavy (combined, auto on) | 81.12 | `--auto-tile-stream`. |
| 2026-02-14 | Heavy (render-only, auto off) | 28.84 | `--optimized --no-auto-tile-stream`. |
| 2026-02-14 | Heavy (render-only, auto on) | 485.88 | `--optimized --auto-tile-stream`. |

## Reuse Optimized Batch (cached)
| Date | Scenario | FPS | Notes |
| --- | --- | --- | --- |
| 2026-02-14 | Default (combined, reuse off) | 415.59 | Median of 3 runs, default settings. |
| 2026-02-14 | Default (combined, reuse on) | 888.49 | Median of 3 runs, `--reuse-optimized`. |
| 2026-02-14 | Default (render-only) | 863.29 | Median of 3 runs, `--optimized`. |
| 2026-02-14 | Heavy (combined, reuse off) | 85.09 | Median of 3 runs, 40k rects/2k texts. |
| 2026-02-14 | Heavy (combined, reuse on) | 557.77 | Median of 3 runs, `--reuse-optimized`. |
| 2026-02-14 | Heavy (render-only) | 457.07 | Median of 3 runs, `--optimized`. |

## Circle Benchmark Protocol (1080p, 250k circles)
Date: 2026-02-15

- Benchmark: `./build-release/renderer_bench --circle-bench --profile`
- Scene: 1920x1080, 250000 circles, radius 4, palette-indexed colors.
- Motion: circle Y positions alternate up/down each frame (step 2px). Random distribution is precomputed; no RNG in the loop.
- Frames: 300 (default).
- Tile size: requested 32, auto tile size picks 128 for circle-majority batches.
- Optimizer: runs every frame (dynamic circles), render-only mode disabled.

## Circle Benchmark Measurements
| Date | Runs | Frames | Mean FPS | Median | Min | Max | Stdev | Commit | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-02-15 | 20 | 300 | 243.23 | 245.30 | 223.76 | 253.27 | 7.50 | `d67b42b` | Circle-only tiles use chunk size 1 in tile pool. |
| 2026-02-15 | 20 | 300 | 239.84 | 241.70 | 229.71 | 245.01 | 4.35 | `9115b50` | Clipped opaque small-circle path uses edge list + span fill. |
| 2026-02-15 | 5 | 300 | 208.45 | 208.93 | 192.40 | 218.53 | 9.13 | `cc6326b` | Baseline after blend lookup cache, tileSize auto=128. |
| 2026-02-15 | 10 | 300 | 218.44 | 221.62 | 198.38 | 231.78 | 11.75 | `ea71015` | Cached edge premultiplied colors (2x5-run batches). |
| 2026-02-15 | 10 | 300 | 197.36 | 198.39 | 183.52 | 207.55 | 7.69 | `76abd6d` | High-quality baseline run set (current head). |
| 2026-02-15 | 20 | 300 | 184.04 | 185.46 | 169.19 | 207.51 | 9.39 | `b8b149a` | 20-run baseline (current head). |
| 2026-02-15 | 20 | 300 | 195.45 | 195.14 | 174.37 | 210.08 | 11.83 | `706f0f9` | 20-run baseline (repeat). |
| 2026-02-15 | 40 | 300 | 189.75 | 188.58 | 169.19 | 210.08 | 12.11 | `706f0f9` | Pinned baseline (combined 2x20-run sets). |
| 2026-02-15 | 40 | 300 | 201.68 | 205.99 | 180.68 | 216.05 | 10.73 | `5fd969d` | A/B baseline run (SIMD comparison). |

## Circle Benchmark Experiments
| Change | Status | Evidence |
| --- | --- | --- |
| Clipped opaque small-circle rendering via edge list + opaque span fill | Kept | 20-run mean 239.84 FPS vs 201.68 baseline (~19% win). |
| Circle-only tile pool chunk size override (`chunkSize=1`) | Kept | 20-run mean 243.23 FPS vs 239.84 current head (~1.4% win). |
| Circle-only tile pool chunk size override (`chunkSize=2`) | Rejected | 20-run mean 236.62 FPS vs 243.23 (regression). |
| Circle-only tileRefs fast path (skip command dispatch) | Rejected | 20-run mean 200.05 FPS vs 239.84 current head (regression). |
| Palette-opaque circle path (reuse cached palette channels, skip alpha checks) | Rejected | 20-run mean 205.66 FPS vs 239.84 current head (regression). |
| Pack circle edge X/cov into 16-bit entries | Rejected | 20-run mean 207.30 FPS vs 239.84 current head (regression, high variance). |
| Sort renderTiles by per-tile circle counts | Rejected | 20-run mean 235.79 FPS vs 239.84 current head (slight regression). |
| Cache per-radius edge premultiplied colors (per palette) | Kept | Mean 218.44 FPS vs 208.45 baseline (~4.8% win). |
| Pointer-increment mask row traversal for partial circles | Rejected | Mean ~197 FPS vs ~204 baseline (regression). |
| Use `pmTable[255]` for circle color (skip palette load) | Rejected | Mean ~165 FPS (regression). |
| TilePool chunk sizing = `workers*2`, max 16 | Rejected | 149–187 FPS range (regression). |
| Uniform-radius binning `compute_span` (avoid per-circle radius load) | Rejected | Means ~188–195 FPS (no win / regression). |
| Refactor masked circle render into shared lambda | Rejected | 137–200 FPS range (regression/noise). |
| Small-count 32-bit fill helper (replace `std::fill` for short spans) | Rejected | Mean ~184 FPS (regression). |
| Precompute full mask premultiplied table (per palette, per radius) | Rejected | Means ~206–220 FPS (no clear win). |
| SIMD (NEON) blend for masked head/tail spans (dst-opaque path) | Rejected | 40-run A/B: SIMD mean 200.95 vs baseline mean 201.68 (regression). |
| Arithmetic `mul_div_255` (avoid lookup table) in dst-opaque blend | Rejected | 10-run A/B: 206.14 mean vs 205.51 baseline (no clear win). |
| Cache palette row pointers + packed RGBA | Rejected | 10-run A/B: 219.74 mean vs 218.75 baseline (no clear win). |
| Tile-stream default when circles are majority | Rejected | ~95 FPS (large regression). |
| Tile size sweep (single-run) | Rejected | 96:210, 128:226, 160:215, 192:173, 224:191, 256:213 (128 best). |
| Misc micro-opts (no wins) | Rejected | Removing uniform-radius branch, edge-byte offsets, `paletteOpaque` handling, circle-only fast path, flatten localCounts, disable parallel binning, power-of-two split in `compute_span`, reuse binning pool, auto tile-stream for circle majority: all regressed in spot checks. |

## Next Steps
1. Pick a baseline commit and add it to Measurements.
2. For each new optimization, capture a 20x600 release measurement and append a row.
3. Use mean and stdev to compare wins, avoid debug-only numbers.
