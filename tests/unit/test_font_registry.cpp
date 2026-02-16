#include "PrimeManifest/text/FontRegistry.hpp"
#include "PrimeManifest/util/BitmapFont.hpp"

#include "third_party/doctest.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

using namespace PrimeManifest;

namespace {

auto default_font_dirs() -> std::vector<std::filesystem::path> {
  std::vector<std::filesystem::path> dirs;
#if defined(__APPLE__)
  dirs.emplace_back("/System/Library/Fonts");
  dirs.emplace_back("/Library/Fonts");
  if (auto* home = std::getenv("HOME")) {
    dirs.emplace_back(std::filesystem::path(home) / "Library/Fonts");
  }
#elif defined(_WIN32)
  if (auto* windir = std::getenv("WINDIR")) {
    dirs.emplace_back(std::filesystem::path(windir) / "Fonts");
  } else {
    dirs.emplace_back("C:\\Windows\\Fonts");
  }
#else
  dirs.emplace_back("/usr/share/fonts");
  dirs.emplace_back("/usr/local/share/fonts");
  if (auto* home = std::getenv("HOME")) {
    dirs.emplace_back(std::filesystem::path(home) / ".local/share/fonts");
    dirs.emplace_back(std::filesystem::path(home) / ".fonts");
  }
#endif
  return dirs;
}

auto to_lower(std::string text) -> std::string {
  for (char& c : text) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return text;
}

auto find_system_font_file() -> std::optional<std::filesystem::path> {
  for (auto const& dir : default_font_dirs()) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) continue;
    for (auto const& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
      if (ec) break;
      if (!entry.is_regular_file()) continue;
      auto ext = to_lower(entry.path().extension().string());
      if (ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".otc") {
        return entry.path();
      }
    }
  }
  return std::nullopt;
}

auto write_psfont(std::filesystem::path const& bundlePath,
                  std::filesystem::path const& fontPath) -> bool {
  std::ifstream input(fontPath, std::ios::binary);
  if (!input.is_open()) return false;
  input.seekg(0, std::ios::end);
  auto size = input.tellg();
  if (size <= 0) return false;
  input.seekg(0, std::ios::beg);

  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  input.read(reinterpret_cast<char*>(bytes.data()), size);
  if (!input) return false;

  std::ofstream output(bundlePath, std::ios::binary);
  if (!output.is_open()) return false;

  constexpr uint8_t kMagic[8] = {'P','S','O','S','F','N','T','\0'};
  uint32_t version = 1;
  uint32_t count = 1;
  uint32_t nameLen = 0;
  uint32_t dataLen = static_cast<uint32_t>(bytes.size());

  output.write(reinterpret_cast<const char*>(kMagic), sizeof(kMagic));
  output.write(reinterpret_cast<const char*>(&version), sizeof(version));
  output.write(reinterpret_cast<const char*>(&count), sizeof(count));
  output.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
  output.write(reinterpret_cast<const char*>(&dataLen), sizeof(dataLen));
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(output);
}

} // namespace

TEST_SUITE_BEGIN("primemanifest.font_registry");

TEST_CASE("measure_text_fallbacks") {
  Typography typography;
  typography.size = 12.0f;
  typography.family = "bitmap";

  auto measured = MeasureText("Hi", typography);
  auto expected = MeasureUiText("Hi", typography.size);
  CHECK_MESSAGE(measured.first == expected.first, "MeasureText width matches bitmap fallback");
  CHECK_MESSAGE(measured.second == expected.second, "MeasureText height matches bitmap fallback");
}

TEST_CASE("to_string_helpers") {
  CHECK_MESSAGE(ToString(FontSlant::Upright) == std::string_view("upright"), "slant upright string");
  CHECK_MESSAGE(ToString(FontSlant::Italic) == std::string_view("italic"), "slant italic string");
  CHECK_MESSAGE(ToString(FontSlant::Oblique) == std::string_view("oblique"), "slant oblique string");
  CHECK_MESSAGE(ToString(static_cast<FontSlant>(255)) == std::string_view("upright"),
           "slant fallback string");

  CHECK_MESSAGE(ToString(FontFallbackPolicy::BundleOnly) == std::string_view("bundle_only"),
           "fallback bundle_only string");
  CHECK_MESSAGE(ToString(FontFallbackPolicy::BundleThenOS) == std::string_view("bundle_then_os"),
           "fallback bundle_then_os string");
  CHECK_MESSAGE(ToString(static_cast<FontFallbackPolicy>(255)) == std::string_view("bundle_then_os"),
           "fallback policy string");
}

