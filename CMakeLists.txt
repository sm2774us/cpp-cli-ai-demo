cmake_minimum_required(VERSION 3.24)
project(wordstat CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake's CXX_STANDARD/dialect-detection tables lag behind new
# standards (a CMake 3.28 build error was hit here: "requires the
# language dialect CXX26 ... does not know the flags to enable it"
# even though the compiler itself supports -std=c++26). Passing the
# flag directly sidesteps that table lookup entirely and works with
# any CMake version once the compiler itself understands the flag.
add_compile_options(-std=c++26)

option(WORDSTAT_ENABLE_COVERAGE "Build with gcov/lcov coverage instrumentation" OFF)
option(WORDSTAT_BUILD_TESTS "Build the GoogleTest suite" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

if(WORDSTAT_ENABLE_COVERAGE)
  add_compile_options(--coverage -O0 -g)
  add_link_options(--coverage)
endif()

# ---- Core library: all real, testable logic. No main() symbols. ----
add_library(wordstat_lib
  src/word_analyzer.cc
  src/benchmark.cc
)
target_include_directories(wordstat_lib PUBLIC include)

# ---- CLI executable: main.cc's main() links here only ----
add_executable(wordstat_cli src/main.cc)
target_link_libraries(wordstat_cli PRIVATE wordstat_lib)
set_target_properties(wordstat_cli PROPERTIES OUTPUT_NAME wordstat)

# ---- Benchmark executable: benchmark_main.cc's main() links here only ----
add_executable(benchmark src/benchmark_main.cc)
target_link_libraries(benchmark PRIVATE wordstat_lib)

# ---- Tests: link wordstat_lib (no main() in it) + gtest_main ----
if(WORDSTAT_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(wordstat_tests
    tests/word_analyzer_test.cc
    tests/benchmark_test.cc
  )
  target_link_libraries(wordstat_tests PRIVATE wordstat_lib GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(wordstat_tests)
endif()
