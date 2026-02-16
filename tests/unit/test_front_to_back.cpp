#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_helpers.hpp"
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

namespace {

void render_front_to_back(RenderTarget target, RenderBatch const& batch) {
  RenderBatch local = batch;
  local.assumeFrontToBack = true;
  OptimizedBatch optimized;
  OptimizeRenderBatch(target, local, optimized);
  RenderOptimized(target, local, optimized);
}

} // namespace


TEST_SUITE_BEGIN("primemanifest.front_to_back");

TEST_CASE("frontmost_rect_wins") {
  RenderBatch batch;
  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{255, 0, 0, 255}));
  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{0, 255, 0, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_front_to_back(target, batch);

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == red, "frontmost rect stays on top");
}

TEST_CASE("translucent_rect_blends") {
  RenderBatch batch;
  add_rect(batch, 0, 0, 2, 2, PackRGBA8(Color{255, 255, 255, 255}));
  batch.rects.opacity[0] = 128;

  uint32_t width = 2;
  uint32_t height = 2;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_front_to_back(target, batch);

  uint32_t expected = PackRGBA8(Color{128, 128, 128, 128});
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == expected, "translucent rect blends with front-to-back path");
}

TEST_SUITE_END();
