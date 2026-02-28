#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include "PrimeManifest/text/GlyphBitmapFormat.hpp"

namespace PrimeManifest {

struct Color {
  uint8_t r{};
  uint8_t g{};
  uint8_t b{};
  uint8_t a{};
};

struct IntRect {
  int32_t x0{};
  int32_t y0{};
  int32_t x1{};
  int32_t y1{};
};

enum class BlendType : uint8_t {
  HardUnion = 0,
  SmoothUnion = 1,
};

enum class CommandType : uint8_t {
  Clear = 0,
  Rect = 1,
  Text = 2,
  DebugTiles = 3,
  ClearPattern = 4,
  Circle = 5,
  SetPixel = 6,
  SetPixelA = 7,
  Line = 8,
  Image = 9,
};

constexpr size_t RendererProfileCommandTypeBuckets = static_cast<size_t>(CommandType::Image) + 1u;

constexpr auto commandTypeName(CommandType type) -> std::string_view {
  switch (type) {
    case CommandType::Clear:
      return "Clear";
    case CommandType::Rect:
      return "Rect";
    case CommandType::Text:
      return "Text";
    case CommandType::DebugTiles:
      return "DebugTiles";
    case CommandType::ClearPattern:
      return "ClearPattern";
    case CommandType::Circle:
      return "Circle";
    case CommandType::SetPixel:
      return "SetPixel";
    case CommandType::SetPixelA:
      return "SetPixelA";
    case CommandType::Line:
      return "Line";
    case CommandType::Image:
      return "Image";
  }
  return "UnknownCommandType";
}

constexpr auto commandTypeName(size_t typeIndex) -> std::string_view {
  if (typeIndex >= RendererProfileCommandTypeBuckets) {
    return "OutOfRangeCommandType";
  }
  return commandTypeName(static_cast<CommandType>(typeIndex));
}

constexpr auto commandTypeFromName(std::string_view name, CommandType& out) -> bool {
  for (size_t typeIndex = 0; typeIndex < RendererProfileCommandTypeBuckets; ++typeIndex) {
    if (commandTypeName(typeIndex) == name) {
      out = static_cast<CommandType>(typeIndex);
      return true;
    }
  }
  return false;
}

enum class SkippedCommandReason : uint8_t {
  InvalidTileReference = 0,
  MissingAnalyzedCommand = 1,
  InactiveAnalyzedCommand = 2,
  InvalidLocalBounds = 3,
  InvalidCommandData = 4,
  UnsupportedCommandType = 5,
  OptimizerInvalidCommandData = 6,
  OptimizerCulledByBounds = 7,
  OptimizerCulledByAlpha = 8,
  OptimizerTileStreamInvalidCommandData = 9,
  OptimizerTileStreamCulledByLocalBounds = 10,
};

constexpr size_t SkippedCommandReasonCount = static_cast<size_t>(SkippedCommandReason::OptimizerTileStreamCulledByLocalBounds) + 1u;

constexpr auto skippedCommandReasonName(SkippedCommandReason reason) -> std::string_view {
  switch (reason) {
    case SkippedCommandReason::InvalidTileReference:
      return "InvalidTileReference";
    case SkippedCommandReason::MissingAnalyzedCommand:
      return "MissingAnalyzedCommand";
    case SkippedCommandReason::InactiveAnalyzedCommand:
      return "InactiveAnalyzedCommand";
    case SkippedCommandReason::InvalidLocalBounds:
      return "InvalidLocalBounds";
    case SkippedCommandReason::InvalidCommandData:
      return "InvalidCommandData";
    case SkippedCommandReason::UnsupportedCommandType:
      return "UnsupportedCommandType";
    case SkippedCommandReason::OptimizerInvalidCommandData:
      return "OptimizerInvalidCommandData";
    case SkippedCommandReason::OptimizerCulledByBounds:
      return "OptimizerCulledByBounds";
    case SkippedCommandReason::OptimizerCulledByAlpha:
      return "OptimizerCulledByAlpha";
    case SkippedCommandReason::OptimizerTileStreamInvalidCommandData:
      return "OptimizerTileStreamInvalidCommandData";
    case SkippedCommandReason::OptimizerTileStreamCulledByLocalBounds:
      return "OptimizerTileStreamCulledByLocalBounds";
  }
  return "UnknownSkippedCommandReason";
}

constexpr auto skippedCommandReasonName(size_t reasonIndex) -> std::string_view {
  if (reasonIndex >= SkippedCommandReasonCount) {
    return "OutOfRangeSkippedCommandReason";
  }
  return skippedCommandReasonName(static_cast<SkippedCommandReason>(reasonIndex));
}

constexpr auto skippedCommandReasonFromName(std::string_view name,
                                            SkippedCommandReason& out) -> bool {
  for (size_t reasonIndex = 0; reasonIndex < SkippedCommandReasonCount; ++reasonIndex) {
    if (skippedCommandReasonName(reasonIndex) == name) {
      out = static_cast<SkippedCommandReason>(reasonIndex);
      return true;
    }
  }
  return false;
}

struct SkippedCommandDiagnostics {
  uint64_t total = 0;
  uint64_t unknownType = 0;
  std::array<uint64_t, RendererProfileCommandTypeBuckets> byType{};
  std::array<uint64_t, SkippedCommandReasonCount> byReason{};
  std::array<std::array<uint64_t, SkippedCommandReasonCount>, RendererProfileCommandTypeBuckets> byTypeAndReason{};

  void clear() {
    total = 0;
    unknownType = 0;
    byType.fill(0);
    byReason.fill(0);
    for (auto& bucket : byTypeAndReason) {
      bucket.fill(0);
    }
  }

  void add(CommandType type, SkippedCommandReason reason) {
    size_t typeIndex = static_cast<size_t>(type);
    size_t reasonIndex = static_cast<size_t>(reason);
    if (typeIndex >= byType.size() || reasonIndex >= byReason.size()) {
      addUnknown(reason);
      return;
    }
    ++total;
    ++byType[typeIndex];
    ++byReason[reasonIndex];
    ++byTypeAndReason[typeIndex][reasonIndex];
  }

  void addUnknown(SkippedCommandReason reason) {
    size_t reasonIndex = static_cast<size_t>(reason);
    ++total;
    ++unknownType;
    if (reasonIndex < byReason.size()) {
      ++byReason[reasonIndex];
    }
  }
};

struct CommandTypeCounts {
  uint32_t clearCount = 0;
  uint32_t rect = 0;
  uint32_t circle = 0;
  uint32_t text = 0;
  uint32_t debugTiles = 0;
  uint32_t clearPattern = 0;
  uint32_t setPixel = 0;
  uint32_t setPixelA = 0;
  uint32_t line = 0;
  uint32_t image = 0;

  void reset() {
    clearCount = 0;
    rect = 0;
    circle = 0;
    text = 0;
    debugTiles = 0;
    clearPattern = 0;
    setPixel = 0;
    setPixelA = 0;
    line = 0;
    image = 0;
  }

  uint32_t drawCount() const {
    return rect + circle + text + setPixel + setPixelA + line + image;
  }
};

struct RenderCommand {
  CommandType type{CommandType::Rect};
  uint32_t index = 0;
};

constexpr auto PackRGBA8(Color c) -> uint32_t {
  return static_cast<uint32_t>(c.r) |
         (static_cast<uint32_t>(c.g) << 8) |
         (static_cast<uint32_t>(c.b) << 16) |
         (static_cast<uint32_t>(c.a) << 24);
}

constexpr auto UnpackRGBA8(uint32_t rgba) -> Color {
  return Color{
      static_cast<uint8_t>(rgba & 0xFFu),
      static_cast<uint8_t>((rgba >> 8) & 0xFFu),
      static_cast<uint8_t>((rgba >> 16) & 0xFFu),
      static_cast<uint8_t>((rgba >> 24) & 0xFFu),
  };
}

enum RectFlags : uint8_t {
  RectFlagGradient = 1u << 0,
  RectFlagClip = 1u << 1,
  RectFlagSmoothBlend = 1u << 2,
};

enum TextFlags : uint8_t {
  TextFlagClip = 1u << 0,
};

enum DebugTilesFlags : uint8_t {
  DebugTilesFlagDirtyOnly = 1u << 0,
};

enum ImageFlags : uint8_t {
  ImageFlagWrapU = 1u << 0,
  ImageFlagWrapV = 1u << 1,
  ImageFlagClip = 1u << 2,
};

struct RenderTarget {
  std::span<uint8_t> data;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t strideBytes = 0;
};

struct RenderValidationIssue {
  std::string code;
  std::string detail;
};

struct RenderValidationReport {
  std::vector<RenderValidationIssue> issues;

