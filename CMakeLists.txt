cmake_minimum_required(VERSION 3.20)
project(PrimeManifest VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_DEBUG_POSTFIX "-debug")
set(CMAKE_RELEASE_POSTFIX "-release")
set(CMAKE_RELWITHDEBINFO_POSTFIX "-relwithdebinfo")
set(CMAKE_MINSIZEREL_POSTFIX "-minsizerel")

function(pm_require_cxx23 target)
  if(CMAKE_CXX_COMPILE_FEATURES)
    target_compile_features(${target} PUBLIC cxx_std_23)
  else()
    message(WARNING "CMake lacks feature metadata for ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}; falling back to CXX_STANDARD on target ${target}.")
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 23)
    set_property(TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET ${target} PROPERTY CXX_EXTENSIONS OFF)
  endif()
endfunction()

option(PRIMEMANIFEST_ENABLE_FONTS "Enable FreeType+HarfBuzz text shaping" OFF)

set(PRIMEMANIFEST_SOURCES
  src/renderer/Optimizer2D.cpp
  src/renderer/Renderer2D.cpp
  src/text/FontBitmap.cpp
  src/text/FontRegistry.cpp
  src/text/TextBake.cpp
  src/util/BitmapFont.cpp
)

add_library(PrimeManifest ${PRIMEMANIFEST_SOURCES})

target_include_directories(PrimeManifest
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(PRIMEMANIFEST_ENABLE_FONTS)
  find_package(PkgConfig)
  if(PkgConfig_FOUND)
    pkg_check_modules(FREETYPE IMPORTED_TARGET freetype2)
    pkg_check_modules(HARFBUZZ IMPORTED_TARGET harfbuzz)
  endif()
  if(FREETYPE_FOUND AND HARFBUZZ_FOUND)
    target_link_libraries(PrimeManifest PRIVATE PkgConfig::FREETYPE PkgConfig::HARFBUZZ)
    target_compile_definitions(PrimeManifest PUBLIC PRIMEMANIFEST_ENABLE_FONTS=1)
  else()
    message(WARNING "FreeType/HarfBuzz not found; font rendering disabled (fallback to bitmap font).")
    target_compile_definitions(PrimeManifest PUBLIC PRIMEMANIFEST_ENABLE_FONTS=0)
  endif()
else()
  target_compile_definitions(PrimeManifest PUBLIC PRIMEMANIFEST_ENABLE_FONTS=0)
endif()

pm_require_cxx23(PrimeManifest)

option(PRIMEMANIFEST_BUILD_TESTS "Build PrimeManifest tests" ON)
if(PRIMEMANIFEST_BUILD_TESTS)
  enable_testing()

  add_executable(PrimeManifest_tests
    tests/unit/test_batch.cpp
    tests/unit/test_bitmap_font.cpp
    tests/unit/test_clear.cpp
    tests/unit/test_color_helpers.cpp
    tests/unit/test_command_structs.cpp
    tests/unit/test_debug_tiles.cpp
    tests/unit/test_font_registry.cpp
    tests/unit/test_font_bitmap.cpp
    tests/unit/test_front_to_back.cpp
    tests/unit/test_gradient_cache.cpp
    tests/unit/test_layout.cpp
    tests/unit/test_main.cpp
    tests/unit/test_misc.cpp
    tests/unit/test_optimizer.cpp
    tests/unit/test_optimizer_filters.cpp
    tests/unit/test_optimizer_text_cache.cpp
    tests/unit/test_optimizer_rect_cache.cpp
    tests/unit/test_ordering.cpp
    tests/unit/test_palette.cpp
    tests/unit/test_palette_store.cpp
    tests/unit/test_profiles.cpp
    tests/unit/test_rect.cpp
    tests/unit/test_rect_cache_edges.cpp
    tests/unit/test_renderer.cpp
    tests/unit/test_text.cpp
    tests/unit/test_text_bake.cpp
    tests/unit/test_text_bake_edge.cpp
    tests/unit/test_text_store_defaults.cpp
    tests/unit/test_text_clip_cache.cpp
    tests/unit/test_tiles.cpp
    tests/unit/test_store_defaults.cpp
    tests/unit/test_store_more.cpp
    tests/unit/test_store_sizes.cpp
    tests/unit/test_target_guard.cpp
    tests/unit/test_tile_stream.cpp
    tests/unit/test_struct_clears.cpp
  )
  target_link_libraries(PrimeManifest_tests PRIVATE PrimeManifest)
  pm_require_cxx23(PrimeManifest_tests)
  add_test(NAME PrimeManifest_tests COMMAND $<TARGET_FILE:PrimeManifest_tests>)
endif()

option(PRIMEMANIFEST_BUILD_EXAMPLES "Build PrimeManifest example apps" ON)
if(PRIMEMANIFEST_BUILD_EXAMPLES)
  add_executable(renderer_bench examples/renderer_bench.cpp)
  target_link_libraries(renderer_bench PRIVATE PrimeManifest)
  pm_require_cxx23(renderer_bench)
  add_executable(text_render_demo examples/text_render_demo.cpp)
  target_link_libraries(text_render_demo PRIVATE PrimeManifest)
  pm_require_cxx23(text_render_demo)
endif()
