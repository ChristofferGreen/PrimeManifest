#include "test_helpers.hpp"

#include <chrono>
#include <cstdlib>
#include <random>
#include "third_party/doctest.h"

using namespace PrimeManifest;
using namespace PrimeManifestTest;


TEST_SUITE_BEGIN("primemanifest.misc");

TEST_CASE("deterministic_output") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));
  add_rect(batch, 2, 2, 6, 6, PackRGBA8(Color{200, 0, 0, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 2;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 2;
  bitmap.stride = 2;
  bitmap.pixels = {255, 255, 255, 255};
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);
  batch.glyphs.glyphXQ8_8.push_back(0);
  batch.glyphs.glyphYQ8_8.push_back(0);
  batch.glyphs.bitmapIndex.push_back(0);
  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(1);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);
  add_text(batch, 1, 1, 2, 2, PackRGBA8(Color{0, 200, 0, 255}), 0);

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> bufferA(width * height * 4, 0);
  std::vector<uint8_t> bufferB(width * height * 4, 0);
  RenderTarget targetA{std::span<uint8_t>(bufferA), width, height, width * 4};
  RenderTarget targetB{std::span<uint8_t>(bufferB), width, height, width * 4};

  render_batch(targetA, batch);
  render_batch(targetB, batch);

  CHECK_MESSAGE(buffers_equal(bufferA, bufferB), "deterministic output for same batch");
}

TEST_CASE("stride_padding_preserved") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{50, 60, 70, 255}));

  uint32_t width = 4;
  uint32_t height = 3;
  uint32_t stride = width * 4 + 8;
  std::vector<uint8_t> buffer(static_cast<size_t>(stride) * height, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, stride};

  render_batch(target, batch);

  for (uint32_t y = 0; y < height; ++y) {
    size_t padStart = static_cast<size_t>(y) * stride + static_cast<size_t>(width) * 4;
    for (size_t i = padStart; i < padStart + 8; ++i) {
      CHECK_MESSAGE(buffer[i] == 0x7F, "stride padding preserved");
    }
  }
}

TEST_CASE("invalid_indices_ignored") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));
  batch.commands.push_back(RenderCommand{CommandType::Rect, 99});
  batch.commands.push_back(RenderCommand{CommandType::Text, 88});

  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  uint32_t expected = PackRGBA8(Color{0, 0, 0, 255});
  CHECK_MESSAGE(pixel_at(buffer, width, 0, 0) == expected, "invalid indices ignored");
}

TEST_CASE("target_short_span_skips") {
  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{10, 20, 30, 255}));
  std::vector<uint8_t> buffer(4, 0x7F);
  RenderTarget target{std::span<uint8_t>(buffer), 2, 2, 8};

  render_batch(target, batch);

  CHECK_MESSAGE(buffer[0] == 0x7F, "short span prevents write");
}

