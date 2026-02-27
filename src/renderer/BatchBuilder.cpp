#include "PrimeManifest/renderer/BatchBuilder.hpp"

#include <cstdint>
#include <limits>

namespace PrimeManifest {
namespace {

auto fits_int16(int32_t value) -> bool {
  return value >= std::numeric_limits<int16_t>::min() && value <= std::numeric_limits<int16_t>::max();
}

} // namespace

auto appendRect(RenderBatch& batch, RectAppend const& rect) -> std::optional<uint32_t> {
  if (!fits_int16(rect.x0) || !fits_int16(rect.y0) || !fits_int16(rect.x1) || !fits_int16(rect.y1)) {
    return std::nullopt;
  }
  if (rect.x1 <= rect.x0 || rect.y1 <= rect.y0) return std::nullopt;

  uint32_t index = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(static_cast<int16_t>(rect.x0));
  batch.rects.y0.push_back(static_cast<int16_t>(rect.y0));
  batch.rects.x1.push_back(static_cast<int16_t>(rect.x1));
  batch.rects.y1.push_back(static_cast<int16_t>(rect.y1));
  batch.rects.colorIndex.push_back(rect.colorIndex);
  batch.rects.radiusQ8_8.push_back(rect.radiusQ8_8);
  batch.rects.rotationQ8_8.push_back(rect.rotationQ8_8);
  batch.rects.zQ8_8.push_back(rect.zQ8_8);
  batch.rects.opacity.push_back(rect.opacity);

  uint8_t flags = 0;
  uint8_t gradientColorIndex = rect.colorIndex;
  int16_t gradientDirX = 0;
  int16_t gradientDirY = 0;
  if (rect.gradient.has_value()) {
    flags |= RectFlagGradient;
    gradientColorIndex = rect.gradient->colorIndex;
    gradientDirX = rect.gradient->dirX;
    gradientDirY = rect.gradient->dirY;
  }
  if (rect.clip.has_value()) {
    flags |= RectFlagClip;
  }
  if (rect.smoothBlend) {
    flags |= RectFlagSmoothBlend;
  }
  batch.rects.flags.push_back(flags);
  batch.rects.gradientColor1Index.push_back(gradientColorIndex);
  batch.rects.gradientDirX.push_back(gradientDirX);
  batch.rects.gradientDirY.push_back(gradientDirY);

  IntRect clip = rect.clip.value_or(IntRect{});
  if (!fits_int16(clip.x0) || !fits_int16(clip.y0) || !fits_int16(clip.x1) || !fits_int16(clip.y1)) {
    batch.rects.x0.pop_back();
    batch.rects.y0.pop_back();
    batch.rects.x1.pop_back();
    batch.rects.y1.pop_back();
    batch.rects.colorIndex.pop_back();
    batch.rects.radiusQ8_8.pop_back();
    batch.rects.rotationQ8_8.pop_back();
    batch.rects.zQ8_8.pop_back();
    batch.rects.opacity.pop_back();
    batch.rects.flags.pop_back();
    batch.rects.gradientColor1Index.pop_back();
    batch.rects.gradientDirX.pop_back();
    batch.rects.gradientDirY.pop_back();
    return std::nullopt;
  }
  batch.rects.clipX0.push_back(static_cast<int16_t>(clip.x0));
  batch.rects.clipY0.push_back(static_cast<int16_t>(clip.y0));
  batch.rects.clipX1.push_back(static_cast<int16_t>(clip.x1));
  batch.rects.clipY1.push_back(static_cast<int16_t>(clip.y1));

  batch.commands.push_back(RenderCommand{CommandType::Rect, index});
  return index;
}

auto appendCircle(RenderBatch& batch, CircleAppend const& circle) -> std::optional<uint32_t> {
  if (!fits_int16(circle.centerX) || !fits_int16(circle.centerY)) return std::nullopt;
  if (circle.radius == 0) return std::nullopt;

  uint32_t index = static_cast<uint32_t>(batch.circles.centerX.size());
  batch.circles.centerX.push_back(static_cast<int16_t>(circle.centerX));
  batch.circles.centerY.push_back(static_cast<int16_t>(circle.centerY));
  batch.circles.radius.push_back(circle.radius);
  batch.circles.colorIndex.push_back(circle.colorIndex);
  batch.commands.push_back(RenderCommand{CommandType::Circle, index});
  return index;
}

auto appendPixel(RenderBatch& batch, PixelAppend const& pixel) -> std::optional<uint32_t> {
  if (!fits_int16(pixel.x) || !fits_int16(pixel.y)) return std::nullopt;

  uint32_t index = static_cast<uint32_t>(batch.pixels.x.size());
  batch.pixels.x.push_back(static_cast<int16_t>(pixel.x));
  batch.pixels.y.push_back(static_cast<int16_t>(pixel.y));
  batch.pixels.colorIndex.push_back(pixel.colorIndex);
  batch.commands.push_back(RenderCommand{CommandType::SetPixel, index});
  return index;
}

auto appendPixelA(RenderBatch& batch, PixelAAppend const& pixel) -> std::optional<uint32_t> {
  if (!fits_int16(pixel.x) || !fits_int16(pixel.y)) return std::nullopt;

  uint32_t index = static_cast<uint32_t>(batch.pixelsA.x.size());
  batch.pixelsA.x.push_back(static_cast<int16_t>(pixel.x));
  batch.pixelsA.y.push_back(static_cast<int16_t>(pixel.y));
  batch.pixelsA.colorIndex.push_back(pixel.colorIndex);
  batch.pixelsA.alpha.push_back(pixel.alpha);
  batch.commands.push_back(RenderCommand{CommandType::SetPixelA, index});
  return index;
}

auto appendLine(RenderBatch& batch, LineAppend const& line) -> std::optional<uint32_t> {
  if (!fits_int16(line.x0) || !fits_int16(line.y0) || !fits_int16(line.x1) || !fits_int16(line.y1)) {
    return std::nullopt;
  }
  if (line.widthQ8_8 == 0) return std::nullopt;

  uint32_t index = static_cast<uint32_t>(batch.lines.x0.size());
  batch.lines.x0.push_back(static_cast<int16_t>(line.x0));
  batch.lines.y0.push_back(static_cast<int16_t>(line.y0));
  batch.lines.x1.push_back(static_cast<int16_t>(line.x1));
  batch.lines.y1.push_back(static_cast<int16_t>(line.y1));
  batch.lines.widthQ8_8.push_back(line.widthQ8_8);
  batch.lines.colorIndex.push_back(line.colorIndex);
  batch.lines.opacity.push_back(line.opacity);
  batch.commands.push_back(RenderCommand{CommandType::Line, index});
  return index;
}

auto buildImageAsset(RenderBatch& batch, ImageAssetBuild const& image) -> std::optional<uint32_t> {
  if (image.width == 0 || image.height == 0) return std::nullopt;
  size_t pixelCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
  if (image.pixelsRGBA8.size() != pixelCount) return std::nullopt;

  uint32_t index = static_cast<uint32_t>(batch.images.width.size());
  batch.images.width.push_back(image.width);
  batch.images.height.push_back(image.height);
  batch.images.strideBytes.push_back(static_cast<uint32_t>(image.width) * 4u);
  batch.images.dataOffset.push_back(static_cast<uint32_t>(batch.images.data.size()));
  batch.images.data.reserve(batch.images.data.size() + pixelCount * 4u);
  for (uint32_t color : image.pixelsRGBA8) {
    batch.images.data.push_back(static_cast<uint8_t>(color & 0xFFu));
    batch.images.data.push_back(static_cast<uint8_t>((color >> 8) & 0xFFu));
    batch.images.data.push_back(static_cast<uint8_t>((color >> 16) & 0xFFu));
    batch.images.data.push_back(static_cast<uint8_t>((color >> 24) & 0xFFu));
  }
  return index;
}

auto appendImage(RenderBatch& batch, ImageAppend const& image) -> std::optional<uint32_t> {
  if (!fits_int16(image.x0) || !fits_int16(image.y0) || !fits_int16(image.x1) || !fits_int16(image.y1)) {
    return std::nullopt;
  }
  if (image.x1 <= image.x0 || image.y1 <= image.y0) return std::nullopt;
  if (image.imageIndex >= batch.images.width.size() || image.imageIndex >= batch.images.height.size()) {
    return std::nullopt;
  }
  if (image.srcX1 <= image.srcX0 || image.srcY1 <= image.srcY0) return std::nullopt;

  IntRect clip = image.clip.value_or(IntRect{});
  if (!fits_int16(clip.x0) || !fits_int16(clip.y0) || !fits_int16(clip.x1) || !fits_int16(clip.y1)) {
    return std::nullopt;
  }

  uint32_t index = static_cast<uint32_t>(batch.imageDraws.x0.size());
  batch.imageDraws.x0.push_back(static_cast<int16_t>(image.x0));
  batch.imageDraws.y0.push_back(static_cast<int16_t>(image.y0));
  batch.imageDraws.x1.push_back(static_cast<int16_t>(image.x1));
  batch.imageDraws.y1.push_back(static_cast<int16_t>(image.y1));
  batch.imageDraws.srcX0.push_back(image.srcX0);
  batch.imageDraws.srcY0.push_back(image.srcY0);
  batch.imageDraws.srcX1.push_back(image.srcX1);
  batch.imageDraws.srcY1.push_back(image.srcY1);
  batch.imageDraws.imageIndex.push_back(image.imageIndex);
  batch.imageDraws.tintColorIndex.push_back(image.tintColorIndex);
  batch.imageDraws.opacity.push_back(image.opacity);

  uint8_t flags = 0;
  if (image.wrapU) flags |= ImageFlagWrapU;
  if (image.wrapV) flags |= ImageFlagWrapV;
  if (image.clip.has_value()) flags |= ImageFlagClip;
  batch.imageDraws.flags.push_back(flags);
  batch.imageDraws.clipX0.push_back(static_cast<int16_t>(clip.x0));
  batch.imageDraws.clipY0.push_back(static_cast<int16_t>(clip.y0));
  batch.imageDraws.clipX1.push_back(static_cast<int16_t>(clip.x1));
  batch.imageDraws.clipY1.push_back(static_cast<int16_t>(clip.y1));

  batch.commands.push_back(RenderCommand{CommandType::Image, index});
  return index;
}

} // namespace PrimeManifest
