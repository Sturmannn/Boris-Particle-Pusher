#!/usr/bin/env python3
import os
import json
import glob
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

# Пути к папкам с результатами на вашем WSL
MY_RESULTS_DIR = "./results"
PYHICHI_RESULTS_DIR = "./results_pyhichi"
PLOTS_DIR = "./plots"

os.makedirs(PLOTS_DIR, exist_ok=True)

# Три структуры данных для раздельного сбора результатов
my_data = {}
pyhichi_data = {}
riscv_data = {}

# 1. Функция парсинга x86 результатов (наш код и pyHiChi)
def parse_json_files(prefix, target_dict, results_dir):
    filepaths = glob.glob(os.path.join(results_dir, f"{prefix}_results_*.json"))
    for filepath in filepaths:
        filename = os.path.basename(filepath)
        parts = filename.replace(f"{prefix}_results_", "").replace(".json", "").split("_")
        binding = parts[-1]
        config = "_".join(parts[:-1])

        with open(filepath, "r") as f:
            try:
                js = json.load(f)
            except json.JSONDecodeError:
                print(f"Предупреждение: Не удалось прочитать файл {filepath}")
                continue
            
        for run in js.get("benchmarks", []):
            run_name = run["name"]
            name_parts = run_name.split("/")
            
            fixture = name_parts[0]
            N = int(name_parts[2])
            iters = int(name_parts[3])
            threads = int(name_parts[4])
            real_time = run["real_time"]
            
            total_particles_pushed = N * iters
            real_throughput = (total_particles_pushed / 1e6) / real_time
            
            if config not in target_dict:
                target_dict[config] = {}
            if binding not in target_dict[config]:
                target_dict[config][binding] = {}
            if fixture not in target_dict[config][binding]:
                target_dict[config][binding][fixture] = {}
                
            target_dict[config][binding][fixture][threads] = {
                'throughput': real_throughput,
                'time': real_time
            }

# 2. Функция парсинга RISC-V результатов (Banana Pi)
def parse_riscv_json_files(results_dir):
    filepaths = glob.glob(os.path.join(results_dir, "riscv_results_*.json"))
    for filepath in filepaths:
        filename = os.path.basename(filepath)
        config = filename.replace("riscv_results_", "").replace(".json", "")

        with open(filepath, "r") as f:
            try:
                js = json.load(f)
            except json.JSONDecodeError:
                print(f"Предупреждение: Не удалось прочитать файл {filepath}")
                continue
            
        for run in js.get("benchmarks", []):
            run_name = run["name"]
            name_parts = run_name.split("/")
            
            fixture = name_parts[0]
            N = int(name_parts[2])
            iters = int(name_parts[3])
            threads = int(name_parts[4])
            real_time = run["real_time"]
            
            total_particles_pushed = N * iters
            real_throughput = (total_particles_pushed / 1e6) / real_time
            
            if config not in riscv_data:
                riscv_data[config] = {}
            if fixture not in riscv_data[config]:
                riscv_data[config][fixture] = {}
                
            riscv_data[config][fixture][threads] = {
                'throughput': real_throughput,
                'time': real_time
            }

# Парсим все папки отдельно
parse_json_files("my", my_data, MY_RESULTS_DIR)
parse_json_files("pyhichi", pyhichi_data, PYHICHI_RESULTS_DIR)
parse_riscv_json_files(MY_RESULTS_DIR)

print("\n=== Все результаты успешно импортированы! ===")

# Настройка единого стиля графиков для научных статей
plt.rcParams.update({
    'font.size': 12,
    'axes.labelsize': 14,
    'axes.titlesize': 13,
    'xtick.labelsize': 11,
    'ytick.labelsize': 11,
    'legend.fontsize': 10,
    'figure.titlesize': 15
})

best_config = "O3_fastmath_native"
best_bind = "spread"
target_config = "O3_fastmath_native"

# Имена фикстур в коде двух разных проектов
MY_SOA = "PusherSoA_Fixture"
MY_AOS = "PusherAoS_Fixture"
PY_SOA = "particleArraySoA"
PY_AOS = "particleArrayAoS"

