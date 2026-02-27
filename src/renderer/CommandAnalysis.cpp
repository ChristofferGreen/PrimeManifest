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

void finalizePrimitiveBounds(PrimitiveBounds& bounds, uint32_t targetWidth, uint32_t targetHeight) {
  if (bounds.x1 <= bounds.x0 || bounds.y1 <= bounds.y0) return;
  if (bounds.x1 <= 0 || bounds.y1 <= 0) return;
  if (bounds.x0 >= static_cast<int32_t>(targetWidth) ||
      bounds.y0 >= static_cast<int32_t>(targetHeight)) {
    return;
  }

  bounds.x0 = std::max<int32_t>(bounds.x0, 0);
  bounds.y0 = std::max<int32_t>(bounds.y0, 0);
  bounds.x1 = std::min<int32_t>(bounds.x1, static_cast<int32_t>(targetWidth));
  bounds.y1 = std::min<int32_t>(bounds.y1, static_cast<int32_t>(targetHeight));
  bounds.valid = (bounds.x1 > bounds.x0 && bounds.y1 > bounds.y0);
}

void finalizeAnalyzedCommand(AnalyzedCommand& analyzed, CommandAnalysisConfig const& config) {
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

void computePrimitiveBounds(RenderBatch const& batch,
                            CommandType type,
                            uint32_t index,
                            uint32_t targetWidth,
                            uint32_t targetHeight,
                            PrimitiveBounds& out) {
  out = PrimitiveBounds{};
  switch (type) {
    case CommandType::Rect: {
      if (index >= batch.rects.x0.size() ||
          index >= batch.rects.y0.size() ||
          index >= batch.rects.x1.size() ||
          index >= batch.rects.y1.size() ||
          index >= batch.rects.colorIndex.size()) {
        return;
      }
      out.x0 = batch.rects.x0[index];
      out.y0 = batch.rects.y0[index];
      out.x1 = batch.rects.x1[index];
      out.y1 = batch.rects.y1[index];
      uint8_t flags = index < batch.rects.flags.size() ? batch.rects.flags[index] : 0u;
      if ((flags & RectFlagClip) != 0u &&
          index < batch.rects.clipX0.size() &&
          index < batch.rects.clipY0.size() &&
          index < batch.rects.clipX1.size() &&
          index < batch.rects.clipY1.size()) {
        out.clipEnabled = true;
        out.clip.x0 = batch.rects.clipX0[index];
        out.clip.y0 = batch.rects.clipY0[index];
        out.clip.x1 = batch.rects.clipX1[index];
        out.clip.y1 = batch.rects.clipY1[index];
        out.x0 = std::max<int32_t>(out.x0, out.clip.x0);
        out.y0 = std::max<int32_t>(out.y0, out.clip.y0);
        out.x1 = std::min<int32_t>(out.x1, out.clip.x1);
        out.y1 = std::min<int32_t>(out.y1, out.clip.y1);
      }
    } break;

    case CommandType::Circle: {
      if (index >= batch.circles.centerX.size() ||
          index >= batch.circles.centerY.size() ||
          index >= batch.circles.radius.size() ||
          index >= batch.circles.colorIndex.size()) {
        return;
      }
      int32_t cx = batch.circles.centerX[index];
      int32_t cy = batch.circles.centerY[index];
      int32_t radius = static_cast<int32_t>(batch.circles.radius[index]);
      int32_t pad = static_cast<int32_t>(batch.circleBoundsPad);
      out.x0 = cx - radius - pad;
      out.y0 = cy - radius - pad;
      out.x1 = cx + radius + 1 + pad;
      out.y1 = cy + radius + 1 + pad;
    } break;

    case CommandType::Text: {
      if (index >= batch.text.x.size() ||
          index >= batch.text.y.size() ||
          index >= batch.text.width.size() ||
          index >= batch.text.height.size() ||
          index >= batch.text.colorIndex.size() ||
          index >= batch.text.opacity.size()) {
        return;
      }
      out.x0 = batch.text.x[index];
      out.y0 = batch.text.y[index];
      out.x1 = out.x0 + batch.text.width[index];
      out.y1 = out.y0 + batch.text.height[index];
      uint8_t flags = index < batch.text.flags.size() ? batch.text.flags[index] : 0u;
      if ((flags & TextFlagClip) != 0u &&
          index < batch.text.clipX0.size() &&
          index < batch.text.clipY0.size() &&
          index < batch.text.clipX1.size() &&
          index < batch.text.clipY1.size()) {
        out.clipEnabled = true;
        out.clip.x0 = batch.text.clipX0[index];
        out.clip.y0 = batch.text.clipY0[index];
        out.clip.x1 = batch.text.clipX1[index];
        out.clip.y1 = batch.text.clipY1[index];
        out.x0 = std::max<int32_t>(out.x0, out.clip.x0);
        out.y0 = std::max<int32_t>(out.y0, out.clip.y0);
        out.x1 = std::min<int32_t>(out.x1, out.clip.x1);
        out.y1 = std::min<int32_t>(out.y1, out.clip.y1);
      }
    } break;

    case CommandType::SetPixel: {
      if (index >= batch.pixels.x.size() ||
          index >= batch.pixels.y.size() ||
          index >= batch.pixels.colorIndex.size()) {
        return;
      }
      out.x0 = batch.pixels.x[index];
      out.y0 = batch.pixels.y[index];
      out.x1 = out.x0 + 1;
      out.y1 = out.y0 + 1;
    } break;

    case CommandType::SetPixelA: {
      if (index >= batch.pixelsA.x.size() ||
          index >= batch.pixelsA.y.size() ||
          index >= batch.pixelsA.colorIndex.size() ||
          index >= batch.pixelsA.alpha.size()) {
        return;
      }
      out.x0 = batch.pixelsA.x[index];
      out.y0 = batch.pixelsA.y[index];
      out.x1 = out.x0 + 1;
      out.y1 = out.y0 + 1;
    } break;

    case CommandType::Line: {
      if (index >= batch.lines.x0.size() ||
          index >= batch.lines.y0.size() ||
          index >= batch.lines.x1.size() ||
          index >= batch.lines.y1.size() ||
          index >= batch.lines.widthQ8_8.size() ||
          index >= batch.lines.colorIndex.size() ||
          index >= batch.lines.opacity.size()) {
        return;
      }
      float fx0 = static_cast<float>(batch.lines.x0[index]);
      float fy0 = static_cast<float>(batch.lines.y0[index]);
      float fx1 = static_cast<float>(batch.lines.x1[index]);
      float fy1 = static_cast<float>(batch.lines.y1[index]);
      float widthPx = static_cast<float>(batch.lines.widthQ8_8[index]) / 256.0f;
      float radius = widthPx * 0.5f;
      float pad = radius + 1.0f;
      out.x0 = static_cast<int32_t>(std::floor(std::min(fx0, fx1) - pad));
      out.y0 = static_cast<int32_t>(std::floor(std::min(fy0, fy1) - pad));
      out.x1 = static_cast<int32_t>(std::ceil(std::max(fx0, fx1) + pad));
      out.y1 = static_cast<int32_t>(std::ceil(std::max(fy0, fy1) + pad));
    } break;

    case CommandType::Image: {
      if (index >= batch.imageDraws.x0.size() ||
          index >= batch.imageDraws.y0.size() ||
          index >= batch.imageDraws.x1.size() ||
          index >= batch.imageDraws.y1.size() ||
          index >= batch.imageDraws.srcX0.size() ||
          index >= batch.imageDraws.srcY0.size() ||
          index >= batch.imageDraws.srcX1.size() ||
          index >= batch.imageDraws.srcY1.size() ||
          index >= batch.imageDraws.imageIndex.size() ||
          index >= batch.imageDraws.tintColorIndex.size() ||
          index >= batch.imageDraws.opacity.size()) {
        return;
      }
      out.x0 = batch.imageDraws.x0[index];
      out.y0 = batch.imageDraws.y0[index];
      out.x1 = batch.imageDraws.x1[index];
      out.y1 = batch.imageDraws.y1[index];
      uint8_t flags = index < batch.imageDraws.flags.size() ? batch.imageDraws.flags[index] : 0u;
      if ((flags & ImageFlagClip) != 0u &&
          index < batch.imageDraws.clipX0.size() &&
          index < batch.imageDraws.clipY0.size() &&
          index < batch.imageDraws.clipX1.size() &&
          index < batch.imageDraws.clipY1.size()) {
        out.clipEnabled = true;
        out.clip.x0 = batch.imageDraws.clipX0[index];
        out.clip.y0 = batch.imageDraws.clipY0[index];
        out.clip.x1 = batch.imageDraws.clipX1[index];
        out.clip.y1 = batch.imageDraws.clipY1[index];
        out.x0 = std::max<int32_t>(out.x0, out.clip.x0);
        out.y0 = std::max<int32_t>(out.y0, out.clip.y0);
        out.x1 = std::min<int32_t>(out.x1, out.clip.x1);
        out.y1 = std::min<int32_t>(out.y1, out.clip.y1);
      }
    } break;

    case CommandType::Clear:
    case CommandType::DebugTiles:
    case CommandType::ClearPattern:
    default:
      return;
  }

  finalizePrimitiveBounds(out, targetWidth, targetHeight);
}

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

    PrimitiveBounds bounds{};
    computePrimitiveBounds(batch,
                           cmd.type,
                           cmd.index,
                           config.targetWidth,
                           config.targetHeight,
                           bounds);
    if (!bounds.valid) {
      out[order] = analyzed;
      continue;
    }

    uint8_t colorAlpha = 255u;
    bool include = false;
    switch (cmd.type) {
      case CommandType::Rect: {
        uint8_t flags = cmd.index < batch.rects.flags.size() ? batch.rects.flags[cmd.index] : 0u;
        uint8_t opacity = cmd.index < batch.rects.opacity.size() ? batch.rects.opacity[cmd.index] : 255u;
        if (opacity == 0u) {
          out[order] = analyzed;
          continue;
        }

        bool hasGradient = (flags & RectFlagGradient) != 0u;
        if (hasGradient && cmd.index >= batch.rects.gradientColor1Index.size()) {
          out[order] = analyzed;
          continue;
        }

        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.rects.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (opacity != 255u && combinedAlphaIsZero(colorAlpha, opacity)) {
            out[order] = analyzed;
            continue;
          }
          if (!hasGradient) {
            if (colorAlpha == 0u) {
              out[order] = analyzed;
              continue;
            }
          } else {
            uint32_t gradientColor = fetchColor(batch, batch.rects.gradientColor1Index, cmd.index, 0u);
            uint8_t gradientAlpha = static_cast<uint8_t>((gradientColor >> 24) & 0xFFu);
            if (colorAlpha == 0u && gradientAlpha == 0u) {
              out[order] = analyzed;
              continue;
            }
          }
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, opacity);
        include = true;
      } break;

      case CommandType::Circle: {
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.circles.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (colorAlpha == 0u) {
            out[order] = analyzed;
            continue;
          }
        }
        analyzed.baseAlpha = colorAlpha;
        include = true;
      } break;

      case CommandType::Text: {
        uint8_t opacity = batch.text.opacity[cmd.index];
        if (opacity == 0u) {
          out[order] = analyzed;
          continue;
        }
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.text.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (opacity != 255u && combinedAlphaIsZero(colorAlpha, opacity)) {
            out[order] = analyzed;
            continue;
          }
          if (opacity == 255u && colorAlpha == 0u) {
            out[order] = analyzed;
            continue;
          }
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, opacity);
        include = true;
      } break;

      case CommandType::SetPixel: {
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.pixels.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
        }
        analyzed.baseAlpha = colorAlpha;
        include = true;
      } break;

      case CommandType::SetPixelA: {
        uint8_t alpha = batch.pixelsA.alpha[cmd.index];
        if (alpha == 0u) {
          out[order] = analyzed;
          continue;
        }
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.pixelsA.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (alpha != 255u) {
            if (combinedAlphaIsZero(colorAlpha, alpha)) {
              out[order] = analyzed;
              continue;
            }
          } else if (colorAlpha == 0u) {
            out[order] = analyzed;
            continue;
          }
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, alpha);
        include = true;
      } break;

      case CommandType::Line: {
        uint16_t widthQ = batch.lines.widthQ8_8[cmd.index];
        uint8_t opacity = batch.lines.opacity[cmd.index];
        if (widthQ == 0u || opacity == 0u) {
          out[order] = analyzed;
          continue;
        }
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.lines.colorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (opacity != 255u) {
            if (combinedAlphaIsZero(colorAlpha, opacity)) {
              out[order] = analyzed;
              continue;
            }
          } else if (colorAlpha == 0u) {
            out[order] = analyzed;
            continue;
          }
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, opacity);
        include = true;
      } break;

      case CommandType::Image: {
        uint8_t opacity = batch.imageDraws.opacity[cmd.index];
        if (opacity == 0u) {
          out[order] = analyzed;
          continue;
        }
        if (!config.paletteOpaque) {
          uint32_t color = fetchColor(batch, batch.imageDraws.tintColorIndex, cmd.index, 0u);
          colorAlpha = static_cast<uint8_t>((color >> 24) & 0xFFu);
          if (opacity != 255u) {
            if (combinedAlphaIsZero(colorAlpha, opacity)) {
              out[order] = analyzed;
              continue;
            }
          } else if (colorAlpha == 0u) {
            out[order] = analyzed;
            continue;
          }
        }
        analyzed.baseAlpha = applyOpacity(colorAlpha, opacity);
        include = true;
      } break;

      case CommandType::Clear:
      case CommandType::ClearPattern:
      case CommandType::DebugTiles:
      default:
        break;
    }
    if (!include) {
      out[order] = analyzed;
      continue;
    }

    analyzed.x0 = bounds.x0;
    analyzed.y0 = bounds.y0;
    analyzed.x1 = bounds.x1;
    analyzed.y1 = bounds.y1;
    analyzed.clipEnabled = bounds.clipEnabled;
    analyzed.clip = bounds.clip;
    finalizeAnalyzedCommand(analyzed, config);
    out[order] = analyzed;
  }
}

} // namespace PrimeManifest
