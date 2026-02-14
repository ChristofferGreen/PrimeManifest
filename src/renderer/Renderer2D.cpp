#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace PrimeManifest {
namespace {

constexpr float DefaultAa = 1.0f;
constexpr uint32_t MacroFactor = 2;

auto clamp01(float v) -> float {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

auto coverage_from_dist(float dist) -> uint8_t {
  float cov = 0.5f - dist / DefaultAa;
  if (cov <= 0.0f) return 0;
  if (cov >= 1.0f) return 255;
  return static_cast<uint8_t>(cov * 255.0f + 0.5f);
}

struct Vec2f {
  float x = 0.0f;
  float y = 0.0f;
};

auto dot(Vec2f a, Vec2f b) -> float {
  return a.x * b.x + a.y * b.y;
}

auto length(Vec2f v) -> float {
  return std::sqrt(dot(v, v));
}

auto normalize_or_default(Vec2f v, Vec2f fallback) -> Vec2f {
  float len = length(v);
  if (len <= 1e-5f) return fallback;
  return Vec2f{v.x / len, v.y / len};
}

auto rotate_point(Vec2f p, float cosA, float sinA) -> Vec2f {
  return Vec2f{p.x * cosA - p.y * sinA, p.x * sinA + p.y * cosA};
}

auto sdf_round_rect(Vec2f p, float hx, float hy, float radius) -> float {
  float rx = std::max(0.0f, radius);
  float qx = std::abs(p.x) - hx + rx;
  float qy = std::abs(p.y) - hy + rx;
  float ax = std::max(qx, 0.0f);
  float ay = std::max(qy, 0.0f);
  float outside = (ax > 0.0f && ay > 0.0f) ? std::sqrt(ax * ax + ay * ay) : (ax + ay);
  float inside = std::min(std::max(qx, qy), 0.0f);
  return outside + inside - rx;
}

constexpr uint8_t OpaqueAlphaCutoff = 250u;

auto blend_premultiplied(uint8_t* dst, uint8_t srcR, uint8_t srcG, uint8_t srcB, uint8_t srcA) -> void {
  uint8_t dstR = dst[0];
  uint8_t dstG = dst[1];
  uint8_t dstB = dst[2];
  uint8_t dstA = dst[3];

  uint16_t invA = static_cast<uint16_t>(255u - srcA);
  dst[0] = static_cast<uint8_t>((static_cast<uint16_t>(srcR) + (dstR * invA + 127u) / 255u) & 0xFFu);
  dst[1] = static_cast<uint8_t>((static_cast<uint16_t>(srcG) + (dstG * invA + 127u) / 255u) & 0xFFu);
  dst[2] = static_cast<uint8_t>((static_cast<uint16_t>(srcB) + (dstB * invA + 127u) / 255u) & 0xFFu);
  dst[3] = static_cast<uint8_t>((static_cast<uint16_t>(srcA) + (dstA * invA + 127u) / 255u) & 0xFFu);
}

auto apply_opacity(uint8_t a, uint8_t opacity) -> uint8_t {
  uint16_t v = static_cast<uint16_t>(a) * static_cast<uint16_t>(opacity);
  v = static_cast<uint16_t>((v + 127u) / 255u);
  return static_cast<uint8_t>(std::min<uint16_t>(v, 255u));
}

auto apply_coverage(uint8_t baseAlpha, uint8_t coverage) -> uint8_t {
  uint16_t v = static_cast<uint16_t>(baseAlpha) * static_cast<uint16_t>(coverage);
  v = static_cast<uint16_t>((v + 127u) / 255u);
  return static_cast<uint8_t>(std::min<uint16_t>(v, 255u));
}


struct TilePool {
  std::mutex mutex;
  std::condition_variable cv;
  std::condition_variable cvDone;
  bool shutdown = false;
  bool workReady = false;
  uint32_t workCount = 0;
  std::atomic<uint32_t> nextWork{0};
  std::atomic<uint32_t> workDone{0};
  std::function<void(uint32_t)> job;
  std::vector<std::thread> workers;

  TilePool() {
    uint32_t count = std::max(1u, std::thread::hardware_concurrency());
    workers.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      workers.emplace_back([this]() { worker_loop(); });
    }
  }

  ~TilePool() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      shutdown = true;
    }
    cv.notify_all();
    for (auto& t : workers) {
      if (t.joinable()) t.join();
    }
  }

  void run(uint32_t jobs, std::function<void(uint32_t)> fn) {
    if (jobs == 0) return;
    {
      std::lock_guard<std::mutex> lock(mutex);
      job = std::move(fn);
      workCount = jobs;
      nextWork.store(0);
      workDone.store(0);
      workReady = true;
    }
    cv.notify_all();

    // Main thread helps.
    do_work();

    std::unique_lock<std::mutex> lock(mutex);
    cvDone.wait(lock, [&]() { return workDone.load() >= workCount; });
    workReady = false;
  }

  void worker_loop() {
    for (;;) {
      {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&]() { return shutdown || workReady; });
        if (shutdown) return;
      }
      do_work();
    }
  }

  void do_work() {
    uint32_t count = workCount;
    constexpr uint32_t ChunkSize = 4;
    for (;;) {
      uint32_t idx = nextWork.fetch_add(ChunkSize);
      if (idx >= count) break;
      uint32_t end = std::min(idx + ChunkSize, count);
      for (uint32_t i = idx; i < end; ++i) {
        job(i);
      }
      uint32_t done = workDone.fetch_add(end - idx) + (end - idx);
      if (done >= count) {
        std::lock_guard<std::mutex> lock(mutex);
        cvDone.notify_one();
      }
    }
  }
};

auto tile_pool() -> TilePool& {
  static TilePool pool;
  return pool;
}

struct TileGrid {
  uint32_t tilesX = 0;
  uint32_t tilesY = 0;
  uint32_t tileSize = 0;
};

auto make_tile_grid(uint32_t width, uint32_t height, uint32_t tileSize) -> TileGrid {
  TileGrid grid;
  grid.tileSize = tileSize == 0 ? 32u : tileSize;
  grid.tilesX = (width + grid.tileSize - 1) / grid.tileSize;
  grid.tilesY = (height + grid.tileSize - 1) / grid.tileSize;
  return grid;
}

struct CommandBounds {
  int32_t x0 = 0;
  int32_t y0 = 0;
  int32_t x1 = 0;
  int32_t y1 = 0;
  bool valid = false;
};

