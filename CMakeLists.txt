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

set(PRIMEMANIFEST_SOURCES
  src/renderer/Optimizer2D.cpp
  src/renderer/Renderer2D.cpp
)

add_library(PrimeManifest ${PRIMEMANIFEST_SOURCES})

target_include_directories(PrimeManifest
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

pm_require_cxx23(PrimeManifest)

option(PRIMEMANIFEST_BUILD_TESTS "Build PrimeManifest tests" ON)
if(PRIMEMANIFEST_BUILD_TESTS)
  enable_testing()

  add_executable(PrimeManifest_tests
    tests/unit/test_main.cpp
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
endif()
