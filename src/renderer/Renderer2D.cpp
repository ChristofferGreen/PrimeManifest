#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
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
constexpr int32_t MaxCircleMaskRadius = 8;

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

auto make_mul_table() -> std::array<std::array<uint8_t, 256>, 256> {
  std::array<std::array<uint8_t, 256>, 256> table{};
  for (uint32_t a = 0; a < 256; ++a) {
    for (uint32_t v = 0; v < 256; ++v) {
      table[a][v] = static_cast<uint8_t>((v * a + 127u) / 255u);
    }
  }
  return table;
}

auto const kMulTable = make_mul_table();

inline auto mul_div_255(uint8_t v, uint8_t a) -> uint8_t {
  return kMulTable[a][v];
}

struct CircleMaskCache {
  std::array<std::vector<uint8_t>, MaxCircleMaskRadius + 1> masks{};
  std::array<std::vector<uint8_t>, MaxCircleMaskRadius + 1> edgeX{};
  std::array<std::vector<uint8_t>, MaxCircleMaskRadius + 1> edgeCov{};
  std::array<std::vector<uint16_t>, MaxCircleMaskRadius + 1> edgeOffset{};
  std::array<std::vector<int8_t>, MaxCircleMaskRadius + 1> opaqueStart{};
  std::array<std::vector<int8_t>, MaxCircleMaskRadius + 1> opaqueEnd{};

  CircleMaskCache() {
    for (int32_t r = 0; r <= MaxCircleMaskRadius; ++r) {
      int32_t size = r * 2 + 1;
      std::vector<uint8_t> mask(static_cast<size_t>(size) * size, 0u);
      std::vector<uint16_t> rowOffset(static_cast<size_t>(size + 1), 0u);
      std::vector<uint8_t> rowEdgeX;
      std::vector<uint8_t> rowEdgeCov;
      std::vector<int8_t> rowStart(static_cast<size_t>(size), static_cast<int8_t>(size));
      std::vector<int8_t> rowEnd(static_cast<size_t>(size), static_cast<int8_t>(-1));
      uint16_t cursor = 0;
      for (int32_t y = 0; y < size; ++y) {
        rowOffset[static_cast<size_t>(y)] = cursor;
        float fy = static_cast<float>(y - r) + 0.5f;
        for (int32_t x = 0; x < size; ++x) {
          float fx = static_cast<float>(x - r) + 0.5f;
          float dist = std::sqrt(fx * fx + fy * fy) - static_cast<float>(r);
          uint8_t coverage = coverage_from_dist(dist);
          mask[static_cast<size_t>(y) * size + x] = coverage;
          if (coverage != 0 && coverage != 255) {
            rowEdgeX.push_back(static_cast<uint8_t>(x));
            rowEdgeCov.push_back(coverage);
            ++cursor;
          }
          if (coverage == 255) {
            if (x < rowStart[static_cast<size_t>(y)]) {
              rowStart[static_cast<size_t>(y)] = static_cast<int8_t>(x);
            }
            if (x > rowEnd[static_cast<size_t>(y)]) {
              rowEnd[static_cast<size_t>(y)] = static_cast<int8_t>(x);
            }
          }
        }
        rowOffset[static_cast<size_t>(y + 1)] = cursor;
      }
      masks[static_cast<size_t>(r)] = std::move(mask);
      edgeX[static_cast<size_t>(r)] = std::move(rowEdgeX);
      edgeCov[static_cast<size_t>(r)] = std::move(rowEdgeCov);
      edgeOffset[static_cast<size_t>(r)] = std::move(rowOffset);
      opaqueStart[static_cast<size_t>(r)] = std::move(rowStart);
      opaqueEnd[static_cast<size_t>(r)] = std::move(rowEnd);
    }
  }
};

