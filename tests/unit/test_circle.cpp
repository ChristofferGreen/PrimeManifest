#include "test_helpers.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(circle, renders_filled_circle) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_circle(batch, 4, 4, 2, PackRGBA8(Color{255, 0, 0, 255}));

  uint32_t width = 9;
  uint32_t height = 9;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{buffer, width, height, width * 4};

  render_batch(target, batch);

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  PM_CHECK(pixel_at(buffer, width, 4, 4) == red, "circle center filled");
  PM_CHECK(pixel_at(buffer, width, 0, 0) == black, "circle outside untouched");
}
