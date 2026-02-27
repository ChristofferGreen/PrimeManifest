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

TEST_CASE("profile_tracks_invalid_command_data_skips") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 8;
  batch.assumeFrontToBack = false;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 0, 0, 8, 8, PackRGBA8(Color{255, 120, 40, 255}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 2};
  TileCommand valid{};
  valid.type = CommandType::Rect;
  valid.index = 0;
  valid.order = 0;
  valid.x = 0;
  valid.y = 0;
  valid.wMinus1 = 7;
  valid.hMinus1 = 7;
  TileCommand invalid = valid;
  invalid.index = 44;
  invalid.order = 1;
  batch.tileStream.commands = {valid, invalid};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  RendererProfile profile;
  batch.profile = &profile;

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  size_t rectType = static_cast<size_t>(CommandType::Rect);
  size_t invalidDataReason = static_cast<size_t>(SkippedCommandReason::InvalidCommandData);
  CHECK_MESSAGE(profile.skippedCommands.total >= 1, "total skipped commands recorded");
  CHECK_MESSAGE(profile.skippedCommands.byType[rectType] >= 1, "rect skipped command counted");
  CHECK_MESSAGE(profile.skippedCommands.byReason[invalidDataReason] >= 1, "invalid data reason counted");
  CHECK_MESSAGE(profile.skippedCommands.byTypeAndReason[rectType][invalidDataReason] >= 1,
                "rect+invalid-data matrix counted");
}

TEST_CASE("profile_tracks_unsupported_command_type_skips") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 8;
  batch.assumeFrontToBack = false;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));
  add_rect(batch, 0, 0, 8, 8, PackRGBA8(Color{200, 60, 20, 255}));

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 2};
  TileCommand rectCmd{};
  rectCmd.type = CommandType::Rect;
  rectCmd.index = 0;
  rectCmd.order = 0;
  rectCmd.x = 0;
  rectCmd.y = 0;
  rectCmd.wMinus1 = 7;
  rectCmd.hMinus1 = 7;
  TileCommand unsupported = rectCmd;
  unsupported.type = CommandType::Clear;
  unsupported.index = 0;
  unsupported.order = 1;
  batch.tileStream.commands = {rectCmd, unsupported};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  RendererProfile profile;
  batch.profile = &profile;

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  size_t clearType = static_cast<size_t>(CommandType::Clear);
  size_t unsupportedReason = static_cast<size_t>(SkippedCommandReason::UnsupportedCommandType);
  CHECK_MESSAGE(profile.skippedCommands.total >= 1, "skipped commands recorded");
  CHECK_MESSAGE(profile.skippedCommands.byType[clearType] >= 1, "clear type skipped command counted");
  CHECK_MESSAGE(profile.skippedCommands.byReason[unsupportedReason] >= 1, "unsupported reason counted");
  CHECK_MESSAGE(profile.skippedCommands.byTypeAndReason[clearType][unsupportedReason] >= 1,
                "clear+unsupported matrix counted");
}

TEST_CASE("optimizer_invalid_data_skips_are_separate") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 0, 0, 8, 8, PackRGBA8(Color{200, 80, 30, 255}));
  batch.commands.push_back(RenderCommand{CommandType::Rect, 999});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  RendererProfile profile;
  batch.profile = &profile;

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  size_t rectType = static_cast<size_t>(CommandType::Rect);
  size_t invalidReason = static_cast<size_t>(SkippedCommandReason::OptimizerInvalidCommandData);
  CHECK_MESSAGE(profile.optimizerSkippedCommands.total >= 1, "optimizer recorded skipped command");
  CHECK_MESSAGE(profile.optimizerSkippedCommands.byType[rectType] >= 1, "optimizer by-type count recorded");
  CHECK_MESSAGE(profile.optimizerSkippedCommands.byReason[invalidReason] >= 1, "optimizer invalid-data reason recorded");
  CHECK_MESSAGE(profile.skippedCommands.total == 0, "render-stage skips remain separate");
}

TEST_CASE("optimizer_bounds_cull_skips_are_separate") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  add_clear(batch, PackRGBA8(Color{5, 10, 15, 255}));
  add_rect(batch, -20, -20, -10, -10, PackRGBA8(Color{220, 40, 10, 255}));

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  RendererProfile profile;
  batch.profile = &profile;

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  size_t rectType = static_cast<size_t>(CommandType::Rect);
  size_t culledReason = static_cast<size_t>(SkippedCommandReason::OptimizerCulledByBounds);
  CHECK_MESSAGE(profile.optimizerSkippedCommands.total >= 1, "optimizer cull recorded");
  CHECK_MESSAGE(profile.optimizerSkippedCommands.byType[rectType] >= 1, "optimizer by-type count recorded");
  CHECK_MESSAGE(profile.optimizerSkippedCommands.byReason[culledReason] >= 1, "optimizer bounds-cull reason recorded");
  CHECK_MESSAGE(profile.skippedCommands.total == 0, "render-stage skips remain separate");
}

TEST_SUITE_END();
