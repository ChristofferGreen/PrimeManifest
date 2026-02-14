#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <chrono>
#include <random>
#include <vector>

using namespace PrimeManifest;

namespace {

int failures = 0;

void render_batch(RenderTarget target, RenderBatch const& batch) {
  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);
}

void check(bool cond, char const* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << "\n";
    ++failures;
  }
}

auto palette_index(RenderBatch& batch, uint32_t color) -> uint8_t {
  if (!batch.palette.enabled) {
    batch.palette.enabled = true;
    batch.palette.size = 0;
    batch.palette.colorRGBA8.fill(0u);
  }
  for (uint16_t i = 0; i < batch.palette.size; ++i) {
    if (batch.palette.colorRGBA8[i] == color) {
      return static_cast<uint8_t>(i);
    }
  }
  if (batch.palette.size >= batch.palette.colorRGBA8.size()) {
    check(false, "palette overflow");
    return 0;
  }
  uint8_t idx = static_cast<uint8_t>(batch.palette.size++);
  batch.palette.colorRGBA8[idx] = color;
  return idx;
}

auto build_test_colors() -> std::array<uint32_t, 64> {
  std::array<uint32_t, 64> colors{};
  uint8_t levels[4] = {0, 85, 170, 255};
  size_t idx = 0;
  for (uint8_t r : levels) {
    for (uint8_t g : levels) {
      for (uint8_t b : levels) {
        colors[idx++] = PackRGBA8(Color{r, g, b, 255});
      }
    }
  }
  return colors;
}

auto pixel_at(std::vector<uint8_t> const& buffer, uint32_t width, uint32_t x, uint32_t y) -> uint32_t {
  size_t idx = static_cast<size_t>(y) * width * 4 + static_cast<size_t>(x) * 4;
  uint32_t r = buffer[idx + 0];
  uint32_t g = buffer[idx + 1];
  uint32_t b = buffer[idx + 2];
  uint32_t a = buffer[idx + 3];
  return r | (g << 8) | (b << 16) | (a << 24);
}

auto channel_at(std::vector<uint8_t> const& buffer, uint32_t width, uint32_t x, uint32_t y, int channel) -> uint8_t {
  size_t idx = static_cast<size_t>(y) * width * 4 + static_cast<size_t>(x) * 4;
  return buffer[idx + static_cast<size_t>(channel)];
}

auto buffers_equal(std::vector<uint8_t> const& a, std::vector<uint8_t> const& b) -> bool {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void add_clear(RenderBatch& batch, uint32_t color) {
  uint32_t idx = static_cast<uint32_t>(batch.clear.colorIndex.size());
  batch.clear.colorIndex.push_back(palette_index(batch, color));
  batch.commands.push_back(RenderCommand{CommandType::Clear, idx});
}

void add_rect(RenderBatch& batch,
              int32_t x0,
              int32_t y0,
              int32_t x1,
              int32_t y1,
              uint32_t color) {
  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(x0);
  batch.rects.y0.push_back(y0);
  batch.rects.x1.push_back(x1);
  batch.rects.y1.push_back(y1);
  uint8_t colorIndex = palette_index(batch, color);
  batch.rects.colorIndex.push_back(colorIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(0);
  batch.rects.gradientColor1Index.push_back(colorIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
}

void add_gradient_rect(RenderBatch& batch,
                       int32_t x0,
                       int32_t y0,
                       int32_t x1,
                       int32_t y1,
                       uint32_t color0,
                       uint32_t color1) {
  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(x0);
  batch.rects.y0.push_back(y0);
  batch.rects.x1.push_back(x1);
  batch.rects.y1.push_back(y1);
  batch.rects.colorIndex.push_back(palette_index(batch, color0));
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagGradient);
  batch.rects.gradientColor1Index.push_back(palette_index(batch, color1));
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(256);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
}

void add_gradient_rect_dir(RenderBatch& batch,
                           int32_t x0,
                           int32_t y0,
                           int32_t x1,
                           int32_t y1,
                           uint32_t color0,
                           uint32_t color1,
                           int16_t dirX,
                           int16_t dirY) {
  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(x0);
  batch.rects.y0.push_back(y0);
  batch.rects.x1.push_back(x1);
  batch.rects.y1.push_back(y1);
  batch.rects.colorIndex.push_back(palette_index(batch, color0));
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagGradient);
  batch.rects.gradientColor1Index.push_back(palette_index(batch, color1));
  batch.rects.gradientDirX.push_back(dirX);
  batch.rects.gradientDirY.push_back(dirY);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
}

void add_text(RenderBatch& batch,
              int32_t x,
              int32_t y,
              int32_t width,
              int32_t height,
              uint32_t color,
              uint32_t runIndex) {
  uint32_t idx = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(x);
  batch.text.y.push_back(y);
  batch.text.width.push_back(width);
  batch.text.height.push_back(height);
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(255);
  batch.text.colorIndex.push_back(palette_index(batch, color));
  batch.text.flags.push_back(0);
  batch.text.runIndex.push_back(runIndex);
  batch.text.clipX0.push_back(0);
  batch.text.clipY0.push_back(0);
  batch.text.clipX1.push_back(0);
  batch.text.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});
}

