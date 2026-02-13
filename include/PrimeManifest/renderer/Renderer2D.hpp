#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PrimeManifest {

struct Color {
  uint8_t r{};
  uint8_t g{};
  uint8_t b{};
  uint8_t a{};
};

struct IntRect {
  int32_t x0{};
  int32_t y0{};
  int32_t x1{};
  int32_t y1{};
};

enum class BlendType : uint8_t {
  HardUnion = 0,
  SmoothUnion = 1,
};

enum class CommandType : uint8_t {
  Clear = 0,
  Rect = 1,
  Text = 2,
  DebugTiles = 3,
};

struct RenderCommand {
  CommandType type{CommandType::Rect};
  uint32_t index = 0;
};

constexpr auto PackRGBA8(Color c) -> uint32_t {
  return static_cast<uint32_t>(c.r) |
         (static_cast<uint32_t>(c.g) << 8) |
         (static_cast<uint32_t>(c.b) << 16) |
         (static_cast<uint32_t>(c.a) << 24);
}

constexpr auto UnpackRGBA8(uint32_t rgba) -> Color {
  return Color{
      static_cast<uint8_t>(rgba & 0xFFu),
      static_cast<uint8_t>((rgba >> 8) & 0xFFu),
      static_cast<uint8_t>((rgba >> 16) & 0xFFu),
      static_cast<uint8_t>((rgba >> 24) & 0xFFu),
  };
}

enum RectFlags : uint8_t {
  RectFlagGradient = 1u << 0,
  RectFlagClip = 1u << 1,
  RectFlagSmoothBlend = 1u << 2,
};

enum TextFlags : uint8_t {
  TextFlagClip = 1u << 0,
};

enum DebugTilesFlags : uint8_t {
  DebugTilesFlagDirtyOnly = 1u << 0,
};

struct RenderTarget {
  std::span<uint8_t> data;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t strideBytes = 0;
};

struct ClearStore {
  std::vector<uint32_t> colorRGBA8;

  void clear() {
    colorRGBA8.clear();
  }
  size_t size() const {
    return colorRGBA8.size();
  }
};

struct RectStore {
  std::vector<int32_t> x0;
  std::vector<int32_t> y0;
  std::vector<int32_t> x1;
  std::vector<int32_t> y1;
  std::vector<uint32_t> colorRGBA8;
  std::vector<uint16_t> radiusQ8_8;
  std::vector<int16_t> rotationQ8_8;
  std::vector<int16_t> zQ8_8;
  std::vector<uint8_t> opacity;
  std::vector<uint8_t> flags;
  std::vector<uint32_t> gradientColor1RGBA8;
  std::vector<int16_t> gradientDirX;
  std::vector<int16_t> gradientDirY;
  std::vector<int32_t> clipX0;
  std::vector<int32_t> clipY0;
  std::vector<int32_t> clipX1;
  std::vector<int32_t> clipY1;

  void clear() {
    x0.clear();
    y0.clear();
    x1.clear();
    y1.clear();
    colorRGBA8.clear();
    radiusQ8_8.clear();
    rotationQ8_8.clear();
    zQ8_8.clear();
    opacity.clear();
    flags.clear();
    gradientColor1RGBA8.clear();
    gradientDirX.clear();
    gradientDirY.clear();
    clipX0.clear();
    clipY0.clear();
    clipX1.clear();
    clipY1.clear();
  }
  size_t size() const {
    return x0.size();
  }
};

struct TextStore {
  std::vector<int32_t> x;
  std::vector<int32_t> y;
  std::vector<int32_t> width;
  std::vector<int32_t> height;
  std::vector<int16_t> zQ8_8;
  std::vector<uint8_t> opacity;
  std::vector<uint32_t> colorRGBA8;
  std::vector<uint8_t> flags;
  std::vector<uint32_t> runIndex;
  std::vector<int32_t> clipX0;
  std::vector<int32_t> clipY0;
  std::vector<int32_t> clipX1;
  std::vector<int32_t> clipY1;

  void clear() {
    x.clear();
    y.clear();
    width.clear();
    height.clear();
    zQ8_8.clear();
    opacity.clear();
    colorRGBA8.clear();
    flags.clear();
    runIndex.clear();
    clipX0.clear();
    clipY0.clear();
    clipX1.clear();
    clipY1.clear();
  }
  size_t size() const {
    return x.size();
  }
};

struct TextRunStore {
  std::vector<uint32_t> glyphStart;
  std::vector<uint32_t> glyphCount;
  std::vector<int16_t> baselineQ8_8;
  std::vector<uint16_t> scaleQ8_8;

  void clear() {
    glyphStart.clear();
    glyphCount.clear();
    baselineQ8_8.clear();
    scaleQ8_8.clear();
  }
  size_t size() const {
    return glyphStart.size();
  }
};

struct GlyphStore {
  struct GlyphBitmap {
    int32_t width = 0;
    int32_t height = 0;
    int32_t bearingX = 0;
    int32_t bearingY = 0;
    int32_t advance = 0;
    int32_t stride = 0;
    int32_t atlasIndex = -1;
    int32_t atlasX = 0;
    int32_t atlasY = 0;
    std::vector<uint8_t> pixels;
  };

  struct GlyphAtlas {
    int32_t width = 0;
    int32_t height = 0;
    int32_t stride = 0;
    std::vector<uint8_t> pixels;
  };

  std::vector<int16_t> glyphXQ8_8;
  std::vector<int16_t> glyphYQ8_8;
  std::vector<uint32_t> bitmapIndex;
  std::vector<GlyphBitmap> bitmaps;
  std::vector<GlyphAtlas> atlases;

  void clear() {
    glyphXQ8_8.clear();
    glyphYQ8_8.clear();
    bitmapIndex.clear();
    bitmaps.clear();
    atlases.clear();
  }
  size_t size() const {
    return glyphXQ8_8.size();
  }
};

struct DebugTilesStore {
  std::vector<uint32_t> colorRGBA8;
  std::vector<uint8_t> lineWidth;
  std::vector<uint8_t> flags;

  void clear() {
    colorRGBA8.clear();
    lineWidth.clear();
    flags.clear();
  }
  size_t size() const {
    return colorRGBA8.size();
  }
};

struct RenderBatch {
  std::vector<RenderCommand> commands;
  ClearStore clear;
  RectStore rects;
  TextStore text;
  TextRunStore runs;
  GlyphStore glyphs;
  DebugTilesStore debugTiles;
  uint16_t tileSize = 64;

  void clearAll() {
    commands.clear();
    clear.clear();
    rects.clear();
    text.clear();
    runs.clear();
    glyphs.clear();
    debugTiles.clear();
  }
};

void Render(RenderTarget target, RenderBatch const& batch);
void Render(RenderTarget target, RenderBatch&& batch);

} // namespace PrimeManifest