bind_labels = {
    "pure": "Абсолютный дефолт (Без привязки, без numactl)",
    "nobind": "Без привязки (с numactl)",
    "close": "Привязка близко (close, кэш-локальность)",
    "spread": "Распределение по сокетам (spread, ширина памяти)"
}

# =============================================================================
# ГРАФИК 1. Абсолютное время работы на x86: AoS против SoA (Для BorisPusher и pyHiChi)
# =============================================================================
plt.figure(figsize=(10, 6))
ax = plt.gca()

for fixture, label, marker, color in [(MY_SOA, "BorisPusher (SoA)", "o", "crimson"), (MY_AOS, "BorisPusher (AoS)", "s", "lightcoral")]:
    if best_config in my_data and best_bind in my_data[best_config] and fixture in my_data[best_config][best_bind]:
        threads_dict = my_data[best_config][best_bind][fixture]
        threads_list = sorted(threads_dict.keys())
        y_values = [threads_dict[t]['time'] for t in threads_list]
        plt.plot(threads_list, y_values, marker=marker, color=color, linewidth=2, label=label)

for fixture, label, marker, color in [(PY_SOA, "pyHiChi (SoA)", "D", "navy"), (PY_AOS, "pyHiChi (AoS)", "^", "cornflowerblue")]:
    if best_config in pyhichi_data and best_bind in pyhichi_data[best_config] and fixture in pyhichi_data[best_config][best_bind]:
        threads_dict = pyhichi_data[best_config][best_bind][fixture]
        threads_list = sorted(threads_dict.keys())
        y_values = [threads_dict[t]['time'] for t in threads_list]
        plt.plot(threads_list, y_values, marker=marker, color=color, linewidth=2, linestyle="--", label=label)

ax.set_xscale('log', base=2)
ax.set_yscale('log', base=2)
ax.set_xticks(threads_list)
ax.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
ax.get_yaxis().set_major_formatter(ticker.ScalarFormatter())
plt.grid(True, which="both", linestyle="--", alpha=0.5)

plt.xlabel("Количество потоков OpenMP (Log-шкала)")
plt.ylabel("Время работы в секундах (Log-шкала)")
plt.title(f"Сравнение структур данных AoS и SoA на x86\n(Конфигурация: {best_config}, привязка: {best_bind})")
plt.legend(loc="upper right")
plt.savefig("./plots/01_AoS_vs_SoA_Time_LogLog.png", dpi=300, bbox_inches="tight")
print("Ок: Построен график 1.")


# =============================================================================
# ГРАФИК 2. Влияние оптимизаций компилятора на x86 (BorisPusher SoA vs pyHiChi SoA)
# =============================================================================
plt.figure(figsize=(10, 6))
ax = plt.gca()
config_labels = {
    "O3": "Base (-O3)",
    "O3_fastmath": "Fast Math (-O3 -ffast-math)",
    "O3_fastmath_native": "AVX-512 (-O3 -ffast-math -march=native -mprefer-vector-width=512)"
}

for i, config in enumerate(["O3", "O3_fastmath", "O3_fastmath_native"]):
    colors = ["orange", "forestgreen", "crimson"]
    if config in my_data and best_bind in my_data[config] and MY_SOA in my_data[config][best_bind]:
        threads_dict = my_data[config][best_bind][MY_SOA]
        threads_list = sorted(threads_dict.keys())
        y_values = [threads_dict[t]['time'] for t in threads_list]
        plt.plot(threads_list, y_values, marker="o", color=colors[i], linewidth=2, label=f"BorisPusher: {config_labels[config]}")
    if config in pyhichi_data and best_bind in pyhichi_data[config] and PY_SOA in pyhichi_data[config][best_bind]:
        threads_dict = pyhichi_data[config][best_bind][PY_SOA]
        threads_list = sorted(threads_dict.keys())
        y_values = [threads_dict[t]['time'] for t in threads_list]
        plt.plot(threads_list, y_values, marker="x", color=colors[i], linewidth=1.5, linestyle="--", label=f"pyHiChi: {config_labels[config]}")

