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

  CommandType parsedType{};
  CHECK_MESSAGE(commandTypeFromName("Image", parsedType), "command type parse success");
  CHECK_MESSAGE(parsedType == CommandType::Image, "command type parse value");
  CHECK_MESSAGE(!commandTypeFromName("NotAType", parsedType), "command type parse rejects unknown");
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

  SkippedCommandReason parsedReason{};
  CHECK_MESSAGE(skippedCommandReasonFromName("OptimizerCulledByAlpha", parsedReason), "reason parse success");
  CHECK_MESSAGE(parsedReason == SkippedCommandReason::OptimizerCulledByAlpha, "reason parse value");
  CHECK_MESSAGE(!skippedCommandReasonFromName("NotAReason", parsedReason), "reason parse rejects unknown");
}

TEST_CASE("renderer_profile_skip_diagnostics_dump_none") {
  RendererProfile profile;
  CHECK_MESSAGE(rendererProfileSkipDiagnosticsDump(profile) == std::string("skip diagnostics: none"),
                "empty profile emits none summary");
  CHECK_MESSAGE(rendererProfileSkipDiagnosticsDump(profile, SkipDiagnosticsDumpFormat::KeyValue) == std::string("skip_diagnostics=none"),
                "empty profile emits none key-value summary");
}

TEST_CASE("renderer_profile_skip_diagnostics_dump_nonzero_buckets") {
  RendererProfile profile;
  profile.optimizerSkippedCommands.total = 3;
  profile.optimizerSkippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::OptimizerCulledByAlpha)] = 1;
  profile.optimizerSkippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamInvalidCommandData)] = 2;
  profile.skippedCommands.total = 3;
  profile.skippedCommands.unknownType = 2;
  profile.skippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::InvalidCommandData)] = 3;

  auto dump = rendererProfileSkipDiagnosticsDump(profile);
  CHECK_MESSAGE(dump ==
                  std::string("optimizerSkippedCommands(total=3): OptimizerCulledByAlpha=1, OptimizerTileStreamInvalidCommandData=2\n")
                    + "skippedCommands(total=3, unknownType=2): InvalidCommandData=3",
                "dump includes non-zero reason buckets with labels");
  CHECK_MESSAGE(dump.find("OptimizerInvalidCommandData=0") == std::string::npos,
                "zero buckets omitted from dump");

  auto keyValueDump = rendererProfileSkipDiagnosticsDump(profile, SkipDiagnosticsDumpFormat::KeyValue);
  CHECK_MESSAGE(keyValueDump ==
                  std::string("optimizerSkippedCommands.total=3;")
                    + "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=1;"
                    + "optimizerSkippedCommands.reason.OptimizerTileStreamInvalidCommandData=2;"
                    + "skippedCommands.total=3;"
                    + "skippedCommands.unknownType=2;"
                    + "skippedCommands.reason.InvalidCommandData=3",
                "compact key-value dump includes totals and non-zero reasons");
  CHECK_MESSAGE(keyValueDump.find('\n') == std::string::npos, "compact key-value dump is single-line");

  SkippedCommandDiagnostics parsedOptimizer;
  SkippedCommandDiagnostics parsedSkipped;
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(keyValueDump, parsedOptimizer, parsedSkipped),
                "compact key-value parse succeeds");
  CHECK_MESSAGE(parsedOptimizer.total == 3, "parsed compact optimizer total");
  CHECK_MESSAGE(parsedOptimizer.byReason[static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamInvalidCommandData)] == 2,
                "parsed compact optimizer reason");
  CHECK_MESSAGE(parsedSkipped.total == 3, "parsed compact renderer total");
  CHECK_MESSAGE(parsedSkipped.unknownType == 2, "parsed compact renderer unknown type");
  CHECK_MESSAGE(parsedSkipped.byReason[static_cast<size_t>(SkippedCommandReason::InvalidCommandData)] == 3,
                "parsed compact renderer reason");
}

TEST_CASE("renderer_profile_skip_diagnostics_dump_verbose_none") {
  RendererProfile profile;
  CHECK_MESSAGE(rendererProfileSkipDiagnosticsDumpVerbose(profile) == std::string("skip diagnostics: none"),
                "empty profile emits none summary");
  CHECK_MESSAGE(rendererProfileSkipDiagnosticsDumpVerbose(profile, SkipDiagnosticsDumpFormat::KeyValue) == std::string("skip_diagnostics=none"),
                "empty profile emits none key-value summary");
}

