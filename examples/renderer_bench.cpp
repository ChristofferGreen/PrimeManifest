#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <chrono>
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
  uint16_t tileSize = 64;
  bool enableText = true;
  bool enableDebugTiles = false;
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
    } else if (arg == "--no-text") {
      cfg.enableText = false;
    } else if (arg == "--debug-tiles") {
      cfg.enableDebugTiles = true;
    } else if (arg == "--seed") {
      cfg.seed = next(cfg.seed);
    }
  }
  return cfg;
}

void add_clear(RenderBatch& batch, uint32_t color) {
  uint32_t idx = static_cast<uint32_t>(batch.clear.colorRGBA8.size());
  batch.clear.colorRGBA8.push_back(color);
  batch.commands.push_back(RenderCommand{CommandType::Clear, idx});
}

void add_rect(RenderBatch& batch,
              int32_t x0,
              int32_t y0,
              int32_t x1,
              int32_t y1,
              uint32_t color,
              uint32_t gradientColor,
              bool gradient) {
  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(x0);
  batch.rects.y0.push_back(y0);
  batch.rects.x1.push_back(x1);
  batch.rects.y1.push_back(y1);
  batch.rects.colorRGBA8.push_back(color);
  batch.rects.radiusQ8_8.push_back(static_cast<uint16_t>(4 * 256));
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  uint8_t flags = 0;
  if (gradient) {
    flags |= RectFlagGradient;
    batch.rects.gradientColor1RGBA8.push_back(gradientColor);
    batch.rects.gradientDirX.push_back(static_cast<int16_t>(0));
    batch.rects.gradientDirY.push_back(static_cast<int16_t>(256));
  } else {
    batch.rects.gradientColor1RGBA8.push_back(color);
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
              uint32_t color,
              uint32_t runIndex) {
  uint32_t idx = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(x);
  batch.text.y.push_back(y);
  batch.text.width.push_back(width);
  batch.text.height.push_back(height);
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(255);
  batch.text.colorRGBA8.push_back(color);
  batch.text.flags.push_back(0);
  batch.text.runIndex.push_back(runIndex);
  batch.text.clipX0.push_back(0);
  batch.text.clipY0.push_back(0);
  batch.text.clipX1.push_back(0);
  batch.text.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});
}

void add_debug_tiles(RenderBatch& batch, uint32_t color) {
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorRGBA8.size());
  batch.debugTiles.colorRGBA8.push_back(color);
  batch.debugTiles.lineWidth.push_back(1);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});
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
}

} // namespace

int main(int argc, char** argv) {
  BenchConfig cfg = parse_args(argc, argv);

  RenderBatch batch;
  batch.tileSize = cfg.tileSize;

  build_glyph_store(batch);
  build_text_run(batch, 12);

  std::mt19937 rng(cfg.seed);
  std::uniform_int_distribution<int32_t> xDist(0, static_cast<int32_t>(cfg.width));
  std::uniform_int_distribution<int32_t> yDist(0, static_cast<int32_t>(cfg.height));
  std::uniform_int_distribution<int32_t> wDist(10, 80);
  std::uniform_int_distribution<int32_t> hDist(10, 80);
  std::uniform_int_distribution<uint32_t> cDist(0, 255);

  add_clear(batch, PackRGBA8(Color{12, 12, 16, 255}));

  for (uint32_t i = 0; i < cfg.rectCount; ++i) {
    int32_t w = wDist(rng);
    int32_t h = hDist(rng);
    int32_t x0 = xDist(rng);
    int32_t y0 = yDist(rng);
    int32_t x1 = x0 + w;
    int32_t y1 = y0 + h;
    uint32_t color = PackRGBA8(Color{static_cast<uint8_t>(cDist(rng)),
                                     static_cast<uint8_t>(cDist(rng)),
                                     static_cast<uint8_t>(cDist(rng)),
                                     255});
    uint32_t g1 = PackRGBA8(Color{static_cast<uint8_t>(cDist(rng)),
                                  static_cast<uint8_t>(cDist(rng)),
                                  static_cast<uint8_t>(cDist(rng)),
                                  255});
    bool gradient = (i % 4 == 0);
    add_rect(batch, x0, y0, x1, y1, color, g1, gradient);
  }

  if (cfg.enableText) {
    uint32_t runIndex = 0;
    for (uint32_t i = 0; i < cfg.textCount; ++i) {
      int32_t x = xDist(rng);
      int32_t y = yDist(rng);
      add_text(batch, x, y, 120, 24, PackRGBA8(Color{230, 230, 230, 255}), runIndex);
    }
  }

  if (cfg.enableDebugTiles) {
    add_debug_tiles(batch, PackRGBA8(Color{255, 0, 0, 255}));
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
  std::cout << "Elapsed: " << elapsed.count() << "s\n";
  std::cout << "FPS: " << fps << "\n";

  return 0;
}
