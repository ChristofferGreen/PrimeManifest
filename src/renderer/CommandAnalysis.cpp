#include "CommandAnalysis.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace PrimeManifest {
namespace {

auto applyOpacity(uint8_t alpha, uint8_t opacity) -> uint8_t {
  uint16_t value = static_cast<uint16_t>(alpha) * static_cast<uint16_t>(opacity);
  value = static_cast<uint16_t>((value + 127u) / 255u);
  return static_cast<uint8_t>(std::min<uint16_t>(value, 255u));
}

auto combinedAlphaIsZero(uint8_t alpha, uint8_t opacity) -> bool {
  uint16_t combined = static_cast<uint16_t>(alpha) * static_cast<uint16_t>(opacity);
  return ((combined + 127u) / 255u) == 0u;
}

template <typename IndexStore>
auto fetchColor(RenderBatch const& batch,
                IndexStore const& indices,
                uint32_t idx,
                uint32_t fallback = 0u) -> uint32_t {
  if (idx >= indices.size()) return fallback;
  uint8_t paletteIndex = indices[idx];
  if (paletteIndex >= batch.palette.size) return fallback;
  return batch.palette.colorRGBA8[paletteIndex];
}

auto finalizeCommand(AnalyzedCommand& analyzed, CommandAnalysisConfig const& config) -> void {
  if (analyzed.x1 <= analyzed.x0 || analyzed.y1 <= analyzed.y0) return;
  if (analyzed.x1 <= 0 || analyzed.y1 <= 0) return;
  if (analyzed.x0 >= static_cast<int32_t>(config.targetWidth) ||
      analyzed.y0 >= static_cast<int32_t>(config.targetHeight)) {
    return;
  }

  analyzed.x0 = std::max<int32_t>(analyzed.x0, 0);
  analyzed.y0 = std::max<int32_t>(analyzed.y0, 0);
  analyzed.x1 = std::min<int32_t>(analyzed.x1, static_cast<int32_t>(config.targetWidth));
  analyzed.y1 = std::min<int32_t>(analyzed.y1, static_cast<int32_t>(config.targetHeight));
  if (analyzed.x1 <= analyzed.x0 || analyzed.y1 <= analyzed.y0) return;

  uint32_t tileSize = config.tileSize == 0 ? 1u : config.tileSize;
  if (config.tilePow2) {
    analyzed.tx0 = static_cast<uint32_t>(analyzed.x0) >> config.tileShift;
    analyzed.ty0 = static_cast<uint32_t>(analyzed.y0) >> config.tileShift;
    analyzed.tx1 = static_cast<uint32_t>(analyzed.x1 - 1) >> config.tileShift;
    analyzed.ty1 = static_cast<uint32_t>(analyzed.y1 - 1) >> config.tileShift;
  } else {
    analyzed.tx0 = static_cast<uint32_t>(analyzed.x0) / tileSize;
    analyzed.ty0 = static_cast<uint32_t>(analyzed.y0) / tileSize;
    analyzed.tx1 = static_cast<uint32_t>(analyzed.x1 - 1) / tileSize;
    analyzed.ty1 = static_cast<uint32_t>(analyzed.y1 - 1) / tileSize;
  }

  analyzed.valid = true;
}

} // namespace

