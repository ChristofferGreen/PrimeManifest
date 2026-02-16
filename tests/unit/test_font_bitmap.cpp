#include "PrimeManifest/text/FontBitmap.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(font_bitmap, convert_gray8) {
  uint8_t data[] = {0, 128, 255, 10, 20, 30};
  FontBitmapView view;
  view.buffer = data;
  view.width = 3;
  view.height = 2;
  view.pitch = 3;
  view.format = FontBitmapFormat::Gray8;

  std::vector<uint8_t> out;
  int32_t stride = 0;
  PM_CHECK(ConvertFontBitmapToAlpha(view, out, stride), "gray8 converts");
  PM_CHECK(stride == 3, "gray8 stride");
  PM_CHECK(out.size() == 6, "gray8 output size");
  PM_CHECK(out[0] == 0 && out[1] == 128 && out[2] == 255, "gray8 first row");
  PM_CHECK(out[3] == 10 && out[4] == 20 && out[5] == 30, "gray8 second row");
}

PM_TEST(font_bitmap, convert_mono1) {
  uint8_t data[] = {0b10100101, 0b10000000};
  FontBitmapView view;
  view.buffer = data;
  view.width = 9;
  view.height = 1;
  view.pitch = 2;
  view.format = FontBitmapFormat::Mono1;

  std::vector<uint8_t> out;
  int32_t stride = 0;
  PM_CHECK(ConvertFontBitmapToAlpha(view, out, stride), "mono converts");
  PM_CHECK(stride == 9, "mono stride");
  PM_CHECK(out.size() == 9, "mono output size");
  PM_CHECK(out[0] == 255, "mono bit 0");
  PM_CHECK(out[1] == 0, "mono bit 1");
  PM_CHECK(out[2] == 255, "mono bit 2");
  PM_CHECK(out[3] == 0, "mono bit 3");
  PM_CHECK(out[4] == 0, "mono bit 4");
  PM_CHECK(out[5] == 255, "mono bit 5");
  PM_CHECK(out[6] == 0, "mono bit 6");
  PM_CHECK(out[7] == 255, "mono bit 7");
  PM_CHECK(out[8] == 255, "mono bit 8");
}

PM_TEST(font_bitmap, convert_bgra32_alpha) {
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
  PM_CHECK(ConvertFontBitmapToAlpha(view, out, stride), "bgra converts");
  PM_CHECK(stride == 2, "bgra stride");
  PM_CHECK(out.size() == 2, "bgra output size");
  PM_CHECK(out[0] == 30, "bgra uses rgb when alpha 0");
  PM_CHECK(out[1] == 200, "bgra alpha 200");
}

PM_TEST(font_bitmap, convert_gray8_negative_pitch) {
  uint8_t data[] = {1, 2, 3, 4};
  FontBitmapView view;
  view.buffer = data;
  view.width = 2;
  view.height = 2;
  view.pitch = -2;
  view.format = FontBitmapFormat::Gray8;

  std::vector<uint8_t> out;
  int32_t stride = 0;
  PM_CHECK(ConvertFontBitmapToAlpha(view, out, stride), "gray8 converts with negative pitch");
  PM_CHECK(stride == 2, "gray8 negative pitch stride");
  PM_CHECK(out.size() == 4, "gray8 negative pitch output size");
  PM_CHECK(out[0] == 3 && out[1] == 4, "gray8 negative pitch first row");
  PM_CHECK(out[2] == 1 && out[3] == 2, "gray8 negative pitch second row");
}

PM_TEST(font_bitmap, convert_invalid_format_fails) {
  uint8_t data[] = {1, 2};
  FontBitmapView view;
  view.buffer = data;
  view.width = 1;
  view.height = 2;
  view.pitch = 1;
  view.format = static_cast<FontBitmapFormat>(255);

  std::vector<uint8_t> out;
  int32_t stride = 0;
  PM_CHECK(!ConvertFontBitmapToAlpha(view, out, stride), "invalid format fails");
}
