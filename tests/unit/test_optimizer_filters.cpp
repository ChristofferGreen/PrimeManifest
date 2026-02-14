#include "PrimeManifest/renderer/Optimizer2D.hpp"

#include "test_helpers.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

namespace {

void enable_palette(RenderBatch& batch, uint32_t color = PackRGBA8(Color{0, 0, 0, 255})) {
  batch.palette.enabled = true;
  batch.palette.size = 1;
  batch.palette.colorRGBA8[0] = color;
}

} // namespace

PM_TEST(optimizer_filters, text_clipped_out_skips_draw) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{255, 255, 255, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(0);
  batch.text.y.push_back(0);
  batch.text.width.push_back(2);
  batch.text.height.push_back(2);
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(255);
  batch.text.colorIndex.push_back(0);
  batch.text.flags.push_back(TextFlagClip);
  batch.text.runIndex.push_back(0);
  batch.text.clipX0.push_back(5);
  batch.text.clipY0.push_back(5);
  batch.text.clipX1.push_back(6);
  batch.text.clipY1.push_back(6);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(!optimized.valid, "text clipped out yields no work");
}

PM_TEST(optimizer_filters, rect_clipped_out_skips_draw) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{255, 0, 0, 255}));

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
  batch.rects.flags.push_back(RectFlagClip);
  batch.rects.gradientColor1Index.push_back(0);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(6);
  batch.rects.clipY0.push_back(6);
  batch.rects.clipX1.push_back(7);
  batch.rects.clipY1.push_back(7);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(!optimized.valid, "rect clipped out yields no work");
}

PM_TEST(optimizer_filters, transparent_gradient_skips_draw) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 0}));
  batch.palette.size = 2;
  batch.palette.colorRGBA8[1] = PackRGBA8(Color{10, 20, 30, 0});

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

  PM_CHECK(!optimized.valid, "transparent gradient yields no work");
}

PM_TEST(optimizer_filters, text_opacity_zero_skips_draw) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{255, 255, 255, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(0);
  batch.text.y.push_back(0);
  batch.text.width.push_back(2);
  batch.text.height.push_back(2);
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(0);
  batch.text.colorIndex.push_back(0);
  batch.text.flags.push_back(0);
  batch.text.runIndex.push_back(0);
  batch.text.clipX0.push_back(0);
  batch.text.clipY0.push_back(0);
  batch.text.clipX1.push_back(0);
  batch.text.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Text, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(!optimized.valid, "text opacity zero yields no work");
}

PM_TEST(optimizer_filters, clear_pattern_offset_out_of_bounds_ignored) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.tileSize = 8;

  uint32_t idx = static_cast<uint32_t>(batch.clearPattern.width.size());
  batch.clearPattern.width.push_back(4);
  batch.clearPattern.height.push_back(4);
  batch.clearPattern.dataOffset.push_back(16);
  batch.clearPattern.data.assign(16, 255);
  batch.commands.push_back(RenderCommand{CommandType::ClearPattern, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(!optimized.valid, "clear pattern offset out of bounds ignored");
}

PM_TEST(optimizer_filters, clear_pattern_valid_sets_flags) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.tileSize = 8;

  std::vector<uint32_t> pixels = {
    PackRGBA8(Color{10, 20, 30, 255}),
    PackRGBA8(Color{40, 50, 60, 255}),
    PackRGBA8(Color{70, 80, 90, 255}),
    PackRGBA8(Color{100, 110, 120, 255}),
  };
  add_clear_pattern(batch, 2, 2, pixels);

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(optimized.valid, "valid clear pattern produces optimized batch");
  PM_CHECK(optimized.clearPattern, "clear pattern flag set");
  PM_CHECK(optimized.clearPatternWidth == 2, "clear pattern width cached");
  PM_CHECK(optimized.clearPatternHeight == 2, "clear pattern height cached");
}

PM_TEST(optimizer_filters, clear_command_sets_color) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{1, 2, 3, 255}));
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(optimized.valid, "clear command produces optimized batch");
  PM_CHECK(optimized.hasClear, "clear flag set");
  PM_CHECK(optimized.clearColor == PackRGBA8(Color{10, 20, 30, 255}), "clear color cached");
}
