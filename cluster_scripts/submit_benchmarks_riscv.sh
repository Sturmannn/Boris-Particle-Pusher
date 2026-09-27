#!/bin/bash
#SBATCH --job-name=rvv_perf
#SBATCH --output=perf_results_riscv.txt
#SBATCH --error=perf_errors_riscv.txt
#SBATCH --time=00:55:00
#SBATCH --cpus-per-task=8
#SBATCH --partition=k1
#SBATCH --exclusive

MY_PROJECT_DIR="$HOME/Projects/BorisParticlePusher"

# Включаем вывод отладочной информации о привязке потоков
export OMP_DISPLAY_AFFINITY=TRUE

# Задаем привязку потоков OpenMP к физическим ядрам платы
export OMP_PLACES=cores
export OMP_PROC_BIND=close

# Список конфигураций для запуска
configs=("rvv_scalar" "rvv_vector" "rvv_vector_fastmath")

for config_name in "${configs[@]}"; do
    echo "========================================================================="
    echo "ЗАПУСК НА BANANA PI: Конфигурация [$config_name]"
    echo "========================================================================="
    
    # Запускаем тест скорости напрямую (на плате numactl не нужен, так как там всего 1 сокет)
    "$MY_PROJECT_DIR/results/PerfTest_riscv_$config_name" \
    --benchmark_format=json \
    --benchmark_out="$MY_PROJECT_DIR/results/riscv_results_$config_name.json"
    
    echo "" # Пустая строка
done

echo "=== Пайплайн замеров на RISC-V успешно завершен! ==="