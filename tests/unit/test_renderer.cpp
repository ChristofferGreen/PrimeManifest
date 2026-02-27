#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_helpers.hpp"
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

namespace {

void enable_palette(RenderBatch& batch, uint32_t color) {
  batch.palette.enabled = true;
  batch.palette.size = 1;
  batch.palette.colorRGBA8[0] = color;
}

static void add_mask_glyph_run(RenderBatch& batch,
                               uint16_t width,
                               uint16_t height,
                               std::vector<uint8_t> pixels) {
  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = width;
  bitmap.height = height;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = static_cast<int16_t>(width);
  bitmap.stride = static_cast<int32_t>(width);
  bitmap.pixels = std::move(pixels);
  batch.glyphs.bitmaps.push_back(std::move(bitmap));
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);
}

} // namespace


TEST_SUITE_BEGIN("primemanifest.renderer");

TEST_CASE("rejects_disabled_palette") {
  RenderBatch batch;

  OptimizedBatch optimized;
  optimized.valid = true;
  optimized.targetWidth = 4;
  optimized.targetHeight = 4;

  std::vector<uint8_t> buffer(4 * 4 * 4, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), 4, 4, 16};

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(buffer[0] == 0x7F, "palette-disabled early return");
}

TEST_CASE("rejects_target_mismatch") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{1, 2, 3, 4}));

  OptimizedBatch optimized;
  optimized.valid = true;
  optimized.targetWidth = 8;
  optimized.targetHeight = 8;

  std::vector<uint8_t> buffer(4 * 4 * 4, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), 4, 4, 16};

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(buffer[0] == 0x7F, "target mismatch early return");
}

TEST_CASE("rejects_zero_stride") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{1, 2, 3, 4}));

  OptimizedBatch optimized;
  optimized.valid = true;
  optimized.targetWidth = 4;
  optimized.targetHeight = 4;

  std::vector<uint8_t> buffer(4 * 4 * 4, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), 4, 4, 0};

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(buffer[0] == 0x7F, "zero stride early return");
}

TEST_CASE("tile_buffer_clear_applies") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 0}));
  batch.tileSize = 8;

  add_clear(batch, PackRGBA8(Color{10, 20, 30, 128}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 0};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  uint32_t expected = PackRGBA8(Color{5, 10, 15, 128});
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == expected, "tile-buffer clear writes premultiplied pixel");
}

TEST_CASE("tile_buffer_clear_pattern_applies") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 0}));
  batch.tileSize = 8;

  std::vector<uint32_t> pixels = {
    PackRGBA8(Color{255, 0, 0, 255}),
    PackRGBA8(Color{0, 255, 0, 255}),
    PackRGBA8(Color{0, 0, 255, 255}),
    PackRGBA8(Color{255, 255, 255, 255}),
  };
  add_clear_pattern(batch, 2, 2, pixels);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 0};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t green = PackRGBA8(Color{0, 255, 0, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == red, "tile-buffer pattern (0,0)");
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 0) == green, "tile-buffer pattern (1,0)");
}

TEST_CASE("gradient_dir_normalized_in_renderer") {
  RenderBatch batch;
  add_gradient_rect_dir(batch,
                        0,
                        0,
                        8,
                        8,
                        PackRGBA8(Color{255, 0, 0, 255}),
                        PackRGBA8(Color{0, 0, 255, 255}),
                        0,
                        0);

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  optimized.rectHasGradient.clear();

  RenderOptimized(target, batch, optimized);

  uint32_t top = pixel_at(buffer, width, 0, 0);
  uint32_t bottom = pixel_at(buffer, width, 0, height - 1);
  CHECK_MESSAGE(top != bottom, "normalized gradient affects output");
}

TEST_CASE("non_tile_stream_uses_clipped_bounds") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 8;

  add_rect(batch, 0, 0, 6, 6, PackRGBA8(Color{80, 160, 240, 255}));
  batch.rects.flags[0] = RectFlagClip;
  batch.rects.clipX0[0] = 2;
  batch.rects.clipY0[0] = 1;
  batch.rects.clipX1[0] = 4;
  batch.rects.clipY1[0] = 5;

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  CHECK_MESSAGE(!optimized.useTileStream, "non-tile-stream path is active");

  RenderOptimized(target, batch, optimized);

  uint32_t drawColor = PackRGBA8(Color{80, 160, 240, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 2) == drawColor, "pixel inside clip rendered");
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 2) == 0u, "pixel left of clip skipped");
  CHECK_MESSAGE(pixel_at(buffer, width, 4, 2) == 0u, "pixel right of clip skipped");
}

TEST_CASE("tile_stream_set_pixel_uses_local_bounds") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 8;
  uint32_t drawColor = PackRGBA8(Color{90, 40, 200, 255});
  add_set_pixel(batch, 0, 0, drawColor);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::SetPixel;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 6;
  cmd.y = 5;
  cmd.wMinus1 = 0;
  cmd.hMinus1 = 0;
  batch.tileStream.commands = {cmd};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(pixel_at(buffer, width, 6, 5) == drawColor, "tile-local set-pixel rendered at local bounds");
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == 0u, "source store coordinate remains untouched");
}

TEST_CASE("tile_stream_set_pixel_a_uses_local_bounds") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 8;
  add_set_pixel_a(batch, 1, 1, PackRGBA8(Color{255, 200, 100, 255}), 128);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::SetPixelA;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 4;
  cmd.y = 6;
  cmd.wMinus1 = 0;
  cmd.hMinus1 = 0;
  batch.tileStream.commands = {cmd};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  uint32_t expected = PackRGBA8(Color{128, 100, 50, 128});
  CHECK_MESSAGE(pixel_at(buffer, width, 4, 6) == expected, "tile-local set-pixel-a rendered at local bounds");
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == 0u, "source store coordinate remains untouched");
}

