#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <string_view>
#include <vector>

using namespace PrimeManifest;

namespace {
struct BenchConfig {
  uint32_t width = 1280;
  uint32_t height = 720;
  uint32_t rectCount = 4000;
  uint32_t textCount = 200;
  uint32_t frames = 300;
  uint16_t tileSize = 32;
  uint16_t rectRadius = 4;
  bool enableText = true;
  bool enableDebugTiles = false;
  bool useTileStream = false;
  uint32_t seed = 1337;
};

auto parse_u32(char const* s, uint32_t fallback) -> uint32_t {
  if (!s) return fallback;
  char* end = nullptr;
  auto v = std::strtoul(s, &end, 10);
  if (end == s) return fallback;
  return static_cast<uint32_t>(v);
}

auto parse_args(int argc, char** argv) -> BenchConfig {
  BenchConfig cfg;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);
    auto next = [&](uint32_t fallback) {
      if (i + 1 >= argc) return fallback;
      return parse_u32(argv[++i], fallback);
    };
    if (arg == "--width") {
      cfg.width = next(cfg.width);
    } else if (arg == "--height") {
      cfg.height = next(cfg.height);
    } else if (arg == "--rects") {
      cfg.rectCount = next(cfg.rectCount);
    } else if (arg == "--texts") {
      cfg.textCount = next(cfg.textCount);
    } else if (arg == "--frames") {
      cfg.frames = next(cfg.frames);
    } else if (arg == "--tile") {
      cfg.tileSize = static_cast<uint16_t>(next(cfg.tileSize));
    } else if (arg == "--radius") {
      cfg.rectRadius = static_cast<uint16_t>(next(cfg.rectRadius));
    } else if (arg == "--no-text") {
      cfg.enableText = false;
    } else if (arg == "--debug-tiles") {
      cfg.enableDebugTiles = true;
    } else if (arg == "--tile-stream") {
      cfg.useTileStream = true;
    } else if (arg == "--seed") {
      cfg.seed = next(cfg.seed);
    }
  }
  return cfg;
}

auto hsv_to_rgb(float h, float s, float v) -> Color {
  float c = v * s;
  float hPrime = std::fmod(h / 60.0f, 6.0f);
  float x = c * (1.0f - std::abs(std::fmod(hPrime, 2.0f) - 1.0f));
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  if (hPrime < 1.0f) {
    r = c;
    g = x;
  } else if (hPrime < 2.0f) {
    r = x;
    g = c;
  } else if (hPrime < 3.0f) {
    g = c;
    b = x;
  } else if (hPrime < 4.0f) {
    g = x;
    b = c;
  } else if (hPrime < 5.0f) {
    r = x;
    b = c;
  } else {
    r = c;
    b = x;
  }
  float m = v - c;
  return Color{static_cast<uint8_t>((r + m) * 255.0f),
               static_cast<uint8_t>((g + m) * 255.0f),
               static_cast<uint8_t>((b + m) * 255.0f),
               255};
}

auto build_palette() -> std::array<uint32_t, 256> {
  std::array<uint32_t, 256> palette{};
  constexpr uint32_t rainbowCount = 192;
  constexpr uint32_t grayCount = 64;
  for (uint32_t i = 0; i < rainbowCount; ++i) {
    float h = (360.0f * static_cast<float>(i)) / static_cast<float>(rainbowCount);
    Color c = hsv_to_rgb(h, 1.0f, 1.0f);
    palette[i] = PackRGBA8(c);
  }
  for (uint32_t i = 0; i < grayCount; ++i) {
    uint8_t v = static_cast<uint8_t>((i * 255u) / (grayCount - 1));
    palette[rainbowCount + i] = PackRGBA8(Color{v, v, v, 255});
  }
  return palette;
}

void add_clear(RenderBatch& batch, uint8_t colorIndex) {
  uint32_t idx = static_cast<uint32_t>(batch.clear.colorIndex.size());
  batch.clear.colorIndex.push_back(colorIndex);
  batch.commands.push_back(RenderCommand{CommandType::Clear, idx});
}

