#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <algorithm>
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

auto apply_alpha(uint8_t a, uint8_t opacity, uint8_t coverage) -> uint8_t {
  uint16_t v = static_cast<uint16_t>(a) * static_cast<uint16_t>(opacity);
  v = static_cast<uint16_t>((v + 127u) / 255u);
  v = static_cast<uint16_t>((v * static_cast<uint16_t>(coverage) + 127u) / 255u);
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
    for (;;) {
      uint32_t idx = nextWork.fetch_add(1);
      if (idx >= count) break;
      job(idx);
      uint32_t done = workDone.fetch_add(1) + 1;
      if (done == count) {
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
  grid.tileSize = tileSize == 0 ? 64u : tileSize;
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

  uint32_t clearColor = 0;
  bool hasClear = false;
  for (auto const& cmd : batch.commands) {
    if (cmd.type != CommandType::Clear) continue;
    if (cmd.index < batch.clear.colorRGBA8.size()) {
      clearColor = batch.clear.colorRGBA8[cmd.index];
      hasClear = true;
    }
  }

  uint32_t debugColor = 0;
  uint8_t debugLineWidth = 1;
  uint8_t debugFlags = 0;
  bool debugTiles = false;
  for (auto const& cmd : batch.commands) {
    if (cmd.type != CommandType::DebugTiles) continue;
    if (cmd.index < batch.debugTiles.colorRGBA8.size()) {
      debugColor = batch.debugTiles.colorRGBA8[cmd.index];
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
    uint8_t r = static_cast<uint8_t>(clearColor & 0xFFu);
    uint8_t g = static_cast<uint8_t>((clearColor >> 8) & 0xFFu);
    uint8_t b = static_cast<uint8_t>((clearColor >> 16) & 0xFFu);
    uint8_t a = static_cast<uint8_t>((clearColor >> 24) & 0xFFu);
    for (uint32_t y = 0; y < target.height; ++y) {
      uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
      for (uint32_t x = 0; x < target.width; ++x) {
        size_t idx = static_cast<size_t>(4u * x);
        row[idx + 0] = r;
        row[idx + 1] = g;
        row[idx + 2] = b;
        row[idx + 3] = a;
      }
    }
  }

  TileGrid grid = make_tile_grid(target.width, target.height, batch.tileSize);
  uint32_t tileCount = grid.tilesX * grid.tilesY;
  if (tileCount == 0) return;

  std::vector<uint32_t> tileCounts(tileCount, 0);
  auto add_command_to_tiles = [&](uint32_t cmdIndex, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (x1 <= 0 || y1 <= 0) return;
    if (x0 >= static_cast<int32_t>(target.width) || y0 >= static_cast<int32_t>(target.height)) return;
    int32_t clampedX0 = std::max<int32_t>(x0, 0);
    int32_t clampedY0 = std::max<int32_t>(y0, 0);
    int32_t clampedX1 = std::min<int32_t>(x1, static_cast<int32_t>(target.width));
    int32_t clampedY1 = std::min<int32_t>(y1, static_cast<int32_t>(target.height));
    if (clampedX1 <= clampedX0 || clampedY1 <= clampedY0) return;
    uint32_t tx0 = static_cast<uint32_t>(clampedX0) / grid.tileSize;
    uint32_t ty0 = static_cast<uint32_t>(clampedY0) / grid.tileSize;
    uint32_t tx1 = static_cast<uint32_t>(clampedX1 - 1) / grid.tileSize;
    uint32_t ty1 = static_cast<uint32_t>(clampedY1 - 1) / grid.tileSize;
    for (uint32_t ty = ty0; ty <= ty1; ++ty) {
      for (uint32_t tx = tx0; tx <= tx1; ++tx) {
        tileCounts[ty * grid.tilesX + tx] += 1;
      }
    }
  };

  for (uint32_t i = 0; i < batch.commands.size(); ++i) {
    auto const& cmd = batch.commands[i];
    if (cmd.type == CommandType::Rect) {
      if (cmd.index >= batch.rects.x0.size() ||
          cmd.index >= batch.rects.y0.size() ||
          cmd.index >= batch.rects.x1.size() ||
          cmd.index >= batch.rects.y1.size()) {
        continue;
      }
      add_command_to_tiles(i, batch.rects.x0[cmd.index], batch.rects.y0[cmd.index],
                           batch.rects.x1[cmd.index], batch.rects.y1[cmd.index]);
    } else if (cmd.type == CommandType::Text) {
      if (cmd.index >= batch.text.x.size() ||
          cmd.index >= batch.text.y.size() ||
          cmd.index >= batch.text.width.size() ||
          cmd.index >= batch.text.height.size()) {
        continue;
      }
      int32_t x0 = batch.text.x[cmd.index];
      int32_t y0 = batch.text.y[cmd.index];
      int32_t x1 = x0 + batch.text.width[cmd.index];
      int32_t y1 = y0 + batch.text.height[cmd.index];
      add_command_to_tiles(i, x0, y0, x1, y1);
    }
  }

  std::vector<uint32_t> tileOffsets(tileCount + 1, 0);
  for (uint32_t i = 0; i < tileCount; ++i) {
    tileOffsets[i + 1] = tileOffsets[i] + tileCounts[i];
  }
  std::vector<uint32_t> tileRefs(tileOffsets.back(), 0);
  std::vector<uint32_t> tileFill(tileCount, 0);
  auto push_ref = [&](uint32_t cmdIndex, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (x1 <= 0 || y1 <= 0) return;
    if (x0 >= static_cast<int32_t>(target.width) || y0 >= static_cast<int32_t>(target.height)) return;
    int32_t clampedX0 = std::max<int32_t>(x0, 0);
    int32_t clampedY0 = std::max<int32_t>(y0, 0);
    int32_t clampedX1 = std::min<int32_t>(x1, static_cast<int32_t>(target.width));
    int32_t clampedY1 = std::min<int32_t>(y1, static_cast<int32_t>(target.height));
    if (clampedX1 <= clampedX0 || clampedY1 <= clampedY0) return;
    uint32_t tx0 = static_cast<uint32_t>(clampedX0) / grid.tileSize;
    uint32_t ty0 = static_cast<uint32_t>(clampedY0) / grid.tileSize;
    uint32_t tx1 = static_cast<uint32_t>(clampedX1 - 1) / grid.tileSize;
    uint32_t ty1 = static_cast<uint32_t>(clampedY1 - 1) / grid.tileSize;
    for (uint32_t ty = ty0; ty <= ty1; ++ty) {
      for (uint32_t tx = tx0; tx <= tx1; ++tx) {
        uint32_t tileIdx = ty * grid.tilesX + tx;
        uint32_t offset = tileOffsets[tileIdx] + tileFill[tileIdx]++;
        tileRefs[offset] = cmdIndex;
      }
    }
  };

  for (uint32_t i = 0; i < batch.commands.size(); ++i) {
    auto const& cmd = batch.commands[i];
    if (cmd.type == CommandType::Rect) {
      if (cmd.index >= batch.rects.x0.size() ||
          cmd.index >= batch.rects.y0.size() ||
          cmd.index >= batch.rects.x1.size() ||
          cmd.index >= batch.rects.y1.size()) {
        continue;
      }
      push_ref(i, batch.rects.x0[cmd.index], batch.rects.y0[cmd.index],
               batch.rects.x1[cmd.index], batch.rects.y1[cmd.index]);
    } else if (cmd.type == CommandType::Text) {
      if (cmd.index >= batch.text.x.size() ||
          cmd.index >= batch.text.y.size() ||
          cmd.index >= batch.text.width.size() ||
          cmd.index >= batch.text.height.size()) {
        continue;
      }
      int32_t x0 = batch.text.x[cmd.index];
      int32_t y0 = batch.text.y[cmd.index];
      int32_t x1 = x0 + batch.text.width[cmd.index];
      int32_t y1 = y0 + batch.text.height[cmd.index];
      push_ref(i, x0, y0, x1, y1);
    }
  }

  std::vector<uint32_t> renderTiles;
  renderTiles.reserve(tileCount);
  for (uint32_t i = 0; i < tileCount; ++i) {
    if (tileCounts[i] != 0) renderTiles.push_back(i);
  }

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
            idx >= batch.rects.colorRGBA8.size()) {
          continue;
        }

        int32_t x0 = batch.rects.x0[idx];
        int32_t y0 = batch.rects.y0[idx];
        int32_t x1 = batch.rects.x1[idx];
        int32_t y1 = batch.rects.y1[idx];

        if (!rect_intersects_tile(x0, y0, x1, y1, tx0, ty0, tx1, ty1)) continue;

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

        uint32_t color = batch.rects.colorRGBA8[idx];
        uint8_t cR = static_cast<uint8_t>(color & 0xFFu);
        uint8_t cG = static_cast<uint8_t>((color >> 8) & 0xFFu);
        uint8_t cB = static_cast<uint8_t>((color >> 16) & 0xFFu);
        uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);

        uint8_t gR = cR;
        uint8_t gG = cG;
        uint8_t gB = cB;
        uint8_t gA = cA;
        Vec2f gradDir{0.0f, 1.0f};
        float gradMin = 0.0f;
        float gradMax = 1.0f;
        bool hasGradient = (flags & RectFlagGradient) != 0u;
        if (hasGradient && idx < batch.rects.gradientColor1RGBA8.size() &&
            idx < batch.rects.gradientDirX.size() &&
            idx < batch.rects.gradientDirY.size()) {
          uint32_t g1 = batch.rects.gradientColor1RGBA8[idx];
          gR = static_cast<uint8_t>(g1 & 0xFFu);
          gG = static_cast<uint8_t>((g1 >> 8) & 0xFFu);
          gB = static_cast<uint8_t>((g1 >> 16) & 0xFFu);
          gA = static_cast<uint8_t>((g1 >> 24) & 0xFFu);
          gradDir.x = static_cast<float>(batch.rects.gradientDirX[idx]) / 256.0f;
          gradDir.y = static_cast<float>(batch.rects.gradientDirY[idx]) / 256.0f;
          gradDir = normalize_or_default(gradDir, Vec2f{0.0f, 1.0f});
          Vec2f p0{static_cast<float>(x0), static_cast<float>(y0)};
          Vec2f p1{static_cast<float>(x1), static_cast<float>(y0)};
          Vec2f p2{static_cast<float>(x0), static_cast<float>(y1)};
          Vec2f p3{static_cast<float>(x1), static_cast<float>(y1)};
          gradMin = std::min({dot(p0, gradDir), dot(p1, gradDir), dot(p2, gradDir), dot(p3, gradDir)});
          gradMax = std::max({dot(p0, gradDir), dot(p1, gradDir), dot(p2, gradDir), dot(p3, gradDir)});
          if (std::abs(gradMax - gradMin) < 1e-5f) {
            gradMin = 0.0f;
            gradMax = 1.0f;
          }
        } else {
          hasGradient = false;
        }

        bool clipEnabled = (flags & RectFlagClip) != 0u;
        IntRect clip{};
        if (clipEnabled && idx < batch.rects.clipX0.size() &&
            idx < batch.rects.clipY0.size() &&
            idx < batch.rects.clipX1.size() &&
            idx < batch.rects.clipY1.size()) {
          clip.x0 = batch.rects.clipX0[idx];
          clip.y0 = batch.rects.clipY0[idx];
          clip.x1 = batch.rects.clipX1[idx];
          clip.y1 = batch.rects.clipY1[idx];
        } else {
          clipEnabled = false;
        }

        float cx = (static_cast<float>(x0) + static_cast<float>(x1)) * 0.5f;
        float cy = (static_cast<float>(y0) + static_cast<float>(y1)) * 0.5f;
        float hx = (static_cast<float>(x1) - static_cast<float>(x0)) * 0.5f;
        float hy = (static_cast<float>(y1) - static_cast<float>(y0)) * 0.5f;
        float cosA = std::cos(rotation);
        float sinA = std::sin(rotation);

        for (int32_t y = ry0; y < ry1; ++y) {
          if (clipEnabled && (y < clip.y0 || y >= clip.y1)) continue;
          uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
          for (int32_t x = rx0; x < rx1; ++x) {
            if (clipEnabled && (x < clip.x0 || x >= clip.x1)) continue;
            float px = static_cast<float>(x) + 0.5f - cx;
            float py = static_cast<float>(y) + 0.5f - cy;
            Vec2f p = rotation == 0.0f ? Vec2f{px, py} : rotate_point(Vec2f{px, py}, cosA, sinA);
            float dist = sdf_round_rect(p, hx, hy, radius);
            float cov = clamp01(0.5f - dist / DefaultAa);
            if (cov <= 0.0f) continue;
            uint8_t coverage = static_cast<uint8_t>(std::min(255.0f, cov * 255.0f + 0.5f));

            uint8_t srcR = cR;
            uint8_t srcG = cG;
            uint8_t srcB = cB;
            uint8_t srcA = cA;

            if (hasGradient) {
              Vec2f pt{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
              float t = (dot(pt, gradDir) - gradMin) / (gradMax - gradMin);
              t = clamp01(t);
              srcR = static_cast<uint8_t>(std::lround(static_cast<float>(cR) + (static_cast<float>(gR) - cR) * t));
              srcG = static_cast<uint8_t>(std::lround(static_cast<float>(cG) + (static_cast<float>(gG) - cG) * t));
              srcB = static_cast<uint8_t>(std::lround(static_cast<float>(cB) + (static_cast<float>(gB) - cB) * t));
              srcA = static_cast<uint8_t>(std::lround(static_cast<float>(cA) + (static_cast<float>(gA) - cA) * t));
            }

            uint8_t finalA = apply_alpha(srcA, opacity, coverage);
            if (finalA == 0) continue;
            uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(srcR) * finalA + 127u) / 255u);
            uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(srcG) * finalA + 127u) / 255u);
            uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(srcB) * finalA + 127u) / 255u);
            blend_premultiplied(&row[static_cast<size_t>(4 * x)], pmR, pmG, pmB, finalA);
          }
        }
      } else if (cmd.type == CommandType::Text) {
        uint32_t idx = cmd.index;
        if (idx >= batch.text.x.size() ||
            idx >= batch.text.y.size() ||
            idx >= batch.text.width.size() ||
            idx >= batch.text.height.size() ||
            idx >= batch.text.colorRGBA8.size() ||
            idx >= batch.text.opacity.size() ||
            idx >= batch.text.runIndex.size()) {
          continue;
        }

        int32_t x0 = batch.text.x[idx];
        int32_t y0 = batch.text.y[idx];
        int32_t x1 = x0 + batch.text.width[idx];
        int32_t y1 = y0 + batch.text.height[idx];
        if (!rect_intersects_tile(x0, y0, x1, y1, tx0, ty0, tx1, ty1)) continue;

        uint32_t runIndex = batch.text.runIndex[idx];
        if (runIndex >= batch.runs.glyphStart.size() ||
            runIndex >= batch.runs.glyphCount.size() ||
            runIndex >= batch.runs.baselineQ8_8.size() ||
            runIndex >= batch.runs.scaleQ8_8.size()) {
          continue;
        }

        uint8_t flags = idx < batch.text.flags.size() ? batch.text.flags[idx] : 0u;
        bool clipEnabled = (flags & TextFlagClip) != 0u;
        IntRect clip{};
        if (clipEnabled && idx < batch.text.clipX0.size() &&
            idx < batch.text.clipY0.size() &&
            idx < batch.text.clipX1.size() &&
            idx < batch.text.clipY1.size()) {
          clip.x0 = batch.text.clipX0[idx];
          clip.y0 = batch.text.clipY0[idx];
          clip.x1 = batch.text.clipX1[idx];
          clip.y1 = batch.text.clipY1[idx];
        } else {
          clipEnabled = false;
        }

        uint32_t color = batch.text.colorRGBA8[idx];
        uint8_t cR = static_cast<uint8_t>(color & 0xFFu);
        uint8_t cG = static_cast<uint8_t>((color >> 8) & 0xFFu);
        uint8_t cB = static_cast<uint8_t>((color >> 16) & 0xFFu);
        uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
        uint8_t opacity = batch.text.opacity[idx];

        uint32_t glyphStart = batch.runs.glyphStart[runIndex];
        uint32_t glyphCount = batch.runs.glyphCount[runIndex];
        float baseline = static_cast<float>(batch.runs.baselineQ8_8[runIndex]) / 256.0f;
        float scale = static_cast<float>(batch.runs.scaleQ8_8[runIndex]) / 256.0f;
        if (scale <= 0.0f) scale = 1.0f;
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
            const uint8_t* src = srcBase + static_cast<size_t>(srcRow) * srcStride;
            uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
            for (int32_t x = cx0; x < cx1; ++x) {
              int32_t srcX = x - gx0;
              uint8_t cov = src[srcX];
              if (cov == 0) continue;
              uint8_t finalA = apply_alpha(cA, opacity, cov);
              if (finalA == 0) continue;
              uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
              uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
              uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
              blend_premultiplied(&row[static_cast<size_t>(4 * x)], pmR, pmG, pmB, finalA);
            }
          }
        }
      }
    }
  };

  if (!renderTiles.empty()) {
    auto& pool = tile_pool();
    pool.run(static_cast<uint32_t>(renderTiles.size()), [&](uint32_t jobIndex) {
      uint32_t tileIndex = renderTiles[jobIndex];
      render_tile(tileIndex);
    });
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
