#include "PrimeManifest/util/BitmapFont.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.bitmap_font");

TEST_CASE("space_is_blank") {
  CHECK_MESSAGE(!UiFontPixel(' ', 0, 0), "space has no pixels");
  CHECK_MESSAGE(!UiFontPixel(' ', 2, 3), "space has no pixels");
}

TEST_CASE("glyph_has_pixels") {
  bool any = false;
  for (int y = 0; y < UiFontHeight; ++y) {
    for (int x = 0; x < UiFontWidth; ++x) {
      if (UiFontPixel('A', x, y)) {
        any = true;
        break;
      }
    }
    if (any) break;
  }
  CHECK_MESSAGE(any, "glyph A has at least one pixel");
}

TEST_CASE("out_of_bounds_false") {
  CHECK_MESSAGE(!UiFontPixel('A', -1, 0), "x < 0 is false");
  CHECK_MESSAGE(!UiFontPixel('A', 0, -1), "y < 0 is false");
  CHECK_MESSAGE(!UiFontPixel('A', UiFontWidth, 0), "x >= width is false");
  CHECK_MESSAGE(!UiFontPixel('A', 0, UiFontHeight), "y >= height is false");
}

TEST_CASE("measure_text_scales") {
  auto empty = MeasureUiText("", 12.0f);
  CHECK_MESSAGE(empty.first == 0, "empty text width measures zero");
  CHECK_MESSAGE(empty.second == 0, "empty text height measures zero");

  auto zero = MeasureUiText("Hi", 0.0f);
  CHECK_MESSAGE(zero.first == 0, "zero size width measures zero");
  CHECK_MESSAGE(zero.second == 0, "zero size height measures zero");

  auto base = MeasureUiText("AA", static_cast<float>(UiFontHeight));
  CHECK_MESSAGE(base.first == UiFontAdvance * 2, "width scales with advance");
  CHECK_MESSAGE(base.second == UiFontHeight, "height equals font height");
}

TEST_SUITE_END();
