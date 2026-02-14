#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_helpers.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

namespace {

void enable_palette(RenderBatch& batch, uint32_t color) {
  batch.palette.enabled = true;
  batch.palette.size = 1;
  batch.palette.colorRGBA8[0] = color;
}

} // namespace

PM_TEST(profile, render_populates_stats) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 0, 0, 8, 8, PackRGBA8(Color{10, 20, 30, 255}));

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  RendererProfile profile;
  batch.profile = &profile;

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  PM_CHECK(profile.tileCount > 0, "profile tracks tile count");
  PM_CHECK(profile.commandCount > 0, "profile tracks command count");
  PM_CHECK(profile.renderedRectCount >= 1, "profile counts rects");
  PM_CHECK(profile.renderedPixelCount > 0, "profile counts pixels");
}