void test_clear() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{10, 20, 30, 255});
  check(pixel_at(buffer, width, 0, 0) == expected, "clear sets pixel 0,0");
  check(pixel_at(buffer, width, 3, 3) == expected, "clear sets pixel 3,3");
}

void test_clear_alpha() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 128}));

  uint32_t width = 2;
  uint32_t height = 2;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  check(channel_at(buffer, width, 0, 0, 3) == 128, "clear preserves alpha");
}

void test_clear_last_wins() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));
  add_clear(batch, PackRGBA8(Color{40, 50, 60, 255}));

  uint32_t width = 2;
  uint32_t height = 2;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{40, 50, 60, 255});
  check(pixel_at(buffer, width, 0, 0) == expected, "last clear wins");
}

void test_rect() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 2, 2, 6, 6, PackRGBA8(Color{200, 0, 0, 255}));

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{200, 0, 0, 255});
  check(pixel_at(buffer, width, 3, 3) == expected, "rect fills interior pixel");
}

void test_text() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 2;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 2;
  bitmap.stride = 2;
  bitmap.pixels = {255, 255, 255, 255};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, 1, 1, 2, 2, PackRGBA8(Color{0, 200, 0, 255}), 0);

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{0, 200, 0, 255});
  check(pixel_at(buffer, width, 1, 1) == expected, "text glyph draws pixel");
}

void test_gradient_rect() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect(batch, 0, 0, 10, 10, PackRGBA8(Color{0, 0, 0, 255}), PackRGBA8(Color{255, 255, 255, 255}));

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t top = channel_at(buffer, width, 5, 2, 0);
  uint8_t bottom = channel_at(buffer, width, 5, 8, 0);
  check(top < bottom, "gradient rect increases along Y");
}

void test_gradient_same_colors() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect(batch, 0, 0, 10, 10,
                    PackRGBA8(Color{50, 60, 70, 255}),
                    PackRGBA8(Color{50, 60, 70, 255}));

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{50, 60, 70, 255});
  check(pixel_at(buffer, width, 2, 2) == expected, "gradient with same colors is uniform");
  check(pixel_at(buffer, width, 8, 8) == expected, "gradient with same colors is uniform");
}

void test_gradient_dir_normalized() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect_dir(batch, 0, 0, 10, 10,
                        PackRGBA8(Color{0, 0, 0, 255}),
                        PackRGBA8(Color{255, 255, 255, 255}),
                        0, 0);

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t top = channel_at(buffer, width, 5, 2, 0);
  uint8_t bottom = channel_at(buffer, width, 5, 8, 0);
  check(top < bottom, "gradient dir fallback behaves vertically");
}

void test_gradient_horizontal() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect_dir(batch, 0, 0, 10, 10,
                        PackRGBA8(Color{0, 0, 0, 255}),
                        PackRGBA8(Color{255, 255, 255, 255}),
                        256, 0);

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t left = channel_at(buffer, width, 2, 5, 0);
  uint8_t right = channel_at(buffer, width, 8, 5, 0);
  check(left < right, "gradient rect increases along X");
}

void test_debug_tiles() {
  RenderBatch batch;
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{10, 10, 10, 255}));

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(1);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  check(pixel_at(buffer, width, 0, 0) == debugColor, "debug tiles draw outline pixel");
  check(pixel_at(buffer, width, 1, 1) == PackRGBA8(Color{10, 10, 10, 255}), "debug tiles leave interior");
}

