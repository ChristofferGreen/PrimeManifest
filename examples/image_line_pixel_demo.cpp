#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace PrimeManifest;

// Image source: Wikimedia Commons "Image icon" (public domain / PD textlogo).
constexpr char kImagePath[] = "assets/images/image_icon_256.rgba";
constexpr uint32_t kImageWidth = 256;
constexpr uint32_t kImageHeight = 256;

auto write_ppm(std::string const& path, RenderTarget const& target) -> bool {
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

auto read_file(std::string const& path, std::vector<uint8_t>& out) -> bool {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) return false;
  std::streamsize size = in.tellg();
  if (size <= 0) return false;
  out.resize(static_cast<size_t>(size));
  in.seekg(0, std::ios::beg);
  return static_cast<bool>(in.read(reinterpret_cast<char*>(out.data()), size));
}

auto find_asset_path(std::string const& relative) -> std::optional<std::string> {
  std::vector<std::string> probes = {
    relative,
    std::string("../") + relative,
    std::string("../../") + relative,
  };
  for (auto const& path : probes) {
    std::ifstream probe(path, std::ios::binary);
    if (probe.good()) return path;
  }
  return std::nullopt;
}

inline auto mul_div_255(uint8_t v, uint8_t a) -> uint8_t {
  return static_cast<uint8_t>((static_cast<uint16_t>(v) * a + 127u) / 255u);
}

auto palette_index(RenderBatch& batch, uint32_t color) -> uint8_t {
  if (!batch.palette.enabled) {
    batch.palette.enabled = true;
    batch.palette.size = 0;
    batch.palette.colorRGBA8.fill(0u);
  }
  for (uint16_t i = 0; i < batch.palette.size; ++i) {
    if (batch.palette.colorRGBA8[i] == color) {
      return static_cast<uint8_t>(i);
    }
  }
  if (batch.palette.size >= batch.palette.colorRGBA8.size()) return 0;
  uint8_t idx = static_cast<uint8_t>(batch.palette.size++);
  batch.palette.colorRGBA8[idx] = color;
  return idx;
}

void add_clear(RenderBatch& batch, uint32_t color) {
  uint32_t idx = static_cast<uint32_t>(batch.clear.colorIndex.size());
  batch.clear.colorIndex.push_back(palette_index(batch, color));
  batch.commands.push_back(RenderCommand{CommandType::Clear, idx});
}

void add_set_pixel(RenderBatch& batch, int32_t x, int32_t y, uint32_t color) {
  uint32_t idx = static_cast<uint32_t>(batch.pixels.x.size());
  batch.pixels.x.push_back(static_cast<int16_t>(x));
  batch.pixels.y.push_back(static_cast<int16_t>(y));
  batch.pixels.colorIndex.push_back(palette_index(batch, color));
  batch.commands.push_back(RenderCommand{CommandType::SetPixel, idx});
}

void add_set_pixel_a(RenderBatch& batch, int32_t x, int32_t y, uint32_t color, uint8_t alpha) {
  uint32_t idx = static_cast<uint32_t>(batch.pixelsA.x.size());
  batch.pixelsA.x.push_back(static_cast<int16_t>(x));
  batch.pixelsA.y.push_back(static_cast<int16_t>(y));
  batch.pixelsA.colorIndex.push_back(palette_index(batch, color));
  batch.pixelsA.alpha.push_back(alpha);
  batch.commands.push_back(RenderCommand{CommandType::SetPixelA, idx});
}

void add_line(RenderBatch& batch,
              int32_t x0,
              int32_t y0,
              int32_t x1,
              int32_t y1,
              float width,
              uint32_t color,
              uint8_t opacity = 255) {
  uint32_t idx = static_cast<uint32_t>(batch.lines.x0.size());
  batch.lines.x0.push_back(static_cast<int16_t>(x0));
  batch.lines.y0.push_back(static_cast<int16_t>(y0));
  batch.lines.x1.push_back(static_cast<int16_t>(x1));
  batch.lines.y1.push_back(static_cast<int16_t>(y1));
  uint16_t widthQ8_8 = static_cast<uint16_t>(std::lround(width * 256.0f));
  batch.lines.widthQ8_8.push_back(widthQ8_8);
  batch.lines.colorIndex.push_back(palette_index(batch, color));
  batch.lines.opacity.push_back(opacity);
  batch.commands.push_back(RenderCommand{CommandType::Line, idx});
}

void add_image(RenderBatch& batch,
               uint32_t imageIndex,
               int32_t x0,
               int32_t y0,
               int32_t x1,
               int32_t y1,
               uint16_t srcX0,
               uint16_t srcY0,
               uint16_t srcX1,
               uint16_t srcY1,
               uint32_t tintColor,
               uint8_t opacity,
               uint8_t flags,
               IntRect clip) {
  uint32_t idx = static_cast<uint32_t>(batch.imageDraws.x0.size());
  batch.imageDraws.x0.push_back(static_cast<int16_t>(x0));
  batch.imageDraws.y0.push_back(static_cast<int16_t>(y0));
  batch.imageDraws.x1.push_back(static_cast<int16_t>(x1));
  batch.imageDraws.y1.push_back(static_cast<int16_t>(y1));
  batch.imageDraws.srcX0.push_back(srcX0);
  batch.imageDraws.srcY0.push_back(srcY0);
  batch.imageDraws.srcX1.push_back(srcX1);
  batch.imageDraws.srcY1.push_back(srcY1);
  batch.imageDraws.imageIndex.push_back(imageIndex);
  batch.imageDraws.tintColorIndex.push_back(palette_index(batch, tintColor));
  batch.imageDraws.opacity.push_back(opacity);
  batch.imageDraws.flags.push_back(flags);
  batch.imageDraws.clipX0.push_back(static_cast<int16_t>(clip.x0));
  batch.imageDraws.clipY0.push_back(static_cast<int16_t>(clip.y0));
  batch.imageDraws.clipX1.push_back(static_cast<int16_t>(clip.x1));
  batch.imageDraws.clipY1.push_back(static_cast<int16_t>(clip.y1));
  batch.commands.push_back(RenderCommand{CommandType::Image, idx});
}

} // namespace

