#pragma once

#include "PrimeManifest/renderer/Renderer2D.hpp"
#include "PrimeManifest/text/FontRegistry.hpp"
#include "PrimeManifest/text/TextLayout.hpp"
#include "PrimeManifest/text/Typography.hpp"

#include <optional>

namespace PrimeManifest {

struct TextBakeResult {
  uint32_t textIndex = 0;
  uint32_t runIndex = 0;
};

auto AppendTextRun(RenderBatch& batch,
                   TextRun const& run,
                   int32_t x,
                   int32_t y,
                   uint8_t colorIndex,
                   uint8_t opacity = 255,
                   uint8_t flags = 0) -> std::optional<TextBakeResult>;

auto AppendText(RenderBatch& batch,
                std::string_view text,
                Typography const& typography,
                float deviceScale,
                int32_t x,
                int32_t y,
                uint8_t colorIndex,
                uint8_t opacity = 255,
                uint8_t flags = 0) -> std::optional<TextBakeResult>;

} // namespace PrimeManifest
