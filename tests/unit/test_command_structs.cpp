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