TEST_CASE("tile_stream_line_respects_local_clip_bounds") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 8;
  add_line(batch, 0, 0, 7, 0, 1.0f, PackRGBA8(Color{255, 0, 0, 255}), 255);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Line;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 0;
  cmd.y = 5;
  cmd.wMinus1 = 7;
  cmd.hMinus1 = 2;
  batch.tileStream.commands = {cmd};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(pixel_at(buffer, width, 2, 0) == 0u, "line outside local clip bounds is skipped");
}

TEST_CASE("tile_stream_image_respects_local_clip_bounds") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 8;

  uint32_t imageIdx = add_image_asset(batch, 1, 1, {PackRGBA8(Color{255, 0, 0, 255})});
  add_image_draw(batch,
                 imageIdx,
                 0,
                 0,
                 4,
                 4,
                 0,
                 0,
                 1,
                 1,
                 PackRGBA8(Color{255, 255, 255, 255}),
                 255);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Image;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 4;
  cmd.y = 4;
  cmd.wMinus1 = 3;
  cmd.hMinus1 = 3;
  batch.tileStream.commands = {cmd};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == 0u, "image outside local clip bounds is skipped");
}

TEST_CASE("tile_stream_rect_respects_local_clip_bounds") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 16;
  uint32_t drawColor = PackRGBA8(Color{30, 150, 240, 255});
  add_rect(batch, 0, 0, 10, 10, drawColor);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Rect;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 6;
  cmd.y = 6;
  cmd.wMinus1 = 3;
  cmd.hMinus1 = 3;
  batch.tileStream.commands = {cmd};

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(pixel_at(buffer, width, 7, 7) == drawColor, "rect pixel inside local clip bounds is rendered");
  CHECK_MESSAGE(pixel_at(buffer, width, 3, 3) == 0u, "rect pixel outside local clip bounds is skipped");
}

TEST_CASE("tile_stream_rounded_rect_respects_local_clip_bounds") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 16;
  uint32_t drawColor = PackRGBA8(Color{200, 110, 40, 255});
  add_rect(batch, 2, 2, 14, 14, drawColor);
  batch.rects.radiusQ8_8[0] = 512;

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Rect;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 8;
  cmd.y = 2;
  cmd.wMinus1 = 5;
  cmd.hMinus1 = 11;
  batch.tileStream.commands = {cmd};

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(pixel_at(buffer, width, 10, 8) == drawColor, "rounded rect pixel inside local clip bounds is rendered");
  CHECK_MESSAGE(pixel_at(buffer, width, 6, 8) == 0u, "rounded rect pixel outside local clip bounds is skipped");
}

TEST_CASE("tile_stream_text_respects_local_clip_bounds") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 8;
  add_mask_glyph_run(batch, 2, 1, {255, 255});
  uint32_t drawColor = PackRGBA8(Color{15, 210, 100, 255});
  add_text(batch, 1, 1, 2, 1, drawColor, 0);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Text;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 2;
  cmd.y = 1;
  cmd.wMinus1 = 0;
  cmd.hMinus1 = 0;
  batch.tileStream.commands = {cmd};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(pixel_at(buffer, width, 2, 1) == drawColor, "text pixel inside local clip bounds is rendered");
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == 0u, "text pixel outside local clip bounds is skipped");
}

TEST_CASE("tile_stream_text_respects_local_and_text_clip_bounds") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 8;
  add_mask_glyph_run(batch, 3, 1, {255, 255, 255});
  uint32_t drawColor = PackRGBA8(Color{230, 120, 20, 255});
  add_text(batch, 1, 1, 3, 1, drawColor, 0);
  batch.text.flags[0] = TextFlagClip;
  batch.text.clipX0[0] = 1;
  batch.text.clipY0[0] = 1;
  batch.text.clipX1[0] = 2;
  batch.text.clipY1[0] = 2;

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Text;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 2;
  cmd.y = 1;
  cmd.wMinus1 = 1;
  cmd.hMinus1 = 0;
  batch.tileStream.commands = {cmd};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == 0u, "text clip-only pixel is excluded by local bounds");
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 1) == 0u, "local-bounds pixel is excluded by text clip");
}

TEST_CASE("tile_stream_circle_respects_local_clip_bounds_small_radius") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 16;

  uint32_t drawColor = PackRGBA8(Color{40, 220, 120, 255});
  add_circle(batch, 6, 6, 3, drawColor);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Circle;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 7;
  cmd.y = 4;
  cmd.wMinus1 = 2;
  cmd.hMinus1 = 3;
  batch.tileStream.commands = {cmd};

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(pixel_at(buffer, width, 8, 6) != 0u, "circle pixel inside local clip bounds is rendered");
  CHECK_MESSAGE(pixel_at(buffer, width, 6, 6) == 0u, "circle pixel outside local clip bounds is skipped");
}

TEST_CASE("tile_stream_circle_respects_local_clip_bounds_large_radius") {
  RenderBatch batch;
  batch.autoTileStream = false;
  batch.tileSize = 16;

  uint32_t drawColor = PackRGBA8(Color{220, 80, 20, 255});
  add_circle(batch, 8, 8, 10, drawColor);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Circle;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 0;
  cmd.y = 0;
  cmd.wMinus1 = 4;
  cmd.hMinus1 = 15;
  batch.tileStream.commands = {cmd};

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "tile stream path enabled");

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(pixel_at(buffer, width, 2, 8) == drawColor, "large-radius circle pixel inside local clip bounds is rendered");
  CHECK_MESSAGE(pixel_at(buffer, width, 8, 8) == 0u, "large-radius circle pixel outside local clip bounds is skipped");
}

TEST_SUITE_END();