void test_debug_tiles_dirty_only() {
  RenderBatch batch;
  batch.tileSize = 8;
  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{0, 0, 255, 255}));

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(1);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  check(pixel_at(buffer, width, 0, 0) == debugColor, "dirty-only tiles outline touched tile");
  check(pixel_at(buffer, width, 12, 0) == 0, "dirty-only tiles skip untouched tile");
}

void test_debug_tiles_all() {
  RenderBatch batch;
  batch.tileSize = 8;

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(1);
  batch.debugTiles.flags.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  check(pixel_at(buffer, width, 0, 0) == debugColor, "debug tiles outline even without dirty");
}

void test_rect_clip() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(7);
  batch.rects.y1.push_back(7);
  uint8_t redIndex = palette_index(batch, PackRGBA8(Color{255, 0, 0, 255}));
  batch.rects.colorIndex.push_back(redIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagClip);
  batch.rects.gradientColor1Index.push_back(redIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(3);
  batch.rects.clipY0.push_back(3);
  batch.rects.clipX1.push_back(5);
  batch.rects.clipY1.push_back(5);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 4, 4) == red, "clip allows interior pixel");
  check(pixel_at(buffer, width, 2, 2) == black, "clip rejects outside pixel");
}

void test_rect_clip_outside() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(5);
  batch.rects.y1.push_back(5);
  uint8_t redIndex = palette_index(batch, PackRGBA8(Color{255, 0, 0, 255}));
  batch.rects.colorIndex.push_back(redIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagClip);
  batch.rects.gradientColor1Index.push_back(redIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(10);
  batch.rects.clipY0.push_back(10);
  batch.rects.clipX1.push_back(12);
  batch.rects.clipY1.push_back(12);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 2, 2) == black, "clip outside prevents draw");
}

void test_text_clip() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 3;
  bitmap.height = 3;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 3;
  bitmap.stride = 3;
  bitmap.pixels = {
      255, 255, 255,
      255, 255, 255,
      255, 255, 255,
  };
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  uint32_t idx = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(1);
  batch.text.y.push_back(1);
  batch.text.width.push_back(3);
  batch.text.height.push_back(3);
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(255);
  batch.text.colorIndex.push_back(palette_index(batch, PackRGBA8(Color{0, 255, 0, 255})));
  batch.text.flags.push_back(TextFlagClip);
  batch.text.runIndex.push_back(0);
  batch.text.clipX0.push_back(2);
  batch.text.clipY0.push_back(2);
  batch.text.clipX1.push_back(3);
  batch.text.clipY1.push_back(3);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});

  uint32_t width = 5;
  uint32_t height = 5;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t green = PackRGBA8(Color{0, 255, 0, 255});
  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 2, 2) == green, "text clip allows pixel");
  check(pixel_at(buffer, width, 1, 1) == black, "text clip rejects pixel");
}

void test_text_missing_bitmap() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(1); // invalid

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, 1, 1, 2, 2, PackRGBA8(Color{0, 200, 0, 255}), 0);

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 1, 1) == black, "missing bitmap skips draw");
}

void test_text_missing_run() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(1);
  batch.text.y.push_back(1);
  batch.text.width.push_back(2);
  batch.text.height.push_back(2);
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(255);
  batch.text.colorIndex.push_back(palette_index(batch, PackRGBA8(Color{0, 255, 0, 255})));
  batch.text.flags.push_back(0);
  batch.text.runIndex.push_back(5); // invalid
  batch.text.clipX0.push_back(0);
  batch.text.clipY0.push_back(0);
  batch.text.clipX1.push_back(0);
  batch.text.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 1, 1) == black, "missing run skips draw");
}

void test_rect_opacity_zero() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(5);
  batch.rects.y1.push_back(5);
  uint8_t yellowIndex = palette_index(batch, PackRGBA8(Color{200, 200, 0, 255}));
  batch.rects.colorIndex.push_back(yellowIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(0);
  batch.rects.flags.push_back(0);
  batch.rects.gradientColor1Index.push_back(yellowIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 2, 2) == black, "opacity zero skips rect");
}