TEST_CASE("renderer_profile_skip_diagnostics_dump_verbose_nonzero_buckets") {
  RendererProfile profile;
  profile.optimizerSkippedCommands.total = 3;
  profile.optimizerSkippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::OptimizerCulledByAlpha)] = 1;
  profile.optimizerSkippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamInvalidCommandData)] = 2;
  profile.optimizerSkippedCommands.byType[static_cast<size_t>(CommandType::Rect)] = 1;
  profile.optimizerSkippedCommands.byType[static_cast<size_t>(CommandType::Text)] = 2;
  profile.optimizerSkippedCommands.byTypeAndReason[static_cast<size_t>(CommandType::Rect)]
                                                 [static_cast<size_t>(SkippedCommandReason::OptimizerCulledByAlpha)] = 1;
  profile.optimizerSkippedCommands.byTypeAndReason[static_cast<size_t>(CommandType::Text)]
                                                 [static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamInvalidCommandData)] = 2;

  profile.skippedCommands.total = 3;
  profile.skippedCommands.unknownType = 2;
  profile.skippedCommands.byReason[static_cast<size_t>(SkippedCommandReason::InvalidCommandData)] = 3;
  profile.skippedCommands.byType[static_cast<size_t>(CommandType::Image)] = 1;
  profile.skippedCommands.byTypeAndReason[static_cast<size_t>(CommandType::Image)]
                                         [static_cast<size_t>(SkippedCommandReason::InvalidCommandData)] = 1;

  auto dump = rendererProfileSkipDiagnosticsDumpVerbose(profile);
  CHECK_MESSAGE(dump ==
                  std::string("optimizerSkippedCommands(total=3): OptimizerCulledByAlpha=1, OptimizerTileStreamInvalidCommandData=2\n")
                    + "skippedCommands(total=3, unknownType=2): InvalidCommandData=3\n"
                    + "optimizerSkippedCommands.byType: Rect=1, Text=2\n"
                    + "skippedCommands.byType: Image=1\n"
                    + "optimizerSkippedCommands.byTypeAndReason: Rect/OptimizerCulledByAlpha=1, "
                      "Text/OptimizerTileStreamInvalidCommandData=2\n"
                    + "skippedCommands.byTypeAndReason: Image/InvalidCommandData=1",
                "verbose dump includes non-zero by-type and matrix buckets");
  CHECK_MESSAGE(dump.find("Clear=0") == std::string::npos, "verbose dump omits zero by-type buckets");
  CHECK_MESSAGE(dump.find("Rect/OptimizerInvalidCommandData=0") == std::string::npos,
                "verbose dump omits zero matrix buckets");

  auto keyValueDump = rendererProfileSkipDiagnosticsDumpVerbose(profile, SkipDiagnosticsDumpFormat::KeyValue);
  CHECK_MESSAGE(keyValueDump ==
                  std::string("optimizerSkippedCommands.total=3;")
                    + "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=1;"
                    + "optimizerSkippedCommands.reason.OptimizerTileStreamInvalidCommandData=2;"
                    + "skippedCommands.total=3;"
                    + "skippedCommands.unknownType=2;"
                    + "skippedCommands.reason.InvalidCommandData=3;"
                    + "optimizerSkippedCommands.type.Rect=1;"
                    + "optimizerSkippedCommands.type.Text=2;"
                    + "skippedCommands.type.Image=1;"
                    + "optimizerSkippedCommands.typeReason.Rect.OptimizerCulledByAlpha=1;"
                    + "optimizerSkippedCommands.typeReason.Text.OptimizerTileStreamInvalidCommandData=2;"
                    + "skippedCommands.typeReason.Image.InvalidCommandData=1",
                "verbose key-value dump includes non-zero by-type and matrix buckets");
  CHECK_MESSAGE(keyValueDump.find('\n') == std::string::npos, "verbose key-value dump is single-line");

  RendererProfile parsedProfile;
  SkipDiagnosticsParseError parseError{99, SkipDiagnosticsParseErrorReason::UnknownKey};
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(keyValueDump, parsedProfile, &parseError),
                "verbose key-value parse succeeds");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "parse error output cleared on success");
  CHECK_MESSAGE(parsedProfile.optimizerSkippedCommands.total == 3, "parsed verbose optimizer total");
  CHECK_MESSAGE(parsedProfile.optimizerSkippedCommands.byType[static_cast<size_t>(CommandType::Rect)] == 1,
                "parsed verbose optimizer by-type");
  CHECK_MESSAGE(parsedProfile.optimizerSkippedCommands.byTypeAndReason[static_cast<size_t>(CommandType::Text)]
                                                     [static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamInvalidCommandData)] == 2,
                "parsed verbose optimizer matrix");
  CHECK_MESSAGE(parsedProfile.skippedCommands.byType[static_cast<size_t>(CommandType::Image)] == 1,
                "parsed verbose renderer by-type");
  CHECK_MESSAGE(parsedProfile.skippedCommands.byTypeAndReason[static_cast<size_t>(CommandType::Image)]
                                                    [static_cast<size_t>(SkippedCommandReason::InvalidCommandData)] == 1,
                "parsed verbose renderer matrix");

  SkipDiagnosticsParseOptions strictOptions;
  strictOptions.strictConsistency = true;
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(keyValueDump, parsedProfile, strictOptions, &parseError),
                "strict parse accepts consistent payload");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "strict parse keeps no-error state on success");

  strictOptions.strictMatrixMarginals = true;
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(keyValueDump, parsedProfile, strictOptions, &parseError),
                "strict matrix-marginal parse accepts compatible payload");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "strict matrix-marginal parse keeps no-error state on success");
}

