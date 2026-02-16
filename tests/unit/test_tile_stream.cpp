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

TEST_CASE("premerge_includes_macro_and_global_commands") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 4;

  add_rect(batch, 0, 0, 8, 8, PackRGBA8(Color{10, 20, 30, 255}));
  batch.rects.flags[0] = RectFlagClip;
  batch.rects.clipX0[0] = 2;
  batch.rects.clipY0[0] = 2;
  batch.rects.clipX1[0] = 6;
  batch.rects.clipY1[0] = 6;

  add_circle(batch, 6, 6, 2, PackRGBA8(Color{200, 10, 10, 255}));

  add_text(batch, 0, 4, 8, 4, PackRGBA8(Color{10, 200, 10, 255}), 0);
  batch.text.flags[0] = TextFlagClip;
  batch.text.clipX0[0] = 2;
  batch.text.clipY0[0] = 4;
  batch.text.clipX1[0] = 6;
  batch.text.clipY1[0] = 7;

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = false;
  batch.tileStream.offsets = {0, 1, 2, 3, 4};
  batch.tileStream.commands.resize(4);
  for (uint32_t i = 0; i < 4; ++i) {
    TileCommand cmd{};
    cmd.type = CommandType::Rect;
    cmd.index = 0;
    cmd.order = 10 + i;
    cmd.x = 0;
    cmd.y = 0;
    cmd.wMinus1 = 3;
    cmd.hMinus1 = 3;
    batch.tileStream.commands[i] = cmd;
  }

  batch.tileStream.macroOffsets = {0, 1};
  TileCommand macroCmd{};
  macroCmd.type = CommandType::Rect;
  macroCmd.index = 0;
  macroCmd.order = 1;
  macroCmd.x = 1;
  macroCmd.y = 1;
  macroCmd.wMinus1 = 2;
  macroCmd.hMinus1 = 2;
  batch.tileStream.macroCommands = {macroCmd};

  TileCommand globalRect{};
  globalRect.type = CommandType::Rect;
  globalRect.index = 0;
  globalRect.order = 2;
  TileCommand globalCircle{};
  globalCircle.type = CommandType::Circle;
  globalCircle.index = 0;
  globalCircle.order = 3;
  TileCommand globalText{};
  globalText.type = CommandType::Text;
  globalText.index = 0;
  globalText.order = 4;
  batch.tileStream.globalCommands = {globalRect, globalCircle, globalText};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "optimizer succeeds");
  CHECK_MESSAGE(optimized.useTileStream, "premerge keeps tile stream enabled");
  CHECK_MESSAGE(optimized.tileStream == &optimized.mergedTileStream, "premerge stores merged stream");
  CHECK_MESSAGE(optimized.mergedTileStream.enabled, "merged tile stream enabled");
  CHECK_MESSAGE(optimized.mergedTileStream.preMerged, "merged tile stream flagged premerged");
  CHECK_MESSAGE(optimized.mergedTileStream.commands.size() > 4, "merged commands include extras");

  bool hasCircle = false;
  bool hasText = false;
  for (auto const& cmd : optimized.mergedTileStream.commands) {
    if (cmd.type == CommandType::Circle) {
      hasCircle = true;
    } else if (cmd.type == CommandType::Text) {
      hasText = true;
    }
  }
  CHECK_MESSAGE(hasCircle, "circle global commands merged");
  CHECK_MESSAGE(hasText, "text global commands merged");
}

TEST_SUITE_END();
