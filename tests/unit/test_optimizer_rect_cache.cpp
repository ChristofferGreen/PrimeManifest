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


TEST_SUITE_BEGIN("primemanifest.optimizer_rect_cache");

TEST_CASE("rect_cache_populated_for_active_rect") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{12, 34, 56, 255}));

  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{12, 34, 56, 255}));

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds");
  CHECK_MESSAGE(!optimized.rectEdgeOffset.empty(), "rect edge offsets created");
  CHECK_MESSAGE(optimized.rectEdgeOffset[0] != 0xFFFFFFFFu, "rect edge offset valid");
  CHECK_MESSAGE(optimized.rectColorR[0] == 12, "rect color cache populated");
}

TEST_SUITE_END();
