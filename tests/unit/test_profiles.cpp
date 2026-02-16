#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_helpers.hpp"
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

namespace {

void enable_palette(RenderBatch& batch, uint32_t color) {
  batch.palette.enabled = true;
  batch.palette.size = 1;
  batch.palette.colorRGBA8[0] = color;
}

} // namespace


TEST_SUITE_BEGIN("primemanifest.profile");

TEST_CASE("render_populates_stats") {
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

  CHECK_MESSAGE(profile.tileCount > 0, "profile tracks tile count");
  CHECK_MESSAGE(profile.commandCount > 0, "profile tracks command count");
  CHECK_MESSAGE(profile.renderedRectCount >= 1, "profile counts rects");
  CHECK_MESSAGE(profile.renderedPixelCount > 0, "profile counts pixels");
}

TEST_CASE("tile_buffer_pixels_reported") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 0}));
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 128}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 0};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  RendererProfile profile;
  batch.profile = &profile;

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(profile.renderedTileBufferPixels == 64, "tile buffer pixels counted");
}

TEST_CASE("tile_pool_records_workers") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.tileSize = 4;
  add_rect(batch, 0, 0, 32, 32, PackRGBA8(Color{200, 100, 50, 255}));

  uint32_t width = 32;
  uint32_t height = 32;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  RendererProfile profile;
  batch.profile = &profile;

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  CHECK_MESSAGE(profile.workerNs.size() > 1, "tile pool worker times recorded");
  CHECK_MESSAGE(profile.workerTiles.size() == profile.workerNs.size(), "worker tile counts sized");
  CHECK_MESSAGE(profile.tileWorkNs > 0, "tile work time recorded");
}

TEST_SUITE_END();
