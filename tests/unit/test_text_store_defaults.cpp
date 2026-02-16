#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.text_store");

TEST_CASE("defaults_are_zeroed") {
  TextStore store;
  CHECK_MESSAGE(store.size() == 0, "text store empty by default");
  CHECK_MESSAGE(store.flags.empty(), "text flags empty");
  CHECK_MESSAGE(store.clipX0.empty(), "clip arrays empty");
}

TEST_SUITE_END();
