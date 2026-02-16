#include "test_helpers.hpp"
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;


TEST_SUITE_BEGIN("primemanifest.batch");

TEST_CASE("clear_all_resets_state") {
  RenderBatch batch;
  batch.tileSize = 64;
  batch.circleBoundsPad = 7;
  batch.disableOpaqueRectFastPath = true;
  batch.revision = 42;
  batch.reuseOptimized = true;
  batch.assumeFrontToBack = false;
  batch.autoTileStream = false;

  add_clear(batch, PackRGBA8(Color{1, 2, 3, 4}));
  add_rect(batch, 0, 0, 1, 1, PackRGBA8(Color{5, 6, 7, 8}));
  add_circle(batch, 2, 2, 1, PackRGBA8(Color{9, 10, 11, 12}));

  batch.clearAll();

  CHECK_MESSAGE(batch.commands.empty(), "commands cleared");
  CHECK_MESSAGE(batch.clear.size() == 0, "clear store cleared");
  CHECK_MESSAGE(batch.rects.size() == 0, "rect store cleared");
  CHECK_MESSAGE(batch.circles.size() == 0, "circle store cleared");
  CHECK_MESSAGE(batch.text.size() == 0, "text store cleared");
  CHECK_MESSAGE(batch.glyphs.size() == 0, "glyph store cleared");
  CHECK_MESSAGE(batch.tileStream.commands.empty(), "tile stream cleared");
  CHECK_MESSAGE(!batch.palette.enabled, "palette disabled");
  CHECK_MESSAGE(batch.palette.size == 0, "palette size reset");
  CHECK_MESSAGE(!batch.disableOpaqueRectFastPath, "fast path default restored");
  CHECK_MESSAGE(batch.circleBoundsPad == 0, "circle bounds pad reset");
  CHECK_MESSAGE(batch.revision == 0, "revision reset");
  CHECK_MESSAGE(!batch.reuseOptimized, "reuseOptimized reset");
  CHECK_MESSAGE(batch.assumeFrontToBack, "front-to-back default restored");
  CHECK_MESSAGE(batch.autoTileStream, "auto tile stream default restored");
}

TEST_SUITE_END();
