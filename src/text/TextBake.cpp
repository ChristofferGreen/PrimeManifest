#include "PrimeManifest/text/TextBake.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace PrimeManifest {

namespace {

auto clamp_u16(uint32_t value) -> uint16_t {
  return static_cast<uint16_t>(std::min<uint32_t>(value, std::numeric_limits<uint16_t>::max()));
}

auto copy_bitmap(GlyphBitmap const& src) -> GlyphStore::GlyphBitmap {
  GlyphStore::GlyphBitmap out{};
  out.width = src.width;
  out.height = src.height;
  out.bearingX = src.bearingX;
  out.bearingY = src.bearingY;
  out.advance = src.advance;
  out.format = src.format;
  out.atlasIndex = -1;
  out.atlasX = 0;
  out.atlasY = 0;
  if (!src.pixels.empty()) {
    out.stride = src.stride > 0 ? src.stride : src.width;
    out.pixels = src.pixels;
    return out;
  }
  if (src.atlas && src.width > 0 && src.height > 0 && src.format == GlyphBitmapFormat::Mask8) {
    out.stride = src.width;
    out.pixels.resize(static_cast<size_t>(src.width) * src.height);
    int32_t atlasStride = src.atlas->stride;
    for (int32_t y = 0; y < src.height; ++y) {
      const uint8_t* srcRow = src.atlas->pixels.data() +
                              static_cast<size_t>(src.atlasY + y) * atlasStride +
                              static_cast<size_t>(src.atlasX);
      uint8_t* dstRow = out.pixels.data() + static_cast<size_t>(y) * out.stride;
      std::memcpy(dstRow, srcRow, static_cast<size_t>(src.width));
    }
  } else {
    out.stride = src.width;
  }
  return out;
}

auto bitmap_is_opaque(GlyphStore::GlyphBitmap const& bmp) -> bool {
  if (bmp.pixels.empty()) return false;
  if (bmp.format == GlyphBitmapFormat::Mask8) {
    for (uint8_t v : bmp.pixels) {
      if (v != 255u) return false;
    }
    return true;
  }
  if (bmp.format == GlyphBitmapFormat::ColorBGRA) {
    for (size_t i = 3; i < bmp.pixels.size(); i += 4) {
      if (bmp.pixels[i] != 255u) return false;
    }
    return true;
  }
  return false;
}

} // namespace

auto AppendTextRun(RenderBatch& batch,
                   TextRun const& run,
                   int32_t x,
                   int32_t y,
                   uint8_t colorIndex,
                   uint8_t opacity,
                   uint8_t flags) -> std::optional<TextBakeResult> {
  uint32_t glyphStart = static_cast<uint32_t>(batch.glyphs.glyphXQ8_8.size());
  std::unordered_map<GlyphBitmap const*, uint32_t> bitmapCache;
  bitmapCache.reserve(run.glyphs.size());

  for (auto const& glyph : run.glyphs) {
    if (!glyph.bitmap) continue;
    if (glyph.bitmap->width <= 0 || glyph.bitmap->height <= 0) continue;
    uint32_t bitmapIndex = 0;
    auto it = bitmapCache.find(glyph.bitmap);
    if (it != bitmapCache.end()) {
      bitmapIndex = it->second;
    } else {
      GlyphStore::GlyphBitmap copied = copy_bitmap(*glyph.bitmap);
      bitmapIndex = static_cast<uint32_t>(batch.glyphs.bitmaps.size());
      batch.glyphs.bitmaps.push_back(std::move(copied));
      batch.glyphs.bitmapOpaque.push_back(bitmap_is_opaque(batch.glyphs.bitmaps.back()) ? 1u : 0u);
      bitmapCache.emplace(glyph.bitmap, bitmapIndex);
    }

    int32_t gx = static_cast<int32_t>(std::lround(glyph.x * 256.0f));
    int32_t gy = static_cast<int32_t>(std::lround(glyph.y * 256.0f));
    batch.glyphs.glyphXQ8_8.push_back(gx);
    batch.glyphs.glyphYQ8_8.push_back(gy);
    batch.glyphs.bitmapIndex.push_back(bitmapIndex);
  }

  uint32_t glyphCount = static_cast<uint32_t>(batch.glyphs.glyphXQ8_8.size()) - glyphStart;
  uint32_t runIndex = static_cast<uint32_t>(batch.runs.glyphStart.size());
  batch.runs.glyphStart.push_back(glyphStart);
  batch.runs.glyphCount.push_back(glyphCount);
  batch.runs.baselineQ8_8.push_back(static_cast<int16_t>(std::lround(run.baseline * 256.0f)));
  float scale = run.layoutScale > 0.0f ? run.layoutScale : 1.0f;
  batch.runs.scaleQ8_8.push_back(clamp_u16(static_cast<uint32_t>(std::lround(scale * 256.0f))));

  uint32_t widthPx = static_cast<uint32_t>(std::ceil(std::max(0.0f, run.width) * scale));
  uint32_t heightPx = static_cast<uint32_t>(std::ceil(std::max(0.0f, run.height) * scale));
  uint32_t textIndex = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(static_cast<int16_t>(x));
  batch.text.y.push_back(static_cast<int16_t>(y));
  batch.text.width.push_back(clamp_u16(widthPx));
  batch.text.height.push_back(clamp_u16(heightPx));
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(opacity);
  batch.text.colorIndex.push_back(colorIndex);
  batch.text.flags.push_back(flags);
  batch.text.runIndex.push_back(runIndex);
  batch.text.clipX0.push_back(0);
  batch.text.clipY0.push_back(0);
  batch.text.clipX1.push_back(0);
  batch.text.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Text, textIndex});

  return TextBakeResult{textIndex, runIndex};
}

auto AppendText(RenderBatch& batch,
                std::string_view text,
                Typography const& typography,
                float deviceScale,
                int32_t x,
                int32_t y,
                uint8_t colorIndex,
                uint8_t opacity,
                uint8_t flags) -> std::optional<TextBakeResult> {
  auto run = LayoutText(text, typography, deviceScale, true);
  if (!run) return std::nullopt;
  return AppendTextRun(batch, *run, x, y, colorIndex, opacity, flags);
}

} // namespace PrimeManifest
