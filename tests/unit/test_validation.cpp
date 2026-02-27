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

TEST_SUITE_END();
