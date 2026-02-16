#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.palette_store");

TEST_CASE("default_size_zero") {
  PaletteStore store;
  CHECK_MESSAGE(store.size == 0, "palette size defaults to zero");
  CHECK_MESSAGE(!store.enabled, "palette disabled by default");
}

TEST_CASE("clear_resets_enabled") {
  PaletteStore store;
  store.enabled = true;
  store.size = 3;
  store.clear();
  CHECK_MESSAGE(!store.enabled, "palette disabled after clear");
  CHECK_MESSAGE(store.size == 0, "palette size reset after clear");
}

TEST_SUITE_END();
