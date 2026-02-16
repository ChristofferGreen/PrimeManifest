#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.stores");

TEST_CASE("rect_store_starts_empty") {
  RectStore store;
  CHECK_MESSAGE(store.size() == 0, "rect store empty");
  store.x0.push_back(1);
  store.clear();
  CHECK_MESSAGE(store.size() == 0, "rect store clear resets size");
}

TEST_CASE("circle_store_starts_empty") {
  CircleStore store;
  CHECK_MESSAGE(store.size() == 0, "circle store empty");
  store.centerX.push_back(1);
  store.clear();
  CHECK_MESSAGE(store.size() == 0, "circle store clear resets size");
}

TEST_CASE("text_store_starts_empty") {
  TextStore store;
  CHECK_MESSAGE(store.size() == 0, "text store empty");
  store.x.push_back(1);
  store.clear();
  CHECK_MESSAGE(store.size() == 0, "text store clear resets size");
}

TEST_CASE("glyph_store_starts_empty") {
  GlyphStore store;
  CHECK_MESSAGE(store.size() == 0, "glyph store empty");
  store.glyphXQ8_8.push_back(1);
  store.clear();
  CHECK_MESSAGE(store.size() == 0, "glyph store clear resets size");
}

TEST_CASE("tile_stream_clear_resets") {
  TileStream stream;
  stream.enabled = true;
  stream.offsets.push_back(1);
  stream.clear();
  CHECK_MESSAGE(stream.offsets.empty(), "tile stream clear empties offsets");
  CHECK_MESSAGE(!stream.enabled, "tile stream disabled after clear");
}

TEST_CASE("palette_clear_resets") {
  PaletteStore palette;
  palette.enabled = true;
  palette.size = 5;
  palette.clear();
  CHECK_MESSAGE(!palette.enabled, "palette disabled after clear");
  CHECK_MESSAGE(palette.size == 0, "palette size reset");
}

TEST_SUITE_END();
