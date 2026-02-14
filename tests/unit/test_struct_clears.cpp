#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(struct_clears, optimized_batch_clear_resets_state) {
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

  PM_CHECK(!batch.valid, "optimized batch valid reset");
  PM_CHECK(batch.targetWidth == 0 && batch.targetHeight == 0, "target size reset");
  PM_CHECK(!batch.useTileStream && !batch.useTileBuffer, "tile flags reset");
  PM_CHECK(!batch.hasClear, "clear flag reset");
  PM_CHECK(batch.clearColor == 0, "clear color reset");
  PM_CHECK(!batch.debugTiles, "debug flag reset");
  PM_CHECK(batch.tileStream == nullptr, "tile stream pointer reset");
  PM_CHECK(batch.tileCounts.empty(), "tile counts cleared");
  PM_CHECK(batch.textColorR.empty(), "text cache cleared");
}

PM_TEST(struct_clears, renderer_profile_clear_resets) {
  RendererProfile profile;
  profile.renderNs = 1;
  profile.buildNs = 2;
  profile.tileCount = 3;
  profile.workerNs = {4};
  profile.workerTiles = {5};

  profile.clear();

  PM_CHECK(profile.renderNs == 0, "renderNs reset");
  PM_CHECK(profile.buildNs == 0, "buildNs reset");
  PM_CHECK(profile.tileCount == 0, "tileCount reset");
  PM_CHECK(profile.workerNs.empty(), "workerNs cleared");
  PM_CHECK(profile.workerTiles.empty(), "workerTiles cleared");
}
