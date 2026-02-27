#include "PrimeManifest/renderer/Optimizer2D.hpp"

#include "test_helpers.hpp"
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

namespace {

auto report_has_code(RenderValidationReport const& report, char const* code) -> bool {
  for (auto const& issue : report.issues) {
    if (issue.code == code) return true;
  }
  return false;
}

auto buffer_all_equal(std::vector<uint8_t> const& buffer, uint8_t value) -> bool {
  for (uint8_t byte : buffer) {
    if (byte != value) return false;
  }
  return true;
}

auto render_scene(RenderBatch const& batch, uint32_t width, uint32_t height) -> std::vector<uint8_t> {
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};
  render_batch(target, batch);
  return buffer;
}

} // namespace

TEST_SUITE_BEGIN("primemanifest.validation");

TEST_CASE("strict_validation_rejects_bad_command_index") {
  RenderBatch batch;
  batch.strictValidation = true;
  RenderValidationReport report;
  batch.validationReport = &report;

  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{200, 30, 20, 255}));
  batch.commands[0].index = 7;

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(!optimized.valid, "strict validation rejects bad command index");
  CHECK_MESSAGE(report.hasErrors(), "validation report contains errors");
  CHECK_MESSAGE(report_has_code(report, "BadCommandIndex"), "report contains bad command index code");
}

TEST_CASE("strict_validation_rejects_store_size_mismatch") {
  RenderBatch batch;
  batch.strictValidation = true;
  RenderValidationReport report;
  batch.validationReport = &report;

  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{50, 100, 150, 255}));
  batch.rects.opacity.pop_back();

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(!optimized.valid, "strict validation rejects store size mismatch");
  CHECK_MESSAGE(report.hasErrors(), "validation report contains errors");
  CHECK_MESSAGE(report_has_code(report, "StoreSizeMismatch"), "report contains store size mismatch code");
}

TEST_CASE("strict_validation_rejects_bad_tile_command_index") {
  RenderBatch batch;
  batch.strictValidation = true;
  RenderValidationReport report;
  batch.validationReport = &report;

  batch.palette.enabled = true;
  batch.palette.size = 1;
  batch.palette.colorRGBA8[0] = PackRGBA8(Color{0, 0, 0, 255});
  batch.autoTileStream = false;
  batch.tileSize = 8;
  batch.tileStream.enabled = true;
  batch.tileStream.preMerged = true;
  batch.tileStream.offsets = {0, 1};
  TileCommand cmd{};
  cmd.type = CommandType::Rect;
  cmd.index = 42;
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

  CHECK_MESSAGE(!optimized.valid, "strict validation rejects bad tile command index");
  CHECK_MESSAGE(report.hasErrors(), "validation report contains errors");
  CHECK_MESSAGE(report_has_code(report, "BadTileCommandIndex"), "report contains bad tile command index code");
}

TEST_CASE("permissive_mode_keeps_previous_behavior") {
  RenderBatch batch;

  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{80, 160, 240, 255}));
  batch.rects.opacity.pop_back();

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);

  CHECK_MESSAGE(optimized.valid, "permissive mode keeps optimization path");
}

TEST_CASE("strict_validation_fail_fast_keeps_target_unchanged") {
  RenderBatch batch;
  batch.strictValidation = true;
  RenderValidationReport report;
  batch.validationReport = &report;

  add_rect(batch, 0, 0, 6, 6, PackRGBA8(Color{200, 30, 20, 255}));
  batch.commands.push_back(RenderCommand{CommandType::Rect, 99});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0x7Cu);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  CHECK_MESSAGE(!optimized.valid, "strict mode rejects malformed batch");
  CHECK_MESSAGE(report_has_code(report, "BadCommandIndex"), "strict mode reports malformed command index");

  RenderOptimized(target, batch, optimized);
  CHECK_MESSAGE(buffer_all_equal(buffer, 0x7Cu), "strict fail-fast keeps target untouched");
}

TEST_CASE("permissive_mode_skips_bad_command_index_deterministically") {
  constexpr uint32_t width = 12;
  constexpr uint32_t height = 12;
  uint32_t drawColor = PackRGBA8(Color{40, 180, 220, 255});
  uint32_t clearColor = PackRGBA8(Color{10, 10, 10, 255});

  RenderBatch clean;
  add_clear(clean, clearColor);
  add_rect(clean, 2, 2, 10, 10, drawColor);

  RenderBatch malformed = clean;
  malformed.commands.push_back(RenderCommand{CommandType::Rect, 999});

  auto cleanFirst = render_scene(clean, width, height);
  auto malformedFirst = render_scene(malformed, width, height);
  auto malformedSecond = render_scene(malformed, width, height);

  CHECK_MESSAGE(buffers_equal(cleanFirst, malformedFirst),
                "permissive mode skips malformed command and matches clean output");
  CHECK_MESSAGE(buffers_equal(malformedFirst, malformedSecond),
                "permissive mode output is deterministic across reruns");
}

TEST_CASE("permissive_mode_skips_bad_tile_command_deterministically") {
  constexpr uint32_t width = 8;
  constexpr uint32_t height = 8;

  auto build_batch_with_tile_stream = [&](bool includeBadTileCommand) {
    RenderBatch batch;
    batch.autoTileStream = false;
    batch.tileSize = 8;
    add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
    add_rect(batch, 0, 0, 8, 8, PackRGBA8(Color{255, 120, 40, 255}));

    batch.tileStream.enabled = true;
    batch.tileStream.preMerged = true;
    batch.tileStream.offsets = includeBadTileCommand ? std::vector<uint32_t>{0, 2}
                                                     : std::vector<uint32_t>{0, 1};
    TileCommand valid{};
    valid.type = CommandType::Rect;
    valid.index = 0;
    valid.order = 0;
    valid.x = 0;
    valid.y = 0;
    valid.wMinus1 = 7;
    valid.hMinus1 = 7;
    batch.tileStream.commands.push_back(valid);
    if (includeBadTileCommand) {
      TileCommand invalid = valid;
      invalid.index = 44;
      invalid.order = 1;
      batch.tileStream.commands.push_back(invalid);
    }
    return batch;
  };

  RenderBatch clean = build_batch_with_tile_stream(false);
  RenderBatch malformed = build_batch_with_tile_stream(true);

  std::vector<uint8_t> scratch(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(scratch), width, height, width * 4};
  OptimizedBatch prepared;
  OptimizeRenderBatch(target, malformed, prepared);
  REQUIRE_MESSAGE(prepared.valid, "permissive malformed tile stream remains renderable");
  REQUIRE_MESSAGE(prepared.useTileStream, "tile stream path remains active");

  auto cleanFirst = render_scene(clean, width, height);
  auto malformedFirst = render_scene(malformed, width, height);
  auto malformedSecond = render_scene(malformed, width, height);

  CHECK_MESSAGE(buffers_equal(cleanFirst, malformedFirst),
                "bad tile command is skipped and output matches clean stream");
  CHECK_MESSAGE(buffers_equal(malformedFirst, malformedSecond),
                "malformed tile stream output is deterministic across reruns");
}

TEST_SUITE_END();
