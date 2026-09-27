#!/bin/bash
#SBATCH --job-name=boris_perf
#SBATCH --output=perf_results.txt
#SBATCH --error=perf_errors.txt
#SBATCH --time=02:00:00
#SBATCH --cpus-per-task=40
#SBATCH --exclusive

# exclusive, чтобы чужие таски не испортили кеш

# Нам не нужно компилировать! Мы просто запускаем готовые файлы.
# Прописываем пути к библиотекам компилятора, чтобы готовые бинарники запустились без ошибок:
#export LD_LIBRARY_PATH=/common/software/gcc-9.5.0/lib64:/common/software/gcc-9.5.0/lib:$LD_LIBRARY_PATH

MY_PROJECT_DIR="$HOME/Projects/BorisParticlePusher"
PYHICHI_DIR_BIN="$HOME/Projects/results_pyhichi"

# Включаем вывод отладочной информации о привязке потоков
export OMP_DISPLAY_AFFINITY=TRUE

# Список режимов привязки для тестирования
bind_modes=("pure" "nobind" "close" "spread")

export SLURM_CPU_BIND=none

# Проверяем, установлена ли утилита numactl на вычислительном узле
if command -v numactl &> /dev/null; then
    NUMA_CMD="numactl --interleave=all"
else
    NUMA_CMD=""
fi

# Цикл запуска готовых бинарников в разных режимах привязки
for config_name in "O3" "O3_fastmath" "O3_fastmath_native"; do
    for bind_mode in "${bind_modes[@]}"; do
		echo "=========================================================================" >&2
		echo "ЗАПУСК БЕНЧМАРКА: Конфигурация [$config_name] | Режим привязки [$bind_mode]" >&2
		echo "=========================================================================" >&2
        
        # 1. Настраиваем привязку OpenMP и NUMA-команду
        if [ "$bind_mode" == "pure" ]; then
            # АБСОЛЮТНЫЙ ДЕФОЛТ: выключаем привязку OpenMP и полностью отключаем numactl!
            export OMP_PROC_BIND=false
            unset OMP_PLACES
            CURRENT_NUMA_CMD=""
        elif [ "$bind_mode" == "nobind" ]; then
            # С принудительной памятью numactl, но без привязки потоков
            export OMP_PROC_BIND=false
            unset OMP_PLACES
            CURRENT_NUMA_CMD=$NUMA_CMD
        else
            # Оптимизированные режимы привязки потоков + numactl
            export OMP_PLACES=cores
            export OMP_PROC_BIND=$bind_mode
            CURRENT_NUMA_CMD=$NUMA_CMD
        fi
        
        # 1. ЗАПУСК ВАШЕГО БЕНЧМАРКА
        echo "=== ЗАПУСК: Наш пушер [$config_name] | Привязка [$bind_mode] ==="
        $CURRENT_NUMA_CMD "$MY_PROJECT_DIR/results/PerfTest_$config_name" \
        --benchmark_format=json \
        --benchmark_out="$MY_PROJECT_DIR/results/my_results_${config_name}_${bind_mode}.json"
        
        # 2. ЗАПУСК БЕНЧМАРКА pyHiChi
        echo "=== ЗАПУСК: pyHiChi [$config_name] | Привязка [$bind_mode] ==="
        $CURRENT_NUMA_CMD "$PYHICHI_DIR_BIN/PerfTest_pyhichi_$config_name" \
        --benchmark_format=json \
        --benchmark_out="$PYHICHI_DIR_BIN/pyhichi_results_${config_name}_${bind_mode}.json"
        
        echo "" # Пустая строка для читаемости логов
    done
done

echo "=== Пайплайн замеров успешно завершен! ==="