void add_rect(RenderBatch& batch,
              int32_t x0,
              int32_t y0,
              int32_t x1,
              int32_t y1,
              uint8_t colorIndex,
              uint8_t gradientIndex,
              bool gradient,
              uint16_t radiusQ8_8) {
  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(static_cast<int16_t>(x0));
  batch.rects.y0.push_back(static_cast<int16_t>(y0));
  batch.rects.x1.push_back(static_cast<int16_t>(x1));
  batch.rects.y1.push_back(static_cast<int16_t>(y1));
  batch.rects.colorIndex.push_back(colorIndex);
  batch.rects.radiusQ8_8.push_back(radiusQ8_8);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  uint8_t flags = 0;
  if (gradient) {
    flags |= RectFlagGradient;
    batch.rects.gradientColor1Index.push_back(gradientIndex);
    batch.rects.gradientDirX.push_back(static_cast<int16_t>(0));
    batch.rects.gradientDirY.push_back(static_cast<int16_t>(256));
  } else {
    batch.rects.gradientColor1Index.push_back(colorIndex);
    batch.rects.gradientDirX.push_back(0);
    batch.rects.gradientDirY.push_back(0);
  }
  batch.rects.flags.push_back(flags);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
}

void add_text(RenderBatch& batch,
              int32_t x,
              int32_t y,
              int32_t width,
              int32_t height,
              uint8_t colorIndex,
              uint32_t runIndex) {
  uint32_t idx = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(static_cast<int16_t>(x));
  batch.text.y.push_back(static_cast<int16_t>(y));
  batch.text.width.push_back(static_cast<uint16_t>(width));
  batch.text.height.push_back(static_cast<uint16_t>(height));
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(255);
  batch.text.colorIndex.push_back(colorIndex);
  batch.text.flags.push_back(0);
  batch.text.runIndex.push_back(runIndex);
  batch.text.clipX0.push_back(0);
  batch.text.clipY0.push_back(0);
  batch.text.clipX1.push_back(0);
  batch.text.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});
}

void add_debug_tiles(RenderBatch& batch, uint8_t colorIndex) {
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(colorIndex);
  batch.debugTiles.lineWidth.push_back(1);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});
}

