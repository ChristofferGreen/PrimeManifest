#include "PrimeManifest/renderer/Optimizer2D.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace PrimeManifest {
namespace {

constexpr uint32_t MacroFactor = 2;

struct BinningPool {
  std::mutex mutex;
  std::condition_variable cv;
  std::condition_variable cvDone;
  bool shutdown = false;
  uint64_t workToken = 0;
  std::atomic<uint32_t> workDone{0};
  std::function<void(uint32_t)> job;
  std::vector<std::thread> workers;

  BinningPool() {
    uint32_t count = std::max(1u, std::thread::hardware_concurrency());
    if (count > 1u) {
      count -= 1u;
    }
    workers.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      workers.emplace_back([this, i]() { worker_loop(i + 1u); });
    }
  }

  ~BinningPool() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      shutdown = true;
    }
    cv.notify_all();
    for (auto& t : workers) {
      if (t.joinable()) t.join();
    }
  }

  uint32_t thread_count() const {
    return static_cast<uint32_t>(workers.size()) + 1u;
  }

  void run(std::function<void(uint32_t)> fn) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      job = std::move(fn);
      workDone.store(0);
      ++workToken;
    }
    cv.notify_all();

    job(0);

    std::unique_lock<std::mutex> lock(mutex);
    cvDone.wait(lock, [&]() { return workDone.load() >= workers.size(); });
  }

  void worker_loop(uint32_t index) {
    uint64_t lastToken = 0;
    for (;;) {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [&]() { return shutdown || workToken != lastToken; });
      if (shutdown) return;
      uint64_t token = workToken;
      auto fn = job;
      lock.unlock();
      fn(index);
      uint32_t done = workDone.fetch_add(1) + 1u;
      if (done >= workers.size()) {
        std::lock_guard<std::mutex> doneLock(mutex);
        cvDone.notify_one();
      }
      lastToken = token;
    }
  }
};

