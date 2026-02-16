#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.struct_clears");

TEST_CASE("optimized_batch_clear_resets_state") {
  OptimizedBatch batch;
  batch.valid = true;
  batch.targetWidth = 10;
  batch.targetHeight = 20;
  batch.tileSize = 32;
  batch.useTileStream = true;
  batch.useTileBuffer = true;
  batch.hasClear = true;
  batch.clearColor = 0x11223344u;
  batch.debugTiles = true;
  batch.tileStream = &batch.mergedTileStream;
  batch.tileCounts.push_back(1);
  batch.textColorR.push_back(2);

  batch.clear();

  CHECK_MESSAGE(!batch.valid, "optimized batch valid reset");
  CHECK_MESSAGE(batch.targetWidth == 0, "target width reset");
  CHECK_MESSAGE(batch.targetHeight == 0, "target height reset");
  CHECK_MESSAGE(!batch.useTileStream, "tile stream flag reset");
  CHECK_MESSAGE(!batch.useTileBuffer, "tile buffer flag reset");
  CHECK_MESSAGE(!batch.hasClear, "clear flag reset");
  CHECK_MESSAGE(batch.clearColor == 0, "clear color reset");
  CHECK_MESSAGE(!batch.debugTiles, "debug flag reset");
  CHECK_MESSAGE(batch.tileStream == nullptr, "tile stream pointer reset");
  CHECK_MESSAGE(batch.tileCounts.empty(), "tile counts cleared");
  CHECK_MESSAGE(batch.textColorR.empty(), "text cache cleared");
}

TEST_CASE("renderer_profile_clear_resets") {
  RendererProfile profile;
  profile.renderNs = 1;
  profile.buildNs = 2;
  profile.tileCount = 3;
  profile.workerNs = {4};
  profile.workerTiles = {5};

  profile.clear();

  CHECK_MESSAGE(profile.renderNs == 0, "renderNs reset");
  CHECK_MESSAGE(profile.buildNs == 0, "buildNs reset");
  CHECK_MESSAGE(profile.tileCount == 0, "tileCount reset");
  CHECK_MESSAGE(profile.workerNs.empty(), "workerNs cleared");
  CHECK_MESSAGE(profile.workerTiles.empty(), "workerTiles cleared");
}

TEST_SUITE_END();
