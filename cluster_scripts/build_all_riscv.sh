#!/bin/bash
# build_all_riscv.sh — Скрипт сборки трех конфигураций под архитектуру RISC-V (RVV 1.0)
set -e

# Очищаем старые модули и загружаем современный кросс-компилятор под RISC-V (GCC 15)
module purge
module load gcc-riscv64-15.1.0

MY_PROJECT_DIR="$HOME/Projects/BorisParticlePusher"
mkdir -p "$MY_PROJECT_DIR/results"

# Функция сборки проекта
build_project() {
    local dir=$1
    local build_type=$2
    local extra_flags=$3
    
    cd "$dir"

    echo "=== Сборка конфигурации RISC-V: $build_type ==="
    echo "Флаги: $extra_flags"
    
    rm -rf "build_$build_type"
    mkdir "build_$build_type"
    cd "build_$build_type"
    
    # Конфигурируем проект, ЯВНО указывая параметры кросс-компиляции под RISC-V!
    cmake -DCMAKE_SYSTEM_NAME=Linux \
          -DCMAKE_SYSTEM_PROCESSOR=riscv64 \
          -DCMAKE_C_COMPILER=riscv64-unknown-linux-gnu-gcc \
          -DCMAKE_CXX_COMPILER=riscv64-unknown-linux-gnu-g++ \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_FLAGS_RELEASE="-O3 -g -DNDEBUG $extra_flags" \
          "$dir"
    make -j$(nproc)
    
    # Копируем готовый бинарник в общую папку результатов
    cp ../bin/PerfTest ../results/PerfTest_riscv_$build_type
    cd ..
}

# Собираем 3 конфигурации под RISC-V
build_project "$MY_PROJECT_DIR" "rvv_scalar" "-march=rv64gc"
build_project "$MY_PROJECT_DIR" "rvv_vector" "-march=rv64gcv"
build_project "$MY_PROJECT_DIR" "rvv_vector_fastmath" "-march=rv64gcv -ffast-math"

echo "=== Все конфигурации под RISC-V успешно собраны на логин-ноде! ==="