  void clear() {
    issues.clear();
  }

  bool hasErrors() const {
    return !issues.empty();
  }
};

struct OptimizedBatch;

struct RendererProfile {
  uint64_t renderNs = 0;
  uint64_t buildNs = 0;
  uint64_t premergeNs = 0;
  uint64_t tileWorkNs = 0;
  uint64_t optScanNs = 0;
  uint64_t optTileGridNs = 0;
  uint64_t optTileStreamNs = 0;
  uint64_t optTileBinningNs = 0;
  uint64_t optRenderTilesNs = 0;
  uint64_t optRectCacheNs = 0;
  uint64_t optTextCacheNs = 0;
  uint64_t renderClearNs = 0;
  uint64_t renderTilesNs = 0;
  uint64_t renderDebugNs = 0;
  uint32_t tileCount = 0;
  uint32_t activeTileCount = 0;
  uint32_t commandCount = 0;
  uint64_t renderedTileCount = 0;
  uint64_t renderedCommandCount = 0;
  uint64_t renderedPixelCount = 0;
  uint64_t renderedRectCount = 0;
  uint64_t renderedTextCount = 0;
  uint64_t renderedRectPixels = 0;
  uint64_t renderedTextPixels = 0;
  uint64_t renderedTileBufferPixels = 0;
  SkippedCommandDiagnostics optimizerSkippedCommands;
  SkippedCommandDiagnostics skippedCommands;
  std::vector<uint64_t> workerNs;
  std::vector<uint32_t> workerTiles;

