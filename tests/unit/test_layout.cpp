#include "PrimeManifest/text/TextLayout.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.layout");

TEST_CASE("defaults_are_zeroed") {
  TextRun run;
  CHECK_MESSAGE(run.glyphs.empty(), "glyph list starts empty");
  CHECK_MESSAGE(run.width == 0.0f, "width defaults to zero");
  CHECK_MESSAGE(run.height == 0.0f, "height defaults to zero");
  CHECK_MESSAGE(run.baseline == 0.0f, "baseline defaults to zero");
  CHECK_MESSAGE(run.layoutScale == 1.0f, "layout scale defaults to one");
  CHECK_MESSAGE(run.contentHash == 0u, "content hash defaults to zero");
}

TEST_SUITE_END();