void build_tile_stream(RenderBatch& batch, uint32_t width, uint32_t height) {
  uint32_t tileSize = batch.tileSize == 0 ? 32u : batch.tileSize;
  if (tileSize > 256u) {
    batch.tileStream.clear();
    return;
  }
  uint32_t tilesX = (width + tileSize - 1) / tileSize;
  uint32_t tilesY = (height + tileSize - 1) / tileSize;
  uint32_t tileCount = tilesX * tilesY;
  if (tileCount == 0) return;

  std::vector<uint32_t> tileCounts(tileCount, 0);

  auto count_tiles = [&](int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (x1 <= 0 || y1 <= 0) return;
    if (x0 >= static_cast<int32_t>(width) || y0 >= static_cast<int32_t>(height)) return;
    int32_t clampedX0 = std::max(x0, 0);
    int32_t clampedY0 = std::max(y0, 0);
    int32_t clampedX1 = std::min(x1, static_cast<int32_t>(width));
    int32_t clampedY1 = std::min(y1, static_cast<int32_t>(height));
    if (clampedX1 <= clampedX0 || clampedY1 <= clampedY0) return;
    uint32_t tx0 = static_cast<uint32_t>(clampedX0) / tileSize;
    uint32_t ty0 = static_cast<uint32_t>(clampedY0) / tileSize;
    uint32_t tx1 = static_cast<uint32_t>(clampedX1 - 1) / tileSize;
    uint32_t ty1 = static_cast<uint32_t>(clampedY1 - 1) / tileSize;
    for (uint32_t ty = ty0; ty <= ty1; ++ty) {
      for (uint32_t tx = tx0; tx <= tx1; ++tx) {
        tileCounts[ty * tilesX + tx] += 1;
      }
    }
  };

  for (uint32_t i = 0; i < batch.rects.x0.size(); ++i) {
    int32_t x0 = batch.rects.x0[i];
    int32_t y0 = batch.rects.y0[i];
    int32_t x1 = batch.rects.x1[i];
    int32_t y1 = batch.rects.y1[i];
    count_tiles(x0, y0, x1, y1);
  }

  for (uint32_t i = 0; i < batch.text.x.size(); ++i) {
    int32_t x0 = batch.text.x[i];
    int32_t y0 = batch.text.y[i];
    int32_t x1 = x0 + batch.text.width[i];
    int32_t y1 = y0 + batch.text.height[i];
    count_tiles(x0, y0, x1, y1);
  }

  batch.tileStream.offsets.assign(tileCount + 1, 0);
  for (uint32_t i = 0; i < tileCount; ++i) {
    batch.tileStream.offsets[i + 1] = batch.tileStream.offsets[i] + tileCounts[i];
  }
  batch.tileStream.commands.assign(batch.tileStream.offsets.back(), TileCommand{});
  std::vector<uint32_t> tileFill(tileCount, 0);

  auto emit_tiles = [&](CommandType type, uint32_t index, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (x1 <= 0 || y1 <= 0) return;
    if (x0 >= static_cast<int32_t>(width) || y0 >= static_cast<int32_t>(height)) return;
    int32_t clampedX0 = std::max(x0, 0);
    int32_t clampedY0 = std::max(y0, 0);
    int32_t clampedX1 = std::min(x1, static_cast<int32_t>(width));
    int32_t clampedY1 = std::min(y1, static_cast<int32_t>(height));
    if (clampedX1 <= clampedX0 || clampedY1 <= clampedY0) return;
    uint32_t tx0 = static_cast<uint32_t>(clampedX0) / tileSize;
    uint32_t ty0 = static_cast<uint32_t>(clampedY0) / tileSize;
    uint32_t tx1 = static_cast<uint32_t>(clampedX1 - 1) / tileSize;
    uint32_t ty1 = static_cast<uint32_t>(clampedY1 - 1) / tileSize;
    for (uint32_t ty = ty0; ty <= ty1; ++ty) {
      for (uint32_t tx = tx0; tx <= tx1; ++tx) {
        int32_t tileX0 = static_cast<int32_t>(tx * tileSize);
        int32_t tileY0 = static_cast<int32_t>(ty * tileSize);
        int32_t tileX1 = std::min(tileX0 + static_cast<int32_t>(tileSize), static_cast<int32_t>(width));
        int32_t tileY1 = std::min(tileY0 + static_cast<int32_t>(tileSize), static_cast<int32_t>(height));
        int32_t ix0 = std::max(x0, tileX0);
        int32_t iy0 = std::max(y0, tileY0);
        int32_t ix1 = std::min(x1, tileX1);
        int32_t iy1 = std::min(y1, tileY1);
        if (ix1 <= ix0 || iy1 <= iy0) continue;
        int32_t localX0 = ix0 - tileX0;
        int32_t localY0 = iy0 - tileY0;
        int32_t localW = ix1 - ix0;
        int32_t localH = iy1 - iy0;
        if (localX0 < 0 || localY0 < 0) continue;
        if (localW <= 0 || localH <= 0) continue;
        if (localX0 > 255 || localY0 > 255) continue;
        if (localW > 256 || localH > 256) continue;
        uint32_t tileIdx = ty * tilesX + tx;
        uint32_t offset = batch.tileStream.offsets[tileIdx] + tileFill[tileIdx]++;
        TileCommand cmd;
        cmd.type = type;
        cmd.index = index;
        cmd.x = static_cast<uint8_t>(localX0);
        cmd.y = static_cast<uint8_t>(localY0);
        cmd.wMinus1 = static_cast<uint8_t>(localW - 1);
        cmd.hMinus1 = static_cast<uint8_t>(localH - 1);
        batch.tileStream.commands[offset] = cmd;
      }
    }
  };

  for (uint32_t i = 0; i < batch.rects.x0.size(); ++i) {
    int32_t x0 = batch.rects.x0[i];
    int32_t y0 = batch.rects.y0[i];
    int32_t x1 = batch.rects.x1[i];
    int32_t y1 = batch.rects.y1[i];
    emit_tiles(CommandType::Rect, i, x0, y0, x1, y1);
  }

  for (uint32_t i = 0; i < batch.text.x.size(); ++i) {
    int32_t x0 = batch.text.x[i];
    int32_t y0 = batch.text.y[i];
    int32_t x1 = x0 + batch.text.width[i];
    int32_t y1 = y0 + batch.text.height[i];
    emit_tiles(CommandType::Text, i, x0, y0, x1, y1);
  }

  batch.tileStream.enabled = true;
}