TEST_CASE("default_bundle_fonts_available") {
  FontRegistry registry;
  registry.loadBundledFonts();
  CHECK_MESSAGE(registry.hasBundledFaces(), "default bundled faces available");
}

TEST_CASE("layout_text_bitmap_family_returns_null") {
  Typography typography;
  typography.size = 14.0f;
  typography.family = "bitmap";

  auto run = LayoutText("Hello", typography, 1.0f, false);
  CHECK_MESSAGE(!run, "LayoutText returns null for bitmap family");
}

TEST_CASE("layout_text_uses_os_fallback") {
  FontRegistry registry;
  std::string chosen;
  for (auto const& candidate : default_font_dirs()) {
    if (std::filesystem::exists(candidate)) {
      chosen = candidate.string();
      break;
    }
  }
  if (chosen.empty()) return;

  registry.addOsFallbackDir(chosen);

  Typography typography;
  typography.size = 12.0f;

  auto run = registry.layoutText("Hi", typography, 1.0f, true);
  CHECK_MESSAGE(run, "layout uses os fallback fonts");
  if (!run) return;
  CHECK_MESSAGE(!run->glyphs.empty(), "os fallback emits glyphs");
}

TEST_CASE("layout_text_empty_returns_run") {
  FontRegistry registry;
  Typography typography;
  typography.size = 16.0f;

  auto run = registry.layoutText("", typography, 2.0f, false);
  CHECK_MESSAGE(run, "empty text returns run");
  if (!run) return;
  CHECK_MESSAGE(run->layoutScale == 2.0f, "layout scale stored for empty text");
}

TEST_CASE("layout_text_scale_defaults") {
  FontRegistry registry;
  Typography typography;
  typography.size = 14.0f;

  auto run = registry.layoutText("Hi", typography, 0.0f, false);
  if (!run) return;
  CHECK_MESSAGE(run->layoutScale == doctest::Approx(1.0f), "zero scale defaults to 1");
}

TEST_CASE("layout_text_spacing_increases_width") {
  FontRegistry registry;
  Typography base;
  base.size = 14.0f;
  base.letterSpacing = 0.0f;
  base.wordSpacing = 0.0f;

  auto baseRun = registry.layoutText("A B", base, 1.0f, false);
  if (!baseRun) return;

  Typography spaced = base;
  spaced.letterSpacing = 0.5f;
  spaced.wordSpacing = 0.5f;
  spaced.features = "kern=1,liga=1";
  spaced.locale = "en";

  auto spacedRun = registry.layoutText("A B", spaced, 1.0f, false);
  CHECK_MESSAGE(spacedRun, "spacing run produced");
  if (!spacedRun) return;
  CHECK_MESSAGE(spacedRun->width >= baseRun->width, "spacing widens run");
}

TEST_CASE("bundle_psfont_loads_faces") {
  auto fontPath = find_system_font_file();
  if (!fontPath) return;

  std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "primemanifest_font_bundle";
  std::error_code ec;
  std::filesystem::remove_all(tempDir, ec);
  std::filesystem::create_directories(tempDir, ec);
  if (ec) return;

  auto bundlePath = tempDir / "bundle.psfont";
  if (!write_psfont(bundlePath, *fontPath)) return;

  FontRegistry registry;
  registry.addBundleDir(tempDir.string());
  registry.loadBundledFonts();
  CHECK_MESSAGE(registry.hasBundledFaces(), "bundle produced faces");

  Typography typography;
  typography.size = 12.0f;
  auto run = registry.layoutText("Hi", typography, 1.0f, false);
  CHECK_MESSAGE(run, "layout from bundle succeeds");
}

TEST_CASE("emoji_fixed_sizes_load") {
#if defined(__APPLE__)
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
  CHECK_MESSAGE(run, "emoji layout returns run");
  if (!run) return;
  CHECK_MESSAGE(!run->glyphs.empty(), "emoji glyph emitted");
  if (!run->glyphs.empty()) {
    CHECK_MESSAGE(run->glyphs[0].bitmap != nullptr, "emoji glyph has bitmap");
  }
#endif
}

TEST_SUITE_END();
