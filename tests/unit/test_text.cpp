#include "test_helpers.hpp"
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;


TEST_SUITE_BEGIN("primemanifest.text");

TEST_CASE("draws_basic_glyph") {
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
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == expected, "text glyph draws pixel");
}

TEST_CASE("opaque_glyph_fast_path") {
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
  batch.glyphs.bitmapOpaque.push_back(1);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, 1, 1, 1, 1, PackRGBA8(Color{200, 100, 50, 255}), 0);

  uint32_t width = 3;
  uint32_t height = 3;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{200, 100, 50, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == expected, "opaque glyph fast path draws");
}

TEST_CASE("partial_coverage_blends") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 1;
  bitmap.height = 1;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 1;
  bitmap.stride = 1;
  bitmap.pixels = {128};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);

  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  add_text(batch, 1, 1, 1, 1, PackRGBA8(Color{255, 255, 255, 255}), 0);

  uint32_t width = 3;
  uint32_t height = 3;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{128, 128, 128, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == expected, "partial coverage blends to mid-gray");
}

TEST_CASE("opacity_scales_coverage") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 1;
  bitmap.height = 1;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 1;
  bitmap.stride = 1;
  bitmap.pixels = {128};
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
  batch.text.width.push_back(1);
  batch.text.height.push_back(1);
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(128);
  batch.text.colorIndex.push_back(palette_index(batch, PackRGBA8(Color{255, 255, 255, 255})));
  batch.text.flags.push_back(0);
  batch.text.runIndex.push_back(0);
  batch.text.clipX0.push_back(0);
  batch.text.clipY0.push_back(0);
  batch.text.clipX1.push_back(0);
  batch.text.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});

  uint32_t width = 3;
  uint32_t height = 3;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{64, 64, 64, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == expected, "opacity scales coverage");
}

TEST_CASE("clip_respected") {
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
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 2) == green, "text clip allows pixel");
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == black, "text clip rejects pixel");
}

TEST_CASE("missing_bitmap_skips_draw") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(1);

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
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == black, "missing bitmap skips draw");
}

TEST_CASE("missing_run_skips_draw") {
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
  batch.text.runIndex.push_back(5);
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
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == black, "missing run skips draw");
}

TEST_CASE("opacity_zero_skips") {
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
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == black, "text opacity zero skips draw");
}

TEST_CASE("offscreen_skips") {
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
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == black, "offscreen text skipped");
}

TEST_CASE("atlas_pixel_draws") {
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
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 2) == expected, "text atlas pixel draws");
}

TEST_CASE("atlas_offset_draws") {
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
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 2) == expected, "text atlas offset draws");
}

TEST_CASE("large_across_tiles") {
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
    batch.glyphs.glyphXQ8_8.push_back(static_cast<int32_t>(i * 2 * 256));
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
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 5) == yellow, "large text draws in early tile");
  CHECK_MESSAGE(pixel_at(buffer, width, 24, 5) == yellow, "large text draws in later tile");
}

TEST_CASE("multiple_glyph_spacing") {
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
    batch.glyphs.glyphXQ8_8.push_back(static_cast<int32_t>(i * 4 * 256));
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

  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == PackRGBA8(Color{255, 255, 255, 255}), "glyph 0 draws");
  CHECK_MESSAGE(pixel_at(buffer, width, 5, 1) == PackRGBA8(Color{255, 255, 255, 255}), "glyph 1 spaced");
  CHECK_MESSAGE(pixel_at(buffer, width, 9, 1) == PackRGBA8(Color{255, 255, 255, 255}), "glyph 2 spaced");
}

TEST_CASE("scale_applies") {
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

  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == PackRGBA8(Color{255, 255, 255, 255}),
                "scaled text draws at origin");
}

TEST_SUITE_END();
