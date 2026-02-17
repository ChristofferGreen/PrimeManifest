#include "test_helpers.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

TEST_SUITE_BEGIN("primemanifest.line");

TEST_CASE("renders_horizontal_line") {
  RenderBatch batch;
  constexpr uint32_t width = 5;
  constexpr uint32_t height = 5;
  std::vector<uint8_t> buffer(width * height * 4, 0u);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  uint32_t white = PackRGBA8(Color{255, 255, 255, 255});
  add_line(batch, 0, 2, 4, 2, 2.0f, white);
  render_batch(target, batch);

  CHECK_MESSAGE(pixel_at(buffer, width, 2, 1) == white, "line covers upper row");
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 2) == white, "line covers lower row");
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 0) == 0u, "outside line remains empty");
}

TEST_SUITE_END();
