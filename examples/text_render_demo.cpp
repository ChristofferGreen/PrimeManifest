#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"
#include "PrimeManifest/text/FontRegistry.hpp"
#include "PrimeManifest/text/TextBake.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace PrimeManifestDemo {

using namespace PrimeManifest;

bool write_ppm(std::string const& path, RenderTarget const& target) {
  if (path.empty() || target.width == 0 || target.height == 0 || target.strideBytes == 0) return false;
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  out << "P6\n" << target.width << " " << target.height << "\n255\n";
  for (uint32_t y = 0; y < target.height; ++y) {
    auto const* row = target.data.data() + static_cast<size_t>(y) * target.strideBytes;
    for (uint32_t x = 0; x < target.width; ++x) {
      size_t idx = static_cast<size_t>(x) * 4u;
      out.put(static_cast<char>(row[idx + 0]));
      out.put(static_cast<char>(row[idx + 1]));
      out.put(static_cast<char>(row[idx + 2]));
    }
  }
  return static_cast<bool>(out);
}

void add_clear(RenderBatch& batch, uint8_t colorIndex) {
  uint32_t idx = static_cast<uint32_t>(batch.clear.colorIndex.size());
  batch.clear.colorIndex.push_back(colorIndex);
  batch.commands.push_back(RenderCommand{CommandType::Clear, idx});
}

void add_rect(RenderBatch& batch,
              int32_t x0,
              int32_t y0,
              int32_t x1,
              int32_t y1,
              uint8_t colorIndex) {
  uint32_t idx = static_cast<uint32_t>(batch.rects.x0.size());
  batch.rects.x0.push_back(static_cast<int16_t>(x0));
  batch.rects.y0.push_back(static_cast<int16_t>(y0));
  batch.rects.x1.push_back(static_cast<int16_t>(x1));
  batch.rects.y1.push_back(static_cast<int16_t>(y1));
  batch.rects.colorIndex.push_back(colorIndex);
  batch.rects.radiusQ8_8.push_back(0);
  batch.rects.rotationQ8_8.push_back(0);
  batch.rects.zQ8_8.push_back(0);
  batch.rects.opacity.push_back(255);
  batch.rects.flags.push_back(0);
  batch.rects.gradientColor1Index.push_back(colorIndex);
  batch.rects.gradientDirX.push_back(0);
  batch.rects.gradientDirY.push_back(0);
  batch.rects.clipX0.push_back(0);
  batch.rects.clipY0.push_back(0);
  batch.rects.clipX1.push_back(0);
  batch.rects.clipY1.push_back(0);
  batch.commands.push_back(RenderCommand{CommandType::Rect, idx});
}

} // namespace PrimeManifestDemo

int main(int argc, char** argv) {
  using namespace PrimeManifest;
  using namespace PrimeManifestDemo;

  std::string outPath = "text_render_demo.ppm";
  std::vector<std::string> fontDirs;
  uint32_t width = 720;
  uint32_t height = 360;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--font-dir" && i + 1 < argc) {
      fontDirs.emplace_back(argv[++i]);
    } else if (arg == "--out" && i + 1 < argc) {
      outPath = argv[++i];
    } else if (arg == "--width" && i + 1 < argc) {
      width = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
    } else if (arg == "--height" && i + 1 < argc) {
      height = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
    }
  }

  auto& registry = GetFontRegistry();
  for (auto const& dir : fontDirs) {
    registry.addBundleDir(dir);
  }
  registry.loadBundledFonts();

  RenderBatch batch;
  batch.tileSize = 32;
  batch.palette.enabled = true;
  batch.palette.size = 6;
  batch.palette.colorRGBA8[0] = PackRGBA8(Color{18, 22, 30, 255});
  batch.palette.colorRGBA8[1] = PackRGBA8(Color{44, 52, 64, 255});
  batch.palette.colorRGBA8[2] = PackRGBA8(Color{234, 196, 53, 255});
  batch.palette.colorRGBA8[3] = PackRGBA8(Color{139, 173, 255, 255});
  batch.palette.colorRGBA8[4] = PackRGBA8(Color{238, 238, 238, 255});
  batch.palette.colorRGBA8[5] = PackRGBA8(Color{120, 127, 140, 255});

  add_clear(batch, 0);
  add_rect(batch, 0, 0, static_cast<int32_t>(width), 72, 1);
  add_rect(batch, 0, static_cast<int32_t>(height - 68), static_cast<int32_t>(width), static_cast<int32_t>(height), 1);

  Typography title;
  title.size = 28.0f;
  title.weight = 600;
  title.fallback = FontFallbackPolicy::BundleOnly;

  Typography body;
  body.size = 18.0f;
  body.weight = 400;
  body.fallback = FontFallbackPolicy::BundleOnly;

  bool textOk = true;
  textOk &= static_cast<bool>(AppendText(batch,
                                         "PrimeManifest text system",
                                         title,
                                         1.0f,
                                         24,
                                         36,
                                         4));
  textOk &= static_cast<bool>(AppendText(batch,
                                         "Glyph atlas + shaping + renderer",
                                         body,
                                         1.0f,
                                         24,
                                         118,
                                         3));
  textOk &= static_cast<bool>(AppendText(batch,
                                         "Palette indexed colors + tile rendering",
                                         body,
                                         1.0f,
                                         24,
                                         146,
                                         5));
  textOk &= static_cast<bool>(AppendText(batch,
                                         "Explicit font dirs only",
                                         body,
                                         1.0f,
                                         24,
                                         static_cast<int32_t>(height - 32),
                                         2));

  if (!textOk) {
    std::cerr << "warning: text layout failed. Provide --font-dir with loadable fonts.\n";
  }

  std::vector<uint8_t> framebuffer(static_cast<size_t>(width) * height * 4, 0);
  RenderTarget target;
  target.data = std::span<uint8_t>(framebuffer.data(), framebuffer.size());
  target.width = width;
  target.height = height;
  target.strideBytes = width * 4;

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  if (!write_ppm(outPath, target)) {
    std::cerr << "failed to write output: " << outPath << "\n";
    return 1;
  }

  std::cout << "wrote " << outPath << "\n";
  return 0;
}
