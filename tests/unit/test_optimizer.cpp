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

PM_TEST(optimizer, rejects_missing_palette) {
  RenderBatch batch;
  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(!optimized.valid, "optimizer rejects missing palette");
}

PM_TEST(optimizer, reuse_optimized_short_circuit) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{10, 20, 30, 255}));
  batch.tileSize = 32;
  batch.reuseOptimized = true;
  batch.revision = 7;
  add_rect(batch, 0, 0, 2, 2, PackRGBA8(Color{10, 20, 30, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  optimized.valid = true;
  optimized.sourceRevision = 7;
  optimized.targetWidth = width;
  optimized.targetHeight = height;
  optimized.tileSize = 32;
  optimized.clearColor = 0x11223344u;
  optimized.useTileStream = true;

  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(optimized.clearColor == 0x11223344u, "reuse optimized keeps cached data");
  PM_CHECK(optimized.useTileStream, "reuse optimized keeps tile stream flag");
}

PM_TEST(optimizer, auto_tile_stream_generates) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 0, 0, 6, 6, PackRGBA8(Color{10, 20, 30, 255}));

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(optimized.valid, "optimizer succeeds");
  PM_CHECK(optimized.useTileStream, "auto tile stream enabled");
  PM_CHECK(optimized.tileStream != nullptr, "tile stream pointer set");
  PM_CHECK(optimized.tileStream->preMerged, "auto tile stream premerged");
  PM_CHECK(!optimized.tileStream->commands.empty(), "auto tile stream has commands");
}

PM_TEST(optimizer, invalid_tile_stream_disabled) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = false;
  batch.tileStream.offsets = {0};
  batch.tileStream.commands.clear();
  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{200, 0, 0, 255}));

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(optimized.valid, "optimizer succeeds with invalid stream");
  PM_CHECK(!optimized.useTileStream, "invalid tile stream disabled");
}

PM_TEST(optimizer, premade_tile_stream_used) {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 8;
  add_rect(batch, 0, 0, 8, 8, PackRGBA8(Color{50, 60, 70, 255}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Rect;
  cmd.index = 0;
  cmd.order = 0;
  cmd.x = 0;
  cmd.y = 0;
  cmd.wMinus1 = 7;
  cmd.hMinus1 = 7;
  batch.tileStream.commands = {cmd};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  PM_CHECK(optimized.valid, "optimizer succeeds with premade stream");
  PM_CHECK(optimized.useTileStream, "premade tile stream used");
  PM_CHECK(optimized.tileStream == &batch.tileStream, "tile stream pointer matches batch");
}
