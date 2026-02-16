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


TEST_SUITE_BEGIN("primemanifest.tile_stream");

TEST_CASE("invalid_offsets_disable_stream") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 8;
  add_rect(batch, 0, 0, 8, 8, PackRGBA8(Color{10, 20, 30, 255}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 2};
  batch.tileStream.commands.resize(1);

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds");
  CHECK_MESSAGE(!optimized.useTileStream, "tile stream disabled when offsets invalid");
}

TEST_CASE("premerge_rejects_macro_commands_without_offsets") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 4;
  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{20, 30, 40, 255}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = false;
  batch.tileStream.offsets = {0, 1, 2, 3, 4};
  batch.tileStream.commands.resize(4);
  batch.tileStream.macroOffsets.clear();
  batch.tileStream.macroCommands.resize(1);

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds");
  CHECK_MESSAGE(!optimized.useTileStream, "premerge rejects macro commands without offsets");
}

TEST_SUITE_END();
