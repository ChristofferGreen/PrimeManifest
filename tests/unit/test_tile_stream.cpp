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

auto build_equivalence_scene() -> RenderBatch {
  RenderBatch batch;
  batch.tileSize = 8;

  add_clear(batch, PackRGBA8(Color{8, 8, 8, 255}));
  add_rect(batch, 1, 1, 14, 11, PackRGBA8(Color{20, 40, 180, 255}));
  add_rect(batch, 8, 6, 19, 17, PackRGBA8(Color{180, 90, 40, 255}));
  batch.rects.flags[1] = RectFlagClip;
  batch.rects.clipX0[1] = 9;
  batch.rects.clipY0[1] = 7;
  batch.rects.clipX1[1] = 18;
  batch.rects.clipY1[1] = 16;
  add_circle(batch, 10, 8, 4, PackRGBA8(Color{220, 70, 30, 255}));
  add_set_pixel(batch, 2, 18, PackRGBA8(Color{255, 255, 0, 255}));
  add_set_pixel_a(batch, 20, 3, PackRGBA8(Color{255, 255, 255, 255}), 96);
  return batch;
}

auto render_scene(RenderBatch const& batch, uint32_t width, uint32_t height) -> std::vector<uint8_t> {
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};
  render_batch(target, batch);
  return buffer;
}

auto is_draw_command(CommandType type) -> bool {
  return type == CommandType::Rect ||
         type == CommandType::Circle ||
         type == CommandType::Text ||
         type == CommandType::SetPixel ||
         type == CommandType::SetPixelA ||
         type == CommandType::Line ||
         type == CommandType::Image;
}