  void clear() {
    renderNs = 0;
    buildNs = 0;
    premergeNs = 0;
    tileWorkNs = 0;
    optScanNs = 0;
    optTileGridNs = 0;
    optTileStreamNs = 0;
    optTileBinningNs = 0;
    optRenderTilesNs = 0;
    optRectCacheNs = 0;
    optTextCacheNs = 0;
    renderClearNs = 0;
    renderTilesNs = 0;
    renderDebugNs = 0;
    tileCount = 0;
    activeTileCount = 0;
    commandCount = 0;
    renderedTileCount = 0;
    renderedCommandCount = 0;
    renderedPixelCount = 0;
    renderedRectCount = 0;
    renderedTextCount = 0;
    renderedRectPixels = 0;
    renderedTextPixels = 0;
    renderedTileBufferPixels = 0;
    optimizerSkippedCommands.clear();
    skippedCommands.clear();
    workerNs.clear();
    workerTiles.clear();
  }
};

enum class SkipDiagnosticsDumpFormat : uint8_t {
  Readable = 0,
  KeyValue = 1,
};

enum class SkipDiagnosticsParseErrorReason : uint8_t {
  None = 0,
  EmptyInput = 1,
  MalformedNonePayload = 2,
  EmptyField = 3,
  MissingEquals = 4,
  EmptyKey = 5,
  EmptyValue = 6,
  InvalidValue = 7,
  UnknownSection = 8,
  UnknownKey = 9,
  UnknownReasonName = 10,
  UnknownTypeName = 11,
  MalformedTypeReasonKey = 12,
  InconsistentReasonTotal = 13,
  InconsistentTypeTotal = 14,
  InconsistentMatrixTotal = 15,
  InconsistentMatrixRowTotals = 16,
  InconsistentMatrixColumnTotals = 17,
  NonContiguousViolationIndex = 18,
  DuplicateViolationConflict = 19,
  DuplicateViolationEntry = 20,
  UnknownReasonFallbackToken = 21,
  ViolationCountLimitExceeded = 22,
  ViolationFieldCountLimitExceeded = 23,
  ViolationIndexLimitExceeded = 24,
  ViolationFieldIndexLimitExceeded = 25,
  DuplicateViolationCountField = 26,
  ViolationCountFieldOrder = 27,
};

constexpr size_t SkipDiagnosticsParseErrorReasonCount =
  static_cast<size_t>(SkipDiagnosticsParseErrorReason::ViolationCountFieldOrder) + 1u;

struct SkipDiagnosticsParseError {
  size_t fieldIndex = 0;
  SkipDiagnosticsParseErrorReason reason = SkipDiagnosticsParseErrorReason::None;
  struct StrictViolation {
    size_t fieldIndex = 0;
    SkipDiagnosticsParseErrorReason reason = SkipDiagnosticsParseErrorReason::None;
  };
  std::vector<StrictViolation> strictViolations{};
};

enum class SkipDiagnosticsParseSectionTarget : uint8_t {
  Both = 0,
  OptimizerOnly = 1,
  RendererOnly = 2,
};

enum class SkipDiagnosticsStrictFailurePrecedence : uint8_t {
  ConsistencyFirst = 0,
  MatrixMarginalsFirst = 1,
};

enum class SkipDiagnosticsStrictFailureMode : uint8_t {
  FirstFailure = 0,
  CollectAll = 1,
};

struct SkipDiagnosticsParseOptions {
  bool strictConsistency = false;
  bool strictMatrixMarginals = false;
  SkipDiagnosticsParseSectionTarget strictSectionTarget = SkipDiagnosticsParseSectionTarget::Both;
  SkipDiagnosticsStrictFailurePrecedence strictFailurePrecedence =
    SkipDiagnosticsStrictFailurePrecedence::ConsistencyFirst;
  SkipDiagnosticsStrictFailureMode strictFailureMode = SkipDiagnosticsStrictFailureMode::FirstFailure;
};

struct SkipDiagnosticsStrictViolationsParseOptions {
  bool enforceContiguousIndices = false;
  bool normalizeOutOfOrderContiguousIndices = false;
  bool rejectConflictingDuplicateIndices = false;
  bool rejectDuplicateIndices = false;
  bool rejectUnknownReasonFallbackToken = false;
  bool enforceMaxViolationCount = false;
  size_t maxViolationCount = 0;
  bool rejectDuplicateCountField = false;
  bool requireCountFieldBeforeEntries = false;
  bool enforceMaxFieldCount = false;
  size_t maxFieldCount = 0;
  bool enforceMaxViolationIndex = false;
  size_t maxViolationIndex = 0;
  bool enforceMaxViolationFieldIndex = false;
  size_t maxViolationFieldIndex = 0;
};

constexpr auto skipDiagnosticsParseErrorReasonName(SkipDiagnosticsParseErrorReason reason) -> std::string_view {
  switch (reason) {
    case SkipDiagnosticsParseErrorReason::None:
      return "None";
    case SkipDiagnosticsParseErrorReason::EmptyInput:
      return "EmptyInput";
    case SkipDiagnosticsParseErrorReason::MalformedNonePayload:
      return "MalformedNonePayload";
    case SkipDiagnosticsParseErrorReason::EmptyField:
      return "EmptyField";
    case SkipDiagnosticsParseErrorReason::MissingEquals:
      return "MissingEquals";
    case SkipDiagnosticsParseErrorReason::EmptyKey:
      return "EmptyKey";
    case SkipDiagnosticsParseErrorReason::EmptyValue:
      return "EmptyValue";
    case SkipDiagnosticsParseErrorReason::InvalidValue:
      return "InvalidValue";
    case SkipDiagnosticsParseErrorReason::UnknownSection:
      return "UnknownSection";
    case SkipDiagnosticsParseErrorReason::UnknownKey:
      return "UnknownKey";
    case SkipDiagnosticsParseErrorReason::UnknownReasonName:
      return "UnknownReasonName";
    case SkipDiagnosticsParseErrorReason::UnknownTypeName:
      return "UnknownTypeName";
    case SkipDiagnosticsParseErrorReason::MalformedTypeReasonKey:
      return "MalformedTypeReasonKey";
    case SkipDiagnosticsParseErrorReason::InconsistentReasonTotal:
      return "InconsistentReasonTotal";
    case SkipDiagnosticsParseErrorReason::InconsistentTypeTotal:
      return "InconsistentTypeTotal";
    case SkipDiagnosticsParseErrorReason::InconsistentMatrixTotal:
      return "InconsistentMatrixTotal";
    case SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals:
      return "InconsistentMatrixRowTotals";
    case SkipDiagnosticsParseErrorReason::InconsistentMatrixColumnTotals:
      return "InconsistentMatrixColumnTotals";
    case SkipDiagnosticsParseErrorReason::NonContiguousViolationIndex:
      return "NonContiguousViolationIndex";
    case SkipDiagnosticsParseErrorReason::DuplicateViolationConflict:
      return "DuplicateViolationConflict";
    case SkipDiagnosticsParseErrorReason::DuplicateViolationEntry:
      return "DuplicateViolationEntry";
    case SkipDiagnosticsParseErrorReason::UnknownReasonFallbackToken:
      return "UnknownReasonFallbackToken";
    case SkipDiagnosticsParseErrorReason::ViolationCountLimitExceeded:
      return "ViolationCountLimitExceeded";
    case SkipDiagnosticsParseErrorReason::ViolationFieldCountLimitExceeded:
      return "ViolationFieldCountLimitExceeded";
    case SkipDiagnosticsParseErrorReason::ViolationIndexLimitExceeded:
      return "ViolationIndexLimitExceeded";
    case SkipDiagnosticsParseErrorReason::ViolationFieldIndexLimitExceeded:
      return "ViolationFieldIndexLimitExceeded";
    case SkipDiagnosticsParseErrorReason::DuplicateViolationCountField:
      return "DuplicateViolationCountField";
    case SkipDiagnosticsParseErrorReason::ViolationCountFieldOrder:
      return "ViolationCountFieldOrder";
  }
  return "UnknownParseErrorReason";
}

constexpr auto skipDiagnosticsParseErrorReasonName(size_t reasonIndex) -> std::string_view {
  if (reasonIndex >= SkipDiagnosticsParseErrorReasonCount) {
    return "OutOfRangeSkipDiagnosticsParseErrorReason";
  }
  return skipDiagnosticsParseErrorReasonName(static_cast<SkipDiagnosticsParseErrorReason>(reasonIndex));
}

constexpr auto skipDiagnosticsParseErrorReasonFromName(std::string_view name,
                                                       SkipDiagnosticsParseErrorReason& out) -> bool {
  for (size_t reasonIndex = 0; reasonIndex < SkipDiagnosticsParseErrorReasonCount; ++reasonIndex) {
    if (skipDiagnosticsParseErrorReasonName(reasonIndex) == name) {
      out = static_cast<SkipDiagnosticsParseErrorReason>(reasonIndex);
      return true;
    }
  }
  return false;
}

inline auto skipDiagnosticsParseStrictViolationsDump(SkipDiagnosticsParseError const& error,
                                                     SkipDiagnosticsDumpFormat format) -> std::string {
  if (error.strictViolations.empty()) {
    return format == SkipDiagnosticsDumpFormat::KeyValue
             ? std::string("strict_violations=none")
             : std::string("strict violations: none");
  }

  if (format == SkipDiagnosticsDumpFormat::KeyValue) {
    std::string out = "strictViolations.count=" + std::to_string(error.strictViolations.size());
    for (size_t violationIndex = 0; violationIndex < error.strictViolations.size(); ++violationIndex) {
      auto const& violation = error.strictViolations[violationIndex];
      out += ";strictViolations.";
      out += std::to_string(violationIndex);
      out += ".fieldIndex=";
      out += std::to_string(violation.fieldIndex);
      out += ";strictViolations.";
      out += std::to_string(violationIndex);
      out += ".reason=";
      out += std::string(skipDiagnosticsParseErrorReasonName(violation.reason));
    }
    return out;
  }

  std::string out = "strict violations(count=" + std::to_string(error.strictViolations.size()) + "): ";
  for (size_t violationIndex = 0; violationIndex < error.strictViolations.size(); ++violationIndex) {
    if (violationIndex != 0) out += ", ";
    auto const& violation = error.strictViolations[violationIndex];
    out += "field[";
    out += std::to_string(violation.fieldIndex);
    out += "]=";
    out += std::string(skipDiagnosticsParseErrorReasonName(violation.reason));
  }
  return out;
}

inline auto skipDiagnosticsParseStrictViolationsDump(SkipDiagnosticsParseError const& error) -> std::string {
  return skipDiagnosticsParseStrictViolationsDump(error, SkipDiagnosticsDumpFormat::Readable);
}

inline void appendKeyValueField(std::string& out,
                                bool& first,
                                std::string_view key,
                                uint64_t value) {
  if (!first) out += ";";
  out += key;
  out += "=";
  out += std::to_string(value);
  first = false;
}

inline auto appendSkippedCommandDiagnosticsSummary(std::string& out,
                                                   std::string_view label,
                                                   SkippedCommandDiagnostics const& diagnostics) -> bool {
  if (diagnostics.total == 0) return false;
  if (!out.empty()) out += '\n';
  out += label;
  out += "(total=";
  out += std::to_string(diagnostics.total);
  if (diagnostics.unknownType != 0) {
    out += ", unknownType=";
    out += std::to_string(diagnostics.unknownType);
  }
  out += ")";

  bool firstBucket = true;
  for (size_t reasonIndex = 0; reasonIndex < diagnostics.byReason.size(); ++reasonIndex) {
    uint64_t count = diagnostics.byReason[reasonIndex];
    if (count == 0) continue;
    out += firstBucket ? ": " : ", ";
    out += skippedCommandReasonName(reasonIndex);
    out += "=";
    out += std::to_string(count);
    firstBucket = false;
  }
  if (firstBucket) out += ": none";
  return true;
}

inline auto appendSkippedCommandDiagnosticsSummaryKeyValue(std::string& out,
                                                           bool& firstField,
                                                           std::string_view label,
                                                           SkippedCommandDiagnostics const& diagnostics) -> bool {
  if (diagnostics.total == 0) return false;
  std::string prefix(label);
  appendKeyValueField(out, firstField, prefix + ".total", diagnostics.total);
  if (diagnostics.unknownType != 0) {
    appendKeyValueField(out, firstField, prefix + ".unknownType", diagnostics.unknownType);
  }
  bool hasReason = false;
  for (size_t reasonIndex = 0; reasonIndex < diagnostics.byReason.size(); ++reasonIndex) {
    uint64_t count = diagnostics.byReason[reasonIndex];
    if (count == 0) continue;
    hasReason = true;
    appendKeyValueField(out, firstField, prefix + ".reason." + std::string(skippedCommandReasonName(reasonIndex)), count);
  }
  if (!hasReason) {
    appendKeyValueField(out, firstField, prefix + ".reason.none", 1);
  }
  return true;
}

inline auto rendererProfileSkipDiagnosticsDump(RendererProfile const& profile,
                                               SkipDiagnosticsDumpFormat format) -> std::string {
  if (format == SkipDiagnosticsDumpFormat::KeyValue) {
    std::string out;
    bool firstField = true;
    appendSkippedCommandDiagnosticsSummaryKeyValue(out, firstField, "optimizerSkippedCommands", profile.optimizerSkippedCommands);
    appendSkippedCommandDiagnosticsSummaryKeyValue(out, firstField, "skippedCommands", profile.skippedCommands);
    if (out.empty()) return "skip_diagnostics=none";
    return out;
  }

  std::string out;
  appendSkippedCommandDiagnosticsSummary(out, "optimizerSkippedCommands", profile.optimizerSkippedCommands);
  appendSkippedCommandDiagnosticsSummary(out, "skippedCommands", profile.skippedCommands);
  if (out.empty()) return "skip diagnostics: none";
  return out;
}

inline auto appendSkippedCommandTypeSummary(std::string& out,
                                            std::string_view label,
                                            SkippedCommandDiagnostics const& diagnostics) -> bool {
  bool firstBucket = true;
  for (size_t typeIndex = 0; typeIndex < diagnostics.byType.size(); ++typeIndex) {
    uint64_t count = diagnostics.byType[typeIndex];
    if (count == 0) continue;
    if (firstBucket) {
      if (!out.empty()) out += '\n';
      out += label;
      out += ".byType: ";
    } else {
      out += ", ";
    }
    out += commandTypeName(typeIndex);
    out += "=";
    out += std::to_string(count);
    firstBucket = false;
  }
  return !firstBucket;
}

inline auto appendSkippedCommandTypeSummaryKeyValue(std::string& out,
                                                    bool& firstField,
                                                    std::string_view label,
                                                    SkippedCommandDiagnostics const& diagnostics) -> bool {
  bool hasType = false;
  std::string prefix(label);
  for (size_t typeIndex = 0; typeIndex < diagnostics.byType.size(); ++typeIndex) {
    uint64_t count = diagnostics.byType[typeIndex];
    if (count == 0) continue;
    hasType = true;
    appendKeyValueField(out, firstField, prefix + ".type." + std::string(commandTypeName(typeIndex)), count);
  }
  return hasType;
}

inline auto appendSkippedCommandTypeReasonMatrixSummary(std::string& out,
                                                        std::string_view label,
                                                        SkippedCommandDiagnostics const& diagnostics) -> bool {
  bool firstBucket = true;
  for (size_t typeIndex = 0; typeIndex < diagnostics.byTypeAndReason.size(); ++typeIndex) {
    for (size_t reasonIndex = 0; reasonIndex < diagnostics.byTypeAndReason[typeIndex].size(); ++reasonIndex) {
      uint64_t count = diagnostics.byTypeAndReason[typeIndex][reasonIndex];
      if (count == 0) continue;
      if (firstBucket) {
        if (!out.empty()) out += '\n';
        out += label;
        out += ".byTypeAndReason: ";
      } else {
        out += ", ";
      }
      out += commandTypeName(typeIndex);
      out += "/";
      out += skippedCommandReasonName(reasonIndex);
      out += "=";
      out += std::to_string(count);
      firstBucket = false;
    }
  }
  return !firstBucket;
}

inline auto appendSkippedCommandTypeReasonMatrixSummaryKeyValue(std::string& out,
                                                                bool& firstField,
                                                                std::string_view label,
                                                                SkippedCommandDiagnostics const& diagnostics) -> bool {
  bool hasMatrix = false;
  std::string prefix(label);
  for (size_t typeIndex = 0; typeIndex < diagnostics.byTypeAndReason.size(); ++typeIndex) {
    for (size_t reasonIndex = 0; reasonIndex < diagnostics.byTypeAndReason[typeIndex].size(); ++reasonIndex) {
      uint64_t count = diagnostics.byTypeAndReason[typeIndex][reasonIndex];
      if (count == 0) continue;
      hasMatrix = true;
      appendKeyValueField(out,
                          firstField,
                          prefix + ".typeReason." + std::string(commandTypeName(typeIndex)) + "." +
                            std::string(skippedCommandReasonName(reasonIndex)),
                          count);
    }
  }
  return hasMatrix;
}

inline auto rendererProfileSkipDiagnosticsDumpVerbose(RendererProfile const& profile,
                                                      SkipDiagnosticsDumpFormat format) -> std::string {
  if (format == SkipDiagnosticsDumpFormat::KeyValue) {
    std::string out;
    bool firstField = true;
    appendSkippedCommandDiagnosticsSummaryKeyValue(out, firstField, "optimizerSkippedCommands", profile.optimizerSkippedCommands);
    appendSkippedCommandDiagnosticsSummaryKeyValue(out, firstField, "skippedCommands", profile.skippedCommands);
    appendSkippedCommandTypeSummaryKeyValue(out, firstField, "optimizerSkippedCommands", profile.optimizerSkippedCommands);
    appendSkippedCommandTypeSummaryKeyValue(out, firstField, "skippedCommands", profile.skippedCommands);
    appendSkippedCommandTypeReasonMatrixSummaryKeyValue(out, firstField, "optimizerSkippedCommands", profile.optimizerSkippedCommands);
    appendSkippedCommandTypeReasonMatrixSummaryKeyValue(out, firstField, "skippedCommands", profile.skippedCommands);
    if (out.empty()) return "skip_diagnostics=none";
    return out;
  }

  std::string out;
  appendSkippedCommandDiagnosticsSummary(out, "optimizerSkippedCommands", profile.optimizerSkippedCommands);
  appendSkippedCommandDiagnosticsSummary(out, "skippedCommands", profile.skippedCommands);
  appendSkippedCommandTypeSummary(out, "optimizerSkippedCommands", profile.optimizerSkippedCommands);
  appendSkippedCommandTypeSummary(out, "skippedCommands", profile.skippedCommands);
  appendSkippedCommandTypeReasonMatrixSummary(out, "optimizerSkippedCommands", profile.optimizerSkippedCommands);
  appendSkippedCommandTypeReasonMatrixSummary(out, "skippedCommands", profile.skippedCommands);
  if (out.empty()) return "skip diagnostics: none";
  return out;
}

inline auto rendererProfileSkipDiagnosticsDump(RendererProfile const& profile) -> std::string {
  return rendererProfileSkipDiagnosticsDump(profile, SkipDiagnosticsDumpFormat::Readable);
}

inline auto rendererProfileSkipDiagnosticsDumpVerbose(RendererProfile const& profile) -> std::string {
  return rendererProfileSkipDiagnosticsDumpVerbose(profile, SkipDiagnosticsDumpFormat::Readable);
}

inline auto parseUnsignedDecimal(std::string_view text, uint64_t& out) -> bool {
  if (text.empty()) return false;
  uint64_t value = 0;
  for (char c : text) {
    if (c < '0' || c > '9') return false;
    uint64_t digit = static_cast<uint64_t>(c - '0');
    if (value > (std::numeric_limits<uint64_t>::max() - digit) / 10u) return false;
    value = value * 10u + digit;
  }
  out = value;
  return true;
}

inline void clearSkipDiagnosticsParseError(SkipDiagnosticsParseError* errorOut) {
  if (!errorOut) return;
  errorOut->fieldIndex = 0;
  errorOut->reason = SkipDiagnosticsParseErrorReason::None;
  errorOut->strictViolations.clear();
}

inline auto failSkipDiagnosticsParse(SkipDiagnosticsParseError* errorOut,
                                     size_t fieldIndex,
                                     SkipDiagnosticsParseErrorReason reason) -> bool {
  if (errorOut) {
    errorOut->fieldIndex = fieldIndex;
    errorOut->reason = reason;
  }
  return false;
}

inline auto reportSkipDiagnosticsStrictFailure(SkipDiagnosticsParseError* errorOut,
                                               size_t fieldIndex,
                                               SkipDiagnosticsParseErrorReason reason,
                                               bool collectAll,
                                               bool& hasStrictFailure) -> bool {
  hasStrictFailure = true;
  if (errorOut && errorOut->reason == SkipDiagnosticsParseErrorReason::None) {
    errorOut->fieldIndex = fieldIndex;
    errorOut->reason = reason;
  }
  if (collectAll) {
    if (errorOut) {
      errorOut->strictViolations.push_back({fieldIndex, reason});
    }
    return true;
  }
  return false;
}

inline auto parseSkipDiagnosticsStrictViolationsKeyValue(
  std::string_view dump,
  std::vector<SkipDiagnosticsParseError::StrictViolation>& violationsOut,
  SkipDiagnosticsStrictViolationsParseOptions const& options,
  SkipDiagnosticsParseError* errorOut) -> bool {
  violationsOut.clear();
  clearSkipDiagnosticsParseError(errorOut);
  if (dump == "strict_violations=none") return true;
  if (dump.empty()) return failSkipDiagnosticsParse(errorOut, 0, SkipDiagnosticsParseErrorReason::EmptyInput);
  if (dump.starts_with("strict_violations=none")) {
    return failSkipDiagnosticsParse(errorOut, 0, SkipDiagnosticsParseErrorReason::MalformedNonePayload);
  }

  struct PendingStrictViolation {
    bool hasFieldIndex = false;
    size_t fieldIndex = 0;
    bool hasReason = false;
    SkipDiagnosticsParseErrorReason reason = SkipDiagnosticsParseErrorReason::None;
  };

  constexpr std::string_view StrictViolationPrefix = "strictViolations.";
  std::vector<PendingStrictViolation> pendingViolations;
  std::vector<bool> seenViolationIndices;
  size_t nextContiguousViolationIndex = 0;
  bool enforceContiguousArrival =
    options.enforceContiguousIndices && !options.normalizeOutOfOrderContiguousIndices;
  bool normalizeOutOfOrderContiguousIndices = options.normalizeOutOfOrderContiguousIndices;
  uint64_t expectedCount = 0;
  bool hasExpectedCount = false;

  size_t start = 0;
  size_t fieldIndex = 0;
  size_t parsedFieldCount = 0;
  while (start <= dump.size()) {
    if (options.enforceMaxFieldCount &&
        parsedFieldCount >= options.maxFieldCount) {
      return failSkipDiagnosticsParse(errorOut,
                                      fieldIndex,
                                      SkipDiagnosticsParseErrorReason::ViolationFieldCountLimitExceeded);
    }

    size_t end = dump.find(';', start);
    if (end == std::string_view::npos) end = dump.size();
    if (end <= start) return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::EmptyField);
    std::string_view field = dump.substr(start, end - start);

    size_t equals = field.find('=');
    if (equals == std::string_view::npos) {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::MissingEquals);
    }
    if (equals == 0) {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::EmptyKey);
    }
    if (equals + 1 >= field.size()) {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::EmptyValue);
    }
    std::string_view key = field.substr(0, equals);
    std::string_view valueText = field.substr(equals + 1);

    if (key == "strictViolations.count") {
      if (options.rejectDuplicateCountField &&
          hasExpectedCount) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::DuplicateViolationCountField);
      }
      if (!parseUnsignedDecimal(valueText, expectedCount)) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::InvalidValue);
      }
      if (options.enforceMaxViolationCount &&
          expectedCount > static_cast<uint64_t>(options.maxViolationCount)) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::ViolationCountLimitExceeded);
      }
      hasExpectedCount = true;
    } else if (key.starts_with(StrictViolationPrefix)) {
      if (options.requireCountFieldBeforeEntries &&
          !hasExpectedCount) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::ViolationCountFieldOrder);
      }
      std::string_view tail = key.substr(StrictViolationPrefix.size());
      size_t separator = tail.find('.');
      if (separator == std::string_view::npos || separator == 0 || separator + 1 >= tail.size()) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownKey);
      }

      std::string_view violationIndexText = tail.substr(0, separator);
      std::string_view leafKey = tail.substr(separator + 1);

      uint64_t violationIndex64 = 0;
      if (!parseUnsignedDecimal(violationIndexText, violationIndex64)) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownKey);
      }
      if (violationIndex64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::InvalidValue);
      }
      if (options.enforceMaxViolationIndex &&
          violationIndex64 > static_cast<uint64_t>(options.maxViolationIndex)) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::ViolationIndexLimitExceeded);
      }
      if (options.enforceMaxViolationCount &&
          violationIndex64 >= static_cast<uint64_t>(options.maxViolationCount)) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::ViolationCountLimitExceeded);
      }
      if (hasExpectedCount && violationIndex64 >= expectedCount) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::InvalidValue);
      }
      size_t violationIndex = static_cast<size_t>(violationIndex64);
      if (seenViolationIndices.size() <= violationIndex) {
        seenViolationIndices.resize(violationIndex + 1u, false);
      }
      if (enforceContiguousArrival && !seenViolationIndices[violationIndex]) {
        if (violationIndex != nextContiguousViolationIndex) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::NonContiguousViolationIndex);
        }
        nextContiguousViolationIndex += 1u;
      }
      seenViolationIndices[violationIndex] = true;
      if (pendingViolations.size() <= violationIndex) {
        pendingViolations.resize(violationIndex + 1u);
      }

      if (leafKey == "fieldIndex") {
        uint64_t parsedFieldIndex = 0;
        if (!parseUnsignedDecimal(valueText, parsedFieldIndex)) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::InvalidValue);
        }
        if (parsedFieldIndex > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::InvalidValue);
        }
        if (options.enforceMaxViolationFieldIndex &&
            parsedFieldIndex > static_cast<uint64_t>(options.maxViolationFieldIndex)) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::ViolationFieldIndexLimitExceeded);
        }
        size_t parsedFieldValue = static_cast<size_t>(parsedFieldIndex);
        if (options.rejectDuplicateIndices &&
            pendingViolations[violationIndex].hasFieldIndex) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::DuplicateViolationEntry);
        }
        if (options.rejectConflictingDuplicateIndices &&
            pendingViolations[violationIndex].hasFieldIndex &&
            pendingViolations[violationIndex].fieldIndex != parsedFieldValue) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::DuplicateViolationConflict);
        }
        pendingViolations[violationIndex].fieldIndex = parsedFieldValue;
        pendingViolations[violationIndex].hasFieldIndex = true;
      } else if (leafKey == "reason") {
        SkipDiagnosticsParseErrorReason parsedReason{};
        if (valueText == "UnknownParseErrorReason") {
          if (options.rejectUnknownReasonFallbackToken) {
            return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownReasonFallbackToken);
          }
          parsedReason = static_cast<SkipDiagnosticsParseErrorReason>(255);
        } else if (!skipDiagnosticsParseErrorReasonFromName(valueText, parsedReason)) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownReasonName);
        }
        if (options.rejectDuplicateIndices &&
            pendingViolations[violationIndex].hasReason) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::DuplicateViolationEntry);
        }
        if (options.rejectConflictingDuplicateIndices &&
            pendingViolations[violationIndex].hasReason &&
            pendingViolations[violationIndex].reason != parsedReason) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::DuplicateViolationConflict);
        }
        pendingViolations[violationIndex].reason = parsedReason;
        pendingViolations[violationIndex].hasReason = true;
      } else {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownKey);
      }
    } else {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownKey);
    }
    parsedFieldCount += 1;

    if (end == dump.size()) break;
    start = end + 1;
    fieldIndex += 1;
  }

  if (!hasExpectedCount) {
    return failSkipDiagnosticsParse(errorOut, parsedFieldCount, SkipDiagnosticsParseErrorReason::UnknownKey);
  }
  if (expectedCount != pendingViolations.size()) {
    return failSkipDiagnosticsParse(errorOut, parsedFieldCount, SkipDiagnosticsParseErrorReason::InvalidValue);
  }
  if (normalizeOutOfOrderContiguousIndices) {
    if (seenViolationIndices.size() < pendingViolations.size()) {
      return failSkipDiagnosticsParse(errorOut,
                                      parsedFieldCount,
                                      SkipDiagnosticsParseErrorReason::NonContiguousViolationIndex);
    }
    for (size_t violationIndex = 0; violationIndex < pendingViolations.size(); ++violationIndex) {
      if (!seenViolationIndices[violationIndex]) {
        return failSkipDiagnosticsParse(errorOut,
                                        parsedFieldCount + violationIndex,
                                        SkipDiagnosticsParseErrorReason::NonContiguousViolationIndex);
      }
    }
  }
  violationsOut.reserve(pendingViolations.size());
  for (size_t violationIndex = 0; violationIndex < pendingViolations.size(); ++violationIndex) {
    auto const& pending = pendingViolations[violationIndex];
    if (!pending.hasFieldIndex || !pending.hasReason) {
      return failSkipDiagnosticsParse(
        errorOut,
        parsedFieldCount + violationIndex,
        SkipDiagnosticsParseErrorReason::UnknownKey);
    }
    violationsOut.push_back({pending.fieldIndex, pending.reason});
  }
  return true;
}

