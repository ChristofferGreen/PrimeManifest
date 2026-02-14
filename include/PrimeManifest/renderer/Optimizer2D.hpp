#pragma once

#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <cstdint>
#include <vector>

namespace PrimeManifest {

struct OptimizedBatch {
  struct CmdTileInfo {
    int32_t x0 = 0;
    int32_t y0 = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    uint32_t tx0 = 0;
    uint32_t ty0 = 0;
    uint32_t tx1 = 0;
    uint32_t ty1 = 0;
  };

  uint32_t targetWidth = 0;
  uint32_t targetHeight = 0;
  uint32_t tileSize = 0;
  uint32_t tilesX = 0;
  uint32_t tilesY = 0;
  uint32_t tileCount = 0;
  bool tilePow2 = false;
  uint32_t tileShift = 0;
  bool useTileStream = false;
  bool useTileBuffer = false;
  bool hasClear = false;
  uint32_t clearColor = 0;
  bool debugTiles = false;
  uint32_t debugColor = 0;
  uint8_t debugLineWidth = 1;
  uint8_t debugFlags = 0;
  bool valid = false;
  uint64_t sourceRevision = 0;

  TileStream mergedTileStream;
  TileStream generatedTileStream;
  TileStream const* tileStream = nullptr;

  std::vector<uint32_t> tileCounts;
  std::vector<CmdTileInfo> cmdTiles;
  std::vector<uint8_t> cmdActive;
  std::vector<uint32_t> tileOffsets;
  std::vector<uint32_t> tileRefs;
  std::vector<uint32_t> tileFill;
  std::vector<uint32_t> renderTiles;
  std::vector<uint8_t> textBaseAlpha;
  std::vector<uint8_t> textActive;
  std::vector<uint32_t> textPmOffset;
  std::vector<uint8_t> textPmRStore;
  std::vector<uint8_t> textPmGStore;
  std::vector<uint8_t> textPmBStore;
  std::vector<uint8_t> textColorR;
  std::vector<uint8_t> textColorG;
  std::vector<uint8_t> textColorB;
  std::vector<uint8_t> textColorA;
  std::vector<uint8_t> textClipEnabled;
  std::vector<int32_t> textClipX0;
  std::vector<int32_t> textClipY0;
  std::vector<int32_t> textClipX1;
  std::vector<int32_t> textClipY1;
  std::vector<uint8_t> rectBaseAlpha;
  std::vector<uint8_t> rectActive;
  std::vector<uint32_t> rectEdgeOffset;
  std::vector<uint8_t> rectEdgePmRStore;
  std::vector<uint8_t> rectEdgePmGStore;
  std::vector<uint8_t> rectEdgePmBStore;
  std::vector<uint8_t> rectHasGradient;
  std::vector<uint8_t> rectColorR;
  std::vector<uint8_t> rectColorG;
  std::vector<uint8_t> rectColorB;
  std::vector<uint8_t> rectColorA;
  std::vector<uint8_t> rectGradColorR;
  std::vector<uint8_t> rectGradColorG;
  std::vector<uint8_t> rectGradColorB;
  std::vector<uint8_t> rectGradColorA;
  std::vector<uint8_t> rectClipEnabled;
  std::vector<int32_t> rectClipX0;
  std::vector<int32_t> rectClipY0;
  std::vector<int32_t> rectClipX1;
  std::vector<int32_t> rectClipY1;
  std::vector<float> rectGradDirX;
  std::vector<float> rectGradDirY;
  std::vector<float> rectGradMin;
  std::vector<float> rectGradInvRange;

  void clear() {
    targetWidth = 0;
    targetHeight = 0;
    tileSize = 0;
    tilesX = 0;
    tilesY = 0;
    tileCount = 0;
    tilePow2 = false;
    tileShift = 0;
    useTileStream = false;
    useTileBuffer = false;
    hasClear = false;
    clearColor = 0;
    debugTiles = false;
    debugColor = 0;
    debugLineWidth = 1;
    debugFlags = 0;
    valid = false;
    sourceRevision = 0;
    mergedTileStream.clear();
    generatedTileStream.clear();
    tileStream = nullptr;
    tileCounts.clear();
    cmdTiles.clear();
    cmdActive.clear();
    tileOffsets.clear();
    tileRefs.clear();
    tileFill.clear();
    renderTiles.clear();
    textBaseAlpha.clear();
    textActive.clear();
    textPmOffset.clear();
    textPmRStore.clear();
    textPmGStore.clear();
    textPmBStore.clear();
    textColorR.clear();
    textColorG.clear();
    textColorB.clear();
    textColorA.clear();
    textClipEnabled.clear();
    textClipX0.clear();
    textClipY0.clear();
    textClipX1.clear();
    textClipY1.clear();
    rectBaseAlpha.clear();
    rectActive.clear();
    rectEdgeOffset.clear();
    rectEdgePmRStore.clear();
    rectEdgePmGStore.clear();
    rectEdgePmBStore.clear();
    rectHasGradient.clear();
    rectColorR.clear();
    rectColorG.clear();
    rectColorB.clear();
    rectColorA.clear();
    rectGradColorR.clear();
    rectGradColorG.clear();
    rectGradColorB.clear();
    rectGradColorA.clear();
    rectClipEnabled.clear();
    rectClipX0.clear();
    rectClipY0.clear();
    rectClipX1.clear();
    rectClipY1.clear();
    rectGradDirX.clear();
    rectGradDirY.clear();
    rectGradMin.clear();
    rectGradInvRange.clear();
  }
};

void OptimizeRenderBatch(RenderTarget target, RenderBatch const& batch, OptimizedBatch& optimized);

} // namespace PrimeManifest