auto build_global_manual_stream(RenderBatch const& scene, uint32_t width, uint32_t height) -> TileStream {
  TileStream stream;
  stream.enabled = true;
  stream.preMerged = false;
  uint32_t tileSize = scene.tileSize == 0 ? 32u : scene.tileSize;
  uint32_t tilesX = (width + tileSize - 1) / tileSize;
  uint32_t tilesY = (height + tileSize - 1) / tileSize;
  stream.offsets.assign(static_cast<size_t>(tilesX) * tilesY + 1, 0);

  for (uint32_t commandIndex = 0; commandIndex < scene.commands.size(); ++commandIndex) {
    auto const& command = scene.commands[commandIndex];
    if (!is_draw_command(command.type)) continue;
    TileCommand out{};
    out.type = command.type;
    out.index = command.index;
    out.order = commandIndex;
    stream.globalCommands.push_back(out);
  }
  return stream;
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

TEST_CASE("premerge_global_image_clip_limits_tiles") {
  RenderBatch batch;
  enable_palette(batch, PackRGBA8(Color{255, 255, 255, 255}));
  batch.autoTileStream = false;
  batch.tileSize = 4;

  uint32_t imageIdx = add_image_asset(batch, 1, 1, {PackRGBA8(Color{255, 0, 0, 255})});
  add_image_draw(batch,
                 imageIdx,
                 0,
                 0,
                 8,
                 8,
                 0,
                 0,
                 1,
                 1,
                 PackRGBA8(Color{255, 255, 255, 255}),
                 255,
                 ImageFlagClip,
                 IntRect{4, 0, 8, 8});

  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = false;
  batch.tileStream.offsets = {0, 0, 0, 0, 0};
  batch.tileStream.commands.clear();
  batch.tileStream.macroOffsets = {0, 0};
  batch.tileStream.macroCommands.clear();

  TileCommand globalImage{};
  globalImage.type = CommandType::Image;
  globalImage.index = 0;
  globalImage.order = 1;
  batch.tileStream.globalCommands = {globalImage};

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  REQUIRE_MESSAGE(optimized.valid, "optimizer succeeds");
  REQUIRE_MESSAGE(optimized.useTileStream, "premerge keeps tile stream enabled");
  REQUIRE_MESSAGE(optimized.tileStream == &optimized.mergedTileStream, "premerge stores merged stream");
  auto const& merged = optimized.mergedTileStream;
  REQUIRE_MESSAGE(merged.offsets.size() == 5, "expected 2x2 tile offsets");
  CHECK_MESSAGE(merged.offsets[1] == 0, "top-left tile has no clipped image command");
  CHECK_MESSAGE(merged.offsets[2] == 1, "top-right tile has clipped image command");
  CHECK_MESSAGE(merged.offsets[3] == 1, "bottom-left tile has no clipped image command");
  CHECK_MESSAGE(merged.offsets[4] == 2, "bottom-right tile has clipped image command");
}

TEST_CASE("tile_stream_on_off_scene_equivalence") {
  constexpr uint32_t width = 24;
  constexpr uint32_t height = 20;

  RenderBatch noStream = build_equivalence_scene();
  noStream.autoTileStream = false;
  noStream.tileStream.clear();
  auto noStreamBuffer = render_scene(noStream, width, height);

  RenderBatch autoStreamSource = build_equivalence_scene();
  autoStreamSource.autoTileStream = true;
  autoStreamSource.tileStream.clear();
  std::vector<uint8_t> scratch(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(scratch), width, height, width * 4};
  OptimizedBatch autoPrepared;
  OptimizeRenderBatch(target, autoStreamSource, autoPrepared);
  REQUIRE_MESSAGE(autoPrepared.valid, "auto path optimized");
  REQUIRE_MESSAGE(autoPrepared.useTileStream, "auto path uses tile stream");
  REQUIRE_MESSAGE(autoPrepared.tileStream != nullptr, "auto stream pointer set");
  REQUIRE_MESSAGE(autoPrepared.tileStream->preMerged, "auto stream is premerged");

  RenderBatch manualPremerged = build_equivalence_scene();
  manualPremerged.autoTileStream = false;
  manualPremerged.tileStream = *autoPrepared.tileStream;
  manualPremerged.tileStream.enabled = true;
  manualPremerged.tileStream.preMerged = true;
  auto manualPremergedBuffer = render_scene(manualPremerged, width, height);

  CHECK_MESSAGE(buffers_equal(noStreamBuffer, manualPremergedBuffer),
                "scene output matches with tile stream disabled/enabled");
}

TEST_CASE("auto_tile_stream_on_off_scene_equivalence") {
  constexpr uint32_t width = 24;
  constexpr uint32_t height = 20;

  RenderBatch autoOff = build_equivalence_scene();
  autoOff.autoTileStream = false;
  autoOff.tileStream.clear();
  auto autoOffBuffer = render_scene(autoOff, width, height);

  RenderBatch autoOn = build_equivalence_scene();
  autoOn.autoTileStream = true;
  autoOn.tileStream.clear();
  std::vector<uint8_t> scratch(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(scratch), width, height, width * 4};
  OptimizedBatch autoPrepared;
  OptimizeRenderBatch(target, autoOn, autoPrepared);
  REQUIRE_MESSAGE(autoPrepared.valid, "auto path optimized");
  REQUIRE_MESSAGE(autoPrepared.useTileStream, "auto path uses tile stream");
  auto autoOnBuffer = render_scene(autoOn, width, height);

  CHECK_MESSAGE(buffers_equal(autoOffBuffer, autoOnBuffer),
                "scene output matches with auto tile stream disabled/enabled");
}

TEST_CASE("premerged_and_manual_stream_scene_equivalence") {
  constexpr uint32_t width = 24;
  constexpr uint32_t height = 20;

  RenderBatch autoStreamSource = build_equivalence_scene();
  autoStreamSource.autoTileStream = true;
  autoStreamSource.tileStream.clear();
  std::vector<uint8_t> scratch(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(scratch), width, height, width * 4};
  OptimizedBatch autoPrepared;
  OptimizeRenderBatch(target, autoStreamSource, autoPrepared);
  REQUIRE_MESSAGE(autoPrepared.valid, "auto path optimized");
  REQUIRE_MESSAGE(autoPrepared.useTileStream, "auto path uses tile stream");
  REQUIRE_MESSAGE(autoPrepared.tileStream != nullptr, "auto stream pointer set");
  REQUIRE_MESSAGE(autoPrepared.tileStream->preMerged, "auto stream is premerged");

  RenderBatch manualPremerged = build_equivalence_scene();
  manualPremerged.autoTileStream = false;
  manualPremerged.tileStream = *autoPrepared.tileStream;
  manualPremerged.tileStream.enabled = true;
  manualPremerged.tileStream.preMerged = true;

  RenderBatch manualNonPremerged = build_equivalence_scene();
  manualNonPremerged.autoTileStream = false;
  manualNonPremerged.tileStream = build_global_manual_stream(manualNonPremerged, width, height);

  OptimizedBatch manualPrepared;
  OptimizeRenderBatch(target, manualNonPremerged, manualPrepared);
  REQUIRE_MESSAGE(manualPrepared.valid, "manual stream optimized");
  REQUIRE_MESSAGE(manualPrepared.useTileStream, "manual stream stays enabled");
  REQUIRE_MESSAGE(manualPrepared.tileStream == &manualPrepared.mergedTileStream,
                  "manual non-premerged stream is merged");

  auto premergedBuffer = render_scene(manualPremerged, width, height);
  auto manualBuffer = render_scene(manualNonPremerged, width, height);
  CHECK_MESSAGE(buffers_equal(premergedBuffer, manualBuffer),
                "scene output matches with premerged and manual streams");
}

TEST_SUITE_END();
