#pragma once

#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace PrimeManifest {

struct RectGradientAppend {
  uint8_t colorIndex = 0;
  int16_t dirX = 0;
  int16_t dirY = 256;
};

struct RectAppend {
  int32_t x0 = 0;
  int32_t y0 = 0;
  int32_t x1 = 0;
  int32_t y1 = 0;
  uint8_t colorIndex = 0;
  uint16_t radiusQ8_8 = 0;
  int16_t rotationQ8_8 = 0;
  int16_t zQ8_8 = 0;
  uint8_t opacity = 255;
  bool smoothBlend = false;
  std::optional<RectGradientAppend> gradient;
  std::optional<IntRect> clip;
};

struct CircleAppend {
  int32_t centerX = 0;
  int32_t centerY = 0;
  uint16_t radius = 0;
  uint8_t colorIndex = 0;
};

struct PixelAppend {
  int32_t x = 0;
  int32_t y = 0;
  uint8_t colorIndex = 0;
};

struct PixelAAppend {
  int32_t x = 0;
  int32_t y = 0;
  uint8_t colorIndex = 0;
  uint8_t alpha = 255;
};

struct LineAppend {
  int32_t x0 = 0;
  int32_t y0 = 0;
  int32_t x1 = 0;
  int32_t y1 = 0;
  uint16_t widthQ8_8 = 256;
  uint8_t colorIndex = 0;
  uint8_t opacity = 255;
};

struct ImageAssetBuild {
  uint16_t width = 0;
  uint16_t height = 0;
  std::span<uint32_t const> pixelsRGBA8;
};

struct ImageAppend {
  uint32_t imageIndex = 0;
  int32_t x0 = 0;
  int32_t y0 = 0;
  int32_t x1 = 0;
  int32_t y1 = 0;
  uint16_t srcX0 = 0;
  uint16_t srcY0 = 0;
  uint16_t srcX1 = 0;
  uint16_t srcY1 = 0;
  uint8_t tintColorIndex = 0;
  uint8_t opacity = 255;
  bool wrapU = false;
  bool wrapV = false;
  std::optional<IntRect> clip;
};

auto appendRect(RenderBatch& batch, RectAppend const& rect) -> std::optional<uint32_t>;
auto appendCircle(RenderBatch& batch, CircleAppend const& circle) -> std::optional<uint32_t>;
auto appendPixel(RenderBatch& batch, PixelAppend const& pixel) -> std::optional<uint32_t>;
auto appendPixelA(RenderBatch& batch, PixelAAppend const& pixel) -> std::optional<uint32_t>;
auto appendLine(RenderBatch& batch, LineAppend const& line) -> std::optional<uint32_t>;
auto buildImageAsset(RenderBatch& batch, ImageAssetBuild const& image) -> std::optional<uint32_t>;
auto appendImage(RenderBatch& batch, ImageAppend const& image) -> std::optional<uint32_t>;

} // namespace PrimeManifest
