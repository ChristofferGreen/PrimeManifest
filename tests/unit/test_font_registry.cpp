#include "PrimeManifest/text/FontRegistry.hpp"
#include "PrimeManifest/util/BitmapFont.hpp"

#include "test_harness.hpp"

#include <filesystem>

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
  PM_CHECK(ToString(static_cast<FontSlant>(255)) == std::string_view("upright"),
           "slant fallback string");

  PM_CHECK(ToString(FontFallbackPolicy::BundleOnly) == std::string_view("bundle_only"),
           "fallback bundle_only string");
  PM_CHECK(ToString(FontFallbackPolicy::BundleThenOS) == std::string_view("bundle_then_os"),
           "fallback bundle_then_os string");
  PM_CHECK(ToString(static_cast<FontFallbackPolicy>(255)) == std::string_view("bundle_then_os"),
           "fallback policy string");
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

PM_TEST(font_registry, registry_methods_noop_without_fonts) {
  FontRegistry registry;
  registry.addBundleDir("unused");
  registry.addOsFallbackDir("unused");
  registry.loadBundledFonts();
  registry.loadOsFallbackFonts();

  PM_CHECK(!registry.hasBundledFaces(), "registry reports no bundled faces");

  Typography typography;
  typography.size = 12.0f;

  auto run = registry.layoutText("Hi", typography, 1.0f, false);
  PM_CHECK(!run, "member layoutText returns null when fonts disabled");

  auto measured = registry.measureText("Hi", typography);
  auto expected = MeasureUiText("Hi", typography.size);
  PM_CHECK(measured.first == expected.first, "member MeasureText width matches bitmap fallback");
  PM_CHECK(measured.second == expected.second, "member MeasureText height matches bitmap fallback");

  auto& globalRegistry = GetFontRegistry();
  PM_CHECK(!globalRegistry.hasBundledFaces(), "global registry has no bundled faces");
}

PM_TEST(font_registry, emoji_fixed_sizes_load) {
#if defined(PRIMEMANIFEST_ENABLE_FONTS) && PRIMEMANIFEST_ENABLE_FONTS && defined(__APPLE__)
  std::filesystem::path emojiPath = "/System/Library/Fonts/Apple Color Emoji.ttc";
  if (!std::filesystem::exists(emojiPath)) {
    return;
  }

  auto& registry = GetFontRegistry();
  registry.addBundleDir(emojiPath.parent_path().string());
  registry.loadBundledFonts();

  Typography emoji;
  emoji.family = "Apple Color Emoji";
  emoji.size = 18.0f;
  emoji.weight = 400;
  emoji.fallback = FontFallbackPolicy::BundleThenOS;

  auto run = LayoutText("😀", emoji, 1.0f, true);
  PM_CHECK(run, "emoji layout returns run");
  if (!run) return;
  PM_CHECK(!run->glyphs.empty(), "emoji glyph emitted");
  if (!run->glyphs.empty()) {
    PM_CHECK(run->glyphs[0].bitmap != nullptr, "emoji glyph has bitmap");
  }
#endif
}
