#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

using namespace PrimeManifest;

namespace {
struct BenchConfig {
  uint32_t width = 1280;
  uint32_t height = 720;
  uint32_t rectCount = 4000;
  uint32_t circleCount = 0;
  uint32_t textCount = 200;
  uint32_t frames = 300;
  uint16_t tileSize = 32;
  uint16_t rectRadius = 4;
  uint16_t circleRadius = 4;
  bool enableText = true;
  bool enableDebugTiles = false;
  bool useTileStream = false;
  bool dump = false;
  std::string dumpPath;
  bool profile = false;
  bool useOptimized = false;
  bool disableOpaqueRectFastPath = false;
  bool reuseOptimized = false;
  bool assumeFrontToBack = true;
  bool autoTileStream = true;
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
    } else if (arg == "--circles") {
      cfg.circleCount = next(cfg.circleCount);
    } else if (arg == "--frames") {
      cfg.frames = next(cfg.frames);
    } else if (arg == "--tile") {
      cfg.tileSize = static_cast<uint16_t>(next(cfg.tileSize));
    } else if (arg == "--radius") {
      cfg.rectRadius = static_cast<uint16_t>(next(cfg.rectRadius));
    } else if (arg == "--circle-radius") {
      cfg.circleRadius = static_cast<uint16_t>(next(cfg.circleRadius));
    } else if (arg == "--circle-bench") {
      cfg.width = 1920;
      cfg.height = 1080;
      cfg.rectCount = 0;
      cfg.circleCount = 750000;
      cfg.textCount = 0;
      cfg.frames = 300;
      cfg.tileSize = 32;
      cfg.rectRadius = 0;
      cfg.circleRadius = 4;
      cfg.enableText = false;
    } else if (arg == "--no-text") {
      cfg.enableText = false;
    } else if (arg == "--debug-tiles") {
      cfg.enableDebugTiles = true;
    } else if (arg == "--tile-stream") {
      cfg.useTileStream = true;
    } else if (arg == "--dump") {
      cfg.dump = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        cfg.dumpPath = argv[++i];
      } else {
        cfg.dumpPath = "renderer_bench.ppm";
      }
    } else if (arg == "--profile") {
      cfg.profile = true;
    } else if (arg == "--optimized") {
      cfg.useOptimized = true;
    } else if (arg == "--no-opaque-rect-fastpath") {
      cfg.disableOpaqueRectFastPath = true;
    } else if (arg == "--reuse-optimized") {
      cfg.reuseOptimized = true;
    } else if (arg == "--front-to-back") {
      cfg.assumeFrontToBack = true;
    } else if (arg == "--no-front-to-back") {
      cfg.assumeFrontToBack = false;
    } else if (arg == "--auto-tile-stream") {
      cfg.autoTileStream = true;
    } else if (arg == "--no-auto-tile-stream") {
      cfg.autoTileStream = false;
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

auto write_ppm(std::string const& path, RenderTarget const& target) -> bool {
  if (path.empty() || target.width == 0 || target.height == 0 || target.strideBytes == 0) return false;
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  out << "P6\n" << target.width << " " << target.height << "\n255\n";
  for (uint32_t y = 0; y < target.height; ++y) {
    auto const* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
    for (uint32_t x = 0; x < target.width; ++x) {
      size_t idx = static_cast<size_t>(x) * 4u;
      out.put(static_cast<char>(row[idx + 0]));
      out.put(static_cast<char>(row[idx + 1]));
      out.put(static_cast<char>(row[idx + 2]));
    }
  }
  return static_cast<bool>(out);
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

void add_circle(RenderBatch& batch,
                int32_t centerX,
                int32_t centerY,
                uint16_t radius,
                uint8_t colorIndex) {
  uint32_t idx = static_cast<uint32_t>(batch.circles.centerX.size());
  batch.circles.centerX.push_back(static_cast<int16_t>(centerX));
  batch.circles.centerY.push_back(static_cast<int16_t>(centerY));
  batch.circles.radius.push_back(radius);
  batch.circles.colorIndex.push_back(colorIndex);
  batch.commands.push_back(RenderCommand{CommandType::Circle, idx});
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

  constexpr uint32_t MacroFactor = 2;
  uint32_t macroTilesX = (tilesX + MacroFactor - 1) / MacroFactor;
  uint32_t macroTilesY = (tilesY + MacroFactor - 1) / MacroFactor;
  uint32_t macroCount = macroTilesX * macroTilesY;
  bool allowMacroLocal = (tileSize * MacroFactor) <= 256u;

  enum class Level : uint8_t { Tile, Macro, Global };
  struct StreamCmd {
    CommandType type = CommandType::Rect;
    uint32_t index = 0;
    uint32_t order = 0;
    int32_t x0 = 0;
    int32_t y0 = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    Level level = Level::Global;
    uint32_t tileIndex = 0;
    uint32_t macroIndex = 0;
    uint8_t localX0 = 0;
    uint8_t localY0 = 0;
    uint8_t wMinus1 = 0;
    uint8_t hMinus1 = 0;
  };

  std::vector<StreamCmd> streamCmds;
  streamCmds.reserve(batch.commands.size());

  auto clamp_bounds = [&](int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                          int32_t& ox0, int32_t& oy0, int32_t& ox1, int32_t& oy1) -> bool {
    if (x1 <= 0 || y1 <= 0) return false;
    if (x0 >= static_cast<int32_t>(width) || y0 >= static_cast<int32_t>(height)) return false;
    ox0 = std::max(x0, 0);
    oy0 = std::max(y0, 0);
    ox1 = std::min(x1, static_cast<int32_t>(width));
    oy1 = std::min(y1, static_cast<int32_t>(height));
    return (ox1 > ox0 && oy1 > oy0);
  };

  for (uint32_t cmdIndex = 0; cmdIndex < batch.commands.size(); ++cmdIndex) {
    auto const& cmd = batch.commands[cmdIndex];
    int32_t x0 = 0;
    int32_t y0 = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    if (cmd.type == CommandType::Rect) {
      if (cmd.index >= batch.rects.x0.size() ||
          cmd.index >= batch.rects.y0.size() ||
          cmd.index >= batch.rects.x1.size() ||
          cmd.index >= batch.rects.y1.size()) {
        continue;
      }
      x0 = batch.rects.x0[cmd.index];
      y0 = batch.rects.y0[cmd.index];
      x1 = batch.rects.x1[cmd.index];
      y1 = batch.rects.y1[cmd.index];
    } else if (cmd.type == CommandType::Circle) {
      if (cmd.index >= batch.circles.centerX.size() ||
          cmd.index >= batch.circles.centerY.size() ||
          cmd.index >= batch.circles.radius.size()) {
        continue;
      }
      int32_t cx = batch.circles.centerX[cmd.index];
      int32_t cy = batch.circles.centerY[cmd.index];
      int32_t r = static_cast<int32_t>(batch.circles.radius[cmd.index]);
      x0 = cx - r;
      y0 = cy - r;
      x1 = cx + r + 1;
      y1 = cy + r + 1;
    } else if (cmd.type == CommandType::Text) {
      if (cmd.index >= batch.text.x.size() ||
          cmd.index >= batch.text.y.size() ||
          cmd.index >= batch.text.width.size() ||
          cmd.index >= batch.text.height.size()) {
        continue;
      }
      x0 = batch.text.x[cmd.index];
      y0 = batch.text.y[cmd.index];
      x1 = x0 + batch.text.width[cmd.index];
      y1 = y0 + batch.text.height[cmd.index];
    } else {
      continue;
    }

    int32_t clampedX0 = 0;
    int32_t clampedY0 = 0;
    int32_t clampedX1 = 0;
    int32_t clampedY1 = 0;
    if (!clamp_bounds(x0, y0, x1, y1, clampedX0, clampedY0, clampedX1, clampedY1)) continue;

    uint32_t tx0 = static_cast<uint32_t>(clampedX0) / tileSize;
    uint32_t ty0 = static_cast<uint32_t>(clampedY0) / tileSize;
    uint32_t tx1 = static_cast<uint32_t>(clampedX1 - 1) / tileSize;
    uint32_t ty1 = static_cast<uint32_t>(clampedY1 - 1) / tileSize;

    StreamCmd out;
    out.type = cmd.type;
    out.index = cmd.index;
    out.order = cmdIndex;
    out.x0 = clampedX0;
    out.y0 = clampedY0;
    out.x1 = clampedX1;
    out.y1 = clampedY1;

    if (tx0 == tx1 && ty0 == ty1) {
      uint32_t tileIdx = ty0 * tilesX + tx0;
      int32_t tileX0 = static_cast<int32_t>(tx0 * tileSize);
      int32_t tileY0 = static_cast<int32_t>(ty0 * tileSize);
      int32_t localX0 = clampedX0 - tileX0;
      int32_t localY0 = clampedY0 - tileY0;
      int32_t localW = clampedX1 - clampedX0;
      int32_t localH = clampedY1 - clampedY0;
      if (localX0 >= 0 && localY0 >= 0 && localW > 0 && localH > 0 &&
          localX0 <= 255 && localY0 <= 255 && localW <= 256 && localH <= 256) {
        out.level = Level::Tile;
        out.tileIndex = tileIdx;
        out.localX0 = static_cast<uint8_t>(localX0);
        out.localY0 = static_cast<uint8_t>(localY0);
        out.wMinus1 = static_cast<uint8_t>(localW - 1);
        out.hMinus1 = static_cast<uint8_t>(localH - 1);
        streamCmds.push_back(out);
        continue;
      }
    }

    uint32_t macroX0 = tx0 / MacroFactor;
    uint32_t macroY0 = ty0 / MacroFactor;
    uint32_t macroX1 = tx1 / MacroFactor;
    uint32_t macroY1 = ty1 / MacroFactor;
    if (allowMacroLocal && macroX0 == macroX1 && macroY0 == macroY1) {
      uint32_t macroIdx = macroY0 * macroTilesX + macroX0;
      int32_t macroTileX0 = static_cast<int32_t>(macroX0 * MacroFactor * tileSize);
      int32_t macroTileY0 = static_cast<int32_t>(macroY0 * MacroFactor * tileSize);
      int32_t localX0 = clampedX0 - macroTileX0;
      int32_t localY0 = clampedY0 - macroTileY0;
      int32_t localW = clampedX1 - clampedX0;
      int32_t localH = clampedY1 - clampedY0;
      if (localX0 >= 0 && localY0 >= 0 && localW > 0 && localH > 0 &&
          localX0 <= 255 && localY0 <= 255 && localW <= 256 && localH <= 256) {
        out.level = Level::Macro;
        out.macroIndex = macroIdx;
        out.localX0 = static_cast<uint8_t>(localX0);
        out.localY0 = static_cast<uint8_t>(localY0);
        out.wMinus1 = static_cast<uint8_t>(localW - 1);
        out.hMinus1 = static_cast<uint8_t>(localH - 1);
        streamCmds.push_back(out);
        continue;
      }
    }

    out.level = Level::Global;
    streamCmds.push_back(out);
  }

  std::vector<uint32_t> tileCounts(tileCount, 0);
  std::vector<uint32_t> macroCounts(macroCount, 0);
  uint32_t globalCount = 0;
  for (auto const& cmd : streamCmds) {
    if (cmd.level == Level::Tile) {
      tileCounts[cmd.tileIndex] += 1;
    } else if (cmd.level == Level::Macro) {
      macroCounts[cmd.macroIndex] += 1;
    } else {
      globalCount += 1;
    }
  }

  batch.tileStream.offsets.assign(tileCount + 1, 0);
  for (uint32_t i = 0; i < tileCount; ++i) {
    batch.tileStream.offsets[i + 1] = batch.tileStream.offsets[i] + tileCounts[i];
  }
  batch.tileStream.commands.assign(batch.tileStream.offsets.back(), TileCommand{});
  std::vector<uint32_t> tileFill(tileCount, 0);

  batch.tileStream.macroOffsets.assign(macroCount + 1, 0);
  for (uint32_t i = 0; i < macroCount; ++i) {
    batch.tileStream.macroOffsets[i + 1] = batch.tileStream.macroOffsets[i] + macroCounts[i];
  }
  batch.tileStream.macroCommands.assign(batch.tileStream.macroOffsets.back(), TileCommand{});
  std::vector<uint32_t> macroFill(macroCount, 0);

  batch.tileStream.globalCommands.clear();
  batch.tileStream.globalCommands.reserve(globalCount);

  for (auto const& cmd : streamCmds) {
    TileCommand out;
    out.type = cmd.type;
    out.index = cmd.index;
    out.order = cmd.order;
    out.x = cmd.localX0;
    out.y = cmd.localY0;
    out.wMinus1 = cmd.wMinus1;
    out.hMinus1 = cmd.hMinus1;
    if (cmd.level == Level::Tile) {
      uint32_t offset = batch.tileStream.offsets[cmd.tileIndex] + tileFill[cmd.tileIndex]++;
      batch.tileStream.commands[offset] = out;
    } else if (cmd.level == Level::Macro) {
      uint32_t offset = batch.tileStream.macroOffsets[cmd.macroIndex] + macroFill[cmd.macroIndex]++;
      batch.tileStream.macroCommands[offset] = out;
    } else {
      batch.tileStream.globalCommands.push_back(out);
    }
  }

  // Pre-merge per-tile lists so the renderer can stay dumb.
  struct Bounds {
    int32_t x0 = 0;
    int32_t y0 = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    bool valid = false;
  };
  std::vector<Bounds> rectBounds(batch.rects.x0.size());
  for (uint32_t i = 0; i < batch.rects.x0.size(); ++i) {
    Bounds b{};
    b.x0 = batch.rects.x0[i];
    b.y0 = batch.rects.y0[i];
    b.x1 = batch.rects.x1[i];
    b.y1 = batch.rects.y1[i];
    if (i < batch.rects.flags.size() && (batch.rects.flags[i] & RectFlagClip) != 0u &&
        i < batch.rects.clipX0.size() && i < batch.rects.clipY0.size() &&
        i < batch.rects.clipX1.size() && i < batch.rects.clipY1.size()) {
      b.x0 = std::max(b.x0, static_cast<int32_t>(batch.rects.clipX0[i]));
      b.y0 = std::max(b.y0, static_cast<int32_t>(batch.rects.clipY0[i]));
      b.x1 = std::min(b.x1, static_cast<int32_t>(batch.rects.clipX1[i]));
      b.y1 = std::min(b.y1, static_cast<int32_t>(batch.rects.clipY1[i]));
    }
    b.x0 = std::max(b.x0, 0);
    b.y0 = std::max(b.y0, 0);
    b.x1 = std::min(b.x1, static_cast<int32_t>(width));
    b.y1 = std::min(b.y1, static_cast<int32_t>(height));
    b.valid = (b.x1 > b.x0 && b.y1 > b.y0);
    rectBounds[i] = b;
  }
  std::vector<Bounds> circleBounds(batch.circles.centerX.size());
  for (uint32_t i = 0; i < batch.circles.centerX.size(); ++i) {
    Bounds b{};
    if (i >= batch.circles.centerY.size() ||
        i >= batch.circles.radius.size()) {
      circleBounds[i] = b;
      continue;
    }
    int32_t cx = batch.circles.centerX[i];
    int32_t cy = batch.circles.centerY[i];
    int32_t r = static_cast<int32_t>(batch.circles.radius[i]);
    b.x0 = cx - r;
    b.y0 = cy - r;
    b.x1 = cx + r + 1;
    b.y1 = cy + r + 1;
    b.x0 = std::max(b.x0, 0);
    b.y0 = std::max(b.y0, 0);
    b.x1 = std::min(b.x1, static_cast<int32_t>(width));
    b.y1 = std::min(b.y1, static_cast<int32_t>(height));
    b.valid = (b.x1 > b.x0 && b.y1 > b.y0);
    circleBounds[i] = b;
  }
  std::vector<Bounds> textBounds(batch.text.x.size());
  for (uint32_t i = 0; i < batch.text.x.size(); ++i) {
    Bounds b{};
    b.x0 = batch.text.x[i];
    b.y0 = batch.text.y[i];
    b.x1 = b.x0 + batch.text.width[i];
    b.y1 = b.y0 + batch.text.height[i];
    if (i < batch.text.flags.size() && (batch.text.flags[i] & TextFlagClip) != 0u &&
        i < batch.text.clipX0.size() && i < batch.text.clipY0.size() &&
        i < batch.text.clipX1.size() && i < batch.text.clipY1.size()) {
      b.x0 = std::max(b.x0, static_cast<int32_t>(batch.text.clipX0[i]));
      b.y0 = std::max(b.y0, static_cast<int32_t>(batch.text.clipY0[i]));
      b.x1 = std::min(b.x1, static_cast<int32_t>(batch.text.clipX1[i]));
      b.y1 = std::min(b.y1, static_cast<int32_t>(batch.text.clipY1[i]));
    }
    b.x0 = std::max(b.x0, 0);
    b.y0 = std::max(b.y0, 0);
    b.x1 = std::min(b.x1, static_cast<int32_t>(width));
    b.y1 = std::min(b.y1, static_cast<int32_t>(height));
    b.valid = (b.x1 > b.x0 && b.y1 > b.y0);
    textBounds[i] = b;
  }

  auto const tileOffsets = batch.tileStream.offsets;
  auto const tileCommands = batch.tileStream.commands;
  auto const macroOffsets = batch.tileStream.macroOffsets;
  auto const macroCommands = batch.tileStream.macroCommands;
  auto const globalCommands = batch.tileStream.globalCommands;

  std::vector<uint32_t> mergedCounts(tileCount, 0);
  auto count_tile = [&](uint32_t tileIndex) {
    uint32_t tx = tileIndex % tilesX;
    uint32_t ty = tileIndex / tilesX;
    int32_t tileX0 = static_cast<int32_t>(tx * tileSize);
    int32_t tileY0 = static_cast<int32_t>(ty * tileSize);
    int32_t tileX1 = std::min(tileX0 + static_cast<int32_t>(tileSize), static_cast<int32_t>(width));
    int32_t tileY1 = std::min(tileY0 + static_cast<int32_t>(tileSize), static_cast<int32_t>(height));
    size_t tileCursor = tileOffsets[tileIndex];
    size_t tileEnd = tileOffsets[tileIndex + 1];
    uint32_t macroX = tx / MacroFactor;
    uint32_t macroY = ty / MacroFactor;
    uint32_t macroIndex = macroY * macroTilesX + macroX;
    size_t macroCursor = macroOffsets[macroIndex];
    size_t macroEnd = macroOffsets[macroIndex + 1];
    size_t globalCursor = 0;
    size_t globalEndLocal = globalCommands.size();
    int32_t macroOriginX = static_cast<int32_t>(macroX * MacroFactor * tileSize);
    int32_t macroOriginY = static_cast<int32_t>(macroY * MacroFactor * tileSize);
    while (tileCursor < tileEnd || macroCursor < macroEnd || globalCursor < globalEndLocal) {
      uint32_t bestOrder = 0xFFFFFFFFu;
      enum class Source : uint8_t { Tile, Macro, Global } src = Source::Tile;
      bool hasSrc = false;
      if (tileCursor < tileEnd) {
        bestOrder = tileCommands[tileCursor].order;
        src = Source::Tile;
        hasSrc = true;
      }
      if (macroCursor < macroEnd) {
        uint32_t order = macroCommands[macroCursor].order;
        if (!hasSrc || order < bestOrder) {
          bestOrder = order;
          src = Source::Macro;
          hasSrc = true;
        }
      }
      if (globalCursor < globalEndLocal) {
        uint32_t order = globalCommands[globalCursor].order;
        if (!hasSrc || order < bestOrder) {
          bestOrder = order;
          src = Source::Global;
          hasSrc = true;
        }
      }
      if (!hasSrc) break;
      if (src == Source::Tile) {
        ++tileCursor;
        mergedCounts[tileIndex] += 1;
      } else if (src == Source::Macro) {
        auto const& cmd = macroCommands[macroCursor++];
        int32_t drawX0 = macroOriginX + static_cast<int32_t>(cmd.x);
        int32_t drawY0 = macroOriginY + static_cast<int32_t>(cmd.y);
        int32_t drawX1 = drawX0 + static_cast<int32_t>(cmd.wMinus1) + 1;
        int32_t drawY1 = drawY0 + static_cast<int32_t>(cmd.hMinus1) + 1;
        int32_t ix0 = std::max(drawX0, tileX0);
        int32_t iy0 = std::max(drawY0, tileY0);
        int32_t ix1 = std::min(drawX1, tileX1);
        int32_t iy1 = std::min(drawY1, tileY1);
        if (ix1 > ix0 && iy1 > iy0) {
          mergedCounts[tileIndex] += 1;
        }
      } else {
        auto const& cmd = globalCommands[globalCursor++];
        Bounds b{};
        if (cmd.type == CommandType::Rect) {
          if (cmd.index >= rectBounds.size() || !rectBounds[cmd.index].valid) continue;
          b = rectBounds[cmd.index];
        } else if (cmd.type == CommandType::Circle) {
          if (cmd.index >= circleBounds.size() || !circleBounds[cmd.index].valid) continue;
          b = circleBounds[cmd.index];
        } else if (cmd.type == CommandType::Text) {
          if (cmd.index >= textBounds.size() || !textBounds[cmd.index].valid) continue;
          b = textBounds[cmd.index];
        } else {
          continue;
        }
        int32_t ix0 = std::max(b.x0, tileX0);
        int32_t iy0 = std::max(b.y0, tileY0);
        int32_t ix1 = std::min(b.x1, tileX1);
        int32_t iy1 = std::min(b.y1, tileY1);
        if (ix1 > ix0 && iy1 > iy0) {
          mergedCounts[tileIndex] += 1;
        }
      }
    }
  };

  for (uint32_t tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
    count_tile(tileIndex);
  }

  std::vector<uint32_t> mergedOffsets(tileCount + 1, 0);
  for (uint32_t i = 0; i < tileCount; ++i) {
    mergedOffsets[i + 1] = mergedOffsets[i] + mergedCounts[i];
  }
  std::vector<TileCommand> mergedCommands;
  mergedCommands.assign(mergedOffsets.back(), TileCommand{});
  std::vector<uint32_t> mergedFill(tileCount, 0);

  auto emit_tile = [&](uint32_t tileIndex) {
    uint32_t tx = tileIndex % tilesX;
    uint32_t ty = tileIndex / tilesX;
    int32_t tileX0 = static_cast<int32_t>(tx * tileSize);
    int32_t tileY0 = static_cast<int32_t>(ty * tileSize);
    int32_t tileX1 = std::min(tileX0 + static_cast<int32_t>(tileSize), static_cast<int32_t>(width));
    int32_t tileY1 = std::min(tileY0 + static_cast<int32_t>(tileSize), static_cast<int32_t>(height));
    size_t tileCursor = tileOffsets[tileIndex];
    size_t tileEnd = tileOffsets[tileIndex + 1];
    uint32_t macroX = tx / MacroFactor;
    uint32_t macroY = ty / MacroFactor;
    uint32_t macroIndex = macroY * macroTilesX + macroX;
    size_t macroCursor = macroOffsets[macroIndex];
    size_t macroEnd = macroOffsets[macroIndex + 1];
    size_t globalCursor = 0;
    size_t globalEndLocal = globalCommands.size();
    int32_t macroOriginX = static_cast<int32_t>(macroX * MacroFactor * tileSize);
    int32_t macroOriginY = static_cast<int32_t>(macroY * MacroFactor * tileSize);
    while (tileCursor < tileEnd || macroCursor < macroEnd || globalCursor < globalEndLocal) {
      uint32_t bestOrder = 0xFFFFFFFFu;
      enum class Source : uint8_t { Tile, Macro, Global } src = Source::Tile;
      bool hasSrc = false;
      if (tileCursor < tileEnd) {
        bestOrder = tileCommands[tileCursor].order;
        src = Source::Tile;
        hasSrc = true;
      }
      if (macroCursor < macroEnd) {
        uint32_t order = macroCommands[macroCursor].order;
        if (!hasSrc || order < bestOrder) {
          bestOrder = order;
          src = Source::Macro;
          hasSrc = true;
        }
      }
      if (globalCursor < globalEndLocal) {
        uint32_t order = globalCommands[globalCursor].order;
        if (!hasSrc || order < bestOrder) {
          bestOrder = order;
          src = Source::Global;
          hasSrc = true;
        }
      }
      if (!hasSrc) break;
      if (src == Source::Tile) {
        auto cmd = tileCommands[tileCursor++];
        uint32_t offset = mergedOffsets[tileIndex] + mergedFill[tileIndex]++;
        mergedCommands[offset] = cmd;
      } else if (src == Source::Macro) {
        auto cmd = macroCommands[macroCursor++];
        int32_t drawX0 = macroOriginX + static_cast<int32_t>(cmd.x);
        int32_t drawY0 = macroOriginY + static_cast<int32_t>(cmd.y);
        int32_t drawX1 = drawX0 + static_cast<int32_t>(cmd.wMinus1) + 1;
        int32_t drawY1 = drawY0 + static_cast<int32_t>(cmd.hMinus1) + 1;
        int32_t ix0 = std::max(drawX0, tileX0);
        int32_t iy0 = std::max(drawY0, tileY0);
        int32_t ix1 = std::min(drawX1, tileX1);
        int32_t iy1 = std::min(drawY1, tileY1);
        if (ix1 <= ix0 || iy1 <= iy0) continue;
        int32_t localX0 = ix0 - tileX0;
        int32_t localY0 = iy0 - tileY0;
        int32_t localW = ix1 - ix0;
        int32_t localH = iy1 - iy0;
        if (localX0 < 0 || localY0 < 0 || localW <= 0 || localH <= 0) continue;
        TileCommand out{};
        out.type = cmd.type;
        out.index = cmd.index;
        out.order = cmd.order;
        out.x = static_cast<uint8_t>(localX0);
        out.y = static_cast<uint8_t>(localY0);
        out.wMinus1 = static_cast<uint8_t>(localW - 1);
        out.hMinus1 = static_cast<uint8_t>(localH - 1);
        uint32_t offset = mergedOffsets[tileIndex] + mergedFill[tileIndex]++;
        mergedCommands[offset] = out;
      } else {
        auto cmd = globalCommands[globalCursor++];
        Bounds b{};
        if (cmd.type == CommandType::Rect) {
          if (cmd.index >= rectBounds.size() || !rectBounds[cmd.index].valid) continue;
          b = rectBounds[cmd.index];
        } else if (cmd.type == CommandType::Circle) {
          if (cmd.index >= circleBounds.size() || !circleBounds[cmd.index].valid) continue;
          b = circleBounds[cmd.index];
        } else if (cmd.type == CommandType::Text) {
          if (cmd.index >= textBounds.size() || !textBounds[cmd.index].valid) continue;
          b = textBounds[cmd.index];
        } else {
          continue;
        }
        int32_t ix0 = std::max(b.x0, tileX0);
        int32_t iy0 = std::max(b.y0, tileY0);
        int32_t ix1 = std::min(b.x1, tileX1);
        int32_t iy1 = std::min(b.y1, tileY1);
        if (ix1 <= ix0 || iy1 <= iy0) continue;
        int32_t localX0 = ix0 - tileX0;
        int32_t localY0 = iy0 - tileY0;
        int32_t localW = ix1 - ix0;
        int32_t localH = iy1 - iy0;
        if (localX0 < 0 || localY0 < 0 || localW <= 0 || localH <= 0) continue;
        TileCommand out{};
        out.type = cmd.type;
        out.index = cmd.index;
        out.order = cmd.order;
        out.x = static_cast<uint8_t>(localX0);
        out.y = static_cast<uint8_t>(localY0);
        out.wMinus1 = static_cast<uint8_t>(localW - 1);
        out.hMinus1 = static_cast<uint8_t>(localH - 1);
        uint32_t offset = mergedOffsets[tileIndex] + mergedFill[tileIndex]++;
        mergedCommands[offset] = out;
      }
    }
  };

  for (uint32_t tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
    emit_tile(tileIndex);
  }

  batch.tileStream.offsets = std::move(mergedOffsets);
  batch.tileStream.commands = std::move(mergedCommands);
  batch.tileStream.macroOffsets.clear();
  batch.tileStream.macroCommands.clear();
  batch.tileStream.globalCommands.clear();
  batch.tileStream.preMerged = true;
  batch.tileStream.enabled = true;
}

void build_text_run(RenderBatch& batch, uint32_t glyphCount) {
  uint32_t start = static_cast<uint32_t>(batch.glyphs.glyphXQ8_8.size());
  for (uint32_t i = 0; i < glyphCount; ++i) {
    batch.glyphs.glyphXQ8_8.push_back(static_cast<int32_t>(i * 10 * 256));
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
  batch.disableOpaqueRectFastPath = cfg.disableOpaqueRectFastPath;
  batch.reuseOptimized = cfg.reuseOptimized;
  batch.assumeFrontToBack = cfg.assumeFrontToBack;
  batch.autoTileStream = cfg.autoTileStream;
  batch.useCommandRevision = true;

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

  std::vector<int16_t> circleBaseY;
  std::vector<uint32_t> circleEdgeIndices;
  int32_t circleMoveStep = 0;

  if (cfg.rectCount > 0) {
    batch.rects.x0.reserve(cfg.rectCount);
    batch.rects.y0.reserve(cfg.rectCount);
    batch.rects.x1.reserve(cfg.rectCount);
    batch.rects.y1.reserve(cfg.rectCount);
    batch.rects.colorIndex.reserve(cfg.rectCount);
    batch.rects.radiusQ8_8.reserve(cfg.rectCount);
    batch.rects.rotationQ8_8.reserve(cfg.rectCount);
    batch.rects.zQ8_8.reserve(cfg.rectCount);
    batch.rects.opacity.reserve(cfg.rectCount);
    batch.rects.flags.reserve(cfg.rectCount);
    batch.rects.gradientColor1Index.reserve(cfg.rectCount);
    batch.rects.gradientDirX.reserve(cfg.rectCount);
    batch.rects.gradientDirY.reserve(cfg.rectCount);
    batch.rects.clipX0.reserve(cfg.rectCount);
    batch.rects.clipY0.reserve(cfg.rectCount);
    batch.rects.clipX1.reserve(cfg.rectCount);
    batch.rects.clipY1.reserve(cfg.rectCount);
  }

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

  if (cfg.circleCount > 0) {
    batch.circles.centerX.reserve(cfg.circleCount);
    batch.circles.centerY.reserve(cfg.circleCount);
    batch.circles.radius.reserve(cfg.circleCount);
    batch.circles.colorIndex.reserve(cfg.circleCount);
    circleBaseY.reserve(cfg.circleCount);
    circleEdgeIndices.reserve(cfg.circleCount / 8);
    circleMoveStep = std::max<int32_t>(2, static_cast<int32_t>(cfg.circleRadius) / 2);
    if (cfg.reuseOptimized) {
      uint32_t pad = static_cast<uint32_t>(circleMoveStep) * 2u;
      if (pad > std::numeric_limits<uint16_t>::max()) {
        pad = std::numeric_limits<uint16_t>::max();
      }
      batch.circleBoundsPad = static_cast<uint16_t>(pad);
    }
    for (uint32_t i = 0; i < cfg.circleCount; ++i) {
      int32_t cx = xDist(rng);
      int32_t cy = yDist(rng);
      uint8_t colorIndex = static_cast<uint8_t>(idxDist(rng));
      add_circle(batch, cx, cy, cfg.circleRadius, colorIndex);
      circleBaseY.push_back(static_cast<int16_t>(cy));
    }
    int32_t maxY = static_cast<int32_t>(cfg.height);
    int32_t safeMin = circleMoveStep;
    int32_t safeMax = maxY - circleMoveStep;
    for (uint32_t i = 0; i < circleBaseY.size(); ++i) {
      int32_t base = static_cast<int32_t>(circleBaseY[i]);
      if (base < safeMin || base > safeMax) {
        circleEdgeIndices.push_back(i);
      }
    }
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

  batch.revision = 1;
  batch.commandRevision = 1;

  if (cfg.enableDebugTiles) {
    uint8_t debugIndex = 0;
    add_debug_tiles(batch, debugIndex);
  }

  if (cfg.useTileStream) {
    build_tile_stream(batch, cfg.width, cfg.height);
  }

  std::vector<uint8_t> buffer(static_cast<size_t>(cfg.width) * cfg.height * 4, 0u);
  RenderTarget target{std::span<uint8_t>(buffer), cfg.width, cfg.height, cfg.width * 4};
  OptimizedBatch optimized;
  bool dynamicCircles = !circleBaseY.empty();
  bool renderOnly = cfg.useOptimized && !dynamicCircles;
  if (renderOnly) {
    OptimizeRenderBatch(target, batch, optimized);
  }
  auto canReuseOptimized = [&]() -> bool {
    if (!batch.reuseOptimized) return false;
    if (!optimized.valid) return false;
    if (optimized.sourceRevision != batch.revision) return false;
    if (optimized.targetWidth != target.width || optimized.targetHeight != target.height) return false;
    return true;
  };

  auto start = std::chrono::steady_clock::now();
  for (uint32_t frame = 0; frame < cfg.frames; ++frame) {
    if (dynamicCircles) {
      int32_t delta = (frame & 1u) == 0u ? -circleMoveStep : circleMoveStep;
      for (size_t i = 0; i < circleBaseY.size(); ++i) {
        int32_t y = static_cast<int32_t>(circleBaseY[i]) + delta;
        batch.circles.centerY[i] = static_cast<int16_t>(y);
      }
      if (!circleEdgeIndices.empty()) {
        int32_t maxY = static_cast<int32_t>(cfg.height);
        for (uint32_t idx : circleEdgeIndices) {
          int32_t y = static_cast<int32_t>(circleBaseY[idx]) + delta;
          if (y < 0) y = 0;
          if (y > maxY) y = maxY;
          batch.circles.centerY[idx] = static_cast<int16_t>(y);
        }
      }
      if (!batch.reuseOptimized) {
        batch.revision += 1;
      }
    }
    if (cfg.useTileStream && dynamicCircles) {
      batch.tileStream.clear();
      build_tile_stream(batch, cfg.width, cfg.height);
    }
    if (renderOnly) {
      RenderOptimized(target, batch, optimized);
      continue;
    }
    OptimizeRenderBatch(target, batch, optimized);
    RenderOptimized(target, batch, optimized);
  }
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  double fps = static_cast<double>(cfg.frames) / elapsed.count();

  std::cout << "PrimeManifest renderer bench\n";
  std::cout << "Resolution: " << cfg.width << "x" << cfg.height << "\n";
  std::cout << "Rects: " << cfg.rectCount
            << " Circles: " << cfg.circleCount
            << " Texts: " << (cfg.enableText ? cfg.textCount : 0)
            << " Frames: " << cfg.frames << "\n";
  if (dynamicCircles) {
    std::cout << "CircleMotion: Enabled (Step " << circleMoveStep << "px)\n";
  } else {
    std::cout << "CircleMotion: Disabled\n";
  }
  uint32_t reportedTileSize = optimized.valid ? optimized.tileSize : cfg.tileSize;
  std::cout << "TileSize: " << reportedTileSize;
  if (optimized.valid && optimized.tileSize != cfg.tileSize) {
    std::cout << " (requested " << cfg.tileSize << ")";
  }
  std::cout << "\n";
  std::cout << "Palette: Indexed\n";
  std::cout << "TileStream: " << (cfg.useTileStream ? "Enabled" : "Disabled") << "\n";
  std::cout << "ReuseOptimized: " << (cfg.reuseOptimized ? "Enabled" : "Disabled") << "\n";
  std::cout << "FrontToBack: " << (cfg.assumeFrontToBack ? "Enabled" : "Disabled") << "\n";
  std::cout << "AutoTileStream: " << (cfg.autoTileStream ? "Enabled" : "Disabled") << "\n";
  std::cout << "Optimized: " << (renderOnly ? "Enabled" : "Disabled") << "\n";
  std::cout << "Elapsed: " << elapsed.count() << "s\n";
  std::cout << "FPS: " << fps << "\n";
  if (cfg.profile) {
    RendererProfile profile;
    profile.clear();
    batch.profile = &profile;
    if (renderOnly) {
      if (!optimized.valid) {
        OptimizeRenderBatch(target, batch, optimized);
      }
      RenderOptimized(target, batch, optimized);
    } else {
      OptimizeRenderBatch(target, batch, optimized);
      RenderOptimized(target, batch, optimized);
    }
    batch.profile = nullptr;
    auto ns_to_ms = [](uint64_t ns) -> double { return static_cast<double>(ns) / 1.0e6; };
    double renderMs = ns_to_ms(profile.renderNs);
    double buildMs = ns_to_ms(profile.buildNs);
    double premergeMs = ns_to_ms(profile.premergeNs);
    double workMs = ns_to_ms(profile.tileWorkNs);
    double optScanMs = ns_to_ms(profile.optScanNs);
    double optTileGridMs = ns_to_ms(profile.optTileGridNs);
    double optTileStreamMs = ns_to_ms(profile.optTileStreamNs);
    double optTileBinningMs = ns_to_ms(profile.optTileBinningNs);
    double optRenderTilesMs = ns_to_ms(profile.optRenderTilesNs);
    double optRectCacheMs = ns_to_ms(profile.optRectCacheNs);
    double optTextCacheMs = ns_to_ms(profile.optTextCacheNs);
    double renderClearMs = ns_to_ms(profile.renderClearNs);
    double renderTilesMs = ns_to_ms(profile.renderTilesNs);
    double renderDebugMs = ns_to_ms(profile.renderDebugNs);
    double coreEquiv = profile.renderNs > 0
                         ? static_cast<double>(profile.tileWorkNs) / static_cast<double>(profile.renderNs)
                         : 0.0;
    size_t workerCount = profile.workerNs.size();
    double utilPct = workerCount > 0 ? (coreEquiv / static_cast<double>(workerCount)) * 100.0 : 0.0;

    std::cout << "Profile: Render " << renderMs << "ms"
              << " Clear " << renderClearMs << "ms"
              << " Tiles " << renderTilesMs << "ms"
              << " Debug " << renderDebugMs << "ms"
              << " TileWork " << workMs << "ms\n";
    std::cout << "Profile: Optimize " << buildMs << "ms"
              << " Scan " << optScanMs << "ms"
              << " TileGrid " << optTileGridMs << "ms"
              << " TileStream " << optTileStreamMs << "ms"
              << " Premerge " << premergeMs << "ms"
              << " Binning " << optTileBinningMs << "ms"
              << " RenderTiles " << optRenderTilesMs << "ms\n";
    std::cout << "Profile: OptRectCache " << optRectCacheMs << "ms"
              << " OptTextCache " << optTextCacheMs << "ms\n";
    std::cout << "Profile: Tiles " << profile.activeTileCount << "/" << profile.tileCount
              << " Commands " << profile.commandCount << "\n";
    std::cout << "Profile: WorkerCount " << workerCount
              << " CoreEquiv " << coreEquiv
              << " Util " << utilPct << "%\n";
    std::cout << "Profile: RenderedTiles " << profile.renderedTileCount
              << " RenderedCommands " << profile.renderedCommandCount
              << " RenderedPixels " << profile.renderedPixelCount << "\n";
    std::cout << "Profile: RenderedRects " << profile.renderedRectCount
              << " RenderedTexts " << profile.renderedTextCount
              << " RectPixels " << profile.renderedRectPixels
              << " TextPixels " << profile.renderedTextPixels
              << " TileBufferPixels " << profile.renderedTileBufferPixels << "\n";
    for (size_t i = 0; i < workerCount; ++i) {
      double workerMs = static_cast<double>(profile.workerNs[i]) / 1.0e6;
      std::cout << "Profile: Worker " << i
                << " Tiles " << profile.workerTiles[i]
                << " Time " << workerMs << "ms\n";
    }
  }
  if (cfg.dump) {
    if (write_ppm(cfg.dumpPath, target)) {
      std::cout << "Framebuffer: " << cfg.dumpPath << "\n";
    } else {
      std::cerr << "Failed to dump framebuffer to " << cfg.dumpPath << "\n";
    }
  }

  return 0;
}
