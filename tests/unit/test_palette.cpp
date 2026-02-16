#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.palette");

TEST_CASE("pack_unpack_roundtrip") {
  Color c{12, 34, 56, 78};
  uint32_t packed = PackRGBA8(c);
  Color unpacked = UnpackRGBA8(packed);
  CHECK_MESSAGE(unpacked.r == 12, "red channel roundtrip");
  CHECK_MESSAGE(unpacked.g == 34, "green channel roundtrip");
  CHECK_MESSAGE(unpacked.b == 56, "blue channel roundtrip");
  CHECK_MESSAGE(unpacked.a == 78, "alpha channel roundtrip");
}

TEST_SUITE_END();
