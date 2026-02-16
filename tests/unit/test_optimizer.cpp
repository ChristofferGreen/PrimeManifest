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


TEST_SUITE_BEGIN("primemanifest.optimizer");

TEST_CASE("rejects_missing_palette") {
  RenderBatch batch;
  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(!optimized.valid, "optimizer rejects missing palette");
}

TEST_CASE("reuse_optimized_short_circuit") {
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

  CHECK_MESSAGE(optimized.clearColor == 0x11223344u, "reuse optimized keeps cached data");
  CHECK_MESSAGE(optimized.useTileStream, "reuse optimized keeps tile stream flag");
}

TEST_CASE("auto_tile_stream_generates") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 0, 0, 6, 6, PackRGBA8(Color{10, 20, 30, 255}));

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds");
  CHECK_MESSAGE(optimized.useTileStream, "auto tile stream enabled");
  CHECK_MESSAGE(optimized.tileStream != nullptr, "tile stream pointer set");
  CHECK_MESSAGE(optimized.tileStream->preMerged, "auto tile stream premerged");
  CHECK_MESSAGE(!optimized.tileStream->commands.empty(), "auto tile stream has commands");
}

TEST_CASE("invalid_tile_stream_disabled") {
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

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds with invalid stream");
  CHECK_MESSAGE(!optimized.useTileStream, "invalid tile stream disabled");
}

TEST_CASE("premade_tile_stream_used") {
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

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds with premade stream");
  CHECK_MESSAGE(optimized.useTileStream, "premade tile stream used");
  CHECK_MESSAGE(optimized.tileStream == &batch.tileStream, "tile stream pointer matches batch");
}

TEST_CASE("premerge_tile_stream_with_fallback_macro_offsets") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 4;

  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{20, 30, 40, 255}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = false;
  batch.tileStream.offsets = {0, 1, 2, 3, 4};
  batch.tileStream.commands.resize(4);
  for (uint32_t i = 0; i < 4; ++i) {
    TileCommand cmd{};
    cmd.type = CommandType::Rect;
    cmd.index = 0;
    cmd.order = i;
    cmd.x = 0;
    cmd.y = 0;
    cmd.wMinus1 = 3;
    cmd.hMinus1 = 3;
    batch.tileStream.commands[i] = cmd;
  }
  batch.tileStream.macroOffsets.clear();
  batch.tileStream.macroCommands.clear();

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds with premerge");
  CHECK_MESSAGE(optimized.useTileStream, "premerge keeps tile stream enabled");
  CHECK_MESSAGE(optimized.tileStream == &optimized.mergedTileStream, "premerge stores merged tile stream");
}

TEST_CASE("circle_binning_parallel_path") {
  RenderBatch batch;
  uint32_t color = PackRGBA8(Color{20, 40, 60, 255});
  enable_palette(batch, color);
  batch.tileSize = 32;

  constexpr uint32_t circleCount = 50000;
  batch.commands.reserve(circleCount);
  batch.circles.centerX.reserve(circleCount);
  batch.circles.centerY.reserve(circleCount);
  batch.circles.radius.reserve(circleCount);
  batch.circles.colorIndex.reserve(circleCount);

  uint32_t width = 64;
  uint32_t height = 64;
  for (uint32_t i = 0; i < circleCount; ++i) {
    int32_t x = static_cast<int32_t>(i % width);
    int32_t y = static_cast<int32_t>((i / width) % height);
    add_circle(batch, x, y, 1, color);
  }

  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds");
  CHECK_MESSAGE(optimized.tileRefsAreCircleIndices, "circle-only draw uses circle refs");
  CHECK_MESSAGE(optimized.tileOffsets.size() == optimized.tileCount + 1, "tile offsets sized");
  CHECK_MESSAGE(optimized.tileRefs.size() >= circleCount, "tile refs include all circles");
}

TEST_CASE("clear_pattern_too_large_ignored") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.tileSize = 16;

  uint32_t idx = static_cast<uint32_t>(batch.clearPattern.width.size());
  batch.clearPattern.width.push_back(32);
  batch.clearPattern.height.push_back(32);
  batch.clearPattern.dataOffset.push_back(0);
  batch.clearPattern.data.assign(static_cast<size_t>(32 * 32 * 4), 255);
  batch.commands.push_back(RenderCommand{CommandType::ClearPattern, idx});

  uint32_t width = 32;
  uint32_t height = 32;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(!optimized.valid, "oversized clear pattern ignored");
}

TEST_CASE("rejects_empty_palette_size") {
  RenderBatch batch;
  batch.palette.enabled = true;
  batch.palette.size = 0;

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(!optimized.valid, "empty palette size rejected");
}

TEST_CASE("rejects_short_target_span") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 0, 0, 2, 2, PackRGBA8(Color{10, 20, 30, 255}));

  std::vector<uint8_t> buffer(4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), 2, 2, 8};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(!optimized.valid, "short target span rejected");
}

TEST_CASE("disables_tile_stream_when_tile_size_too_large") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 512;
  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{10, 20, 30, 255}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  batch.tileStream.commands.resize(1);

  uint32_t width = 512;
  uint32_t height = 512;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds with large tiles");
  CHECK_MESSAGE(!optimized.useTileStream, "tile stream disabled when tile size too large");
}

TEST_CASE("premerge_invalid_macro_offsets_disables_stream") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 4;
  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{20, 30, 40, 255}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = false;
  batch.tileStream.offsets = {0, 1, 2, 3, 4};
  batch.tileStream.commands.resize(4);
  batch.tileStream.macroOffsets = {0};
  batch.tileStream.macroCommands.clear();

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds with invalid macro offsets");
  CHECK_MESSAGE(!optimized.useTileStream, "invalid macro offsets disable tile stream");
}

TEST_SUITE_END();
