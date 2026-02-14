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

PM_TEST(optimizer_text_cache, text_cache_populated_for_active_text) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{10, 20, 30, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.text.x.size());
  batch.text.x.push_back(0);
  batch.text.y.push_back(0);
  batch.text.width.push_back(2);
  batch.text.height.push_back(2);
  batch.text.zQ8_8.push_back(0);
  batch.text.opacity.push_back(255);
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

  PM_CHECK(optimized.valid, "optimizer succeeds");
  PM_CHECK(!optimized.textPmOffset.empty(), "text PM offsets created");
  PM_CHECK(optimized.textPmOffset[0] != 0xFFFFFFFFu, "text PM offset valid");
  PM_CHECK(optimized.textColorR[0] == 10, "text color cache populated");
}
