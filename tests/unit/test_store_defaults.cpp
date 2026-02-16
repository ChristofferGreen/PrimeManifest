#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(stores, rect_store_starts_empty) {
  RectStore store;
  PM_CHECK(store.size() == 0, "rect store empty");
  store.x0.push_back(1);
  store.clear();
  PM_CHECK(store.size() == 0, "rect store clear resets size");
}

PM_TEST(stores, circle_store_starts_empty) {
  CircleStore store;
  PM_CHECK(store.size() == 0, "circle store empty");
  store.centerX.push_back(1);
  store.clear();
  PM_CHECK(store.size() == 0, "circle store clear resets size");
}

PM_TEST(stores, text_store_starts_empty) {
  TextStore store;
  PM_CHECK(store.size() == 0, "text store empty");
  store.x.push_back(1);
  store.clear();
  PM_CHECK(store.size() == 0, "text store clear resets size");
}

PM_TEST(stores, glyph_store_starts_empty) {
  GlyphStore store;
  PM_CHECK(store.size() == 0, "glyph store empty");
  store.glyphXQ8_8.push_back(1);
  store.clear();
  PM_CHECK(store.size() == 0, "glyph store clear resets size");
}

PM_TEST(stores, tile_stream_clear_resets) {
  TileStream stream;
  stream.enabled = true;
  stream.offsets.push_back(1);
  stream.clear();
  PM_CHECK(stream.offsets.empty(), "tile stream clear empties offsets");
  PM_CHECK(!stream.enabled, "tile stream disabled after clear");
}

PM_TEST(stores, palette_clear_resets) {
  PaletteStore palette;
  palette.enabled = true;
  palette.size = 5;
  palette.clear();
  PM_CHECK(!palette.enabled, "palette disabled after clear");
  PM_CHECK(palette.size == 0, "palette size reset");
}
