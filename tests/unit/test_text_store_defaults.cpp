#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(text_store, defaults_are_zeroed) {
  TextStore store;
  PM_CHECK(store.size() == 0, "text store empty by default");
  PM_CHECK(store.flags.empty(), "text flags empty");
  PM_CHECK(store.clipX0.empty(), "clip arrays empty");
}
