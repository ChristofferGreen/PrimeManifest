#include "test_helpers.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(rect, fills_interior) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, 2, 2, 6, 6, PackRGBA8(Color{200, 0, 0, 255}));

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{200, 0, 0, 255});
  PM_CHECK(pixel_at(buffer, width, 3, 3) == expected, "rect fills interior pixel");
}

PM_TEST(rect, clip_applies) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(7);
  batch.rects.y1.push_back(7);
  uint8_t redIndex = palette_index(batch, PackRGBA8(Color{255, 0, 0, 255}));
  batch.rects.colorIndex.push_back(redIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagClip);
  batch.rects.gradientColor1Index.push_back(redIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(3);
  batch.rects.clipY0.push_back(3);
  batch.rects.clipX1.push_back(5);
  batch.rects.clipY1.push_back(5);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t red = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  PM_CHECK(pixel_at(buffer, width, 4, 4) == red, "clip allows interior pixel");
  PM_CHECK(pixel_at(buffer, width, 2, 2) == black, "clip rejects outside pixel");
}

PM_TEST(rect, clip_outside_skips) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(5);
  batch.rects.y1.push_back(5);
  uint8_t redIndex = palette_index(batch, PackRGBA8(Color{255, 0, 0, 255}));
  batch.rects.colorIndex.push_back(redIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagClip);
  batch.rects.gradientColor1Index.push_back(redIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(10);
  batch.rects.clipY0.push_back(10);
  batch.rects.clipX1.push_back(12);
  batch.rects.clipY1.push_back(12);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  PM_CHECK(pixel_at(buffer, width, 2, 2) == black, "clip outside prevents draw");
}

PM_TEST(rect, opacity_zero_skips) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(5);
  batch.rects.y1.push_back(5);
  uint8_t yellowIndex = palette_index(batch, PackRGBA8(Color{200, 200, 0, 255}));
  batch.rects.colorIndex.push_back(yellowIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(0);
  batch.rects.flags.push_back(0);
  batch.rects.gradientColor1Index.push_back(yellowIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  PM_CHECK(pixel_at(buffer, width, 2, 2) == black, "opacity zero skips rect");
}

PM_TEST(rect, opacity_half_blends) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(5);
  batch.rects.y1.push_back(5);
  uint8_t redIndex = palette_index(batch, PackRGBA8(Color{100, 0, 0, 255}));
  batch.rects.colorIndex.push_back(redIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(128);
  batch.rects.flags.push_back(0);
  batch.rects.gradientColor1Index.push_back(redIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t red = channel_at(buffer, width, 2, 2, 0);
  PM_CHECK(red >= 49 && red <= 51, "opacity half blends to ~50");
}

PM_TEST(rect, offscreen_skipped) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_rect(batch, -10, -10, -2, -2, PackRGBA8(Color{255, 0, 0, 255}));

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  PM_CHECK(pixel_at(buffer, width, 0, 0) == black, "offscreen rect skipped");
}

PM_TEST(rect, rotation_draws) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(2);
  batch.rects.y0.push_back(2);
  batch.rects.x1.push_back(6);
  batch.rects.y1.push_back(6);
  uint8_t blueIndex = palette_index(batch, PackRGBA8(Color{0, 0, 255, 255}));
  batch.rects.colorIndex.push_back(blueIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(static_cast<int16_t>(static_cast<int>(3.14159f * 0.5f * 256)));
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(0);
  batch.rects.gradientColor1Index.push_back(blueIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t blue = PackRGBA8(Color{0, 0, 255, 255});
  PM_CHECK(pixel_at(buffer, width, 4, 4) == blue, "rotated rect draws center");
}

PM_TEST(rect, gradient_vertical) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect(batch, 0, 0, 10, 10,
                    PackRGBA8(Color{0, 0, 0, 255}),
                    PackRGBA8(Color{255, 255, 255, 255}));

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t top = channel_at(buffer, width, 5, 2, 0);
  uint8_t bottom = channel_at(buffer, width, 5, 8, 0);
  PM_CHECK(top < bottom, "gradient rect increases along Y");
}

PM_TEST(rect, gradient_same_colors_uniform) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect(batch, 0, 0, 10, 10,
                    PackRGBA8(Color{50, 60, 70, 255}),
                    PackRGBA8(Color{50, 60, 70, 255}));

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{50, 60, 70, 255});
  PM_CHECK(pixel_at(buffer, width, 2, 2) == expected, "gradient with same colors is uniform");
  PM_CHECK(pixel_at(buffer, width, 8, 8) == expected, "gradient with same colors is uniform");
}

PM_TEST(rect, gradient_dir_normalized) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect_dir(batch, 0, 0, 10, 10,
                        PackRGBA8(Color{0, 0, 0, 255}),
                        PackRGBA8(Color{255, 255, 255, 255}),
                        0, 0);

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t top = channel_at(buffer, width, 5, 2, 0);
  uint8_t bottom = channel_at(buffer, width, 5, 8, 0);
  PM_CHECK(top < bottom, "gradient dir fallback behaves vertically");
}

PM_TEST(rect, gradient_horizontal) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect_dir(batch, 0, 0, 10, 10,
                        PackRGBA8(Color{0, 0, 0, 255}),
                        PackRGBA8(Color{255, 255, 255, 255}),
                        256, 0);

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t left = channel_at(buffer, width, 2, 5, 0);
  uint8_t right = channel_at(buffer, width, 8, 5, 0);
  PM_CHECK(left < right, "gradient rect increases along X");
}

PM_TEST(rect, gradient_clip_respected) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(0);
  batch.rects.y0.push_back(0);
  batch.rects.x1.push_back(10);
  batch.rects.y1.push_back(10);
  uint8_t blackIndex = palette_index(batch, PackRGBA8(Color{0, 0, 0, 255}));
  uint8_t whiteIndex = palette_index(batch, PackRGBA8(Color{255, 255, 255, 255}));
  batch.rects.colorIndex.push_back(blackIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagGradient | RectFlagClip);
  batch.rects.gradientColor1Index.push_back(whiteIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(256);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(5);
  batch.rects.clipY1.push_back(5);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 10;
  uint32_t height = 10;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  PM_CHECK(pixel_at(buffer, width, 7, 7) == black, "gradient clip prevents outside draw");
}

PM_TEST(rect, gradient_opacity_applies) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  add_gradient_rect(batch, 0, 0, 10, 10,
                    PackRGBA8(Color{100, 0, 0, 255}),
                    PackRGBA8(Color{200, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.opacity.size()) - 1;
  batch.rects.opacity[idx] = 128;

  uint32_t width = 12;
  uint32_t height = 12;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint8_t red = channel_at(buffer, width, 5, 5, 0);
  PM_CHECK(red >= 49 && red <= 101, "gradient opacity applies");
}

PM_TEST(rect, negative_clip_prevents_draw) {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(1);
  batch.rects.y0.push_back(1);
  batch.rects.x1.push_back(5);
  batch.rects.y1.push_back(5);
  uint8_t redIndex = palette_index(batch, PackRGBA8(Color{255, 0, 0, 255}));
  batch.rects.colorIndex.push_back(redIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(RectFlagClip);
  batch.rects.gradientColor1Index.push_back(redIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(-10);
  batch.rects.clipY0.push_back(-10);
  batch.rects.clipX1.push_back(-5);
  batch.rects.clipY1.push_back(-5);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});

  uint32_t width = 6;
  uint32_t height = 6;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t black = PackRGBA8(Color{0, 0, 0, 255});
  PM_CHECK(pixel_at(buffer, width, 2, 2) == black, "negative clip prevents draw");
}
