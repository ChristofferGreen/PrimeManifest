#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(palette_store, default_size_zero) {
  PaletteStore store;
  PM_CHECK(store.size == 0, "palette size defaults to zero");
  PM_CHECK(!store.enabled, "palette disabled by default");
}

PM_TEST(palette_store, clear_resets_enabled) {
  PaletteStore store;
  store.enabled = true;
  store.size = 3;
  store.clear();
  PM_CHECK(!store.enabled, "palette disabled after clear");
  PM_CHECK(store.size == 0, "palette size reset after clear");
}
