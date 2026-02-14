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

PM_TEST(renderer, rejects_disabled_palette) {
  RenderBatch batch;

  OptimizedBatch optimized;
  optimized.valid = true;
  optimized.targetWidth = 4;
  optimized.targetHeight = 4;

  std::vector<uint8_t> buffer(4 * 4 * 4, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), 4, 4, 16};

  RenderOptimized(target, batch, optimized);

  PM_CHECK(buffer[0] == 0x7F, "palette-disabled early return");
}

PM_TEST(renderer, rejects_target_mismatch) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{1, 2, 3, 4}));

  OptimizedBatch optimized;
  optimized.valid = true;
  optimized.targetWidth = 8;
  optimized.targetHeight = 8;

  std::vector<uint8_t> buffer(4 * 4 * 4, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), 4, 4, 16};

  RenderOptimized(target, batch, optimized);

  PM_CHECK(buffer[0] == 0x7F, "target mismatch early return");
}

PM_TEST(renderer, rejects_zero_stride) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{1, 2, 3, 4}));

  OptimizedBatch optimized;
  optimized.valid = true;
  optimized.targetWidth = 4;
  optimized.targetHeight = 4;

  std::vector<uint8_t> buffer(4 * 4 * 4, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), 4, 4, 0};

  RenderOptimized(target, batch, optimized);

  PM_CHECK(buffer[0] == 0x7F, "zero stride early return");
}

PM_TEST(renderer, tile_buffer_clear_applies) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 0}));
  batch.tileSize = 8;

  add_clear(batch, PackRGBA8(Color{10, 20, 30, 128}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 0};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  uint32_t expected = PackRGBA8(Color{5, 10, 15, 128});
  PM_CHECK(pixel_at(buffer, width, 0, 0) == expected, "tile-buffer clear writes premultiplied pixel");
}

PM_TEST(renderer, tile_buffer_clear_pattern_applies) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 0}));
  batch.tileSize = 8;

  std::vector<uint32_t> pixels = {
    PackRGBA8(Color{255, 0, 0, 255}),
    PackRGBA8(Color{0, 255, 0, 255}),
    PackRGBA8(Color{0, 0, 255, 255}),
    PackRGBA8(Color{255, 255, 255, 255}),
  };
  add_clear_pattern(batch, 2, 2, pixels);

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 0};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t green = PackRGBA8(Color{0, 255, 0, 255});
  PM_CHECK(pixel_at(buffer, width, 0, 0) == red, "tile-buffer pattern (0,0)");
  PM_CHECK(pixel_at(buffer, width, 1, 0) == green, "tile-buffer pattern (1,0)");
}
