#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.command_structs");

TEST_CASE("render_command_defaults") {
  RenderCommand cmd;
  CHECK_MESSAGE(cmd.type == CommandType::Rect, "default type is rect");
  CHECK_MESSAGE(cmd.index == 0, "default index zero");
}

TEST_CASE("tile_command_defaults") {
  TileCommand cmd;
  CHECK_MESSAGE(cmd.type == CommandType::Rect, "default tile command type");
  CHECK_MESSAGE(cmd.index == 0, "default tile command index");
  CHECK_MESSAGE(cmd.order == 0, "default tile command order");
  CHECK_MESSAGE(cmd.wMinus1 == 0, "default tile command width");
}

TEST_CASE("skipped_command_reason_name_formatter") {
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::InvalidTileReference) == std::string_view("InvalidTileReference"),
                "invalid tile reference name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::MissingAnalyzedCommand) == std::string_view("MissingAnalyzedCommand"),
                "missing analyzed command name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::InactiveAnalyzedCommand) == std::string_view("InactiveAnalyzedCommand"),
                "inactive analyzed command name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::InvalidLocalBounds) == std::string_view("InvalidLocalBounds"),
                "invalid local bounds name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::InvalidCommandData) == std::string_view("InvalidCommandData"),
                "invalid command data name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::UnsupportedCommandType) == std::string_view("UnsupportedCommandType"),
                "unsupported command type name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::OptimizerInvalidCommandData) == std::string_view("OptimizerInvalidCommandData"),
                "optimizer invalid command data name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::OptimizerCulledByBounds) == std::string_view("OptimizerCulledByBounds"),
                "optimizer culled by bounds name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::OptimizerCulledByAlpha) == std::string_view("OptimizerCulledByAlpha"),
                "optimizer culled by alpha name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::OptimizerTileStreamInvalidCommandData) ==
                  std::string_view("OptimizerTileStreamInvalidCommandData"),
                "optimizer tile stream invalid data name");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReason::OptimizerTileStreamCulledByLocalBounds) ==
                  std::string_view("OptimizerTileStreamCulledByLocalBounds"),
                "optimizer tile stream local bounds name");

  CHECK_MESSAGE(skippedCommandReasonName(static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamInvalidCommandData)) ==
                  std::string_view("OptimizerTileStreamInvalidCommandData"),
                "index formatter resolves known reason");
  CHECK_MESSAGE(skippedCommandReasonName(static_cast<SkippedCommandReason>(255)) == std::string_view("UnknownSkippedCommandReason"),
                "unknown reason fallback");
  CHECK_MESSAGE(skippedCommandReasonName(SkippedCommandReasonCount + 1) == std::string_view("OutOfRangeSkippedCommandReason"),
                "out-of-range index fallback");
}

TEST_SUITE_END();
