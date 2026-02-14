#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(stores_more, clear_store_clear_resets) {
  ClearStore store;
  store.colorIndex.push_back(1);
  PM_CHECK(store.size() == 1, "clear store size reflects entries");
  store.clear();
  PM_CHECK(store.size() == 0, "clear store cleared");
}

PM_TEST(stores_more, clear_pattern_store_clear_resets) {
  ClearPatternStore store;
  store.width.push_back(2);
  store.height.push_back(2);
  store.dataOffset.push_back(0);
  store.data.resize(4);
  PM_CHECK(store.size() == 1, "clear pattern size reflects entries");
  store.clear();
  PM_CHECK(store.size() == 0, "clear pattern cleared");
  PM_CHECK(store.data.empty(), "clear pattern data cleared");
}

PM_TEST(stores_more, text_run_store_clear_resets) {
  TextRunStore store;
  store.glyphStart.push_back(0);
  store.glyphCount.push_back(1);
  store.baselineQ8_8.push_back(2);
  store.scaleQ8_8.push_back(256);
  PM_CHECK(store.size() == 1, "text run size reflects entries");
  store.clear();
  PM_CHECK(store.size() == 0, "text run cleared");
}

PM_TEST(stores_more, debug_tiles_store_clear_resets) {
  DebugTilesStore store;
  store.colorIndex.push_back(3);
  store.lineWidth.push_back(1);
  store.flags.push_back(0);
  PM_CHECK(store.size() == 1, "debug tiles size reflects entries");
  store.clear();
  PM_CHECK(store.size() == 0, "debug tiles cleared");
}