ax.set_xscale('log', base=2)
ax.set_yscale('log', base=2)
ax.set_xticks(threads_list)
ax.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
ax.get_yaxis().set_major_formatter(ticker.ScalarFormatter())
plt.grid(True, which="both", linestyle="--", alpha=0.5)

plt.xlabel("Количество потоков OpenMP (Log-шкала)")
plt.ylabel("Время работы в секундах (Log-шкала)")
plt.title(f"Влияние оптимизаций компилятора на время работы на x86\n(структура SoA, привязка: {best_bind})")
plt.legend(loc="upper right")
plt.savefig("./plots/02_Compiler_Optimizations_Time_LogLog.png", dpi=300, bbox_inches="tight")
print("Ок: Построен график 2.")


# =============================================================================
# ГРАФИК 3. Влияние привязки потоков на NUMA-узле на x86 (Для BorisPusher SoA)
# =============================================================================
plt.figure(figsize=(10, 6))
ax = plt.gca()

colors = {"pure": "purple", "nobind": "dodgerblue", "close": "darkorange", "spread": "forestgreen"}
markers = {"pure": "v", "nobind": "s", "close": "o", "spread": "^"}

for bind in ["pure", "nobind", "close", "spread"]:
    if best_config in my_data and bind in my_data[best_config] and MY_SOA in my_data[best_config][bind]:
        threads_dict = my_data[best_config][bind][MY_SOA]
        threads_list = sorted(threads_dict.keys())
        y_values = [threads_dict[t]['time'] for t in threads_list]
        plt.plot(threads_list, y_values, marker=markers[bind], color=colors[bind], linewidth=2, label=bind_labels[bind])

ax.set_xscale('log', base=2)
ax.set_yscale('log', base=2)
ax.set_xticks(threads_list)
ax.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
ax.get_yaxis().set_major_formatter(ticker.ScalarFormatter())
plt.grid(True, which="both", linestyle="--", alpha=0.5)

plt.xlabel("Количество потоков OpenMP (Log-шкала)")
plt.ylabel("Время работы в секундах (Log-шкала)")
plt.title(f"Влияние привязки потоков на время работы на x86\n(BorisPusher SoA, конфигурация: {best_config})")
plt.legend(loc="upper right")
plt.savefig("./plots/03_Thread_Affinity_Time_LogLog.png", dpi=300, bbox_inches="tight")
print("Ок: Построен график 3.")


# =============================================================================
# ГРАФИК 4. Влияние оптимизаций и вектора RVV 1.0 на Banana Pi (Log-Log)
# =============================================================================
plt.figure(figsize=(10, 6))
ax = plt.gca()

riscv_configs = {
    "rvv_scalar": ("Скалярная версия (-march=rv64gc)", "s", "orange", "--"),
    "rvv_vector": ("Векторная версия RVV 1.0 (-march=rv64gcv)", "o", "forestgreen", "-"),
    "rvv_vector_fastmath": ("Векторная версия + Fast Math", "^", "crimson", "-")
}

threads_list_rvv = [1, 2, 4, 8] # максимум для Spacemit K1

for config, (label, marker, color, linestyle) in riscv_configs.items():
    if config in riscv_data and MY_SOA in riscv_data[config]:
        threads_dict = riscv_data[config][MY_SOA]
        threads_list_rvv = sorted(threads_dict.keys())
        y_values = [threads_dict[t]['time'] for t in threads_list_rvv]
        plt.plot(threads_list_rvv, y_values, marker=marker, color=color, linewidth=2, linestyle=linestyle, label=label)

ax.set_xscale('log', base=2)
ax.set_yscale('log', base=2)
ax.set_xticks(threads_list_rvv)
ax.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
ax.get_yaxis().set_major_formatter(ticker.ScalarFormatter())
plt.grid(True, which="both", linestyle="--", alpha=0.5)

plt.xlabel("Количество потоков OpenMP (Log-шкала)")
plt.ylabel("Время работы в секундах (Log-шкала)")
plt.title("Влияние оптимизаций и вектора RVV 1.0 на процессоре Spacemit X60\n(BorisPusher SoA, плата Banana Pi BPI-F3)")
plt.legend(loc="upper right")
plt.savefig("./plots/04_RISCV_Optimizations_Time_LogLog.png", dpi=300, bbox_inches="tight")
print("Ок: Построен график 4.")