auto premerge_tile_stream(RenderBatch const& batch,
                          TileGrid const& grid,
                          uint32_t width,
                          uint32_t height) -> TileStream {
  TileStream merged;
  auto const& src = batch.tileStream;
  if (!src.enabled || src.preMerged) return merged;
  if (grid.tileSize == 0 || grid.tileSize > 256u) return merged;
  uint32_t tileCount = grid.tilesX * grid.tilesY;
  if (tileCount == 0) return merged;
  if (src.offsets.size() != static_cast<size_t>(tileCount + 1) ||
      src.offsets.back() != src.commands.size()) {
    return merged;
  }

  uint32_t macroTilesX = (grid.tilesX + MacroFactor - 1) / MacroFactor;
  uint32_t macroTilesY = (grid.tilesY + MacroFactor - 1) / MacroFactor;
  uint32_t macroCount = macroTilesX * macroTilesY;

  std::vector<uint32_t> fallbackMacroOffsets;
  auto const* macroOffsets = &src.macroOffsets;
  if (src.macroOffsets.empty()) {
    if (!src.macroCommands.empty()) return merged;
    fallbackMacroOffsets.assign(macroCount + 1, 0);
    macroOffsets = &fallbackMacroOffsets;
  } else if (src.macroOffsets.size() != static_cast<size_t>(macroCount + 1) ||
             src.macroOffsets.back() != src.macroCommands.size()) {
    return merged;
  }

  std::vector<CommandBounds> rectBounds(batch.rects.x0.size());
  for (uint32_t i = 0; i < batch.rects.x0.size(); ++i) {
    CommandBounds b{};
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

  std::vector<CommandBounds> textBounds(batch.text.x.size());
  for (uint32_t i = 0; i < batch.text.x.size(); ++i) {
    CommandBounds b{};
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

  std::vector<uint32_t> mergedCounts(tileCount, 0);
  auto count_tile = [&](uint32_t tileIndex) {
    uint32_t tx = tileIndex % grid.tilesX;
    uint32_t ty = tileIndex / grid.tilesX;
    int32_t tileX0 = static_cast<int32_t>(tx * grid.tileSize);
    int32_t tileY0 = static_cast<int32_t>(ty * grid.tileSize);
    int32_t tileX1 = std::min(tileX0 + static_cast<int32_t>(grid.tileSize), static_cast<int32_t>(width));
    int32_t tileY1 = std::min(tileY0 + static_cast<int32_t>(grid.tileSize), static_cast<int32_t>(height));
    size_t tileCursor = src.offsets[tileIndex];
    size_t tileEnd = src.offsets[tileIndex + 1];
    uint32_t macroX = tx / MacroFactor;
    uint32_t macroY = ty / MacroFactor;
    uint32_t macroIndex = macroY * macroTilesX + macroX;
    size_t macroCursor = (*macroOffsets)[macroIndex];
    size_t macroEnd = (*macroOffsets)[macroIndex + 1];
    size_t globalCursor = 0;
    size_t globalEnd = src.globalCommands.size();
    int32_t macroOriginX = static_cast<int32_t>(macroX * MacroFactor * grid.tileSize);
    int32_t macroOriginY = static_cast<int32_t>(macroY * MacroFactor * grid.tileSize);
    while (tileCursor < tileEnd || macroCursor < macroEnd || globalCursor < globalEnd) {
      uint32_t bestOrder = 0xFFFFFFFFu;
      enum class Source : uint8_t { Tile, Macro, Global } srcType = Source::Tile;
      bool hasSrc = false;
      if (tileCursor < tileEnd) {
        bestOrder = src.commands[tileCursor].order;
        srcType = Source::Tile;
        hasSrc = true;
      }
      if (macroCursor < macroEnd) {
        uint32_t order = src.macroCommands[macroCursor].order;
        if (!hasSrc || order < bestOrder) {
          bestOrder = order;
          srcType = Source::Macro;
          hasSrc = true;
        }
      }
      if (globalCursor < globalEnd) {
        uint32_t order = src.globalCommands[globalCursor].order;
        if (!hasSrc || order < bestOrder) {
          bestOrder = order;
          srcType = Source::Global;
          hasSrc = true;
        }
      }
      if (!hasSrc) break;
      if (srcType == Source::Tile) {
        ++tileCursor;
        mergedCounts[tileIndex] += 1;
      } else if (srcType == Source::Macro) {
        auto const& cmd = src.macroCommands[macroCursor++];
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
        auto const& cmd = src.globalCommands[globalCursor++];
        CommandBounds b{};
        if (cmd.type == CommandType::Rect) {
          if (cmd.index >= rectBounds.size() || !rectBounds[cmd.index].valid) continue;
          b = rectBounds[cmd.index];
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

  merged.offsets.assign(tileCount + 1, 0);
  for (uint32_t i = 0; i < tileCount; ++i) {
    merged.offsets[i + 1] = merged.offsets[i] + mergedCounts[i];
  }
  merged.commands.assign(merged.offsets.back(), TileCommand{});
  std::vector<uint32_t> mergedFill(tileCount, 0);

  auto emit_tile = [&](uint32_t tileIndex) {
    uint32_t tx = tileIndex % grid.tilesX;
    uint32_t ty = tileIndex / grid.tilesX;
    int32_t tileX0 = static_cast<int32_t>(tx * grid.tileSize);
    int32_t tileY0 = static_cast<int32_t>(ty * grid.tileSize);
    int32_t tileX1 = std::min(tileX0 + static_cast<int32_t>(grid.tileSize), static_cast<int32_t>(width));
    int32_t tileY1 = std::min(tileY0 + static_cast<int32_t>(grid.tileSize), static_cast<int32_t>(height));
    size_t tileCursor = src.offsets[tileIndex];
    size_t tileEnd = src.offsets[tileIndex + 1];
    uint32_t macroX = tx / MacroFactor;
    uint32_t macroY = ty / MacroFactor;
    uint32_t macroIndex = macroY * macroTilesX + macroX;
    size_t macroCursor = (*macroOffsets)[macroIndex];
    size_t macroEnd = (*macroOffsets)[macroIndex + 1];
    size_t globalCursor = 0;
    size_t globalEnd = src.globalCommands.size();
    int32_t macroOriginX = static_cast<int32_t>(macroX * MacroFactor * grid.tileSize);
    int32_t macroOriginY = static_cast<int32_t>(macroY * MacroFactor * grid.tileSize);
    while (tileCursor < tileEnd || macroCursor < macroEnd || globalCursor < globalEnd) {
      uint32_t bestOrder = 0xFFFFFFFFu;
      enum class Source : uint8_t { Tile, Macro, Global } srcType = Source::Tile;
      bool hasSrc = false;
      if (tileCursor < tileEnd) {
        bestOrder = src.commands[tileCursor].order;
        srcType = Source::Tile;
        hasSrc = true;
      }
      if (macroCursor < macroEnd) {
        uint32_t order = src.macroCommands[macroCursor].order;
        if (!hasSrc || order < bestOrder) {
          bestOrder = order;
          srcType = Source::Macro;
          hasSrc = true;
        }
      }
      if (globalCursor < globalEnd) {
        uint32_t order = src.globalCommands[globalCursor].order;
        if (!hasSrc || order < bestOrder) {
          bestOrder = order;
          srcType = Source::Global;
          hasSrc = true;
        }
      }
      if (!hasSrc) break;
      if (srcType == Source::Tile) {
        auto cmd = src.commands[tileCursor++];
        uint32_t offset = merged.offsets[tileIndex] + mergedFill[tileIndex]++;
        merged.commands[offset] = cmd;
      } else if (srcType == Source::Macro) {
        auto cmd = src.macroCommands[macroCursor++];
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
        if (localX0 > 255 || localY0 > 255 || localW > 256 || localH > 256) continue;
        TileCommand out{};
        out.type = cmd.type;
        out.index = cmd.index;
        out.order = cmd.order;
        out.x = static_cast<uint8_t>(localX0);
        out.y = static_cast<uint8_t>(localY0);
        out.wMinus1 = static_cast<uint8_t>(localW - 1);
        out.hMinus1 = static_cast<uint8_t>(localH - 1);
        uint32_t offset = merged.offsets[tileIndex] + mergedFill[tileIndex]++;
        merged.commands[offset] = out;
      } else {
        auto cmd = src.globalCommands[globalCursor++];
        CommandBounds b{};
        if (cmd.type == CommandType::Rect) {
          if (cmd.index >= rectBounds.size() || !rectBounds[cmd.index].valid) continue;
          b = rectBounds[cmd.index];
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
        if (localX0 > 255 || localY0 > 255 || localW > 256 || localH > 256) continue;
        TileCommand out{};
        out.type = cmd.type;
        out.index = cmd.index;
        out.order = cmd.order;
        out.x = static_cast<uint8_t>(localX0);
        out.y = static_cast<uint8_t>(localY0);
        out.wMinus1 = static_cast<uint8_t>(localW - 1);
        out.hMinus1 = static_cast<uint8_t>(localH - 1);
        uint32_t offset = merged.offsets[tileIndex] + mergedFill[tileIndex]++;
        merged.commands[offset] = out;
      }
    }
  };

  for (uint32_t tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
    emit_tile(tileIndex);
  }

  merged.enabled = true;
  merged.preMerged = true;
  return merged;
}

} // namespace

namespace {

void RenderImpl(RenderTarget target, RenderBatch const& batch) {
  if (target.width == 0 || target.height == 0) return;
  if (target.strideBytes == 0) return;
  if (target.data.size() < static_cast<size_t>(target.strideBytes) * target.height) return;

  if (!batch.palette.enabled || batch.palette.size == 0) return;
  auto fetch_color = [&](auto const& indices, uint32_t idx, uint32_t fallback) -> uint32_t {
    if (idx >= indices.size()) return fallback;
    uint8_t paletteIndex = indices[idx];
    if (paletteIndex >= batch.palette.size) return fallback;
    return batch.palette.colorRGBA8[paletteIndex];
  };

  uint32_t clearColor = 0;
  bool hasClear = false;
  for (auto const& cmd : batch.commands) {
    if (cmd.type != CommandType::Clear) continue;
    if (cmd.index < batch.clear.colorIndex.size()) {
      clearColor = fetch_color(batch.clear.colorIndex, cmd.index, clearColor);
      hasClear = true;
    }
  }

  uint32_t debugColor = 0;
  uint8_t debugLineWidth = 1;
  uint8_t debugFlags = 0;
  bool debugTiles = false;
  for (auto const& cmd : batch.commands) {
    if (cmd.type != CommandType::DebugTiles) continue;
    if (cmd.index < batch.debugTiles.colorIndex.size()) {
      debugColor = fetch_color(batch.debugTiles.colorIndex, cmd.index, debugColor);
      debugTiles = true;
      if (cmd.index < batch.debugTiles.lineWidth.size()) {
        debugLineWidth = std::max<uint8_t>(1, batch.debugTiles.lineWidth[cmd.index]);
      }
      if (cmd.index < batch.debugTiles.flags.size()) {
        debugFlags = batch.debugTiles.flags[cmd.index];
      }
    }
  }

  TileGrid grid = make_tile_grid(target.width, target.height, batch.tileSize);
  uint32_t tileCount = grid.tilesX * grid.tilesY;
  if (tileCount == 0) return;
  bool tilePow2 = (grid.tileSize & (grid.tileSize - 1)) == 0;
  uint32_t tileShift = 0;
  if (tilePow2) {
    while ((1u << tileShift) < grid.tileSize) {
      ++tileShift;
    }
  }

  bool useTileStream = batch.tileStream.enabled;
  TileStream const* tileStream = &batch.tileStream;
  TileStream mergedTileStream;
  if (useTileStream) {
    if (grid.tileSize > 256u) {
      useTileStream = false;
    } else if (batch.tileStream.offsets.size() != static_cast<size_t>(tileCount + 1) ||
               batch.tileStream.offsets.back() != batch.tileStream.commands.size()) {
      useTileStream = false;
    } else if (!batch.tileStream.preMerged) {
      mergedTileStream = premerge_tile_stream(batch, grid, target.width, target.height);
      if (!mergedTileStream.enabled) {
        useTileStream = false;
      } else {
        tileStream = &mergedTileStream;
      }
    }
  }
  bool useTileBuffer = useTileStream;

  if (hasClear && !useTileBuffer) {
    uint32_t packed = clearColor;
    uint8_t* base = target.data.data();
    if (target.strideBytes == target.width * 4 &&
        (reinterpret_cast<uintptr_t>(base) % alignof(uint32_t) == 0)) {
      auto* dst = reinterpret_cast<uint32_t*>(base);
      std::fill(dst, dst + static_cast<size_t>(target.width) * target.height, packed);
    } else {
      uint8_t r = static_cast<uint8_t>(clearColor & 0xFFu);
      uint8_t g = static_cast<uint8_t>((clearColor >> 8) & 0xFFu);
      uint8_t b = static_cast<uint8_t>((clearColor >> 16) & 0xFFu);
      uint8_t a = static_cast<uint8_t>((clearColor >> 24) & 0xFFu);
      for (uint32_t y = 0; y < target.height; ++y) {
        uint8_t* row = base + static_cast<size_t>(y) * target.strideBytes;
        if (reinterpret_cast<uintptr_t>(row) % alignof(uint32_t) == 0) {
          auto* row32 = reinterpret_cast<uint32_t*>(row);
          std::fill(row32, row32 + target.width, packed);
        } else {
          for (uint32_t x = 0; x < target.width; ++x) {
            size_t idx = static_cast<size_t>(4u * x);
            row[idx + 0] = r;
            row[idx + 1] = g;
            row[idx + 2] = b;
            row[idx + 3] = a;
          }
        }
      }
    }
  }

  bool hasDraw = false;
  if (useTileStream) {
    hasDraw = !tileStream->commands.empty();
  } else {
    for (auto const& cmd : batch.commands) {
      if (cmd.type == CommandType::Rect || cmd.type == CommandType::Text) {
        hasDraw = true;
        break;
      }
    }
  }
  if (useTileBuffer && hasClear) {
    hasDraw = true;
  }
  if (!hasDraw && !debugTiles) return;

  std::vector<uint32_t> tileCounts;
  struct CmdTileInfo {
    int32_t x0 = 0;
    int32_t y0 = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    uint32_t tx0 = 0;
    uint32_t ty0 = 0;
    uint32_t tx1 = 0;
    uint32_t ty1 = 0;
  };
  std::vector<CmdTileInfo> cmdTiles;
  std::vector<uint8_t> cmdActive;
  std::vector<uint32_t> tileOffsets;
  std::vector<uint32_t> tileRefs;
  std::vector<uint32_t> tileFill;
  std::vector<uint32_t> renderTiles;
  std::vector<uint8_t> textBaseAlpha;
  std::vector<uint8_t> textActive;
  std::vector<uint32_t> textPmOffset;
  std::vector<uint8_t> textPmRStore;
  std::vector<uint8_t> textPmGStore;
  std::vector<uint8_t> textPmBStore;
  std::vector<uint8_t> textColorR;
  std::vector<uint8_t> textColorG;
  std::vector<uint8_t> textColorB;
  std::vector<uint8_t> textColorA;
  std::vector<uint8_t> textClipEnabled;
  std::vector<int32_t> textClipX0;
  std::vector<int32_t> textClipY0;
  std::vector<int32_t> textClipX1;
  std::vector<int32_t> textClipY1;
  std::vector<uint8_t> rectBaseAlpha;
  std::vector<uint8_t> rectActive;
  std::vector<uint32_t> rectEdgeOffset;
  std::vector<uint8_t> rectEdgePmRStore;
  std::vector<uint8_t> rectEdgePmGStore;
  std::vector<uint8_t> rectEdgePmBStore;
  std::vector<uint8_t> rectHasGradient;
  std::vector<uint8_t> rectColorR;
  std::vector<uint8_t> rectColorG;
  std::vector<uint8_t> rectColorB;
  std::vector<uint8_t> rectColorA;
  std::vector<uint8_t> rectGradColorR;
  std::vector<uint8_t> rectGradColorG;
  std::vector<uint8_t> rectGradColorB;
  std::vector<uint8_t> rectGradColorA;
  std::vector<uint8_t> rectClipEnabled;
  std::vector<int32_t> rectClipX0;
  std::vector<int32_t> rectClipY0;
  std::vector<int32_t> rectClipX1;
  std::vector<int32_t> rectClipY1;
  std::vector<float> rectGradDirX;
  std::vector<float> rectGradDirY;
  std::vector<float> rectGradMin;
  std::vector<float> rectGradInvRange;
  constexpr uint32_t InvalidOffset = 0xFFFFFFFFu;

  if (hasDraw) {
    renderTiles.clear();
    textPmRStore.clear();
    textPmGStore.clear();
    textPmBStore.clear();
    rectEdgePmRStore.clear();
    rectEdgePmGStore.clear();
    rectEdgePmBStore.clear();

    size_t rectCount = std::min(batch.rects.colorIndex.size(), batch.rects.opacity.size());
    if (rectCount > 0) {
      rectBaseAlpha.assign(rectCount, 0);
      rectActive.assign(rectCount, 0);
      rectEdgeOffset.assign(rectCount, InvalidOffset);
      rectHasGradient.assign(rectCount, 0);
      rectColorR.assign(rectCount, 0);
      rectColorG.assign(rectCount, 0);
      rectColorB.assign(rectCount, 0);
      rectColorA.assign(rectCount, 0);
      rectGradColorR.assign(rectCount, 0);
      rectGradColorG.assign(rectCount, 0);
      rectGradColorB.assign(rectCount, 0);
      rectGradColorA.assign(rectCount, 0);
      rectClipEnabled.assign(rectCount, 0);
      rectClipX0.assign(rectCount, 0);
      rectClipY0.assign(rectCount, 0);
      rectClipX1.assign(rectCount, 0);
      rectClipY1.assign(rectCount, 0);
      rectGradDirX.assign(rectCount, 0.0f);
      rectGradDirY.assign(rectCount, 0.0f);
      rectGradMin.assign(rectCount, 0.0f);
      rectGradInvRange.assign(rectCount, 1.0f);
    }
    size_t textCount = std::min(batch.text.colorIndex.size(), batch.text.opacity.size());
    if (textCount > 0) {
      textBaseAlpha.assign(textCount, 0);
      textActive.assign(textCount, 0);
      textPmOffset.assign(textCount, InvalidOffset);
      textColorR.assign(textCount, 0);
      textColorG.assign(textCount, 0);
      textColorB.assign(textCount, 0);
      textColorA.assign(textCount, 0);
      textClipEnabled.assign(textCount, 0);
      textClipX0.assign(textCount, 0);
      textClipY0.assign(textCount, 0);
      textClipX1.assign(textCount, 0);
      textClipY1.assign(textCount, 0);
    }
    if (useTileStream) {
      renderTiles.reserve(tileCount);
      if (hasClear) {
        for (uint32_t i = 0; i < tileCount; ++i) {
          renderTiles.push_back(i);
        }
      } else {
        std::vector<uint8_t> tileMask(tileCount, 0);
        for (uint32_t i = 0; i < tileCount; ++i) {
          if (tileStream->offsets[i] != tileStream->offsets[i + 1]) {
            tileMask[i] = 1;
          }
        }
        for (uint32_t i = 0; i < tileCount; ++i) {
          if (tileMask[i]) renderTiles.push_back(i);
        }
      }

      auto mark_active = [&](auto const& cmdList) {
        for (auto const& cmd : cmdList) {
          if (cmd.type == CommandType::Rect) {
            if (cmd.index < rectActive.size()) {
              rectActive[cmd.index] = 1;
            }
          } else if (cmd.type == CommandType::Text) {
            if (cmd.index < textActive.size()) {
              textActive[cmd.index] = 1;
            }
          }
        }
      };
      mark_active(tileStream->commands);
    } else {
      tileCounts.assign(tileCount, 0);
      cmdTiles.assign(batch.commands.size(), CmdTileInfo{});
      cmdActive.assign(batch.commands.size(), 0);

      for (uint32_t i = 0; i < batch.commands.size(); ++i) {
        auto const& cmd = batch.commands[i];
        int32_t x0 = 0;
        int32_t y0 = 0;
        int32_t x1 = 0;
        int32_t y1 = 0;
        if (cmd.type == CommandType::Rect) {
          if (cmd.index >= batch.rects.x0.size() ||
              cmd.index >= batch.rects.y0.size() ||
              cmd.index >= batch.rects.x1.size() ||
              cmd.index >= batch.rects.y1.size() ||
              cmd.index >= batch.rects.colorIndex.size()) {
            continue;
          }
          uint8_t opacity = cmd.index < batch.rects.opacity.size() ? batch.rects.opacity[cmd.index] : 255u;
          if (opacity == 0) continue;
          uint32_t color = fetch_color(batch.rects.colorIndex, cmd.index, 0u);
          uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
          uint8_t flags = cmd.index < batch.rects.flags.size() ? batch.rects.flags[cmd.index] : 0u;
          if (opacity != 255u) {
            uint16_t combinedA = static_cast<uint16_t>(cA) * static_cast<uint16_t>(opacity);
            if ((combinedA + 127u) / 255u == 0u) continue;
          }
          bool hasGradient = (flags & RectFlagGradient) != 0u;
          if (!hasGradient) {
            if (cA == 0) continue;
          } else {
            if (cmd.index >= batch.rects.gradientColor1Index.size()) continue;
            uint32_t g1 = fetch_color(batch.rects.gradientColor1Index, cmd.index, 0u);
            uint8_t gA = static_cast<uint8_t>((g1 >> 24) & 0xFFu);
            if (cA == 0 && gA == 0) continue;
          }
          x0 = batch.rects.x0[cmd.index];
          y0 = batch.rects.y0[cmd.index];
          x1 = batch.rects.x1[cmd.index];
          y1 = batch.rects.y1[cmd.index];
          if ((flags & RectFlagClip) != 0u &&
              cmd.index < batch.rects.clipX0.size() &&
              cmd.index < batch.rects.clipY0.size() &&
              cmd.index < batch.rects.clipX1.size() &&
              cmd.index < batch.rects.clipY1.size()) {
            x0 = std::max(x0, static_cast<int32_t>(batch.rects.clipX0[cmd.index]));
            y0 = std::max(y0, static_cast<int32_t>(batch.rects.clipY0[cmd.index]));
            x1 = std::min(x1, static_cast<int32_t>(batch.rects.clipX1[cmd.index]));
            y1 = std::min(y1, static_cast<int32_t>(batch.rects.clipY1[cmd.index]));
            if (x1 <= x0 || y1 <= y0) continue;
          }
        } else if (cmd.type == CommandType::Text) {
          if (cmd.index >= batch.text.x.size() ||
              cmd.index >= batch.text.y.size() ||
              cmd.index >= batch.text.width.size() ||
              cmd.index >= batch.text.height.size() ||
              cmd.index >= batch.text.colorIndex.size() ||
              cmd.index >= batch.text.opacity.size()) {
            continue;
          }
          uint8_t opacity = batch.text.opacity[cmd.index];
          if (opacity == 0) continue;
          uint32_t color = fetch_color(batch.text.colorIndex, cmd.index, 0u);
          uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (opacity != 255u) {
            uint16_t combinedA = static_cast<uint16_t>(cA) * static_cast<uint16_t>(opacity);
            if ((combinedA + 127u) / 255u == 0u) continue;
          } else if (cA == 0) {
            continue;
          }
          x0 = batch.text.x[cmd.index];
          y0 = batch.text.y[cmd.index];
          x1 = x0 + batch.text.width[cmd.index];
          y1 = y0 + batch.text.height[cmd.index];
          uint8_t flags = cmd.index < batch.text.flags.size() ? batch.text.flags[cmd.index] : 0u;
          if ((flags & TextFlagClip) != 0u &&
              cmd.index < batch.text.clipX0.size() &&
              cmd.index < batch.text.clipY0.size() &&
              cmd.index < batch.text.clipX1.size() &&
              cmd.index < batch.text.clipY1.size()) {
            x0 = std::max(x0, static_cast<int32_t>(batch.text.clipX0[cmd.index]));
            y0 = std::max(y0, static_cast<int32_t>(batch.text.clipY0[cmd.index]));
            x1 = std::min(x1, static_cast<int32_t>(batch.text.clipX1[cmd.index]));
            y1 = std::min(y1, static_cast<int32_t>(batch.text.clipY1[cmd.index]));
            if (x1 <= x0 || y1 <= y0) continue;
          }
        } else {
          continue;
        }

        if (x1 <= 0 || y1 <= 0) continue;
        if (x0 >= static_cast<int32_t>(target.width) || y0 >= static_cast<int32_t>(target.height)) continue;
        int32_t clampedX0 = std::max<int32_t>(x0, 0);
        int32_t clampedY0 = std::max<int32_t>(y0, 0);
        int32_t clampedX1 = std::min<int32_t>(x1, static_cast<int32_t>(target.width));
        int32_t clampedY1 = std::min<int32_t>(y1, static_cast<int32_t>(target.height));
        if (clampedX1 <= clampedX0 || clampedY1 <= clampedY0) continue;

        uint32_t tx0 = 0;
        uint32_t ty0 = 0;
        uint32_t tx1 = 0;
        uint32_t ty1 = 0;
        if (tilePow2) {
          tx0 = static_cast<uint32_t>(clampedX0) >> tileShift;
          ty0 = static_cast<uint32_t>(clampedY0) >> tileShift;
          tx1 = static_cast<uint32_t>(clampedX1 - 1) >> tileShift;
          ty1 = static_cast<uint32_t>(clampedY1 - 1) >> tileShift;
        } else {
          tx0 = static_cast<uint32_t>(clampedX0) / grid.tileSize;
          ty0 = static_cast<uint32_t>(clampedY0) / grid.tileSize;
          tx1 = static_cast<uint32_t>(clampedX1 - 1) / grid.tileSize;
          ty1 = static_cast<uint32_t>(clampedY1 - 1) / grid.tileSize;
        }

        cmdActive[i] = 1;
        cmdTiles[i] = CmdTileInfo{clampedX0, clampedY0, clampedX1, clampedY1, tx0, ty0, tx1, ty1};
        if (cmd.type == CommandType::Rect && cmd.index < rectActive.size()) {
          rectActive[cmd.index] = 1;
        }
        if (cmd.type == CommandType::Text && cmd.index < textActive.size()) {
          textActive[cmd.index] = 1;
        }
        for (uint32_t ty = ty0; ty <= ty1; ++ty) {
          for (uint32_t tx = tx0; tx <= tx1; ++tx) {
            tileCounts[ty * grid.tilesX + tx] += 1;
          }
        }
      }

      tileOffsets.assign(tileCount + 1, 0);
      for (uint32_t i = 0; i < tileCount; ++i) {
        tileOffsets[i + 1] = tileOffsets[i] + tileCounts[i];
      }
      tileRefs.assign(tileOffsets.back(), 0);
      tileFill.assign(tileCount, 0);
      for (uint32_t i = 0; i < batch.commands.size(); ++i) {
        if (cmdActive[i] == 0) continue;
        auto const& info = cmdTiles[i];
        for (uint32_t ty = info.ty0; ty <= info.ty1; ++ty) {
          for (uint32_t tx = info.tx0; tx <= info.tx1; ++tx) {
            uint32_t tileIdx = ty * grid.tilesX + tx;
            uint32_t offset = tileOffsets[tileIdx] + tileFill[tileIdx]++;
            tileRefs[offset] = i;
          }
        }
      }
    }

    if (!rectActive.empty()) {
      for (uint32_t i = 0; i < rectActive.size(); ++i) {
        if (rectActive[i] == 0) continue;
        uint32_t color = fetch_color(batch.rects.colorIndex, i, 0u);
        uint8_t cR = static_cast<uint8_t>(color & 0xFFu);
        uint8_t cG = static_cast<uint8_t>((color >> 8) & 0xFFu);
        uint8_t cB = static_cast<uint8_t>((color >> 16) & 0xFFu);
        uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
        rectColorR[i] = cR;
        rectColorG[i] = cG;
        rectColorB[i] = cB;
        rectColorA[i] = cA;
        uint8_t opacity = batch.rects.opacity[i];
        uint8_t baseAlpha = apply_opacity(cA, opacity);
        rectBaseAlpha[i] = baseAlpha;
        uint8_t flags = i < batch.rects.flags.size() ? batch.rects.flags[i] : 0u;
        if ((flags & RectFlagClip) != 0u &&
            i < batch.rects.clipX0.size() &&
            i < batch.rects.clipY0.size() &&
            i < batch.rects.clipX1.size() &&
            i < batch.rects.clipY1.size()) {
          rectClipEnabled[i] = 1;
          rectClipX0[i] = batch.rects.clipX0[i];
          rectClipY0[i] = batch.rects.clipY0[i];
          rectClipX1[i] = batch.rects.clipX1[i];
          rectClipY1[i] = batch.rects.clipY1[i];
        }
        bool hasGradient = (flags & RectFlagGradient) != 0u;
        if (hasGradient) {
          if (i >= batch.rects.gradientColor1Index.size() ||
              i >= batch.rects.gradientDirX.size() ||
              i >= batch.rects.gradientDirY.size()) {
            hasGradient = false;
          }
        }
        if (!hasGradient && baseAlpha == 255u) {
          uint32_t offset = static_cast<uint32_t>(rectEdgePmRStore.size());
          rectEdgeOffset[i] = offset;
          rectEdgePmRStore.resize(static_cast<size_t>(offset) + 256);
          rectEdgePmGStore.resize(static_cast<size_t>(offset) + 256);
          rectEdgePmBStore.resize(static_cast<size_t>(offset) + 256);
          for (uint32_t cov = 0; cov < 256; ++cov) {
            rectEdgePmRStore[offset + cov] =
              static_cast<uint8_t>((static_cast<uint16_t>(cR) * cov + 127u) / 255u);
            rectEdgePmGStore[offset + cov] =
              static_cast<uint8_t>((static_cast<uint16_t>(cG) * cov + 127u) / 255u);
            rectEdgePmBStore[offset + cov] =
              static_cast<uint8_t>((static_cast<uint16_t>(cB) * cov + 127u) / 255u);
          }
        }
        if (hasGradient) {
          rectHasGradient[i] = 1;
          Vec2f gradDir{static_cast<float>(batch.rects.gradientDirX[i]) / 256.0f,
                        static_cast<float>(batch.rects.gradientDirY[i]) / 256.0f};
          gradDir = normalize_or_default(gradDir, Vec2f{0.0f, 1.0f});
          rectGradDirX[i] = gradDir.x;
          rectGradDirY[i] = gradDir.y;
          uint32_t g1 = fetch_color(batch.rects.gradientColor1Index, i, 0u);
          rectGradColorR[i] = static_cast<uint8_t>(g1 & 0xFFu);
          rectGradColorG[i] = static_cast<uint8_t>((g1 >> 8) & 0xFFu);
          rectGradColorB[i] = static_cast<uint8_t>((g1 >> 16) & 0xFFu);
          rectGradColorA[i] = static_cast<uint8_t>((g1 >> 24) & 0xFFu);
          if (i < batch.rects.x0.size() &&
              i < batch.rects.y0.size() &&
              i < batch.rects.x1.size() &&
              i < batch.rects.y1.size()) {
            float x0 = static_cast<float>(batch.rects.x0[i]);
            float y0 = static_cast<float>(batch.rects.y0[i]);
            float x1 = static_cast<float>(batch.rects.x1[i]);
            float y1 = static_cast<float>(batch.rects.y1[i]);
            Vec2f p0{x0, y0};
            Vec2f p1{x1, y0};
            Vec2f p2{x0, y1};
            Vec2f p3{x1, y1};
            float gradMin = std::min({dot(p0, gradDir), dot(p1, gradDir), dot(p2, gradDir), dot(p3, gradDir)});
            float gradMax = std::max({dot(p0, gradDir), dot(p1, gradDir), dot(p2, gradDir), dot(p3, gradDir)});
            if (std::abs(gradMax - gradMin) < 1e-5f) {
              rectGradMin[i] = 0.0f;
              rectGradInvRange[i] = 1.0f;
            } else {
              rectGradMin[i] = gradMin;
              rectGradInvRange[i] = 1.0f / (gradMax - gradMin);
            }
          }
        }

      }
    }

    if (!textActive.empty()) {
      for (uint32_t i = 0; i < textActive.size(); ++i) {
        if (textActive[i] == 0) continue;
        uint32_t color = fetch_color(batch.text.colorIndex, i, 0u);
        uint8_t cR = static_cast<uint8_t>(color & 0xFFu);
        uint8_t cG = static_cast<uint8_t>((color >> 8) & 0xFFu);
        uint8_t cB = static_cast<uint8_t>((color >> 16) & 0xFFu);
        uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
        textColorR[i] = cR;
        textColorG[i] = cG;
        textColorB[i] = cB;
        textColorA[i] = cA;
        uint8_t flags = i < batch.text.flags.size() ? batch.text.flags[i] : 0u;
        if ((flags & TextFlagClip) != 0u &&
            i < batch.text.clipX0.size() &&
            i < batch.text.clipY0.size() &&
            i < batch.text.clipX1.size() &&
            i < batch.text.clipY1.size()) {
          textClipEnabled[i] = 1;
          textClipX0[i] = batch.text.clipX0[i];
          textClipY0[i] = batch.text.clipY0[i];
          textClipX1[i] = batch.text.clipX1[i];
          textClipY1[i] = batch.text.clipY1[i];
        }
        uint8_t opacity = batch.text.opacity[i];
        uint8_t baseAlpha = apply_opacity(cA, opacity);
        textBaseAlpha[i] = baseAlpha;
        if (baseAlpha == 255u) {
          uint32_t offset = static_cast<uint32_t>(textPmRStore.size());
          textPmOffset[i] = offset;
          textPmRStore.resize(static_cast<size_t>(offset) + 256);
          textPmGStore.resize(static_cast<size_t>(offset) + 256);
          textPmBStore.resize(static_cast<size_t>(offset) + 256);
          for (uint32_t cov = 0; cov < 256; ++cov) {
            textPmRStore[offset + cov] =
              static_cast<uint8_t>((static_cast<uint16_t>(cR) * cov + 127u) / 255u);
            textPmGStore[offset + cov] =
              static_cast<uint8_t>((static_cast<uint16_t>(cG) * cov + 127u) / 255u);
            textPmBStore[offset + cov] =
              static_cast<uint8_t>((static_cast<uint16_t>(cB) * cov + 127u) / 255u);
          }
        }
      }
    }

    if (!useTileStream) {
      renderTiles.reserve(tileCount);
      for (uint32_t i = 0; i < tileCount; ++i) {
        if (tileCounts[i] != 0) renderTiles.push_back(i);
      }
    }
  }

  if (!hasDraw) {
    renderTiles.clear();
  }
  if (renderTiles.empty() && !debugTiles) return;

  auto render_tile = [&](uint32_t tileIndex) {
    uint32_t tx = tileIndex % grid.tilesX;
    uint32_t ty = tileIndex / grid.tilesX;
    uint32_t tx0 = tx * grid.tileSize;
    uint32_t ty0 = ty * grid.tileSize;
    uint32_t tx1 = std::min(tx0 + grid.tileSize, target.width);
    uint32_t ty1 = std::min(ty0 + grid.tileSize, target.height);

    bool frontToBack = useTileStream;
    uint32_t tileArea = (tx1 - tx0) * (ty1 - ty0);
    uint32_t opaqueCount = 0;
    uint8_t* surfaceBase = target.data.data();
    uint32_t surfaceStride = target.strideBytes;
    int32_t surfaceY0 = 0;
    if (useTileBuffer) {
      for (uint32_t y = ty0; y < ty1; ++y) {
        uint8_t* row = surfaceBase + static_cast<size_t>(y) * surfaceStride +
                       static_cast<size_t>(4 * tx0);
        std::memset(row, 0, static_cast<size_t>(tx1 - tx0) * 4);
      }
    }

    auto row_ptr = [&](int32_t y) -> uint8_t* {
      return surfaceBase + static_cast<size_t>(y - surfaceY0) * surfaceStride;
    };
    auto blend_px = [&](uint8_t* dst, uint8_t pmR, uint8_t pmG, uint8_t pmB, uint8_t srcA) {
      if (frontToBack) {
        uint8_t dstA = dst[3];
        if (dstA >= OpaqueAlphaCutoff) return;
        uint16_t invA = static_cast<uint16_t>(255u - dstA);
        dst[0] = static_cast<uint8_t>(
          (static_cast<uint16_t>(dst[0]) + (static_cast<uint16_t>(pmR) * invA + 127u) / 255u) & 0xFFu);
        dst[1] = static_cast<uint8_t>(
          (static_cast<uint16_t>(dst[1]) + (static_cast<uint16_t>(pmG) * invA + 127u) / 255u) & 0xFFu);
        dst[2] = static_cast<uint8_t>(
          (static_cast<uint16_t>(dst[2]) + (static_cast<uint16_t>(pmB) * invA + 127u) / 255u) & 0xFFu);
        uint8_t newA = static_cast<uint8_t>(
          (static_cast<uint16_t>(dstA) + (static_cast<uint16_t>(srcA) * invA + 127u) / 255u) & 0xFFu);
        dst[3] = newA;
        if (dstA < OpaqueAlphaCutoff && newA >= OpaqueAlphaCutoff) {
          ++opaqueCount;
        }
      } else {
        blend_premultiplied(dst, pmR, pmG, pmB, srcA);
      }
    };
    auto write_px = [&](uint8_t* dst, uint8_t r, uint8_t g, uint8_t b) {
      if (frontToBack) {
        uint8_t dstA = dst[3];
        if (dstA >= OpaqueAlphaCutoff) return;
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = 255u;
        ++opaqueCount;
      } else {
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = 255u;
      }
    };

    uint32_t start = 0;
    uint32_t end = 0;
    size_t tileCursor = 0;
    size_t tileEnd = 0;
    if (useTileStream) {
      tileCursor = tileStream->offsets[tileIndex];
      tileEnd = tileStream->offsets[tileIndex + 1];
    } else {
      start = tileOffsets[tileIndex];
      end = tileOffsets[tileIndex + 1];
    }

    auto next_tile_command = [&](CommandType& type,
                                 uint32_t& idx,
                                 bool& hasLocalBounds,
                                 int32_t& localX0,
                                 int32_t& localY0,
                                 int32_t& localX1,
                                 int32_t& localY1) -> bool {
      if (tileCursor >= tileEnd) return false;
      auto const& cmd = tileStream->commands[tileCursor++];
      type = cmd.type;
      idx = cmd.index;
      hasLocalBounds = true;
      localX0 = static_cast<int32_t>(tx0) + static_cast<int32_t>(cmd.x);
      localY0 = static_cast<int32_t>(ty0) + static_cast<int32_t>(cmd.y);
      localX1 = localX0 + static_cast<int32_t>(cmd.wMinus1) + 1;
      localY1 = localY0 + static_cast<int32_t>(cmd.hMinus1) + 1;
      if (localX1 <= localX0 || localY1 <= localY0) return false;
      return true;
    };

    for (uint32_t i = start;; ++i) {
      if (frontToBack && opaqueCount >= tileArea) break;
      CommandType type = CommandType::Rect;
      uint32_t idx = 0;
      bool hasLocalBounds = false;
      int32_t localX0 = 0;
      int32_t localY0 = 0;
      int32_t localX1 = 0;
      int32_t localY1 = 0;
      if (useTileStream) {
        if (!next_tile_command(type, idx, hasLocalBounds, localX0, localY0, localX1, localY1)) break;
      } else {
        if (i >= end) break;
        uint32_t cmdIndex = tileRefs[i];
        if (cmdIndex >= batch.commands.size()) continue;
        auto const& cmd = batch.commands[cmdIndex];
        type = cmd.type;
        idx = cmd.index;
      }

      if (type == CommandType::Rect) {
        if (idx >= batch.rects.x0.size() ||
            idx >= batch.rects.y0.size() ||
            idx >= batch.rects.x1.size() ||
            idx >= batch.rects.y1.size() ||
            idx >= batch.rects.colorIndex.size()) {
          continue;
        }

        int32_t x0 = batch.rects.x0[idx];
        int32_t y0 = batch.rects.y0[idx];
        int32_t x1 = batch.rects.x1[idx];
        int32_t y1 = batch.rects.y1[idx];

        int32_t drawX0 = hasLocalBounds ? localX0 : x0;
        int32_t drawY0 = hasLocalBounds ? localY0 : y0;
        int32_t drawX1 = hasLocalBounds ? localX1 : x1;
        int32_t drawY1 = hasLocalBounds ? localY1 : y1;

        int32_t rx0 = std::max<int32_t>(drawX0, static_cast<int32_t>(tx0));
        int32_t ry0 = std::max<int32_t>(drawY0, static_cast<int32_t>(ty0));
        int32_t rx1 = std::min<int32_t>(drawX1, static_cast<int32_t>(tx1));
        int32_t ry1 = std::min<int32_t>(drawY1, static_cast<int32_t>(ty1));
        if (rx1 <= rx0 || ry1 <= ry0) continue;

        uint16_t radiusQ = idx < batch.rects.radiusQ8_8.size() ? batch.rects.radiusQ8_8[idx] : 0;
        float radius = static_cast<float>(radiusQ) / 256.0f;
        int16_t rotationQ = idx < batch.rects.rotationQ8_8.size() ? batch.rects.rotationQ8_8[idx] : 0;
        float rotation = static_cast<float>(rotationQ) / 256.0f;
        uint8_t opacity = idx < batch.rects.opacity.size() ? batch.rects.opacity[idx] : 255u;
        uint8_t flags = idx < batch.rects.flags.size() ? batch.rects.flags[idx] : 0u;

        uint32_t color = fetch_color(batch.rects.colorIndex, idx, 0u);
        uint8_t cR = 0;
        uint8_t cG = 0;
        uint8_t cB = 0;
        uint8_t cA = 0;
        if (idx < rectColorR.size()) {
          cR = rectColorR[idx];
          cG = rectColorG[idx];
          cB = rectColorB[idx];
          cA = rectColorA[idx];
        } else {
          cR = static_cast<uint8_t>(color & 0xFFu);
          cG = static_cast<uint8_t>((color >> 8) & 0xFFu);
          cB = static_cast<uint8_t>((color >> 16) & 0xFFu);
          cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
        }

        uint8_t gR = cR;
        uint8_t gG = cG;
        uint8_t gB = cB;
        uint8_t gA = cA;
        Vec2f gradDir{0.0f, 1.0f};
        float gradMin = 0.0f;
        float gradInvRange = 1.0f;
        bool hasGradient = false;
        if (idx < rectHasGradient.size() && rectHasGradient[idx] != 0) {
          hasGradient = true;
          gradDir.x = rectGradDirX[idx];
          gradDir.y = rectGradDirY[idx];
          gradMin = rectGradMin[idx];
          gradInvRange = rectGradInvRange[idx];
        } else if ((flags & RectFlagGradient) != 0u &&
                   idx < batch.rects.gradientColor1Index.size() &&
                   idx < batch.rects.gradientDirX.size() &&
                   idx < batch.rects.gradientDirY.size()) {
          hasGradient = true;
          gradDir.x = static_cast<float>(batch.rects.gradientDirX[idx]) / 256.0f;
          gradDir.y = static_cast<float>(batch.rects.gradientDirY[idx]) / 256.0f;
          gradDir = normalize_or_default(gradDir, Vec2f{0.0f, 1.0f});
          Vec2f p0{static_cast<float>(x0), static_cast<float>(y0)};
          Vec2f p1{static_cast<float>(x1), static_cast<float>(y0)};
          Vec2f p2{static_cast<float>(x0), static_cast<float>(y1)};
          Vec2f p3{static_cast<float>(x1), static_cast<float>(y1)};
          float gmin = std::min({dot(p0, gradDir), dot(p1, gradDir), dot(p2, gradDir), dot(p3, gradDir)});
          float gmax = std::max({dot(p0, gradDir), dot(p1, gradDir), dot(p2, gradDir), dot(p3, gradDir)});
          if (std::abs(gmax - gmin) < 1e-5f) {
            gradMin = 0.0f;
            gradInvRange = 1.0f;
          } else {
            gradMin = gmin;
            gradInvRange = 1.0f / (gmax - gmin);
          }
        }
        if (hasGradient) {
          if (idx < rectGradColorR.size()) {
            gR = rectGradColorR[idx];
            gG = rectGradColorG[idx];
            gB = rectGradColorB[idx];
            gA = rectGradColorA[idx];
          } else if (idx < batch.rects.gradientColor1Index.size()) {
            uint32_t g1 = fetch_color(batch.rects.gradientColor1Index, idx, 0u);
            gR = static_cast<uint8_t>(g1 & 0xFFu);
            gG = static_cast<uint8_t>((g1 >> 8) & 0xFFu);
            gB = static_cast<uint8_t>((g1 >> 16) & 0xFFu);
            gA = static_cast<uint8_t>((g1 >> 24) & 0xFFu);
          } else {
            hasGradient = false;
          }
        } else {
          hasGradient = false;
        }
        if (!hasGradient) {
          if (opacity == 0 || cA == 0) continue;
        } else {
          if (opacity == 0 || (cA == 0 && gA == 0)) continue;
        }

        bool clipEnabled = false;
        IntRect clip{};
        if (idx < rectClipEnabled.size() && rectClipEnabled[idx] != 0u) {
          clipEnabled = true;
          clip.x0 = rectClipX0[idx];
          clip.y0 = rectClipY0[idx];
          clip.x1 = rectClipX1[idx];
          clip.y1 = rectClipY1[idx];
        } else if ((flags & RectFlagClip) != 0u &&
                   idx < batch.rects.clipX0.size() &&
                   idx < batch.rects.clipY0.size() &&
                   idx < batch.rects.clipX1.size() &&
                   idx < batch.rects.clipY1.size()) {
          clipEnabled = true;
          clip.x0 = batch.rects.clipX0[idx];
          clip.y0 = batch.rects.clipY0[idx];
          clip.x1 = batch.rects.clipX1[idx];
          clip.y1 = batch.rects.clipY1[idx];
        }
        if (clipEnabled) {
          if (clip.x1 <= drawX0 || clip.x0 >= drawX1 || clip.y1 <= drawY0 || clip.y0 >= drawY1) continue;
        }

        float cx = (static_cast<float>(x0) + static_cast<float>(x1)) * 0.5f;
        float cy = (static_cast<float>(y0) + static_cast<float>(y1)) * 0.5f;
        float hx = (static_cast<float>(x1) - static_cast<float>(x0)) * 0.5f;
        float hy = (static_cast<float>(y1) - static_cast<float>(y0)) * 0.5f;
        float cosA = std::cos(rotation);
        float sinA = std::sin(rotation);
        float gradStepX = 0.0f;
        float cRf = 0.0f;
        float cGf = 0.0f;
        float cBf = 0.0f;
        float cAf = 0.0f;
        float dRf = 0.0f;
        float dGf = 0.0f;
        float dBf = 0.0f;
        float dAf = 0.0f;
        if (hasGradient) {
          gradStepX = gradDir.x * gradInvRange;
          cRf = static_cast<float>(cR);
          cGf = static_cast<float>(cG);
          cBf = static_cast<float>(cB);
          cAf = static_cast<float>(cA);
          dRf = static_cast<float>(gR) - cRf;
          dGf = static_cast<float>(gG) - cGf;
          dBf = static_cast<float>(gB) - cBf;
          dAf = static_cast<float>(gA) - cAf;
        }
        uint8_t baseAlpha = 0;
        if (idx < rectBaseAlpha.size()) {
          baseAlpha = rectBaseAlpha[idx];
        } else {
          baseAlpha = apply_opacity(cA, opacity);
        }
        bool opaqueBase = (baseAlpha == 255);
        bool opaqueGradient = hasGradient && opacity == 255 && cA == 255 && gA == 255;
        const uint8_t* edgePmR = nullptr;
        const uint8_t* edgePmG = nullptr;
        const uint8_t* edgePmB = nullptr;
        std::array<uint8_t, 256> edgePmLocalR{};
        std::array<uint8_t, 256> edgePmLocalG{};
        std::array<uint8_t, 256> edgePmLocalB{};
        if (opaqueBase && !hasGradient) {
          if (idx < rectEdgeOffset.size()) {
            uint32_t offset = rectEdgeOffset[idx];
            if (offset != InvalidOffset) {
              edgePmR = rectEdgePmRStore.data() + offset;
              edgePmG = rectEdgePmGStore.data() + offset;
              edgePmB = rectEdgePmBStore.data() + offset;
            }
          }
          if (!edgePmR) {
            for (uint32_t i = 0; i < 256; ++i) {
              edgePmLocalR[i] = static_cast<uint8_t>((static_cast<uint16_t>(cR) * i + 127u) / 255u);
              edgePmLocalG[i] = static_cast<uint8_t>((static_cast<uint16_t>(cG) * i + 127u) / 255u);
              edgePmLocalB[i] = static_cast<uint8_t>((static_cast<uint16_t>(cB) * i + 127u) / 255u);
            }
            edgePmR = edgePmLocalR.data();
            edgePmG = edgePmLocalG.data();
            edgePmB = edgePmLocalB.data();
          }
        }

        if (!clipEnabled && radius <= 0.0f && rotation == 0.0f) {
          int32_t ix0 = rx0 + 1;
          int32_t iy0 = ry0 + 1;
          int32_t ix1 = rx1 - 1;
          int32_t iy1 = ry1 - 1;
          if (!hasGradient) {
            uint8_t finalA = baseAlpha;
            if (finalA != 0) {
              uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
              uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
              uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);

              if (ix1 > ix0 && iy1 > iy0) {
                for (int32_t y = iy0; y < iy1; ++y) {
                  uint8_t* row = row_ptr(y);
                  if (opaqueBase) {
                    uint8_t* px = row + static_cast<size_t>(4 * ix0);
                    if ((reinterpret_cast<uintptr_t>(px) % alignof(uint32_t)) == 0) {
                      auto* row32 = reinterpret_cast<uint32_t*>(px);
                      if (!frontToBack) {
                        std::fill(row32, row32 + (ix1 - ix0), color);
                      } else {
                        for (int32_t x = ix0; x < ix1; ++x, ++row32) {
                          uint8_t* dst = reinterpret_cast<uint8_t*>(row32);
                          write_px(dst, cR, cG, cB);
                        }
                      }
                    } else {
                      for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                        write_px(&row[static_cast<size_t>(offset)], cR, cG, cB);
                      }
                    }
                  } else {
                    for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                      blend_px(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                }
              }

              for (int32_t y = ry0; y < ry1; ++y) {
                uint8_t* row = row_ptr(y);
                float py = static_cast<float>(y) + 0.5f - cy;
                float absPy = std::abs(py);
                float dy = absPy - hy;
                for (int32_t x = rx0, offset = 4 * rx0; x < rx1; ++x, offset += 4) {
                  if (x >= ix0 && x < ix1 && y >= iy0 && y < iy1) continue;
                  float px = static_cast<float>(x) + 0.5f - cx;
                  float dx = std::abs(px) - hx;
                  float dist = std::max(dx, dy);
                  uint8_t coverage = coverage_from_dist(dist);
                  if (coverage == 0) continue;
                  if (opaqueBase) {
                    blend_px(&row[static_cast<size_t>(offset)], edgePmR[coverage], edgePmG[coverage],
                                        edgePmB[coverage], coverage);
                  } else {
                    uint8_t edgeA = apply_coverage(baseAlpha, coverage);
                    if (edgeA == 0) continue;
                    uint8_t edgeR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * edgeA + 127u) / 255u);
                    uint8_t edgeG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * edgeA + 127u) / 255u);
                    uint8_t edgeB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * edgeA + 127u) / 255u);
                    blend_px(&row[static_cast<size_t>(offset)], edgeR, edgeG, edgeB, edgeA);
                  }
                }
              }
            }
          } else {
            if (ix1 > ix0 && iy1 > iy0) {
              for (int32_t y = iy0; y < iy1; ++y) {
                uint8_t* row = row_ptr(y);
                float tRow = ((static_cast<float>(y) + 0.5f) * gradDir.y - gradMin) * gradInvRange;
                float t = tRow + (static_cast<float>(ix0) + 0.5f) * gradStepX;
                float tEnd = tRow + (static_cast<float>(ix1) - 0.5f) * gradStepX;
                float tMin = std::min(t, tEnd);
                float tMax = std::max(t, tEnd);
                bool needsClamp = (tMin < 0.0f || tMax > 1.0f);
                if (opaqueGradient) {
                  for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                    float clamped = needsClamp ? clamp01(t) : t;
                    uint8_t srcR = static_cast<uint8_t>(cRf + dRf * clamped + 0.5f);
                    uint8_t srcG = static_cast<uint8_t>(cGf + dGf * clamped + 0.5f);
                    uint8_t srcB = static_cast<uint8_t>(cBf + dBf * clamped + 0.5f);
                    write_px(&row[static_cast<size_t>(offset)], srcR, srcG, srcB);
                    t += gradStepX;
                  }
                } else {
                  for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                    float clamped = needsClamp ? clamp01(t) : t;
                    uint8_t srcR = static_cast<uint8_t>(cRf + dRf * clamped + 0.5f);
                    uint8_t srcG = static_cast<uint8_t>(cGf + dGf * clamped + 0.5f);
                    uint8_t srcB = static_cast<uint8_t>(cBf + dBf * clamped + 0.5f);
                    uint8_t srcA = static_cast<uint8_t>(cAf + dAf * clamped + 0.5f);
                    uint8_t finalA = apply_opacity(srcA, opacity);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(srcR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(srcG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(srcB) * finalA + 127u) / 255u);
                      blend_px(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                    t += gradStepX;
                  }
                }
              }
            }

            for (int32_t y = ry0; y < ry1; ++y) {
              uint8_t* row = row_ptr(y);
              float tRow = ((static_cast<float>(y) + 0.5f) * gradDir.y - gradMin) * gradInvRange;
              float t = tRow + (static_cast<float>(rx0) + 0.5f) * gradStepX;
              float tEnd = tRow + (static_cast<float>(rx1) - 0.5f) * gradStepX;
              float tMin = std::min(t, tEnd);
              float tMax = std::max(t, tEnd);
              bool needsClamp = (tMin < 0.0f || tMax > 1.0f);
              float py = static_cast<float>(y) + 0.5f - cy;
              float absPy = std::abs(py);
              float dy = absPy - hy;
              for (int32_t x = rx0, offset = 4 * rx0; x < rx1; ++x, offset += 4) {
                bool skipInterior = (x >= ix0 && x < ix1 && y >= iy0 && y < iy1);
                if (!skipInterior) {
                  float px = static_cast<float>(x) + 0.5f - cx;
                  float dx = std::abs(px) - hx;
                  float dist = std::max(dx, dy);
                  uint8_t coverage = coverage_from_dist(dist);
                  if (coverage > 0) {
                    float clamped = needsClamp ? clamp01(t) : t;
                    uint8_t srcR = static_cast<uint8_t>(cRf + dRf * clamped + 0.5f);
                    uint8_t srcG = static_cast<uint8_t>(cGf + dGf * clamped + 0.5f);
                    uint8_t srcB = static_cast<uint8_t>(cBf + dBf * clamped + 0.5f);
                    uint8_t srcA = static_cast<uint8_t>(cAf + dAf * clamped + 0.5f);
                    uint8_t baseA = opaqueGradient ? 255u : apply_opacity(srcA, opacity);
                    uint8_t edgeA = opaqueGradient ? coverage : apply_coverage(baseA, coverage);
                    if (edgeA != 0) {
                      uint8_t edgeR = static_cast<uint8_t>((static_cast<uint16_t>(srcR) * edgeA + 127u) / 255u);
                      uint8_t edgeG = static_cast<uint8_t>((static_cast<uint16_t>(srcG) * edgeA + 127u) / 255u);
                      uint8_t edgeB = static_cast<uint8_t>((static_cast<uint16_t>(srcB) * edgeA + 127u) / 255u);
                      blend_px(&row[static_cast<size_t>(offset)], edgeR, edgeG, edgeB, edgeA);
                    }
                  }
                }
                t += gradStepX;
              }
            }
          }
          continue;
        }

        bool fillInterior = (!clipEnabled && rotation == 0.0f && radius > 0.0f);
        int32_t ix0 = 0;
        int32_t iy0 = 0;
        int32_t ix1 = 0;
        int32_t iy1 = 0;
        if (fillInterior) {
          float inset = std::max(radius, 0.5f);
          int32_t fillX0 = static_cast<int32_t>(std::ceil(static_cast<float>(x0) + inset - 0.5f));
          int32_t fillY0 = static_cast<int32_t>(std::ceil(static_cast<float>(y0) + inset - 0.5f));
          int32_t fillX1 = static_cast<int32_t>(std::floor(static_cast<float>(x1) - inset - 0.5f) + 1.0f);
          int32_t fillY1 = static_cast<int32_t>(std::floor(static_cast<float>(y1) - inset - 0.5f) + 1.0f);
          ix0 = std::max(rx0, fillX0);
          iy0 = std::max(ry0, fillY0);
          ix1 = std::min(rx1, fillX1);
          iy1 = std::min(ry1, fillY1);
          if (ix1 > ix0 && iy1 > iy0) {
            if (!hasGradient) {
              uint8_t finalA = baseAlpha;
              if (finalA != 0) {
                uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
                uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
                uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
                for (int32_t y = iy0; y < iy1; ++y) {
                  uint8_t* row = row_ptr(y);
                  if (opaqueBase) {
                    uint8_t* px = row + static_cast<size_t>(4 * ix0);
                    if ((reinterpret_cast<uintptr_t>(px) % alignof(uint32_t)) == 0) {
                      auto* row32 = reinterpret_cast<uint32_t*>(px);
                      if (!frontToBack) {
                        std::fill(row32, row32 + (ix1 - ix0), color);
                      } else {
                        for (int32_t x = ix0; x < ix1; ++x, ++row32) {
                          uint8_t* dst = reinterpret_cast<uint8_t*>(row32);
                          write_px(dst, cR, cG, cB);
                        }
                      }
                    } else {
                      for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                        write_px(&row[static_cast<size_t>(offset)], cR, cG, cB);
                      }
                    }
                  } else {
                    for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                      blend_px(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                }
              }
            } else {
              for (int32_t y = iy0; y < iy1; ++y) {
                uint8_t* row = row_ptr(y);
                float tRow = ((static_cast<float>(y) + 0.5f) * gradDir.y - gradMin) * gradInvRange;
                float t = tRow + (static_cast<float>(ix0) + 0.5f) * gradStepX;
                float tEnd = tRow + (static_cast<float>(ix1) - 0.5f) * gradStepX;
                float tMin = std::min(t, tEnd);
                float tMax = std::max(t, tEnd);
                bool needsClamp = (tMin < 0.0f || tMax > 1.0f);
                if (opaqueGradient) {
                  for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                    float clamped = needsClamp ? clamp01(t) : t;
                    uint8_t srcR = static_cast<uint8_t>(cRf + dRf * clamped + 0.5f);
                    uint8_t srcG = static_cast<uint8_t>(cGf + dGf * clamped + 0.5f);
                    uint8_t srcB = static_cast<uint8_t>(cBf + dBf * clamped + 0.5f);
                    write_px(&row[static_cast<size_t>(offset)], srcR, srcG, srcB);
                    t += gradStepX;
                  }
                } else {
                  for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                    float clamped = needsClamp ? clamp01(t) : t;
                    uint8_t srcR = static_cast<uint8_t>(cRf + dRf * clamped + 0.5f);
                    uint8_t srcG = static_cast<uint8_t>(cGf + dGf * clamped + 0.5f);
                    uint8_t srcB = static_cast<uint8_t>(cBf + dBf * clamped + 0.5f);
                    uint8_t srcA = static_cast<uint8_t>(cAf + dAf * clamped + 0.5f);
                    uint8_t finalA = apply_opacity(srcA, opacity);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(srcR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(srcG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(srcB) * finalA + 127u) / 255u);
                      blend_px(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                    t += gradStepX;
                  }
                }
              }
            }
          } else {
            fillInterior = false;
          }
        }

        auto render_span = [&](int32_t y, int32_t sx0, int32_t sx1) {
          if (sx1 <= sx0) return;
          uint8_t* row = row_ptr(y);
          float tRow = 0.0f;
          float t = 0.0f;
          float tEnd = 0.0f;
          bool needsClamp = false;
          if (hasGradient) {
            tRow = ((static_cast<float>(y) + 0.5f) * gradDir.y - gradMin) * gradInvRange;
            t = tRow + (static_cast<float>(sx0) + 0.5f) * gradStepX;
            tEnd = tRow + (static_cast<float>(sx1) - 0.5f) * gradStepX;
            float tMin = std::min(t, tEnd);
            float tMax = std::max(t, tEnd);
            needsClamp = (tMin < 0.0f || tMax > 1.0f);
          }
          if (rotation == 0.0f) {
            float py = static_cast<float>(y) + 0.5f - cy;
            float absPy = std::abs(py);
            float dy = absPy - hy;
            for (int32_t x = sx0, offset = 4 * sx0; x < sx1; ++x, offset += 4) {
              float px = static_cast<float>(x) + 0.5f - cx;
              float dx = std::abs(px) - hx;
              float dist = std::max(dx, dy);
              uint8_t coverage = coverage_from_dist(dist);
              if (coverage > 0) {
                if (hasGradient) {
                  float clamped = needsClamp ? clamp01(t) : t;
                  uint8_t srcR = static_cast<uint8_t>(cRf + dRf * clamped + 0.5f);
                  uint8_t srcG = static_cast<uint8_t>(cGf + dGf * clamped + 0.5f);
                  uint8_t srcB = static_cast<uint8_t>(cBf + dBf * clamped + 0.5f);
                  uint8_t srcA = static_cast<uint8_t>(cAf + dAf * clamped + 0.5f);
                  if (opaqueGradient && coverage == 255) {
                    write_px(&row[static_cast<size_t>(offset)], srcR, srcG, srcB);
                  } else {
                    uint8_t baseA = opaqueGradient ? 255u : apply_opacity(srcA, opacity);
                    uint8_t finalA = opaqueGradient ? coverage : apply_coverage(baseA, coverage);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(srcR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(srcG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(srcB) * finalA + 127u) / 255u);
                      blend_px(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                } else {
                  if (opaqueBase && coverage == 255) {
                    write_px(&row[static_cast<size_t>(offset)], cR, cG, cB);
                  } else if (opaqueBase) {
                    blend_px(&row[static_cast<size_t>(offset)], edgePmR[coverage], edgePmG[coverage],
                                        edgePmB[coverage], coverage);
                  } else {
                    uint8_t finalA = apply_coverage(baseAlpha, coverage);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
                      blend_px(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                }
              }
              if (hasGradient) t += gradStepX;
            }
          } else {
            for (int32_t x = sx0, offset = 4 * sx0; x < sx1; ++x, offset += 4) {
              float px = static_cast<float>(x) + 0.5f - cx;
              float py = static_cast<float>(y) + 0.5f - cy;
              Vec2f p = rotate_point(Vec2f{px, py}, cosA, sinA);
              float dist = sdf_round_rect(p, hx, hy, radius);
              uint8_t coverage = coverage_from_dist(dist);
              if (coverage > 0) {
                if (hasGradient) {
                  float clamped = needsClamp ? clamp01(t) : t;
                  uint8_t srcR = static_cast<uint8_t>(cRf + dRf * clamped + 0.5f);
                  uint8_t srcG = static_cast<uint8_t>(cGf + dGf * clamped + 0.5f);
                  uint8_t srcB = static_cast<uint8_t>(cBf + dBf * clamped + 0.5f);
                  uint8_t srcA = static_cast<uint8_t>(cAf + dAf * clamped + 0.5f);
                  if (opaqueGradient && coverage == 255) {
                    write_px(&row[static_cast<size_t>(offset)], srcR, srcG, srcB);
                  } else {
                    uint8_t baseA = opaqueGradient ? 255u : apply_opacity(srcA, opacity);
                    uint8_t finalA = opaqueGradient ? coverage : apply_coverage(baseA, coverage);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(srcR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(srcG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(srcB) * finalA + 127u) / 255u);
                      blend_px(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                } else {
                  if (opaqueBase && coverage == 255) {
                    write_px(&row[static_cast<size_t>(offset)], cR, cG, cB);
                  } else if (opaqueBase) {
                    blend_px(&row[static_cast<size_t>(offset)], edgePmR[coverage], edgePmG[coverage],
                                        edgePmB[coverage], coverage);
                  } else {
                    uint8_t finalA = apply_coverage(baseAlpha, coverage);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
                      blend_px(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                }
              }
              if (hasGradient) t += gradStepX;
            }
          }
        };

        for (int32_t y = ry0; y < ry1; ++y) {
          if (clipEnabled && (y < clip.y0 || y >= clip.y1)) continue;
          if (!fillInterior || y < iy0 || y >= iy1) {
            render_span(y, rx0, rx1);
            continue;
          }
          render_span(y, rx0, ix0);
          render_span(y, ix1, rx1);
        }
      } else if (type == CommandType::Text) {
        if (idx >= batch.text.x.size() ||
            idx >= batch.text.y.size() ||
            idx >= batch.text.width.size() ||
            idx >= batch.text.height.size() ||
            idx >= batch.text.colorIndex.size() ||
            idx >= batch.text.opacity.size() ||
            idx >= batch.text.runIndex.size()) {
          continue;
        }

        int32_t x0 = batch.text.x[idx];
        int32_t y0 = batch.text.y[idx];
        int32_t x1 = x0 + batch.text.width[idx];
        int32_t y1 = y0 + batch.text.height[idx];
        uint32_t runIndex = batch.text.runIndex[idx];
        if (runIndex >= batch.runs.glyphStart.size() ||
            runIndex >= batch.runs.glyphCount.size() ||
            runIndex >= batch.runs.baselineQ8_8.size() ||
            runIndex >= batch.runs.scaleQ8_8.size()) {
          continue;
        }

        uint8_t flags = idx < batch.text.flags.size() ? batch.text.flags[idx] : 0u;
        bool clipEnabled = false;
        IntRect clip{};
        if (idx < textClipEnabled.size() && textClipEnabled[idx] != 0u) {
          clipEnabled = true;
          clip.x0 = textClipX0[idx];
          clip.y0 = textClipY0[idx];
          clip.x1 = textClipX1[idx];
          clip.y1 = textClipY1[idx];
        } else if ((flags & TextFlagClip) != 0u &&
                   idx < batch.text.clipX0.size() &&
                   idx < batch.text.clipY0.size() &&
                   idx < batch.text.clipX1.size() &&
                   idx < batch.text.clipY1.size()) {
          clipEnabled = true;
          clip.x0 = batch.text.clipX0[idx];
          clip.y0 = batch.text.clipY0[idx];
          clip.x1 = batch.text.clipX1[idx];
          clip.y1 = batch.text.clipY1[idx];
        }
        int32_t drawX0 = hasLocalBounds ? localX0 : x0;
        int32_t drawY0 = hasLocalBounds ? localY0 : y0;
        int32_t drawX1 = hasLocalBounds ? localX1 : x1;
        int32_t drawY1 = hasLocalBounds ? localY1 : y1;
        if (clipEnabled) {
          if (clip.x1 <= drawX0 || clip.x0 >= drawX1 || clip.y1 <= drawY0 || clip.y0 >= drawY1) continue;
        }

        uint32_t color = fetch_color(batch.text.colorIndex, idx, 0u);
        uint8_t cR = 0;
        uint8_t cG = 0;
        uint8_t cB = 0;
        uint8_t cA = 0;
        if (idx < textColorR.size()) {
          cR = textColorR[idx];
          cG = textColorG[idx];
          cB = textColorB[idx];
          cA = textColorA[idx];
        } else {
          cR = static_cast<uint8_t>(color & 0xFFu);
          cG = static_cast<uint8_t>((color >> 8) & 0xFFu);
          cB = static_cast<uint8_t>((color >> 16) & 0xFFu);
          cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
        }
        uint8_t opacity = batch.text.opacity[idx];
        uint8_t baseAlpha = 0;
        if (idx < textBaseAlpha.size()) {
          baseAlpha = textBaseAlpha[idx];
        } else {
          if (opacity == 0 || cA == 0) continue;
          baseAlpha = apply_opacity(cA, opacity);
        }
        if (baseAlpha == 0) continue;
        bool opaqueText = (baseAlpha == 255);
        const uint8_t* textPmR = nullptr;
        const uint8_t* textPmG = nullptr;
        const uint8_t* textPmB = nullptr;
        std::array<uint8_t, 256> textPmLocalR;
        std::array<uint8_t, 256> textPmLocalG;
        std::array<uint8_t, 256> textPmLocalB;
        if (opaqueText) {
          if (idx < textPmOffset.size()) {
            uint32_t offset = textPmOffset[idx];
            if (offset != InvalidOffset) {
              textPmR = textPmRStore.data() + offset;
              textPmG = textPmGStore.data() + offset;
              textPmB = textPmBStore.data() + offset;
            }
          }
          if (!textPmR) {
            for (uint32_t i = 0; i < 256; ++i) {
              textPmLocalR[i] = static_cast<uint8_t>((static_cast<uint16_t>(cR) * i + 127u) / 255u);
              textPmLocalG[i] = static_cast<uint8_t>((static_cast<uint16_t>(cG) * i + 127u) / 255u);
              textPmLocalB[i] = static_cast<uint8_t>((static_cast<uint16_t>(cB) * i + 127u) / 255u);
            }
            textPmR = textPmLocalR.data();
            textPmG = textPmLocalG.data();
            textPmB = textPmLocalB.data();
          }
        }

        uint32_t glyphStart = batch.runs.glyphStart[runIndex];
        uint32_t glyphCount = batch.runs.glyphCount[runIndex];
        float baseline = static_cast<float>(batch.runs.baselineQ8_8[runIndex]) / 256.0f;
        float scale = static_cast<float>(batch.runs.scaleQ8_8[runIndex]) / 256.0f;
        if (scale <= 0.0f || glyphCount == 0) continue;
        float baseY = static_cast<float>(y0) + baseline * scale;

        uint32_t glyphEnd = glyphStart + glyphCount;
        if (glyphEnd > batch.glyphs.glyphXQ8_8.size() ||
            glyphEnd > batch.glyphs.glyphYQ8_8.size() ||
            glyphEnd > batch.glyphs.bitmapIndex.size()) {
          continue;
        }

        for (uint32_t gi = glyphStart; gi < glyphEnd; ++gi) {
          uint32_t bitmapIndex = batch.glyphs.bitmapIndex[gi];
          if (bitmapIndex >= batch.glyphs.bitmaps.size()) continue;
          auto const& bmp = batch.glyphs.bitmaps[bitmapIndex];
          if (bmp.width <= 0 || bmp.height <= 0) continue;
          float gx = static_cast<float>(batch.glyphs.glyphXQ8_8[gi]) / 256.0f;
          float gy = static_cast<float>(batch.glyphs.glyphYQ8_8[gi]) / 256.0f;
          int32_t gx0 = static_cast<int32_t>(std::lround(static_cast<float>(x0) + gx * scale +
                                                        static_cast<float>(bmp.bearingX)));
          int32_t gy0 = static_cast<int32_t>(std::lround(baseY + gy * scale -
                                                        static_cast<float>(bmp.bearingY)));
          int32_t gx1 = gx0 + bmp.width;
          int32_t gy1 = gy0 + bmp.height;

          int32_t cx0 = std::max<int32_t>(gx0, static_cast<int32_t>(tx0));
          int32_t cy0 = std::max<int32_t>(gy0, static_cast<int32_t>(ty0));
          int32_t cx1 = std::min<int32_t>(gx1, static_cast<int32_t>(tx1));
          int32_t cy1 = std::min<int32_t>(gy1, static_cast<int32_t>(ty1));

          if (clipEnabled) {
            cx0 = std::max<int32_t>(cx0, clip.x0);
            cy0 = std::max<int32_t>(cy0, clip.y0);
            cx1 = std::min<int32_t>(cx1, clip.x1);
            cy1 = std::min<int32_t>(cy1, clip.y1);
          }

          if (cx1 <= cx0 || cy1 <= cy0) continue;

          if (opaqueText) {
            bool glyphOpaque = false;
            if (bitmapIndex < batch.glyphs.bitmapOpaque.size()) {
              glyphOpaque = batch.glyphs.bitmapOpaque[bitmapIndex] != 0u;
            } else if (bmp.atlasIndex < 0 && !bmp.pixels.empty()) {
              bool allOpaque = true;
              for (uint8_t v : bmp.pixels) {
                if (v != 255u) {
                  allOpaque = false;
                  break;
                }
              }
              glyphOpaque = allOpaque;
            }

            if (glyphOpaque) {
              for (int32_t y = cy0; y < cy1; ++y) {
                uint8_t* row = row_ptr(y) + static_cast<size_t>(4 * cx0);
                if ((reinterpret_cast<uintptr_t>(row) % alignof(uint32_t)) == 0) {
                  auto* row32 = reinterpret_cast<uint32_t*>(row);
                  if (!frontToBack) {
                    std::fill(row32, row32 + (cx1 - cx0), color);
                  } else {
                    for (int32_t x = cx0; x < cx1; ++x, ++row32) {
                      uint8_t* dst = reinterpret_cast<uint8_t*>(row32);
                      write_px(dst, cR, cG, cB);
                    }
                  }
                } else {
                  for (int32_t x = cx0; x < cx1; ++x, row += 4) {
                    write_px(row, cR, cG, cB);
                  }
                }
              }
              continue;
            }
          }

          const uint8_t* srcBase = nullptr;
          int32_t srcStride = bmp.stride;
          if (bmp.atlasIndex >= 0 && bmp.atlasIndex < static_cast<int32_t>(batch.glyphs.atlases.size())) {
            auto const& atlas = batch.glyphs.atlases[static_cast<size_t>(bmp.atlasIndex)];
            srcStride = atlas.stride;
            srcBase = atlas.pixels.data() + static_cast<size_t>(bmp.atlasY) * srcStride +
                      static_cast<size_t>(bmp.atlasX);
          } else {
            srcBase = bmp.pixels.data();
          }
          if (!srcBase || srcStride <= 0) continue;

          for (int32_t y = cy0; y < cy1; ++y) {
            int32_t srcRow = y - gy0;
            const uint8_t* src = srcBase + static_cast<size_t>(srcRow) * srcStride +
                                 static_cast<size_t>(cx0 - gx0);
            uint8_t* row = row_ptr(y) + static_cast<size_t>(4 * cx0);
            for (int32_t x = cx0; x < cx1; ++x, ++src, row += 4) {
              uint8_t cov = *src;
              if (cov == 0) continue;
              if (opaqueText) {
                if (cov == 255) {
                  write_px(row, cR, cG, cB);
                } else {
                  blend_px(row, textPmR[cov], textPmG[cov], textPmB[cov], cov);
                }
              } else {
                uint8_t finalA = apply_coverage(baseAlpha, cov);
                if (finalA == 0) continue;
                uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
                uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
                uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
                blend_px(row, pmR, pmG, pmB, finalA);
              }
            }
          }
        }
      }
    }

    if (useTileBuffer && hasClear && opaqueCount < tileArea) {
      uint8_t clearR = static_cast<uint8_t>(clearColor & 0xFFu);
      uint8_t clearG = static_cast<uint8_t>((clearColor >> 8) & 0xFFu);
      uint8_t clearB = static_cast<uint8_t>((clearColor >> 16) & 0xFFu);
      uint8_t clearA = static_cast<uint8_t>((clearColor >> 24) & 0xFFu);
      uint8_t clearPmR = static_cast<uint8_t>((static_cast<uint16_t>(clearR) * clearA + 127u) / 255u);
      uint8_t clearPmG = static_cast<uint8_t>((static_cast<uint16_t>(clearG) * clearA + 127u) / 255u);
      uint8_t clearPmB = static_cast<uint8_t>((static_cast<uint16_t>(clearB) * clearA + 127u) / 255u);
      for (uint32_t y = ty0; y < ty1; ++y) {
        uint8_t* dstRow = target.data.data() + static_cast<size_t>(y) * target.strideBytes +
                          static_cast<size_t>(4 * tx0);
        for (uint32_t x = tx0; x < tx1; ++x, dstRow += 4) {
          uint8_t srcA = dstRow[3];
          uint16_t invA = static_cast<uint16_t>(255u - srcA);
          dstRow[0] = static_cast<uint8_t>((static_cast<uint16_t>(dstRow[0]) +
                                            (static_cast<uint16_t>(clearPmR) * invA + 127u) / 255u) &
                                           0xFFu);
          dstRow[1] = static_cast<uint8_t>((static_cast<uint16_t>(dstRow[1]) +
                                            (static_cast<uint16_t>(clearPmG) * invA + 127u) / 255u) &
                                           0xFFu);
          dstRow[2] = static_cast<uint8_t>((static_cast<uint16_t>(dstRow[2]) +
                                            (static_cast<uint16_t>(clearPmB) * invA + 127u) / 255u) &
                                           0xFFu);
          dstRow[3] = static_cast<uint8_t>((static_cast<uint16_t>(srcA) +
                                            (static_cast<uint16_t>(clearA) * invA + 127u) / 255u) &
                                           0xFFu);
        }
      }
    }
  };

  if (!renderTiles.empty()) {
    if (renderTiles.size() <= 2) {
      for (uint32_t tileIndex : renderTiles) {
        render_tile(tileIndex);
      }
    } else {
      auto& pool = tile_pool();
      pool.run(static_cast<uint32_t>(renderTiles.size()), [&](uint32_t jobIndex) {
        uint32_t tileIndex = renderTiles[jobIndex];
        render_tile(tileIndex);
      });
    }
  }

  if (debugTiles) {
    uint8_t dR = static_cast<uint8_t>(debugColor & 0xFFu);
    uint8_t dG = static_cast<uint8_t>((debugColor >> 8) & 0xFFu);
    uint8_t dB = static_cast<uint8_t>((debugColor >> 16) & 0xFFu);
    uint8_t dA = static_cast<uint8_t>((debugColor >> 24) & 0xFFu);

    std::vector<uint32_t> outlineTiles;
    if ((debugFlags & DebugTilesFlagDirtyOnly) != 0u) {
      if (hasClear) {
        outlineTiles.reserve(tileCount);
        for (uint32_t i = 0; i < tileCount; ++i) outlineTiles.push_back(i);
      } else {
        outlineTiles = renderTiles;
      }
    } else {
      outlineTiles.reserve(tileCount);
      for (uint32_t i = 0; i < tileCount; ++i) outlineTiles.push_back(i);
    }

    for (uint32_t tileIndex : outlineTiles) {
      uint32_t tx = tileIndex % grid.tilesX;
      uint32_t ty = tileIndex / grid.tilesX;
      uint32_t tx0 = tx * grid.tileSize;
      uint32_t ty0 = ty * grid.tileSize;
      uint32_t tx1 = std::min(tx0 + grid.tileSize, target.width);
      uint32_t ty1 = std::min(ty0 + grid.tileSize, target.height);

      for (uint8_t line = 0; line < debugLineWidth; ++line) {
        uint32_t top = ty0 + line;
        uint32_t bottom = ty1 > 0 ? (ty1 - 1 - line) : 0;
        uint32_t left = tx0 + line;
        uint32_t right = tx1 > 0 ? (tx1 - 1 - line) : 0;
        if (top < ty1) {
          uint8_t* row = target.data.data() + static_cast<size_t>(top) * target.strideBytes;
          for (uint32_t x = tx0; x < tx1; ++x) {
            size_t idx = static_cast<size_t>(4u * x);
            row[idx + 0] = dR;
            row[idx + 1] = dG;
            row[idx + 2] = dB;
            row[idx + 3] = dA;
          }
        }
        if (bottom < ty1 && bottom != top) {
          uint8_t* row = target.data.data() + static_cast<size_t>(bottom) * target.strideBytes;
          for (uint32_t x = tx0; x < tx1; ++x) {
            size_t idx = static_cast<size_t>(4u * x);
            row[idx + 0] = dR;
            row[idx + 1] = dG;
            row[idx + 2] = dB;
            row[idx + 3] = dA;
          }
        }
        if (left < tx1) {
          for (uint32_t y = ty0; y < ty1; ++y) {
            uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
            size_t idx = static_cast<size_t>(4u * left);
            row[idx + 0] = dR;
            row[idx + 1] = dG;
            row[idx + 2] = dB;
            row[idx + 3] = dA;
          }
        }
        if (right < tx1 && right != left) {
          for (uint32_t y = ty0; y < ty1; ++y) {
            uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
            size_t idx = static_cast<size_t>(4u * right);
            row[idx + 0] = dR;
            row[idx + 1] = dG;
            row[idx + 2] = dB;
            row[idx + 3] = dA;
          }
        }
      }
    }
  }
}

} // namespace

void Render(RenderTarget target, RenderBatch const& batch) {
  RenderImpl(target, batch);
}

void Render(RenderTarget target, RenderBatch&& batch) {
  RenderImpl(target, batch);
}

} // namespace PrimeManifest
