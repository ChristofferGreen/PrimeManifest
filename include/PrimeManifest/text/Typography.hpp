#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace PrimeManifest {

enum class FontSlant : uint8_t {
  Upright = 0,
  Italic,
  Oblique,
};

enum class FontFallbackPolicy : uint8_t {
  BundleOnly = 0,
  BundleThenOS,
};

struct Typography {
  std::string family = "default";
  float size = 16.0f;
  uint16_t weight = 400;
  FontSlant slant = FontSlant::Upright;
  float lineHeight = 0.0f;
  float letterSpacing = 0.0f;
  float wordSpacing = 0.0f;
  std::string features;
  std::string locale;
  FontFallbackPolicy fallback = FontFallbackPolicy::BundleThenOS;
};

auto ToString(FontSlant slant) -> std::string_view;
auto ToString(FontFallbackPolicy policy) -> std::string_view;

} // namespace PrimeManifest

inline auto PrimeManifest::ToString(FontSlant slant) -> std::string_view {
  switch (slant) {
    case FontSlant::Upright: return "upright";
    case FontSlant::Italic: return "italic";
    case FontSlant::Oblique: return "oblique";
  }
  return "upright";
}

inline auto PrimeManifest::ToString(FontFallbackPolicy policy) -> std::string_view {
  switch (policy) {
    case FontFallbackPolicy::BundleOnly: return "bundle_only";
    case FontFallbackPolicy::BundleThenOS: return "bundle_then_os";
  }
  return "bundle_then_os";
}