# =============================================================================
# ГРАФИК 5. СУПЕР-СРАВНЕНИЕ: Все ключевые версии на одном графике (Log-Log)
# Сравнивает x86 (Xeon) и RISC-V (Spacemit X60) на общем диапазоне 1-8 потоков
# =============================================================================
plt.figure(figsize=(10, 6))
ax = plt.gca()

# Настройки общих потоков (максимум для платы)
threads_common = [1, 2, 4, 8]

# 1. Наш x86 - AoS
if "O3_fastmath_native" in my_data and "spread" in my_data["O3_fastmath_native"] and MY_AOS in my_data["O3_fastmath_native"]["spread"]:
    y = [my_data["O3_fastmath_native"]["spread"][MY_AOS][t]['time'] for t in threads_common]
    plt.plot(threads_common, y, marker="s", color="lightcoral", linewidth=1.5, linestyle=":", label="x86_64: BorisPusher (AoS)")

# 2. Наш x86 - SoA (AVX-512)
if "O3_fastmath_native" in my_data and "spread" in my_data["O3_fastmath_native"] and MY_SOA in my_data["O3_fastmath_native"]["spread"]:
    y = [my_data["O3_fastmath_native"]["spread"][MY_SOA][t]['time'] for t in threads_common]
    plt.plot(threads_common, y, marker="o", color="crimson", linewidth=2.5, label="x86_64: BorisPusher (SoA, AVX-512)")

# 3. pyHiChi x86 - SoA (AVX-512)
if "O3_fastmath_native" in pyhichi_data and "spread" in pyhichi_data["O3_fastmath_native"] and PY_SOA in pyhichi_data["O3_fastmath_native"]["spread"]:
    y = [pyhichi_data["O3_fastmath_native"]["spread"][PY_SOA][t]['time'] for t in threads_common]
    plt.plot(threads_common, y, marker="D", color="navy", linewidth=2, linestyle="--", label="x86_64: pyHiChi (SoA)")

# 4. RISC-V - Скаляр (Banana Pi)
if "rvv_scalar" in riscv_data and MY_SOA in riscv_data["rvv_scalar"]:
    y = [riscv_data["rvv_scalar"][MY_SOA][t]['time'] for t in threads_common]
    plt.plot(threads_common, y, marker="s", color="orange", linewidth=1.5, linestyle=":", label="RISC-V: BorisPusher (Скаляр)")

# 5. RISC-V - Вектор (Banana Pi, RVV 1.0)
if "rvv_vector_fastmath" in riscv_data and MY_SOA in riscv_data["rvv_vector_fastmath"]:
    y = [riscv_data["rvv_vector_fastmath"][MY_SOA][t]['time'] for t in threads_common]
    plt.plot(threads_common, y, marker="^", color="forestgreen", linewidth=2.5, label="RISC-V: BorisPusher (Вектор, RVV 1.0)")

# Настройки логарифмических осей
ax.set_xscale('log', base=2)
ax.set_yscale('log', base=2)
ax.set_xticks(threads_common)
ax.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
ax.get_yaxis().set_major_formatter(ticker.ScalarFormatter())
plt.grid(True, which="both", linestyle="--", alpha=0.5)

plt.xlabel("Количество потоков OpenMP (Log-шкала)")
plt.ylabel("Время работы в секундах (Log-шкала)")
plt.title("Глобальное сравнение времени работы алгоритма Бориса\n(Intel Xeon Silver 4310T vs. Spacemit X60, 1-8 потоков)")
plt.legend(loc="upper right")
plt.savefig("./plots/05_Global_Performance_Comparison_LogLog.png", dpi=300, bbox_inches="tight")
print("Ок: Построен глобальный график 5.")


