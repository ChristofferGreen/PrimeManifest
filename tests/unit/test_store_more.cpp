#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.stores_more");

TEST_CASE("clear_store_clear_resets") {
  ClearStore store;
  store.colorIndex.push_back(1);
  CHECK_MESSAGE(store.size() == 1, "clear store size reflects entries");
  store.clear();
  CHECK_MESSAGE(store.size() == 0, "clear store cleared");
}

TEST_CASE("clear_pattern_store_clear_resets") {
  ClearPatternStore store;
  store.width.push_back(2);
  store.height.push_back(2);
  store.dataOffset.push_back(0);
  store.data.resize(4);
  CHECK_MESSAGE(store.size() == 1, "clear pattern size reflects entries");
  store.clear();
  CHECK_MESSAGE(store.size() == 0, "clear pattern cleared");
  CHECK_MESSAGE(store.data.empty(), "clear pattern data cleared");
}

TEST_CASE("text_run_store_clear_resets") {
  TextRunStore store;
  store.glyphStart.push_back(0);
  store.glyphCount.push_back(1);
  store.baselineQ8_8.push_back(2);
  store.scaleQ8_8.push_back(256);
  CHECK_MESSAGE(store.size() == 1, "text run size reflects entries");
  store.clear();
  CHECK_MESSAGE(store.size() == 0, "text run cleared");
}

TEST_CASE("debug_tiles_store_clear_resets") {
  DebugTilesStore store;
  store.colorIndex.push_back(3);
  store.lineWidth.push_back(1);
  store.flags.push_back(0);
  CHECK_MESSAGE(store.size() == 1, "debug tiles size reflects entries");
  store.clear();
  CHECK_MESSAGE(store.size() == 0, "debug tiles cleared");
}

TEST_SUITE_END();