void test_rect_opacity_half() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(5);
  batch.rects.y1.push_back(5);
  uint8_t redIndex = palette_index(batch, PackRGBA8(Color{100, 0, 0, 255}));
  batch.rects.colorIndex.push_back(redIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(128);
  batch.rects.flags.push_back(0);
  batch.rects.gradientColor1Index.push_back(redIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t red = channel_at(buffer, width, 2, 2, 0);
  check(red >= 49 && red <= 51, "opacity half blends to ~50");
}

void test_text_opacity_zero() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 2;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 2;
  bitmap.stride = 2;
  bitmap.pixels = {255, 255, 255, 255};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  uint32_t idx = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(1);
  batch.text.y.push_back(1);
  batch.text.width.push_back(2);
  batch.text.height.push_back(2);
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(0);
  batch.text.colorIndex.push_back(palette_index(batch, PackRGBA8(Color{0, 255, 0, 255})));
  batch.text.flags.push_back(0);
  batch.text.runIndex.push_back(0);
  batch.text.clipX0.push_back(0);
  batch.text.clipY0.push_back(0);
  batch.text.clipX1.push_back(0);
  batch.text.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 1, 1) == black, "text opacity zero skips draw");
}

void test_rect_offscreen() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, -10, -10, -2, -2, PackRGBA8(Color{255, 0, 0, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 0, 0) == black, "offscreen rect skipped");
}

void test_text_offscreen() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 2;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 2;
  bitmap.stride = 2;
  bitmap.pixels = {255, 255, 255, 255};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, -5, -5, 2, 2, PackRGBA8(Color{255, 255, 255, 255}), 0);

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 0, 0) == black, "offscreen text skipped");
}

void test_command_order() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 1, 1, 5, 5, PackRGBA8(Color{0, 0, 255, 255}));
  add_rect(batch, 1, 1, 5, 5, PackRGBA8(Color{255, 0, 0, 255}));

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  check(pixel_at(buffer, width, 2, 2) == red, "later command draws on top");
}

void test_rect_rotation_draws() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(2);
  batch.rects.y0.push_back(2);
  batch.rects.x1.push_back(6);
  batch.rects.y1.push_back(6);
  uint8_t blueIndex = palette_index(batch, PackRGBA8(Color{0, 0, 255, 255}));
  batch.rects.colorIndex.push_back(blueIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(static_cast<int16_t>(static_cast<int>(3.14159f * 0.5f * 256)));
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(0);
  batch.rects.gradientColor1Index.push_back(blueIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t blue = PackRGBA8(Color{0, 0, 255, 255});
  check(pixel_at(buffer, width, 4, 4) == blue, "rotated rect draws center");
}

void test_tile_size_zero_default() {
  RenderBatch batch;
  batch.tileSize = 0;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 1, 1, 3, 3, PackRGBA8(Color{100, 100, 255, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{100, 100, 255, 255});
  check(pixel_at(buffer, width, 2, 2) == expected, "tile size zero falls back to default");
}

void test_tile_size_large() {
  RenderBatch batch;
  batch.tileSize = 512;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 1, 1, 3, 3, PackRGBA8(Color{120, 120, 255, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{120, 120, 255, 255});
  check(pixel_at(buffer, width, 2, 2) == expected, "large tile size still renders");
}

void test_tile_size_non_power_of_two() {
  RenderBatch batch;
  batch.tileSize = 7;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 6, 6, 9, 9, PackRGBA8(Color{10, 200, 10, 255}));

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{10, 200, 10, 255});
  check(pixel_at(buffer, width, 7, 7) == expected, "non power-of-two tile size works");
}

void test_multi_tile_rect() {
  RenderBatch batch;
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 4, 4, 20, 20, PackRGBA8(Color{255, 0, 255, 255}));

  uint32_t width = 24;
  uint32_t height = 24;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{255, 0, 255, 255});
  check(pixel_at(buffer, width, 6, 6) == expected, "rect draws in first tile");
  check(pixel_at(buffer, width, 18, 18) == expected, "rect draws in later tile");
}

void test_debug_tiles_line_width() {
  RenderBatch batch;
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(2);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  check(pixel_at(buffer, width, 0, 0) == debugColor, "debug line width draws outer border");
  check(pixel_at(buffer, width, 1, 1) == debugColor, "debug line width draws inner border");
}

void test_debug_tiles_line_width_zero() {
  RenderBatch batch;
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(0);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  check(pixel_at(buffer, width, 0, 0) == debugColor, "debug line width 0 defaults to 1");
}

