#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <iostream>
#include "BorisPusher.h"

namespace bp {

// Предельно допустимая относительная погрешность для интеграционных физических тестов
constexpr double MAX_RELATIVE_ERROR = 1e-3;

// Предельно допустимая абсолютная погрешность для тестов сохранения энергии (машинный ноль)
constexpr double DOUBLE_MACHINE_PRECISION = 1e-13;

// =============================================================================
// ТЕСТ 1: Релятивистское ускорение в статическом электрическом поле (E-field)
// =============================================================================
// Проверяет точность расчета релятивистской динамики при ускорении вдоль оси X.
// Энергия частицы в этом тесте возрастает. Сравнивается с точной аналитикой.
TEST(BorisPusherTest, RelativisticAcceleration) {
    // --- Начальные условия ---
    // Напряженность поля E0 задается в СГС (статвольт/см).
    // Переводим 10^8 В/м из СИ в СГС (делением на 30 000).
    double E0 = 1e8 / 30000.0;
    size_t N = 10000;  // Количество расчетных шагов

    // Инициализируем электрон в состоянии покоя в начале координат
    Particle electron({0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, Q_E, MASS_E);
    Field field = {{E0, 0.0, 0.0}, {0.0, 0.0, 0.0}};  // Поле только по оси X

    // --- Выбор шага по времени dt ---
    // Цель: разогнать электрон до импульса p = mc (при этом gamma = sqrt(2)).
    // Характерное время разгона t = p / (q*E) = mc / (|q|*E)
    double total_time_analytical = (MASS_E * C_LIGHT) / (std::abs(Q_E) * E0);
    double dt = total_time_analytical / N;

    // --- Симуляционный цикл ---
    for (size_t i = 0; i < N; ++i) {
        BorisPusher(electron, field, dt);
    }

    // --- Аналитический расчет финального состояния ---
    // Координата x(t) = (mc^2 / qE) * (gamma - 1).
    // Поскольку заряд электрона q < 0, смещение будет отрицательным (против вектора E).
    double factor = (MASS_E * C_LIGHT * C_LIGHT) / (Q_E * E0);
    double r_final_x_analytical = factor * (std::sqrt(2.0) - 1.0);

    // Импульс px = q * E * t. Направлен против оси X.
    double p_final_x_analytical = Q_E * E0 * total_time_analytical;

    // --- Сравнение результатов симуляции и аналитики ---
    Vec3 r_final_sim = electron.pos;
    Vec3 p_final_sim = electron.getMomentumPhys();

    double rel_error_pos = std::abs(r_final_sim.x - r_final_x_analytical) / std::abs(r_final_x_analytical);
    double rel_error_mom = std::abs(p_final_sim.x - p_final_x_analytical) / std::abs(p_final_x_analytical);

    // Google Test проверки
    EXPECT_LT(rel_error_pos, MAX_RELATIVE_ERROR) << "Ошибка координаты X превысила лимит. Симуляция: " << r_final_sim.x << ", Аналитика: " << r_final_x_analytical;

    EXPECT_LT(rel_error_mom, MAX_RELATIVE_ERROR) << "Ошибка импульса Px превысила лимит. Симуляция: " << p_final_sim.x << ", Аналитика: " << p_final_x_analytical;
}

// =============================================================================
// ТЕСТ 2: Релятивистское вращение в статическом магнитном поле (B-field)
// =============================================================================
// Проверяет точность расчета спиральной траектории (циклотронного вращения).
// Симуляция запускается ровно на половину периода, импульс должен развернуться на 180 градусов.
TEST(BorisPusherTest, MagneticOscillation) {
    // --- Начальные условия ---
    // Индукция магнитного поля B0 задается в СГС (Гаусс). 1 Тесла = 10 000 Гаусс.
    double B0 = 1.0 * 10000.0;
    size_t N = 10000;

    // Начальный физический импульс p0 = mc (движение релятивистское)
    double p0_mag = MASS_E * C_LIGHT * 1.0;

    // Запуск электрона вдоль оси X
    Particle electron({0.0, 0.0, 0.0}, {p0_mag, 0.0, 0.0}, Q_E, MASS_E);
    Field field = {{0.0, 0.0, 0.0}, {0.0, 0.0, B0}};  // Магнитное поле по оси Z

    // --- Выбор шага по времени dt ---
    // Релятивистский циклотронный период T = 2 * pi * gamma * m * c / (|q| * B)
    double initial_gamma = electron.gamma;
    double period = (2.0 * PI * initial_gamma * MASS_E * C_LIGHT) / (std::abs(Q_E) * B0);

    // Интегрируем ровно до половины периода (разворот на 180 градусов)
    double total_time = period / 2.0;
    double dt = total_time / N;

    // --- Симуляционный цикл ---
    for (size_t i = 0; i < N; ++i) {
        BorisPusher(electron, field, dt);
    }

    // --- Аналитический расчет финального состояния ---
    // Положение: диаметр траектории по оси Y равен 2R = 2 * p0 * c / (|q| * B0)
    double r_final_y_analytical = 2.0 * p0_mag * C_LIGHT / (std::abs(Q_E) * B0);

    // Направление импульса меняется на противоположное: (p0, 0, 0) -> (-p0, 0, 0)
    double p_final_x_analytical = -p0_mag;

    // --- Сравнение результатов ---
    Vec3 r_final_sim = electron.pos;
    Vec3 p_final_sim = electron.getMomentumPhys();

    double rel_error_pos = std::abs(r_final_sim.y - r_final_y_analytical) / std::abs(r_final_y_analytical);
    double rel_error_mom = std::abs(p_final_sim.x - p_final_x_analytical) / std::abs(p_final_x_analytical);

    EXPECT_LT(rel_error_pos, MAX_RELATIVE_ERROR) << "Ошибка координаты Y превысила лимит. Симуляция: " << r_final_sim.y << ", Аналитика: " << r_final_y_analytical;

    EXPECT_LT(rel_error_mom, MAX_RELATIVE_ERROR) << "Ошибка импульса Px превысила лимит. Симуляция: " << p_final_sim.x << ", Аналитика: " << p_final_x_analytical;
}

// =============================================================================
// ТЕСТ 3: Сохранение энергии для одиночной частицы (аналог pyHiChi)
// =============================================================================
// Проверяет строгое математическое свойство схемы Бориса: при нулевом электрическом
// поле энергия (величина безразмерного импульса u) за один шаг должна сохраняться
// с точностью до погрешности машинного округления double.
TEST(BorisPusherTest, SaveEnergySingleParticle) {
    // Инициализируем частицу с произвольным релятивистским импульсом
    Vec3 pos = {1.5, -2.2, 0.5};
    Vec3 p_phys = {MASS_E * C_LIGHT * 0.7, -MASS_E * C_LIGHT * 1.2, MASS_E * C_LIGHT * 0.4};
    Particle electron(pos, p_phys, Q_E, MASS_E);

    // Запоминаем квадрат нормы начального безразмерного импульса u^2
    double initial_u_sq = electron.u.norm2();

    // Чисто магнитное поле в произвольном направлении
    Field field = {{0.0, 0.0, 0.0}, {10000.0, -15000.0, 5000.0}};
    double dt = 0.05;

    // Делаем ровно один шаг пушера
    BorisPusher(electron, field, dt);

    double final_u_sq = electron.u.norm2();

    // Ожидаем сохранение величины импульса на уровне машинной точности double
    EXPECT_NEAR(initial_u_sq, final_u_sq, DOUBLE_MACHINE_PRECISION) << "Нарушена консервативность схемы Бориса. u^2 изменился с " << initial_u_sq << " до " << final_u_sq;
}

// =============================================================================
// ТЕСТ 4: Сохранение энергии для пачки частиц в структуре SoA (аналог pyHiChi)
// =============================================================================
// Самый важный тест для твоей будущей оптимизации под RISC-V. Он проверяет, что
// многопоточный SoA-код сохраняет математическую точность работы с памятью и
// алгоритм Бориса по-прежнему безупречно консервирует энергию для всех частиц.
TEST(BorisPusherTest, SaveEnergySoAChunk) {
    const size_t num_particles = 16;
    ParticlesSoA soa;
    soa.resize(0);  // Сброс размера перед заполнением

    std::vector<double> initial_u_sq(num_particles);

    // Заполняем SoA-структуру пачкой частиц со случайными параметрами
    for (size_t i = 0; i < num_particles; ++i) {
        double factor = 0.1 * (i + 1);
        Vec3 pos = {factor, -factor * 1.5, factor * 0.5};
        Vec3 p_phys = {MASS_E * C_LIGHT * factor, -MASS_E * C_LIGHT * factor * 0.8, MASS_E * C_LIGHT * factor * 1.3};

        Particle p(pos, p_phys, Q_E, MASS_E);
        soa.add(p);

        initial_u_sq[i] = p.u.norm2();
    }

    // Чисто магнитное поле
    Field field = {{0.0, 0.0, 0.0}, {12000.0, 8000.0, -10000.0}};
    double dt = 0.01;

    // Запускаем многопоточный/векторизованный пушер для всей структуры SoA
    BorisPusherSoA_OMP(soa, field, dt);

    // Проверяем консервативность энергии для каждой частицы в массиве по отдельности
    for (size_t i = 0; i < num_particles; ++i) {
        double final_u_sq = soa.ux[i] * soa.ux[i] + soa.uy[i] * soa.uy[i] + soa.uz[i] * soa.uz[i];

        EXPECT_NEAR(initial_u_sq[i], final_u_sq, DOUBLE_MACHINE_PRECISION)
            << "Нарушена консервативность SoA-пушера на частице с индексом " << i << ". Начальный u^2: " << initial_u_sq[i] << ", Конечный u^2: " << final_u_sq;
    }
}

}  // namespace bp
