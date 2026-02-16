#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.color_helpers");

TEST_CASE("pack_unpack_full_range") {
  Color c{255, 128, 64, 32};
  uint32_t packed = PackRGBA8(c);
  Color unpacked = UnpackRGBA8(packed);
  CHECK_MESSAGE(unpacked.r == 255, "red channel max");
  CHECK_MESSAGE(unpacked.g == 128, "green channel mid");
  CHECK_MESSAGE(unpacked.b == 64, "blue channel mid");
  CHECK_MESSAGE(unpacked.a == 32, "alpha channel low");
}

TEST_SUITE_END();
