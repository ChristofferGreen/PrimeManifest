#include "test_helpers.hpp"
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;


TEST_SUITE_BEGIN("primemanifest.clear");

TEST_CASE("fills_entire_target") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{10, 20, 30, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == expected, "clear sets pixel 0,0");
  CHECK_MESSAGE(pixel_at(buffer, width, 3, 3) == expected, "clear sets pixel 3,3");
}

TEST_CASE("alpha_preserved") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 128}));

  uint32_t width = 2;
  uint32_t height = 2;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  CHECK_MESSAGE(channel_at(buffer, width, 0, 0, 3) == 128, "clear preserves alpha");
}

TEST_CASE("pattern_tiles") {
  RenderBatch batch;
  batch.palette.enabled = true;
  batch.palette.size = 1;
  batch.palette.colorRGBA8[0] = PackRGBA8(Color{0, 0, 0, 0});

  std::vector<uint32_t> pixels = {
    PackRGBA8(Color{255, 0, 0, 255}),
    PackRGBA8(Color{0, 255, 0, 255}),
    PackRGBA8(Color{0, 0, 255, 255}),
    PackRGBA8(Color{255, 255, 255, 255}),
  };
  add_clear_pattern(batch, 2, 2, pixels);

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t green = PackRGBA8(Color{0, 255, 0, 255});
  uint32_t blue = PackRGBA8(Color{0, 0, 255, 255});
  uint32_t white = PackRGBA8(Color{255, 255, 255, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == red, "pattern (0,0) is red");
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 0) == green, "pattern (1,0) is green");
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 1) == blue, "pattern (0,1) is blue");
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == white, "pattern (1,1) is white");
  CHECK_MESSAGE(pixel_at(buffer, width, 2, 0) == red, "pattern repeats horizontally");
}

TEST_CASE("last_command_wins") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));
  add_clear(batch, PackRGBA8(Color{40, 50, 60, 255}));

  uint32_t width = 2;
  uint32_t height = 2;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{40, 50, 60, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == expected, "last clear wins");
}

TEST_SUITE_END();
