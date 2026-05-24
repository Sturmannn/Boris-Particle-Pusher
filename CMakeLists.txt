cmake_minimum_required(VERSION 3.14)

project(BorisParticlePusher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)




include(FetchContent)

# --- Google Benchmark ---
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(googlebenchmark)

# --- Google Test ---
set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)


add_library(BorisPusherLib STATIC
    src/BorisPusher.cpp
)
# target_compile_options(BorisPusherLib PRIVATE -O2 -fno-tree-vectorize)
target_compile_options(BorisPusherLib PRIVATE -O3 -ffast-math -march=native)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(BorisPusherLib PRIVATE -fopt-info-vec-optimized -fopt-info-vec-missed)
endif()

find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    target_link_libraries(BorisPusherLib PUBLIC OpenMP::OpenMP_CXX)
endif()


target_include_directories(BorisPusherLib PUBLIC include)


# --- Sample (Пример) ---
add_executable(Sample samples/main.cpp)
target_link_libraries(Sample PRIVATE BorisPusherLib)

# --- UnitTest (Тесты логики) ---
add_executable(UnitTest unit_tests/main.cpp)
target_link_libraries(UnitTest PRIVATE BorisPusherLib gtest_main)

# --- PerfTest (Тесты скорости) ---
add_executable(PerfTest perf_tests/main.cpp)
target_link_libraries(PerfTest PRIVATE BorisPusherLib benchmark::benchmark)


message(STATUS "Build configured. Output dir: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")