TEST_CASE("renderer_profile_skip_diagnostics_key_value_parse_invalid") {
  RendererProfile profile;
  SkipDiagnosticsParseError parseError;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue("optimizerSkippedCommands.reason.NotAReason=1", profile, &parseError),
                "parse rejects unknown reason name");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "unknown reason index reported");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "unknown reason error kind reported");

  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue("skip_diagnostics=none;extra=1", profile, &parseError),
                "parse rejects malformed none payload");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "malformed none index reported");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::MalformedNonePayload,
                "malformed none error kind reported");

  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(
                  "optimizerSkippedCommands.total=1;skippedCommands.type.NotAType=4",
                  profile,
                  &parseError),
                "parse rejects unknown type name");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "unknown type field index reported");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownTypeName,
                "unknown type error kind reported");

  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue("optimizerSkippedCommands.total=abc", profile, &parseError),
                "parse rejects invalid numeric value");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "invalid value field index reported");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InvalidValue,
                "invalid value error kind reported");

  std::string inconsistentPayload =
    "optimizerSkippedCommands.total=9;"
    "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.reason.OptimizerTileStreamInvalidCommandData=2;"
    "skippedCommands.total=3;"
    "skippedCommands.unknownType=2;"
    "skippedCommands.reason.InvalidCommandData=1;"
    "optimizerSkippedCommands.type.Rect=2;"
    "optimizerSkippedCommands.type.Text=1;"
    "skippedCommands.type.Image=1;"
    "optimizerSkippedCommands.typeReason.Rect.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.typeReason.Text.OptimizerTileStreamInvalidCommandData=2;"
    "skippedCommands.typeReason.Image.InvalidCommandData=1";

  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(inconsistentPayload, profile),
                "default parse remains permissive for inconsistent totals");

  SkipDiagnosticsParseOptions strictOptions;
  strictOptions.strictConsistency = true;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(inconsistentPayload, profile, strictOptions, &parseError),
                "strict parse rejects inconsistent totals");
  CHECK_MESSAGE(parseError.fieldIndex == 12, "strict consistency reports post-parse index");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentReasonTotal,
                "strict consistency reason reported");

  std::string optimizerOnlyMismatchPayload =
    "optimizerSkippedCommands.total=9;"
    "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.reason.OptimizerTileStreamInvalidCommandData=2;"
    "optimizerSkippedCommands.type.Rect=1;"
    "optimizerSkippedCommands.type.Text=2;"
    "optimizerSkippedCommands.typeReason.Rect.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.typeReason.Text.OptimizerTileStreamInvalidCommandData=2;"
    "skippedCommands.total=3;"
    "skippedCommands.unknownType=2;"
    "skippedCommands.reason.InvalidCommandData=3;"
    "skippedCommands.type.Image=1;"
    "skippedCommands.typeReason.Image.InvalidCommandData=1";

  strictOptions.strictSectionTarget = SkipDiagnosticsParseSectionTarget::RendererOnly;
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(optimizerOnlyMismatchPayload, profile, strictOptions),
                "renderer-only strict checks ignore optimizer inconsistency");
  strictOptions.strictSectionTarget = SkipDiagnosticsParseSectionTarget::OptimizerOnly;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(optimizerOnlyMismatchPayload, profile, strictOptions, &parseError),
                "optimizer-only strict checks catch optimizer inconsistency");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentReasonTotal,
                "optimizer-only strict reason reported");

  std::string rendererOnlyMismatchPayload =
    "optimizerSkippedCommands.total=3;"
    "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.reason.OptimizerTileStreamInvalidCommandData=2;"
    "optimizerSkippedCommands.type.Rect=1;"
    "optimizerSkippedCommands.type.Text=2;"
    "optimizerSkippedCommands.typeReason.Rect.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.typeReason.Text.OptimizerTileStreamInvalidCommandData=2;"
    "skippedCommands.total=9;"
    "skippedCommands.unknownType=2;"
    "skippedCommands.reason.InvalidCommandData=3;"
    "skippedCommands.type.Image=1;"
    "skippedCommands.typeReason.Image.InvalidCommandData=1";

  strictOptions.strictSectionTarget = SkipDiagnosticsParseSectionTarget::OptimizerOnly;
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(rendererOnlyMismatchPayload, profile, strictOptions),
                "optimizer-only strict checks ignore renderer inconsistency");
  strictOptions.strictSectionTarget = SkipDiagnosticsParseSectionTarget::RendererOnly;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(rendererOnlyMismatchPayload, profile, strictOptions, &parseError),
                "renderer-only strict checks catch renderer inconsistency");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentReasonTotal,
                "renderer-only strict reason reported");

  std::string rowMismatchPayload =
    "optimizerSkippedCommands.total=3;"
    "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.reason.OptimizerTileStreamInvalidCommandData=2;"
    "optimizerSkippedCommands.type.Rect=2;"
    "optimizerSkippedCommands.type.Text=1;"
    "optimizerSkippedCommands.typeReason.Rect.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.typeReason.Text.OptimizerTileStreamInvalidCommandData=2";

  strictOptions.strictSectionTarget = SkipDiagnosticsParseSectionTarget::Both;
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(rowMismatchPayload, profile, strictOptions),
                "strict consistency accepts row-mismatch payload");
  strictOptions.strictMatrixMarginals = true;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(rowMismatchPayload, profile, strictOptions, &parseError),
                "strict matrix-marginals reject row mismatches");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals,
                "row mismatch reason reported");
  CHECK_MESSAGE(parseError.fieldIndex == 8, "row mismatch field index reported");
  strictOptions.strictSectionTarget = SkipDiagnosticsParseSectionTarget::RendererOnly;
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(rowMismatchPayload, profile, strictOptions),
                "renderer-only matrix checks ignore optimizer row mismatch");

  std::string columnMismatchPayload =
    "optimizerSkippedCommands.total=3;"
    "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=3;"
    "optimizerSkippedCommands.type.Rect=1;"
    "optimizerSkippedCommands.type.Text=2;"
    "optimizerSkippedCommands.typeReason.Rect.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.typeReason.Text.OptimizerTileStreamInvalidCommandData=2";

  strictOptions.strictSectionTarget = SkipDiagnosticsParseSectionTarget::Both;
  strictOptions.strictMatrixMarginals = false;
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(columnMismatchPayload, profile, strictOptions),
                "strict consistency accepts column-mismatch payload");
  strictOptions.strictMatrixMarginals = true;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(columnMismatchPayload, profile, strictOptions, &parseError),
                "strict matrix-marginals reject column mismatches");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentMatrixColumnTotals,
                "column mismatch reason reported");
  CHECK_MESSAGE(parseError.fieldIndex == 25, "column mismatch field index reported");

  std::string rendererColumnMismatchPayload =
    "optimizerSkippedCommands.total=3;"
    "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.reason.OptimizerTileStreamInvalidCommandData=2;"
    "optimizerSkippedCommands.type.Rect=1;"
    "optimizerSkippedCommands.type.Text=2;"
    "optimizerSkippedCommands.typeReason.Rect.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.typeReason.Text.OptimizerTileStreamInvalidCommandData=2;"
    "skippedCommands.total=3;"
    "skippedCommands.unknownType=2;"
    "skippedCommands.reason.InvalidCommandData=3;"
    "skippedCommands.type.Image=1;"
    "skippedCommands.typeReason.Image.OptimizerCulledByAlpha=1";

  strictOptions.strictSectionTarget = SkipDiagnosticsParseSectionTarget::OptimizerOnly;
  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue(rendererColumnMismatchPayload, profile, strictOptions),
                "optimizer-only matrix checks ignore renderer column mismatch");
  strictOptions.strictSectionTarget = SkipDiagnosticsParseSectionTarget::RendererOnly;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(rendererColumnMismatchPayload, profile, strictOptions, &parseError),
                "renderer-only matrix checks catch renderer column mismatch");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentMatrixColumnTotals,
                "renderer-only matrix reason reported");
}

