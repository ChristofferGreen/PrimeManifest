#include "PrimeManifest/text/TextBake.hpp"

#include "test_harness.hpp"

#include <memory>

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(text_bake, append_text_run_copies_bitmaps) {
  RenderBatch batch;

  GlyphBitmap glyph;
  glyph.width = 1;
  glyph.height = 1;
  glyph.bearingX = 0;
  glyph.bearingY = 0;
  glyph.advance = 1;
  glyph.stride = 1;
  glyph.pixels = {255};

  TextRun run;
  run.width = 5.0f;
  run.height = 4.0f;
  run.baseline = 3.0f;
  run.layoutScale = 2.0f;
  run.glyphs.push_back(GlyphPlacement{&glyph, 1, 1.5f, -2.5f});
  run.glyphs.push_back(GlyphPlacement{&glyph, 2, 2.25f, 0.0f});

  auto result = AppendTextRun(batch, run, 10, 20, 7);
  PM_CHECK(result.has_value(), "AppendTextRun returns result");
  PM_CHECK(batch.glyphs.bitmaps.size() == 1, "bitmap cache reuses glyph bitmap");
  PM_CHECK(batch.glyphs.bitmapOpaque.size() == 1, "bitmap opacity stored");
  PM_CHECK(batch.glyphs.bitmapOpaque[0] == 1, "opaque bitmap detected");
  PM_CHECK(batch.glyphs.glyphXQ8_8.size() == 2, "glyphs emitted");
  PM_CHECK(batch.glyphs.glyphXQ8_8[0] == 384, "glyph x quantized");
  PM_CHECK(batch.glyphs.glyphYQ8_8[0] == -640, "glyph y quantized");

  PM_CHECK(batch.runs.glyphStart.size() == 1, "run stored");
  PM_CHECK(batch.runs.glyphCount[0] == 2, "run glyph count set");
  PM_CHECK(batch.runs.baselineQ8_8[0] == 768, "baseline quantized");
  PM_CHECK(batch.runs.scaleQ8_8[0] == 512, "scale quantized");

  PM_CHECK(batch.text.width[0] == 10, "text width scaled");
  PM_CHECK(batch.text.height[0] == 8, "text height scaled");
  PM_CHECK(batch.text.x[0] == 10 && batch.text.y[0] == 20, "text position stored");
  PM_CHECK(batch.text.colorIndex[0] == 7, "text color index stored");
}

PM_TEST(text_bake, append_text_run_copies_atlas_pixels) {
  RenderBatch batch;

  auto atlas = std::make_shared<GlyphAtlas>();
  atlas->width = 4;
  atlas->height = 4;
  atlas->stride = 4;
  atlas->pixels.assign(static_cast<size_t>(atlas->height) * atlas->stride, 0);
  atlas->pixels[1 * atlas->stride + 1] = 10;
  atlas->pixels[1 * atlas->stride + 2] = 20;
  atlas->pixels[2 * atlas->stride + 1] = 30;
  atlas->pixels[2 * atlas->stride + 2] = 40;

  GlyphBitmap glyph;
  glyph.width = 2;
  glyph.height = 2;
  glyph.bearingX = 0;
  glyph.bearingY = 0;
  glyph.advance = 2;
  glyph.stride = 0;
  glyph.atlas = atlas;
  glyph.atlasX = 1;
  glyph.atlasY = 1;

  TextRun run;
  run.width = 2.0f;
  run.height = 2.0f;
  run.baseline = 1.0f;
  run.layoutScale = 1.0f;
  run.glyphs.push_back(GlyphPlacement{&glyph, 1, 0.0f, 0.0f});

  auto result = AppendTextRun(batch, run, 0, 0, 1);
  PM_CHECK(result.has_value(), "AppendTextRun returns result");
  PM_CHECK(batch.glyphs.bitmaps.size() == 1, "bitmap created from atlas");
  PM_CHECK(batch.glyphs.bitmaps[0].pixels.size() == 4, "atlas pixels copied");
  PM_CHECK(batch.glyphs.bitmaps[0].pixels[0] == 10, "atlas pixel (0,0)");
  PM_CHECK(batch.glyphs.bitmaps[0].pixels[1] == 20, "atlas pixel (1,0)");
  PM_CHECK(batch.glyphs.bitmaps[0].pixels[2] == 30, "atlas pixel (0,1)");
  PM_CHECK(batch.glyphs.bitmaps[0].pixels[3] == 40, "atlas pixel (1,1)");
}

PM_TEST(text_bake, append_text_run_skips_null_glyphs) {
  RenderBatch batch;

  GlyphBitmap glyph;
  glyph.width = 1;
  glyph.height = 1;
  glyph.bearingX = 0;
  glyph.bearingY = 0;
  glyph.advance = 1;
  glyph.stride = 1;
  glyph.pixels = {255};

  TextRun run;
  run.width = 2.0f;
  run.height = 2.0f;
  run.baseline = 1.0f;
  run.layoutScale = 1.0f;
  run.glyphs.push_back(GlyphPlacement{nullptr, 0, 0.0f, 0.0f});
  run.glyphs.push_back(GlyphPlacement{&glyph, 1, 1.0f, 0.0f});

  auto result = AppendTextRun(batch, run, 0, 0, 1);
  PM_CHECK(result.has_value(), "AppendTextRun returns result");
  PM_CHECK(batch.glyphs.glyphXQ8_8.size() == 1, "null glyph skipped");
  PM_CHECK(batch.runs.glyphCount[0] == 1, "glyph count matches emitted glyphs");
}

PM_TEST(text_bake, append_text_without_fonts_returns_nullopt) {
  RenderBatch batch;
  Typography typography;
  typography.size = 14.0f;

#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  (void)batch;
  (void)typography;
#else
  auto result = AppendText(batch, "Hello", typography, 1.0f, 0, 0, 0);
  PM_CHECK(!result.has_value(), "AppendText returns null when fonts disabled");
#endif
}
