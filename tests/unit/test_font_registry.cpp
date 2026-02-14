#include "PrimeManifest/text/FontRegistry.hpp"
#include "PrimeManifest/util/BitmapFont.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(font_registry, measure_text_fallbacks) {
  Typography typography;
  typography.size = 12.0f;

  auto measured = MeasureText("Hi", typography);

#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  (void)measured;
#else
  auto expected = MeasureUiText("Hi", typography.size);
  PM_CHECK(measured.first == expected.first, "MeasureText width matches bitmap fallback");
  PM_CHECK(measured.second == expected.second, "MeasureText height matches bitmap fallback");
#endif
}

PM_TEST(font_registry, to_string_helpers) {
  PM_CHECK(ToString(FontSlant::Upright) == std::string_view("upright"), "slant upright string");
  PM_CHECK(ToString(FontSlant::Italic) == std::string_view("italic"), "slant italic string");
  PM_CHECK(ToString(FontSlant::Oblique) == std::string_view("oblique"), "slant oblique string");

  PM_CHECK(ToString(FontFallbackPolicy::BundleOnly) == std::string_view("bundle_only"),
           "fallback bundle_only string");
  PM_CHECK(ToString(FontFallbackPolicy::BundleThenOS) == std::string_view("bundle_then_os"),
           "fallback bundle_then_os string");
}

PM_TEST(font_registry, layout_text_handles_fonts_disabled) {
  Typography typography;
  typography.size = 14.0f;

  auto run = LayoutText("Hello", typography, 1.0f, false);

#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS
  (void)run;
#else
  PM_CHECK(!run, "LayoutText returns null when fonts disabled");
#endif
}