inline auto parseSkipDiagnosticsStrictViolationsKeyValue(
  std::string_view dump,
  std::vector<SkipDiagnosticsParseError::StrictViolation>& violationsOut,
  SkipDiagnosticsStrictViolationsParseOptions const& options) -> bool {
  return parseSkipDiagnosticsStrictViolationsKeyValue(dump, violationsOut, options, nullptr);
}

inline auto parseSkipDiagnosticsStrictViolationsKeyValue(
  std::string_view dump,
  std::vector<SkipDiagnosticsParseError::StrictViolation>& violationsOut,
  SkipDiagnosticsParseError* errorOut) -> bool {
  return parseSkipDiagnosticsStrictViolationsKeyValue(
    dump,
    violationsOut,
    SkipDiagnosticsStrictViolationsParseOptions{},
    errorOut);
}

inline auto parseSkipDiagnosticsStrictViolationsKeyValue(
  std::string_view dump,
  std::vector<SkipDiagnosticsParseError::StrictViolation>& violationsOut) -> bool {
  return parseSkipDiagnosticsStrictViolationsKeyValue(
    dump,
    violationsOut,
    SkipDiagnosticsStrictViolationsParseOptions{},
    nullptr);
}

inline auto sumSkipReasonBuckets(SkippedCommandDiagnostics const& diagnostics) -> uint64_t {
  uint64_t sum = 0;
  for (uint64_t count : diagnostics.byReason) {
    sum += count;
  }
  return sum;
}