void build_text_run(RenderBatch& batch, uint32_t glyphCount) {
  uint32_t start = static_cast<uint32_t>(batch.glyphs.glyphXQ8_8.size());
  for (uint32_t i = 0; i < glyphCount; ++i) {
    batch.glyphs.glyphXQ8_8.push_back(static_cast<int16_t>(i * 10 * 256));
    batch.glyphs.glyphYQ8_8.push_back(0);
    batch.glyphs.bitmapIndex.push_back(0);
  }

  batch.runs.glyphStart.push_back(start);
  batch.runs.glyphCount.push_back(glyphCount);
  batch.runs.baselineQ8_8.push_back(static_cast<int16_t>(8 * 256));
  batch.runs.scaleQ8_8.push_back(static_cast<uint16_t>(1 * 256));
}

void build_glyph_store(RenderBatch& batch) {
  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 8;
  bitmap.height = 8;
  bitmap.bearingX = 0;
  bitmap.bearingY = 8;
  bitmap.advance = 9;
  bitmap.stride = 8;
  bitmap.pixels.resize(static_cast<size_t>(bitmap.height) * bitmap.stride, 255);
  batch.glyphs.bitmaps.push_back(std::move(bitmap));
  batch.glyphs.bitmapOpaque.push_back(1);
}

} // namespace

int main(int argc, char** argv) {
  BenchConfig cfg = parse_args(argc, argv);

  RenderBatch batch;
  batch.tileSize = cfg.tileSize;
  batch.palette.enabled = true;
  batch.palette.colorRGBA8 = build_palette();
  batch.palette.size = 256;

  build_glyph_store(batch);
  build_text_run(batch, 12);

  std::mt19937 rng(cfg.seed);
  std::uniform_int_distribution<int32_t> xDist(0, static_cast<int32_t>(cfg.width));
  std::uniform_int_distribution<int32_t> yDist(0, static_cast<int32_t>(cfg.height));
  std::uniform_int_distribution<int32_t> wDist(10, 80);
  std::uniform_int_distribution<int32_t> hDist(10, 80);
  std::uniform_int_distribution<uint32_t> idxDist(0, 255);

  uint8_t clearIndex = 192;
  add_clear(batch, clearIndex);

  for (uint32_t i = 0; i < cfg.rectCount; ++i) {
    int32_t w = wDist(rng);
    int32_t h = hDist(rng);
    int32_t x0 = xDist(rng);
    int32_t y0 = yDist(rng);
    int32_t x1 = x0 + w;
    int32_t y1 = y0 + h;
    uint8_t colorIndex = static_cast<uint8_t>(idxDist(rng));
    uint8_t gradientIndex = static_cast<uint8_t>(idxDist(rng));
    bool gradient = (i % 4 == 0);
    uint16_t radiusQ8_8 = static_cast<uint16_t>(cfg.rectRadius * 256);
    add_rect(batch, x0, y0, x1, y1, colorIndex, gradientIndex, gradient, radiusQ8_8);
  }

  if (cfg.enableText) {
    uint32_t runIndex = 0;
    for (uint32_t i = 0; i < cfg.textCount; ++i) {
      int32_t x = xDist(rng);
      int32_t y = yDist(rng);
      uint8_t textIndex = 255;
      add_text(batch, x, y, 120, 24, textIndex, runIndex);
    }
  }

  if (cfg.enableDebugTiles) {
    uint8_t debugIndex = 0;
    add_debug_tiles(batch, debugIndex);
  }

  if (cfg.useTileStream) {
    build_tile_stream(batch, cfg.width, cfg.height);
  }

  std::vector<uint8_t> buffer(static_cast<size_t>(cfg.width) * cfg.height * 4, 0u);
  RenderTarget target{std::span<uint8_t>(buffer), cfg.width, cfg.height, cfg.width * 4};

  auto start = std::chrono::steady_clock::now();
  for (uint32_t frame = 0; frame < cfg.frames; ++frame) {
    Render(target, batch);
  }
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  double fps = static_cast<double>(cfg.frames) / elapsed.count();

  std::cout << "PrimeManifest renderer bench\n";
  std::cout << "Resolution: " << cfg.width << "x" << cfg.height << "\n";
  std::cout << "Rects: " << cfg.rectCount << " Texts: " << (cfg.enableText ? cfg.textCount : 0)
            << " Frames: " << cfg.frames << "\n";
  std::cout << "TileSize: " << cfg.tileSize << "\n";
  std::cout << "Palette: Indexed\n";
  std::cout << "TileStream: " << (cfg.useTileStream ? "Enabled" : "Disabled") << "\n";
  std::cout << "Elapsed: " << elapsed.count() << "s\n";
  std::cout << "FPS: " << fps << "\n";

  return 0;
}
