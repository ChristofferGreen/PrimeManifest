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

TEST_CASE("command_type_name_formatter") {
  CHECK_MESSAGE(commandTypeName(CommandType::Clear) == std::string_view("Clear"), "clear name");
  CHECK_MESSAGE(commandTypeName(CommandType::Rect) == std::string_view("Rect"), "rect name");
  CHECK_MESSAGE(commandTypeName(CommandType::Text) == std::string_view("Text"), "text name");
  CHECK_MESSAGE(commandTypeName(CommandType::DebugTiles) == std::string_view("DebugTiles"), "debug tiles name");
  CHECK_MESSAGE(commandTypeName(CommandType::ClearPattern) == std::string_view("ClearPattern"), "clear pattern name");
  CHECK_MESSAGE(commandTypeName(CommandType::Circle) == std::string_view("Circle"), "circle name");
  CHECK_MESSAGE(commandTypeName(CommandType::SetPixel) == std::string_view("SetPixel"), "setpixel name");
  CHECK_MESSAGE(commandTypeName(CommandType::SetPixelA) == std::string_view("SetPixelA"), "setpixela name");
  CHECK_MESSAGE(commandTypeName(CommandType::Line) == std::string_view("Line"), "line name");
  CHECK_MESSAGE(commandTypeName(CommandType::Image) == std::string_view("Image"), "image name");
  CHECK_MESSAGE(commandTypeName(static_cast<CommandType>(255)) == std::string_view("UnknownCommandType"),
                "unknown command type fallback");
  CHECK_MESSAGE(commandTypeName(RendererProfileCommandTypeBuckets + 1) == std::string_view("OutOfRangeCommandType"),
                "out-of-range command type fallback");
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

TEST_CASE("renderer_profile_skip_diagnostics_dump_none") {
  RendererProfile profile;
  CHECK_MESSAGE(rendererProfileSkipDiagnosticsDump(profile) == std::string("skip diagnostics: none"),
                "empty profile emits none summary");
}

TEST_CASE("renderer_profile_skip_diagnostics_dump_nonzero_buckets") {
  RendererProfile profile;
  profile.optimizerSkippedCommands.total = 3;
  profile.optimizerSkippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::OptimizerCulledByAlpha)] = 1;
  profile.optimizerSkippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamInvalidCommandData)] = 2;
  profile.skippedCommands.total = 3;
  profile.skippedCommands.unknownType = 2;
  profile.skippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::InvalidCommandData)] = 1;

  auto dump = rendererProfileSkipDiagnosticsDump(profile);
  CHECK_MESSAGE(dump ==
                  std::string("optimizerSkippedCommands(total=3): OptimizerCulledByAlpha=1, OptimizerTileStreamInvalidCommandData=2\n")
                    + "skippedCommands(total=3, unknownType=2): InvalidCommandData=1",
                "dump includes non-zero reason buckets with labels");
  CHECK_MESSAGE(dump.find("OptimizerInvalidCommandData=0") == std::string::npos,
                "zero buckets omitted from dump");
}

TEST_CASE("renderer_profile_skip_diagnostics_dump_verbose_none") {
  RendererProfile profile;
  CHECK_MESSAGE(rendererProfileSkipDiagnosticsDumpVerbose(profile) == std::string("skip diagnostics: none"),
                "empty profile emits none summary");
}

TEST_CASE("renderer_profile_skip_diagnostics_dump_verbose_nonzero_buckets") {
  RendererProfile profile;
  profile.optimizerSkippedCommands.total = 3;
  profile.optimizerSkippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::OptimizerCulledByAlpha)] = 1;
  profile.optimizerSkippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamInvalidCommandData)] = 2;
  profile.optimizerSkippedCommands.byType[static_cast<size_t>(CommandType::Rect)] = 2;
  profile.optimizerSkippedCommands.byType[static_cast<size_t>(CommandType::Text)] = 1;
  profile.optimizerSkippedCommands.byTypeAndReason[static_cast<size_t>(CommandType::Rect)]
                                                 [static_cast<size_t>(SkippedCommandReason::OptimizerCulledByAlpha)] = 1;
  profile.optimizerSkippedCommands.byTypeAndReason[static_cast<size_t>(CommandType::Text)]
                                                 [static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamInvalidCommandData)] = 2;

  profile.skippedCommands.total = 3;
  profile.skippedCommands.unknownType = 2;
  profile.skippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::InvalidCommandData)] = 1;
  profile.skippedCommands.byType[static_cast<size_t>(CommandType::Image)] = 1;
  profile.skippedCommands.byTypeAndReason[static_cast<size_t>(CommandType::Image)]
                                         [static_cast<size_t>(SkippedCommandReason::InvalidCommandData)] = 1;

  auto dump = rendererProfileSkipDiagnosticsDumpVerbose(profile);
  CHECK_MESSAGE(dump ==
                  std::string("optimizerSkippedCommands(total=3): OptimizerCulledByAlpha=1, OptimizerTileStreamInvalidCommandData=2\n")
                    + "skippedCommands(total=3, unknownType=2): InvalidCommandData=1\n"
                    + "optimizerSkippedCommands.byType: Rect=2, Text=1\n"
                    + "skippedCommands.byType: Image=1\n"
                    + "optimizerSkippedCommands.byTypeAndReason: Rect/OptimizerCulledByAlpha=1, "
                      "Text/OptimizerTileStreamInvalidCommandData=2\n"
                    + "skippedCommands.byTypeAndReason: Image/InvalidCommandData=1",
                "verbose dump includes non-zero by-type and matrix buckets");
  CHECK_MESSAGE(dump.find("Clear=0") == std::string::npos, "verbose dump omits zero by-type buckets");
  CHECK_MESSAGE(dump.find("Rect/OptimizerInvalidCommandData=0") == std::string::npos,
                "verbose dump omits zero matrix buckets");
}

TEST_SUITE_END();
