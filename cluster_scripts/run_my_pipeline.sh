#!/bin/bash
set -e

MY_PROJECT_DIR="$HOME/Projects/BorisParticlePusher"

# Создаем папку для сохранения результатов
mkdir -p "$MY_PROJECT_DIR/results"

# --- ВЫВОД КАРТЫ ПРИВЯЗКИ ПОТОКОВ В ЛОГ-ФАЙЛ ---
# Эта переменная прикажет OpenMP напечатать в консоль (в лог Slurm) 
# точную информацию о том, какой поток сел на какое ядро.
export OMP_DISPLAY_AFFINITY=TRUE

# Функция сборки проекта
build_project() {
    local dir=$1
    local build_type=$2
    local extra_flags=$3

    echo "=== Сборка конфигурации: $build_type ==="
    echo "Флаги: $extra_flags"
    
    # Очищаем и создаем изолированную папку сборки для этой конфигурации
    rm -rf "build_$build_type"
    mkdir "build_$build_type"
    cd "build_$build_type"
    
    # Конфигурируем проект с генератором Ninja
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG $extra_flags" \
          "$dir"
          
    # Запускаем сборку на всех доступных ядрах
    ninja -j$(nproc)
    
    # РЕШЕНИЕ ПРОБЛЕМЫ ПЕРЕЗАПИСИ:
    # Поскольку CMakeLists.txt принудительно копирует бинарник в общую папку ../bin/PerfTest,
    # мы копируем его в папку результатов под уникальным именем, чтобы не затереть на следующем шаге.
    cp ../bin/PerfTest ../results/PerfTest_$build_type
    
    cd ..
}

# 1. Шаг сборки для разных конфигураций оптимизации
declare -A configs
configs=(
    ["O3"]=""
    ["O3_fastmath"]="-ffast-math"
    ["O3_fastmath_native"]="-ffast-math -march=native"
)

# Запускаем цикл сборки для каждой конфигурации
for config_name in "${!configs[@]}"; do
    flags=${configs[$config_name]}

    cd $MY_PROJECT_DIR
    build_project $MY_PROJECT_DIR $config_name "$flags"
done

# -----------------------------------------------------------------------------
# ЭТАП 2. Шаг запуска бенчмарков с перебором режимов привязки потоков
# -----------------------------------------------------------------------------

# Список режимов привязки для тестирования:
# 1. nobind - без привязки (поведение ОС по умолчанию)
# 2. close  - привязка к соседним ядрам (кэш-локальность)
# 3. spread - распределение по сокетам (максимум пропускной способности памяти)
bind_modes=("nobind" "close" "spread")

# Проверяем, установлена ли утилита numactl на машине
if command -v numactl &> /dev/null; then
    NUMA_CMD="numactl --interleave=all"
    echo "=== Оптимизация NUMA через numactl активна ==="
else
    NUMA_CMD=""
    echo "=== Предупреждение: numactl не найден, запуск без NUMA-интерливинга ==="
fi

for config_name in "O3" "O3_fastmath" "O3_fastmath_native"; do
    for bind_mode in "${bind_modes[@]}"; do
        echo "========================================================================="
        echo "ЗАПУСК БЕНЧМАРКА: Конфигурация [$config_name] | Режим привязки [$bind_mode]"
        echo "========================================================================="
        
        # Настраиваем переменные окружения OpenMP в зависимости от выбранного режима
        if [ "$bind_mode" == "nobind" ]; then
            # Выключаем привязку
            export OMP_PROC_BIND=false
            unset OMP_PLACES
        else
            # Включаем привязку к физическим ядрам
            export OMP_PLACES=cores
            export OMP_PROC_BIND=$bind_mode
        fi
        
        # Запуск бенчмарка (с автоматической проверкой numactl)
        $NUMA_CMD "$MY_PROJECT_DIR/results/PerfTest_$config_name" \
        --benchmark_format=json \
        --benchmark_out="$MY_PROJECT_DIR/results/my_results_${config_name}_${bind_mode}.json"
        
        echo "" # Пустая строка для читаемости логов
    done
done

echo "=== Пайплайн успешно завершен! Результаты сохранены в папку results/ ==="