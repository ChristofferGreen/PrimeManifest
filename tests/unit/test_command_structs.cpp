#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "test_harness.hpp"

using namespace PrimeManifest;
using namespace PrimeManifestTest;

PM_TEST(command_structs, render_command_defaults) {
  RenderCommand cmd;
  PM_CHECK(cmd.type == CommandType::Rect, "default type is rect");
  PM_CHECK(cmd.index == 0, "default index zero");
}

PM_TEST(command_structs, tile_command_defaults) {
  TileCommand cmd;
  PM_CHECK(cmd.type == CommandType::Rect, "default tile command type");
  PM_CHECK(cmd.index == 0, "default tile command index");
  PM_CHECK(cmd.order == 0, "default tile command order");
  PM_CHECK(cmd.wMinus1 == 0, "default tile command width");
}
