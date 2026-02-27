#include "PrimeManifest/renderer/BatchBuilder.hpp"

#include "third_party/doctest.h"

#include <vector>

using namespace PrimeManifest;

namespace {

void enable_palette(RenderBatch& batch, uint16_t size = 8) {
  batch.palette.enabled = true;
  batch.palette.size = size;
  for (uint16_t i = 0; i < size; ++i) {
    batch.palette.colorRGBA8[i] = PackRGBA8(Color{static_cast<uint8_t>(i), static_cast<uint8_t>(i), static_cast<uint8_t>(i), 255});
  }
}

} // namespace

TEST_SUITE_BEGIN("primemanifest.batch_builder");

TEST_CASE("append_rect_sets_typed_fields") {
  RenderBatch batch;
  enable_palette(batch);

  RectAppend rect{};
  rect.x0 = 1;
  rect.y0 = 2;
  rect.x1 = 9;
  rect.y1 = 10;
  rect.colorIndex = 2;
  rect.radiusQ8_8 = 64;
  rect.rotationQ8_8 = 32;
  rect.zQ8_8 = 7;
  rect.opacity = 120;
  rect.smoothBlend = true;
  rect.gradient = RectGradientAppend{3, 10, 20};
  rect.clip = IntRect{2, 3, 8, 9};

  auto rectIndex = appendRect(batch, rect);

  REQUIRE_MESSAGE(rectIndex.has_value(), "typed rect append succeeds");
  CHECK_MESSAGE(*rectIndex == 0u, "first rect index is zero");
  CHECK_MESSAGE(batch.commands.size() == 1, "command appended");
  CHECK_MESSAGE(batch.commands[0].type == CommandType::Rect, "rect command type");
  CHECK_MESSAGE(batch.rects.flags[0] == (RectFlagGradient | RectFlagClip | RectFlagSmoothBlend), "flags set from typed fields");
  CHECK_MESSAGE(batch.rects.gradientColor1Index[0] == 3, "gradient color index set");
  CHECK_MESSAGE(batch.rects.clipX0[0] == 2, "clip x0 set");
}

TEST_CASE("append_circle_line_pixel_variants") {
  RenderBatch batch;
  enable_palette(batch);

  auto circleIndex = appendCircle(batch, CircleAppend{5, 6, 3, 1});
  auto lineIndex = appendLine(batch, LineAppend{0, 1, 7, 8, 512, 2, 200});
  auto pixelIndex = appendPixel(batch, PixelAppend{3, 4, 3});
  auto pixelAIndex = appendPixelA(batch, PixelAAppend{6, 7, 4, 90});

  REQUIRE(circleIndex.has_value());
  REQUIRE(lineIndex.has_value());
  REQUIRE(pixelIndex.has_value());
  REQUIRE(pixelAIndex.has_value());
  CHECK_MESSAGE(batch.commands.size() == 4, "all typed commands appended");
  CHECK_MESSAGE(batch.commands[0].type == CommandType::Circle, "circle command type");
  CHECK_MESSAGE(batch.commands[1].type == CommandType::Line, "line command type");
  CHECK_MESSAGE(batch.commands[2].type == CommandType::SetPixel, "pixel command type");
  CHECK_MESSAGE(batch.commands[3].type == CommandType::SetPixelA, "pixelA command type");
  CHECK_MESSAGE(batch.pixelsA.alpha[0] == 90, "pixelA alpha stored");
}

TEST_CASE("build_image_asset_and_append_image") {
  RenderBatch batch;
  enable_palette(batch);

  std::vector<uint32_t> pixels = {
    PackRGBA8(Color{255, 0, 0, 255}),
    PackRGBA8(Color{0, 255, 0, 255}),
    PackRGBA8(Color{0, 0, 255, 255}),
    PackRGBA8(Color{255, 255, 255, 255}),
  };
  auto imageIndex = buildImageAsset(batch, ImageAssetBuild{2, 2, pixels});
  REQUIRE_MESSAGE(imageIndex.has_value(), "typed image asset build succeeds");

  ImageAppend image{};
  image.imageIndex = *imageIndex;
  image.x0 = 1;
  image.y0 = 2;
  image.x1 = 5;
  image.y1 = 6;
  image.srcX0 = 0;
  image.srcY0 = 0;
  image.srcX1 = 2;
  image.srcY1 = 2;
  image.tintColorIndex = 5;
  image.opacity = 180;
  image.wrapU = true;
  image.clip = IntRect{2, 3, 4, 5};

  auto imageDrawIndex = appendImage(batch, image);

  REQUIRE_MESSAGE(imageDrawIndex.has_value(), "typed image append succeeds");
  CHECK_MESSAGE(batch.commands.back().type == CommandType::Image, "image command appended");
  CHECK_MESSAGE((batch.imageDraws.flags[0] & ImageFlagWrapU) != 0u, "wrapU flag set");
  CHECK_MESSAGE((batch.imageDraws.flags[0] & ImageFlagClip) != 0u, "clip flag set");
  CHECK_MESSAGE(batch.imageDraws.clipX0[0] == 2, "image clip stored");
}

TEST_CASE("typed_api_rejects_invalid_inputs") {
  RenderBatch batch;
  enable_palette(batch);

  auto badRect = appendRect(batch, RectAppend{50000, 0, 1, 1, 0});
  CHECK_MESSAGE(!badRect.has_value(), "rect rejects out-of-range coordinate");

  std::vector<uint32_t> pixels = {PackRGBA8(Color{1, 2, 3, 255})};
  auto badAsset = buildImageAsset(batch, ImageAssetBuild{2, 2, pixels});
  CHECK_MESSAGE(!badAsset.has_value(), "image asset rejects mismatched pixel count");

  auto missingImage = appendImage(batch, ImageAppend{99, 0, 0, 2, 2, 0, 0, 1, 1, 0, 255});
  CHECK_MESSAGE(!missingImage.has_value(), "image draw rejects missing image index");
}

TEST_SUITE_END();
