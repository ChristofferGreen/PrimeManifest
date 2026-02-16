#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.store_sizes");

TEST_CASE("clear_pattern_size_matches_entries") {
  ClearPatternStore store;
  store.width.push_back(1);
  store.height.push_back(1);
  store.dataOffset.push_back(0);
  store.data.resize(4);
  CHECK_MESSAGE(store.size() == 1, "clear pattern size counts entries");
}

TEST_CASE("text_run_size_matches_entries") {
  TextRunStore store;
  store.glyphStart.push_back(0);
  store.glyphCount.push_back(2);
  store.baselineQ8_8.push_back(0);
  store.scaleQ8_8.push_back(256);
  CHECK_MESSAGE(store.size() == 1, "text run size counts entries");
}

TEST_SUITE_END();