TEST_CASE("renderer_profile_skip_diagnostics_strict_failure_precedence") {
  RendererProfile profile;
  SkipDiagnosticsParseError parseError;
  std::string combinedMismatchPayload =
    "optimizerSkippedCommands.total=9;"
    "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.reason.OptimizerTileStreamInvalidCommandData=2;"
    "optimizerSkippedCommands.type.Rect=2;"
    "optimizerSkippedCommands.type.Text=1;"
    "optimizerSkippedCommands.typeReason.Rect.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.typeReason.Text.OptimizerTileStreamInvalidCommandData=2";

  SkipDiagnosticsParseOptions strictOptions;
  strictOptions.strictConsistency = true;
  strictOptions.strictMatrixMarginals = true;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(combinedMismatchPayload, profile, strictOptions, &parseError),
                "default strict precedence rejects consistency violation first");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentReasonTotal,
                "default precedence reports consistency failure");
  CHECK_MESSAGE(parseError.fieldIndex == 7, "default precedence field index matches consistency check");

  strictOptions.strictFailurePrecedence = SkipDiagnosticsStrictFailurePrecedence::MatrixMarginalsFirst;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(combinedMismatchPayload, profile, strictOptions, &parseError),
                "matrix-first precedence rejects matrix violation first");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals,
                "matrix-first precedence reports matrix row failure");
  CHECK_MESSAGE(parseError.fieldIndex == 8, "matrix-first precedence field index matches row check");
}

