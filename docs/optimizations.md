# Renderer 2D Optimization Log

Date: 2026-02-14

## Protocol
- Build: Release only.
- Benchmark: `./renderer_bench --frames 600`.
- Runs: 20 per measurement.
- Report: mean FPS plus dispersion (median, min, max, stdev).

## Current Aggregate
- Mean: 384.66 FPS
- Median: 391.71 FPS
- Min: 337.18 FPS
- Max: 409.73 FPS
- Stdev: 22.48 FPS
- Build: Release
- Runs: 20
- Frames: 600
- Commit: working tree (uncommitted)

## Measurements
| Date | Build | Runs | Frames | Mean FPS | Median | Min | Max | Stdev | Commit | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-02-14 | Release | 20 | 600 | 384.66 | 391.71 | 337.18 | 409.73 | 22.48 | Working tree | Current aggregate (palette-only command colors, int16 geometry, tileSize=32). |
| 2026-02-14 | Release | 20 | 600 | 360.40 | 370.81 | 296.29 | 383.49 | 22.94 | Working tree | Tile-local command stream (pre-binned, uint8 coords). |
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
| 32-bit fills for opaque rect interior + opaque glyph rects | Kept | Release measurement pending. |
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
| Tile-local command stream (uint8 coords) | Rejected | Release mean 360.40 FPS vs 384.66 FPS baseline (regression). |

## Next Steps
1. Pick a baseline commit and add it to Measurements.
2. For each new optimization, capture a 20x600 release measurement and append a row.
3. Use mean and stdev to compare wins, avoid debug-only numbers.
