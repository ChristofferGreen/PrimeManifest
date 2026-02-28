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

TEST_CASE("skip_diagnostics_strict_violations_key_value_parse") {
  SkipDiagnosticsParseError dumpSource;
  dumpSource.strictViolations.push_back({3, SkipDiagnosticsParseErrorReason::InconsistentReasonTotal});
  dumpSource.strictViolations.push_back({11, SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals});
  dumpSource.strictViolations.push_back({13, static_cast<SkipDiagnosticsParseErrorReason>(255)});

  std::vector<SkipDiagnosticsParseError::StrictViolation> parsedViolations;
  SkipDiagnosticsParseError parseError{99, SkipDiagnosticsParseErrorReason::UnknownKey};
  std::string keyValueDump = skipDiagnosticsParseStrictViolationsDump(dumpSource, SkipDiagnosticsDumpFormat::KeyValue);
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(keyValueDump, parsedViolations, &parseError),
                "strict-violation key-value parser accepts formatted dump");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "strict-violation parser clears parse error on success");
  CHECK_MESSAGE(parsedViolations.size() == 3, "strict-violation parser restores all entries");
  CHECK_MESSAGE(parsedViolations[0].fieldIndex == 3, "strict-violation parser restores first field index");
  CHECK_MESSAGE(parsedViolations[0].reason == SkipDiagnosticsParseErrorReason::InconsistentReasonTotal,
                "strict-violation parser restores first reason");
  CHECK_MESSAGE(parsedViolations[2].reason == static_cast<SkipDiagnosticsParseErrorReason>(255),
                "strict-violation parser preserves unknown reason fallback");

  SkipDiagnosticsStrictViolationsParseOptions strictReasonTokenOptions;
  strictReasonTokenOptions.rejectUnknownReasonFallbackToken = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(keyValueDump, parsedViolations, strictReasonTokenOptions, &parseError),
                "strict reason-token mode rejects unknown-reason fallback token");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonFallbackToken,
                "strict reason-token mode reports fallback-token reason");

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  strictReasonTokenOptions,
                  &parseError),
                "strict reason-token mode accepts known reason names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "strict reason-token mode clears parse error on valid known reason");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason= InconsistentReasonTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects reason tokens with leading whitespace as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for leading-whitespace reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions reasonWhitespaceOptions;
  reasonWhitespaceOptions.rejectReasonNameAsciiWhitespaceTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  reasonWhitespaceOptions,
                  &parseError),
                "reason-whitespace mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "reason-whitespace mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\tInconsistentReasonTotal",
                  parsedViolations,
                  reasonWhitespaceOptions,
                  &parseError),
                "reason-whitespace mode rejects reason tokens with leading whitespace");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameAsciiWhitespaceToken,
                "reason-whitespace mode reports dedicated leading-whitespace reason-token error");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "reason-whitespace mode reports reason field index for leading-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal \r",
                  parsedViolations,
                  reasonWhitespaceOptions,
                  &parseError),
                "reason-whitespace mode rejects reason tokens with trailing whitespace");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameAsciiWhitespaceToken,
                "reason-whitespace mode reports dedicated trailing-whitespace reason-token error");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "reason-whitespace mode reports reason field index for trailing-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix RowTotals",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects embedded-whitespace reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for embedded-whitespace reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions embeddedReasonWhitespaceOptions;
  embeddedReasonWhitespaceOptions.rejectReasonNameEmbeddedAsciiWhitespaceTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  embeddedReasonWhitespaceOptions,
                  &parseError),
                "embedded-reason-whitespace mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "embedded-reason-whitespace mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix \t RowTotals",
                  parsedViolations,
                  embeddedReasonWhitespaceOptions,
                  &parseError),
                "embedded-reason-whitespace mode rejects embedded-whitespace reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameEmbeddedAsciiWhitespaceToken,
                "embedded-reason-whitespace mode reports dedicated embedded-whitespace reason-token error");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "embedded-reason-whitespace mode reports reason field index for embedded-whitespace tokens");
  SkipDiagnosticsStrictViolationsParseOptions asciiWhitespacePrecedenceOptions;
  asciiWhitespacePrecedenceOptions.rejectReasonNameAsciiWhitespaceTokens = true;
  asciiWhitespacePrecedenceOptions.rejectReasonNameEmbeddedAsciiWhitespaceTokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason= InconsistentReasonTotal",
                  parsedViolations,
                  asciiWhitespacePrecedenceOptions,
                  &parseError),
                "ASCII-whitespace reason-token mode prioritizes leading/trailing whitespace diagnostics over embedded-whitespace diagnostics when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameAsciiWhitespaceToken,
                "ASCII-whitespace reason-token mode reports ASCII-whitespace reason before embedded-whitespace reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "ASCII-whitespace reason-token mode reports reason field index for leading-or-trailing-over-embedded whitespace precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal \t",
                  parsedViolations,
                  asciiWhitespacePrecedenceOptions,
                  &parseError),
                "ASCII-whitespace reason-token mode also prioritizes trailing-whitespace diagnostics over embedded-whitespace diagnostics when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameAsciiWhitespaceToken,
                "ASCII-whitespace reason-token mode reports ASCII-whitespace reason for trailing whitespace when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "ASCII-whitespace reason-token mode reports reason field index for trailing-over-embedded whitespace precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0RowTotals",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects non-ASCII-whitespace reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for non-ASCII-whitespace reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions nonAsciiReasonWhitespaceOptions;
  nonAsciiReasonWhitespaceOptions.rejectReasonNameNonAsciiWhitespaceTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "non-ASCII-reason-whitespace mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\xC2\xA0InconsistentMatrixRowTotals",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode rejects leading non-ASCII-whitespace reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiWhitespaceToken,
                "non-ASCII-reason-whitespace mode reports dedicated non-ASCII-whitespace reason-token error for leading token whitespace");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for leading non-ASCII-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrixRowTotals\xC2\xA0",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode rejects trailing non-ASCII-whitespace reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiWhitespaceToken,
                "non-ASCII-reason-whitespace mode reports dedicated non-ASCII-whitespace reason-token error for trailing token whitespace");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for trailing non-ASCII-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0RowTotals",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode rejects embedded non-ASCII-whitespace reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiWhitespaceToken,
                "non-ASCII-reason-whitespace mode reports dedicated non-ASCII-whitespace reason-token error for embedded token whitespace");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for embedded non-ASCII-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies bracketing-malformed accented tokens as unknown names when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments bracket the first accented code point and non-ASCII whitespace appears on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-bracketing-first-accented with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies bracketing-malformed emoji tokens as unknown names when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments bracket the first emoji code point and non-ASCII whitespace appears on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-bracketing-first-emoji with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC3\xA9Row\xC2\xA0Totals\xC2"
                  "Break\x80"
                  "Tail\xE3\x80\x80",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies post-first malformed accented tokens as unknown names when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when malformed segments appear only after the first accented code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for first-accented then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80Row\xE3\x80\x80Totals\xC2"
                  "Break\x80"
                  "Tail\xC2\xA0",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies post-first malformed emoji tokens as unknown names when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when malformed segments appear only after the first emoji code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for first-emoji then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC3\xA9RowTotals\xC2"
                  "Break\x80"
                  "Tail\xC2",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple non-ASCII whitespace segments appear before the first accented code point and multiple malformed segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple non-ASCII whitespace segments precede the first accented code point and multiple malformed segments occur only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-whitespace-before-first-accented then post-first-multi-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80RowTotals\xC2"
                  "Break\x80"
                  "Tail\xC2",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple non-ASCII whitespace segments appear before the first emoji code point and multiple malformed segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple non-ASCII whitespace segments precede the first emoji code point and multiple malformed segments occur only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-whitespace-before-first-emoji then post-first-multi-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies bracketing-malformed accented tokens as unknown names when multiple non-ASCII whitespace segments appear before the first accented code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple non-ASCII whitespace segments precede the first accented code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-whitespace-before-first-accented with multiple-malformed-bracketing tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies bracketing-malformed emoji tokens as unknown names when multiple non-ASCII whitespace segments appear before the first emoji code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple non-ASCII whitespace segments precede the first emoji code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-whitespace-before-first-emoji with multiple-malformed-bracketing tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies bracketing-malformed accented tokens as unknown names when multiple non-ASCII whitespace segments appear both before and after the first accented code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple non-ASCII whitespace segments surround the first accented code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-whitespace-before-and-after-first-accented with multiple-malformed-bracketing tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies bracketing-malformed emoji tokens as unknown names when multiple non-ASCII whitespace segments appear both before and after the first emoji code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple non-ASCII whitespace segments surround the first emoji code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-whitespace-before-and-after-first-emoji with multiple-malformed-bracketing tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC3\xA9RowTotals\xC2"
                  "Break\x80"
                  "Tail\xC2"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies post-first malformed accented tokens as unknown names when multiple non-ASCII whitespace segments appear both before and after the first accented code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple non-ASCII whitespace segments surround the first accented code point and multiple malformed segments appear only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-whitespace-before-and-after-first-accented then post-first-multi-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xF0\x9F\x98\x80RowTotals\xC2"
                  "Break\x80"
                  "Tail\xC2"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies post-first malformed emoji tokens as unknown names when multiple non-ASCII whitespace segments appear both before and after the first emoji code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple non-ASCII whitespace segments surround the first emoji code point and multiple malformed segments appear only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-whitespace-before-and-after-first-emoji then post-first-multi-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments appear only before the first accented code point while multiple non-ASCII whitespace segments surround that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-only-before-first-accented with multiple-whitespace-before-and-after tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments appear only before the first emoji code point while multiple non-ASCII whitespace segments surround that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-only-before-first-emoji with multiple-whitespace-before-and-after tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments appear only before the first accented code point while multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-only-before-first-accented with pre-first-only non-ASCII-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments appear only before the first emoji code point while multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-only-before-first-emoji with pre-first-only non-ASCII-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments appear only before the first accented code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-only-before-first-accented with post-first-only non-ASCII-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments appear only before the first emoji code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-only-before-first-emoji with post-first-only non-ASCII-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments bracket the first accented code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-bracketing-first-accented with post-first-only non-ASCII-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments bracket the first emoji code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-bracketing-first-emoji with post-first-only non-ASCII-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments bracket the first accented code point while multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-bracketing-first-accented with pre-first-only non-ASCII-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments bracket the first emoji code point while multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-bracketing-first-emoji with pre-first-only non-ASCII-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC2"
                  "Tail\x80"
                  "\xC3\xA9RowTotals\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments appear only before the first accented code point while multiple non-ASCII whitespace segments surround that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-only-before-first-accented with multiple-whitespace-before-and-after tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC2"
                  "Tail\x80"
                  "\xF0\x9F\x98\x80RowTotals\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonAsciiReasonWhitespaceOptions,
                  &parseError),
                "non-ASCII-reason-whitespace mode still classifies tokens as unknown names when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-ASCII-reason-whitespace mode reports unknown reason when multiple malformed segments appear only before the first emoji code point while multiple non-ASCII whitespace segments surround that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII-reason-whitespace mode reports reason field index for multiple-malformed-only-before-first-emoji with multiple-whitespace-before-and-after tokens");

  SkipDiagnosticsStrictViolationsParseOptions nonAsciiControlWhitespacePrecedenceOptions;
  nonAsciiControlWhitespacePrecedenceOptions.rejectReasonNameNonAsciiUnicodeControlCharacterTokens = true;
  nonAsciiControlWhitespacePrecedenceOptions.rejectReasonNameNonAsciiWhitespaceTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  nonAsciiControlWhitespacePrecedenceOptions,
                  &parseError),
                "combined non-ASCII-control and non-ASCII-whitespace mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "combined non-ASCII-control and non-ASCII-whitespace mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\x85RowTotals",
                  parsedViolations,
                  nonAsciiControlWhitespacePrecedenceOptions,
                  &parseError),
                "combined non-ASCII-control and non-ASCII-whitespace mode prioritizes Unicode-control diagnostics over non-ASCII-whitespace diagnostics when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiUnicodeControlCharacterToken,
                "combined non-ASCII-control and non-ASCII-whitespace mode reports Unicode-control reason before non-ASCII-whitespace reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined non-ASCII-control and non-ASCII-whitespace mode reports reason field index for overlapping C1-control/non-ASCII-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\xC2\x85InconsistentMatrixRowTotals",
                  parsedViolations,
                  nonAsciiControlWhitespacePrecedenceOptions,
                  &parseError),
                "combined non-ASCII-control and non-ASCII-whitespace mode prioritizes Unicode-control diagnostics for leading C1-control/non-ASCII-whitespace tokens when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiUnicodeControlCharacterToken,
                "combined non-ASCII-control and non-ASCII-whitespace mode reports Unicode-control reason for leading overlap tokens when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined non-ASCII-control and non-ASCII-whitespace mode reports reason field index for leading overlap tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrixRowTotals\xC2",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects malformed UTF-8 reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for malformed UTF-8 reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions malformedUtf8ReasonOptions;
  malformedUtf8ReasonOptions.rejectReasonNameMalformedUtf8Tokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "malformed-UTF-8 reason-token mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrixRowTotals\xC2",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode rejects truncated UTF-8 reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode reports dedicated malformed-UTF-8 reason-token error for truncated sequences");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for truncated UTF-8 tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\x80InconsistentMatrixRowTotals",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode rejects stray UTF-8 continuation bytes");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode reports dedicated malformed-UTF-8 reason-token error for stray continuation bytes");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for stray continuation bytes");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\xE3\x80\x80",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear before the first accented code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments precede the first accented code point and later malformed plus non-ASCII whitespace segments follow");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-before-first-accented with later-malformed-and-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\xC2\xA0",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear before the first emoji code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments precede the first emoji code point and later malformed plus non-ASCII whitespace segments follow");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-before-first-emoji with later-malformed-and-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments surround that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-only-before-first-accented with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments surround that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-only-before-first-emoji with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-only-before-first-accented with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-only-before-first-emoji with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-only-before-first-accented with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-only-before-first-emoji with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-bracketing-first-accented with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-bracketing-first-emoji with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-bracketing-first-accented with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-bracketing-first-emoji with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC3\xA9RowTotals\xC2"
                  "Break\x80Tail",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only after the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments occur only after the first accented code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-whitespace-before-first-accented with multiple post-accented malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80RowTotals\xC2"
                  "Split\x80"
                  "End",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only after the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments occur only after the first emoji code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-whitespace-before-first-emoji with multiple post-emoji malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC3\xA9Row\xC2\xA0Totals\xC2"
                  "Break\x80"
                  "Tail\xE3\x80\x80",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only after the first accented code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when malformed segments appear only after the first accented code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for first-accented then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80Row\xE3\x80\x80Totals\xC2"
                  "Break\x80"
                  "Tail\xC2\xA0",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments appear only after the first emoji code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when malformed segments appear only after the first emoji code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for first-emoji then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments bracket the first accented code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments bracket the first accented code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-bracketing-first-accented with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedUtf8ReasonOptions,
                  &parseError),
                "malformed-UTF-8 reason-token mode reports malformed diagnostics when multiple malformed segments bracket the first emoji code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "malformed-UTF-8 reason-token mode keeps malformed diagnostics when multiple malformed segments bracket the first emoji code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "malformed-UTF-8 reason-token mode reports reason field index for multiple-malformed-bracketing-first-emoji with surrounding non-ASCII whitespace segments");

  SkipDiagnosticsStrictViolationsParseOptions malformedAndNonAsciiWhitespaceReasonOptions;
  malformedAndNonAsciiWhitespaceReasonOptions.rejectReasonNameMalformedUtf8Tokens = true;
  malformedAndNonAsciiWhitespaceReasonOptions.rejectReasonNameNonAsciiWhitespaceTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0RowTotals\xC2",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode prioritizes malformed-UTF-8 diagnostics over non-ASCII-whitespace diagnostics when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 reason before non-ASCII-whitespace reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for malformed-over-non-ASCII-whitespace precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\xC2\xA0InconsistentMatrixRowTotals\xC2",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode also prioritizes malformed-UTF-8 diagnostics for leading non-ASCII-whitespace overlap tokens when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 reason for leading overlap tokens when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for leading overlap precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC3\xA9RowTotals\xC2"
                  "Break\x80"
                  "Tail\xC2",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments appear before the first accented code point and multiple malformed segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments precede the first accented code point and malformed segments occur only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-whitespace-before-first-accented then post-first-multi-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80RowTotals\xC2"
                  "Break\x80"
                  "Tail\xC2",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments appear before the first emoji code point and multiple malformed segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments precede the first emoji code point and malformed segments occur only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-whitespace-before-first-emoji then post-first-multi-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-only-before-first-accented with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-only-before-first-emoji with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-only-before-first-accented with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-only-before-first-emoji with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-bracketing-first-accented with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-bracketing-first-emoji with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-bracketing-first-accented with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-bracketing-first-emoji with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments appear before the first accented code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments precede the first accented code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-whitespace-before-first-accented with multiple-malformed-bracketing tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments appear before the first emoji code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments precede the first emoji code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-whitespace-before-first-emoji with multiple-malformed-bracketing tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC3\xA9Row\xC2\xA0Totals\xC2"
                  "Break\x80"
                  "Tail\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only after the first accented code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when malformed segments appear only after the first accented code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for first-accented then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80Row\xE3\x80\x80Totals\xC2"
                  "Break\x80"
                  "Tail\xC2\xA0",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only after the first emoji code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when malformed segments appear only after the first emoji code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for first-emoji then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC3\xA9RowTotals\xC2"
                  "Break\x80"
                  "Tail\xC2"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments appear both before and after the first accented code point and multiple malformed segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments surround the first accented code point and multiple malformed segments occur only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-whitespace-before-and-after-first-accented then post-first-multi-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xF0\x9F\x98\x80RowTotals\xC2"
                  "Break\x80"
                  "Tail\xC2"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments appear both before and after the first emoji code point and multiple malformed segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments surround the first emoji code point and multiple malformed segments occur only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-whitespace-before-and-after-first-emoji then post-first-multi-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-bracketing-first-accented with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-malformed-bracketing-first-emoji with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9RowTotals\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments appear both before and after the first accented code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments surround the first accented code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-whitespace-before-and-after-first-accented with multiple-malformed-bracketing tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentMatrix\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80RowTotals\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonAsciiWhitespaceReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments appear both before and after the first emoji code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode keeps malformed-UTF-8 diagnostics when multiple non-ASCII whitespace segments surround the first emoji code point and multiple malformed segments bracket that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII-whitespace mode reports reason field index for multiple-whitespace-before-and-after-first-emoji with multiple-malformed-bracketing tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\x01Total",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects ASCII-control reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for ASCII-control reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions asciiControlReasonOptions;
  asciiControlReasonOptions.rejectReasonNameAsciiControlCharacterTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  asciiControlReasonOptions,
                  &parseError),
                "ASCII-control reason-token mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "ASCII-control reason-token mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\x01Total",
                  parsedViolations,
                  asciiControlReasonOptions,
                  &parseError),
                "ASCII-control reason-token mode rejects embedded control-byte reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameAsciiControlCharacterToken,
                "ASCII-control reason-token mode reports dedicated ASCII-control reason-token error for embedded control bytes");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "ASCII-control reason-token mode reports reason field index for embedded control bytes");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal\x7F",
                  parsedViolations,
                  asciiControlReasonOptions,
                  &parseError),
                "ASCII-control reason-token mode rejects trailing DEL control-byte reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameAsciiControlCharacterToken,
                "ASCII-control reason-token mode reports dedicated ASCII-control reason-token error for trailing DEL bytes");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "ASCII-control reason-token mode reports reason field index for trailing DEL bytes");

  SkipDiagnosticsStrictViolationsParseOptions malformedAndAsciiControlReasonOptions;
  malformedAndAsciiControlReasonOptions.rejectReasonNameMalformedUtf8Tokens = true;
  malformedAndAsciiControlReasonOptions.rejectReasonNameAsciiControlCharacterTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  malformedAndAsciiControlReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and ASCII-control mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "combined malformed-UTF-8 and ASCII-control mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\x01Total\xC2",
                  parsedViolations,
                  malformedAndAsciiControlReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and ASCII-control mode prioritizes malformed-UTF-8 diagnostics over ASCII-control diagnostics when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and ASCII-control mode reports malformed-UTF-8 reason before ASCII-control reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and ASCII-control mode reports reason field index for malformed-over-ASCII-control precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\x80InconsistentReason\x01Total",
                  parsedViolations,
                  malformedAndAsciiControlReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and ASCII-control mode also prioritizes malformed-UTF-8 diagnostics for leading malformed bytes when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and ASCII-control mode reports malformed-UTF-8 reason for leading malformed bytes when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and ASCII-control mode reports reason field index for leading malformed-over-ASCII-control precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE2\x80\x8ETotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects non-ASCII Unicode-control reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for non-ASCII Unicode-control reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions nonAsciiUnicodeControlReasonOptions;
  nonAsciiUnicodeControlReasonOptions.rejectReasonNameNonAsciiUnicodeControlCharacterTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  nonAsciiUnicodeControlReasonOptions,
                  &parseError),
                "non-ASCII Unicode-control reason-token mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "non-ASCII Unicode-control reason-token mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE2\x80\x8ETotal",
                  parsedViolations,
                  nonAsciiUnicodeControlReasonOptions,
                  &parseError),
                "non-ASCII Unicode-control reason-token mode rejects embedded format-control reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiUnicodeControlCharacterToken,
                "non-ASCII Unicode-control reason-token mode reports dedicated Unicode-control reason-token error for embedded format controls");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII Unicode-control reason-token mode reports reason field index for embedded format controls");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal\xC2\x91",
                  parsedViolations,
                  nonAsciiUnicodeControlReasonOptions,
                  &parseError),
                "non-ASCII Unicode-control reason-token mode rejects trailing C1-control reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiUnicodeControlCharacterToken,
                "non-ASCII Unicode-control reason-token mode reports dedicated Unicode-control reason-token error for trailing C1 controls");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-ASCII Unicode-control reason-token mode reports reason field index for trailing C1 controls");

  SkipDiagnosticsStrictViolationsParseOptions malformedAndNonAsciiControlReasonOptions;
  malformedAndNonAsciiControlReasonOptions.rejectReasonNameMalformedUtf8Tokens = true;
  malformedAndNonAsciiControlReasonOptions.rejectReasonNameNonAsciiUnicodeControlCharacterTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  malformedAndNonAsciiControlReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII Unicode-control mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "combined malformed-UTF-8 and non-ASCII Unicode-control mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE2\x80\x8ETotal\xC2",
                  parsedViolations,
                  malformedAndNonAsciiControlReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII Unicode-control mode prioritizes malformed-UTF-8 diagnostics over non-ASCII Unicode-control diagnostics when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII Unicode-control mode reports malformed-UTF-8 reason before non-ASCII Unicode-control reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII Unicode-control mode reports reason field index for malformed-over-non-ASCII-Unicode-control precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\xC2\x91InconsistentReasonTotal\xC2",
                  parsedViolations,
                  malformedAndNonAsciiControlReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-ASCII Unicode-control mode also prioritizes malformed-UTF-8 diagnostics for leading C1 controls when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-ASCII Unicode-control mode reports malformed-UTF-8 reason for leading C1-control overlap tokens when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-ASCII Unicode-control mode reports reason field index for leading overlap precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xEF\xB7\x90Total",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects Unicode-noncharacter reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for Unicode-noncharacter reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions unicodeNoncharacterReasonOptions;
  unicodeNoncharacterReasonOptions.rejectReasonNameUnicodeNoncharacterTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  unicodeNoncharacterReasonOptions,
                  &parseError),
                "Unicode-noncharacter reason-token mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "Unicode-noncharacter reason-token mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xEF\xB7\x90Total",
                  parsedViolations,
                  unicodeNoncharacterReasonOptions,
                  &parseError),
                "Unicode-noncharacter reason-token mode rejects embedded U+FDD0 noncharacter tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameUnicodeNoncharacterToken,
                "Unicode-noncharacter reason-token mode reports dedicated noncharacter reason-token error for embedded U+FDD0");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "Unicode-noncharacter reason-token mode reports reason field index for embedded U+FDD0");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal\xEF\xBF\xBF",
                  parsedViolations,
                  unicodeNoncharacterReasonOptions,
                  &parseError),
                "Unicode-noncharacter reason-token mode rejects trailing U+FFFF noncharacter tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameUnicodeNoncharacterToken,
                "Unicode-noncharacter reason-token mode reports dedicated noncharacter reason-token error for trailing U+FFFF");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "Unicode-noncharacter reason-token mode reports reason field index for trailing U+FFFF");

  SkipDiagnosticsStrictViolationsParseOptions malformedAndUnicodeNoncharacterReasonOptions;
  malformedAndUnicodeNoncharacterReasonOptions.rejectReasonNameMalformedUtf8Tokens = true;
  malformedAndUnicodeNoncharacterReasonOptions.rejectReasonNameUnicodeNoncharacterTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  malformedAndUnicodeNoncharacterReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and Unicode-noncharacter mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "combined malformed-UTF-8 and Unicode-noncharacter mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xEF\xB7\x90Total\xC2",
                  parsedViolations,
                  malformedAndUnicodeNoncharacterReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and Unicode-noncharacter mode prioritizes malformed-UTF-8 diagnostics over Unicode-noncharacter diagnostics when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and Unicode-noncharacter mode reports malformed-UTF-8 reason before Unicode-noncharacter reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and Unicode-noncharacter mode reports reason field index for malformed-over-Unicode-noncharacter precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal\xEF\xBF\xBF\x80",
                  parsedViolations,
                  malformedAndUnicodeNoncharacterReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and Unicode-noncharacter mode also prioritizes malformed-UTF-8 diagnostics for trailing malformed bytes when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and Unicode-noncharacter mode reports malformed-UTF-8 reason for trailing malformed-byte overlap tokens when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and Unicode-noncharacter mode reports reason field index for trailing overlap precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\x80Total",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects lone CESU-8 surrogate reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for lone CESU-8 surrogate reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions loneCesu8SurrogateReasonOptions;
  loneCesu8SurrogateReasonOptions.rejectReasonNameLoneCesu8SurrogateTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  loneCesu8SurrogateReasonOptions,
                  &parseError),
                "lone-CESU-8-surrogate reason-token mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "lone-CESU-8-surrogate reason-token mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\x80Total",
                  parsedViolations,
                  loneCesu8SurrogateReasonOptions,
                  &parseError),
                "lone-CESU-8-surrogate reason-token mode rejects lone high-surrogate CESU-8 tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameLoneCesu8SurrogateToken,
                "lone-CESU-8-surrogate reason-token mode reports dedicated lone-surrogate reason for lone high-surrogate tokens");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "lone-CESU-8-surrogate reason-token mode reports reason field index for lone high-surrogate tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB0\x80Total",
                  parsedViolations,
                  loneCesu8SurrogateReasonOptions,
                  &parseError),
                "lone-CESU-8-surrogate reason-token mode rejects lone low-surrogate CESU-8 tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameLoneCesu8SurrogateToken,
                "lone-CESU-8-surrogate reason-token mode reports dedicated lone-surrogate reason for lone low-surrogate tokens");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "lone-CESU-8-surrogate reason-token mode reports reason field index for lone low-surrogate tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\xBD\xED\xB8\x80Total",
                  parsedViolations,
                  loneCesu8SurrogateReasonOptions,
                  &parseError),
                "lone-CESU-8-surrogate reason-token mode leaves paired CESU-8 surrogate sequences to generic reason validation");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "lone-CESU-8-surrogate reason-token mode does not classify paired surrogate sequences as lone-surrogate tokens");

  SkipDiagnosticsStrictViolationsParseOptions pairedCesu8SurrogateReasonOptions;
  pairedCesu8SurrogateReasonOptions.rejectReasonNamePairedCesu8SurrogateTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  pairedCesu8SurrogateReasonOptions,
                  &parseError),
                "paired-CESU-8-surrogate reason-token mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "paired-CESU-8-surrogate reason-token mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\xBD\xED\xB8\x80Total",
                  parsedViolations,
                  pairedCesu8SurrogateReasonOptions,
                  &parseError),
                "paired-CESU-8-surrogate reason-token mode rejects paired CESU-8 surrogate sequences");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNamePairedCesu8SurrogateToken,
                "paired-CESU-8-surrogate reason-token mode reports dedicated paired-surrogate reason");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "paired-CESU-8-surrogate reason-token mode reports reason field index for paired surrogate sequences");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\x80Total",
                  parsedViolations,
                  pairedCesu8SurrogateReasonOptions,
                  &parseError),
                "paired-CESU-8-surrogate reason-token mode leaves lone surrogate sequences to generic reason validation");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "paired-CESU-8-surrogate reason-token mode does not classify lone surrogate sequences as paired surrogates");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB8\x80\xED\xA0\xBDTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects mixed-order CESU-8 surrogate reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for mixed-order CESU-8 surrogate reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions mixedOrderCesu8SurrogateReasonOptions;
  mixedOrderCesu8SurrogateReasonOptions.rejectReasonNameMixedOrderCesu8SurrogateTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  mixedOrderCesu8SurrogateReasonOptions,
                  &parseError),
                "mixed-order-CESU-8-surrogate reason-token mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "mixed-order-CESU-8-surrogate reason-token mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB8\x80\xED\xA0\xBDTotal",
                  parsedViolations,
                  mixedOrderCesu8SurrogateReasonOptions,
                  &parseError),
                "mixed-order-CESU-8-surrogate reason-token mode rejects low-before-high CESU-8 surrogate sequences");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMixedOrderCesu8SurrogateToken,
                "mixed-order-CESU-8-surrogate reason-token mode reports dedicated mixed-order surrogate reason");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "mixed-order-CESU-8-surrogate reason-token mode reports reason field index for mixed-order surrogate sequences");

  SkipDiagnosticsStrictViolationsParseOptions mixedOrderAndLoneCesu8SurrogateReasonOptions;
  mixedOrderAndLoneCesu8SurrogateReasonOptions.rejectReasonNameMixedOrderCesu8SurrogateTokens = true;
  mixedOrderAndLoneCesu8SurrogateReasonOptions.rejectReasonNameLoneCesu8SurrogateTokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB8\x80\xED\xA0\xBDTotal",
                  parsedViolations,
                  mixedOrderAndLoneCesu8SurrogateReasonOptions,
                  &parseError),
                "mixed-order-CESU-8-surrogate reason-token mode keeps dedicated mixed-order reason precedence when lone-surrogate mode is also enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMixedOrderCesu8SurrogateToken,
                "mixed-order-CESU-8-surrogate reason-token mode reports mixed-order reason before lone-surrogate reason when both modes are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "mixed-order-CESU-8-surrogate reason-token mode reports reason field index when mixed-order precedence wins over lone-surrogate mode");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\xBD\xED\xB8\x80Total",
                  parsedViolations,
                  mixedOrderCesu8SurrogateReasonOptions,
                  &parseError),
                "mixed-order-CESU-8-surrogate reason-token mode leaves high-before-low surrogate pairs to generic reason validation");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "mixed-order-CESU-8-surrogate reason-token mode does not classify high-before-low surrogate pairs as mixed-order");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\x80\xED\xA0\xBDTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects same-order high-high CESU-8 surrogate reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for same-order high-high CESU-8 surrogate reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions sameOrderCesu8SurrogateReasonOptions;
  sameOrderCesu8SurrogateReasonOptions.rejectReasonNameSameOrderCesu8SurrogateTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  sameOrderCesu8SurrogateReasonOptions,
                  &parseError),
                "same-order-CESU-8-surrogate reason-token mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "same-order-CESU-8-surrogate reason-token mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\x80\xED\xA0\xBDTotal",
                  parsedViolations,
                  sameOrderCesu8SurrogateReasonOptions,
                  &parseError),
                "same-order-CESU-8-surrogate reason-token mode rejects high-before-high CESU-8 surrogate sequences");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameSameOrderCesu8SurrogateToken,
                "same-order-CESU-8-surrogate reason-token mode reports dedicated same-order surrogate reason for high-before-high sequences");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "same-order-CESU-8-surrogate reason-token mode reports reason field index for high-before-high surrogate sequences");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB0\x80\xED\xB8\x80Total",
                  parsedViolations,
                  sameOrderCesu8SurrogateReasonOptions,
                  &parseError),
                "same-order-CESU-8-surrogate reason-token mode rejects low-before-low CESU-8 surrogate sequences");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameSameOrderCesu8SurrogateToken,
                "same-order-CESU-8-surrogate reason-token mode reports dedicated same-order surrogate reason for low-before-low sequences");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "same-order-CESU-8-surrogate reason-token mode reports reason field index for low-before-low surrogate sequences");

  SkipDiagnosticsStrictViolationsParseOptions sameOrderAndLoneCesu8SurrogateReasonOptions;
  sameOrderAndLoneCesu8SurrogateReasonOptions.rejectReasonNameSameOrderCesu8SurrogateTokens = true;
  sameOrderAndLoneCesu8SurrogateReasonOptions.rejectReasonNameLoneCesu8SurrogateTokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\x80\xED\xA0\xBDTotal",
                  parsedViolations,
                  sameOrderAndLoneCesu8SurrogateReasonOptions,
                  &parseError),
                "same-order-CESU-8-surrogate reason-token mode keeps dedicated same-order reason precedence when lone-surrogate mode is also enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameSameOrderCesu8SurrogateToken,
                "same-order-CESU-8-surrogate reason-token mode reports same-order reason before lone-surrogate reason when both modes are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "same-order-CESU-8-surrogate reason-token mode reports reason field index when same-order precedence wins over lone-surrogate mode");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB8\x80\xED\xA0\xBDTotal",
                  parsedViolations,
                  sameOrderCesu8SurrogateReasonOptions,
                  &parseError),
                "same-order-CESU-8-surrogate reason-token mode leaves low-before-high surrogate pairs to generic reason validation");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "same-order-CESU-8-surrogate reason-token mode does not classify mixed-order surrogate pairs as same-order");

  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateReasonOptions;
  allCesu8SurrogateReasonOptions.rejectReasonNameLoneCesu8SurrogateTokens = true;
  allCesu8SurrogateReasonOptions.rejectReasonNamePairedCesu8SurrogateTokens = true;
  allCesu8SurrogateReasonOptions.rejectReasonNameMixedOrderCesu8SurrogateTokens = true;
  allCesu8SurrogateReasonOptions.rejectReasonNameSameOrderCesu8SurrogateTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  allCesu8SurrogateReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes accept canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "all-CESU-8-surrogate reason-token modes clear parse error on canonical reason-name tokens");
  CHECK_MESSAGE(parsedViolations.size() == 1,
                "all-CESU-8-surrogate reason-token modes decode exactly one canonical reason entry");
  CHECK_MESSAGE(parsedViolations[0].reason == SkipDiagnosticsParseErrorReason::InconsistentReasonTotal,
                "all-CESU-8-surrogate reason-token modes decode canonical reason names without remapping");

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=ReasonNameSameOrderCesu8SurrogateToken",
                  parsedViolations,
                  allCesu8SurrogateReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes accept canonical surrogate-related reason names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "all-CESU-8-surrogate reason-token modes clear parse error on canonical surrogate-related reason names");
  CHECK_MESSAGE(parsedViolations.size() == 1,
                "all-CESU-8-surrogate reason-token modes decode exactly one canonical surrogate-related reason entry");
  CHECK_MESSAGE(parsedViolations[0].reason == SkipDiagnosticsParseErrorReason::ReasonNameSameOrderCesu8SurrogateToken,
                "all-CESU-8-surrogate reason-token modes decode canonical surrogate-related reason names without remapping");
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReason",
                  parsedViolations,
                  allCesu8SurrogateReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes accept unknown-reason fallback token when fallback rejection mode is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "all-CESU-8-surrogate reason-token modes clear parse error on unknown-reason fallback token when fallback rejection mode is disabled");
  CHECK_MESSAGE(parsedViolations.size() == 1,
                "all-CESU-8-surrogate reason-token modes decode exactly one fallback-token reason entry");
  CHECK_MESSAGE(parsedViolations[0].reason == static_cast<SkipDiagnosticsParseErrorReason>(255),
                "all-CESU-8-surrogate reason-token modes preserve unknown-reason fallback value");

  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateStrictReasonTokenOptions = allCesu8SurrogateReasonOptions;
  allCesu8SurrogateStrictReasonTokenOptions.rejectUnknownReasonFallbackToken = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReason",
                  parsedViolations,
                  allCesu8SurrogateStrictReasonTokenOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes reject unknown-reason fallback token when fallback rejection mode is enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonFallbackToken,
                "all-CESU-8-surrogate reason-token modes report fallback-token reason when fallback rejection mode is enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index when fallback rejection mode is enabled");
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReasonSuffix",
                  parsedViolations,
                  allCesu8SurrogateStrictReasonTokenOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes keep exact-match semantics for fallback-token rejection");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "all-CESU-8-surrogate reason-token modes treat fallback-like suffix tokens as unknown names instead of fallback tokens");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for fallback-like suffix tokens");
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=PrefixUnknownParseErrorReason",
                  parsedViolations,
                  allCesu8SurrogateStrictReasonTokenOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes treat fallback-like prefix tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "all-CESU-8-surrogate reason-token modes do not classify fallback-like prefix tokens as fallback tokens");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for fallback-like prefix tokens");
  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateFallbackAndAsciiWhitespaceOptions =
    allCesu8SurrogateStrictReasonTokenOptions;
  allCesu8SurrogateFallbackAndAsciiWhitespaceOptions.rejectReasonNameAsciiWhitespaceTokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason= UnknownParseErrorReason",
                  parsedViolations,
                  allCesu8SurrogateFallbackAndAsciiWhitespaceOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize ASCII-whitespace diagnostics over fallback-token rejection when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameAsciiWhitespaceToken,
                "all-CESU-8-surrogate reason-token modes report ASCII-whitespace reason before fallback-token reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for ASCII-whitespace-over-fallback precedence");
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReason \t",
                  parsedViolations,
                  allCesu8SurrogateFallbackAndAsciiWhitespaceOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize trailing ASCII-whitespace diagnostics over fallback-token rejection when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameAsciiWhitespaceToken,
                "all-CESU-8-surrogate reason-token modes report trailing ASCII-whitespace reason before fallback-token reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for trailing-ASCII-whitespace-over-fallback precedence");
  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateFallbackAndNonAsciiWhitespaceOptions =
    allCesu8SurrogateStrictReasonTokenOptions;
  allCesu8SurrogateFallbackAndNonAsciiWhitespaceOptions.rejectReasonNameNonAsciiWhitespaceTokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReason\xC2\xA0",
                  parsedViolations,
                  allCesu8SurrogateFallbackAndNonAsciiWhitespaceOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize non-ASCII-whitespace diagnostics over fallback-token rejection when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiWhitespaceToken,
                "all-CESU-8-surrogate reason-token modes report non-ASCII-whitespace reason before fallback-token reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for non-ASCII-whitespace-over-fallback precedence");
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\xC2\xA0UnknownParseErrorReason",
                  parsedViolations,
                  allCesu8SurrogateFallbackAndNonAsciiWhitespaceOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize leading non-ASCII-whitespace diagnostics over fallback-token rejection when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiWhitespaceToken,
                "all-CESU-8-surrogate reason-token modes report leading non-ASCII-whitespace reason before fallback-token reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for leading-non-ASCII-whitespace-over-fallback precedence");
  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateFallbackAndNonWhitespaceNonAsciiOptions =
    allCesu8SurrogateStrictReasonTokenOptions;
  allCesu8SurrogateFallbackAndNonWhitespaceNonAsciiOptions.rejectReasonNameNonWhitespaceNonAsciiTokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReason\xC3\xA9",
                  parsedViolations,
                  allCesu8SurrogateFallbackAndNonWhitespaceNonAsciiOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize non-whitespace non-ASCII diagnostics over fallback-token rejection when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "all-CESU-8-surrogate reason-token modes report non-whitespace non-ASCII reason before fallback-token reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for non-whitespace-non-ASCII-over-fallback precedence");
  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateFallbackAndAsciiControlOptions =
    allCesu8SurrogateStrictReasonTokenOptions;
  allCesu8SurrogateFallbackAndAsciiControlOptions.rejectReasonNameAsciiControlCharacterTokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReason\x1F",
                  parsedViolations,
                  allCesu8SurrogateFallbackAndAsciiControlOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize ASCII-control diagnostics over fallback-token rejection when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameAsciiControlCharacterToken,
                "all-CESU-8-surrogate reason-token modes report ASCII-control reason before fallback-token reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for ASCII-control-over-fallback precedence");
  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateFallbackAndNonAsciiControlOptions =
    allCesu8SurrogateStrictReasonTokenOptions;
  allCesu8SurrogateFallbackAndNonAsciiControlOptions.rejectReasonNameNonAsciiUnicodeControlCharacterTokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReason\xE2\x80\x8F",
                  parsedViolations,
                  allCesu8SurrogateFallbackAndNonAsciiControlOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize non-ASCII Unicode-control diagnostics over fallback-token rejection when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiUnicodeControlCharacterToken,
                "all-CESU-8-surrogate reason-token modes report non-ASCII Unicode-control reason before fallback-token reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for non-ASCII-Unicode-control-over-fallback precedence");
  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateFallbackAndUnicodeNoncharacterOptions =
    allCesu8SurrogateStrictReasonTokenOptions;
  allCesu8SurrogateFallbackAndUnicodeNoncharacterOptions.rejectReasonNameUnicodeNoncharacterTokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReason\xEF\xB7\x90",
                  parsedViolations,
                  allCesu8SurrogateFallbackAndUnicodeNoncharacterOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize Unicode-noncharacter diagnostics over fallback-token rejection when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameUnicodeNoncharacterToken,
                "all-CESU-8-surrogate reason-token modes report Unicode-noncharacter reason before fallback-token reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for Unicode-noncharacter-over-fallback precedence");

  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateAndMalformedReasonOptions = allCesu8SurrogateReasonOptions;
  allCesu8SurrogateAndMalformedReasonOptions.rejectReasonNameMalformedUtf8Tokens = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0Total",
                  parsedViolations,
                  allCesu8SurrogateAndMalformedReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes report malformed UTF-8 for truncated CESU-8 surrogate prefixes when malformed-UTF-8 mode is enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "all-CESU-8-surrogate reason-token modes keep malformed-UTF-8 diagnostics for truncated CESU-8 surrogate prefixes");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for malformed UTF-8 tokens");
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB8\x80\xED\xA0\xBD\xED\xA0",
                  parsedViolations,
                  allCesu8SurrogateAndMalformedReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize surrogate diagnostics over malformed-UTF-8 diagnostics when both conditions occur in one token");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMixedOrderCesu8SurrogateToken,
                "all-CESU-8-surrogate reason-token modes report mixed-order-surrogate reason before malformed-UTF-8 reason when both conditions occur in one token");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for surrogate-over-malformed precedence");

  SkipDiagnosticsStrictViolationsParseOptions allCesu8SurrogateMalformedAndFallbackOptions = allCesu8SurrogateAndMalformedReasonOptions;
  allCesu8SurrogateMalformedAndFallbackOptions.rejectUnknownReasonFallbackToken = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=UnknownParseErrorReason\xED\xA0",
                  parsedViolations,
                  allCesu8SurrogateMalformedAndFallbackOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes prioritize malformed-UTF-8 diagnostics over unknown-fallback-token diagnostics when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "all-CESU-8-surrogate reason-token modes report malformed-UTF-8 reason before unknown-fallback-token reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for malformed-UTF-8 precedence over fallback-token rejection");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\x80Total",
                  parsedViolations,
                  allCesu8SurrogateReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes classify lone high-surrogate tokens as lone surrogates");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameLoneCesu8SurrogateToken,
                "all-CESU-8-surrogate reason-token modes report lone-surrogate reason for lone high-surrogate tokens");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for lone high-surrogate tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB0\x80Total",
                  parsedViolations,
                  allCesu8SurrogateReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes classify lone low-surrogate tokens as lone surrogates");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameLoneCesu8SurrogateToken,
                "all-CESU-8-surrogate reason-token modes report lone-surrogate reason for lone low-surrogate tokens");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for lone low-surrogate tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\xBD\xED\xB8\x80Total",
                  parsedViolations,
                  allCesu8SurrogateReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes classify high-before-low surrogate pairs as paired surrogates");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNamePairedCesu8SurrogateToken,
                "all-CESU-8-surrogate reason-token modes report paired-surrogate reason for high-before-low surrogate pairs");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for high-before-low surrogate pairs");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB8\x80\xED\xA0\xBDTotal",
                  parsedViolations,
                  allCesu8SurrogateReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes classify low-before-high surrogate pairs as mixed-order surrogates");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMixedOrderCesu8SurrogateToken,
                "all-CESU-8-surrogate reason-token modes report mixed-order-surrogate reason for low-before-high surrogate pairs");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for low-before-high surrogate pairs");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xA0\x80\xED\xA0\xBDTotal",
                  parsedViolations,
                  allCesu8SurrogateReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes classify high-before-high surrogate pairs as same-order surrogates");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameSameOrderCesu8SurrogateToken,
                "all-CESU-8-surrogate reason-token modes report same-order-surrogate reason for high-before-high surrogate pairs");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for high-before-high surrogate pairs");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xED\xB0\x80\xED\xB8\x80Total",
                  parsedViolations,
                  allCesu8SurrogateReasonOptions,
                  &parseError),
                "all-CESU-8-surrogate reason-token modes classify low-before-low surrogate pairs as same-order surrogates");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameSameOrderCesu8SurrogateToken,
                "all-CESU-8-surrogate reason-token modes report same-order-surrogate reason for low-before-low surrogate pairs");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "all-CESU-8-surrogate reason-token modes report reason field index for low-before-low surrogate pairs");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=Inconsist\xC3\xA9ntReasonTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects non-whitespace non-ASCII reason tokens as unknown names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "default strict-violation parser reports unknown reason for non-whitespace non-ASCII reason tokens");

  SkipDiagnosticsStrictViolationsParseOptions nonWhitespaceNonAsciiReasonOptions;
  nonWhitespaceNonAsciiReasonOptions.rejectReasonNameNonWhitespaceNonAsciiTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "non-whitespace non-ASCII reason-token mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=Inconsist\xC3\xA9ntReasonTotal",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode rejects accented-letter reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated non-whitespace non-ASCII reason-token error");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for accented-letter tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xF0\x9F\x98\x80Total",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode rejects emoji-bearing reason tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason for emoji-bearing tokens");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for emoji-bearing tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\xC2Inconsist\xC3\xA9ntReasonTotal",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode leaves malformed-before-accented tokens to unknown-name validation when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed bytes appear before any valid accented code points and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-before-accented tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2Total\xF0\x9F\x98\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode leaves malformed-before-emoji tokens to unknown-name validation when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed bytes appear before any valid emoji code points and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-before-emoji tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xC2Total\xC3\xA9",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode leaves malformed-before-first-non-whitespace tokens as unknown names even when earlier non-ASCII whitespace is present and malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed bytes appear before the first accented code point after earlier non-ASCII whitespace and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-before-first-accented-after-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2Total\xF0\x9F\x98\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode leaves malformed-before-first-emoji tokens as unknown names even when earlier non-ASCII whitespace is present and malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed bytes appear before the first emoji code point after earlier non-ASCII whitespace and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-before-first-emoji-after-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xC2"
                  "Break\xE3\x80\x80\x80"
                  "Gap\xC3\xA9Total",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode leaves tokens as unknown names when multiple malformed segments occur before the first accented code point even if multiple non-ASCII whitespace segments are present and malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments occur before the first accented code point despite multiple preceding non-ASCII whitespace segments and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-before-first-accented-after-multiple-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2"
                  "Break\xC2\xA0\x80"
                  "Gap\xF0\x9F\x98\x80Total",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode leaves tokens as unknown names when multiple malformed segments occur before the first emoji code point even if multiple non-ASCII whitespace segments are present and malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments occur before the first emoji code point despite multiple preceding non-ASCII whitespace segments and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-before-first-emoji-after-multiple-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\xC3\xA9Total\x80"
                  "Tail",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps malformed-before-first tokens classified as unknown names when malformed UTF-8 also appears after the first accented code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed bytes appear both before and after the first accented code point and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-before-and-after-first-accented tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\xF0\x9F\x98\x80Total\xC2"
                  "Trail",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps malformed-before-first tokens classified as unknown names when malformed UTF-8 also appears after the first emoji code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed bytes appear both before and after the first emoji code point and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-before-and-after-first-emoji tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xC2"
                  "Break\xC3\xA9Total\x80"
                  "Gap\xC2"
                  "Tail\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when malformed UTF-8 appears before the first accented code point even if additional malformed and non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed bytes appear before the first accented code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-before-first-accented with later-malformed-and-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2"
                  "Break\xF0\x9F\x98\x80Total\x80"
                  "Gap\xC2"
                  "Tail\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when malformed UTF-8 appears before the first emoji code point even if additional malformed and non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed bytes appear before the first emoji code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-before-first-emoji with later-malformed-and-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed UTF-8 segments appear before the first accented code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments appear before the first accented code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-before-first-accented with later-malformed-and-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed UTF-8 segments appear before the first emoji code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments appear before the first emoji code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-before-first-emoji with later-malformed-and-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments appear only before the first accented code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-only-before-first-accented with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments appear only before the first emoji code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-only-before-first-emoji with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments surround that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-only-before-first-accented with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments surround that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-only-before-first-emoji with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments bracket the first accented code point while multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-bracketing-first-accented with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments bracket the first emoji code point while multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-bracketing-first-emoji with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments bracket the first accented code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments bracket the first accented code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-bracketing-first-accented with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments bracket the first emoji code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments bracket the first emoji code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-bracketing-first-emoji with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xC3\xA9Total\xC2",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode classifies mixed tokens when non-ASCII whitespace appears before the first accented code point and malformed UTF-8 appears only after that first accented code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when malformed bytes occur only after the first accented code point and earlier non-ASCII whitespace is present");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for whitespace-before-first-accented then malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xF0\x9F\x98\x80Total\xC2",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode classifies mixed tokens when non-ASCII whitespace appears before the first emoji code point and malformed UTF-8 appears only after that first emoji code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when malformed bytes occur only after the first emoji code point and earlier non-ASCII whitespace is present");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for whitespace-before-first-emoji then malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC3\xA9Total\xC2",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple non-ASCII whitespace segments appear before the first accented code point and malformed UTF-8 appears only after that first accented code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when multiple non-ASCII whitespace segments precede the first accented code point and malformed bytes occur only after that first accented code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-whitespace-before-first-accented then malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80Total\xC2",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple non-ASCII whitespace segments appear before the first emoji code point and malformed UTF-8 appears only after that first emoji code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when multiple non-ASCII whitespace segments precede the first emoji code point and malformed bytes occur only after that first emoji code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-whitespace-before-first-emoji then malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC3\xA9Total\xC2"
                  "Break\x80Tail",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple non-ASCII whitespace segments appear before the first accented code point and multiple malformed segments appear only after that first accented code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when multiple malformed segments occur only after the first accented code point and multiple non-ASCII whitespace segments precede it");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-whitespace-before-first-accented with multiple post-accented malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80Total\xC2"
                  "Split\x80"
                  "End",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple non-ASCII whitespace segments appear before the first emoji code point and multiple malformed segments appear only after that first emoji code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when multiple malformed segments occur only after the first emoji code point and multiple non-ASCII whitespace segments precede it");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-whitespace-before-first-emoji with multiple post-emoji malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC3\xA9Total\xC2"
                  "Break\x80"
                  "Tail\xC2"
                  "End",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple non-ASCII whitespace segments appear only before the first accented code point and three malformed segments appear only after that first accented code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when three malformed segments occur only after the first accented code point and multiple non-ASCII whitespace segments remain only before it");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for pre-first-only multiple-whitespace with three post-first accented malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80Total\xC2"
                  "Split\x80"
                  "Tail\xC2"
                  "End",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple non-ASCII whitespace segments appear only before the first emoji code point and three malformed segments appear only after that first emoji code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when three malformed segments occur only after the first emoji code point and multiple non-ASCII whitespace segments remain only before it");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for pre-first-only multiple-whitespace with three post-first emoji malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\xC2InconsistentReason\xC2\xA0Total",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode leaves malformed-first tokens with subsequent non-ASCII-whitespace-only bytes to unknown-name validation when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed-first tokens only contain non-ASCII whitespace after the malformed bytes and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-first plus non-ASCII-whitespace-only tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=\xC2InconsistentReason\xE3\x80\x80Total\xC2\xA0",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode leaves malformed-first tokens with multiple non-ASCII-whitespace-only segments to unknown-name validation when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when malformed-first tokens contain only IDEOGRAPHIC SPACE/NBSP non-ASCII whitespace after the malformed bytes and malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for malformed-first tokens with multiple non-ASCII-whitespace-only segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=Inconsist\xC3\xA9\xC2ntReasonTotal",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode classifies mixed accented-letter-plus-interior-malformed tokens when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason for mixed accented-letter-plus-interior-malformed tokens when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for mixed accented-letter-plus-interior-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xF0\x9F\x98\x80\xC2Total",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode classifies mixed emoji-plus-interior-malformed tokens when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason for mixed emoji-plus-interior-malformed tokens when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for mixed emoji-plus-interior-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=Inconsist\xC3\xA9\xC2nt\xC2ReasonTotal",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode classifies mixed accented-letter tokens with multiple interior malformed segments when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason for mixed accented-letter tokens with multiple interior malformed segments when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for mixed accented-letter tokens with multiple interior malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xF0\x9F\x98\x80\xC2Tot\xC2"
                  "al",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode classifies mixed emoji tokens with multiple interior malformed segments when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason for mixed emoji tokens with multiple interior malformed segments when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for mixed emoji tokens with multiple interior malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=Inconsist\xC3\xA9ntReasonTotal\xC2",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode classifies mixed accented-letter-plus-trailing-malformed tokens when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason for mixed accented-letter-plus-trailing-malformed tokens when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for mixed accented-letter-plus-trailing-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xF0\x9F\x98\x80Total\xC2",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode classifies mixed emoji-plus-trailing-malformed tokens when malformed-UTF-8 rejection is disabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason for mixed emoji-plus-trailing-malformed tokens when malformed rejection is disabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for mixed emoji-plus-trailing-malformed tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC3\xA9Total\xC2"
                  "Break\x80"
                  "Gap\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when an accented first non-whitespace code point is followed by multiple malformed segments and later additional non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when malformed bytes follow the first accented code point before later non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for first-accented then multiple-malformed then later-non-ASCII-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xF0\x9F\x98\x80Total\xC2"
                  "Break\x80"
                  "Gap\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when an emoji first non-whitespace code point is followed by multiple malformed segments and later additional non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when malformed bytes follow the first emoji code point before later non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for first-emoji then multiple-malformed then later-non-ASCII-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC3\xA9Total\xC2"
                  "Break\x80"
                  "Gap\xE3\x80\x80\xC2\xA0"
                  "Trail\xC2"
                  "Tail\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple malformed segments appear only after the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when multiple malformed segments and multiple non-ASCII whitespace segments both appear only after the first accented code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for post-first-only accented malformed and post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xF0\x9F\x98\x80Total\xC2"
                  "Split\x80"
                  "Gap\xC2\xA0\xE3\x80\x80"
                  "Trail\xC2"
                  "Tail\xC2\xA0",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple malformed segments appear only after the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when multiple malformed segments and multiple non-ASCII whitespace segments both appear only after the first emoji code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for post-first-only emoji malformed and post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\xC3\xA9Total\x80"
                  "Tail\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments bracket the first accented code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-bracketing-first-accented with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\xF0\x9F\x98\x80Total\x80"
                  "Tail\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode still leaves tokens as unknown names when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "non-whitespace non-ASCII reason-token mode reports unknown reason when multiple malformed segments bracket the first emoji code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for multiple-malformed-bracketing-first-emoji with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC3\xA9Row\xC2\xA0Totals\xC2"
                  "Break\x80"
                  "Tail\xE3\x80\x80",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple malformed segments appear only after the first accented code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when malformed segments appear only after the first accented code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for first-accented then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80Row\xE3\x80\x80Totals\xC2"
                  "Break\x80"
                  "Tail\xC2\xA0",
                  parsedViolations,
                  nonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "non-whitespace non-ASCII reason-token mode keeps classifying tokens when multiple malformed segments appear only after the first emoji code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken,
                "non-whitespace non-ASCII reason-token mode reports dedicated reason when malformed segments appear only after the first emoji code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "non-whitespace non-ASCII reason-token mode reports reason field index for first-emoji then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  SkipDiagnosticsStrictViolationsParseOptions malformedAndNonWhitespaceNonAsciiReasonOptions;
  malformedAndNonWhitespaceNonAsciiReasonOptions.rejectReasonNameMalformedUtf8Tokens = true;
  malformedAndNonWhitespaceNonAsciiReasonOptions.rejectReasonNameNonWhitespaceNonAsciiTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode accepts canonical reason-name tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode clears parse error on canonical reason-name tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=Inconsist\xC3\xA9ntReasonTotal\xC2",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode prioritizes malformed-UTF-8 diagnostics over non-whitespace non-ASCII diagnostics when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 reason before non-whitespace non-ASCII reason when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for malformed-over-non-whitespace-non-ASCII precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xF0\x9F\x98\x80Total\xC2",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode also prioritizes malformed-UTF-8 diagnostics for emoji overlap tokens when both checks are enabled");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 reason for emoji overlap tokens when both checks are enabled");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for emoji overlap precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\xC3\xA9Total\x80"
                  "Tail",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode still reports malformed-UTF-8 diagnostics when malformed segments appear before and after the first accented code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when malformed bytes bracket the first accented code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for malformed-before-and-after-first-accented precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\xF0\x9F\x98\x80Total\xC2"
                  "Trail",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode still reports malformed-UTF-8 diagnostics when malformed segments appear before and after the first emoji code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when malformed bytes bracket the first emoji code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for malformed-before-and-after-first-emoji precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xC2"
                  "Break\xE3\x80\x80\x80"
                  "Gap\xC3\xA9Total",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear before the first accented code point even with multiple non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments precede the first accented code point and multiple non-ASCII whitespace segments are present");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-before-first-accented with multiple-whitespace precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2"
                  "Break\xC2\xA0\x80"
                  "Gap\xF0\x9F\x98\x80Total",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear before the first emoji code point even with multiple non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments precede the first emoji code point and multiple non-ASCII whitespace segments are present");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-before-first-emoji with multiple-whitespace precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode continues reporting malformed-UTF-8 diagnostics when multiple malformed segments appear before and after the first accented code point while multiple non-ASCII whitespace segments are present");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments are present");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-before-and-after-first-accented with multiple-whitespace precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode continues reporting malformed-UTF-8 diagnostics when multiple malformed segments appear before and after the first emoji code point while multiple non-ASCII whitespace segments are present");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments are present");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-before-and-after-first-emoji with multiple-whitespace precedence");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear before the first accented code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments precede the first accented code point and later malformed plus non-ASCII whitespace segments follow");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-before-first-accented with later-malformed-and-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear before the first emoji code point and additional malformed plus non-ASCII whitespace segments appear later");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments precede the first emoji code point and later malformed plus non-ASCII whitespace segments follow");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-before-first-emoji with later-malformed-and-whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC3\xA9Total\xC2"
                  "Break\x80"
                  "Gap\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when an accented first non-whitespace code point is followed by multiple malformed segments and later additional non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when malformed bytes follow the first accented code point before later non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for first-accented then multiple-malformed then later-non-ASCII-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xF0\x9F\x98\x80Total\xC2"
                  "Break\x80"
                  "Gap\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when an emoji first non-whitespace code point is followed by multiple malformed segments and later additional non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when malformed bytes follow the first emoji code point before later non-ASCII whitespace segments");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for first-emoji then multiple-malformed then later-non-ASCII-whitespace tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC3\xA9Total\xC2"
                  "Break\x80Tail",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only after the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments occur only after the first accented code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-whitespace-before-first-accented with multiple post-accented malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80Total\xC2"
                  "Split\x80"
                  "End",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only after the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments occur only after the first emoji code point and multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-whitespace-before-first-emoji with multiple post-emoji malformed segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments appear only before the first accented code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-only-before-first-accented with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments appear only before the first emoji code point and multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-only-before-first-emoji with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC3\xA9Row\xC2\xA0Totals\xC2"
                  "Break\x80"
                  "Tail\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only after the first accented code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when malformed segments appear only after the first accented code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for first-accented then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xF0\x9F\x98\x80Row\xE3\x80\x80Totals\xC2"
                  "Break\x80"
                  "Tail\xC2\xA0",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments appear only after the first emoji code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when malformed segments appear only after the first emoji code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for first-emoji then post-first-multi-malformed with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point while multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-bracketing-first-accented with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xC2"
                  "Tail\x80"
                  "End",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only before that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point while multiple non-ASCII whitespace segments remain only before that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-bracketing-first-emoji with pre-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\xC3\xA9Total\x80"
                  "Tail\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-bracketing-first-accented with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2"
                  "Break\xF0\x9F\x98\x80Total\x80"
                  "Tail\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point and multiple non-ASCII whitespace segments appear only after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point while multiple non-ASCII whitespace segments remain only after that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-bracketing-first-emoji with post-first-only non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2"
                  "Break\x80"
                  "Gap\xC3\xA9Total\xC2"
                  "Tail\x80"
                  "End\xC2\xA0\xE3\x80\x80",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first accented code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-bracketing-first-accented with surrounding non-ASCII whitespace segments");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReason\xE3\x80\x80\xC2\xA0\xC2"
                  "Break\x80"
                  "Gap\xF0\x9F\x98\x80Total\xC2"
                  "Tail\x80"
                  "End\xE3\x80\x80\xC2\xA0",
                  parsedViolations,
                  malformedAndNonWhitespaceNonAsciiReasonOptions,
                  &parseError),
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point and non-ASCII whitespace appears both before and after that first code point");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode keeps malformed-UTF-8 diagnostics when multiple malformed segments bracket the first emoji code point with non-ASCII whitespace on both sides of that first code point");
  CHECK_MESSAGE(parseError.fieldIndex == 2,
                "combined malformed-UTF-8 and non-whitespace non-ASCII mode reports reason field index for multiple-malformed-bracketing-first-emoji with surrounding non-ASCII whitespace segments");

  std::string nonContiguousButCompletePayload =
    "strictViolations.count=3;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal;"
    "strictViolations.2.fieldIndex=13;"
    "strictViolations.2.reason=UnknownParseErrorReason;"
    "strictViolations.1.fieldIndex=11;"
    "strictViolations.1.reason=InconsistentMatrixRowTotals";
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(nonContiguousButCompletePayload, parsedViolations, &parseError),
                "default strict-violation parser accepts non-contiguous index arrival when complete");
  CHECK_MESSAGE(parsedViolations.size() == 3, "non-contiguous complete payload restores all entries");

  SkipDiagnosticsStrictViolationsParseOptions strictIndexOptions;
  strictIndexOptions.enforceContiguousIndices = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  nonContiguousButCompletePayload,
                  parsedViolations,
                  strictIndexOptions,
                  &parseError),
                "contiguous-index mode rejects first sparse index jump early");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::NonContiguousViolationIndex,
                "contiguous-index mode reports dedicated sparse-index reason");
  CHECK_MESSAGE(parseError.fieldIndex == 3, "contiguous-index mode reports sparse index field");

  SkipDiagnosticsStrictViolationsParseOptions normalizedIndexOptions;
  normalizedIndexOptions.enforceContiguousIndices = true;
  normalizedIndexOptions.normalizeOutOfOrderContiguousIndices = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  nonContiguousButCompletePayload,
                  parsedViolations,
                  normalizedIndexOptions,
                  &parseError),
                "normalized contiguous-index mode accepts out-of-order contiguous entries");
  CHECK_MESSAGE(parsedViolations.size() == 3, "normalized contiguous-index mode restores all entries");
  CHECK_MESSAGE(parsedViolations[0].fieldIndex == 3, "normalized contiguous-index mode keeps index-ordered output");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "normalized contiguous-index mode clears parse error on success");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=3;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.2.fieldIndex=13;"
                  "strictViolations.2.reason=UnknownParseErrorReason",
                  parsedViolations,
                  normalizedIndexOptions,
                  &parseError),
                "normalized contiguous-index mode rejects missing intermediate index");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::NonContiguousViolationIndex,
                "normalized contiguous-index mode reports missing-index reason");

  std::string conflictingFieldDuplicatePayload =
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.fieldIndex=4;"
    "strictViolations.0.reason=InconsistentReasonTotal";
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(conflictingFieldDuplicatePayload, parsedViolations, &parseError),
                "default strict-violation parser allows conflicting duplicate fields");
  CHECK_MESSAGE(parsedViolations[0].fieldIndex == 4, "default strict-violation parser keeps last duplicate field value");

  SkipDiagnosticsStrictViolationsParseOptions duplicateConflictOptions;
  duplicateConflictOptions.rejectConflictingDuplicateIndices = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  conflictingFieldDuplicatePayload,
                  parsedViolations,
                  duplicateConflictOptions,
                  &parseError),
                "duplicate-conflict mode rejects conflicting duplicate field values");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::DuplicateViolationConflict,
                "duplicate-conflict mode reports dedicated conflict reason");

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  duplicateConflictOptions,
                  &parseError),
                "duplicate-conflict mode accepts duplicate entries with identical values");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "duplicate-conflict mode keeps no-error state on identical duplicates");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.0.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  duplicateConflictOptions,
                  &parseError),
                "duplicate-conflict mode rejects conflicting duplicate reason values");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::DuplicateViolationConflict,
                "duplicate-conflict mode reports reason conflict");

  SkipDiagnosticsStrictViolationsParseOptions duplicateRejectOptions;
  duplicateRejectOptions.rejectDuplicateIndices = true;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  duplicateRejectOptions,
                  &parseError),
                "duplicate-reject mode rejects duplicate fields even when values match");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::DuplicateViolationEntry,
                "duplicate-reject mode reports duplicate-entry reason for field");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  duplicateRejectOptions,
                  &parseError),
                "duplicate-reject mode rejects duplicate reasons even when values match");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::DuplicateViolationEntry,
                "duplicate-reject mode reports duplicate-entry reason for reason");

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  duplicateRejectOptions,
                  &parseError),
                "duplicate-reject mode accepts non-duplicate entries");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "duplicate-reject mode clears parse error on valid payload");

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.count=1",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser accepts duplicate count fields");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "default strict-violation parser keeps no-error state on duplicate count fields");

  SkipDiagnosticsStrictViolationsParseOptions duplicateCountOptions;
  duplicateCountOptions.rejectDuplicateCountField = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  duplicateCountOptions,
                  &parseError),
                "duplicate-count mode accepts payloads with a single count field");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "duplicate-count mode clears parse error on accepted payload");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.count=1",
                  parsedViolations,
                  duplicateCountOptions,
                  &parseError),
                "duplicate-count mode rejects duplicate strict-violation count fields");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::DuplicateViolationCountField,
                "duplicate-count mode reports dedicated duplicate count reason");
  CHECK_MESSAGE(parseError.fieldIndex == 3, "duplicate-count mode reports duplicate count field index");

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.count=1",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser accepts trailing count field");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "default strict-violation parser clears parse error on trailing count payload");

  SkipDiagnosticsStrictViolationsParseOptions countOrderOptions;
  countOrderOptions.requireCountFieldBeforeEntries = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  countOrderOptions,
                  &parseError),
                "count-order mode accepts payloads where count appears before indexed entries");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "count-order mode clears parse error on ordered payload");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.count=1",
                  parsedViolations,
                  countOrderOptions,
                  &parseError),
                "count-order mode rejects indexed entries that appear before count");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ViolationCountFieldOrder,
                "count-order mode reports dedicated count-order reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "count-order mode reports first out-of-order indexed entry");

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=01;"
                  "strictViolations.00.fieldIndex=03;"
                  "strictViolations.00.reason=InconsistentReasonTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser accepts zero-padded numeric tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "default strict-violation parser keeps no-error state on zero-padded numeric tokens");

  SkipDiagnosticsStrictViolationsParseOptions zeroPaddedNumericOptions;
  zeroPaddedNumericOptions.rejectZeroPaddedNumericTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  zeroPaddedNumericOptions,
                  &parseError),
                "zero-padded-token mode accepts canonical numeric tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "zero-padded-token mode clears parse error on canonical numeric tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=01;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  zeroPaddedNumericOptions,
                  &parseError),
                "zero-padded-token mode rejects zero-padded count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ZeroPaddedNumericToken,
                "zero-padded-token mode reports zero-padded count reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "zero-padded-token mode reports count field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.00.fieldIndex=3;"
                  "strictViolations.00.reason=InconsistentReasonTotal",
                  parsedViolations,
                  zeroPaddedNumericOptions,
                  &parseError),
                "zero-padded-token mode rejects zero-padded index tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ZeroPaddedNumericToken,
                "zero-padded-token mode reports zero-padded index reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "zero-padded-token mode reports zero-padded index field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=03;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  zeroPaddedNumericOptions,
                  &parseError),
                "zero-padded-token mode rejects zero-padded fieldIndex tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ZeroPaddedNumericToken,
                "zero-padded-token mode reports zero-padded fieldIndex reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "zero-padded-token mode reports fieldIndex field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=+1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects plus-prefixed count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InvalidValue,
                "default strict-violation parser reports invalid value for plus-prefixed count tokens");

  SkipDiagnosticsStrictViolationsParseOptions plusPrefixedNumericOptions;
  plusPrefixedNumericOptions.rejectPlusPrefixedNumericTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  plusPrefixedNumericOptions,
                  &parseError),
                "plus-prefixed-token mode accepts canonical numeric tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "plus-prefixed-token mode clears parse error on canonical numeric tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=+1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  plusPrefixedNumericOptions,
                  &parseError),
                "plus-prefixed-token mode rejects plus-prefixed count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::PlusPrefixedNumericToken,
                "plus-prefixed-token mode reports plus-prefixed count reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "plus-prefixed-token mode reports plus-prefixed count field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.+0.fieldIndex=3;"
                  "strictViolations.+0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  plusPrefixedNumericOptions,
                  &parseError),
                "plus-prefixed-token mode rejects plus-prefixed index tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::PlusPrefixedNumericToken,
                "plus-prefixed-token mode reports plus-prefixed index reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "plus-prefixed-token mode reports plus-prefixed index field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=+3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  plusPrefixedNumericOptions,
                  &parseError),
                "plus-prefixed-token mode rejects plus-prefixed fieldIndex tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::PlusPrefixedNumericToken,
                "plus-prefixed-token mode reports plus-prefixed fieldIndex reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "plus-prefixed-token mode reports plus-prefixed fieldIndex field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=-1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects minus-prefixed count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InvalidValue,
                "default strict-violation parser reports invalid value for minus-prefixed count tokens");

  SkipDiagnosticsStrictViolationsParseOptions minusPrefixedNumericOptions;
  minusPrefixedNumericOptions.rejectMinusPrefixedNumericTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  minusPrefixedNumericOptions,
                  &parseError),
                "minus-prefixed-token mode accepts canonical numeric tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "minus-prefixed-token mode clears parse error on canonical numeric tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=-1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  minusPrefixedNumericOptions,
                  &parseError),
                "minus-prefixed-token mode rejects minus-prefixed count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::MinusPrefixedNumericToken,
                "minus-prefixed-token mode reports minus-prefixed count reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "minus-prefixed-token mode reports minus-prefixed count field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.-0.fieldIndex=3;"
                  "strictViolations.-0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  minusPrefixedNumericOptions,
                  &parseError),
                "minus-prefixed-token mode rejects minus-prefixed index tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::MinusPrefixedNumericToken,
                "minus-prefixed-token mode reports minus-prefixed index reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "minus-prefixed-token mode reports minus-prefixed index field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=-3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  minusPrefixedNumericOptions,
                  &parseError),
                "minus-prefixed-token mode rejects minus-prefixed fieldIndex tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::MinusPrefixedNumericToken,
                "minus-prefixed-token mode reports minus-prefixed fieldIndex reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "minus-prefixed-token mode reports minus-prefixed fieldIndex field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=-0;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects negative-zero count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InvalidValue,
                "default strict-violation parser reports invalid value for negative-zero count tokens");

  SkipDiagnosticsStrictViolationsParseOptions negativeZeroOptions;
  negativeZeroOptions.rejectNegativeZeroTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  negativeZeroOptions,
                  &parseError),
                "negative-zero mode accepts canonical numeric tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "negative-zero mode clears parse error on canonical numeric tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=-0;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  negativeZeroOptions,
                  &parseError),
                "negative-zero mode rejects negative-zero count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::NegativeZeroNumericToken,
                "negative-zero mode reports negative-zero count reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "negative-zero mode reports negative-zero count field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.-0.fieldIndex=3;"
                  "strictViolations.-0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  negativeZeroOptions,
                  &parseError),
                "negative-zero mode rejects negative-zero index tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::NegativeZeroNumericToken,
                "negative-zero mode reports negative-zero index reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "negative-zero mode reports negative-zero index field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=-0;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  negativeZeroOptions,
                  &parseError),
                "negative-zero mode rejects negative-zero fieldIndex tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::NegativeZeroNumericToken,
                "negative-zero mode reports negative-zero fieldIndex reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "negative-zero mode reports negative-zero fieldIndex field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count= 1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects leading-whitespace count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InvalidValue,
                "default strict-violation parser reports invalid value for leading-whitespace count tokens");

  SkipDiagnosticsStrictViolationsParseOptions leadingWhitespaceOptions;
  leadingWhitespaceOptions.rejectLeadingAsciiWhitespaceNumericTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  leadingWhitespaceOptions,
                  &parseError),
                "leading-whitespace-token mode accepts canonical numeric tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "leading-whitespace-token mode clears parse error on canonical numeric tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=\t1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  leadingWhitespaceOptions,
                  &parseError),
                "leading-whitespace-token mode rejects leading-whitespace count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::LeadingAsciiWhitespaceNumericToken,
                "leading-whitespace-token mode reports leading-whitespace count reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "leading-whitespace-token mode reports leading-whitespace count field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations. 0.fieldIndex=3;"
                  "strictViolations. 0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  leadingWhitespaceOptions,
                  &parseError),
                "leading-whitespace-token mode rejects leading-whitespace index tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::LeadingAsciiWhitespaceNumericToken,
                "leading-whitespace-token mode reports leading-whitespace index reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "leading-whitespace-token mode reports leading-whitespace index field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex= 3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  leadingWhitespaceOptions,
                  &parseError),
                "leading-whitespace-token mode rejects leading-whitespace fieldIndex tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::LeadingAsciiWhitespaceNumericToken,
                "leading-whitespace-token mode reports leading-whitespace fieldIndex reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "leading-whitespace-token mode reports leading-whitespace fieldIndex field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1 ;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  &parseError),
                "default strict-violation parser rejects trailing-whitespace count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InvalidValue,
                "default strict-violation parser reports invalid value for trailing-whitespace count tokens");

  SkipDiagnosticsStrictViolationsParseOptions trailingWhitespaceOptions;
  trailingWhitespaceOptions.rejectTrailingAsciiWhitespaceNumericTokens = true;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  trailingWhitespaceOptions,
                  &parseError),
                "trailing-whitespace-token mode accepts canonical numeric tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "trailing-whitespace-token mode clears parse error on canonical numeric tokens");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1\t;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  trailingWhitespaceOptions,
                  &parseError),
                "trailing-whitespace-token mode rejects trailing-whitespace count tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::TrailingAsciiWhitespaceNumericToken,
                "trailing-whitespace-token mode reports trailing-whitespace count reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "trailing-whitespace-token mode reports trailing-whitespace count field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0 .fieldIndex=3;"
                  "strictViolations.0 .reason=InconsistentReasonTotal",
                  parsedViolations,
                  trailingWhitespaceOptions,
                  &parseError),
                "trailing-whitespace-token mode rejects trailing-whitespace index tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::TrailingAsciiWhitespaceNumericToken,
                "trailing-whitespace-token mode reports trailing-whitespace index reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "trailing-whitespace-token mode reports trailing-whitespace index field index");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3 \t;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  trailingWhitespaceOptions,
                  &parseError),
                "trailing-whitespace-token mode rejects trailing-whitespace fieldIndex tokens");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::TrailingAsciiWhitespaceNumericToken,
                "trailing-whitespace-token mode reports trailing-whitespace fieldIndex reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "trailing-whitespace-token mode reports trailing-whitespace fieldIndex field index");

  SkipDiagnosticsStrictViolationsParseOptions countCapOptions;
  countCapOptions.enforceMaxViolationCount = true;
  countCapOptions.maxViolationCount = 2;
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(keyValueDump, parsedViolations, countCapOptions, &parseError),
                "count-cap mode rejects payloads above configured max count");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ViolationCountLimitExceeded,
                "count-cap mode reports count-limit reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "count-cap mode reports count field index");

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=2;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.1.fieldIndex=11;"
                  "strictViolations.1.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  countCapOptions,
                  &parseError),
                "count-cap mode accepts payloads at configured max count");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "count-cap mode clears parse error on accepted payload");
  CHECK_MESSAGE(parsedViolations.size() == 2, "count-cap mode preserves accepted entry count");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.2.fieldIndex=7;"
                  "strictViolations.2.reason=InconsistentReasonTotal;"
                  "strictViolations.count=3",
                  parsedViolations,
                  countCapOptions,
                  &parseError),
                "count-cap mode rejects out-of-cap index entries before count");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ViolationCountLimitExceeded,
                "count-cap mode reports index-limit reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "count-cap mode reports offending index field");

  SkipDiagnosticsStrictViolationsParseOptions indexCapOptions;
  indexCapOptions.enforceMaxViolationIndex = true;
  indexCapOptions.maxViolationIndex = 1;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=2;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.1.fieldIndex=11;"
                  "strictViolations.1.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  indexCapOptions,
                  &parseError),
                "index-cap mode accepts payloads at configured max decoded index");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "index-cap mode clears parse error on accepted payload");
  CHECK_MESSAGE(parsedViolations.size() == 2, "index-cap mode preserves accepted entry count");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.2.fieldIndex=7;"
                  "strictViolations.2.reason=InconsistentReasonTotal;"
                  "strictViolations.count=3",
                  parsedViolations,
                  indexCapOptions,
                  &parseError),
                "index-cap mode rejects entries above configured max decoded index");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ViolationIndexLimitExceeded,
                "index-cap mode reports index-limit reason");
  CHECK_MESSAGE(parseError.fieldIndex == 0, "index-cap mode reports offending index field");

  SkipDiagnosticsStrictViolationsParseOptions fieldIndexCapOptions;
  fieldIndexCapOptions.enforceMaxViolationFieldIndex = true;
  fieldIndexCapOptions.maxViolationFieldIndex = 11;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=2;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.1.fieldIndex=11;"
                  "strictViolations.1.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  fieldIndexCapOptions,
                  &parseError),
                "field-index-cap mode accepts payloads at configured max decoded fieldIndex");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "field-index-cap mode clears parse error on accepted payload");
  CHECK_MESSAGE(parsedViolations.size() == 2, "field-index-cap mode preserves accepted entry count");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=12;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  fieldIndexCapOptions,
                  &parseError),
                "field-index-cap mode rejects fieldIndex values above configured cap");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ViolationFieldIndexLimitExceeded,
                "field-index-cap mode reports field-index-limit reason");
  CHECK_MESSAGE(parseError.fieldIndex == 1, "field-index-cap mode reports offending field-index entry");

  SkipDiagnosticsStrictViolationsParseOptions fieldCapOptions;
  fieldCapOptions.enforceMaxFieldCount = true;
  fieldCapOptions.maxFieldCount = 5;
  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=2;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.1.fieldIndex=11;"
                  "strictViolations.1.reason=InconsistentMatrixRowTotals",
                  parsedViolations,
                  fieldCapOptions,
                  &parseError),
                "field-cap mode accepts payloads at configured max field count");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "field-cap mode clears parse error on accepted payload");
  CHECK_MESSAGE(parsedViolations.size() == 2, "field-cap mode preserves accepted entry count");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=3;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal;"
                  "strictViolations.1.fieldIndex=11;"
                  "strictViolations.1.reason=InconsistentMatrixRowTotals;"
                  "strictViolations.2.fieldIndex=13;"
                  "strictViolations.2.reason=UnknownParseErrorReason",
                  parsedViolations,
                  fieldCapOptions,
                  &parseError),
                "field-cap mode rejects payloads above configured max field count");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::ViolationFieldCountLimitExceeded,
                "field-cap mode reports field-limit reason");
  CHECK_MESSAGE(parseError.fieldIndex == 5, "field-cap mode reports first field beyond configured cap");

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue("strict_violations=none", parsedViolations, &parseError),
                "strict-violation parser accepts none sentinel");
  CHECK_MESSAGE(parsedViolations.empty(), "none sentinel clears parsed strict-violation list");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "none sentinel keeps parse error clear");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=2;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  &parseError),
                "strict-violation parser rejects count mismatches");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::InvalidValue,
                "count mismatch reports invalid value");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  &parseError),
                "strict-violation parser requires count field");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownKey,
                "missing count reports unknown key");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=NoSuchReason",
                  parsedViolations,
                  &parseError),
                "strict-violation parser rejects unknown reason names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::UnknownReasonName,
                "unknown strict-violation reason reported");

  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue("strict_violations=none;extra=1", parsedViolations, &parseError),
                "strict-violation parser rejects malformed none payload");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::MalformedNonePayload,
                "malformed none strict-violation error reported");
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
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::NonContiguousViolationIndex) ==
                  std::string_view("NonContiguousViolationIndex"),
                "non-contiguous strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::DuplicateViolationConflict) ==
                  std::string_view("DuplicateViolationConflict"),
                "duplicate strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::DuplicateViolationEntry) ==
                  std::string_view("DuplicateViolationEntry"),
                "duplicate-entry strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::UnknownReasonFallbackToken) ==
                  std::string_view("UnknownReasonFallbackToken"),
                "unknown-reason fallback-token parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ViolationCountLimitExceeded) ==
                  std::string_view("ViolationCountLimitExceeded"),
                "count-limit strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ViolationFieldCountLimitExceeded) ==
                  std::string_view("ViolationFieldCountLimitExceeded"),
                "field-limit strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ViolationIndexLimitExceeded) ==
                  std::string_view("ViolationIndexLimitExceeded"),
                "index-limit strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ViolationFieldIndexLimitExceeded) ==
                  std::string_view("ViolationFieldIndexLimitExceeded"),
                "field-index-limit strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::DuplicateViolationCountField) ==
                  std::string_view("DuplicateViolationCountField"),
                "duplicate-count strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ViolationCountFieldOrder) ==
                  std::string_view("ViolationCountFieldOrder"),
                "count-order strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ZeroPaddedNumericToken) ==
                  std::string_view("ZeroPaddedNumericToken"),
                "zero-padded numeric strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::PlusPrefixedNumericToken) ==
                  std::string_view("PlusPrefixedNumericToken"),
                "plus-prefixed numeric strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::MinusPrefixedNumericToken) ==
                  std::string_view("MinusPrefixedNumericToken"),
                "minus-prefixed numeric strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::NegativeZeroNumericToken) ==
                  std::string_view("NegativeZeroNumericToken"),
                "negative-zero numeric strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::LeadingAsciiWhitespaceNumericToken) ==
                  std::string_view("LeadingAsciiWhitespaceNumericToken"),
                "leading-whitespace numeric strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::TrailingAsciiWhitespaceNumericToken) ==
                  std::string_view("TrailingAsciiWhitespaceNumericToken"),
                "trailing-whitespace numeric strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameAsciiWhitespaceToken) ==
                  std::string_view("ReasonNameAsciiWhitespaceToken"),
                "reason-name-whitespace strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameEmbeddedAsciiWhitespaceToken) ==
                  std::string_view("ReasonNameEmbeddedAsciiWhitespaceToken"),
                "reason-name-embedded-whitespace strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiWhitespaceToken) ==
                  std::string_view("ReasonNameNonAsciiWhitespaceToken"),
                "reason-name-non-ASCII-whitespace strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token) ==
                  std::string_view("ReasonNameMalformedUtf8Token"),
                "reason-name-malformed-UTF-8 strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken) ==
                  std::string_view("ReasonNameNonWhitespaceNonAsciiToken"),
                "reason-name-non-whitespace-non-ASCII strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameAsciiControlCharacterToken) ==
                  std::string_view("ReasonNameAsciiControlCharacterToken"),
                "reason-name-ASCII-control strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiUnicodeControlCharacterToken) ==
                  std::string_view("ReasonNameNonAsciiUnicodeControlCharacterToken"),
                "reason-name-non-ASCII-Unicode-control strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameUnicodeNoncharacterToken) ==
                  std::string_view("ReasonNameUnicodeNoncharacterToken"),
                "reason-name-Unicode-noncharacter strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameLoneCesu8SurrogateToken) ==
                  std::string_view("ReasonNameLoneCesu8SurrogateToken"),
                "reason-name-lone-CESU-8-surrogate strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNamePairedCesu8SurrogateToken) ==
                  std::string_view("ReasonNamePairedCesu8SurrogateToken"),
                "reason-name-paired-CESU-8-surrogate strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameMixedOrderCesu8SurrogateToken) ==
                  std::string_view("ReasonNameMixedOrderCesu8SurrogateToken"),
                "reason-name-mixed-order-CESU-8-surrogate strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason::ReasonNameSameOrderCesu8SurrogateToken) ==
                  std::string_view("ReasonNameSameOrderCesu8SurrogateToken"),
                "reason-name-same-order-CESU-8-surrogate strict-violation parse error name");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(static_cast<size_t>(SkipDiagnosticsParseErrorReason::InconsistentTypeTotal)) ==
                  std::string_view("InconsistentTypeTotal"),
                "parse error name by index resolves known reason");
  SkipDiagnosticsParseErrorReason parsedReason{};
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonFromName("InconsistentMatrixRowTotals", parsedReason),
                "parse error reason parser accepts known names");
  CHECK_MESSAGE(parsedReason == SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals,
                "parse error reason parser value");
  CHECK_MESSAGE(!skipDiagnosticsParseErrorReasonFromName("NotAParseReason", parsedReason),
                "parse error reason parser rejects unknown names");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(static_cast<SkipDiagnosticsParseErrorReason>(255)) ==
                  std::string_view("UnknownParseErrorReason"),
                "unknown parse error name fallback");
  CHECK_MESSAGE(skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReasonCount + 1) ==
                  std::string_view("OutOfRangeSkipDiagnosticsParseErrorReason"),
                "parse error index formatter out-of-range fallback");
}

TEST_SUITE_END();
