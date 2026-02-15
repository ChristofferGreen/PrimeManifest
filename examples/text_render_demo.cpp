#include "PrimeManifest/renderer/Optimizer2D.hpp"
#include "PrimeManifest/renderer/Renderer2D.hpp"
#include "PrimeManifest/text/FontRegistry.hpp"
#include "PrimeManifest/text/TextBake.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
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

std::vector<std::string> wrap_text(std::string_view text,
                                   Typography const& typography,
                                   int32_t maxWidth) {
  std::vector<std::string> lines;
  std::istringstream stream(std::string{text});
  std::string word;
  std::string line;

  while (stream >> word) {
    std::string candidate = line.empty() ? word : line + " " + word;
    auto [width, height] = MeasureText(candidate, typography);
    (void)height;
    if (width <= maxWidth || line.empty()) {
      line = std::move(candidate);
      if (width > maxWidth && line == word) {
        lines.push_back(line);
        line.clear();
      }
    } else {
      lines.push_back(line);
      line = word;
    }
  }

  if (!line.empty()) {
    lines.push_back(line);
  }

  return lines;
}

std::string clamp_line_with_ellipsis(std::string line,
                                     Typography const& typography,
                                     int32_t maxWidth) {
  if (line.empty()) return line;
  std::string candidate = line + "…";
  auto [width, height] = MeasureText(candidate, typography);
  (void)height;
  if (width <= maxWidth) return candidate;

  std::string trimmed = line;
  while (!trimmed.empty()) {
    auto lastSpace = trimmed.find_last_of(' ');
    if (lastSpace == std::string::npos) {
      trimmed.clear();
    } else {
      trimmed.erase(lastSpace);
    }
    if (trimmed.empty()) break;
    candidate = trimmed + "…";
    auto [candWidth, candHeight] = MeasureText(candidate, typography);
    (void)candHeight;
    if (candWidth <= maxWidth) return candidate;
  }
  return "…";
}

int32_t measure_line_height(Typography const& typography) {
  auto [width, height] = MeasureText("Mg", typography);
  (void)width;
  return std::max(1, height);
}

int32_t advance_for_line(std::string_view text,
                         Typography const& typography,
                         int32_t minHeight,
                         int32_t gap) {
  auto [width, height] = MeasureText(std::string{text}, typography);
  (void)width;
  int32_t lineHeight = std::max(height, minHeight);
  return std::max(1, lineHeight + gap);
}

bool add_shadowed_text(RenderBatch& batch,
                       std::string_view text,
                       Typography const& typography,
                       float deviceScale,
                       int32_t x,
                       int32_t y,
                       uint8_t colorIndex,
                       uint8_t shadowColorIndex,
                       int32_t shadowDx,
                       int32_t shadowDy,
                       uint8_t shadowOpacity = 140) {
  bool ok = true;
  ok &= static_cast<bool>(AppendText(batch,
                                     text,
                                     typography,
                                     deviceScale,
                                     x + shadowDx,
                                     y + shadowDy,
                                     shadowColorIndex,
                                     shadowOpacity));
  ok &= static_cast<bool>(AppendText(batch,
                                     text,
                                     typography,
                                     deviceScale,
                                     x,
                                     y,
                                     colorIndex));
  return ok;
}

bool add_rainbow_text(RenderBatch& batch,
                      std::string_view text,
                      Typography const& typography,
                      float deviceScale,
                      int32_t x,
                      int32_t lineTop,
                      float baselineRef,
                      std::vector<uint8_t> const& colors,
                      uint8_t fallbackColor) {
  std::istringstream stream(std::string{text});
  std::string word;
  int32_t penX = x;
  auto [spaceWidth, spaceHeight] = MeasureText(" ", typography);
  (void)spaceHeight;

  bool ok = true;
  size_t idx = 0;
  while (stream >> word) {
    uint8_t color = colors.empty() ? fallbackColor : colors[idx % colors.size()];
    auto run = LayoutText(word, typography, deviceScale, true);
    if (run) {
      int32_t y = lineTop +
                  static_cast<int32_t>(std::lround(baselineRef - run->baseline * run->layoutScale));
      ok &= static_cast<bool>(AppendTextRun(batch, *run, penX, y, color));
      penX += static_cast<int32_t>(std::lround(run->width)) + spaceWidth;
    } else {
      ok &= static_cast<bool>(AppendText(batch, word, typography, deviceScale, penX, lineTop, color));
      auto [wordWidth, wordHeight] = MeasureText(word, typography);
      (void)wordHeight;
      penX += wordWidth + spaceWidth;
    }
    ++idx;
  }
  return ok;
}