TEST_CASE("random_fuzz_is_deterministic") {
  RenderBatch batch;
  batch.tileSize = 32;
  add_clear(batch, PackRGBA8(Color{5, 5, 5, 255}));

  GlyphStore::GlyphBitmap bitmap;
  bitmap.width = 3;
  bitmap.height = 3;
  bitmap.bearingX = 0;
  bitmap.bearingY = 0;
  bitmap.advance = 3;
  bitmap.stride = 3;
  bitmap.pixels = {
      0, 255, 0,
      255, 255, 255,
      0, 255, 0,
  };
  batch.glyphs.bitmaps.push_back(bitmap);
  batch.glyphs.bitmapOpaque.push_back(0);

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> posDist(0, 120);
  std::uniform_int_distribution<int32_t> sizeDist(4, 24);
  auto colors = build_test_colors();
  std::uniform_int_distribution<size_t> colorIndexDist(0, colors.size() - 1);
  std::uniform_int_distribution<uint32_t> dirDist(0, 255);
  std::uniform_int_distribution<int> flagDist(0, 1);

  for (uint32_t i = 0; i < 200; ++i) {
    int32_t x0 = posDist(rng);
    int32_t y0 = posDist(rng);
    int32_t x1 = x0 + sizeDist(rng);
    int32_t y1 = y0 + sizeDist(rng);
    uint32_t c0 = colors[colorIndexDist(rng)];
    uint32_t c1 = colors[colorIndexDist(rng)];

    if (flagDist(rng)) {
      add_gradient_rect_dir(batch, x0, y0, x1, y1, c0, c1,
                            static_cast<int16_t>(dirDist(rng)), static_cast<int16_t>(dirDist(rng)));
    } else {
      add_rect(batch, x0, y0, x1, y1, c0);
    }
  }

  for (uint32_t i = 0; i < 40; ++i) {
    batch.glyphs.glyphXQ8_8.push_back(static_cast<int32_t>(i * 2 * 256));
    batch.glyphs.glyphYQ8_8.push_back(0);
    batch.glyphs.bitmapIndex.push_back(0);
  }
  batch.runs.glyphStart.push_back(0);
  batch.runs.glyphCount.push_back(40);
  batch.runs.baselineQ8_8.push_back(0);
  batch.runs.scaleQ8_8.push_back(256);

  for (uint32_t i = 0; i < 20; ++i) {
    add_text(batch, posDist(rng), posDist(rng), 60, 12,
             PackRGBA8(Color{200, 200, 200, 255}), 0);
  }

  uint32_t width = 128;
  uint32_t height = 128;
  std::vector<uint8_t> bufferA(width * height * 4, 0);
  std::vector<uint8_t> bufferB(width * height * 4, 0);
  RenderTarget targetA{std::span<uint8_t>(bufferA), width, height, width * 4};
  RenderTarget targetB{std::span<uint8_t>(bufferB), width, height, width * 4};

  render_batch(targetA, batch);
  render_batch(targetB, batch);

  CHECK_MESSAGE(buffers_equal(bufferA, bufferB), "fuzz batch renders deterministically");
}

TEST_CASE("multithread_stress_deterministic") {
  RenderBatch batch;
  batch.tileSize = 16;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  std::mt19937 rng(7);
  std::uniform_int_distribution<int32_t> posDist(0, 200);
  std::uniform_int_distribution<int32_t> sizeDist(8, 32);

  for (uint32_t i = 0; i < 800; ++i) {
    int32_t x0 = posDist(rng);
    int32_t y0 = posDist(rng);
    int32_t x1 = x0 + sizeDist(rng);
    int32_t y1 = y0 + sizeDist(rng);
    add_rect(batch, x0, y0, x1, y1, PackRGBA8(Color{static_cast<uint8_t>(i % 255), 0, 0, 255}));
  }

  uint32_t width = 256;
  uint32_t height = 256;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  std::vector<uint8_t> reference;
  for (int i = 0; i < 8; ++i) {
    render_batch(target, batch);
    if (i == 0) {
      reference = buffer;
    } else {
      CHECK_MESSAGE(buffers_equal(buffer, reference), "stress render remains deterministic");
    }
  }
}

TEST_CASE("empty_batch_no_change") {
  RenderBatch batch;
  uint32_t width = 4;
  uint32_t height = 4;
  std::vector<uint8_t> buffer(width * height * 4, 0x7F);
  std::vector<uint8_t> original = buffer;
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  CHECK_MESSAGE(buffers_equal(buffer, original), "empty batch leaves buffer unchanged");
}