auto binning_pool() -> BinningPool& {
  thread_local BinningPool pool;
  return pool;
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

auto apply_opacity(uint8_t a, uint8_t opacity) -> uint8_t {
  uint16_t v = static_cast<uint16_t>(a) * static_cast<uint16_t>(opacity);
  v = static_cast<uint16_t>((v + 127u) / 255u);
  return static_cast<uint8_t>(std::min<uint16_t>(v, 255u));
}

auto count_command_types(RenderBatch const& batch) -> CommandTypeCounts {
  CommandTypeCounts counts{};
  for (auto const& cmd : batch.commands) {
    switch (cmd.type) {
      case CommandType::Clear:
        counts.clearCount += 1;
        break;
      case CommandType::Rect:
        counts.rect += 1;
        break;
      case CommandType::Circle:
        counts.circle += 1;
        break;
      case CommandType::Text:
        counts.text += 1;
        break;
      case CommandType::DebugTiles:
        counts.debugTiles += 1;
        break;
      case CommandType::ClearPattern:
        counts.clearPattern += 1;
        break;
    }
  }
  return counts;
}

auto choose_tile_size(RenderBatch const& batch, CommandTypeCounts const& counts) -> uint32_t {
  uint32_t tileSize = batch.tileSize == 0 ? 32u : batch.tileSize;
  if (!batch.autoTileStream) return tileSize;
  if (batch.tileStream.enabled) return tileSize;
  uint32_t drawCount = counts.drawCount();
  bool circleMajority = drawCount > 0 && (counts.circle * 2 > drawCount);
  bool circleOnly = circleMajority && counts.rect == 0 && counts.text == 0;
  if (tileSize == 32u && circleOnly) {
    return batch.reuseOptimized ? 16u : 64u;
  }
  return tileSize;
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

  std::vector<CommandBounds> circleBounds(batch.circles.centerX.size());
  for (uint32_t i = 0; i < batch.circles.centerX.size(); ++i) {
    CommandBounds b{};
    if (i >= batch.circles.centerY.size() ||
        i >= batch.circles.radius.size() ||
        i >= batch.circles.colorIndex.size()) {
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

auto optimize_batch(RenderTarget target,
                    RenderBatch const& batch,
                    OptimizedBatch& prepared,
                    uint32_t tileSizeOverride,
                    CommandTypeCounts const& commandCounts) -> bool {
  prepared.clear();
  if (target.width == 0 || target.height == 0) return false;
  if (target.strideBytes == 0) return false;
  if (target.data.size() < static_cast<size_t>(target.strideBytes) * target.height) return false;

  if (!batch.palette.enabled || batch.palette.size == 0) return false;
  bool paletteOpaque = true;
  for (uint16_t i = 0; i < batch.palette.size; ++i) {
    if ((batch.palette.colorRGBA8[i] & 0xFF000000u) != 0xFF000000u) {
      paletteOpaque = false;
      break;
    }
  }
  RendererProfile* profile = batch.profile;
  if (profile) {
    profile->clear();
  }
  auto buildStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  auto to_ns = [](auto start, auto end) -> uint64_t {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
  };
  auto fetch_color = [&](auto const& indices, uint32_t idx, uint32_t fallback) -> uint32_t {
    if (idx >= indices.size()) return fallback;
    uint8_t paletteIndex = indices[idx];
    if (paletteIndex >= batch.palette.size) return fallback;
    return batch.palette.colorRGBA8[paletteIndex];
  };

  auto gridStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  TileGrid grid = make_tile_grid(target.width, target.height, tileSizeOverride);
  uint32_t tileCount = grid.tilesX * grid.tilesY;
  if (tileCount == 0) return false;
  bool tilePow2 = (grid.tileSize & (grid.tileSize - 1)) == 0;
  uint32_t tileShift = 0;
  if (tilePow2) {
    while ((1u << tileShift) < grid.tileSize) {
      ++tileShift;
    }
  }
  if (profile) {
    profile->optTileGridNs = to_ns(gridStart, std::chrono::steady_clock::now());
  }

  auto scanStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  uint32_t clearColor = 0;
  bool hasClear = false;
  bool clearPattern = false;
  uint16_t clearPatternWidth = 0;
  uint16_t clearPatternHeight = 0;
  uint32_t clearPatternOffset = 0;
  if (commandCounts.clearCount == 1 && commandCounts.clearPattern == 0 &&
      !batch.commands.empty() && batch.commands.front().type == CommandType::Clear) {
    auto const& cmd = batch.commands.front();
    if (cmd.index < batch.clear.colorIndex.size()) {
      clearColor = fetch_color(batch.clear.colorIndex, cmd.index, clearColor);
      hasClear = true;
      clearPattern = false;
    }
  } else {
    for (auto const& cmd : batch.commands) {
      if (cmd.type == CommandType::Clear) {
        if (cmd.index < batch.clear.colorIndex.size()) {
          clearColor = fetch_color(batch.clear.colorIndex, cmd.index, clearColor);
          hasClear = true;
          clearPattern = false;
        }
      } else if (cmd.type == CommandType::ClearPattern) {
        if (cmd.index < batch.clearPattern.width.size() &&
            cmd.index < batch.clearPattern.height.size() &&
            cmd.index < batch.clearPattern.dataOffset.size()) {
          uint16_t width = batch.clearPattern.width[cmd.index];
          uint16_t height = batch.clearPattern.height[cmd.index];
          uint32_t offset = batch.clearPattern.dataOffset[cmd.index];
          if (width > 0 && height > 0 &&
              width <= grid.tileSize && height <= grid.tileSize) {
            size_t bytes = static_cast<size_t>(width) * height * 4u;
            if (static_cast<size_t>(offset) + bytes <= batch.clearPattern.data.size()) {
              clearPattern = true;
              clearPatternWidth = width;
              clearPatternHeight = height;
              clearPatternOffset = offset;
              hasClear = true;
            }
          }
        }
      }
    }
  }

  uint32_t debugColor = 0;
  uint8_t debugLineWidth = 1;
  uint8_t debugFlags = 0;
  bool debugTiles = false;
  if (commandCounts.debugTiles > 0) {
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
  }
  if (profile) {
    profile->optScanNs = to_ns(scanStart, std::chrono::steady_clock::now());
  }

  bool circleRadiusUniform = false;
  uint16_t circleRadiusValue = 0;
  size_t circleUniformCount = std::min({batch.circles.centerX.size(),
                                        batch.circles.centerY.size(),
                                        batch.circles.radius.size(),
                                        batch.circles.colorIndex.size()});
  if (circleUniformCount > 0) {
    circleRadiusValue = batch.circles.radius[0];
    circleRadiusUniform = true;
    for (size_t i = 1; i < circleUniformCount; ++i) {
      if (batch.circles.radius[i] != circleRadiusValue) {
        circleRadiusUniform = false;
        break;
      }
    }
  }
  prepared.circleRadiusUniform = circleRadiusUniform;
  prepared.circleRadiusValue = circleRadiusUniform ? circleRadiusValue : 0;

  auto tileStreamStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  bool useTileStream = batch.tileStream.enabled;
  TileStream const* tileStream = &batch.tileStream;
  if (useTileStream) {
    if (grid.tileSize > 256u) {
      useTileStream = false;
    } else if (batch.tileStream.offsets.size() != static_cast<size_t>(tileCount + 1) ||
               batch.tileStream.offsets.back() != batch.tileStream.commands.size()) {
      useTileStream = false;
    } else if (!batch.tileStream.preMerged) {
      if (profile) {
        auto premergeStart = std::chrono::steady_clock::now();
        prepared.mergedTileStream = premerge_tile_stream(batch, grid, target.width, target.height);
        auto premergeEnd = std::chrono::steady_clock::now();
        profile->premergeNs = static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(premergeEnd - premergeStart).count());
      } else {
        prepared.mergedTileStream = premerge_tile_stream(batch, grid, target.width, target.height);
      }
      if (!prepared.mergedTileStream.enabled) {
        useTileStream = false;
      } else {
        tileStream = &prepared.mergedTileStream;
      }
    }
  }
  bool useTileBuffer = useTileStream;
  if (profile) {
    profile->optTileStreamNs = to_ns(tileStreamStart, std::chrono::steady_clock::now());
  }
  bool allowAutoTileStream = batch.autoTileStream && !useTileStream && grid.tileSize <= 256u;
  uint32_t drawCount = commandCounts.drawCount();
  bool circleMajority = drawCount > 0 && (commandCounts.circle * 2 > drawCount);
  if (circleMajority && !batch.reuseOptimized) {
    allowAutoTileStream = false;
  }
  bool circleOnlyDraw = commandCounts.circle > 0 && commandCounts.rect == 0 && commandCounts.text == 0;
  bool useCircleRefs = circleOnlyDraw && !useTileStream && !allowAutoTileStream;

  bool hasDraw = false;
  if (useTileStream) {
    hasDraw = !tileStream->commands.empty();
  } else {
    for (auto const& cmd : batch.commands) {
      if (cmd.type == CommandType::Rect ||
          cmd.type == CommandType::Circle ||
          cmd.type == CommandType::Text) {
        hasDraw = true;
        break;
      }
    }
  }
  if (useTileBuffer && hasClear) {
    hasDraw = true;
  }
  if (!hasDraw && !debugTiles && !hasClear) return false;

  prepared.targetWidth = target.width;
  prepared.targetHeight = target.height;
  prepared.tileSize = grid.tileSize;
  prepared.tilesX = grid.tilesX;
  prepared.tilesY = grid.tilesY;
  prepared.tileCount = tileCount;
  prepared.tilePow2 = tilePow2;
  prepared.tileShift = tileShift;
  prepared.useTileStream = useTileStream;
  prepared.useTileBuffer = useTileBuffer;
  prepared.tileRefsAreCircleIndices = useCircleRefs;
  prepared.tileStream = useTileStream ? tileStream : nullptr;
  prepared.hasClear = hasClear;
  prepared.clearColor = clearColor;
  prepared.clearPattern = clearPattern;
  prepared.clearPatternWidth = clearPatternWidth;
  prepared.clearPatternHeight = clearPatternHeight;
  prepared.clearPatternOffset = clearPatternOffset;
  prepared.debugTiles = debugTiles;
  prepared.debugColor = debugColor;
  prepared.debugLineWidth = debugLineWidth;
  prepared.debugFlags = debugFlags;
  prepared.commandTypeCounts = commandCounts;

  auto& tileCounts = prepared.tileCounts;
  auto& cmdTiles = prepared.cmdTiles;
  auto& cmdActive = prepared.cmdActive;
  auto& tileOffsets = prepared.tileOffsets;
  auto& tileRefs = prepared.tileRefs;
  auto& tileFill = prepared.tileFill;
  auto& renderTiles = prepared.renderTiles;
  auto& textBaseAlpha = prepared.textBaseAlpha;
  auto& textActive = prepared.textActive;
  auto& textPmOffset = prepared.textPmOffset;
  auto& textPmRStore = prepared.textPmRStore;
  auto& textPmGStore = prepared.textPmGStore;
  auto& textPmBStore = prepared.textPmBStore;
  auto& textColorR = prepared.textColorR;
  auto& textColorG = prepared.textColorG;
  auto& textColorB = prepared.textColorB;
  auto& textColorA = prepared.textColorA;
  auto& textClipEnabled = prepared.textClipEnabled;
  auto& textClipX0 = prepared.textClipX0;
  auto& textClipY0 = prepared.textClipY0;
  auto& textClipX1 = prepared.textClipX1;
  auto& textClipY1 = prepared.textClipY1;
  auto& rectBaseAlpha = prepared.rectBaseAlpha;
  auto& rectActive = prepared.rectActive;
  auto& rectEdgeOffset = prepared.rectEdgeOffset;
  auto& rectEdgePmRStore = prepared.rectEdgePmRStore;
  auto& rectEdgePmGStore = prepared.rectEdgePmGStore;
  auto& rectEdgePmBStore = prepared.rectEdgePmBStore;
  auto& rectHasGradient = prepared.rectHasGradient;
  auto& rectColorR = prepared.rectColorR;
  auto& rectColorG = prepared.rectColorG;
  auto& rectColorB = prepared.rectColorB;
  auto& rectColorA = prepared.rectColorA;
  auto& rectGradColorR = prepared.rectGradColorR;
  auto& rectGradColorG = prepared.rectGradColorG;
  auto& rectGradColorB = prepared.rectGradColorB;
  auto& rectGradColorA = prepared.rectGradColorA;
  auto& rectClipEnabled = prepared.rectClipEnabled;
  auto& rectClipX0 = prepared.rectClipX0;
  auto& rectClipY0 = prepared.rectClipY0;
  auto& rectClipX1 = prepared.rectClipX1;
  auto& rectClipY1 = prepared.rectClipY1;
  auto& rectGradDirX = prepared.rectGradDirX;
  auto& rectGradDirY = prepared.rectGradDirY;
  auto& rectGradMin = prepared.rectGradMin;
  auto& rectGradInvRange = prepared.rectGradInvRange;
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
      auto renderTilesStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
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
      if (profile) {
        profile->optRenderTilesNs += to_ns(renderTilesStart, std::chrono::steady_clock::now());
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
      auto binStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
      tileCounts.assign(tileCount, 0);
      if (useCircleRefs) {
        size_t circleCount = std::min({batch.circles.centerX.size(),
                                       batch.circles.centerY.size(),
                                       batch.circles.radius.size(),
                                       batch.circles.colorIndex.size()});
        auto const* centerX = batch.circles.centerX.data();
        auto const* centerY = batch.circles.centerY.data();
        auto const* radius = batch.circles.radius.data();
        int32_t maxX = static_cast<int32_t>(target.width);
        int32_t maxY = static_cast<int32_t>(target.height);
        auto bin_circles = [&](auto&& compute_span) {
          for (uint32_t i = 0; i < circleCount; ++i) {
            uint32_t tx0 = 0;
            uint32_t ty0 = 0;
            uint32_t tx1 = 0;
            uint32_t ty1 = 0;
            if (!compute_span(i, tx0, ty0, tx1, ty1)) continue;
            if (tx0 == tx1 && ty0 == ty1) {
              tileCounts[ty0 * grid.tilesX + tx0] += 1;
            } else {
              for (uint32_t ty = ty0; ty <= ty1; ++ty) {
                for (uint32_t tx = tx0; tx <= tx1; ++tx) {
                  tileCounts[ty * grid.tilesX + tx] += 1;
                }
              }
            }
          }

          tileOffsets.assign(tileCount + 1, 0);
          for (uint32_t i = 0; i < tileCount; ++i) {
            tileOffsets[i + 1] = tileOffsets[i] + tileCounts[i];
          }
          tileRefs.assign(tileOffsets.back(), 0);
          tileFill.assign(tileCount, 0);
          for (uint32_t i = 0; i < circleCount; ++i) {
            uint32_t tx0 = 0;
            uint32_t ty0 = 0;
            uint32_t tx1 = 0;
            uint32_t ty1 = 0;
            if (!compute_span(i, tx0, ty0, tx1, ty1)) continue;
            if (tx0 == tx1 && ty0 == ty1) {
              uint32_t tileIdx = ty0 * grid.tilesX + tx0;
              uint32_t offset = tileOffsets[tileIdx] + tileFill[tileIdx]++;
              tileRefs[offset] = i;
            } else {
              for (uint32_t ty = ty0; ty <= ty1; ++ty) {
                for (uint32_t tx = tx0; tx <= tx1; ++tx) {
                  uint32_t tileIdx = ty * grid.tilesX + tx;
                  uint32_t offset = tileOffsets[tileIdx] + tileFill[tileIdx]++;
                  tileRefs[offset] = i;
                }
              }
            }
          }
        };

        if (paletteOpaque) {
          auto compute_span = [&](uint32_t i,
                                  uint32_t& tx0,
                                  uint32_t& ty0,
                                  uint32_t& tx1,
                                  uint32_t& ty1) -> bool {
            int32_t cx = centerX[i];
            int32_t cy = centerY[i];
            int32_t r = static_cast<int32_t>(radius[i]);
            int32_t x0 = cx - r;
            int32_t y0 = cy - r;
            int32_t x1 = cx + r + 1;
            int32_t y1 = cy + r + 1;
            if (x1 <= 0 || y1 <= 0) return false;
            if (x0 >= maxX || y0 >= maxY) return false;
            int32_t clampedX0 = std::max<int32_t>(x0, 0);
            int32_t clampedY0 = std::max<int32_t>(y0, 0);
            int32_t clampedX1 = std::min<int32_t>(x1, maxX);
            int32_t clampedY1 = std::min<int32_t>(y1, maxY);
            if (clampedX1 <= clampedX0 || clampedY1 <= clampedY0) return false;

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
            return true;
          };
          bin_circles(compute_span);
        } else {
          auto compute_span = [&](uint32_t i,
                                  uint32_t& tx0,
                                  uint32_t& ty0,
                                  uint32_t& tx1,
                                  uint32_t& ty1) -> bool {
            uint32_t color = fetch_color(batch.circles.colorIndex, i, 0u);
            uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
            if (cA == 0) return false;
            int32_t cx = centerX[i];
            int32_t cy = centerY[i];
            int32_t r = static_cast<int32_t>(radius[i]);
            int32_t x0 = cx - r;
            int32_t y0 = cy - r;
            int32_t x1 = cx + r + 1;
            int32_t y1 = cy + r + 1;
            if (x1 <= 0 || y1 <= 0) return false;
            if (x0 >= maxX || y0 >= maxY) return false;
            int32_t clampedX0 = std::max<int32_t>(x0, 0);
            int32_t clampedY0 = std::max<int32_t>(y0, 0);
            int32_t clampedX1 = std::min<int32_t>(x1, maxX);
            int32_t clampedY1 = std::min<int32_t>(y1, maxY);
            if (clampedX1 <= clampedX0 || clampedY1 <= clampedY0) return false;

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
            return true;
          };
          bin_circles(compute_span);
        }
      } else {
        cmdTiles.resize(batch.commands.size());
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
            uint8_t flags = cmd.index < batch.rects.flags.size() ? batch.rects.flags[cmd.index] : 0u;
            uint8_t opacity = cmd.index < batch.rects.opacity.size() ? batch.rects.opacity[cmd.index] : 255u;
            if (opacity == 0) continue;
            bool hasGradient = (flags & RectFlagGradient) != 0u;
            if (hasGradient && cmd.index >= batch.rects.gradientColor1Index.size()) continue;
            if (!paletteOpaque) {
              uint32_t color = fetch_color(batch.rects.colorIndex, cmd.index, 0u);
              uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
              if (opacity != 255u) {
                uint16_t combinedA = static_cast<uint16_t>(cA) * static_cast<uint16_t>(opacity);
                if ((combinedA + 127u) / 255u == 0u) continue;
              }
              if (!hasGradient) {
                if (cA == 0) continue;
              } else {
                uint32_t g1 = fetch_color(batch.rects.gradientColor1Index, cmd.index, 0u);
                uint8_t gA = static_cast<uint8_t>((g1 >> 24) & 0xFFu);
                if (cA == 0 && gA == 0) continue;
              }
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
          } else if (cmd.type == CommandType::Circle) {
            if (cmd.index >= batch.circles.centerX.size() ||
                cmd.index >= batch.circles.centerY.size() ||
                cmd.index >= batch.circles.radius.size() ||
                cmd.index >= batch.circles.colorIndex.size()) {
              continue;
            }
            if (!paletteOpaque) {
              uint32_t color = fetch_color(batch.circles.colorIndex, cmd.index, 0u);
              uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
              if (cA == 0) continue;
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
                cmd.index >= batch.text.height.size() ||
                cmd.index >= batch.text.colorIndex.size() ||
                cmd.index >= batch.text.opacity.size()) {
              continue;
            }
            uint8_t opacity = batch.text.opacity[cmd.index];
            if (opacity == 0) continue;
            if (!paletteOpaque) {
              uint32_t color = fetch_color(batch.text.colorIndex, cmd.index, 0u);
              uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
              if (opacity != 255u) {
                uint16_t combinedA = static_cast<uint16_t>(cA) * static_cast<uint16_t>(opacity);
                if ((combinedA + 127u) / 255u == 0u) continue;
              } else if (cA == 0) {
                continue;
              }
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
          cmdTiles[i] = OptimizedBatch::CmdTileInfo{clampedX0, clampedY0, clampedX1, clampedY1, tx0, ty0, tx1, ty1};
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
              if (useCircleRefs) {
                auto const& cmd = batch.commands[i];
                tileRefs[offset] = cmd.type == CommandType::Circle ? cmd.index : i;
              } else {
                tileRefs[offset] = i;
              }
            }
          }
        }
      }

      if (allowAutoTileStream) {
        TileStream generated;
        generated.offsets.assign(tileOffsets.begin(), tileOffsets.end());
        generated.commands.resize(tileRefs.size());
        for (uint32_t tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
          uint32_t tx = tileIndex % grid.tilesX;
          uint32_t ty = tileIndex / grid.tilesX;
          int32_t tx0 = static_cast<int32_t>(tx * grid.tileSize);
          int32_t ty0 = static_cast<int32_t>(ty * grid.tileSize);
          int32_t tx1 = std::min(tx0 + static_cast<int32_t>(grid.tileSize), static_cast<int32_t>(target.width));
          int32_t ty1 = std::min(ty0 + static_cast<int32_t>(grid.tileSize), static_cast<int32_t>(target.height));
          uint32_t start = tileOffsets[tileIndex];
          uint32_t end = tileOffsets[tileIndex + 1];
          for (uint32_t i = start; i < end; ++i) {
            uint32_t cmdIndex = tileRefs[i];
            if (cmdIndex >= batch.commands.size() || cmdIndex >= cmdTiles.size()) continue;
            auto const& cmd = batch.commands[cmdIndex];
            auto const& info = cmdTiles[cmdIndex];
            int32_t lx0 = std::max(info.x0, tx0);
            int32_t ly0 = std::max(info.y0, ty0);
            int32_t lx1 = std::min(info.x1, tx1);
            int32_t ly1 = std::min(info.y1, ty1);
            if (lx1 <= lx0 || ly1 <= ly0) continue;
            int32_t localX = lx0 - tx0;
            int32_t localY = ly0 - ty0;
            int32_t localW = lx1 - lx0;
            int32_t localH = ly1 - ly0;
            if (localX < 0 || localY < 0 || localW <= 0 || localH <= 0) continue;
            if (localX > 255 || localY > 255 || localW > 256 || localH > 256) continue;
            TileCommand out{};
            out.type = cmd.type;
            out.index = cmd.index;
            out.order = cmdIndex;
            out.x = static_cast<uint8_t>(localX);
            out.y = static_cast<uint8_t>(localY);
            out.wMinus1 = static_cast<uint8_t>(localW - 1);
            out.hMinus1 = static_cast<uint8_t>(localH - 1);
            generated.commands[i] = out;
          }
        }
        generated.enabled = true;
        generated.preMerged = true;
        prepared.generatedTileStream = std::move(generated);
        tileStream = &prepared.generatedTileStream;
        useTileStream = true;
        useTileBuffer = true;
        prepared.useTileStream = true;
        prepared.useTileBuffer = true;
        prepared.tileStream = tileStream;
        allowAutoTileStream = false;
      }

      if (profile) {
        profile->optTileBinningNs = to_ns(binStart, std::chrono::steady_clock::now());
      }

      auto renderTilesStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
      renderTiles.clear();
      renderTiles.reserve(tileCount);
      if (hasClear) {
        for (uint32_t i = 0; i < tileCount; ++i) {
          renderTiles.push_back(i);
        }
      } else {
        for (uint32_t i = 0; i < tileCount; ++i) {
          if (tileCounts[i] > 0) {
            renderTiles.push_back(i);
          }
        }
      }
      if (profile) {
        profile->optRenderTilesNs += to_ns(renderTilesStart, std::chrono::steady_clock::now());
      }
      if (useCircleRefs && renderTiles.size() > 1) {
        // Sorting tiles by load helps when there are few tiles, but for large
        // tile sets the sort cost outweighs the minor ordering benefit.
        if (renderTiles.size() <= 256) {
          std::sort(renderTiles.begin(), renderTiles.end(), [&](uint32_t a, uint32_t b) {
            return tileCounts[a] > tileCounts[b];
          });
        }
      }
    }

    auto rectCacheStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
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
          uint32_t g1 = fetch_color(batch.rects.gradientColor1Index, i, 0u);
          rectGradColorR[i] = static_cast<uint8_t>(g1 & 0xFFu);
          rectGradColorG[i] = static_cast<uint8_t>((g1 >> 8) & 0xFFu);
          rectGradColorB[i] = static_cast<uint8_t>((g1 >> 16) & 0xFFu);
          rectGradColorA[i] = static_cast<uint8_t>((g1 >> 24) & 0xFFu);
          Vec2f dir{static_cast<float>(batch.rects.gradientDirX[i]) / 256.0f,
                    static_cast<float>(batch.rects.gradientDirY[i]) / 256.0f};
          dir = normalize_or_default(dir, Vec2f{0.0f, 1.0f});
          rectGradDirX[i] = dir.x;
          rectGradDirY[i] = dir.y;
          int32_t x0 = batch.rects.x0[i];
          int32_t y0 = batch.rects.y0[i];
          int32_t x1 = batch.rects.x1[i];
          int32_t y1 = batch.rects.y1[i];
          Vec2f p0{static_cast<float>(x0), static_cast<float>(y0)};
          Vec2f p1{static_cast<float>(x1), static_cast<float>(y0)};
          Vec2f p2{static_cast<float>(x0), static_cast<float>(y1)};
          Vec2f p3{static_cast<float>(x1), static_cast<float>(y1)};
          float gmin = std::min({dot(p0, dir), dot(p1, dir), dot(p2, dir), dot(p3, dir)});
          float gmax = std::max({dot(p0, dir), dot(p1, dir), dot(p2, dir), dot(p3, dir)});
          if (std::abs(gmax - gmin) < 1e-5f) {
            rectGradMin[i] = 0.0f;
            rectGradInvRange[i] = 1.0f;
          } else {
            rectGradMin[i] = gmin;
            rectGradInvRange[i] = 1.0f / (gmax - gmin);
          }
        }
      }
    }
    if (profile) {
      profile->optRectCacheNs = to_ns(rectCacheStart, std::chrono::steady_clock::now());
    }

    auto textCacheStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
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
        uint8_t opacity = batch.text.opacity[i];
        uint8_t baseAlpha = apply_opacity(cA, opacity);
        textBaseAlpha[i] = baseAlpha;
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
    if (profile) {
      profile->optTextCacheNs = to_ns(textCacheStart, std::chrono::steady_clock::now());
    }
  }

  if (!hasDraw) {
    renderTiles.clear();
  }
  if (renderTiles.empty() && !debugTiles && !hasClear) return false;

  prepared.valid = true;
  if (profile) {
    profile->tileCount = tileCount;
    profile->activeTileCount = static_cast<uint32_t>(renderTiles.size());
    profile->commandCount = useTileStream ? static_cast<uint32_t>(tileStream->commands.size())
                                          : static_cast<uint32_t>(batch.commands.size());
    auto buildEnd = std::chrono::steady_clock::now();
    profile->buildNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(buildEnd - buildStart).count());
  }
  return true;
}

} // namespace

void OptimizeRenderBatch(RenderTarget target, RenderBatch const& batch, OptimizedBatch& optimized) {
  CommandTypeCounts commandCounts = count_command_types(batch);
  uint32_t tileSizeOverride = choose_tile_size(batch, commandCounts);
  if (batch.reuseOptimized && optimized.valid) {
    if (optimized.sourceRevision == batch.revision &&
        optimized.targetWidth == target.width &&
        optimized.targetHeight == target.height &&
        optimized.tileSize == tileSizeOverride) {
      return;
    }
  }
  optimize_batch(target, batch, optimized, tileSizeOverride, commandCounts);
  if (optimized.valid) {
    optimized.sourceRevision = batch.revision;
    if (batch.useCommandRevision) {
      optimized.commandCountsRevision = batch.commandRevision;
    }
  }
}

} // namespace PrimeManifest
