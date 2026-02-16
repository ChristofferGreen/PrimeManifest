cmake_minimum_required(VERSION 3.20)
project(PrimeManifest VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_DEBUG_POSTFIX "-debug")
set(CMAKE_RELEASE_POSTFIX "-release")
set(CMAKE_RELWITHDEBINFO_POSTFIX "-relwithdebinfo")
set(CMAKE_MINSIZEREL_POSTFIX "-minsizerel")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
target_link_libraries(PrimeManifest PRIVATE PkgConfig::FREETYPE PkgConfig::HARFBUZZ)

pm_require_cxx23(PrimeManifest)

option(PRIMEMANIFEST_BUILD_TESTS "Build PrimeManifest tests" ON)
if(PRIMEMANIFEST_BUILD_TESTS)
  enable_testing()

  add_executable(PrimeManifest_tests
    tests/unit/test_batch.cpp
    tests/unit/test_bitmap_font.cpp
    tests/unit/test_clear.cpp
    tests/unit/test_circle.cpp
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
  target_include_directories(PrimeManifest_tests PRIVATE third_party ${CMAKE_SOURCE_DIR})
  target_link_libraries(PrimeManifest_tests PRIVATE PrimeManifest)
  pm_require_cxx23(PrimeManifest_tests)
  set(PrimeManifestTestSuites
    primemanifest.batch
    primemanifest.bitmap_font
    primemanifest.circle
    primemanifest.clear
    primemanifest.color_helpers
    primemanifest.command_structs
    primemanifest.debug_tiles
    primemanifest.font_bitmap
    primemanifest.font_registry
    primemanifest.front_to_back
    primemanifest.gradient_cache
    primemanifest.layout
    primemanifest.misc
    primemanifest.optimizer
    primemanifest.optimizer_filters
    primemanifest.optimizer_rect_cache
    primemanifest.optimizer_text_cache
    primemanifest.ordering
    primemanifest.palette
    primemanifest.palette_store
    primemanifest.profile
    primemanifest.rect
    primemanifest.rect_cache_edges
    primemanifest.renderer
    primemanifest.store_sizes
    primemanifest.stores
    primemanifest.stores_more
    primemanifest.struct_clears
    primemanifest.target_guard
    primemanifest.text
    primemanifest.text_bake
    primemanifest.text_bake_edge
    primemanifest.text_clip_cache
    primemanifest.text_store
    primemanifest.tile_stream
    primemanifest.tiles
  )

  foreach(suite IN LISTS PrimeManifestTestSuites)
    string(REPLACE "." "_" suiteId "${suite}")
    set(testName "PrimeManifest_${suiteId}")
    add_test(NAME "${testName}" COMMAND $<TARGET_FILE:PrimeManifest_tests> "--test-suite=${suite}")
    set_tests_properties("${testName}" PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
  endforeach()
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
