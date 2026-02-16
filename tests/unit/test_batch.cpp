#include "test_helpers.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(batch, clear_all_resets_state) {
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

  PM_CHECK(batch.commands.empty(), "commands cleared");
  PM_CHECK(batch.clear.size() == 0, "clear store cleared");
  PM_CHECK(batch.rects.size() == 0, "rect store cleared");
  PM_CHECK(batch.circles.size() == 0, "circle store cleared");
  PM_CHECK(batch.text.size() == 0, "text store cleared");
  PM_CHECK(batch.glyphs.size() == 0, "glyph store cleared");
  PM_CHECK(batch.tileStream.commands.empty(), "tile stream cleared");
  PM_CHECK(!batch.palette.enabled, "palette disabled");
  PM_CHECK(batch.palette.size == 0, "palette size reset");
  PM_CHECK(!batch.disableOpaqueRectFastPath, "fast path default restored");
  PM_CHECK(batch.circleBoundsPad == 0, "circle bounds pad reset");
  PM_CHECK(batch.revision == 0, "revision reset");
  PM_CHECK(!batch.reuseOptimized, "reuseOptimized reset");
  PM_CHECK(batch.assumeFrontToBack, "front-to-back default restored");
  PM_CHECK(batch.autoTileStream, "auto tile stream default restored");
}
