#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

#include <string_view>
#include <vector>

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.command_structs");

namespace {

auto expectStrictViolationReject(
  std::string_view payload,
  std::vector<SkipDiagnosticsParseError::StrictViolation>& parsedViolations,
  SkipDiagnosticsStrictViolationsParseOptions const& options,
  SkipDiagnosticsParseError& parseError,
  SkipDiagnosticsParseErrorReason expectedReason,
  size_t expectedFieldIndex = 2) -> void {
  CHECK_MESSAGE(!parseSkipDiagnosticsStrictViolationsKeyValue(payload, parsedViolations, options, &parseError),
                "strict-violation parser rejects payload");
  CHECK_MESSAGE(parseError.reason == expectedReason, "strict-violation parser reports expected reason");
  CHECK_MESSAGE(parseError.fieldIndex == expectedFieldIndex, "strict-violation parser reports expected field index");
}

} // namespace

TEST_CASE("skip_diagnostics_strict_violations_key_value_parse_compact") {
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

  SkipDiagnosticsStrictViolationsParseOptions rejectFallbackOptions;
  rejectFallbackOptions.rejectUnknownReasonFallbackToken = true;
  expectStrictViolationReject(
    keyValueDump,
    parsedViolations,
    rejectFallbackOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::UnknownReasonFallbackToken,
    6);

  CHECK_MESSAGE(parseSkipDiagnosticsStrictViolationsKeyValue(
                  "strictViolations.count=1;"
                  "strictViolations.0.fieldIndex=3;"
                  "strictViolations.0.reason=InconsistentReasonTotal",
                  parsedViolations,
                  rejectFallbackOptions,
                  &parseError),
                "strict reason-token mode accepts known reason names");
  CHECK_MESSAGE(parseError.reason == SkipDiagnosticsParseErrorReason::None,
                "strict reason-token mode clears parse error on valid known reason");

  // Whitespace modes (ASCII, embedded ASCII, and non-ASCII) should map to dedicated reasons.
  SkipDiagnosticsStrictViolationsParseOptions asciiWhitespaceOptions;
  asciiWhitespaceOptions.rejectReasonNameAsciiWhitespaceTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason= InconsistentReasonTotal",
    parsedViolations,
    asciiWhitespaceOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameAsciiWhitespaceToken);

  SkipDiagnosticsStrictViolationsParseOptions embeddedWhitespaceOptions;
  embeddedWhitespaceOptions.rejectReasonNameEmbeddedAsciiWhitespaceTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentMatrix RowTotals",
    parsedViolations,
    embeddedWhitespaceOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameEmbeddedAsciiWhitespaceToken);

  SkipDiagnosticsStrictViolationsParseOptions nonAsciiWhitespaceOptions;
  nonAsciiWhitespaceOptions.rejectReasonNameNonAsciiWhitespaceTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentMatrix\xC2\xA0RowTotals",
    parsedViolations,
    nonAsciiWhitespaceOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiWhitespaceToken);

  // Non-whitespace non-ASCII mode still classifies mixed tokens unless malformed bytes are seen first.
  SkipDiagnosticsStrictViolationsParseOptions nonWhitespaceNonAsciiOptions;
  nonWhitespaceNonAsciiOptions.rejectReasonNameNonWhitespaceNonAsciiTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\xC3\xA9Total",
    parsedViolations,
    nonWhitespaceNonAsciiOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameNonWhitespaceNonAsciiToken);

  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\xC2"
    "Break\x80"
    "Gap\xC3\xA9Total\xC2\xA0\xE3\x80\x80",
    parsedViolations,
    nonWhitespaceNonAsciiOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::UnknownReasonName);

  // Malformed UTF-8 mode should win precedence when combined with non-whitespace non-ASCII checks.
  SkipDiagnosticsStrictViolationsParseOptions malformedUtf8Options;
  malformedUtf8Options.rejectReasonNameMalformedUtf8Tokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal\xC2",
    parsedViolations,
    malformedUtf8Options,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token);

  SkipDiagnosticsStrictViolationsParseOptions malformedAndNonWhitespaceOptions;
  malformedAndNonWhitespaceOptions.rejectReasonNameMalformedUtf8Tokens = true;
  malformedAndNonWhitespaceOptions.rejectReasonNameNonWhitespaceNonAsciiTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=Inconsist\xC3\xA9ntReasonTotal\xC2",
    parsedViolations,
    malformedAndNonWhitespaceOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token);

  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\xC2\xA0\xE3\x80\x80\xC2"
    "Break\x80"
    "Gap\xC2"
    "Split\xC3\xA9Total\xC2\xA0\xE3\x80\x80",
    parsedViolations,
    malformedAndNonWhitespaceOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameMalformedUtf8Token);

  // Control, noncharacter, and CESU-8 surrogate checks.
  SkipDiagnosticsStrictViolationsParseOptions asciiControlOptions;
  asciiControlOptions.rejectReasonNameAsciiControlCharacterTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\x1FTotal",
    parsedViolations,
    asciiControlOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameAsciiControlCharacterToken);

  SkipDiagnosticsStrictViolationsParseOptions unicodeControlOptions;
  unicodeControlOptions.rejectReasonNameNonAsciiUnicodeControlCharacterTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\xE2\x80\x8ETotal",
    parsedViolations,
    unicodeControlOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameNonAsciiUnicodeControlCharacterToken);

  SkipDiagnosticsStrictViolationsParseOptions noncharacterOptions;
  noncharacterOptions.rejectReasonNameUnicodeNoncharacterTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\xEF\xB7\x90Total",
    parsedViolations,
    noncharacterOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameUnicodeNoncharacterToken);

  SkipDiagnosticsStrictViolationsParseOptions loneSurrogateOptions;
  loneSurrogateOptions.rejectReasonNameLoneCesu8SurrogateTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\xED\xA0\x80Total",
    parsedViolations,
    loneSurrogateOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameLoneCesu8SurrogateToken);

  SkipDiagnosticsStrictViolationsParseOptions pairedSurrogateOptions;
  pairedSurrogateOptions.rejectReasonNamePairedCesu8SurrogateTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\xED\xA0\xBD\xED\xB8\x80Total",
    parsedViolations,
    pairedSurrogateOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNamePairedCesu8SurrogateToken);

  SkipDiagnosticsStrictViolationsParseOptions mixedSurrogateOptions;
  mixedSurrogateOptions.rejectReasonNameMixedOrderCesu8SurrogateTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\xED\xB8\x80\xED\xA0\xBDTotal",
    parsedViolations,
    mixedSurrogateOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameMixedOrderCesu8SurrogateToken);

  SkipDiagnosticsStrictViolationsParseOptions sameSurrogateOptions;
  sameSurrogateOptions.rejectReasonNameSameOrderCesu8SurrogateTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReason\xED\xA0\x80\xED\xA0\xBDTotal",
    parsedViolations,
    sameSurrogateOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ReasonNameSameOrderCesu8SurrogateToken);

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
  expectStrictViolationReject(
    nonContiguousButCompletePayload,
    parsedViolations,
    strictIndexOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::NonContiguousViolationIndex,
    3);

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

  SkipDiagnosticsStrictViolationsParseOptions duplicateConflictOptions;
  duplicateConflictOptions.rejectConflictingDuplicateIndices = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.fieldIndex=4;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    duplicateConflictOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::DuplicateViolationConflict,
    2);

  SkipDiagnosticsStrictViolationsParseOptions duplicateRejectOptions;
  duplicateRejectOptions.rejectDuplicateIndices = true;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    duplicateRejectOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::DuplicateViolationEntry,
    2);

  SkipDiagnosticsStrictViolationsParseOptions countOrderOptions;
  countOrderOptions.requireCountFieldBeforeEntries = true;
  expectStrictViolationReject(
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal;"
    "strictViolations.count=1",
    parsedViolations,
    countOrderOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ViolationCountFieldOrder,
    0);

  SkipDiagnosticsStrictViolationsParseOptions zeroPaddedNumericOptions;
  zeroPaddedNumericOptions.rejectZeroPaddedNumericTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=01;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    zeroPaddedNumericOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ZeroPaddedNumericToken,
    0);

  SkipDiagnosticsStrictViolationsParseOptions plusPrefixedNumericOptions;
  plusPrefixedNumericOptions.rejectPlusPrefixedNumericTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=+1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    plusPrefixedNumericOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::PlusPrefixedNumericToken,
    0);

  SkipDiagnosticsStrictViolationsParseOptions minusPrefixedNumericOptions;
  minusPrefixedNumericOptions.rejectMinusPrefixedNumericTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=-1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    minusPrefixedNumericOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::MinusPrefixedNumericToken,
    0);

  SkipDiagnosticsStrictViolationsParseOptions negativeZeroOptions;
  negativeZeroOptions.rejectNegativeZeroTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=-0;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    negativeZeroOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::NegativeZeroNumericToken,
    0);

  SkipDiagnosticsStrictViolationsParseOptions leadingWhitespaceOptions;
  leadingWhitespaceOptions.rejectLeadingAsciiWhitespaceNumericTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=\t1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    leadingWhitespaceOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::LeadingAsciiWhitespaceNumericToken,
    0);

  SkipDiagnosticsStrictViolationsParseOptions trailingWhitespaceOptions;
  trailingWhitespaceOptions.rejectTrailingAsciiWhitespaceNumericTokens = true;
  expectStrictViolationReject(
    "strictViolations.count=1\t;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    trailingWhitespaceOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::TrailingAsciiWhitespaceNumericToken,
    0);

  SkipDiagnosticsStrictViolationsParseOptions countCapOptions;
  countCapOptions.enforceMaxViolationCount = true;
  countCapOptions.maxViolationCount = 1;
  expectStrictViolationReject(
    "strictViolations.count=2;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal;"
    "strictViolations.1.fieldIndex=11;"
    "strictViolations.1.reason=InconsistentMatrixRowTotals",
    parsedViolations,
    countCapOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ViolationCountLimitExceeded,
    0);

  SkipDiagnosticsStrictViolationsParseOptions indexCapOptions;
  indexCapOptions.enforceMaxViolationIndex = true;
  indexCapOptions.maxViolationIndex = 0;
  expectStrictViolationReject(
    "strictViolations.count=2;"
    "strictViolations.1.fieldIndex=11;"
    "strictViolations.1.reason=InconsistentMatrixRowTotals",
    parsedViolations,
    indexCapOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ViolationIndexLimitExceeded,
    1);

  SkipDiagnosticsStrictViolationsParseOptions fieldIndexCapOptions;
  fieldIndexCapOptions.enforceMaxViolationFieldIndex = true;
  fieldIndexCapOptions.maxViolationFieldIndex = 10;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=11;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    fieldIndexCapOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ViolationFieldIndexLimitExceeded,
    1);

  SkipDiagnosticsStrictViolationsParseOptions fieldCapOptions;
  fieldCapOptions.enforceMaxFieldCount = true;
  fieldCapOptions.maxFieldCount = 2;
  expectStrictViolationReject(
    "strictViolations.count=1;"
    "strictViolations.0.fieldIndex=3;"
    "strictViolations.0.reason=InconsistentReasonTotal",
    parsedViolations,
    fieldCapOptions,
    parseError,
    SkipDiagnosticsParseErrorReason::ViolationFieldCountLimitExceeded,
    2);

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

TEST_SUITE_END();