int main() {
  using namespace PrimeManifest;

  constexpr uint32_t width = 640;
  constexpr uint32_t height = 360;
  std::vector<uint8_t> buffer(width * height * 4, 0u);
  RenderTarget target{std::span<uint8_t>(buffer), width, height, width * 4};

  RenderBatch batch;
  add_clear(batch, PackRGBA8(Color{18, 18, 24, 255}));

  auto assetPath = find_asset_path(kImagePath);
  if (!assetPath) {
    std::cerr << "missing image asset: " << kImagePath << "\n";
    return 1;
  }

  std::vector<uint8_t> imageData;
  if (!read_file(*assetPath, imageData)) {
    std::cerr << "failed to read image asset: " << *assetPath << "\n";
    return 1;
  }
  size_t expectedSize = static_cast<size_t>(kImageWidth) * kImageHeight * 4u;
  if (imageData.size() != expectedSize) {
    std::cerr << "unexpected image size: " << imageData.size() << " expected " << expectedSize << "\n";
    return 1;
  }

  for (size_t i = 0; i < imageData.size(); i += 4) {
    uint8_t a = imageData[i + 3];
    imageData[i + 0] = mul_div_255(imageData[i + 0], a);
    imageData[i + 1] = mul_div_255(imageData[i + 1], a);
    imageData[i + 2] = mul_div_255(imageData[i + 2], a);
  }

  uint32_t imageIndex = static_cast<uint32_t>(batch.images.width.size());
  batch.images.width.push_back(kImageWidth);
  batch.images.height.push_back(kImageHeight);
  batch.images.strideBytes.push_back(kImageWidth * 4u);
  batch.images.dataOffset.push_back(static_cast<uint32_t>(batch.images.data.size()));
  batch.images.data.insert(batch.images.data.end(), imageData.begin(), imageData.end());

  uint32_t white = PackRGBA8(Color{255, 255, 255, 255});
  uint32_t accent = PackRGBA8(Color{255, 190, 64, 255});
  uint32_t accentA = PackRGBA8(Color{255, 120, 64, 255});

  add_line(batch, 10, 15, 630, 15, 3.0f, accent, 200);
  add_line(batch, 10, 340, 630, 340, 3.0f, accent, 200);
  add_line(batch, 20, 30, 300, 310, 4.0f, white, 200);
  add_line(batch, 320, 310, 600, 30, 6.0f, accentA, 220);
  add_line(batch, 40, 320, 200, 320, 1.0f, white, 255);
  add_line(batch, 200, 60, 200, 200, 1.0f, white, 255);

  add_image(batch,
            imageIndex,
            20,
            40,
            276,
            296,
            0,
            0,
            kImageWidth,
            kImageHeight,
            white,
            255,
            0,
            IntRect{});

  add_image(batch,
            imageIndex,
            320,
            40,
            576,
            296,
            0,
            0,
            96,
            96,
            white,
            255,
            static_cast<uint8_t>(ImageFlagWrapU | ImageFlagWrapV),
            IntRect{});

  IntRect clip{360, 100, 540, 240};
  add_image(batch,
            imageIndex,
            320,
            40,
            576,
            296,
            64,
            64,
            192,
            192,
            PackRGBA8(Color{180, 220, 255, 255}),
            220,
            static_cast<uint8_t>(ImageFlagClip),
            clip);

  add_set_pixel(batch, 15, 350, PackRGBA8(Color{255, 255, 255, 255}));
  add_set_pixel_a(batch, 16, 350, PackRGBA8(Color{255, 0, 0, 255}), 128);
  for (int32_t y = 330; y < 338; ++y) {
    for (int32_t x = 600; x < 608; ++x) {
      add_set_pixel(batch, x, y, PackRGBA8(Color{255, 255, 255, 255}));
    }
  }
  for (int32_t y = 334; y < 336; ++y) {
    for (int32_t x = 604; x < 606; ++x) {
      add_set_pixel_a(batch, x, y, PackRGBA8(Color{255, 120, 64, 255}), 160);
    }
  }

  OptimizedBatch optimized;
  OptimizeRenderBatch(target, batch, optimized);
  RenderOptimized(target, batch, optimized);

  std::string outPath = "image_line_pixel_demo.ppm";
  if (!write_ppm(outPath, target)) {
    std::cerr << "failed to write " << outPath << "\n";
    return 1;
  }

  std::cout << "wrote " << outPath << "\n";
  return 0;
}
