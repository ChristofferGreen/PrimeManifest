#include "PrimeManifest/text/TextLayout.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(layout, defaults_are_zeroed) {
  TextRun run;
  PM_CHECK(run.glyphs.empty(), "glyph list starts empty");
  PM_CHECK(run.width == 0.0f, "width defaults to zero");
  PM_CHECK(run.height == 0.0f, "height defaults to zero");
  PM_CHECK(run.baseline == 0.0f, "baseline defaults to zero");
  PM_CHECK(run.layoutScale == 1.0f, "layout scale defaults to one");
  PM_CHECK(run.contentHash == 0u, "content hash defaults to zero");
}
