#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "PrimeManifest/text/GlyphBitmapFormat.hpp"

namespace PrimeManifest {

struct GlyphAtlas;

struct GlyphBitmap {
  int32_t width = 0;
  int32_t height = 0;
  int32_t bearingX = 0;
  int32_t bearingY = 0;
  int32_t advance = 0;
  int32_t stride = 0;
  GlyphBitmapFormat format = GlyphBitmapFormat::Mask8;
  std::vector<uint8_t> pixels;
  std::shared_ptr<GlyphAtlas> atlas;
  int32_t atlasX = 0;
  int32_t atlasY = 0;
};

struct GlyphAtlas {
  int32_t width = 0;
  int32_t height = 0;
  int32_t stride = 0;
  int32_t cursorX = 0;
  int32_t cursorY = 0;
  int32_t rowHeight = 0;
  std::vector<uint8_t> pixels;
};

struct GlyphPlacement {
  GlyphBitmap* bitmap = nullptr;
  int32_t glyphId = 0;
  float x = 0.0f;
  float y = 0.0f;
};

struct TextRun {
  std::vector<GlyphPlacement> glyphs;
  float width = 0.0f;
  float height = 0.0f;
  float baseline = 0.0f;
  float layoutScale = 1.0f;
  uint64_t contentHash = 0;
};

} // namespace PrimeManifest
