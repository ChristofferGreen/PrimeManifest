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

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        SkippedCommandDiagnostics& optimizerOut,
                                                        SkippedCommandDiagnostics& skippedOut) -> bool {
  optimizerOut.clear();
  skippedOut.clear();
  if (dump == "skip_diagnostics=none") return true;
  if (dump.empty()) return false;

  constexpr std::string_view OptimizerPrefix = "optimizerSkippedCommands.";
  constexpr std::string_view RendererPrefix = "skippedCommands.";

  size_t start = 0;
  while (start <= dump.size()) {
    size_t end = dump.find(';', start);
    if (end == std::string_view::npos) end = dump.size();
    if (end <= start) return false;
    std::string_view field = dump.substr(start, end - start);

    size_t equals = field.find('=');
    if (equals == std::string_view::npos || equals == 0 || equals + 1 >= field.size()) return false;
    std::string_view key = field.substr(0, equals);
    std::string_view valueText = field.substr(equals + 1);
    uint64_t value = 0;
    if (!parseUnsignedDecimal(valueText, value)) return false;

    SkippedCommandDiagnostics* diagnostics = nullptr;
    std::string_view tail;
    if (key.starts_with(OptimizerPrefix)) {
      diagnostics = &optimizerOut;
      tail = key.substr(OptimizerPrefix.size());
    } else if (key.starts_with(RendererPrefix)) {
      diagnostics = &skippedOut;
      tail = key.substr(RendererPrefix.size());
    } else {
      return false;
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
        if (!skippedCommandReasonFromName(reasonName, reason)) return false;
        diagnostics->byReason[static_cast<size_t>(reason)] = value;
      }
    } else if (tail.starts_with("type.")) {
      std::string_view typeName = tail.substr(5);
      CommandType type{};
      if (!commandTypeFromName(typeName, type)) return false;
      diagnostics->byType[static_cast<size_t>(type)] = value;
    } else if (tail.starts_with("typeReason.")) {
      std::string_view pair = tail.substr(11);
      size_t separator = pair.find('.');
      if (separator == std::string_view::npos || separator == 0 || separator + 1 >= pair.size()) return false;
      std::string_view typeName = pair.substr(0, separator);
      std::string_view reasonName = pair.substr(separator + 1);
      CommandType type{};
      SkippedCommandReason reason{};
      if (!commandTypeFromName(typeName, type) || !skippedCommandReasonFromName(reasonName, reason)) return false;
      diagnostics->byTypeAndReason[static_cast<size_t>(type)][static_cast<size_t>(reason)] = value;
    } else {
      return false;
    }

    if (end == dump.size()) break;
    start = end + 1;
  }
  return true;
}

inline auto parseRendererProfileSkipDiagnosticsKeyValue(std::string_view dump,
                                                        RendererProfile& profileOut) -> bool {
  return parseRendererProfileSkipDiagnosticsKeyValue(dump,
                                                     profileOut.optimizerSkippedCommands,
                                                     profileOut.skippedCommands);
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
