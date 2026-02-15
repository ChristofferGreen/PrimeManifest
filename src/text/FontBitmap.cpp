#include "PrimeManifest/text/FontBitmap.hpp"

#include <algorithm>

namespace PrimeManifest {

bool ConvertFontBitmapToAlpha(FontBitmapView view,
                              std::vector<uint8_t>& outPixels,
                              int32_t& outStride) {
  outPixels.clear();
  outStride = 0;
  if (!view.buffer || view.width <= 0 || view.height <= 0) return false;

  outStride = view.width;
  outPixels.resize(static_cast<size_t>(view.width) * view.height);

  auto row_ptr = [&](int32_t y) -> const uint8_t* {
    int32_t pitch = view.pitch;
    if (pitch == 0) return nullptr;
    if (pitch > 0) {
      return view.buffer + static_cast<size_t>(y) * pitch;
    }
    int32_t absPitch = -pitch;
    return view.buffer + static_cast<size_t>(view.height - 1 - y) * absPitch;
  };

  switch (view.format) {
    case FontBitmapFormat::Gray8: {
      for (int32_t y = 0; y < view.height; ++y) {
        const uint8_t* src = row_ptr(y);
        if (!src) return false;
        uint8_t* dst = outPixels.data() + static_cast<size_t>(y) * outStride;
        std::copy_n(src, view.width, dst);
      }
      return true;
    }
    case FontBitmapFormat::Mono1: {
      for (int32_t y = 0; y < view.height; ++y) {
        const uint8_t* src = row_ptr(y);
        if (!src) return false;
        uint8_t* dst = outPixels.data() + static_cast<size_t>(y) * outStride;
        for (int32_t x = 0; x < view.width; ++x) {
          uint8_t byte = src[x / 8];
          uint8_t bit = 0x80u >> (x % 8);
          dst[x] = (byte & bit) ? 255u : 0u;
        }
      }
      return true;
    }
    case FontBitmapFormat::BGRA32: {
      for (int32_t y = 0; y < view.height; ++y) {
        const uint8_t* src = row_ptr(y);
        if (!src) return false;
        uint8_t* dst = outPixels.data() + static_cast<size_t>(y) * outStride;
        for (int32_t x = 0; x < view.width; ++x) {
          uint8_t b = src[x * 4 + 0];
          uint8_t g = src[x * 4 + 1];
          uint8_t r = src[x * 4 + 2];
          uint8_t a = src[x * 4 + 3];
          if (a == 0) {
            a = std::max(r, std::max(g, b));
          }
          dst[x] = a;
        }
      }
      return true;
    }
  }

  return false;
}

} // namespace PrimeManifest
