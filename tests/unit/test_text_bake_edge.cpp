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
