#include "PrimeManifest/text/TextBake.hpp"

#include "third_party/doctest.h"

#include <memory>

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.text_bake");

TEST_CASE("append_text_run_copies_bitmaps") {
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
  CHECK_MESSAGE(result.has_value(), "AppendTextRun returns result");
  CHECK_MESSAGE(batch.glyphs.bitmaps.size() == 1, "bitmap cache reuses glyph bitmap");
  CHECK_MESSAGE(batch.glyphs.bitmapOpaque.size() == 1, "bitmap opacity stored");
  CHECK_MESSAGE(batch.glyphs.bitmapOpaque[0] == 1, "opaque bitmap detected");
  CHECK_MESSAGE(batch.glyphs.glyphXQ8_8.size() == 2, "glyphs emitted");
  CHECK_MESSAGE(batch.glyphs.glyphXQ8_8[0] == 384, "glyph x quantized");
  CHECK_MESSAGE(batch.glyphs.glyphYQ8_8[0] == -640, "glyph y quantized");

  CHECK_MESSAGE(batch.runs.glyphStart.size() == 1, "run stored");
  CHECK_MESSAGE(batch.runs.glyphCount[0] == 2, "run glyph count set");
  CHECK_MESSAGE(batch.runs.baselineQ8_8[0] == 768, "baseline quantized");
  CHECK_MESSAGE(batch.runs.scaleQ8_8[0] == 512, "scale quantized");

  CHECK_MESSAGE(batch.text.width[0] == 10, "text width scaled");
  CHECK_MESSAGE(batch.text.height[0] == 8, "text height scaled");
  CHECK_MESSAGE(batch.text.x[0] == 10, "text x position stored");
  CHECK_MESSAGE(batch.text.y[0] == 20, "text y position stored");
  CHECK_MESSAGE(batch.text.colorIndex[0] == 7, "text color index stored");
}

TEST_CASE("append_text_run_copies_atlas_pixels") {
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
  CHECK_MESSAGE(result.has_value(), "AppendTextRun returns result");
  CHECK_MESSAGE(batch.glyphs.bitmaps.size() == 1, "bitmap created from atlas");
  CHECK_MESSAGE(batch.glyphs.bitmaps[0].pixels.size() == 4, "atlas pixels copied");
  CHECK_MESSAGE(batch.glyphs.bitmaps[0].pixels[0] == 10, "atlas pixel (0,0)");
  CHECK_MESSAGE(batch.glyphs.bitmaps[0].pixels[1] == 20, "atlas pixel (1,0)");
  CHECK_MESSAGE(batch.glyphs.bitmaps[0].pixels[2] == 30, "atlas pixel (0,1)");
  CHECK_MESSAGE(batch.glyphs.bitmaps[0].pixels[3] == 40, "atlas pixel (1,1)");
}

TEST_CASE("append_text_run_skips_null_glyphs") {
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
  CHECK_MESSAGE(result.has_value(), "AppendTextRun returns result");
  CHECK_MESSAGE(batch.glyphs.glyphXQ8_8.size() == 1, "null glyph skipped");
  CHECK_MESSAGE(batch.runs.glyphCount[0] == 1, "glyph count matches emitted glyphs");
}

TEST_CASE("append_text_run_empty_bitmap_stride") {
  RenderBatch batch;

  GlyphBitmap glyph;
  glyph.width = 2;
  glyph.height = 2;
  glyph.bearingX = 0;
  glyph.bearingY = 0;
  glyph.advance = 2;
  glyph.stride = 0;
  glyph.format = GlyphBitmapFormat::Mask8;

  TextRun run;
  run.width = 2.0f;
  run.height = 2.0f;
  run.baseline = 1.0f;
  run.layoutScale = 1.0f;
  run.glyphs.push_back(GlyphPlacement{&glyph, 1, 0.0f, 0.0f});

  auto result = AppendTextRun(batch, run, 0, 0, 1);
  CHECK_MESSAGE(result.has_value(), "AppendTextRun returns result");
  CHECK_MESSAGE(batch.glyphs.bitmaps.size() == 1, "bitmap stored");
  CHECK_MESSAGE(batch.glyphs.bitmaps[0].stride == 2, "empty bitmap stride uses width");
  CHECK_MESSAGE(batch.glyphs.bitmapOpaque[0] == 0, "empty bitmap not opaque");
}

TEST_CASE("append_text_run_color_bgra_opacity") {
  RenderBatch batch;

  GlyphBitmap glyph;
  glyph.width = 1;
  glyph.height = 1;
  glyph.bearingX = 0;
  glyph.bearingY = 0;
  glyph.advance = 1;
  glyph.stride = 4;
  glyph.format = GlyphBitmapFormat::ColorBGRA;
  glyph.pixels = {10, 20, 30, 255};

  TextRun run;
  run.width = 1.0f;
  run.height = 1.0f;
  run.baseline = 1.0f;
  run.layoutScale = 1.0f;
  run.glyphs.push_back(GlyphPlacement{&glyph, 1, 0.0f, 0.0f});

  auto result = AppendTextRun(batch, run, 0, 0, 1);
  CHECK_MESSAGE(result.has_value(), "AppendTextRun returns result");
  CHECK_MESSAGE(batch.glyphs.bitmapOpaque[0] == 1, "bgra bitmap opaque when alpha full");
}

TEST_CASE("append_text_run_unknown_format_opacity") {
  RenderBatch batch;

  GlyphBitmap glyph;
  glyph.width = 1;
  glyph.height = 1;
  glyph.bearingX = 0;
  glyph.bearingY = 0;
  glyph.advance = 1;
  glyph.stride = 1;
  glyph.format = static_cast<GlyphBitmapFormat>(255);
  glyph.pixels = {255};

  TextRun run;
  run.width = 1.0f;
  run.height = 1.0f;
  run.baseline = 1.0f;
  run.layoutScale = 1.0f;
  run.glyphs.push_back(GlyphPlacement{&glyph, 1, 0.0f, 0.0f});

  auto result = AppendTextRun(batch, run, 0, 0, 1);
  CHECK_MESSAGE(result.has_value(), "AppendTextRun returns result");
  CHECK_MESSAGE(batch.glyphs.bitmapOpaque[0] == 0, "unknown format not opaque");
}

TEST_CASE("append_text_bitmap_family_returns_nullopt") {
  RenderBatch batch;
  Typography typography;
  typography.size = 14.0f;
  typography.family = "bitmap";
  auto result = AppendText(batch, "Hello", typography, 1.0f, 0, 0, 0);
  CHECK_MESSAGE(!result.has_value(), "AppendText returns null for bitmap family");
}

TEST_SUITE_END();
