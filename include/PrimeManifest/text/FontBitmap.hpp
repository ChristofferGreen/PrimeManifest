#pragma once

#include <cstdint>
#include <vector>

namespace PrimeManifest {

enum class FontBitmapFormat : uint8_t {
  Gray8,
  Mono1,
  BGRA32,
};

struct FontBitmapView {
  const uint8_t* buffer = nullptr;
  int32_t width = 0;
  int32_t height = 0;
  int32_t pitch = 0;
  FontBitmapFormat format = FontBitmapFormat::Gray8;
};

bool ConvertFontBitmapToAlpha(FontBitmapView view,
                              std::vector<uint8_t>& outPixels,
                              int32_t& outStride);

} // namespace PrimeManifest