void test_gradient_clip() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(0);
  batch.rects.y0.push_back(0);
  batch.rects.x1.push_back(10);
  batch.rects.y1.push_back(10);
  uint8_t blackIndex = palette_index(batch, PackRGBA8(Color{0, 0, 0, 255}));
  uint8_t whiteIndex = palette_index(batch, PackRGBA8(Color{255, 255, 255, 255}));
  batch.rects.colorIndex.push_back(blackIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagGradient | RectFlagClip);
  batch.rects.gradientColor1Index.push_back(whiteIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(256);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(5);
  batch.rects.clipY1.push_back(5);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 10;
  uint32_t height = 10;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 7, 7) == black, "gradient clip prevents outside draw");
}

void test_determinism() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));
  add_rect(batch, 2, 2, 6, 6, PackRGBA8(Color{200, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 2;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 2;
  bitmap.stride = 2;
  bitmap.pixels = {255, 255, 255, 255};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);
  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);
  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);
  add_text(batch, 1, 1, 2, 2, PackRGBA8(Color{0, 200, 0, 255}), 0);

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> bufferA(width * height * 4, 0);
  std::vector<uint8_t> bufferB(width * height * 4, 0);
  RenderTarget targetA{std::span<uint8_t>(bufferA), width, height, width * 4};
  RenderTarget targetB{std::span<uint8_t>(bufferB), width, height, width * 4};

  render_batch(targetA, batch);
  render_batch(targetB, batch);

  check(buffers_equal(bufferA, bufferB), "deterministic output for same batch");
}

void test_text_atlas() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphAtlas atlas;
  atlas.width = 4;
  atlas.height = 4;
  atlas.stride = 4;
  atlas.pixels.resize(static_cast<size_t>(atlas.height) * atlas.stride, 0);
  atlas.pixels[1 * atlas.stride + 1] = 255;
  batch.glyphs.atlases.push_back(atlas);

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 2;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 2;
  bitmap.stride = 2;
  bitmap.atlasIndex = 0;
  bitmap.atlasX = 0;
  bitmap.atlasY = 0;
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, 1, 1, 2, 2, PackRGBA8(Color{0, 200, 200, 255}), 0);

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{0, 200, 200, 255});
  check(pixel_at(buffer, width, 2, 2) == expected, "text atlas pixel draws");
}

void test_text_atlas_offset() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphAtlas atlas;
  atlas.width = 4;
  atlas.height = 4;
  atlas.stride = 4;
  atlas.pixels.resize(static_cast<size_t>(atlas.height) * atlas.stride, 0);
  atlas.pixels[2 * atlas.stride + 2] = 255;
  batch.glyphs.atlases.push_back(atlas);

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 2;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 2;
  bitmap.stride = 2;
  bitmap.atlasIndex = 0;
  bitmap.atlasX = 1;
  bitmap.atlasY = 1;
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, 1, 1, 2, 2, PackRGBA8(Color{200, 100, 0, 255}), 0);

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{200, 100, 0, 255});
  check(pixel_at(buffer, width, 2, 2) == expected, "text atlas offset draws");
}

void test_stride_padding_preserved() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{50, 60, 70, 255}));

  uint32_t width = 4;
  uint32_t height = 3;
  uint32_t stride = width * 4 + 8;
  std::vector<uint8_t> buffer(static_cast<size_t>(stride) * height, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, stride};

  render_batch(target, batch);

  for (uint32_t y = 0; y < height; ++y) {
    size_t padStart = static_cast<size_t>(y) * stride + static_cast<size_t>(width) * 4;
    for (size_t i = padStart; i < padStart + 8; ++i) {
      check(buffer[i] == 0x7F, "stride padding preserved");
    }
  }
}

void test_invalid_indices_ignored() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.commands.push_back(RenderCommand{CommandType::Rect, 99});
  batch.commands.push_back(RenderCommand{CommandType::Text, 88});

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 0, 0) == expected, "invalid indices ignored");
}

void test_target_short_span() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));
  std::vector<uint8_t> buffer(4, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), 2, 2, 8};

  render_batch(target, batch);

  check(buffer[0] == 0x7F, "short span prevents write");
}