void analyzeCommands(RenderBatch const& batch,
                     CommandAnalysisConfig const& config,
                     std::vector<AnalyzedCommand>& out) {
  out.assign(batch.commands.size(), AnalyzedCommand{});
  for (uint32_t order = 0; order < batch.commands.size(); ++order) {
    RenderCommand const& cmd = batch.commands[order];
    AnalyzedCommand analyzed{};
    analyzed.type = cmd.type;
    analyzed.index = cmd.index;
    analyzed.order = order;

    switch (cmd.type) {
      case CommandType::Rect: {
        if (cmd.index >= batch.rects.x0.size() ||
            cmd.index >= batch.rects.y0.size() ||
            cmd.index >= batch.rects.x1.size() ||
            cmd.index >= batch.rects.y1.size() ||
            cmd.index >= batch.rects.colorIndex.size()) {
          break;
        }
        uint8_t flags = cmd.index < batch.rects.flags.size() ? batch.rects.flags[cmd.index] : 0u;
        uint8_t opacity = cmd.index < batch.rects.opacity.size() ? batch.rects.opacity[cmd.index] : 255u;
        if (opacity == 0u) break;

        bool hasGradient = (flags & RectFlagGradient) != 0u;
        if (hasGradient && cmd.index >= batch.rects.gradientColor1Index.size()) break;

        uint8_t colorAlpha = 255u;
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.rects.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (opacity != 255u && combinedAlphaIsZero(colorAlpha, opacity)) break;
          if (!hasGradient) {
            if (colorAlpha == 0u) break;
          } else {
            uint32_t gradientColor = fetchColor(batch, batch.rects.gradientColor1Index, cmd.index, 0u);
            uint8_t gradientAlpha = static_cast<uint8_t>((gradientColor >> 24) & 0xFFu);
            if (colorAlpha == 0u && gradientAlpha == 0u) break;
          }
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, opacity);

        analyzed.x0 = batch.rects.x0[cmd.index];
        analyzed.y0 = batch.rects.y0[cmd.index];
        analyzed.x1 = batch.rects.x1[cmd.index];
        analyzed.y1 = batch.rects.y1[cmd.index];

        if ((flags & RectFlagClip) != 0u &&
            cmd.index < batch.rects.clipX0.size() &&
            cmd.index < batch.rects.clipY0.size() &&
            cmd.index < batch.rects.clipX1.size() &&
            cmd.index < batch.rects.clipY1.size()) {
          analyzed.clipEnabled = true;
          analyzed.clip.x0 = batch.rects.clipX0[cmd.index];
          analyzed.clip.y0 = batch.rects.clipY0[cmd.index];
          analyzed.clip.x1 = batch.rects.clipX1[cmd.index];
          analyzed.clip.y1 = batch.rects.clipY1[cmd.index];
          analyzed.x0 = std::max<int32_t>(analyzed.x0, analyzed.clip.x0);
          analyzed.y0 = std::max<int32_t>(analyzed.y0, analyzed.clip.y0);
          analyzed.x1 = std::min<int32_t>(analyzed.x1, analyzed.clip.x1);
          analyzed.y1 = std::min<int32_t>(analyzed.y1, analyzed.clip.y1);
        }
        finalizeCommand(analyzed, config);
      } break;

      case CommandType::Circle: {
        if (cmd.index >= batch.circles.centerX.size() ||
            cmd.index >= batch.circles.centerY.size() ||
            cmd.index >= batch.circles.radius.size() ||
            cmd.index >= batch.circles.colorIndex.size()) {
          break;
        }
        uint8_t colorAlpha = 255u;
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.circles.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (colorAlpha == 0u) break;
        }
        analyzed.baseAlpha = colorAlpha;

        int32_t cx = batch.circles.centerX[cmd.index];
        int32_t cy = batch.circles.centerY[cmd.index];
        int32_t radius = static_cast<int32_t>(batch.circles.radius[cmd.index]);
        int32_t pad = static_cast<int32_t>(batch.circleBoundsPad);
        analyzed.x0 = cx - radius - pad;
        analyzed.y0 = cy - radius - pad;
        analyzed.x1 = cx + radius + 1 + pad;
        analyzed.y1 = cy + radius + 1 + pad;
        finalizeCommand(analyzed, config);
      } break;

      case CommandType::Text: {
        if (cmd.index >= batch.text.x.size() ||
            cmd.index >= batch.text.y.size() ||
            cmd.index >= batch.text.width.size() ||
            cmd.index >= batch.text.height.size() ||
            cmd.index >= batch.text.colorIndex.size() ||
            cmd.index >= batch.text.opacity.size()) {
          break;
        }
        uint8_t opacity = batch.text.opacity[cmd.index];
        if (opacity == 0u) break;

        uint8_t colorAlpha = 255u;
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.text.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (opacity != 255u && combinedAlphaIsZero(colorAlpha, opacity)) break;
          if (opacity == 255u && colorAlpha == 0u) break;
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, opacity);

        analyzed.x0 = batch.text.x[cmd.index];
        analyzed.y0 = batch.text.y[cmd.index];
        analyzed.x1 = analyzed.x0 + batch.text.width[cmd.index];
        analyzed.y1 = analyzed.y0 + batch.text.height[cmd.index];

        uint8_t flags = cmd.index < batch.text.flags.size() ? batch.text.flags[cmd.index] : 0u;
        if ((flags & TextFlagClip) != 0u &&
            cmd.index < batch.text.clipX0.size() &&
            cmd.index < batch.text.clipY0.size() &&
            cmd.index < batch.text.clipX1.size() &&
            cmd.index < batch.text.clipY1.size()) {
          analyzed.clipEnabled = true;
          analyzed.clip.x0 = batch.text.clipX0[cmd.index];
          analyzed.clip.y0 = batch.text.clipY0[cmd.index];
          analyzed.clip.x1 = batch.text.clipX1[cmd.index];
          analyzed.clip.y1 = batch.text.clipY1[cmd.index];
          analyzed.x0 = std::max<int32_t>(analyzed.x0, analyzed.clip.x0);
          analyzed.y0 = std::max<int32_t>(analyzed.y0, analyzed.clip.y0);
          analyzed.x1 = std::min<int32_t>(analyzed.x1, analyzed.clip.x1);
          analyzed.y1 = std::min<int32_t>(analyzed.y1, analyzed.clip.y1);
        }

        finalizeCommand(analyzed, config);
      } break;

      case CommandType::SetPixel: {
        if (cmd.index >= batch.pixels.x.size() ||
            cmd.index >= batch.pixels.y.size() ||
            cmd.index >= batch.pixels.colorIndex.size()) {
          break;
        }

        uint8_t colorAlpha = 255u;
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.pixels.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
        }
        analyzed.baseAlpha = colorAlpha;

        analyzed.x0 = batch.pixels.x[cmd.index];
        analyzed.y0 = batch.pixels.y[cmd.index];
        analyzed.x1 = analyzed.x0 + 1;
        analyzed.y1 = analyzed.y0 + 1;
        finalizeCommand(analyzed, config);
      } break;

      case CommandType::SetPixelA: {
        if (cmd.index >= batch.pixelsA.x.size() ||
            cmd.index >= batch.pixelsA.y.size() ||
            cmd.index >= batch.pixelsA.colorIndex.size() ||
            cmd.index >= batch.pixelsA.alpha.size()) {
          break;
        }
        uint8_t alpha = batch.pixelsA.alpha[cmd.index];
        if (alpha == 0u) break;

        uint8_t colorAlpha = 255u;
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.pixelsA.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (alpha != 255u) {
            if (combinedAlphaIsZero(colorAlpha, alpha)) break;
          } else if (colorAlpha == 0u) {
            break;
          }
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, alpha);

        analyzed.x0 = batch.pixelsA.x[cmd.index];
        analyzed.y0 = batch.pixelsA.y[cmd.index];
        analyzed.x1 = analyzed.x0 + 1;
        analyzed.y1 = analyzed.y0 + 1;
        finalizeCommand(analyzed, config);
      } break;

      case CommandType::Line: {
        if (cmd.index >= batch.lines.x0.size() ||
            cmd.index >= batch.lines.y0.size() ||
            cmd.index >= batch.lines.x1.size() ||
            cmd.index >= batch.lines.y1.size() ||
            cmd.index >= batch.lines.widthQ8_8.size() ||
            cmd.index >= batch.lines.colorIndex.size() ||
            cmd.index >= batch.lines.opacity.size()) {
          break;
        }
        uint16_t widthQ = batch.lines.widthQ8_8[cmd.index];
        uint8_t opacity = batch.lines.opacity[cmd.index];
        if (widthQ == 0u || opacity == 0u) break;

        uint8_t colorAlpha = 255u;
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.lines.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (opacity != 255u) {
            if (combinedAlphaIsZero(colorAlpha, opacity)) break;
          } else if (colorAlpha == 0u) {
            break;
          }
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, opacity);

        float fx0 = static_cast<float>(batch.lines.x0[cmd.index]);
        float fy0 = static_cast<float>(batch.lines.y0[cmd.index]);
        float fx1 = static_cast<float>(batch.lines.x1[cmd.index]);
        float fy1 = static_cast<float>(batch.lines.y1[cmd.index]);
        float widthPx = static_cast<float>(widthQ) / 256.0f;
        float radius = widthPx * 0.5f;
        float pad = radius + 1.0f;
        analyzed.x0 = static_cast<int32_t>(std::floor(std::min(fx0, fx1) - pad));
        analyzed.y0 = static_cast<int32_t>(std::floor(std::min(fy0, fy1) - pad));
        analyzed.x1 = static_cast<int32_t>(std::ceil(std::max(fx0, fx1) + pad));
        analyzed.y1 = static_cast<int32_t>(std::ceil(std::max(fy0, fy1) + pad));
        finalizeCommand(analyzed, config);
      } break;

      case CommandType::Image: {
        if (cmd.index >= batch.imageDraws.x0.size() ||
            cmd.index >= batch.imageDraws.y0.size() ||
            cmd.index >= batch.imageDraws.x1.size() ||
            cmd.index >= batch.imageDraws.y1.size() ||
            cmd.index >= batch.imageDraws.srcX0.size() ||
            cmd.index >= batch.imageDraws.srcY0.size() ||
            cmd.index >= batch.imageDraws.srcX1.size() ||
            cmd.index >= batch.imageDraws.srcY1.size() ||
            cmd.index >= batch.imageDraws.imageIndex.size() ||
            cmd.index >= batch.imageDraws.tintColorIndex.size() ||
            cmd.index >= batch.imageDraws.opacity.size()) {
          break;
        }
        uint8_t opacity = batch.imageDraws.opacity[cmd.index];
        if (opacity == 0u) break;

        uint8_t colorAlpha = 255u;
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.imageDraws.tintColorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (opacity != 255u) {
            if (combinedAlphaIsZero(colorAlpha, opacity)) break;
          } else if (colorAlpha == 0u) {
            break;
          }
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, opacity);

        analyzed.x0 = batch.imageDraws.x0[cmd.index];
        analyzed.y0 = batch.imageDraws.y0[cmd.index];
        analyzed.x1 = batch.imageDraws.x1[cmd.index];
        analyzed.y1 = batch.imageDraws.y1[cmd.index];

        uint8_t flags = cmd.index < batch.imageDraws.flags.size() ? batch.imageDraws.flags[cmd.index] : 0u;
        if ((flags & ImageFlagClip) != 0u &&
            cmd.index < batch.imageDraws.clipX0.size() &&
            cmd.index < batch.imageDraws.clipY0.size() &&
            cmd.index < batch.imageDraws.clipX1.size() &&
            cmd.index < batch.imageDraws.clipY1.size()) {
          analyzed.clipEnabled = true;
          analyzed.clip.x0 = batch.imageDraws.clipX0[cmd.index];
          analyzed.clip.y0 = batch.imageDraws.clipY0[cmd.index];
          analyzed.clip.x1 = batch.imageDraws.clipX1[cmd.index];
          analyzed.clip.y1 = batch.imageDraws.clipY1[cmd.index];
          analyzed.x0 = std::max<int32_t>(analyzed.x0, analyzed.clip.x0);
          analyzed.y0 = std::max<int32_t>(analyzed.y0, analyzed.clip.y0);
          analyzed.x1 = std::min<int32_t>(analyzed.x1, analyzed.clip.x1);
          analyzed.y1 = std::min<int32_t>(analyzed.y1, analyzed.clip.y1);
        }
        finalizeCommand(analyzed, config);
      } break;

      case CommandType::Clear:
      case CommandType::ClearPattern:
      case CommandType::DebugTiles:
      default:
        break;
    }

    out[order] = analyzed;
  }
}

} // namespace PrimeManifest