auto circle_mask_cache() -> CircleMaskCache& {
  static CircleMaskCache cache;
  return cache;
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

  uint8_t invA = static_cast<uint8_t>(255u - srcA);
  dst[0] = static_cast<uint8_t>(static_cast<uint16_t>(srcR) + mul_div_255(dstR, invA));
  dst[1] = static_cast<uint8_t>(static_cast<uint16_t>(srcG) + mul_div_255(dstG, invA));
  dst[2] = static_cast<uint8_t>(static_cast<uint16_t>(srcB) + mul_div_255(dstB, invA));
  dst[3] = static_cast<uint8_t>(static_cast<uint16_t>(srcA) + mul_div_255(dstA, invA));
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

struct RenderProfileScope {
  RendererProfile* profile = nullptr;
  std::chrono::steady_clock::time_point start{};

  explicit RenderProfileScope(RendererProfile* profileInput) : profile(profileInput) {
    if (profile) {
      profile->clear();
      start = std::chrono::steady_clock::now();
    }
  }

  ~RenderProfileScope() {
    if (!profile) return;
    auto end = std::chrono::steady_clock::now();
    profile->renderNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
  }
};

struct RenderTimeScope {
  RendererProfile* profile = nullptr;
  std::chrono::steady_clock::time_point start{};

  explicit RenderTimeScope(RendererProfile* profileInput) : profile(profileInput) {
    if (profile) {
      start = std::chrono::steady_clock::now();
    }
  }

  ~RenderTimeScope() {
    if (!profile) return;
    auto end = std::chrono::steady_clock::now();
    profile->renderNs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
  }
};


struct TilePool {
  std::mutex mutex;
  std::condition_variable cv;
  std::condition_variable cvDone;
  bool shutdown = false;
  bool workReady = false;
  uint32_t workCount = 0;
  uint32_t chunkSizeOverride = 0;
  std::atomic<uint32_t> nextWork{0};
  std::atomic<uint32_t> workDone{0};
  std::function<void(uint32_t)> job;
  struct Profile {
    std::vector<uint64_t> activeNs;
    std::vector<uint32_t> items;

    void reset(size_t count) {
      activeNs.resize(count);
      items.resize(count);
      for (auto& v : activeNs) v = 0;
      for (auto& v : items) v = 0;
    }
  };
  Profile* profile = nullptr;
  std::vector<std::thread> workers;

  TilePool() {
    uint32_t count = std::max(1u, std::thread::hardware_concurrency());
    if (count > 1u) {
      count -= 1u;
    }
    workers.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      workers.emplace_back([this, i]() { worker_loop(i); });
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

  void run(uint32_t jobs,
           std::function<void(uint32_t)> fn,
           Profile* profileInput = nullptr,
           uint32_t chunkOverride = 0) {
    if (jobs == 0) return;
    if (profileInput) {
      profileInput->reset(workers.size() + 1);
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      job = std::move(fn);
      workCount = jobs;
      chunkSizeOverride = chunkOverride;
      nextWork.store(0);
      workDone.store(0);
      workReady = true;
      profile = profileInput;
    }
    cv.notify_all();

    // Main thread helps.
    do_work(static_cast<uint32_t>(workers.size()));

    std::unique_lock<std::mutex> lock(mutex);
    cvDone.wait(lock, [&]() { return workDone.load() >= workCount; });
    workReady = false;
    profile = nullptr;
  }

  void worker_loop(uint32_t workerIndex) {
    for (;;) {
      {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&]() { return shutdown || workReady; });
        if (shutdown) return;
      }
      do_work(workerIndex);
    }
  }

  void do_work(uint32_t workerIndex) {
    uint32_t count = workCount;
    uint32_t chunkSize = chunkSizeOverride;
    if (chunkSize == 0) {
      chunkSize = count / (static_cast<uint32_t>(workers.size()) * 4u);
      if (chunkSize < 1u) chunkSize = 1u;
      if (chunkSize > 8u) chunkSize = 8u;
    }
    Profile* localProfile = profile;
    for (;;) {
      uint32_t idx = nextWork.fetch_add(chunkSize);
      if (idx >= count) break;
      uint32_t end = std::min(idx + ChunkSize, count);
      if (localProfile) {
        auto start = std::chrono::steady_clock::now();
        for (uint32_t i = idx; i < end; ++i) {
          job(i);
        }
        auto endTime = std::chrono::steady_clock::now();
        uint64_t ns = static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - start).count());
        localProfile->activeNs[workerIndex] += ns;
        localProfile->items[workerIndex] += end - idx;
      } else {
        for (uint32_t i = idx; i < end; ++i) {
          job(i);
        }
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

} // namespace

namespace {

void RenderOptimizedImpl(RenderTarget target, RenderBatch const& batch, OptimizedBatch const& prepared);

void RenderOptimizedImpl(RenderTarget target, RenderBatch const& batch, OptimizedBatch const& prepared) {
  if (!prepared.valid) return;
  if (target.width == 0 || target.height == 0) return;
  if (target.strideBytes == 0) return;
  if (target.data.size() < static_cast<size_t>(target.strideBytes) * target.height) return;
  if (prepared.targetWidth != target.width || prepared.targetHeight != target.height) return;
  if (!batch.palette.enabled || batch.palette.size == 0) return;

  RendererProfile* profile = batch.profile;
  RenderTimeScope renderScope(profile);
  auto to_ns = [](auto start, auto end) -> uint64_t {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
  };
  auto fetch_color = [&](auto const& indices, uint32_t idx, uint32_t fallback) -> uint32_t {
    if (idx >= indices.size()) return fallback;
    uint8_t paletteIndex = indices[idx];
    if (paletteIndex >= batch.palette.size) return fallback;
    return batch.palette.colorRGBA8[paletteIndex];
  };
  struct PalettePmCache {
    std::vector<uint32_t> table;
    std::vector<uint8_t> colorR;
    std::vector<uint8_t> colorG;
    std::vector<uint8_t> colorB;
    std::vector<uint8_t> colorA;
    uint64_t hash = 0;
    uint16_t size = 0;
  };
  static PalettePmCache palettePm;
  size_t paletteSize = static_cast<size_t>(batch.palette.size);
  uint64_t paletteHash = 1469598103934665603ull;
  for (size_t i = 0; i < paletteSize; ++i) {
    paletteHash ^= static_cast<uint64_t>(batch.palette.colorRGBA8[i]);
    paletteHash *= 1099511628211ull;
  }
  if (palettePm.size != batch.palette.size || palettePm.hash != paletteHash) {
    palettePm.size = batch.palette.size;
    palettePm.hash = paletteHash;
    palettePm.table.assign(paletteSize * 256u, 0u);
    palettePm.colorR.assign(paletteSize, 0u);
    palettePm.colorG.assign(paletteSize, 0u);
    palettePm.colorB.assign(paletteSize, 0u);
    palettePm.colorA.assign(paletteSize, 0u);
    for (size_t i = 0; i < paletteSize; ++i) {
      uint32_t color = batch.palette.colorRGBA8[i];
      uint8_t r = static_cast<uint8_t>(color & 0xFFu);
      uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFFu);
      uint8_t b = static_cast<uint8_t>((color >> 16) & 0xFFu);
      uint8_t a = static_cast<uint8_t>((color >> 24) & 0xFFu);
      palettePm.colorR[i] = r;
      palettePm.colorG[i] = g;
      palettePm.colorB[i] = b;
      palettePm.colorA[i] = a;
      size_t base = i * 256u;
      if (a == 0) {
        continue;
      }
      for (uint32_t cov = 0; cov < 256u; ++cov) {
        uint8_t srcA = apply_coverage(a, static_cast<uint8_t>(cov));
        if (srcA == 0) continue;
        if (srcA == 255u) {
          palettePm.table[base + cov] = color;
          continue;
        }
        uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(r) * srcA + 127u) / 255u);
        uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(g) * srcA + 127u) / 255u);
        uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(b) * srcA + 127u) / 255u);
        palettePm.table[base + cov] =
          static_cast<uint32_t>(pmR) |
          (static_cast<uint32_t>(pmG) << 8) |
          (static_cast<uint32_t>(pmB) << 16) |
          (static_cast<uint32_t>(srcA) << 24);
      }
    }
  }
  auto const& palettePmCache = palettePm.table;
  auto const& paletteR = palettePm.colorR;
  auto const& paletteG = palettePm.colorG;
  auto const& paletteB = palettePm.colorB;
  auto const& paletteA = palettePm.colorA;
  bool paletteFull = batch.palette.size >= 256;
  bool circleOnly =
    prepared.commandTypeCounts.circle > 0 &&
    prepared.commandTypeCounts.rect == 0 &&
    prepared.commandTypeCounts.text == 0;
  bool circleArraysPacked =
    circleOnly &&
    batch.circles.centerX.size() == batch.circles.centerY.size() &&
    batch.circles.centerX.size() == batch.circles.radius.size() &&
    batch.circles.centerX.size() == batch.circles.colorIndex.size() &&
    batch.circles.centerX.size() == prepared.commandTypeCounts.circle;
  auto const* circleCenterX = batch.circles.centerX.data();
  auto const* circleCenterY = batch.circles.centerY.data();
  auto const* circleRadius = batch.circles.radius.data();
  auto const* circleColorIndex = batch.circles.colorIndex.data();
  CircleMaskCache const* circleCache = nullptr;
  if (prepared.commandTypeCounts.circle > 0) {
    circleCache = &circle_mask_cache();
  }

  uint32_t tilesX = prepared.tilesX;
  uint32_t tilesY = prepared.tilesY;
  uint32_t tileSize = prepared.tileSize;
  uint32_t tileCount = prepared.tileCount;
  if (tileCount == 0) return;
  bool tilePow2 = prepared.tilePow2;
  uint32_t tileShift = prepared.tileShift;
  bool useTileStream = prepared.useTileStream;
  TileStream const* tileStream = prepared.tileStream;
  if (useTileStream && tileStream == nullptr) return;
  bool useTileBuffer = prepared.useTileBuffer;
  bool hasClear = prepared.hasClear;
  uint32_t clearColor = prepared.clearColor;
  bool clearPattern = prepared.clearPattern;
  uint16_t clearPatternWidth = prepared.clearPatternWidth;
  uint16_t clearPatternHeight = prepared.clearPatternHeight;
  uint32_t clearPatternOffset = prepared.clearPatternOffset;
  bool debugTiles = prepared.debugTiles;
  uint32_t debugColor = prepared.debugColor;
  uint8_t debugLineWidth = prepared.debugLineWidth;
  uint8_t debugFlags = prepared.debugFlags;

  auto const& tileOffsets = prepared.tileOffsets;
  auto const& tileRefs = prepared.tileRefs;
  auto const& renderTiles = prepared.renderTiles;
  auto const& textBaseAlpha = prepared.textBaseAlpha;
  auto const& textActive = prepared.textActive;
  auto const& textPmOffset = prepared.textPmOffset;
  auto const& textPmRStore = prepared.textPmRStore;
  auto const& textPmGStore = prepared.textPmGStore;
  auto const& textPmBStore = prepared.textPmBStore;
  auto const& textColorR = prepared.textColorR;
  auto const& textColorG = prepared.textColorG;
  auto const& textColorB = prepared.textColorB;
  auto const& textColorA = prepared.textColorA;
  auto const& textClipEnabled = prepared.textClipEnabled;
  auto const& textClipX0 = prepared.textClipX0;
  auto const& textClipY0 = prepared.textClipY0;
  auto const& textClipX1 = prepared.textClipX1;
  auto const& textClipY1 = prepared.textClipY1;
  auto const& rectBaseAlpha = prepared.rectBaseAlpha;
  auto const& rectActive = prepared.rectActive;
  auto const& rectEdgeOffset = prepared.rectEdgeOffset;
  auto const& rectEdgePmRStore = prepared.rectEdgePmRStore;
  auto const& rectEdgePmGStore = prepared.rectEdgePmGStore;
  auto const& rectEdgePmBStore = prepared.rectEdgePmBStore;
  auto const& rectHasGradient = prepared.rectHasGradient;
  auto const& rectColorR = prepared.rectColorR;
  auto const& rectColorG = prepared.rectColorG;
  auto const& rectColorB = prepared.rectColorB;
  auto const& rectColorA = prepared.rectColorA;
  auto const& rectGradColorR = prepared.rectGradColorR;
  auto const& rectGradColorG = prepared.rectGradColorG;
  auto const& rectGradColorB = prepared.rectGradColorB;
  auto const& rectGradColorA = prepared.rectGradColorA;
  auto const& rectClipEnabled = prepared.rectClipEnabled;
  auto const& rectClipX0 = prepared.rectClipX0;
  auto const& rectClipY0 = prepared.rectClipY0;
  auto const& rectClipX1 = prepared.rectClipX1;
  auto const& rectClipY1 = prepared.rectClipY1;
  auto const& rectGradDirX = prepared.rectGradDirX;
  auto const& rectGradDirY = prepared.rectGradDirY;
  auto const& rectGradMin = prepared.rectGradMin;
  auto const& rectGradInvRange = prepared.rectGradInvRange;
  constexpr uint32_t InvalidOffset = 0xFFFFFFFFu;
  bool clearOpaque = hasClear && !prepared.clearPattern && ((clearColor >> 24) & 0xFFu) == 255u;
  bool dstOpaque = clearOpaque && !prepared.useTileBuffer;

  auto clearStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  if (hasClear && !useTileBuffer) {
    uint8_t* base = target.data.data();
    if (clearPattern) {
      uint32_t patternStride = static_cast<uint32_t>(clearPatternWidth) * 4u;
      uint8_t const* pattern = batch.clearPattern.data.data() + clearPatternOffset;
      for (uint32_t y = 0; y < target.height; ++y) {
        uint32_t py = y % clearPatternHeight;
        uint8_t const* srcRow = pattern + static_cast<size_t>(py) * patternStride;
        uint8_t* row = base + static_cast<size_t>(y) * target.strideBytes;
        for (uint32_t x = 0; x < target.width; ++x) {
          uint32_t px = x % clearPatternWidth;
          size_t src = static_cast<size_t>(px) * 4u;
          size_t dst = static_cast<size_t>(x) * 4u;
          row[dst + 0] = srcRow[src + 0];
          row[dst + 1] = srcRow[src + 1];
          row[dst + 2] = srcRow[src + 2];
          row[dst + 3] = srcRow[src + 3];
        }
      }
    } else {
      uint32_t packed = clearColor;
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
  }
  if (profile) {
    profile->renderClearNs = to_ns(clearStart, std::chrono::steady_clock::now());
  }

  if (profile) {
    profile->tileCount = tileCount;
    profile->activeTileCount = static_cast<uint32_t>(renderTiles.size());
    profile->commandCount = useTileStream ? static_cast<uint32_t>(tileStream->commands.size())
                                          : static_cast<uint32_t>(batch.commands.size());
  }
  bool doProfile = profile != nullptr;

  if (renderTiles.empty() && !debugTiles) return;

  std::atomic<uint64_t> renderedTiles{0};
  std::atomic<uint64_t> renderedCommands{0};
  std::atomic<uint64_t> renderedPixels{0};
  std::atomic<uint64_t> renderedRects{0};
  std::atomic<uint64_t> renderedTexts{0};
  std::atomic<uint64_t> renderedRectPixels{0};
  std::atomic<uint64_t> renderedTextPixels{0};
  std::atomic<uint64_t> renderedTileBufferPixels{0};

  auto render_tile = [&](uint32_t tileIndex) {
    uint32_t tx = tileIndex % tilesX;
    uint32_t ty = tileIndex / tilesX;
    uint32_t tx0 = tx * tileSize;
    uint32_t ty0 = ty * tileSize;
    uint32_t tx1 = std::min(tx0 + tileSize, target.width);
    uint32_t ty1 = std::min(ty0 + tileSize, target.height);

    bool frontToBack = batch.assumeFrontToBack && useTileBuffer;
    uint32_t tileArea = (tx1 - tx0) * (ty1 - ty0);
    uint32_t opaqueCount = 0;
    uint64_t tileCommands = 0;
    uint64_t tilePixels = doProfile
      ? static_cast<uint64_t>(tx1 - tx0) * static_cast<uint64_t>(ty1 - ty0)
      : 0;
    uint64_t tileRects = 0;
    uint64_t tileTexts = 0;
    uint64_t tileRectPixels = 0;
    uint64_t tileTextPixels = 0;
    uint64_t tileTileBufferPixels = 0;
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
        uint8_t invA = static_cast<uint8_t>(255u - dstA);
        dst[0] = static_cast<uint8_t>(static_cast<uint16_t>(dst[0]) + mul_div_255(pmR, invA));
        dst[1] = static_cast<uint8_t>(static_cast<uint16_t>(dst[1]) + mul_div_255(pmG, invA));
        dst[2] = static_cast<uint8_t>(static_cast<uint16_t>(dst[2]) + mul_div_255(pmB, invA));
        uint8_t newA = static_cast<uint8_t>(static_cast<uint16_t>(dstA) + mul_div_255(srcA, invA));
        dst[3] = newA;
        if (dstA < OpaqueAlphaCutoff && newA >= OpaqueAlphaCutoff) {
          ++opaqueCount;
        }
      } else if (dstOpaque) {
        uint8_t invA = static_cast<uint8_t>(255u - srcA);
        dst[0] = static_cast<uint8_t>(static_cast<uint16_t>(pmR) + mul_div_255(dst[0], invA));
        dst[1] = static_cast<uint8_t>(static_cast<uint16_t>(pmG) + mul_div_255(dst[1], invA));
        dst[2] = static_cast<uint8_t>(static_cast<uint16_t>(pmB) + mul_div_255(dst[2], invA));
        dst[3] = 255u;
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
        if (prepared.tileRefsAreCircleIndices) {
          type = CommandType::Circle;
          idx = cmdIndex;
        } else {
          if (cmdIndex >= batch.commands.size()) continue;
          auto const& cmd = batch.commands[cmdIndex];
          type = cmd.type;
          idx = cmd.index;
        }
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
        if (profile) {
          ++tileRects;
          tileRectPixels += static_cast<uint64_t>(rx1 - rx0) * static_cast<uint64_t>(ry1 - ry0);
        }

        uint16_t radiusQ = idx < batch.rects.radiusQ8_8.size() ? batch.rects.radiusQ8_8[idx] : 0;
        float radius = static_cast<float>(radiusQ) / 256.0f;
        int16_t rotationQ = idx < batch.rects.rotationQ8_8.size() ? batch.rects.rotationQ8_8[idx] : 0;
        bool axisAligned = (rotationQ == 0);
        float rotation = axisAligned ? 0.0f : static_cast<float>(rotationQ) / 256.0f;
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
        Vec2f rectCenter{cx, cy};
        float cosA = 1.0f;
        float sinA = 0.0f;
        if (!axisAligned) {
          cosA = std::cos(rotation);
          sinA = std::sin(rotation);
        }
        Vec2f halfExtents{(static_cast<float>(x1) - static_cast<float>(x0)) * 0.5f,
                          (static_cast<float>(y1) - static_cast<float>(y0)) * 0.5f};

        IntRect clipRect{};
        if (clipEnabled) {
          clipRect.x0 = std::max<int32_t>(clip.x0, drawX0);
          clipRect.y0 = std::max<int32_t>(clip.y0, drawY0);
          clipRect.x1 = std::min<int32_t>(clip.x1, drawX1);
          clipRect.y1 = std::min<int32_t>(clip.y1, drawY1);
          if (clipRect.x1 <= clipRect.x0 || clipRect.y1 <= clipRect.y0) continue;
        } else {
          clipRect.x0 = drawX0;
          clipRect.y0 = drawY0;
          clipRect.x1 = drawX1;
          clipRect.y1 = drawY1;
        }

        IntRect region{};
        region.x0 = std::max<int32_t>(clipRect.x0, static_cast<int32_t>(tx0));
        region.y0 = std::max<int32_t>(clipRect.y0, static_cast<int32_t>(ty0));
        region.x1 = std::min<int32_t>(clipRect.x1, static_cast<int32_t>(tx1));
        region.y1 = std::min<int32_t>(clipRect.y1, static_cast<int32_t>(ty1));
        if (region.x1 <= region.x0 || region.y1 <= region.y0) continue;

        bool useEdgeTable = (idx < rectEdgeOffset.size() && rectEdgeOffset[idx] != 0xFFFFFFFFu);
        uint32_t edgeOffset = useEdgeTable ? rectEdgeOffset[idx] : 0u;

        bool gradientVertical = false;
        float gradSign = 1.0f;
        if (hasGradient) {
          constexpr float GradEpsilon = 1e-4f;
          if (std::abs(gradDir.x) <= GradEpsilon &&
              std::abs(std::abs(gradDir.y) - 1.0f) <= GradEpsilon) {
            gradientVertical = true;
            gradSign = gradDir.y >= 0.0f ? 1.0f : -1.0f;
          }
        }

        bool smoothBlend = (flags & RectFlagSmoothBlend) != 0u;
        uint8_t baseAlpha = idx < rectBaseAlpha.size() ? rectBaseAlpha[idx] : cA;
        auto fill_opaque_region = [&](int32_t x0f, int32_t y0f, int32_t x1f, int32_t y1f) {
          if (x1f <= x0f || y1f <= y0f) return;
          if (frontToBack) {
            for (int32_t y = y0f; y < y1f; ++y) {
              uint8_t* row = row_ptr(y) + static_cast<size_t>(4u * x0f);
              for (int32_t x = x0f; x < x1f; ++x, row += 4) {
                write_px(row, cR, cG, cB);
              }
            }
          } else {
            uint32_t packed = color;
            for (int32_t y = y0f; y < y1f; ++y) {
              uint8_t* row = row_ptr(y) + static_cast<size_t>(4u * x0f);
              if ((reinterpret_cast<uintptr_t>(row) % alignof(uint32_t)) == 0) {
                auto* row32 = reinterpret_cast<uint32_t*>(row);
                std::fill(row32, row32 + (x1f - x0f), packed);
              } else {
                for (int32_t x = x0f; x < x1f; ++x, row += 4) {
                  row[0] = cR;
                  row[1] = cG;
                  row[2] = cB;
                  row[3] = 255u;
                }
              }
            }
          }
        };
        auto render_sdf_region = [&](int32_t x0f, int32_t y0f, int32_t x1f, int32_t y1f) {
          if (x1f <= x0f || y1f <= y0f) return;
          if (hasGradient && gradientVertical) {
            for (int32_t y = y0f; y < y1f; ++y) {
              float dotBase = gradSign * (static_cast<float>(y) + 0.5f);
              float t = clamp01((dotBase - gradMin) * gradInvRange);
              uint8_t rowR = static_cast<uint8_t>(static_cast<float>(cR) + t * (static_cast<float>(gR) - cR));
              uint8_t rowG = static_cast<uint8_t>(static_cast<float>(cG) + t * (static_cast<float>(gG) - cG));
              uint8_t rowB = static_cast<uint8_t>(static_cast<float>(cB) + t * (static_cast<float>(gB) - cB));
              uint8_t rowA = static_cast<uint8_t>(static_cast<float>(cA) + t * (static_cast<float>(gA) - cA));
              uint8_t alpha = apply_opacity(rowA, opacity);
              if (alpha == 0) continue;
              uint8_t* row = row_ptr(y) + static_cast<size_t>(4u * x0f);
              for (int32_t x = x0f; x < x1f; ++x, row += 4) {
                Vec2f p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
                Vec2f local{p.x - rectCenter.x, p.y - rectCenter.y};
                if (!axisAligned) {
                  local = rotate_point(local, cosA, -sinA);
                }
                float dist = sdf_round_rect(local, halfExtents.x, halfExtents.y, radius);
                if (dist > 1.0f) continue;
                uint8_t cov = coverage_from_dist(dist);
                if (cov == 0) continue;
                uint8_t alphaCov = cov != 255u ? apply_coverage(alpha, cov) : alpha;
                if (alphaCov == 0) continue;
                uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(rowR) * alphaCov + 127u) / 255u);
                uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(rowG) * alphaCov + 127u) / 255u);
                uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(rowB) * alphaCov + 127u) / 255u);
                blend_px(row, pmR, pmG, pmB, alphaCov);
              }
            }
          } else if (hasGradient) {
            for (int32_t y = y0f; y < y1f; ++y) {
              float dotBase = gradDir.x * (static_cast<float>(x0f) + 0.5f) +
                              gradDir.y * (static_cast<float>(y) + 0.5f);
              uint8_t* row = row_ptr(y) + static_cast<size_t>(4u * x0f);
              for (int32_t x = x0f; x < x1f; ++x, row += 4) {
                float t = clamp01((dotBase - gradMin) * gradInvRange);
                dotBase += gradDir.x;
                uint8_t r = static_cast<uint8_t>(static_cast<float>(cR) + t * (static_cast<float>(gR) - cR));
                uint8_t g = static_cast<uint8_t>(static_cast<float>(cG) + t * (static_cast<float>(gG) - cG));
                uint8_t b = static_cast<uint8_t>(static_cast<float>(cB) + t * (static_cast<float>(gB) - cB));
                uint8_t a = static_cast<uint8_t>(static_cast<float>(cA) + t * (static_cast<float>(gA) - cA));
                uint8_t alpha = apply_opacity(a, opacity);
                if (alpha == 0) continue;
                Vec2f p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
                Vec2f local{p.x - rectCenter.x, p.y - rectCenter.y};
                if (!axisAligned) {
                  local = rotate_point(local, cosA, -sinA);
                }
                float dist = sdf_round_rect(local, halfExtents.x, halfExtents.y, radius);
                if (dist > 1.0f) continue;
                uint8_t cov = coverage_from_dist(dist);
                if (cov == 0) continue;
                uint8_t alphaCov = cov != 255u ? apply_coverage(alpha, cov) : alpha;
                if (alphaCov == 0) continue;
                uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(r) * alphaCov + 127u) / 255u);
                uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(g) * alphaCov + 127u) / 255u);
                uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(b) * alphaCov + 127u) / 255u);
                blend_px(row, pmR, pmG, pmB, alphaCov);
              }
            }
          } else {
            for (int32_t y = y0f; y < y1f; ++y) {
              uint8_t* row = row_ptr(y) + static_cast<size_t>(4u * x0f);
              for (int32_t x = x0f; x < x1f; ++x, row += 4) {
                Vec2f p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
                Vec2f local{p.x - rectCenter.x, p.y - rectCenter.y};
                if (!axisAligned) {
                  local = rotate_point(local, cosA, -sinA);
                }
                float dist = sdf_round_rect(local, halfExtents.x, halfExtents.y, radius);
                if (dist > 1.0f) continue;
                uint8_t cov = coverage_from_dist(dist);
                if (cov == 0) continue;

                uint8_t finalA = baseAlpha;
                if (cov != 255u) {
                  finalA = apply_coverage(baseAlpha, cov);
                }
                if (finalA == 0) continue;

                if (smoothBlend) {
                  uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
                  uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
                  uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
                  blend_px(row, pmR, pmG, pmB, finalA);
                } else if (useEdgeTable && cov != 255u && baseAlpha == 255u) {
                  uint8_t pmR = rectEdgePmRStore[edgeOffset + cov];
                  uint8_t pmG = rectEdgePmGStore[edgeOffset + cov];
                  uint8_t pmB = rectEdgePmBStore[edgeOffset + cov];
                  blend_px(row, pmR, pmG, pmB, cov);
                } else {
                  uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(cR) * finalA + 127u) / 255u);
                  uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(cG) * finalA + 127u) / 255u);
                  uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(cB) * finalA + 127u) / 255u);
                  blend_px(row, pmR, pmG, pmB, finalA);
                }
              }
            }
          }
        };
        if (!batch.disableOpaqueRectFastPath && !hasGradient && baseAlpha == 255u && !smoothBlend &&
            rotation == 0.0f && radius <= 0.0f) {
          fill_opaque_region(region.x0, region.y0, region.x1, region.y1);
          continue;
        }
        if (!batch.disableOpaqueRectFastPath && !hasGradient && baseAlpha == 255u && !smoothBlend &&
            rotation == 0.0f && radius > 0.0f) {
          float inset = radius + 0.5f;
          int32_t coreX0 = std::max(region.x0, static_cast<int32_t>(std::ceil(static_cast<float>(x0) + inset)));
          int32_t coreY0 = std::max(region.y0, static_cast<int32_t>(std::ceil(static_cast<float>(y0) + inset)));
          int32_t coreX1 = std::min(region.x1, static_cast<int32_t>(std::floor(static_cast<float>(x1) - inset)));
          int32_t coreY1 = std::min(region.y1, static_cast<int32_t>(std::floor(static_cast<float>(y1) - inset)));
          if (coreX1 > coreX0 && coreY1 > coreY0) {
            fill_opaque_region(coreX0, coreY0, coreX1, coreY1);
            render_sdf_region(region.x0, region.y0, region.x1, coreY0);
            render_sdf_region(region.x0, coreY1, region.x1, region.y1);
            render_sdf_region(region.x0, coreY0, coreX0, coreY1);
            render_sdf_region(coreX1, coreY0, region.x1, coreY1);
            continue;
          }
        }
        if (!batch.disableOpaqueRectFastPath && hasGradient && gradientVertical && !smoothBlend &&
            rotation == 0.0f && radius > 0.0f && opacity == 255u && cA == 255u && gA == 255u) {
          float inset = radius + 0.5f;
          int32_t coreX0 = std::max(region.x0, static_cast<int32_t>(std::ceil(static_cast<float>(x0) + inset)));
          int32_t coreY0 = std::max(region.y0, static_cast<int32_t>(std::ceil(static_cast<float>(y0) + inset)));
          int32_t coreX1 = std::min(region.x1, static_cast<int32_t>(std::floor(static_cast<float>(x1) - inset)));
          int32_t coreY1 = std::min(region.y1, static_cast<int32_t>(std::floor(static_cast<float>(y1) - inset)));
          if (coreX1 > coreX0 && coreY1 > coreY0) {
            for (int32_t y = coreY0; y < coreY1; ++y) {
              float dotBase = gradSign * (static_cast<float>(y) + 0.5f);
              float t = clamp01((dotBase - gradMin) * gradInvRange);
              uint8_t rowR = static_cast<uint8_t>(static_cast<float>(cR) + t * (static_cast<float>(gR) - cR));
              uint8_t rowG = static_cast<uint8_t>(static_cast<float>(cG) + t * (static_cast<float>(gG) - cG));
              uint8_t rowB = static_cast<uint8_t>(static_cast<float>(cB) + t * (static_cast<float>(gB) - cB));
              if (frontToBack) {
                uint8_t* row = row_ptr(y) + static_cast<size_t>(4u * coreX0);
                for (int32_t x = coreX0; x < coreX1; ++x, row += 4) {
                  write_px(row, rowR, rowG, rowB);
                }
              } else {
                uint32_t packed = static_cast<uint32_t>(rowR) |
                                  (static_cast<uint32_t>(rowG) << 8) |
                                  (static_cast<uint32_t>(rowB) << 16) |
                                  (255u << 24);
                uint8_t* row = row_ptr(y) + static_cast<size_t>(4u * coreX0);
                if ((reinterpret_cast<uintptr_t>(row) % alignof(uint32_t)) == 0) {
                  auto* row32 = reinterpret_cast<uint32_t*>(row);
                  std::fill(row32, row32 + (coreX1 - coreX0), packed);
                } else {
                  for (int32_t x = coreX0; x < coreX1; ++x, row += 4) {
                    row[0] = rowR;
                    row[1] = rowG;
                    row[2] = rowB;
                    row[3] = 255u;
                  }
                }
              }
            }
            render_sdf_region(region.x0, region.y0, region.x1, coreY0);
            render_sdf_region(region.x0, coreY1, region.x1, region.y1);
            render_sdf_region(region.x0, coreY0, coreX0, coreY1);
            render_sdf_region(coreX1, coreY0, region.x1, coreY1);
            continue;
          }
        }
        render_sdf_region(region.x0, region.y0, region.x1, region.y1);
      } else if (type == CommandType::Circle) {
        if (!circleArraysPacked) {
          if (idx >= batch.circles.centerX.size() ||
              idx >= batch.circles.centerY.size() ||
              idx >= batch.circles.radius.size() ||
              idx >= batch.circles.colorIndex.size()) {
            continue;
          }
        }
        int32_t cx = circleCenterX[idx];
        int32_t cy = circleCenterY[idx];
        int32_t r = static_cast<int32_t>(circleRadius[idx]);
        int32_t x0 = cx - r;
        int32_t y0 = cy - r;
        int32_t x1 = cx + r + 1;
        int32_t y1 = cy + r + 1;

        uint8_t paletteIndex = circleColorIndex[idx];
        if (!paletteFull && paletteIndex >= batch.palette.size) continue;
        size_t pmOffset = static_cast<size_t>(paletteIndex) * 256u;
        uint32_t const* pmTable = palettePmCache.data() + pmOffset;
        uint32_t color = batch.palette.colorRGBA8[paletteIndex];
        uint8_t cR = static_cast<uint8_t>(color & 0xFFu);
        uint8_t cG = static_cast<uint8_t>((color >> 8) & 0xFFu);
        uint8_t cB = static_cast<uint8_t>((color >> 16) & 0xFFu);
        uint8_t cA = static_cast<uint8_t>((color >> 24) & 0xFFu);
        if (cA == 0) continue;

        if (r <= MaxCircleMaskRadius) {
          auto const& cache = *circleCache;
          auto const& mask = cache.masks[static_cast<size_t>(r)];
          auto const& edgeOffset = cache.edgeOffset[static_cast<size_t>(r)];
          auto const& edgeX = cache.edgeX[static_cast<size_t>(r)];
          auto const& edgeCov = cache.edgeCov[static_cast<size_t>(r)];
          auto const& rowOpaqueStart = cache.opaqueStart[static_cast<size_t>(r)];
          auto const& rowOpaqueEnd = cache.opaqueEnd[static_cast<size_t>(r)];
          int32_t size = r * 2 + 1;
          int32_t maskX0 = cx - r;
          int32_t maskY0 = cy - r;
          bool fullInside = maskX0 >= static_cast<int32_t>(tx0) &&
                            maskY0 >= static_cast<int32_t>(ty0) &&
                            (maskX0 + size) <= static_cast<int32_t>(tx1) &&
                            (maskY0 + size) <= static_cast<int32_t>(ty1);
          if (fullInside && cA == 255) {
            if (profile) {
              ++tileRects;
              tileRectPixels += static_cast<uint64_t>(size) * static_cast<uint64_t>(size);
            }
            uint8_t* rowBase = row_ptr(maskY0) + static_cast<size_t>(4u * maskX0);
            for (int32_t localY = 0; localY < size; ++localY, rowBase += surfaceStride) {
              int32_t opaqueStart = static_cast<int32_t>(rowOpaqueStart[static_cast<size_t>(localY)]);
              int32_t opaqueEnd = static_cast<int32_t>(rowOpaqueEnd[static_cast<size_t>(localY)]);
              if (opaqueEnd >= opaqueStart) {
                uint8_t* opaqueRow = rowBase + static_cast<size_t>(4u * opaqueStart);
                int32_t count = opaqueEnd - opaqueStart + 1;
                if ((reinterpret_cast<uintptr_t>(opaqueRow) % alignof(uint32_t)) == 0) {
                  auto* row32 = reinterpret_cast<uint32_t*>(opaqueRow);
                  std::fill(row32, row32 + count, color);
                } else {
                  for (int32_t i = 0; i < count; ++i, opaqueRow += 4) {
                    opaqueRow[0] = cR;
                    opaqueRow[1] = cG;
                    opaqueRow[2] = cB;
                    opaqueRow[3] = 255u;
                  }
                }
              }
              uint16_t start = edgeOffset[static_cast<size_t>(localY)];
              uint16_t end = edgeOffset[static_cast<size_t>(localY + 1)];
              for (uint16_t i = start; i < end; ++i) {
                uint8_t x = edgeX[i];
                uint8_t cov = edgeCov[i];
                uint32_t pm = pmTable[cov];
                blend_px(rowBase + static_cast<size_t>(4u * x),
                         static_cast<uint8_t>(pm & 0xFFu),
                         static_cast<uint8_t>((pm >> 8) & 0xFFu),
                         static_cast<uint8_t>((pm >> 16) & 0xFFu),
                         cov);
              }
            }
            continue;
          }

          int32_t drawX0 = hasLocalBounds ? localX0 : x0;
          int32_t drawY0 = hasLocalBounds ? localY0 : y0;
          int32_t drawX1 = hasLocalBounds ? localX1 : x1;
          int32_t drawY1 = hasLocalBounds ? localY1 : y1;

          int32_t rx0 = std::max<int32_t>(drawX0, static_cast<int32_t>(tx0));
          int32_t ry0 = std::max<int32_t>(drawY0, static_cast<int32_t>(ty0));
          int32_t rx1 = std::min<int32_t>(drawX1, static_cast<int32_t>(tx1));
          int32_t ry1 = std::min<int32_t>(drawY1, static_cast<int32_t>(ty1));
          if (rx1 <= rx0 || ry1 <= ry0) continue;
          if (profile) {
            ++tileRects;
            tileRectPixels += static_cast<uint64_t>(rx1 - rx0) * static_cast<uint64_t>(ry1 - ry0);
          }
          int32_t offsetX = rx0 - maskX0;
          int32_t rowWidth = rx1 - rx0;
          auto const* maskBase = mask.data();
          int32_t localY = ry0 - maskY0;
          auto const* maskRowPtr = maskBase + localY * size + offsetX;
          auto const* rowOpaqueStartPtr = rowOpaqueStart.data() + localY;
          auto const* rowOpaqueEndPtr = rowOpaqueEnd.data() + localY;
          uint8_t* rowBase = row_ptr(ry0) + static_cast<size_t>(4u * rx0);
          for (int32_t y = ry0; y < ry1; ++y,
               ++localY,
               maskRowPtr += size,
               ++rowOpaqueStartPtr,
               ++rowOpaqueEndPtr,
               rowBase += surfaceStride) {
            int32_t opaqueStart = static_cast<int32_t>(*rowOpaqueStartPtr) - offsetX;
            int32_t opaqueEnd = static_cast<int32_t>(*rowOpaqueEndPtr) - offsetX;
            if (opaqueEnd < 0 || opaqueStart >= rowWidth || opaqueStart > opaqueEnd) {
              opaqueStart = rowWidth;
              opaqueEnd = -1;
            } else {
              opaqueStart = std::max(opaqueStart, 0);
              opaqueEnd = std::min(opaqueEnd, rowWidth - 1);
            }

            auto const* maskRow = maskRowPtr;
            uint8_t* row = rowBase;
            if (cA == 255) {
              for (int32_t x = 0; x < opaqueStart; ++x, row += 4) {
                uint8_t coverage = maskRow[x];
                if (coverage == 0) continue;
                uint32_t pm = pmTable[coverage];
                blend_px(row,
                         static_cast<uint8_t>(pm & 0xFFu),
                         static_cast<uint8_t>((pm >> 8) & 0xFFu),
                         static_cast<uint8_t>((pm >> 16) & 0xFFu),
                         coverage);
              }
              if (opaqueEnd >= opaqueStart) {
                uint32_t packed = color;
                uint8_t* opaqueRow = rowBase + static_cast<size_t>(4u * opaqueStart);
                int32_t count = opaqueEnd - opaqueStart + 1;
                if ((reinterpret_cast<uintptr_t>(opaqueRow) % alignof(uint32_t)) == 0) {
                  auto* row32 = reinterpret_cast<uint32_t*>(opaqueRow);
                  std::fill(row32, row32 + count, packed);
                } else {
                  for (int32_t i = 0; i < count; ++i, opaqueRow += 4) {
                    opaqueRow[0] = cR;
                    opaqueRow[1] = cG;
                    opaqueRow[2] = cB;
                    opaqueRow[3] = 255u;
                  }
                }
              }
              int32_t tailStart = std::max(opaqueEnd + 1, 0);
              row = rowBase + static_cast<size_t>(4u * tailStart);
              for (int32_t x = tailStart; x < rowWidth; ++x, row += 4) {
                uint8_t coverage = maskRow[x];
                if (coverage == 0) continue;
                uint32_t pm = pmTable[coverage];
                blend_px(row,
                         static_cast<uint8_t>(pm & 0xFFu),
                         static_cast<uint8_t>((pm >> 8) & 0xFFu),
                         static_cast<uint8_t>((pm >> 16) & 0xFFu),
                         coverage);
              }
            } else {
              for (int32_t x = 0; x < opaqueStart; ++x, row += 4) {
                uint8_t coverage = maskRow[x];
                if (coverage == 0) continue;
                uint32_t pm = pmTable[coverage];
                uint8_t srcA = static_cast<uint8_t>((pm >> 24) & 0xFFu);
                if (srcA == 0) continue;
                blend_px(row,
                         static_cast<uint8_t>(pm & 0xFFu),
                         static_cast<uint8_t>((pm >> 8) & 0xFFu),
                         static_cast<uint8_t>((pm >> 16) & 0xFFu),
                         srcA);
              }
              if (opaqueEnd >= opaqueStart) {
                uint32_t pmOpaque = pmTable[255u];
                uint8_t srcA = static_cast<uint8_t>((pmOpaque >> 24) & 0xFFu);
                row = rowBase + static_cast<size_t>(4u * opaqueStart);
                for (int32_t x = opaqueStart; x <= opaqueEnd; ++x, row += 4) {
                  blend_px(row,
                           static_cast<uint8_t>(pmOpaque & 0xFFu),
                           static_cast<uint8_t>((pmOpaque >> 8) & 0xFFu),
                           static_cast<uint8_t>((pmOpaque >> 16) & 0xFFu),
                           srcA);
                }
              }
              int32_t tailStart = std::max(opaqueEnd + 1, 0);
              row = rowBase + static_cast<size_t>(4u * tailStart);
              for (int32_t x = tailStart; x < rowWidth; ++x, row += 4) {
                uint8_t coverage = maskRow[x];
                if (coverage == 0) continue;
                uint32_t pm = pmTable[coverage];
                uint8_t srcA = static_cast<uint8_t>((pm >> 24) & 0xFFu);
                if (srcA == 0) continue;
                blend_px(row,
                         static_cast<uint8_t>(pm & 0xFFu),
                         static_cast<uint8_t>((pm >> 8) & 0xFFu),
                         static_cast<uint8_t>((pm >> 16) & 0xFFu),
                         srcA);
              }
            }
          }
        } else {
          int32_t drawX0 = hasLocalBounds ? localX0 : x0;
          int32_t drawY0 = hasLocalBounds ? localY0 : y0;
          int32_t drawX1 = hasLocalBounds ? localX1 : x1;
          int32_t drawY1 = hasLocalBounds ? localY1 : y1;

          int32_t rx0 = std::max<int32_t>(drawX0, static_cast<int32_t>(tx0));
          int32_t ry0 = std::max<int32_t>(drawY0, static_cast<int32_t>(ty0));
          int32_t rx1 = std::min<int32_t>(drawX1, static_cast<int32_t>(tx1));
          int32_t ry1 = std::min<int32_t>(drawY1, static_cast<int32_t>(ty1));
          if (rx1 <= rx0 || ry1 <= ry0) continue;
          if (profile) {
            ++tileRects;
            tileRectPixels += static_cast<uint64_t>(rx1 - rx0) * static_cast<uint64_t>(ry1 - ry0);
          }
          float fcX = static_cast<float>(cx);
          float fcY = static_cast<float>(cy);
          float fr = static_cast<float>(r);
          float innerR = std::max(0.0f, fr - 0.5f);
          float outerR = fr + 0.5f;
          float innerR2 = innerR * innerR;
          float outerR2 = outerR * outerR;
          for (int32_t y = ry0; y < ry1; ++y) {
            float dy = (static_cast<float>(y) + 0.5f) - fcY;
            float dy2 = dy * dy;
            uint8_t* row = row_ptr(y) + static_cast<size_t>(4u * rx0);
            for (int32_t x = rx0; x < rx1; ++x, row += 4) {
              float dx = (static_cast<float>(x) + 0.5f) - fcX;
              float dist2 = dx * dx + dy2;
              if (dist2 >= outerR2) continue;
              uint8_t coverage = 0;
              if (dist2 <= innerR2) {
                coverage = 255;
              } else {
                float dist = std::sqrt(dist2) - fr;
                coverage = coverage_from_dist(dist);
                if (coverage == 0) continue;
              }
              uint32_t pm = pmTable[coverage];
              uint8_t srcA = static_cast<uint8_t>((pm >> 24) & 0xFFu);
              if (srcA == 0) continue;
              if (srcA == 255) {
                write_px(row,
                         static_cast<uint8_t>(pm & 0xFFu),
                         static_cast<uint8_t>((pm >> 8) & 0xFFu),
                         static_cast<uint8_t>((pm >> 16) & 0xFFu));
              } else {
                blend_px(row,
                         static_cast<uint8_t>(pm & 0xFFu),
                         static_cast<uint8_t>((pm >> 8) & 0xFFu),
                         static_cast<uint8_t>((pm >> 16) & 0xFFu),
                         srcA);
              }
            }
          }
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
        if (profile) {
          ++tileTexts;
        }
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
          if (profile) {
            tileTextPixels += static_cast<uint64_t>(cx1 - cx0) * static_cast<uint64_t>(cy1 - cy0);
          }

          bool colorGlyph = bmp.format == GlyphBitmapFormat::ColorBGRA;
          if (opaqueText && !colorGlyph) {
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

          if (colorGlyph) {
            const uint8_t* srcBase = bmp.pixels.data();
            int32_t srcStride = bmp.stride;
            if (!srcBase || srcStride <= 0) continue;
            for (int32_t y = cy0; y < cy1; ++y) {
              int32_t srcRow = y - gy0;
              const uint8_t* src = srcBase + static_cast<size_t>(srcRow) * srcStride +
                                   static_cast<size_t>(cx0 - gx0) * 4u;
              uint8_t* row = row_ptr(y) + static_cast<size_t>(4 * cx0);
              for (int32_t x = cx0; x < cx1; ++x, src += 4, row += 4) {
                uint8_t b = src[0];
                uint8_t g = src[1];
                uint8_t r = src[2];
                uint8_t a = src[3];
                if (a == 0) {
                  a = std::max(r, std::max(g, b));
                }
                a = apply_opacity(a, opacity);
                if (a == 0) continue;
                blend_rgba(row, r, g, b, a);
              }
            }
            continue;
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
      if (clearPattern) {
        uint32_t patternStride = static_cast<uint32_t>(clearPatternWidth) * 4u;
        uint8_t const* pattern = batch.clearPattern.data.data() + clearPatternOffset;
        for (uint32_t y = ty0; y < ty1; ++y) {
          uint32_t py = y % clearPatternHeight;
          uint8_t const* srcRow = pattern + static_cast<size_t>(py) * patternStride;
          uint8_t* dstRow = target.data.data() + static_cast<size_t>(y) * target.strideBytes +
                            static_cast<size_t>(4 * tx0);
          for (uint32_t x = tx0; x < tx1; ++x, dstRow += 4) {
            uint32_t px = x % clearPatternWidth;
            size_t src = static_cast<size_t>(px) * 4u;
            uint8_t clearR = srcRow[src + 0];
            uint8_t clearG = srcRow[src + 1];
            uint8_t clearB = srcRow[src + 2];
            uint8_t clearA = srcRow[src + 3];
            uint8_t clearPmR = static_cast<uint8_t>((static_cast<uint16_t>(clearR) * clearA + 127u) / 255u);
            uint8_t clearPmG = static_cast<uint8_t>((static_cast<uint16_t>(clearG) * clearA + 127u) / 255u);
            uint8_t clearPmB = static_cast<uint8_t>((static_cast<uint16_t>(clearB) * clearA + 127u) / 255u);
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
      } else {
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
      if (profile) {
        tileTileBufferPixels += static_cast<uint64_t>(tileArea);
      }
    }

    if (profile) {
      renderedTiles.fetch_add(1, std::memory_order_relaxed);
      renderedCommands.fetch_add(tileCommands, std::memory_order_relaxed);
      renderedPixels.fetch_add(tilePixels, std::memory_order_relaxed);
      renderedRects.fetch_add(tileRects, std::memory_order_relaxed);
      renderedTexts.fetch_add(tileTexts, std::memory_order_relaxed);
      renderedRectPixels.fetch_add(tileRectPixels, std::memory_order_relaxed);
      renderedTextPixels.fetch_add(tileTextPixels, std::memory_order_relaxed);
      renderedTileBufferPixels.fetch_add(tileTileBufferPixels, std::memory_order_relaxed);
    }
  };

  auto tilesStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  if (!renderTiles.empty()) {
    if (renderTiles.size() <= 2) {
      if (profile) {
        auto start = std::chrono::steady_clock::now();
        for (uint32_t tileIndex : renderTiles) {
          render_tile(tileIndex);
        }
        auto end = std::chrono::steady_clock::now();
        uint64_t ns = static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        profile->workerNs.assign(1, ns);
        profile->workerTiles.assign(1, static_cast<uint32_t>(renderTiles.size()));
        profile->tileWorkNs = ns;
      } else {
        for (uint32_t tileIndex : renderTiles) {
          render_tile(tileIndex);
        }
      }
    } else {
      auto& pool = tile_pool();
      TilePool::Profile poolProfile;
      TilePool::Profile* profilePtr = profile ? &poolProfile : nullptr;
      uint32_t chunkOverride = 0;
      if (!useTileStream && prepared.tileRefsAreCircleIndices) {
        chunkOverride = 1u;
      }
      pool.run(static_cast<uint32_t>(renderTiles.size()),
               [&](uint32_t jobIndex) {
        uint32_t tileIndex = renderTiles[jobIndex];
        render_tile(tileIndex);
               },
               profilePtr,
               chunkOverride);
      if (profilePtr) {
        size_t workerCount = poolProfile.activeNs.size();
        profile->workerNs.resize(workerCount);
        profile->workerTiles.resize(workerCount);
        uint64_t totalNs = 0;
        for (size_t i = 0; i < workerCount; ++i) {
          uint64_t ns = poolProfile.activeNs[i];
          uint32_t items = poolProfile.items[i];
          profile->workerNs[i] = ns;
          profile->workerTiles[i] = items;
          totalNs += ns;
        }
        profile->tileWorkNs = totalNs;
      }
    }
  }

  if (profile && !renderTiles.empty()) {
    profile->renderTilesNs = to_ns(tilesStart, std::chrono::steady_clock::now());
  }

  auto debugStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
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
      uint32_t tx = tileIndex % tilesX;
      uint32_t ty = tileIndex / tilesX;
      uint32_t tx0 = tx * tileSize;
      uint32_t ty0 = ty * tileSize;
      uint32_t tx1 = std::min(tx0 + tileSize, target.width);
      uint32_t ty1 = std::min(ty0 + tileSize, target.height);

      uint32_t lineWidth = debugLineWidth;
      uint32_t innerX0 = tx0 + lineWidth;
      uint32_t innerY0 = ty0 + lineWidth;
      uint32_t innerX1 = (tx1 > lineWidth) ? tx1 - lineWidth : tx1;
      uint32_t innerY1 = (ty1 > lineWidth) ? ty1 - lineWidth : ty1;

      uint32_t x0 = std::min(innerX0, tx1);
      uint32_t y0 = std::min(innerY0, ty1);
      uint32_t x1 = std::max(innerX1, tx0);
      uint32_t y1 = std::max(innerY1, ty0);

      for (uint32_t y = ty0; y < ty1; ++y) {
        uint8_t* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes +
                       static_cast<size_t>(4 * tx0);
        for (uint32_t x = tx0; x < tx1; ++x, row += 4) {
          bool onBorder = (x < x0 || x >= x1 || y < y0 || y >= y1);
          if (!onBorder) continue;
          if (dA == 255u) {
            row[0] = dR;
            row[1] = dG;
            row[2] = dB;
            row[3] = dA;
          } else {
            uint8_t pmR = static_cast<uint8_t>((static_cast<uint16_t>(dR) * dA + 127u) / 255u);
            uint8_t pmG = static_cast<uint8_t>((static_cast<uint16_t>(dG) * dA + 127u) / 255u);
            uint8_t pmB = static_cast<uint8_t>((static_cast<uint16_t>(dB) * dA + 127u) / 255u);
            blend_premultiplied(row, pmR, pmG, pmB, dA);
          }
        }
      }
    }
  }

  if (profile) {
    if (debugTiles) {
      profile->renderDebugNs = to_ns(debugStart, std::chrono::steady_clock::now());
    }
    profile->renderedTileCount = renderedTiles.load(std::memory_order_relaxed);
    profile->renderedCommandCount = renderedCommands.load(std::memory_order_relaxed);
    profile->renderedPixelCount = renderedPixels.load(std::memory_order_relaxed);
    profile->renderedRectCount = renderedRects.load(std::memory_order_relaxed);
    profile->renderedTextCount = renderedTexts.load(std::memory_order_relaxed);
    profile->renderedRectPixels = renderedRectPixels.load(std::memory_order_relaxed);
    profile->renderedTextPixels = renderedTextPixels.load(std::memory_order_relaxed);
    profile->renderedTileBufferPixels = renderedTileBufferPixels.load(std::memory_order_relaxed);
  }
}

} // namespace

void RenderOptimized(RenderTarget target, RenderBatch const& batch, OptimizedBatch const& prepared) {
  RenderOptimizedImpl(target, batch, prepared);
}

} // namespace PrimeManifest