void test_random_fuzz_determinism() {
  RenderBatch batch;
  batch.tileSize = 32;
  add_clear(batch, PackRGBA8(Color{5, 5, 5, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 3;
  bitmap.height = 3;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 3;
  bitmap.stride = 3;
  bitmap.pixels = {
      0, 255, 0,
      255, 255, 255,
      0, 255, 0,
  };
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> posDist(0, 120);
  std::uniform_int_distribution<int32_t> sizeDist(4, 24);
  auto colors = build_test_colors();
  std::uniform_int_distribution<size_t> colorIndexDist(0, colors.size() - 1);
  std::uniform_int_distribution<uint32_t> dirDist(0, 255);
  std::uniform_int_distribution<int> flagDist(0, 1);

  for (uint32_t i = 0; i < 200; ++i) {
    int32_t x0 = posDist(rng);
    int32_t y0 = posDist(rng);
    int32_t x1 = x0 + sizeDist(rng);
    int32_t y1 = y0 + sizeDist(rng);
    uint32_t c0 = colors[colorIndexDist(rng)];
    uint32_t c1 = colors[colorIndexDist(rng)];

    if (flagDist(rng)) {
      add_gradient_rect_dir(batch, x0, y0, x1, y1, c0, c1,
                            static_cast<int16_t>(dirDist(rng)), static_cast<int16_t>(dirDist(rng)));
    } else {
      add_rect(batch, x0, y0, x1, y1, c0);
    }
  }

  for (uint32_t i = 0; i < 40; ++i) {
    batch.glyphs.glyphXQ8_8.push_back(static_cast<int16_t>(i * 2 * 256));
    batch.glyphs.glyphYQ8_8.push_back(0);
    batch.glyphs.bitmapIndex.push_back(0);
  }
  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(40);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  for (uint32_t i = 0; i < 20; ++i) {
    add_text(batch, posDist(rng), posDist(rng), 60, 12,
             PackRGBA8(Color{200, 200, 200, 255}), 0);
  }

  uint32_t width = 128;
  uint32_t height = 128;
  std::vector<uint8_t> bufferA(width * height * 4, 0);
  std::vector<uint8_t> bufferB(width * height * 4, 0);
  RenderTarget targetA{std::span<uint8_t>(bufferA), width, height, width * 4};
  RenderTarget targetB{std::span<uint8_t>(bufferB), width, height, width * 4};

  render_batch(targetA, batch);
  render_batch(targetB, batch);

  check(buffers_equal(bufferA, bufferB), "fuzz batch renders deterministically");
}

void test_multithread_stress() {
  RenderBatch batch;
  batch.tileSize = 16;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  std::mt19937 rng(7);
  std::uniform_int_distribution<int32_t> posDist(0, 200);
  std::uniform_int_distribution<int32_t> sizeDist(8, 32);

  for (uint32_t i = 0; i < 800; ++i) {
    int32_t x0 = posDist(rng);
    int32_t y0 = posDist(rng);
    int32_t x1 = x0 + sizeDist(rng);
    int32_t y1 = y0 + sizeDist(rng);
    add_rect(batch, x0, y0, x1, y1, PackRGBA8(Color{static_cast<uint8_t>(i % 255), 0, 0, 255}));
  }

  uint32_t width = 256;
  uint32_t height = 256;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  std::vector<uint8_t> reference;
  for (int i = 0; i < 8; ++i) {
    render_batch(target, batch);
    if (i == 0) {
      reference = buffer;
    } else {
      check(buffers_equal(buffer, reference), "stress render remains deterministic");
    }
  }
}

void test_empty_batch_no_change() {
  RenderBatch batch;
  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0x7F);
  std::vector<uint8_t> original = buffer;
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  check(buffers_equal(buffer, original), "empty batch leaves buffer unchanged");
}

void test_text_over_rect_order() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 1, 1, 5, 5, PackRGBA8(Color{0, 0, 255, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 2;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 2;
  bitmap.stride = 2;
  bitmap.pixels = {255, 255, 255, 255};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, 2, 2, 2, 2, PackRGBA8(Color{0, 255, 0, 255}), 0);

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t green = PackRGBA8(Color{0, 255, 0, 255});
  check(pixel_at(buffer, width, 2, 2) == green, "text draws after rect");
}

void test_large_text_across_tiles() {
  RenderBatch batch;
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 2;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 2;
  bitmap.stride = 2;
  bitmap.pixels = {255, 255, 255, 255};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  for (uint32_t i = 0; i < 32; ++i) {
    batch.glyphs.glyphXQ8_8.push_back(static_cast<int16_t>(i * 2 * 256));
    batch.glyphs.glyphYQ8_8.push_back(0);
    batch.glyphs.bitmapIndex.push_back(0);
  }
  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(32);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, 0, 4, 64, 4, PackRGBA8(Color{255, 255, 0, 255}), 0);

  uint32_t width = 32;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t yellow = PackRGBA8(Color{255, 255, 0, 255});
  check(pixel_at(buffer, width, 1, 5) == yellow, "large text draws in early tile");
  check(pixel_at(buffer, width, 24, 5) == yellow, "large text draws in later tile");
}

void test_random_clip_rotation_mix() {
  RenderBatch batch;
  batch.tileSize = 16;
  add_clear(batch, PackRGBA8(Color{3, 3, 3, 255}));

  std::mt19937 rng(99);
  std::uniform_int_distribution<int32_t> posDist(0, 120);
  std::uniform_int_distribution<int32_t> sizeDist(8, 30);
  auto colors = build_test_colors();
  std::uniform_int_distribution<size_t> colorIndexDist(0, colors.size() - 1);
  std::uniform_int_distribution<uint32_t> colorDist(0, 255);
  std::uniform_int_distribution<int> flagDist(0, 1);
  std::uniform_int_distribution<int16_t> rotDist(-256, 256);

  for (uint32_t i = 0; i < 200; ++i) {
    int32_t x0 = posDist(rng);
    int32_t y0 = posDist(rng);
    int32_t x1 = x0 + sizeDist(rng);
    int32_t y1 = y0 + sizeDist(rng);
    uint32_t c0 = colors[colorIndexDist(rng)];
    uint32_t c1 = colors[colorIndexDist(rng)];

    uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
    batch.rects.x0.push_back(x0);
    batch.rects.y0.push_back(y0);
    batch.rects.x1.push_back(x1);
    batch.rects.y1.push_back(y1);
    batch.rects.colorIndex.push_back(palette_index(batch, c0));
    batch.rects.radiusQ8_8.push_back(static_cast<uint16_t>(sizeDist(rng)));
    batch.rects.rotationQ8_8.push_back(rotDist(rng));
    batch.rects.zQ8_8.push_back(0);
    batch.rects.opacity.push_back(255);
    uint8_t flags = 0;
    if (flagDist(rng)) flags |= RectFlagGradient;
    if (flagDist(rng)) flags |= RectFlagClip;
    batch.rects.flags.push_back(flags);
    batch.rects.gradientColor1Index.push_back(palette_index(batch, c1));
    batch.rects.gradientDirX.push_back(static_cast<int16_t>(colorDist(rng)));
    batch.rects.gradientDirY.push_back(static_cast<int16_t>(colorDist(rng)));
    int32_t cx0 = x0 + 2;
    int32_t cy0 = y0 + 2;
    int32_t cx1 = x1 - 2;
    int32_t cy1 = y1 - 2;
    batch.rects.clipX0.push_back(cx0);
    batch.rects.clipY0.push_back(cy0);
    batch.rects.clipX1.push_back(cx1);
    batch.rects.clipY1.push_back(cy1);
    batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
  }

  uint32_t width = 128;
  uint32_t height = 128;
  std::vector<uint8_t> bufferA(width * height * 4, 0);
  std::vector<uint8_t> bufferB(width * height * 4, 0);
  RenderTarget targetA{std::span<uint8_t>(bufferA), width, height, width * 4};
  RenderTarget targetB{std::span<uint8_t>(bufferB), width, height, width * 4};

  render_batch(targetA, batch);
  render_batch(targetB, batch);

  check(buffers_equal(bufferA, bufferB), "clip+rotation mix deterministic");
}

void test_perf_smoke() {
  if (!std::getenv("PRIMEMANIFEST_PERF")) return;

  RenderBatch batch;
  batch.tileSize = 16;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  std::mt19937 rng(123);
  std::uniform_int_distribution<int32_t> posDist(0, 400);
  std::uniform_int_distribution<int32_t> sizeDist(6, 20);
  auto colors = build_test_colors();
  std::uniform_int_distribution<size_t> colorIndexDist(0, colors.size() - 1);

  for (uint32_t i = 0; i < 2000; ++i) {
    int32_t x0 = posDist(rng);
    int32_t y0 = posDist(rng);
    int32_t x1 = x0 + sizeDist(rng);
    int32_t y1 = y0 + sizeDist(rng);
    uint32_t c0 = colors[colorIndexDist(rng)];
    add_rect(batch, x0, y0, x1, y1, c0);
  }

  uint32_t width = 512;
  uint32_t height = 512;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 5; ++i) {
    render_batch(target, batch);
  }
  auto end = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration<double>(end - start).count();

  check(elapsed < 10.0, "perf smoke under 10s (guarded by PRIMEMANIFEST_PERF)");
}

