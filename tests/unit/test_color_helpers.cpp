#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(color_helpers, pack_unpack_full_range) {
  Color c{255, 128, 64, 32};
  uint32_t packed = PackRGBA8(c);
  Color unpacked = UnpackRGBA8(packed);
  PM_CHECK(unpacked.r == 255, "red channel max");
  PM_CHECK(unpacked.g == 128, "green channel mid");
  PM_CHECK(unpacked.b == 64, "blue channel mid");
  PM_CHECK(unpacked.a == 32, "alpha channel low");
}
