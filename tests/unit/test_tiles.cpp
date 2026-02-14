#include "test_helpers.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(tiles, size_zero_defaults) {
  RenderBatch batch;
  batch.tileSize = 0;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 1, 1, 3, 3, PackRGBA8(Color{100, 100, 255, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{100, 100, 255, 255});
  PM_CHECK(pixel_at(buffer, width, 2, 2) == expected, "tile size zero falls back to default");
}

PM_TEST(tiles, size_large_still_renders) {
  RenderBatch batch;
  batch.tileSize = 512;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 1, 1, 3, 3, PackRGBA8(Color{120, 120, 255, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{120, 120, 255, 255});
  PM_CHECK(pixel_at(buffer, width, 2, 2) == expected, "large tile size still renders");
}

PM_TEST(tiles, size_non_power_of_two) {
  RenderBatch batch;
  batch.tileSize = 7;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 6, 6, 9, 9, PackRGBA8(Color{10, 200, 10, 255}));

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{10, 200, 10, 255});
  PM_CHECK(pixel_at(buffer, width, 7, 7) == expected, "non power-of-two tile size works");
}

PM_TEST(tiles, multi_tile_rect) {
  RenderBatch batch;
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 4, 4, 20, 20, PackRGBA8(Color{255, 0, 255, 255}));

  uint32_t width = 24;
  uint32_t height = 24;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{255, 0, 255, 255});
  PM_CHECK(pixel_at(buffer, width, 6, 6) == expected, "rect draws in first tile");
  PM_CHECK(pixel_at(buffer, width, 18, 18) == expected, "rect draws in later tile");
}
