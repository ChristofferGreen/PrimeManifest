#include "test_helpers.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

TEST_SUITE_BEGIN("primemanifest.pixel");

TEST_CASE("set_pixel_writes_color") {
  RenderBatch batch;
  constexpr uint32_t width = 4;
  constexpr uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0u);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  add_set_pixel(batch, 1, 1, red);
  render_batch(target, batch);

  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == red, "set pixel writes color");
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == 0u, "other pixels remain untouched");
}

TEST_CASE("set_pixel_a_blends_with_alpha") {
  RenderBatch batch;
  constexpr uint32_t width = 3;
  constexpr uint32_t height = 3;
  std::vector<uint8_t> buffer(width * height * 4, 0u);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  uint32_t blue = PackRGBA8(Color{0, 0, 255, 255});
  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  add_clear(batch, blue);
  add_set_pixel_a(batch, 1, 1, red, 128);
  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{128, 0, 127, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == expected, "alpha blend uses premultiplied math");
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == blue, "background preserved");
}

TEST_SUITE_END();
