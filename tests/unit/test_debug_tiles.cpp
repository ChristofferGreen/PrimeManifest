#include "test_helpers.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(debug_tiles, draws_outline) {
  RenderBatch batch;
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{10, 10, 10, 255}));

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(1);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  PM_CHECK(pixel_at(buffer, width, 0, 0) == debugColor, "debug tiles draw outline pixel");
  PM_CHECK(pixel_at(buffer, width, 1, 1) == PackRGBA8(Color{10, 10, 10, 255}), "debug tiles leave interior");
}

PM_TEST(debug_tiles, dirty_only_limits_tiles) {
  RenderBatch batch;
  batch.tileSize = 8;
  add_rect(batch, 0, 0, 4, 4, PackRGBA8(Color{0, 0, 255, 255}));

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(1);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  PM_CHECK(pixel_at(buffer, width, 0, 0) == debugColor, "dirty-only tiles outline touched tile");
  PM_CHECK(pixel_at(buffer, width, 12, 0) == 0, "dirty-only tiles skip untouched tile");
}

PM_TEST(debug_tiles, draw_all_tiles) {
  RenderBatch batch;
  batch.tileSize = 8;

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(1);
  batch.debugTiles.flags.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 16;
  uint32_t height = 16;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  PM_CHECK(pixel_at(buffer, width, 0, 0) == debugColor, "debug tiles outline even without dirty");
}

PM_TEST(debug_tiles, line_width_two) {
  RenderBatch batch;
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(2);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  PM_CHECK(pixel_at(buffer, width, 0, 0) == debugColor, "debug line width draws outer border");
  PM_CHECK(pixel_at(buffer, width, 1, 1) == debugColor, "debug line width draws inner border");
}

PM_TEST(debug_tiles, line_width_zero_defaults_to_one) {
  RenderBatch batch;
  batch.tileSize = 8;
  add_clear(batch, PackRGBA8(Color{0, 0, 0, 255}));

  uint32_t debugColor = PackRGBA8(Color{255, 0, 0, 255});
  uint32_t idx = static_cast<uint32_t>(batch.debugTiles.colorIndex.size());
  batch.debugTiles.colorIndex.push_back(palette_index(batch, debugColor));
  batch.debugTiles.lineWidth.push_back(0);
  batch.debugTiles.flags.push_back(DebugTilesFlagDirtyOnly);
  batch.commands.push_back(RenderCommand{CommandType::DebugTiles, idx});

  uint32_t width = 8;
  uint32_t height = 8;
  std::vector<uint8_t> buffer(width * height * 4, 0);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  render_batch(target, batch);

  PM_CHECK(pixel_at(buffer, width, 0, 0) == debugColor, "debug line width 0 defaults to 1");
}