TEST_CASE("random_clip_rotation_mix") {
  RenderBatch batch;
  batch.tileSize = 16;
  add_clear(batch, PackRGBA8(Color{3, 3, 3, 255}));

  std::mt19937 rng(99);
  std::uniform_int_distribution<int32_t> posDist(0, 120);
  std::uniform_int_distribution<int32_t> sizeDist(8, 30);
  auto colors = build_test_colors();
  std::uniform_int_distribution<size_t> colorIndexDist(0, colors.size() - 1);
  std::uniform_int_distribution<uint32_t> colorDist(0, 255);
  std::uniform_int_distribution<int> flagDist(0, 1);
  std::uniform_int_distribution<int16_t> rotDist(-256, 256);

  for (uint32_t i = 0; i < 200; ++i) {
    int32_t x0 = posDist(rng);
    int32_t y0 = posDist(rng);
    int32_t x1 = x0 + sizeDist(rng);
    int32_t y1 = y0 + sizeDist(rng);
    uint32_t c0 = colors[colorIndexDist(rng)];
    uint32_t c1 = colors[colorIndexDist(rng)];

    uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
    batch.rects.x0.push_back(x0);
    batch.rects.y0.push_back(y0);
    batch.rects.x1.push_back(x1);
    batch.rects.y1.push_back(y1);
    batch.rects.colorIndex.push_back(palette_index(batch, c0));
    batch.rects.radiusQ8_8.push_back(static_cast<uint16_t>(sizeDist(rng)));
    batch.rects.rotationQ8_8.push_back(rotDist(rng));
    batch.rects.zQ8_8.push_back(0);
    batch.rects.opacity.push_back(255);
    uint8_t flags = 0;
    if (flagDist(rng)) flags |= RectFlagGradient;
    if (flagDist(rng)) flags |= RectFlagClip;
    batch.rects.flags.push_back(flags);
    batch.rects.gradientColor1Index.push_back(palette_index(batch, c1));
    batch.rects.gradientDirX.push_back(static_cast<int16_t>(colorDist(rng)));
    batch.rects.gradientDirY.push_back(static_cast<int16_t>(colorDist(rng)));
    int32_t cx0 = x0 + 2;
    int32_t cy0 = y0 + 2;
    int32_t cx1 = x1 - 2;
    int32_t cy1 = y1 - 2;
    batch.rects.clipX0.push_back(cx0);
    batch.rects.clipY0.push_back(cy0);
    batch.rects.clipX1.push_back(cx1);
    batch.rects.clipY1.push_back(cy1);
    batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
  }

  uint32_t width = 128;
  uint32_t height = 128;
  std::vector<uint8_t> bufferA(width * height * 4, 0);
  std::vector<uint8_t> bufferB(width * height * 4, 0);
  RenderTarget targetA{std::span<uint8_t>(bufferA), width, height, width * 4};
  RenderTarget targetB{std::span<uint8_t>(bufferB), width, height, width * 4};

  render_batch(targetA, batch);
  render_batch(targetB, batch);

  CHECK_MESSAGE(buffers_equal(bufferA, bufferB), "clip+rotation mix deterministic");
}

TEST_CASE("perf_smoke_guarded") {
  if (!std::getenv("PRIMEMANIFEST_PERF")) return;

  RenderBatch batch;
  batch.tileSize = 16;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  std::mt19937 rng(123);
  std::uniform_int_distribution<int32_t> posDist(0, 400);
  std::uniform_int_distribution<int32_t> sizeDist(6, 20);
  auto colors = build_test_colors();
  std::uniform_int_distribution<size_t> colorIndexDist(0, colors.size() - 1);

  for (uint32_t i = 0; i < 2000; ++i) {
    int32_t x0 = posDist(rng);
    int32_t y0 = posDist(rng);
    int32_t x1 = x0 + sizeDist(rng);
    int32_t y1 = y0 + sizeDist(rng);
    uint32_t c0 = colors[colorIndexDist(rng)];
    add_rect(batch, x0, y0, x1, y1, c0);
  }

  uint32_t width = 512;
  uint32_t height = 512;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 5; ++i) {
    render_batch(target, batch);
  }
  auto end = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration<double>(end - start).count();

  CHECK_MESSAGE(elapsed < 10.0, "perf smoke under 10s (guarded by PRIMEMANIFEST_PERF)");
}

TEST_SUITE_END();