inline auto sumSkipTypeBuckets(SkippedCommandDiagnostics const& diagnostics) -> uint64_t {
  uint64_t sum = 0;
  for (uint64_t count : diagnostics.byType) {
    sum += count;
  }
  return sum;
}

inline auto sumSkipTypeReasonBuckets(SkippedCommandDiagnostics const& diagnostics) -> uint64_t {
  uint64_t sum = 0;
  for (auto const& row : diagnostics.byTypeAndReason) {
    for (uint64_t count : row) {
      sum += count;
    }
  }
  return sum;
}

inline auto validateSkipDiagnosticsConsistency(SkippedCommandDiagnostics const& diagnostics,
                                               size_t fieldIndex,
                                               bool collectAll,
                                               bool& hasStrictFailure,
                                               SkipDiagnosticsParseError* errorOut) -> bool {
  uint64_t reasonSum = sumSkipReasonBuckets(diagnostics);
  if (reasonSum != diagnostics.total) {
    if (!reportSkipDiagnosticsStrictFailure(errorOut,
                                            fieldIndex,
                                            SkipDiagnosticsParseErrorReason::InconsistentReasonTotal,
                                            collectAll,
                                            hasStrictFailure)) {
      return false;
    }
  }
  uint64_t typeSum = sumSkipTypeBuckets(diagnostics);
  if (typeSum + diagnostics.unknownType != diagnostics.total) {
    if (!reportSkipDiagnosticsStrictFailure(errorOut,
                                            fieldIndex,
                                            SkipDiagnosticsParseErrorReason::InconsistentTypeTotal,
                                            collectAll,
                                            hasStrictFailure)) {
      return false;
    }
  }
  uint64_t matrixSum = sumSkipTypeReasonBuckets(diagnostics);
  if (matrixSum != typeSum) {
    if (!reportSkipDiagnosticsStrictFailure(errorOut,
                                            fieldIndex,
                                            SkipDiagnosticsParseErrorReason::InconsistentMatrixTotal,
                                            collectAll,
                                            hasStrictFailure)) {
      return false;
    }
  }
  return true;
}