bool append_text_with_baseline(RenderBatch& batch,
                               std::string_view text,
                               Typography const& typography,
                               float deviceScale,
                               int32_t x,
                               int32_t lineTop,
                               float baselineRef,
                               uint8_t colorIndex,
                               uint8_t opacity = 255) {
  auto run = LayoutText(text, typography, deviceScale, true);
  if (!run) {
    return static_cast<bool>(AppendText(batch, text, typography, deviceScale, x, lineTop, colorIndex, opacity));
  }
  int32_t y = lineTop + static_cast<int32_t>(std::lround(baselineRef - run->baseline * run->layoutScale));
  return static_cast<bool>(AppendTextRun(batch, *run, x, y, colorIndex, opacity));
}

bool add_label_and_emoji(RenderBatch& batch,
                         std::string_view label,
                         std::string_view emojiText,
                         Typography const& labelType,
                         Typography const& emojiType,
                         float deviceScale,
                         int32_t x,
                         int32_t lineTop,
                         float baselineRef,
                         uint8_t labelColor,
                         uint8_t emojiColor) {
  bool ok = true;
  ok &= append_text_with_baseline(batch, label, labelType, deviceScale, x, lineTop, baselineRef, labelColor);
  auto [labelWidth, labelHeight] = MeasureText(std::string{label}, labelType);
  (void)labelHeight;
  int32_t emojiX = x + labelWidth + 8;
  auto result = LayoutText(emojiText, emojiType, deviceScale, true);
  if (result) {
    int32_t emojiY =
        lineTop + static_cast<int32_t>(std::lround(baselineRef - result->baseline * result->layoutScale));
    ok &= static_cast<bool>(AppendTextRun(batch, *result, emojiX, emojiY, emojiColor));
    return ok;
  }
  ok &= append_text_with_baseline(batch, emojiText, labelType, deviceScale, emojiX, lineTop, baselineRef, emojiColor);
  return ok;
}

} // namespace PrimeManifestDemo

