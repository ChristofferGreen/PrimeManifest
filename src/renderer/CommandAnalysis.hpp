#pragma once

#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <cstdint>
#include <vector>

namespace PrimeManifest {

struct PrimitiveBounds {
  int32_t x0 = 0;
  int32_t y0 = 0;
  int32_t x1 = 0;
  int32_t y1 = 0;
  bool clipEnabled = false;
  IntRect clip{};
  bool valid = false;
};

struct AnalyzedCommand {
  CommandType type{CommandType::Rect};
  uint32_t index = 0;
  uint32_t order = 0;
  int32_t x0 = 0;
  int32_t y0 = 0;
  int32_t x1 = 0;
  int32_t y1 = 0;
  bool clipEnabled = false;
  IntRect clip{};
  uint8_t baseAlpha = 0;
  uint32_t tx0 = 0;
  uint32_t ty0 = 0;
  uint32_t tx1 = 0;
  uint32_t ty1 = 0;
  bool valid = false;
};

struct CommandAnalysisConfig {
  uint32_t targetWidth = 0;
  uint32_t targetHeight = 0;
  uint32_t tileSize = 1;
  bool tilePow2 = false;
  uint32_t tileShift = 0;
  bool paletteOpaque = false;
};

void computePrimitiveBounds(RenderBatch const& batch,
                            CommandType type,
                            uint32_t index,
                            uint32_t targetWidth,
                            uint32_t targetHeight,
                            PrimitiveBounds& out);

void analyzeCommands(RenderBatch const& batch,
                     CommandAnalysisConfig const& config,
                     std::vector<AnalyzedCommand>& out);

} // namespace PrimeManifest