inline auto validateSkipDiagnosticsMatrixMarginals(SkippedCommandDiagnostics const& diagnostics,
                                                   size_t fieldIndexBase,
                                                   bool collectAll,
                                                   bool& hasStrictFailure,
                                                   SkipDiagnosticsParseError* errorOut) -> bool {
  for (size_t typeIndex = 0; typeIndex < diagnostics.byTypeAndReason.size(); ++typeIndex) {
    uint64_t rowSum = 0;
    for (uint64_t count : diagnostics.byTypeAndReason[typeIndex]) {
      rowSum += count;
    }
    if (rowSum != diagnostics.byType[typeIndex]) {
      if (!reportSkipDiagnosticsStrictFailure(errorOut,
                                              fieldIndexBase + typeIndex,
                                              SkipDiagnosticsParseErrorReason::InconsistentMatrixRowTotals,
                                              collectAll,
                                              hasStrictFailure)) {
        return false;
      }
    }
  }

  uint64_t unknownByReason = 0;
  size_t columnFieldBase = fieldIndexBase + RendererProfileCommandTypeBuckets;
  for (size_t reasonIndex = 0; reasonIndex < SkippedCommandReasonCount; ++reasonIndex) {
    uint64_t columnSum = 0;
    for (size_t typeIndex = 0; typeIndex < diagnostics.byTypeAndReason.size(); ++typeIndex) {
      columnSum += diagnostics.byTypeAndReason[typeIndex][reasonIndex];
    }
    uint64_t reasonTotal = diagnostics.byReason[reasonIndex];
    if (columnSum > reasonTotal) {
      if (!reportSkipDiagnosticsStrictFailure(errorOut,
                                              columnFieldBase + reasonIndex,
                                              SkipDiagnosticsParseErrorReason::InconsistentMatrixColumnTotals,
                                              collectAll,
                                              hasStrictFailure)) {
        return false;
      }
    }
    unknownByReason += reasonTotal - columnSum;
  }
  if (unknownByReason != diagnostics.unknownType) {
    if (!reportSkipDiagnosticsStrictFailure(errorOut,
                                            columnFieldBase + SkippedCommandReasonCount,
                                            SkipDiagnosticsParseErrorReason::InconsistentMatrixColumnTotals,
                                            collectAll,
                                            hasStrictFailure)) {
      return false;
    }
  }
  return true;
}

constexpr auto shouldValidateOptimizerSection(SkipDiagnosticsParseSectionTarget target) -> bool {
  return target == SkipDiagnosticsParseSectionTarget::Both ||
         target == SkipDiagnosticsParseSectionTarget::OptimizerOnly;
}

