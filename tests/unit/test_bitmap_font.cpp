#include "PrimeManifest/util/BitmapFont.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(bitmap_font, space_is_blank) {
  PM_CHECK(!UiFontPixel(' ', 0, 0), "space has no pixels");
  PM_CHECK(!UiFontPixel(' ', 2, 3), "space has no pixels");
}

PM_TEST(bitmap_font, glyph_has_pixels) {
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
  PM_CHECK(any, "glyph A has at least one pixel");
}

PM_TEST(bitmap_font, out_of_bounds_false) {
  PM_CHECK(!UiFontPixel('A', -1, 0), "x < 0 is false");
  PM_CHECK(!UiFontPixel('A', 0, -1), "y < 0 is false");
  PM_CHECK(!UiFontPixel('A', UiFontWidth, 0), "x >= width is false");
  PM_CHECK(!UiFontPixel('A', 0, UiFontHeight), "y >= height is false");
}

PM_TEST(bitmap_font, measure_text_scales) {
  auto empty = MeasureUiText("", 12.0f);
  PM_CHECK(empty.first == 0 && empty.second == 0, "empty text measures zero");

  auto zero = MeasureUiText("Hi", 0.0f);
  PM_CHECK(zero.first == 0 && zero.second == 0, "zero size measures zero");

  auto base = MeasureUiText("AA", static_cast<float>(UiFontHeight));
  PM_CHECK(base.first == UiFontAdvance * 2, "width scales with advance");
  PM_CHECK(base.second == UiFontHeight, "height equals font height");
}