int main(int argc, char** argv) {
  using namespace PrimeManifest;
  using namespace PrimeManifestDemo;

  std::string outPath = "text_render_demo.ppm";
  std::vector<std::string> fontDirs;
  uint32_t width = 960;
  uint32_t height = 600;

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
    registry.addOsFallbackDir(dir);
  }
  registry.loadBundledFonts();
  registry.loadOsFallbackFonts();

  RenderBatch batch;
  batch.tileSize = 32;
  batch.assumeFrontToBack = false;
  batch.palette.enabled = true;
  batch.palette.size = 12;
  batch.palette.colorRGBA8[0] = PackRGBA8(Color{18, 22, 30, 255});
  batch.palette.colorRGBA8[1] = PackRGBA8(Color{44, 52, 64, 255});
  batch.palette.colorRGBA8[2] = PackRGBA8(Color{234, 196, 53, 255});
  batch.palette.colorRGBA8[3] = PackRGBA8(Color{139, 173, 255, 255});
  batch.palette.colorRGBA8[4] = PackRGBA8(Color{238, 238, 238, 255});
  batch.palette.colorRGBA8[5] = PackRGBA8(Color{120, 127, 140, 255});
  batch.palette.colorRGBA8[6] = PackRGBA8(Color{86, 192, 208, 255});
  batch.palette.colorRGBA8[7] = PackRGBA8(Color{242, 132, 130, 255});
  batch.palette.colorRGBA8[8] = PackRGBA8(Color{152, 220, 120, 255});
  batch.palette.colorRGBA8[9] = PackRGBA8(Color{255, 168, 88, 255});
  batch.palette.colorRGBA8[10] = PackRGBA8(Color{189, 148, 245, 255});
  batch.palette.colorRGBA8[11] = PackRGBA8(Color{28, 32, 40, 255});

  add_clear(batch, 0);
  int32_t topBar = 84;
  int32_t bottomBar = 84;
  add_rect(batch, 0, 0, static_cast<int32_t>(width), topBar, 1);
  add_rect(batch,
           0,
           static_cast<int32_t>(height - bottomBar),
           static_cast<int32_t>(width),
           static_cast<int32_t>(height),
           1);

  Typography title;
  title.size = 28.0f;
  title.weight = 600;
  title.fallback = FontFallbackPolicy::BundleThenOS;
  title.lineHeight = title.size * 1.2f;

  Typography subtitle = title;
  subtitle.size = 16.0f;
  subtitle.weight = 500;
  subtitle.letterSpacing = 0.02f;
  subtitle.lineHeight = subtitle.size * 1.2f;

  Typography body;
  body.size = 18.0f;
  body.weight = 400;
  body.fallback = FontFallbackPolicy::BundleThenOS;
  body.lineHeight = body.size * 1.9f;

  Typography small = body;
  small.size = 16.0f;
  small.lineHeight = small.size * 1.4f;

  Typography rtl = body;
  rtl.locale = "ar";

  Typography emoji = body;
  emoji.family = "Apple Color Emoji";
  emoji.size = body.size * 1.2f;
  emoji.lineHeight = emoji.size * 1.2f;
  emoji.weight = 400;
  emoji.fallback = FontFallbackPolicy::BundleThenOS;

  std::string lorem =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore "
      "et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
      "aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse "
      "cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
      "culpa qui officia deserunt mollit anim id est laborum.";

  bool textOk = true;
  auto [titleW, titleH] = MeasureText("PrimeManifest Text Playground", title);
  auto [subtitleW, subtitleH] = MeasureText("Unicode, shaping, emoji, and palette-indexed color", subtitle);
  (void)titleW;
  (void)subtitleW;
  int32_t headerSpacing = 6;
  int32_t headerHeight = titleH + subtitleH + headerSpacing;
  int32_t headerStart = std::max(4, (topBar - headerHeight) / 2);
  int32_t titleY = headerStart;
  int32_t subtitleY = titleY + titleH + headerSpacing;

  textOk &= add_shadowed_text(batch,
                              "PrimeManifest Text Playground",
                              title,
                              1.0f,
                              24,
                              titleY,
                              4,
                              0,
                              1,
                              2);
  textOk &= static_cast<bool>(AppendText(batch,
                                         "Unicode, shaping, emoji, and palette-indexed color",
                                         subtitle,
                                         1.0f,
                                         24,
                                         subtitleY,
                                         5));

  int32_t textLeft = 24;
  int32_t maxWidth = static_cast<int32_t>(width) - 48;
  int32_t contentTop = std::max(topBar + 16, subtitleY + subtitleH + 16);
  int32_t contentBottom = static_cast<int32_t>(height) - bottomBar - 20;
  int32_t contentHeight = std::max(0, contentBottom - contentTop);

  constexpr int featureCount = 8;
  constexpr int minLoremLines = 3;
  int32_t baseGap = std::max(6, static_cast<int32_t>(std::lround(body.size * 0.3f)));
  int32_t baseLine = measure_line_height(body) + baseGap;
  float desiredLines = static_cast<float>(featureCount + minLoremLines);
  float scale = desiredLines > 0.0f ? static_cast<float>(contentHeight) / (desiredLines * baseLine) : 1.0f;
  scale = std::min(1.0f, std::max(0.75f, scale));
  if (scale < 1.0f) {
    body.size *= scale;
    body.lineHeight = body.size * 1.6f;
    small.size = std::max(12.0f, body.size - 2.0f);
    small.lineHeight = small.size * 1.35f;
    rtl.size = body.size;
    rtl.lineHeight = body.lineHeight;
  }

  add_rect(batch, textLeft - 10, contentTop - 4, textLeft - 4, contentBottom + 4, 2);

  int32_t y = contentTop;
  int32_t bodyMeasured = measure_line_height(body);
  int32_t bodyMinHeight = std::max(bodyMeasured,
                                   static_cast<int32_t>(std::lround(body.lineHeight)));
  int32_t gap = std::max(8, static_cast<int32_t>(std::lround(body.size * 0.45f)));
  float bodyBaseline = static_cast<float>(bodyMeasured);
  if (auto refRun = LayoutText("Mg", body, 1.0f, false)) {
    bodyBaseline = refRun->baseline * refRun->layoutScale;
  }
  std::vector<uint8_t> rainbow = {2, 9, 8, 6, 3, 10, 7};
  textOk &= add_rainbow_text(batch,
                             "Colorful words: red orange yellow green cyan blue violet coral",
                             body,
                             1.0f,
                             textLeft,
                             y,
                             bodyBaseline,
                             rainbow,
                             4);
  y += advance_for_line("Colorful words: red orange yellow green cyan blue violet coral",
                        body,
                        bodyMinHeight,
                        gap);

  textOk &= append_text_with_baseline(batch,
                                      "Accents: naïve façade coöperate déjà vu — São Paulo, Łódź, İstanbul",
                                      body,
                                      1.0f,
                                      textLeft,
                                      y,
                                      bodyBaseline,
                                      4);
  y += advance_for_line("Accents: naïve façade coöperate déjà vu — São Paulo, Łódź, İstanbul",
                        body,
                        bodyMinHeight,
                        gap);

  textOk &= append_text_with_baseline(batch,
                                      "Greek: Καλημέρα κόσμε · Ελληνικά · Ευρώπη",
                                      body,
                                      1.0f,
                                      textLeft,
                                      y,
                                      bodyBaseline,
                                      6);
  y += advance_for_line("Greek: Καλημέρα κόσμε · Ελληνικά · Ευρώπη",
                        body,
                        bodyMinHeight,
                        gap);

  textOk &= append_text_with_baseline(batch,
                                      "Cyrillic: Привет мир · До свидания · Москва",
                                      body,
                                      1.0f,
                                      textLeft,
                                      y,
                                      bodyBaseline,
                                      7);
  y += advance_for_line("Cyrillic: Привет мир · До свидания · Москва",
                        body,
                        bodyMinHeight,
                        gap);

  textOk &= append_text_with_baseline(batch,
                                      "CJK: 你好，世界 · こんにちは世界 · 안녕하세요",
                                      body,
                                      1.0f,
                                      textLeft,
                                      y,
                                      bodyBaseline,
                                      8);
  y += advance_for_line("CJK: 你好，世界 · こんにちは世界 · 안녕하세요",
                        body,
                        bodyMinHeight,
                        gap);

  textOk &= append_text_with_baseline(batch,
                                      "Arabic (RTL): مرحبا بالعالم",
                                      rtl,
                                      1.0f,
                                      textLeft,
                                      y,
                                      bodyBaseline,
                                      9);
  y += advance_for_line("Arabic (RTL): مرحبا بالعالم",
                        rtl,
                        std::max(bodyMinHeight, measure_line_height(rtl)),
                        gap);

  textOk &= append_text_with_baseline(batch,
                                      "Math: ∑ ∫ √ π ≈ ≤ ≥ ± ∞ · Symbols: ✓ ✔ ✗ ★ ♫ — • …",
                                      body,
                                      1.0f,
                                      textLeft,
                                      y,
                                      bodyBaseline,
                                      10);
  y += advance_for_line("Math: ∑ ∫ √ π ≈ ≤ ≥ ± ∞ · Symbols: ✓ ✔ ✗ ★ ♫ — • …",
                        body,
                        bodyMinHeight,
                        gap);

  textOk &= add_label_and_emoji(batch,
                                "Emoji:",
                                "😀 😎 🚀 ✨ 🧪 🎨 🧵",
                                body,
                                emoji,
                                1.0f,
                                textLeft,
                                y,
                                bodyBaseline,
                                3,
                                3);
  {
    auto [labelW, labelH] = MeasureText("Emoji:", body);
    auto [emojiW, emojiH] = MeasureText("😀 😎 🚀 ✨ 🧪 🎨 🧵", emoji);
    (void)labelW;
    (void)emojiW;
    int32_t lineH = std::max({labelH, emojiH, bodyMinHeight});
    y += std::max(1, lineH + gap);
  }

  int32_t loremTop = y + 8;
  int32_t loremInset = 6;
  int32_t loremTextX = textLeft + loremInset;
  int32_t loremMaxWidth = std::max(0, maxWidth - loremInset);
  int32_t smallAdvance = small.lineHeight > 0.0f ? static_cast<int32_t>(std::lround(small.lineHeight))
                                                 : measure_line_height(small);
  float loremAdvance = static_cast<float>(smallAdvance);
  int32_t remaining = contentBottom - loremTop;
  int32_t maxLines = remaining > 0 ? static_cast<int32_t>(remaining / smallAdvance) : 0;
  auto lines = wrap_text(lorem, small, loremMaxWidth);
  if (maxLines >= 2) {
    int32_t lineCount = std::min<int32_t>(static_cast<int32_t>(lines.size()), maxLines);
    if (lineCount > 0 && lineCount < static_cast<int32_t>(lines.size())) {
      lines[static_cast<size_t>(lineCount - 1)] =
          clamp_line_with_ellipsis(lines[static_cast<size_t>(lineCount - 1)], small, loremMaxWidth);
    }

    int32_t loremHeight = static_cast<int32_t>(std::lround(lineCount * loremAdvance));
    add_rect(batch,
             textLeft + 2,
             loremTop - 6,
             textLeft + maxWidth + 8,
             loremTop + loremHeight + 6,
             11);

    int32_t loremY = loremTop;
    for (int32_t i = 0; i < lineCount; ++i) {
      textOk &= static_cast<bool>(AppendText(batch,
                                             lines[static_cast<size_t>(i)],
                                             small,
                                             1.0f,
                                             loremTextX,
                                             loremY,
                                             4));
      loremY += static_cast<int32_t>(std::lround(loremAdvance));
    }
  }

  textOk &= static_cast<bool>(AppendText(batch,
                                         "Bundled fonts only (pass --font-dir to load fonts)",
                                         small,
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
