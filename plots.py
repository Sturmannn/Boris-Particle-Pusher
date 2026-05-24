import matplotlib.pyplot as plt
import numpy as np

# --- 1. Ввод данных (время выполнения в секундах) ---
# Данные из первого запуска (-O2 -fno-tree-vectorize) - "Скалярная" версия
scalar_data = {
    'aos': {
        1: 2.09,
        2: 0.941,
        4: 0.489,
        8: 0.289
    },
    'soa': {
        1: 0.905,
        2: 0.474,
        4: 0.260,
        8: 0.197
    }
}

# Данные из второго запуска (-O3 -ffast-math -march=native) - "Оптимизированная" версия
optimized_data = {
    'aos': {
        1: 1.41,
        2: 0.721,
        4: 0.387,
        8: 0.275
    },
    'soa': {
        1: 0.280,
        2: 0.187,
        4: 0.182,
        8: 0.190
    }
}

# Настройки для графиков
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams['figure.figsize'] = (10, 6)
plt.rcParams['axes.titlesize'] = 16
plt.rcParams['axes.labelsize'] = 12
plt.rcParams['xtick.labelsize'] = 11
plt.rcParams['ytick.labelsize'] = 11
plt.rcParams['legend.fontsize'] = 12


# --- 2. График №1: Влияние флагов компиляции на AoS ---
# Сравниваем однопоточную версию AoS с разными флагами

fig, ax = plt.subplots()
labels = ['Скалярная (-O2)', 'Оптимизированная (-O3, native)']
times = [
    scalar_data['aos'][1],      # Время AoS на 1 потоке со скалярными флагами
    optimized_data['aos'][1]    # Время AoS на 1 потоке с оптимизацией
]
colors = ['skyblue', 'royalblue']

bars = ax.bar(labels, times, color=colors)
ax.bar_label(bars, fmt='%.2f s')

ax.set_ylabel('Время выполнения, секунды (меньше - лучше)')
ax.set_title('Влияние флагов компиляции на AoS (1 поток)')
# Убираем метки на оси X, т.к. они уже есть в названиях столбцов
ax.set_xticks([])

fig.tight_layout()
plt.savefig("compiler_impact_on_AoS.png", dpi=300)
plt.show()


# --- 3. График №2: Сравнение масштабируемости AoS и SoA ---
# Линейный график времени выполнения для оптимизированных версий

fig, ax = plt.subplots()
threads = list(optimized_data['aos'].keys())

# Данные времени выполнения для оптимизированных версий
aos_times = list(optimized_data['aos'].values())
soa_times = list(optimized_data['soa'].values())

ax.plot(threads, aos_times, 'o-', label='AoS (массив структур)')
ax.plot(threads, soa_times, 's-', label='SoA (структура массивов)')

# Использование логарифмической шкалы по оси Y
# Это лучший способ визуализировать данные, где одно значение
# на порядок больше другого, и видеть динамику обоих.
ax.set_yscale('log')

# Настройка осей и заголовка
ax.set_xlabel('Число потоков OpenMP')
ax.set_ylabel('Время выполнения, секунды (шкала логарифмическая)')
ax.set_title('Сравнение масштабируемости AoS и SoA (с оптимизацией)')
ax.legend()
ax.grid(True, which='both', linestyle='--')

# Устанавливаем метки на оси X, чтобы были все значения (1, 2, 4, 8)
ax.set_xticks(threads)
ax.get_yaxis().set_major_formatter(plt.ScalarFormatter()) # Для красивых меток на оси Y

fig.tight_layout()
plt.savefig("AoS_vs_SoA_scaling.png", dpi=300)
plt.show()