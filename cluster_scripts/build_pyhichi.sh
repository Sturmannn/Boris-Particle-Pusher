#!/bin/bash
# build_pyhichi.sh — Мастер-скрипт сборки трех конфигураций pyHiChi на логин-ноде
set -e

# Подгружаем окружение (компилятор GCC 9.5.0 и пути к Conda-Python 3.10)
source $HOME/env.sh

PYHICHI_DIR="$HOME/Projects/pyHiChi"
GLOBAL_RESULTS_DIR="$PYHICHI_DIR/results"

mkdir -p "$GLOBAL_RESULTS_DIR"

build_config() {
    local config_name=$1
    local flags=$2

    echo "========================================================================="
    echo "СБОРКА pyHiChi: Конфигурация [$config_name]"
    echo "========================================================================="

    # Очищаем старую папку сборки, если она была
    rm -rf "build_ninja"
    rm -rf "bin"

    # Экспортируем флаги компилятора в переменную окружения
    export EXTRA_CXX_FLAGS="$flags"

    # Запускаем наш адаптированный скрипт, передавая путь к Python 3.10 из Conda
    ./build_pyhichi_cluster.sh -openmp -ptests -tests -python $HOME/miniconda/bin/python3

    # Копируем полученный С++ бенчмарк в глобальную папку результатов нашего основного проекта
    cp bin/PerfTest "$GLOBAL_RESULTS_DIR/PerfTest_pyhichi_$config_name"
}

# Запускаем последовательную сборку трех версий pyHiChi
cd "$PYHICHI_DIR"
build_config "O3" ""
build_config "O3_fastmath" "-ffast-math"
build_config "O3_fastmath_native" "-ffast-math -march=native"

echo "=== Все конфигурации pyHiChi успешно собраны на логин-ноде! ==="