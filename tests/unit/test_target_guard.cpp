#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.target_guard");

TEST_CASE("optimizer_rejects_zero_dimensions") {
  RenderBatch batch;
  batch.palette.enabled = true;
  batch.palette.size = 1;
  batch.palette.colorRGBA8[0] = PackRGBA8(Color{0, 0, 0, 255});

  std::vector<uint8_t> buffer(4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), 0, 0, 0};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(!optimized.valid, "zero-sized target rejected");
}

TEST_CASE("renderer_rejects_empty_target") {
  RenderBatch batch;
  batch.palette.enabled = true;
  batch.palette.size = 1;
  batch.palette.colorRGBA8[0] = PackRGBA8(Color{0, 0, 0, 255});

  OptimizedBatch optimized;
  optimized.valid = true;
  optimized.targetWidth = 0;
  optimized.targetHeight = 0;

  std::vector<uint8_t> buffer(4, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), 0, 0, 0};

  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(buffer[0] == 0x7F, "renderer skips empty target");
}

TEST_SUITE_END();
