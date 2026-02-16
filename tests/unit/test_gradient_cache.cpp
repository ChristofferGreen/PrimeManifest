#include "PrimeManifest/renderer/Optimizer2D.hpp"

#include "test_helpers.hpp"
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

namespace {

void enable_palette(RenderBatch& batch, uint32_t color = PackRGBA8(Color{0, 0, 0, 255})) {
  batch.palette.enabled = true;
  batch.palette.size = 1;
  batch.palette.colorRGBA8[0] = color;
}

} // namespace


TEST_SUITE_BEGIN("primemanifest.gradient_cache");

TEST_CASE("gradient_dir_cached") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.palette.size = 2;
  batch.palette.colorRGBA8[1] = PackRGBA8(Color{255, 255, 255, 255});

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(0);
  batch.rects.y0.push_back(0);
  batch.rects.x1.push_back(4);
  batch.rects.y1.push_back(4);
  batch.rects.colorIndex.push_back(0);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagGradient);
  batch.rects.gradientColor1Index.push_back(1);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(256);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds");
  CHECK_MESSAGE(!optimized.rectGradDirX.empty(), "gradient dir cache populated");
  CHECK_MESSAGE(optimized.rectGradDirY[0] > 0.0f, "gradient dir has positive Y");
}

TEST_CASE("zero_dir_defaults_to_vertical") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.palette.size = 2;
  batch.palette.colorRGBA8[1] = PackRGBA8(Color{255, 255, 255, 255});

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(0);
  batch.rects.y0.push_back(0);
  batch.rects.x1.push_back(4);
  batch.rects.y1.push_back(4);
  batch.rects.colorIndex.push_back(0);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagGradient);
  batch.rects.gradientColor1Index.push_back(1);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds");
  CHECK_MESSAGE(optimized.rectGradDirX[0] == 0.0f, "default gradient dir X is 0");
  CHECK_MESSAGE(optimized.rectGradDirY[0] > 0.0f, "default gradient dir Y positive");
  CHECK_MESSAGE(optimized.rectGradInvRange[0] > 0.0f, "gradient inv range positive");
}

TEST_SUITE_END();