void test_gradient_opacity() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect(batch, 0, 0, 10, 10,
                    PackRGBA8(Color{100, 0, 0, 255}),
                    PackRGBA8(Color{200, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.opacity.size()) - 1;
  batch.rects.opacity[idx] = 128;

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t red = channel_at(buffer, width, 5, 5, 0);
  check(red >= 49 && red <= 101, "gradient opacity applies");
}

void test_text_multiple_glyphs_spacing() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 1;
  bitmap.height = 1;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 1;
  bitmap.stride = 1;
  bitmap.pixels = {255};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  for (uint32_t i = 0; i < 3; ++i) {
    batch.glyphs.glyphXQ8_8.push_back(static_cast<int16_t>(i * 4 * 256));
    batch.glyphs.glyphYQ8_8.push_back(0);
    batch.glyphs.bitmapIndex.push_back(0);
  }
  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(3);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, 1, 1, 12, 4, PackRGBA8(Color{255, 255, 255, 255}), 0);

  uint32_t width = 16;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  check(pixel_at(buffer, width, 1, 1) == PackRGBA8(Color{255, 255, 255, 255}), "glyph 0 draws");
  check(pixel_at(buffer, width, 5, 1) == PackRGBA8(Color{255, 255, 255, 255}), "glyph 1 spaced");
  check(pixel_at(buffer, width, 9, 1) == PackRGBA8(Color{255, 255, 255, 255}), "glyph 2 spaced");
}

