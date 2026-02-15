#include "PrimeManifest/text/TextBake.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(text_bake_edge, append_text_run_empty_run) {
  RenderBatch batch;
  TextRun run;
  run.width = 3.0f;
  run.height = 4.0f;
  run.baseline = 2.0f;
  run.layoutScale = 1.0f;

  auto result = AppendTextRun(batch, run, 5, 6, 2);
  PM_CHECK(result.has_value(), "AppendTextRun returns result for empty run");
  PM_CHECK(batch.glyphs.size() == 0, "no glyphs emitted");
  PM_CHECK(batch.text.size() == 1, "text entry created");
}

PM_TEST(text_bake_edge, append_text_run_clamps_dimensions) {
  RenderBatch batch;
  TextRun run;
  run.width = 100000.0f;
  run.height = 200000.0f;
  run.baseline = 0.0f;
  run.layoutScale = 1.0f;

  auto result = AppendTextRun(batch, run, 0, 0, 1);
  PM_CHECK(result.has_value(), "AppendTextRun returns result");
  PM_CHECK(batch.text.width[0] == 65535, "text width clamps to u16 max");
  PM_CHECK(batch.text.height[0] == 65535, "text height clamps to u16 max");
}

PM_TEST(text_bake_edge, append_text_run_large_positions_preserved) {
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
  PM_CHECK(result.has_value(), "AppendTextRun returns result");
  PM_CHECK(batch.glyphs.glyphXQ8_8.size() == 1, "glyph emitted");
  PM_CHECK(batch.glyphs.glyphXQ8_8[0] == 200 * 256, "large glyph x preserved");
}