# =============================================================================
# ГРАФИК 6. НОВЫЙ: Эффективность параллелизации (Parallel Efficiency, E(p)) в %
# Сравнивает эффективность x86 (Xeon) и RISC-V (Banana Pi) SoA версий
# =============================================================================
plt.figure(figsize=(10, 6))

# 1. Эффективность на x86 (до 8 потоков)
if "O3_fastmath_native" in my_data and "spread" in my_data["O3_fastmath_native"] and MY_SOA in my_data["O3_fastmath_native"]["spread"]:
    threads_dict = my_data["O3_fastmath_native"]["spread"][MY_SOA]
    threads_list = [1, 2, 4, 8]
    t1_time = threads_dict[1]['time']
    efficiency = [(t1_time / (t * threads_dict[t]['time'])) * 100.0 for t in threads_list]
    plt.plot(threads_list, efficiency, marker="o", color="crimson", linewidth=2.5, label="x86_64: BorisPusher (SoA, AVX-512)")

# 2. Эффективность на RISC-V (до 8 потоков)
if "rvv_vector_fastmath" in riscv_data and MY_SOA in riscv_data["rvv_vector_fastmath"]:
    threads_dict = riscv_data["rvv_vector_fastmath"][MY_SOA]
    threads_list = [1, 2, 4, 8]
    t1_time = threads_dict[1]['time']
    efficiency = [(t1_time / (t * threads_dict[t]['time'])) * 100.0 for t in threads_list]
    plt.plot(threads_list, efficiency, marker="^", color="forestgreen", linewidth=2.5, label="RISC-V: BorisPusher (SoA, RVV 1.0)")

plt.grid(True, linestyle="--", alpha=0.5)
plt.xticks([1, 2, 4, 8])
plt.xlabel("Количество потоков OpenMP")
plt.ylabel("Эффективность параллелизации E(p) (%)")
plt.title("Эффективность масштабирования алгоритма Бориса\n(Intel Xeon Silver 4310T vs. Spacemit X60, 1-8 потоков)")
plt.legend(loc="lower left")
plt.ylim(0, 110)
plt.savefig("./plots/06_Parallel_Efficiency.png", dpi=300, bbox_inches="tight")
print("Ок: Построен график 6 (Эффективность параллелизации).")


# =============================================================================
# ГРАФИК 7. НОВЫЙ: Коэффициент векторного ускорения на RISC-V (Vector Speedup)
# Показывает во сколько раз ручной векторный RVV-код быстрее скаляра на плате
# =============================================================================
plt.figure(figsize=(10, 6))

threads_list_rvv = [1, 2, 4, 8]

if "rvv_scalar" in riscv_data and "rvv_vector_fastmath" in riscv_data:
    scalar_dict = riscv_data["rvv_scalar"][MY_SOA]
    vector_dict = riscv_data["rvv_vector_fastmath"][MY_SOA]
    
    # Считаем S_vec = T_scalar / T_vector для каждого числа потоков
    vector_speedup = [scalar_dict[t]['time'] / vector_dict[t]['time'] for t in threads_list_rvv]
    
    # Рисуем линию теоретического предела VLEN=256 (4x ускорение для double)
    plt.axhline(y=4.0, color="black", linestyle=":", linewidth=1.5, label="Теоретический предел RVV 1.0 VLEN=256 (4x)")
    
    plt.plot(threads_list_rvv, vector_speedup, marker="^", color="crimson", linewidth=2.5, label="Реальное векторное ускорение")

plt.grid(True, linestyle="--", alpha=0.5)
plt.xticks(threads_list_rvv)
plt.xlabel("Количество потоков OpenMP")
plt.ylabel("Кратность ускорения (T_scalar / T_vector)")
plt.title("Коэффициент аппаратного векторного ускорения на Spacemit X60\n(Относительно скалярной версии на аналогичном числе потоков)")
plt.legend(loc="lower right")
plt.ylim(0, 5.5) # Немного выше предела для наглядности
plt.savefig("./plots/07_RISCV_Vector_Speedup.png", dpi=300, bbox_inches="tight")
print("Ок: Построен график 7 (Векторное ускорение RISC-V).")

print("\n=== Все 7 графиков успешно построены и сохранены в папку plots/ ===")