TEST_CASE("renderer_profile_skip_diagnostics_collect_all_strict_violations") {
  RendererProfile profile;
  SkipDiagnosticsParseError parseError;

  std::string combinedStrictMismatchPayload =
    "optimizerSkippedCommands.total=5;"
    "optimizerSkippedCommands.reason.OptimizerCulledByAlpha=1;"
    "optimizerSkippedCommands.type.Rect=2;"
    "optimizerSkippedCommands.typeReason.Rect.OptimizerCulledByAlpha=1;"
    "skippedCommands.total=4;"
    "skippedCommands.reason.InvalidCommandData=2;"
    "skippedCommands.type.Image=3;"
    "skippedCommands.typeReason.Image.InvalidCommandData=1";

  SkipDiagnosticsParseOptions strictOptions;
  strictOptions.strictConsistency = true;
  strictOptions.strictMatrixMarginals = true;
  strictOptions.strictFailureMode = SkipDiagnosticsStrictFailureMode::CollectAll;

  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(combinedStrictMismatchPayload, profile, strictOptions, &parseError),
                "collect-all strict mode rejects payload with violations");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentReasonTotal,
                "collect-all keeps first consistency reason");
  CHECK_MESSAGE(parseError.fieldIndex == 8, "collect-all keeps first consistency field index");
  CHECK_MESSAGE(parseError.strictViolations.size() == 9, "collect-all returns every strict violation");

  size_t reasonTotalViolations = 0;
  size_t typeTotalViolations = 0;
  size_t matrixTotalViolations = 0;
  size_t rowMarginalViolations = 0;
  size_t columnMarginalViolations = 0;
  bool foundRendererUnknownColumnMismatch = false;
  for (auto const& violation : parseError.strictViolations) {
    if (violation.reason == SkipDiagnosticsParseErrorReason::InconsistentReasonTotal) {
      reasonTotalViolations += 1;
    } else if (violation.reason == SkipDiagnosticsParseErrorReason::InconsistentTypeTotal) {
      typeTotalViolations += 1;
    } else if (violation.reason == SkipDiagnosticsParseErrorReason::InconsistentMatrixTotal) {
      matrixTotalViolations += 1;
    } else if (violation.reason == SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals) {
      rowMarginalViolations += 1;
    } else if (violation.reason == SkipDiagnosticsParseErrorReason::InconsistentMatrixColumnTotals) {
      columnMarginalViolations += 1;
      if (violation.fieldIndex == 51) {
        foundRendererUnknownColumnMismatch = true;
      }
    }
  }
  CHECK_MESSAGE(reasonTotalViolations == 2, "collect-all includes consistency reason-total violations for both sections");
  CHECK_MESSAGE(typeTotalViolations == 2, "collect-all includes consistency type-total violations for both sections");
  CHECK_MESSAGE(matrixTotalViolations == 2, "collect-all includes consistency matrix-total violations for both sections");
  CHECK_MESSAGE(rowMarginalViolations == 2, "collect-all includes row-marginal violations for both sections");
  CHECK_MESSAGE(columnMarginalViolations == 1, "collect-all includes renderer column-marginal violation");
  CHECK_MESSAGE(foundRendererUnknownColumnMismatch, "collect-all reports renderer unknown-type column mismatch index");

  strictOptions.strictFailurePrecedence = SkipDiagnosticsStrictFailurePrecedence::MatrixMarginalsFirst;
  CHECK_MESSAGE(!parseRendererProfileSkipDiagnosticsKeyValue(combinedStrictMismatchPayload, profile, strictOptions, &parseError),
                "collect-all still rejects under matrix-first precedence");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals,
                "matrix-first collect-all records matrix violation as primary");
  CHECK_MESSAGE(parseError.fieldIndex == 9, "matrix-first collect-all reports optimizer row index first");
  CHECK_MESSAGE(parseError.strictViolations.size() == 9, "matrix-first collect-all keeps full violation list");

  CHECK_MESSAGE(parseRendererProfileSkipDiagnosticsKeyValue("skip_diagnostics=none", profile, strictOptions, &parseError),
                "collect-all accepts valid payload");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "collect-all clears reason on success");
  CHECK_MESSAGE(parseError.strictViolations.empty(),
                "collect-all clears strict violation list on success");
}