void test_rect_negative_clip_ignored() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(5);
  batch.rects.y1.push_back(5);
  uint8_t redIndex = palette_index(batch, PackRGBA8(Color{255, 0, 0, 255}));
  batch.rects.colorIndex.push_back(redIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagClip);
  batch.rects.gradientColor1Index.push_back(redIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(-10);
  batch.rects.clipY0.push_back(-10);
  batch.rects.clipX1.push_back(-5);
  batch.rects.clipY1.push_back(-5);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  check(pixel_at(buffer, width, 2, 2) == black, "negative clip prevents draw");
}

void test_text_scale() {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 1;
  bitmap.height = 1;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 1;
  bitmap.stride = 1;
  bitmap.pixels = {255};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(512);

  add_text(batch, 1, 1, 4, 4, PackRGBA8(Color{255, 255, 255, 255}), 0);

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  check(pixel_at(buffer, width, 1, 1) == PackRGBA8(Color{255, 255, 255, 255}), "scaled text draws at origin");
}

} // namespace

int main() {
  test_clear();
  test_clear_alpha();
  test_clear_last_wins();
  test_rect();
  test_text();
  test_gradient_rect();
  test_gradient_same_colors();
  test_gradient_dir_normalized();
  test_gradient_horizontal();
  test_debug_tiles();
  test_debug_tiles_dirty_only();
  test_debug_tiles_all();
  test_rect_clip();
  test_rect_clip_outside();
  test_text_clip();
  test_text_missing_bitmap();
  test_text_missing_run();
  test_rect_opacity_zero();
  test_rect_opacity_half();
  test_text_opacity_zero();
  test_rect_offscreen();
  test_text_offscreen();
  test_command_order();
  test_rect_rotation_draws();
  test_tile_size_zero_default();
  test_tile_size_large();
  test_tile_size_non_power_of_two();
  test_multi_tile_rect();
  test_debug_tiles_line_width();
  test_debug_tiles_line_width_zero();
  test_gradient_clip();
  test_determinism();
  test_text_atlas();
  test_text_atlas_offset();
  test_stride_padding_preserved();
  test_invalid_indices_ignored();
  test_target_short_span();
  test_random_fuzz_determinism();
  test_multithread_stress();
  test_empty_batch_no_change();
  test_text_over_rect_order();
  test_large_text_across_tiles();
  test_random_clip_rotation_mix();
  test_perf_smoke();
  test_gradient_opacity();
  test_text_multiple_glyphs_spacing();
  test_rect_negative_clip_ignored();
  test_text_scale();
  if (failures == 0) {
    std::cout << "All tests passed.\n";
  }
  return failures == 0 ? 0 : 1;
}
