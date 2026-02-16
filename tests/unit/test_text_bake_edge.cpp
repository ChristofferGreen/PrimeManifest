#include "PrimeManifest/text/TextBake.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.text_bake_edge");

TEST_CASE("append_text_run_empty_run") {
  RenderBatch batch;
  TextRun run;
  run.width = 3.0f;
  run.height = 4.0f;
  run.baseline = 2.0f;
  run.layoutScale = 1.0f;

  auto result = AppendTextRun(batch, run, 5, 6, 2);
  CHECK_MESSAGE(result.has_value(), "AppendTextRun returns result for empty run");
  CHECK_MESSAGE(batch.glyphs.size() == 0, "no glyphs emitted");
  CHECK_MESSAGE(batch.text.size() == 1, "text entry created");
}

TEST_CASE("append_text_run_clamps_dimensions") {
  RenderBatch batch;
  TextRun run;
  run.width = 100000.0f;
  run.height = 200000.0f;
  run.baseline = 0.0f;
  run.layoutScale = 1.0f;

  auto result = AppendTextRun(batch, run, 0, 0, 1);
  CHECK_MESSAGE(result.has_value(), "AppendTextRun returns result");
  CHECK_MESSAGE(batch.text.width[0] == 65535, "text width clamps to u16 max");
  CHECK_MESSAGE(batch.text.height[0] == 65535, "text height clamps to u16 max");
}

TEST_CASE("append_text_run_large_positions_preserved") {
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
  run.glyphs.push_back(GlyphPlacement{&glyph, 1, 200.0f, 0.0f});

  auto result = AppendTextRun(batch, run, 0, 0, 1);
  CHECK_MESSAGE(result.has_value(), "AppendTextRun returns result");
  CHECK_MESSAGE(batch.glyphs.glyphXQ8_8.size() == 1, "glyph emitted");
  CHECK_MESSAGE(batch.glyphs.glyphXQ8_8[0] == 200 * 256, "large glyph x preserved");
}

TEST_SUITE_END();
