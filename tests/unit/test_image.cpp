#include "test_helpers.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

namespace {
auto build_test_image_pixels() -> std::vector<uint32_t> {
  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t green = PackRGBA8(Color{0, 255, 0, 255});
  uint32_t blue = PackRGBA8(Color{0, 0, 255, 255});
  uint32_t white = PackRGBA8(Color{255, 255, 255, 255});
  return {red, green, blue, white};
}
} // namespace

TEST_SUITE_BEGIN("primemanifest.image");

TEST_CASE("image_bilinear_clamp_sampling") {
  RenderBatch batch;
  constexpr uint32_t width = 4;
  constexpr uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0u);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  auto pixels = build_test_image_pixels();
  uint32_t imageIndex = add_image_asset(batch, 2, 2, pixels);
  uint32_t tint = PackRGBA8(Color{255, 255, 255, 255});
  add_image_draw(batch, imageIndex, 0, 0, 4, 4, 0, 0, 2, 2, tint);
  render_batch(target, batch);

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t white = PackRGBA8(Color{255, 255, 255, 255});
  uint32_t blended = PackRGBA8(Color{159, 63, 63, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == red, "clamp keeps top-left pixel");
  CHECK_MESSAGE(pixel_at(buffer, width, 3, 3) == white, "clamp keeps bottom-right pixel");
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) == blended, "bilinear blend matches expected");
}

TEST_CASE("image_wrap_sampling_uses_repeat") {
  RenderBatch batch;
  constexpr uint32_t width = 4;
  constexpr uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0u);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  auto pixels = build_test_image_pixels();
  uint32_t imageIndex = add_image_asset(batch, 2, 2, pixels);
  uint32_t tint = PackRGBA8(Color{255, 255, 255, 255});
  uint8_t flags = static_cast<uint8_t>(ImageFlagWrapU | ImageFlagWrapV);
  add_image_draw(batch, imageIndex, 0, 0, 4, 4, 0, 0, 2, 2, tint, 255, flags);
  render_batch(target, batch);

  uint32_t wrapped = PackRGBA8(Color{159, 63, 63, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == wrapped, "wrap samples across edges");
}

TEST_CASE("image_clip_restricts_draw") {
  RenderBatch batch;
  constexpr uint32_t width = 4;
  constexpr uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0u);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  auto pixels = build_test_image_pixels();
  uint32_t imageIndex = add_image_asset(batch, 2, 2, pixels);
  uint32_t tint = PackRGBA8(Color{255, 255, 255, 255});
  IntRect clip{0, 0, 2, 2};
  uint8_t flags = static_cast<uint8_t>(ImageFlagClip);
  add_image_draw(batch, imageIndex, 0, 0, 4, 4, 0, 0, 2, 2, tint, 255, flags, clip);
  render_batch(target, batch);

  CHECK_MESSAGE(pixel_at(buffer, width, 3, 3) == 0u, "clip keeps pixels outside region untouched");
  CHECK_MESSAGE(pixel_at(buffer, width, 1, 1) != 0u, "clip keeps pixels inside region");
}

TEST_SUITE_END();
