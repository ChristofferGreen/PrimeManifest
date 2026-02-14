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

auto apply_alpha(uint8_t a, uint8_t opacity, uint8_t coverage) -> uint8_t {
  return apply_coverage(apply_opacity(a, opacity), coverage);
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

auto rect_intersects_tile(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                          uint32_t tx0, uint32_t ty0, uint32_t tx1, uint32_t ty1) -> bool {
  if (x1 <= static_cast<int32_t>(tx0)) return false;
  if (x0 >= static_cast<int32_t>(tx1)) return false;
  if (y1 <= static_cast<int32_t>(ty0)) return false;
  if (y0 >= static_cast<int32_t>(ty1)) return false;
  return true;
}

} // namespace

namespace {

void RenderImpl(RenderTarget target, RenderBatch const& batch) {
  if (target.width == 0 || target.height == 0) return;
  if (target.strideBytes == 0) return;
  if (target.data.size() < static_cast<size_t>(target.strideBytes) * target.height) return;

  bool usePalette = batch.palette.enabled;
  auto color_count = [&](auto const& indices, auto const& colors) -> size_t {
    return usePalette ? indices.size() : colors.size();
  };
  auto fetch_color = [&](auto const& indices, auto const& colors, uint32_t idx, uint32_t fallback) -> uint32_t {
    if (usePalette) {
      if (idx >= indices.size()) return fallback;
      return batch.palette.colorRGBA8[indices[idx]];
    }
    if (idx >= colors.size()) return fallback;
    return colors[idx];
  };

  uint32_t clearColor = 0;
  bool hasClear = false;
  for (auto const& cmd : batch.commands) {
    if (cmd.type != CommandType::Clear) continue;
    if (cmd.index < color_count(batch.clear.colorIndex, batch.clear.colorRGBA8)) {
      clearColor = fetch_color(batch.clear.colorIndex, batch.clear.colorRGBA8, cmd.index, clearColor);
      hasClear = true;
    }
  }

  uint32_t debugColor = 0;
  uint8_t debugLineWidth = 1;
  uint8_t debugFlags = 0;
  bool debugTiles = false;
  for (auto const& cmd : batch.commands) {
    if (cmd.type != CommandType::DebugTiles) continue;
    if (cmd.index < color_count(batch.debugTiles.colorIndex, batch.debugTiles.colorRGBA8)) {
      debugColor = fetch_color(batch.debugTiles.colorIndex, batch.debugTiles.colorRGBA8, cmd.index, debugColor);
      debugTiles = true;
      if (cmd.index < batch.debugTiles.lineWidth.size()) {
        debugLineWidth = std::max<uint8_t>(1, batch.debugTiles.lineWidth[cmd.index]);
      }
      if (cmd.index < batch.debugTiles.flags.size()) {
        debugFlags = batch.debugTiles.flags[cmd.index];
      }
    }
  }

  if (hasClear) {
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

  bool hasDraw = false;
  for (auto const& cmd : batch.commands) {
    if (cmd.type == CommandType::Rect || cmd.type == CommandType::Text) {
      hasDraw = true;
      break;
    }
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

    size_t rectCount = std::min(color_count(batch.rects.colorIndex, batch.rects.colorRGBA8),
                                batch.rects.opacity.size());
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
    size_t textCount = std::min(color_count(batch.text.colorIndex, batch.text.colorRGBA8),
                                batch.text.opacity.size());
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
            cmd.index >= color_count(batch.rects.colorIndex, batch.rects.colorRGBA8)) {
          continue;
        }
        uint8_t opacity = cmd.index < batch.rects.opacity.size() ? batch.rects.opacity[cmd.index] : 255u;
        if (opacity == 0) continue;
        uint32_t color = fetch_color(batch.rects.colorIndex, batch.rects.colorRGBA8, cmd.index, 0u);
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
          if (cmd.index >= color_count(batch.rects.gradientColor1Index, batch.rects.gradientColor1RGBA8)) continue;
          uint32_t g1 =
              fetch_color(batch.rects.gradientColor1Index, batch.rects.gradientColor1RGBA8, cmd.index, 0u);
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
          x0 = std::max(x0, batch.rects.clipX0[cmd.index]);
          y0 = std::max(y0, batch.rects.clipY0[cmd.index]);
          x1 = std::min(x1, batch.rects.clipX1[cmd.index]);
          y1 = std::min(y1, batch.rects.clipY1[cmd.index]);
          if (x1 <= x0 || y1 <= y0) continue;
        }
      } else if (cmd.type == CommandType::Text) {
        if (cmd.index >= batch.text.x.size() ||
            cmd.index >= batch.text.y.size() ||
            cmd.index >= batch.text.width.size() ||
            cmd.index >= batch.text.height.size() ||
            cmd.index >= color_count(batch.text.colorIndex, batch.text.colorRGBA8) ||
            cmd.index >= batch.text.opacity.size()) {
          continue;
        }
        uint8_t opacity = batch.text.opacity[cmd.index];
        if (opacity == 0) continue;
        uint32_t color = fetch_color(batch.text.colorIndex, batch.text.colorRGBA8, cmd.index, 0u);
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
          x0 = std::max(x0, batch.text.clipX0[cmd.index]);
          y0 = std::max(y0, batch.text.clipY0[cmd.index]);
          x1 = std::min(x1, batch.text.clipX1[cmd.index]);
          y1 = std::min(y1, batch.text.clipY1[cmd.index]);
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

    if (!rectActive.empty()) {
      for (uint32_t i = 0; i < rectActive.size(); ++i) {
        if (rectActive[i] == 0) continue;
        uint32_t color = fetch_color(batch.rects.colorIndex, batch.rects.colorRGBA8, i, 0u);
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
          if (i >= color_count(batch.rects.gradientColor1Index, batch.rects.gradientColor1RGBA8) ||
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
          uint32_t g1 =
              fetch_color(batch.rects.gradientColor1Index, batch.rects.gradientColor1RGBA8, i, 0u);
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
        uint32_t color = fetch_color(batch.text.colorIndex, batch.text.colorRGBA8, i, 0u);
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

    renderTiles.reserve(tileCount);
    for (uint32_t i = 0; i < tileCount; ++i) {
      if (tileCounts[i] != 0) renderTiles.push_back(i);
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

    uint32_t start = tileOffsets[tileIndex];
    uint32_t end = tileOffsets[tileIndex + 1];
    for (uint32_t i = start; i < end; ++i) {
      uint32_t cmdIndex = tileRefs[i];
      if (cmdIndex >= batch.commands.size()) continue;
      auto const& cmd = batch.commands[cmdIndex];
      if (cmd.type == CommandType::Rect) {
        uint32_t idx = cmd.index;
        if (idx >= batch.rects.x0.size() ||
            idx >= batch.rects.y0.size() ||
            idx >= batch.rects.x1.size() ||
            idx >= batch.rects.y1.size() ||
            idx >= color_count(batch.rects.colorIndex, batch.rects.colorRGBA8)) {
          continue;
        }

        int32_t x0 = batch.rects.x0[idx];
        int32_t y0 = batch.rects.y0[idx];
        int32_t x1 = batch.rects.x1[idx];
        int32_t y1 = batch.rects.y1[idx];

        int32_t rx0 = std::max<int32_t>(x0, static_cast<int32_t>(tx0));
        int32_t ry0 = std::max<int32_t>(y0, static_cast<int32_t>(ty0));
        int32_t rx1 = std::min<int32_t>(x1, static_cast<int32_t>(tx1));
        int32_t ry1 = std::min<int32_t>(y1, static_cast<int32_t>(ty1));
        if (rx1 <= rx0 || ry1 <= ry0) continue;

        uint16_t radiusQ = idx < batch.rects.radiusQ8_8.size() ? batch.rects.radiusQ8_8[idx] : 0;
        float radius = static_cast<float>(radiusQ) / 256.0f;
        int16_t rotationQ = idx < batch.rects.rotationQ8_8.size() ? batch.rects.rotationQ8_8[idx] : 0;
        float rotation = static_cast<float>(rotationQ) / 256.0f;
        uint8_t opacity = idx < batch.rects.opacity.size() ? batch.rects.opacity[idx] : 255u;
        uint8_t flags = idx < batch.rects.flags.size() ? batch.rects.flags[idx] : 0u;

        uint32_t color = fetch_color(batch.rects.colorIndex, batch.rects.colorRGBA8, idx, 0u);
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
                   idx < color_count(batch.rects.gradientColor1Index, batch.rects.gradientColor1RGBA8) &&
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
          } else if (idx < color_count(batch.rects.gradientColor1Index, batch.rects.gradientColor1RGBA8)) {
            uint32_t g1 =
                fetch_color(batch.rects.gradientColor1Index, batch.rects.gradientColor1RGBA8, idx, 0u);
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
          if (clip.x1 <= x0 || clip.x0 >= x1 || clip.y1 <= y0 || clip.y0 >= y1) continue;
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
                  uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
                  if (opaqueBase) {
                    uint8_t* px = row + static_cast<size_t>(4 * ix0);
                    if ((reinterpret_cast<uintptr_t>(px) % alignof(uint32_t)) == 0) {
                      auto* row32 = reinterpret_cast<uint32_t*>(px);
                      std::fill(row32, row32 + (ix1 - ix0), color);
                    } else {
                      for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                        row[static_cast<size_t>(offset)] = cR;
                        row[static_cast<size_t>(offset + 1)] = cG;
                        row[static_cast<size_t>(offset + 2)] = cB;
                        row[static_cast<size_t>(offset + 3)] = 255u;
                      }
                    }
                  } else {
                    for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                      blend_premultiplied(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                }
              }

              for (int32_t y = ry0; y < ry1; ++y) {
                uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
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
                    blend_premultiplied(&row[static_cast<size_t>(offset)], edgePmR[coverage], edgePmG[coverage],
                                        edgePmB[coverage], coverage);
                  } else {
                    uint8_t edgeA = apply_coverage(baseAlpha, coverage);
                    if (edgeA == 0) continue;
                    uint8_t edgeR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * edgeA + 127u) / 255u);
                    uint8_t edgeG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * edgeA + 127u) / 255u);
                    uint8_t edgeB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * edgeA + 127u) / 255u);
                    blend_premultiplied(&row[static_cast<size_t>(offset)], edgeR, edgeG, edgeB, edgeA);
                  }
                }
              }
            }
          } else {
            if (ix1 > ix0 && iy1 > iy0) {
              for (int32_t y = iy0; y < iy1; ++y) {
                uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
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
                    row[static_cast<size_t>(offset)] = srcR;
                    row[static_cast<size_t>(offset + 1)] = srcG;
                    row[static_cast<size_t>(offset + 2)] = srcB;
                    row[static_cast<size_t>(offset + 3)] = 255u;
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
                      blend_premultiplied(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                    t += gradStepX;
                  }
                }
              }
            }

            for (int32_t y = ry0; y < ry1; ++y) {
              uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
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
                      blend_premultiplied(&row[static_cast<size_t>(offset)], edgeR, edgeG, edgeB, edgeA);
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
                  uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
                  if (opaqueBase) {
                    uint8_t* px = row + static_cast<size_t>(4 * ix0);
                    if ((reinterpret_cast<uintptr_t>(px) % alignof(uint32_t)) == 0) {
                      auto* row32 = reinterpret_cast<uint32_t*>(px);
                      std::fill(row32, row32 + (ix1 - ix0), color);
                    } else {
                      for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                        row[static_cast<size_t>(offset)] = cR;
                        row[static_cast<size_t>(offset + 1)] = cG;
                        row[static_cast<size_t>(offset + 2)] = cB;
                        row[static_cast<size_t>(offset + 3)] = 255u;
                      }
                    }
                  } else {
                    for (int32_t x = ix0, offset = 4 * ix0; x < ix1; ++x, offset += 4) {
                      blend_premultiplied(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                }
              }
            } else {
              for (int32_t y = iy0; y < iy1; ++y) {
                uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
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
                    row[static_cast<size_t>(offset)] = srcR;
                    row[static_cast<size_t>(offset + 1)] = srcG;
                    row[static_cast<size_t>(offset + 2)] = srcB;
                    row[static_cast<size_t>(offset + 3)] = 255u;
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
                      blend_premultiplied(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
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
          uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
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
                    row[static_cast<size_t>(offset)] = srcR;
                    row[static_cast<size_t>(offset + 1)] = srcG;
                    row[static_cast<size_t>(offset + 2)] = srcB;
                    row[static_cast<size_t>(offset + 3)] = 255u;
                  } else {
                    uint8_t baseA = opaqueGradient ? 255u : apply_opacity(srcA, opacity);
                    uint8_t finalA = opaqueGradient ? coverage : apply_coverage(baseA, coverage);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(srcR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(srcG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(srcB) * finalA + 127u) / 255u);
                      blend_premultiplied(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                } else {
                  if (opaqueBase && coverage == 255) {
                    row[static_cast<size_t>(offset)] = cR;
                    row[static_cast<size_t>(offset + 1)] = cG;
                    row[static_cast<size_t>(offset + 2)] = cB;
                    row[static_cast<size_t>(offset + 3)] = 255u;
                  } else if (opaqueBase) {
                    blend_premultiplied(&row[static_cast<size_t>(offset)], edgePmR[coverage], edgePmG[coverage],
                                        edgePmB[coverage], coverage);
                  } else {
                    uint8_t finalA = apply_coverage(baseAlpha, coverage);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
                      blend_premultiplied(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
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
                    row[static_cast<size_t>(offset)] = srcR;
                    row[static_cast<size_t>(offset + 1)] = srcG;
                    row[static_cast<size_t>(offset + 2)] = srcB;
                    row[static_cast<size_t>(offset + 3)] = 255u;
                  } else {
                    uint8_t baseA = opaqueGradient ? 255u : apply_opacity(srcA, opacity);
                    uint8_t finalA = opaqueGradient ? coverage : apply_coverage(baseA, coverage);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(srcR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(srcG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(srcB) * finalA + 127u) / 255u);
                      blend_premultiplied(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
                    }
                  }
                } else {
                  if (opaqueBase && coverage == 255) {
                    row[static_cast<size_t>(offset)] = cR;
                    row[static_cast<size_t>(offset + 1)] = cG;
                    row[static_cast<size_t>(offset + 2)] = cB;
                    row[static_cast<size_t>(offset + 3)] = 255u;
                  } else if (opaqueBase) {
                    blend_premultiplied(&row[static_cast<size_t>(offset)], edgePmR[coverage], edgePmG[coverage],
                                        edgePmB[coverage], coverage);
                  } else {
                    uint8_t finalA = apply_coverage(baseAlpha, coverage);
                    if (finalA != 0) {
                      uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
                      uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
                      uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
                      blend_premultiplied(&row[static_cast<size_t>(offset)], pmR, pmG, pmB, finalA);
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
      } else if (cmd.type == CommandType::Text) {
        uint32_t idx = cmd.index;
        if (idx >= batch.text.x.size() ||
            idx >= batch.text.y.size() ||
            idx >= batch.text.width.size() ||
            idx >= batch.text.height.size() ||
            idx >= color_count(batch.text.colorIndex, batch.text.colorRGBA8) ||
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
        if (clipEnabled) {
          if (clip.x1 <= x0 || clip.x0 >= x1 || clip.y1 <= y0 || clip.y0 >= y1) continue;
        }

        uint32_t color = fetch_color(batch.text.colorIndex, batch.text.colorRGBA8, idx, 0u);
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
                uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes +
                               static_cast<size_t>(4 * cx0);
                if ((reinterpret_cast<uintptr_t>(row) % alignof(uint32_t)) == 0) {
                  auto* row32 = reinterpret_cast<uint32_t*>(row);
                  std::fill(row32, row32 + (cx1 - cx0), color);
                } else {
                  for (int32_t x = cx0; x < cx1; ++x, row += 4) {
                    row[0] = cR;
                    row[1] = cG;
                    row[2] = cB;
                    row[3] = 255u;
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
            uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes +
                           static_cast<size_t>(4 * cx0);
            for (int32_t x = cx0; x < cx1; ++x, ++src, row += 4) {
              uint8_t cov = *src;
              if (cov == 0) continue;
              if (opaqueText) {
                if (cov == 255) {
                  row[0] = cR;
                  row[1] = cG;
                  row[2] = cB;
                  row[3] = 255u;
                } else {
                  blend_premultiplied(row, textPmR[cov], textPmG[cov], textPmB[cov], cov);
                }
              } else {
                uint8_t finalA = apply_coverage(baseAlpha, cov);
                if (finalA == 0) continue;
                uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
                uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
                uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
                blend_premultiplied(row, pmR, pmG, pmB, finalA);
              }
            }
          }
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
