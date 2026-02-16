#include "test_helpers.hpp"
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;


TEST_SUITE_BEGIN("primemanifest.ordering");

TEST_CASE("later_command_on_top") {
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
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 2) == red, "later command draws on top");
}

TEST_CASE("text_over_rect") {
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
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 2) == green, "text draws after rect");
}

TEST_SUITE_END();