constexpr auto shouldValidateRendererSection(SkipDiagnosticsParseSectionTarget target) -> bool {
  return target == SkipDiagnosticsParseSectionTarget::Both ||
         target == SkipDiagnosticsParseSectionTarget::RendererOnly;
}

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        SkippedCommandDiagnostics& optimizerOut,
                                                        SkippedCommandDiagnostics& skippedOut,
                                                        SkipDiagnosticsParseOptions const& options,
                                                        SkipDiagnosticsParseError* errorOut) -> bool {
  optimizerOut.clear();
  skippedOut.clear();
  clearSkipDiagnosticsParseError(errorOut);
  if (dump == "skip_diagnostics=none") return true;
  if (dump.empty()) return failSkipDiagnosticsParse(errorOut, 0, SkipDiagnosticsParseErrorReason::EmptyInput);
  if (dump.starts_with("skip_diagnostics=none")) {
    return failSkipDiagnosticsParse(errorOut, 0, SkipDiagnosticsParseErrorReason::MalformedNonePayload);
  }

  constexpr std::string_view OptimizerPrefix = "optimizerSkippedCommands.";
  constexpr std::string_view RendererPrefix = "skippedCommands.";

  size_t start = 0;
  size_t fieldIndex = 0;
  size_t parsedFieldCount = 0;
  while (start <= dump.size()) {
    size_t end = dump.find(';', start);
    if (end == std::string_view::npos) end = dump.size();
    if (end <= start) return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::EmptyField);
    std::string_view field = dump.substr(start, end - start);

    size_t equals = field.find('=');
    if (equals == std::string_view::npos) {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::MissingEquals);
    }
    if (equals == 0) {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::EmptyKey);
    }
    if (equals + 1 >= field.size()) {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::EmptyValue);
    }
    std::string_view key = field.substr(0, equals);
    std::string_view valueText = field.substr(equals + 1);
    uint64_t value = 0;
    if (!parseUnsignedDecimal(valueText, value)) {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::InvalidValue);
    }

    SkippedCommandDiagnostics* diagnostics = nullptr;
    std::string_view tail;
    if (key.starts_with(OptimizerPrefix)) {
      diagnostics = &optimizerOut;
      tail = key.substr(OptimizerPrefix.size());
    } else if (key.starts_with(RendererPrefix)) {
      diagnostics = &skippedOut;
      tail = key.substr(RendererPrefix.size());
    } else {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownSection);
    }

    if (tail == "total") {
      diagnostics->total = value;
    } else if (tail == "unknownType") {
      diagnostics->unknownType = value;
    } else if (tail.starts_with("reason.")) {
      std::string_view reasonName = tail.substr(7);
      if (reasonName == "none") {
        // Marker from compact dump when totals exist but reason buckets are all zero.
      } else {
        SkippedCommandReason reason{};
        if (!skippedCommandReasonFromName(reasonName, reason)) {
          return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownReasonName);
        }
        diagnostics->byReason[static_cast<size_t>(reason)] = value;
      }
    } else if (tail.starts_with("type.")) {
      std::string_view typeName = tail.substr(5);
      CommandType type{};
      if (!commandTypeFromName(typeName, type)) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownTypeName);
      }
      diagnostics->byType[static_cast<size_t>(type)] = value;
    } else if (tail.starts_with("typeReason.")) {
      std::string_view pair = tail.substr(11);
      size_t separator = pair.find('.');
      if (separator == std::string_view::npos || separator == 0 || separator + 1 >= pair.size()) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::MalformedTypeReasonKey);
      }
      std::string_view typeName = pair.substr(0, separator);
      std::string_view reasonName = pair.substr(separator + 1);
      CommandType type{};
      SkippedCommandReason reason{};
      if (!commandTypeFromName(typeName, type)) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownTypeName);
      }
      if (!skippedCommandReasonFromName(reasonName, reason)) {
        return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownReasonName);
      }
      diagnostics->byTypeAndReason[static_cast<size_t>(type)][static_cast<size_t>(reason)] = value;
    } else {
      return failSkipDiagnosticsParse(errorOut, fieldIndex, SkipDiagnosticsParseErrorReason::UnknownKey);
    }
    parsedFieldCount += 1;

    if (end == dump.size()) break;
    start = end + 1;
    fieldIndex += 1;
  }
  bool hasStrictFailure = false;

  auto runStrictConsistencyChecks = [&]() -> bool {
    if (!options.strictConsistency) return true;
    bool collectAll = options.strictFailureMode == SkipDiagnosticsStrictFailureMode::CollectAll;
    size_t consistencyFieldIndex = parsedFieldCount;
    if (shouldValidateOptimizerSection(options.strictSectionTarget)) {
      if (!validateSkipDiagnosticsConsistency(optimizerOut, consistencyFieldIndex, collectAll, hasStrictFailure, errorOut)) {
        return false;
      }
    }
    if (shouldValidateRendererSection(options.strictSectionTarget)) {
      if (!validateSkipDiagnosticsConsistency(skippedOut, consistencyFieldIndex, collectAll, hasStrictFailure, errorOut)) {
        return false;
      }
    }
    return true;
  };

  auto runStrictMatrixMarginalChecks = [&]() -> bool {
    if (!options.strictMatrixMarginals) return true;
    bool collectAll = options.strictFailureMode == SkipDiagnosticsStrictFailureMode::CollectAll;
    size_t matrixFieldIndex = parsedFieldCount;
    constexpr size_t PerSectionFieldSpan = RendererProfileCommandTypeBuckets + SkippedCommandReasonCount + 1u;
    if (shouldValidateOptimizerSection(options.strictSectionTarget)) {
      if (!validateSkipDiagnosticsMatrixMarginals(optimizerOut, matrixFieldIndex, collectAll, hasStrictFailure, errorOut)) {
        return false;
      }
    }
    if (shouldValidateRendererSection(options.strictSectionTarget)) {
      if (!validateSkipDiagnosticsMatrixMarginals(
            skippedOut,
            matrixFieldIndex + PerSectionFieldSpan,
            collectAll,
            hasStrictFailure,
            errorOut)) {
        return false;
      }
    }
    return true;
  };

  if (options.strictConsistency && options.strictMatrixMarginals &&
      options.strictFailurePrecedence == SkipDiagnosticsStrictFailurePrecedence::MatrixMarginalsFirst) {
    if (!runStrictMatrixMarginalChecks()) return false;
    if (!runStrictConsistencyChecks()) return false;
    return !hasStrictFailure;
  }

  if (!runStrictConsistencyChecks()) return false;
  if (!runStrictMatrixMarginalChecks()) return false;
  if (hasStrictFailure) return false;
  return true;
}

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        SkippedCommandDiagnostics& optimizerOut,
                                                        SkippedCommandDiagnostics& skippedOut,
                                                        SkipDiagnosticsParseError* errorOut) -> bool {
  return parseRendererProfileSkipDiagnosticsKeyValue(
    dump,
    optimizerOut,
    skippedOut,
    SkipDiagnosticsParseOptions{},
    errorOut);
}

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        SkippedCommandDiagnostics& optimizerOut,
                                                        SkippedCommandDiagnostics& skippedOut) -> bool {
  return parseRendererProfileSkipDiagnosticsKeyValue(
    dump,
    optimizerOut,
    skippedOut,
    SkipDiagnosticsParseOptions{},
    nullptr);
}

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        SkippedCommandDiagnostics& optimizerOut,
                                                        SkippedCommandDiagnostics& skippedOut,
                                                        SkipDiagnosticsParseOptions const& options) -> bool {
  return parseRendererProfileSkipDiagnosticsKeyValue(dump, optimizerOut, skippedOut, options, nullptr);
}

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        RendererProfile& profileOut,
                                                        SkipDiagnosticsParseOptions const& options,
                                                        SkipDiagnosticsParseError* errorOut) -> bool {
  return parseRendererProfileSkipDiagnosticsKeyValue(dump,
                                                     profileOut.optimizerSkippedCommands,
                                                     profileOut.skippedCommands,
                                                     options,
                                                     errorOut);
}

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        RendererProfile& profileOut,
                                                        SkipDiagnosticsParseError* errorOut) -> bool {
  return parseRendererProfileSkipDiagnosticsKeyValue(dump, profileOut, SkipDiagnosticsParseOptions{}, errorOut);
}

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        RendererProfile& profileOut) -> bool {
  return parseRendererProfileSkipDiagnosticsKeyValue(dump, profileOut, SkipDiagnosticsParseOptions{}, nullptr);
}

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        RendererProfile& profileOut,
                                                        SkipDiagnosticsParseOptions const& options) -> bool {
  return parseRendererProfileSkipDiagnosticsKeyValue(dump, profileOut, options, nullptr);
}

struct ClearStore {
  std::vector<uint8_t> colorIndex;

  void clear() {
    colorIndex.clear();
  }
  size_t size() const {
    return colorIndex.size();
  }
};

struct ClearPatternStore {
  std::vector<uint16_t> width;
  std::vector<uint16_t> height;
  std::vector<uint32_t> dataOffset;
  std::vector<uint8_t> data;

  void clear() {
    width.clear();
    height.clear();
    dataOffset.clear();
    data.clear();
  }
  size_t size() const {
    return width.size();
  }
};

struct RectStore {
  std::vector<int16_t> x0;
  std::vector<int16_t> y0;
  std::vector<int16_t> x1;
  std::vector<int16_t> y1;
  std::vector<uint8_t> colorIndex;
  std::vector<uint16_t> radiusQ8_8;
  std::vector<int16_t> rotationQ8_8;
  std::vector<int16_t> zQ8_8;
  std::vector<uint8_t> opacity;
  std::vector<uint8_t> flags;
  std::vector<uint8_t> gradientColor1Index;
  std::vector<int16_t> gradientDirX;
  std::vector<int16_t> gradientDirY;
  std::vector<int16_t> clipX0;
  std::vector<int16_t> clipY0;
  std::vector<int16_t> clipX1;
  std::vector<int16_t> clipY1;

  void clear() {
    x0.clear();
    y0.clear();
    x1.clear();
    y1.clear();
    colorIndex.clear();
    radiusQ8_8.clear();
    rotationQ8_8.clear();
    zQ8_8.clear();
    opacity.clear();
    flags.clear();
    gradientColor1Index.clear();
    gradientDirX.clear();
    gradientDirY.clear();
    clipX0.clear();
    clipY0.clear();
    clipX1.clear();
    clipY1.clear();
  }
  size_t size() const {
    return x0.size();
  }
};

struct CircleStore {
  std::vector<int16_t> centerX;
  std::vector<int16_t> centerY;
  std::vector<uint16_t> radius;
  std::vector<uint8_t> colorIndex;

  void clear() {
    centerX.clear();
    centerY.clear();
    radius.clear();
    colorIndex.clear();
  }
  size_t size() const {
    return centerX.size();
  }
};

struct PixelStore {
  std::vector<int16_t> x;
  std::vector<int16_t> y;
  std::vector<uint8_t> colorIndex;

  void clear() {
    x.clear();
    y.clear();
    colorIndex.clear();
  }
  size_t size() const {
    return x.size();
  }
};

struct PixelAStore {
  std::vector<int16_t> x;
  std::vector<int16_t> y;
  std::vector<uint8_t> colorIndex;
  std::vector<uint8_t> alpha;

