#!/bin/bash
set -e

# Настраиваем окружение на логин-ноде (здесь модули работают отлично!)
source $HOME/env.sh

MY_PROJECT_DIR="$HOME/Projects/BorisParticlePusher"
mkdir -p "$MY_PROJECT_DIR/results"

# Функция сборки проекта
build_project() {
    local dir=$1
    local build_type=$2
    local extra_flags=$3
	
	cd "$dir"

    echo "=== Сборка конфигурации: $build_type ==="
    rm -rf "build_$build_type"
    mkdir "build_$build_type"
    cd "build_$build_type"
    
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_FLAGS_RELEASE="-O3 -g -DNDEBUG $extra_flags" \
          "$dir"
    ninja -j$(nproc)
    
    # Копируем готовый бинарник в папку результатов
    cp ../bin/PerfTest ../results/PerfTest_$build_type
    cd ..
}

# Собираем 3 конфигурации прямо на логин-ноде
build_project "$MY_PROJECT_DIR" "O3" ""
build_project "$MY_PROJECT_DIR" "O3_fastmath" "-ffast-math"
build_project "$MY_PROJECT_DIR" "O3_fastmath_native" "-ffast-math -march=native -mprefer-vector-width=512 -fopt-info-vec-optimized -fopt-info-vec-missed"

echo "=== Все конфигурации успешно собраны на логин-ноде! ==="
# echo "Отправляем замеры в очередь Slurm..."

# Автоматически отправляем задачу в очередь Slurm
#sbatch submit_benchmarks.sh