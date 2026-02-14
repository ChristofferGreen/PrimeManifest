#pragma once

#include <string>
#include <utility>

namespace PrimeManifest {

inline constexpr int UiFontWidth = 5;
inline constexpr int UiFontHeight = 7;
inline constexpr int UiFontAdvance = 6;

bool UiFontPixel(char c, int x, int y);
std::pair<int, int> MeasureUiText(std::string const& text, float sizePixels);

} // namespace PrimeManifest