  void clear() {
    x.clear();
    y.clear();
    colorIndex.clear();
    alpha.clear();
  }
  size_t size() const {
    return x.size();
  }
};

struct LineStore {
  std::vector<int16_t> x0;
  std::vector<int16_t> y0;
  std::vector<int16_t> x1;
  std::vector<int16_t> y1;
  std::vector<uint16_t> widthQ8_8;
  std::vector<uint8_t> colorIndex;
  std::vector<uint8_t> opacity;

  void clear() {
    x0.clear();
    y0.clear();
    x1.clear();
    y1.clear();
    widthQ8_8.clear();
    colorIndex.clear();
    opacity.clear();
  }
  size_t size() const {
    return x0.size();
  }
};

struct ImageStore {
  std::vector<uint16_t> width;
  std::vector<uint16_t> height;
  std::vector<uint32_t> strideBytes;
  std::vector<uint32_t> dataOffset;
  std::vector<uint8_t> data;

  void clear() {
    width.clear();
    height.clear();
    strideBytes.clear();
    dataOffset.clear();
    data.clear();
  }
  size_t size() const {
    return width.size();
  }
};

struct ImageDrawStore {
  std::vector<int16_t> x0;
  std::vector<int16_t> y0;
  std::vector<int16_t> x1;
  std::vector<int16_t> y1;
  std::vector<uint16_t> srcX0;
  std::vector<uint16_t> srcY0;
  std::vector<uint16_t> srcX1;
  std::vector<uint16_t> srcY1;
  std::vector<uint32_t> imageIndex;
  std::vector<uint8_t> tintColorIndex;
  std::vector<uint8_t> opacity;
  std::vector<uint8_t> flags;
  std::vector<int16_t> clipX0;
  std::vector<int16_t> clipY0;
  std::vector<int16_t> clipX1;
  std::vector<int16_t> clipY1;

  void clear() {
    x0.clear();
    y0.clear();
    x1.clear();
    y1.clear();
    srcX0.clear();
    srcY0.clear();
    srcX1.clear();
    srcY1.clear();
    imageIndex.clear();
    tintColorIndex.clear();
    opacity.clear();
    flags.clear();
    clipX0.clear();
    clipY0.clear();
    clipX1.clear();
    clipY1.clear();
  }
  size_t size() const {
    return x0.size();
  }
};

struct TextStore {
  std::vector<int16_t> x;
  std::vector<int16_t> y;
  std::vector<uint16_t> width;
  std::vector<uint16_t> height;
  std::vector<int16_t> zQ8_8;
  std::vector<uint8_t> opacity;
  std::vector<uint8_t> colorIndex;
  std::vector<uint8_t> flags;
  std::vector<uint32_t> runIndex;
  std::vector<int16_t> clipX0;
  std::vector<int16_t> clipY0;
  std::vector<int16_t> clipX1;
  std::vector<int16_t> clipY1;

  void clear() {
    x.clear();
    y.clear();
    width.clear();
    height.clear();
    zQ8_8.clear();
    opacity.clear();
    colorIndex.clear();
    flags.clear();
    runIndex.clear();
    clipX0.clear();
    clipY0.clear();
    clipX1.clear();
    clipY1.clear();
  }
  size_t size() const {
    return x.size();
  }
};

struct TextRunStore {
  std::vector<uint32_t> glyphStart;
  std::vector<uint32_t> glyphCount;
  std::vector<int16_t> baselineQ8_8;
  std::vector<uint16_t> scaleQ8_8;

  void clear() {
    glyphStart.clear();
    glyphCount.clear();
    baselineQ8_8.clear();
    scaleQ8_8.clear();
  }
  size_t size() const {
    return glyphStart.size();
  }
};

struct GlyphStore {
  struct GlyphBitmap {
    int32_t width = 0;
    int32_t height = 0;
    int32_t bearingX = 0;
    int32_t bearingY = 0;
    int32_t advance = 0;
    int32_t stride = 0;
    GlyphBitmapFormat format = GlyphBitmapFormat::Mask8;
    int32_t atlasIndex = -1;
    int32_t atlasX = 0;
    int32_t atlasY = 0;
    std::vector<uint8_t> pixels;
  };

  struct GlyphAtlas {
    int32_t width = 0;
    int32_t height = 0;
    int32_t stride = 0;
    std::vector<uint8_t> pixels;
  };

  std::vector<int32_t> glyphXQ8_8;
  std::vector<int32_t> glyphYQ8_8;
  std::vector<uint32_t> bitmapIndex;
  std::vector<GlyphBitmap> bitmaps;
  std::vector<uint8_t> bitmapOpaque;
  std::vector<GlyphAtlas> atlases;

  void clear() {
    glyphXQ8_8.clear();
    glyphYQ8_8.clear();
    bitmapIndex.clear();
    bitmaps.clear();
    bitmapOpaque.clear();
    atlases.clear();
  }
  size_t size() const {
    return glyphXQ8_8.size();
  }
};

struct DebugTilesStore {
  std::vector<uint8_t> colorIndex;
  std::vector<uint8_t> lineWidth;
  std::vector<uint8_t> flags;

  void clear() {
    colorIndex.clear();
    lineWidth.clear();
    flags.clear();
  }
  size_t size() const {
    return colorIndex.size();
  }
};

struct TileCommand {
  CommandType type{CommandType::Rect};
  uint32_t index = 0;
  uint32_t order = 0;
  uint8_t x = 0;
  uint8_t y = 0;
  uint8_t wMinus1 = 0;
  uint8_t hMinus1 = 0;
};

struct TileStream {
  std::vector<uint32_t> offsets;
  std::vector<TileCommand> commands;
  std::vector<uint32_t> macroOffsets;
  std::vector<TileCommand> macroCommands;
  std::vector<TileCommand> globalCommands;
  bool enabled = false;
  bool preMerged = false;

  void clear() {
    offsets.clear();
    commands.clear();
    macroOffsets.clear();
    macroCommands.clear();
    globalCommands.clear();
    enabled = false;
    preMerged = false;
  }
};

struct PaletteStore {
  std::array<uint32_t, 256> colorRGBA8{};
  bool enabled = false;
  uint16_t size = 0;

  void clear() {
    enabled = false;
    size = 0;
  }
};

struct RenderBatch {
  // Advanced/unsafe: direct writes to raw stores bypass typed append/build APIs and
  // can violate invariants. Prefer APIs in PrimeManifest/renderer/BatchBuilder.hpp.
  std::vector<RenderCommand> commands;
  ClearStore clear;
  ClearPatternStore clearPattern;
  RectStore rects;
  CircleStore circles;
  PixelStore pixels;
  PixelAStore pixelsA;
  LineStore lines;
  ImageStore images;
  ImageDrawStore imageDraws;
  TextStore text;
  TextRunStore runs;
  GlyphStore glyphs;
  DebugTilesStore debugTiles;
  TileStream tileStream;
  PaletteStore palette;
  uint16_t tileSize = 32;
  uint16_t circleBoundsPad = 0;
  bool disableOpaqueRectFastPath = false;
  uint64_t revision = 0;
  bool useCommandRevision = false;
  uint64_t commandRevision = 0;
  bool reuseOptimized = false;
  bool strictValidation = false;
  bool assumeFrontToBack = true;
  bool autoTileStream = true;
  RendererProfile* profile = nullptr;
  RenderValidationReport* validationReport = nullptr;

  void clearAll() {
    commands.clear();
    clear.clear();
    clearPattern.clear();
    rects.clear();
    circles.clear();
    pixels.clear();
    pixelsA.clear();
    lines.clear();
    images.clear();
    imageDraws.clear();
    text.clear();
    runs.clear();
    glyphs.clear();
    debugTiles.clear();
    tileStream.clear();
    palette.clear();
    disableOpaqueRectFastPath = false;
    circleBoundsPad = 0;
    revision = 0;
    useCommandRevision = false;
    commandRevision = 0;
    reuseOptimized = false;
    strictValidation = false;
    assumeFrontToBack = true;
    autoTileStream = true;
    validationReport = nullptr;
  }
};

void RenderOptimized(RenderTarget target, RenderBatch const& batch, OptimizedBatch const& prepared);

} // namespace PrimeManifest
