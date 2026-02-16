#include "PrimeManifest/text/FontBitmap.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.font_bitmap");

TEST_CASE("convert_gray8") {
  uint8_t data[] = {0, 128, 255, 10, 20, 30};
  FontBitmapView view;
  view.buffer = data;
  view.width = 3;
  view.height = 2;
  view.pitch = 3;
  view.format = FontBitmapFormat::Gray8;

  std::vector<uint8_t> out;
  int32_t stride = 0;
  CHECK_MESSAGE(ConvertFontBitmapToAlpha(view, out, stride), "gray8 converts");
  CHECK_MESSAGE(stride == 3, "gray8 stride");
  CHECK_MESSAGE(out.size() == 6, "gray8 output size");
  CHECK_MESSAGE(out[0] == 0, "gray8 first row byte 0");
  CHECK_MESSAGE(out[1] == 128, "gray8 first row byte 1");
  CHECK_MESSAGE(out[2] == 255, "gray8 first row byte 2");
  CHECK_MESSAGE(out[3] == 10, "gray8 second row byte 0");
  CHECK_MESSAGE(out[4] == 20, "gray8 second row byte 1");
  CHECK_MESSAGE(out[5] == 30, "gray8 second row byte 2");
}

TEST_CASE("convert_mono1") {
  uint8_t data[] = {0b10100101, 0b10000000};
  FontBitmapView view;
  view.buffer = data;
  view.width = 9;
  view.height = 1;
  view.pitch = 2;
  view.format = FontBitmapFormat::Mono1;

  std::vector<uint8_t> out;
  int32_t stride = 0;
  CHECK_MESSAGE(ConvertFontBitmapToAlpha(view, out, stride), "mono converts");
  CHECK_MESSAGE(stride == 9, "mono stride");
  CHECK_MESSAGE(out.size() == 9, "mono output size");
  CHECK_MESSAGE(out[0] == 255, "mono bit 0");
  CHECK_MESSAGE(out[1] == 0, "mono bit 1");
  CHECK_MESSAGE(out[2] == 255, "mono bit 2");
  CHECK_MESSAGE(out[3] == 0, "mono bit 3");
  CHECK_MESSAGE(out[4] == 0, "mono bit 4");
  CHECK_MESSAGE(out[5] == 255, "mono bit 5");
  CHECK_MESSAGE(out[6] == 0, "mono bit 6");
  CHECK_MESSAGE(out[7] == 255, "mono bit 7");
  CHECK_MESSAGE(out[8] == 255, "mono bit 8");
}

TEST_CASE("convert_bgra32_alpha") {
  uint8_t data[] = {
      10, 20, 30, 0,
      40, 50, 60, 200,
  };
  FontBitmapView view;
  view.buffer = data;
  view.width = 2;
  view.height = 1;
  view.pitch = 8;
  view.format = FontBitmapFormat::BGRA32;

  std::vector<uint8_t> out;
  int32_t stride = 0;
  CHECK_MESSAGE(ConvertFontBitmapToAlpha(view, out, stride), "bgra converts");
  CHECK_MESSAGE(stride == 2, "bgra stride");
  CHECK_MESSAGE(out.size() == 2, "bgra output size");
  CHECK_MESSAGE(out[0] == 30, "bgra uses rgb when alpha 0");
  CHECK_MESSAGE(out[1] == 200, "bgra alpha 200");
}

TEST_CASE("convert_gray8_negative_pitch") {
  uint8_t data[] = {1, 2, 3, 4};
  FontBitmapView view;
  view.buffer = data;
  view.width = 2;
  view.height = 2;
  view.pitch = -2;
  view.format = FontBitmapFormat::Gray8;

  std::vector<uint8_t> out;
  int32_t stride = 0;
  CHECK_MESSAGE(ConvertFontBitmapToAlpha(view, out, stride), "gray8 converts with negative pitch");
  CHECK_MESSAGE(stride == 2, "gray8 negative pitch stride");
  CHECK_MESSAGE(out.size() == 4, "gray8 negative pitch output size");
  CHECK_MESSAGE(out[0] == 3, "gray8 negative pitch first row byte 0");
  CHECK_MESSAGE(out[1] == 4, "gray8 negative pitch first row byte 1");
  CHECK_MESSAGE(out[2] == 1, "gray8 negative pitch second row byte 0");
  CHECK_MESSAGE(out[3] == 2, "gray8 negative pitch second row byte 1");
}

TEST_CASE("convert_invalid_format_fails") {
  uint8_t data[] = {1, 2};
  FontBitmapView view;
  view.buffer = data;
  view.width = 1;
  view.height = 2;
  view.pitch = 1;
  view.format = static_cast<FontBitmapFormat>(255);

  std::vector<uint8_t> out;
  int32_t stride = 0;
  CHECK_MESSAGE(!ConvertFontBitmapToAlpha(view, out, stride), "invalid format fails");
}

TEST_SUITE_END();
