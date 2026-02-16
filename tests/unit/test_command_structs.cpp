#include "PrimeManifest/renderer/Renderer2D.hpp"

#include "third_party/doctest.h"

using namespace PrimeManifest;
TEST_SUITE_BEGIN("primemanifest.command_structs");

TEST_CASE("render_command_defaults") {
  RenderCommand cmd;
  CHECK_MESSAGE(cmd.type == CommandType::Rect, "default type is rect");
  CHECK_MESSAGE(cmd.index == 0, "default index zero");
}

TEST_CASE("tile_command_defaults") {
  TileCommand cmd;
  CHECK_MESSAGE(cmd.type == CommandType::Rect, "default tile command type");
  CHECK_MESSAGE(cmd.index == 0, "default tile command index");
  CHECK_MESSAGE(cmd.order == 0, "default tile command order");
  CHECK_MESSAGE(cmd.wMinus1 == 0, "default tile command width");
}

TEST_SUITE_END();