TEST_CASE("skip_diagnostics_strict_violations_dump_formatter") {
  SkipDiagnosticsParseError parseError;
  CHECK_MESSAGE(skipDiagnosticsParseStrictViolationsDump(parseError) == std::string("strict violations: none"),
                "readable strict-violations dump emits none summary");
  CHECK_MESSAGE(skipDiagnosticsParseStrictViolationsDump(parseError, SkipDiagnosticsDumpFormat::KeyValue) ==
                  std::string("strict_violations=none"),
                "key-value strict-violations dump emits none summary");

  parseError.strictViolations.push_back({3, SkipDiagnosticsParseErrorReason::InconsistentReasonTotal});
  parseError.strictViolations.push_back({11, SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals});
  parseError.strictViolations.push_back({13, static_cast<SkipDiagnosticsParseErrorReason>(255)});

  CHECK_MESSAGE(skipDiagnosticsParseStrictViolationsDump(parseError) ==
                  std::string("strict violations(count=3): "
                              "field[3]=InconsistentReasonTotal, "
                              "field[11]=InconsistentMatrixRowTotals, "
                              "field[13]=UnknownParseErrorReason"),
                "readable strict-violations dump lists field/reason entries in order");
  CHECK_MESSAGE(skipDiagnosticsParseStrictViolationsDump(parseError, SkipDiagnosticsDumpFormat::KeyValue) ==
                  std::string("strictViolations.count=3;")
                    + "strictViolations.0.fieldIndex=3;"
                    + "strictViolations.0.reason=InconsistentReasonTotal;"
                    + "strictViolations.1.fieldIndex=11;"
                    + "strictViolations.1.reason=InconsistentMatrixRowTotals;"
                    + "strictViolations.2.fieldIndex=13;"
                    + "strictViolations.2.reason=UnknownParseErrorReason",
                "key-value strict-violations dump lists indexed field/reason pairs");
}

TEST_CASE("skip_diagnostics_parse_error_reason_name_formatter") {
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::None) == std::string_view("None"),
                "none parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::MalformedTypeReasonKey) ==
                  std::string_view("MalformedTypeReasonKey"),
                "malformed type-reason parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::InconsistentReasonTotal) ==
                  std::string_view("InconsistentReasonTotal"),
                "inconsistent reason-total parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::InconsistentTypeTotal) ==
                  std::string_view("InconsistentTypeTotal"),
                "inconsistent type-total parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::InconsistentMatrixTotal) ==
                  std::string_view("InconsistentMatrixTotal"),
                "inconsistent matrix-total parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals) ==
                  std::string_view("InconsistentMatrixRowTotals"),
                "inconsistent matrix-row parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::InconsistentMatrixColumnTotals) ==
                  std::string_view("InconsistentMatrixColumnTotals"),
                "inconsistent matrix-column parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(static_cast<SkipDiagnosticsParseErrorReason>(255)) ==
                  std::string_view("UnknownParseErrorReason"),
                "unknown parse error name fallback");
}

TEST_SUITE_END();
