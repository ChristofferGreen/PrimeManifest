#pragma once

#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_harness.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace PrimeManifestTest {

using namespace PrimeManifest;

inline void render_batch(RenderTarget target, RenderBatch const& batch) {
  RenderBatch local = batch;
  local.assumeFrontToBack = false;
  OptimizedBatch optimized;
  OptimizeRenderBatch(target, local, optimized);
  RenderOptimized(target, local, optimized);
}

inline auto palette_index(RenderBatch& batch, uint32_t color) -> uint8_t {
  if (!batch.palette.enabled) {
    batch.palette.enabled = true;
    batch.palette.size = 0;
    batch.palette.colorRGBA8.fill(0u);
  }
  for (uint16_t i = 0; i < batch.palette.size; ++i) {
    if (batch.palette.colorRGBA8[i] == color) {
      return static_cast<uint8_t>(i);
    }
  }
  if (batch.palette.size >= batch.palette.colorRGBA8.size()) {
    PM_CHECK(false, "palette overflow");
    return 0;
  }
  uint8_t idx = static_cast<uint8_t>(batch.palette.size++);
  batch.palette.colorRGBA8[idx] = color;
  return idx;
}

inline auto build_test_colors() -> std::array<uint32_t, 64> {
  std::array<uint32_t, 64> colors{};
  uint8_t levels[4] = {0, 85, 170, 255};
  size_t idx = 0;
  for (uint8_t r : levels) {
    for (uint8_t g : levels) {
      for (uint8_t b : levels) {
        colors[idx++] = PackRGBA8(Color{r, g, b, 255});
      }
    }
  }
  return colors;
}

inline auto pixel_at(std::vector<uint8_t> const& buffer, uint32_t width, uint32_t x, uint32_t y) -> uint32_t {
  size_t idx = static_cast<size_t>(y) * width * 4 + static_cast<size_t>(x) * 4;
  uint32_t r = buffer[idx + 0];
  uint32_t g = buffer[idx + 1];
  uint32_t b = buffer[idx + 2];
  uint32_t a = buffer[idx + 3];
  return r | (g << 8) | (b << 16) | (a << 24);
}

inline auto channel_at(std::vector<uint8_t> const& buffer, uint32_t width, uint32_t x, uint32_t y, int channel)
    -> uint8_t {
  size_t idx = static_cast<size_t>(y) * width * 4 + static_cast<size_t>(x) * 4;
  return buffer[idx + static_cast<size_t>(channel)];
}

inline auto buffers_equal(std::vector<uint8_t> const& a, std::vector<uint8_t> const& b) -> bool {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

inline void add_clear(RenderBatch& batch, uint32_t color) {
  uint32_t idx = static_cast<uint32_t>(batch.clear.colorIndex.size());
  batch.clear.colorIndex.push_back(palette_index(batch, color));
  batch.commands.push_back(RenderCommand{CommandType::Clear, idx});
}

inline void add_clear_pattern(RenderBatch& batch,
                              uint16_t width,
                              uint16_t height,
                              std::vector<uint32_t> const& pixels) {
  uint32_t idx = static_cast<uint32_t>(batch.clearPattern.width.size());
  batch.clearPattern.width.push_back(width);
  batch.clearPattern.height.push_back(height);
  batch.clearPattern.dataOffset.push_back(static_cast<uint32_t>(batch.clearPattern.data.size()));
  batch.clearPattern.data.reserve(batch.clearPattern.data.size() + pixels.size() * 4u);
  for (uint32_t color : pixels) {
    batch.clearPattern.data.push_back(static_cast<uint8_t>(color & 0xFFu));
    batch.clearPattern.data.push_back(static_cast<uint8_t>((color >> 8) & 0xFFu));
    batch.clearPattern.data.push_back(static_cast<uint8_t>((color >> 16) & 0xFFu));
    batch.clearPattern.data.push_back(static_cast<uint8_t>((color >> 24) & 0xFFu));
  }
  batch.commands.push_back(RenderCommand{CommandType::ClearPattern, idx});
}

inline void add_rect(RenderBatch& batch,
                     int32_t x0,
                     int32_t y0,
                     int32_t x1,
                     int32_t y1,
                     uint32_t color) {
  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(x0);
  batch.rects.y0.push_back(y0);
  batch.rects.x1.push_back(x1);
  batch.rects.y1.push_back(y1);
  uint8_t colorIndex = palette_index(batch, color);
  batch.rects.colorIndex.push_back(colorIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(0);
  batch.rects.gradientColor1Index.push_back(colorIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
}

inline void add_gradient_rect(RenderBatch& batch,
                              int32_t x0,
                              int32_t y0,
                              int32_t x1,
                              int32_t y1,
                              uint32_t color0,
                              uint32_t color1) {
  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(x0);
  batch.rects.y0.push_back(y0);
  batch.rects.x1.push_back(x1);
  batch.rects.y1.push_back(y1);
  batch.rects.colorIndex.push_back(palette_index(batch, color0));
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagGradient);
  batch.rects.gradientColor1Index.push_back(palette_index(batch, color1));
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(256);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
}

inline void add_gradient_rect_dir(RenderBatch& batch,
                                  int32_t x0,
                                  int32_t y0,
                                  int32_t x1,
                                  int32_t y1,
                                  uint32_t color0,
                                  uint32_t color1,
                                  int16_t dirX,
                                  int16_t dirY) {
  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(x0);
  batch.rects.y0.push_back(y0);
  batch.rects.x1.push_back(x1);
  batch.rects.y1.push_back(y1);
  batch.rects.colorIndex.push_back(palette_index(batch, color0));
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagGradient);
  batch.rects.gradientColor1Index.push_back(palette_index(batch, color1));
  batch.rects.gradientDirX.push_back(dirX);
  batch.rects.gradientDirY.push_back(dirY);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
}

inline void add_text(RenderBatch& batch,
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
  batch.text.colorIndex.push_back(palette_index(batch, color));
  batch.text.flags.push_back(0);
  batch.text.runIndex.push_back(runIndex);
  batch.text.clipX0.push_back(0);
  batch.text.clipY0.push_back(0);
  batch.text.clipX1.push_back(0);
  batch.text.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});
}

} // namespace PrimeManifestTest
