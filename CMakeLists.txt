cmake_minimum_required(VERSION 3.14)

project(BorisParticlePusher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Папка для готовых исполняемых файлов
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

# Включаем встроенную систему тестирования CMake (для ctest)
enable_testing()

# =============================================================================
# Загрузка внешних зависимостей (FetchContent)
# =============================================================================
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
# Для Windows: форсируем использование динамического CRT (предотвращает конфликты линковки)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) 
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# =============================================================================
# Сборка библиотеки BorisPusherLib
# =============================================================================

# 1. Автоматически собираем все заголовки и исходники из src/
file(GLOB_RECURSE LIB_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h*")
file(GLOB LIB_COMMON_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Исключаем платформозависимые файлы из общего списка сборки, 
# чтобы они не компилировались одновременно
list(REMOVE_ITEM LIB_COMMON_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/BorisPusher_x86.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/BorisPusher_RISCV.cpp"
)

# 2. Определяем целевую архитектуру (по процессору или по имени кросс-компилятора)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64|RISCV64|rv64gcv" OR CMAKE_CXX_COMPILER MATCHES "riscv64")
    message(STATUS "Конфигурация сборки для архитектуры: RISC-V")
    set(PUSHER_IMPL_SRC "src/BorisPusher_RISCV.cpp")
    set(IS_RISCV TRUE)
else()
    message(STATUS "Конфигурация сборки для архитектуры: x86_64")
    set(PUSHER_IMPL_SRC "src/BorisPusher_x86.cpp")
    set(IS_RISCV FALSE)
endif()

# 3. Собираем библиотеку из общих исходников, заголовков и выбранной реализации
add_library(BorisPusherLib STATIC 
    ${LIB_COMMON_SOURCES} 
    ${LIB_HEADERS} 
    ${PUSHER_IMPL_SRC}
)

# Опции компилятора делаем PUBLIC, чтобы они наследовались исполняемыми файлами
target_compile_options(BorisPusherLib PUBLIC 
    -O3 
)

# Если собираем под RISC-V, принудительно пробрасываем флаг поддержки векторного расширения RVV 1.0
# Ключи будут передавать из скрипта build_all_riscv.sh кластера
if (IS_RISCV)
    target_compile_options(BorisPusherLib PUBLIC -march=rv64gcv)
endif()

# Вывод информации о векторизации компилятора (только для GCC)
# if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#     target_compile_options(BorisPusherLib PRIVATE -fopt-info-vec-optimized -fopt-info-vec-missed)
# endif()

# Поиск и линковка OpenMP
find_package(OpenMP REQUIRED)
target_link_libraries(BorisPusherLib PUBLIC OpenMP::OpenMP_CXX)

# Пути к заголовочным файлам библиотеки (тоже PUBLIC)
target_include_directories(BorisPusherLib PUBLIC include)

# =============================================================================
# Сборка исполняемых файлов (Executables)
# =============================================================================

# 1. --- Sample (Пример) ---
file(GLOB_RECURSE SAMPLE_SOURCES CONFIGURE_DEPENDS "samples/*.cpp" "samples/*.h")
add_executable(Sample ${SAMPLE_SOURCES})
target_link_libraries(Sample PRIVATE BorisPusherLib)

# 2. --- UnitTest (Тесты логики) ---
file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS "unit_tests/*.cpp" "unit_tests/*.h")
add_executable(UnitTest ${UNIT_TEST_SOURCES})
# Линкуем библиотеку и gtest. Флаги -O3 унаследуются от BorisPusherLib автоматически!
target_link_libraries(UnitTest PRIVATE BorisPusherLib gtest_main)

# Интегрируем тесты с CTest (позволит запускать тесты через команду `ctest`)
include(GoogleTest)
gtest_discover_tests(UnitTest)

# 3. --- PerfTest (Тесты скорости) ---
file(GLOB_RECURSE PERF_TEST_SOURCES CONFIGURE_DEPENDS "perf_tests/*.cpp" "perf_tests/*.h")
add_executable(PerfTest ${PERF_TEST_SOURCES})
# Сюда тоже прилетят флаги -O3 и -ffast-math, что критически важно для бенчмарков!
target_link_libraries(PerfTest PRIVATE BorisPusherLib benchmark::benchmark)


# =============================================================================
# Диагностика и вывод параметров сборки
# =============================================================================

message(STATUS "==================== BUILD CONFIGURATION ====================")
message(STATUS "Project Name:               ${PROJECT_NAME}")
message(STATUS "CMake Generator:            ${CMAKE_GENERATOR}")

# Режим сборки (Debug, Release, RelWithDebInfo и т.д.)
if(CMAKE_BUILD_TYPE)
    message(STATUS "Build Type:                 ${CMAKE_BUILD_TYPE}")
else()
    # Для мультиконфигурационных генераторов (например, Visual Studio или Xcode)
    # CMAKE_BUILD_TYPE на этапе конфигурации может быть пустым.
    message(STATUS "Build Type:                 Multi-config (defined at build time)")
endif()

message(STATUS "C++ Standard:               ${CMAKE_CXX_STANDARD} (Required: ${CMAKE_CXX_STANDARD_REQUIRED})")
message(STATUS "Compiler ID:                ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler Version:           ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Compiler Path:              ${CMAKE_CXX_COMPILER}")

# 1. Глобальные флаги компилятора (заданные по умолчанию системой или пользователем)
message(STATUS "Global CXX Flags:           ${CMAKE_CXX_FLAGS}")
if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
    message(STATUS "Global CXX Flags (${CMAKE_BUILD_TYPE}): ${CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER}}")
endif()

# 2. Флаги, привязанные непосредственно к BorisPusherLib
# Читаем локальные (PRIVATE) и публичные (INTERFACE/PUBLIC) опции таргета
get_target_property(TARGET_OPTS BorisPusherLib COMPILE_OPTIONS)
if(TARGET_OPTS)
    message(STATUS "BorisPusherLib Options:     ${TARGET_OPTS}")
else()
    message(STATUS "BorisPusherLib Options:     (none)")
endif()

# 3. Информация о найденном OpenMP
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP CXX Version:         ${OpenMP_CXX_VERSION}")
    message(STATUS "OpenMP CXX Flags:           ${OpenMP_CXX_FLAGS}")
endif()

# 4. Директория вывода исполняемых файлов
message(STATUS "Output Directory:           ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "=============================================================")