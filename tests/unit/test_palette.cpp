#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(palette, pack_unpack_roundtrip) {
  Color c{12, 34, 56, 78};
  uint32_t packed = PackRGBA8(c);
  Color unpacked = UnpackRGBA8(packed);
  PM_CHECK(unpacked.r == 12, "red channel roundtrip");
  PM_CHECK(unpacked.g == 34, "green channel roundtrip");
  PM_CHECK(unpacked.b == 56, "blue channel roundtrip");
  PM_CHECK(unpacked.a == 78, "alpha channel roundtrip");
}
