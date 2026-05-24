// #include "BorisPusher.h"
// #include <cassert>
// #include <cmath>
// #include <iomanip>
// #include <iostream>

// // Максимально допустимая относительная погрешность
// constexpr double MAX_RELATIVE_ERROR = 1e-3;
// namespace bp {
// void Test_RelativisticAcceleration() {
//     std::cout << "--- Тест 1: Релятивистское ускорение в статическом поле E ---\n";

//     // --- Начальные условия ---
//     // E0 в СГС (статвольт/см). 1 статвольт/см = 30000 В/м
//     // double E0 = 1e8 / 30000.0; // статвольт/см
//     double E0 = 1e8 / 30000.0;  // статвольт/см
//     size_t N = 10000;           // Количество шагов

//     Particle electron({0, 0, 0}, {0, 0, 0}, Q_E, MASS_E);
//     Field field = {{E0, 0, 0}, {0, 0, 0}};  // E по оси X

//     // --- Выбор dt ---
//     // Цель: разогнать до p = mc (gamma = sqrt(2))
//     // t = p / (qE) = mc / (qE)
//     double total_time_analytical = (MASS_E * C_LIGHT) / (std::abs(Q_E) * E0);
//     double dt = total_time_analytical / N;

//     // --- Цикл симуляции ---
//     for (size_t i = 0; i < N; ++i) {
//         BorisPusher(electron, field, dt);
//     }

//     // --- Аналитический результат ---
//     // x = (mc^2 / qE) * (gamma - 1)
//     // Так как q < 0, а E > 0, то x будет отрицательным.
//     // gamma_final = sqrt(2)
//     double factor = (MASS_E * C_LIGHT * C_LIGHT) / (Q_E * E0);  // Отрицательное число
//     double r_final_x_analytical = factor * (std::sqrt(2.0) - 1.0);

//     // Импульс p = q * E * t. Направлен против оси X (так как q < 0)
//     double p_final_x_analytical = Q_E * E0 * total_time_analytical;
//     // Это должно быть равно -mc

//     // --- Сравнение ---
//     Vec3 r_final_sim = electron.pos;
//     Vec3 p_final_sim = electron.getMomentumPhys();

//     std::cout << "Конечное положение (симуляция): x = " << r_final_sim.x << std::endl;
//     std::cout << "Конечное положение (аналитика): x = " << r_final_x_analytical << std::endl;
//     std::cout << "Относительная погрешность (положение): " << std::abs(r_final_sim.x - r_final_x_analytical) / std::abs(r_final_x_analytical) << std::endl << std::endl;

//     std::cout << "Конечный импульс (симуляция): px = " << p_final_sim.x << std::endl;
//     std::cout << "Конечный импульс (аналитика): px = " << p_final_x_analytical << std::endl;
//     std::cout << "Относительная погрешность (импульс): " << std::abs(p_final_sim.x - p_final_x_analytical) / std::abs(p_final_x_analytical) << std::endl;

//     assert(std::abs(r_final_sim.x - r_final_x_analytical) / std::abs(r_final_x_analytical) < MAX_RELATIVE_ERROR);
//     assert(std::abs(p_final_sim.x - p_final_x_analytical) / std::abs(p_final_x_analytical) < MAX_RELATIVE_ERROR);
// }

// void Test_MagneticOscillation() {
//     std::cout << "\n--- Тест 2: Осцилляция в статическом магнитном поле B ---\n";

//     // --- Начальные условия ---
//     // B0 в СГС (Гаусс). 1 Тесла = 10^4 Гаусс
//     double B0 = 1.0 * 10000.0;  // Гаусс
//     size_t N = 10000;           // Количество шагов
//     // Начальный импульс p_start = (p0, 0, 0)
//     double p0_mag = MASS_E * C_LIGHT * 1.0;

//     Particle electron({0, 0, 0}, {p0_mag, 0, 0}, Q_E, MASS_E);
//     Field field = {{0, 0, 0}, {0, 0, B0}};  // B по оси Z

//     // --- Выбор dt ---
//     // Период вращения в СГС T = 2*pi*gamma*m*c / (|q|*B)
//     // Это формула для релятивистского циклотронного периода
//     double initial_gamma = electron.gamma;
//     double period = (2.0 * PI * initial_gamma * MASS_E * C_LIGHT) / (std::abs(Q_E) * B0);
//     // Половина оборота
//     double total_time = period / 2.0;
//     double dt = total_time / N;

//     // --- Цикл симуляции ---
//     for (size_t i = 0; i < N; ++i) {
//         BorisPusher(electron, field, dt);
//     }

//     // --- Аналитический результат (для половины оборота) в СГС ---
//     // Электрон начинает в (0,0), v=(v0, 0, 0). B=(0, 0, B0).
//     // Сила F = q(v x B). q<0. v x B = -y. F направлена в +y.
//     double r_final_y_analytical = 2.0 * p0_mag * C_LIGHT / (std::abs(Q_E) * B0);

//     // Импульс разворачивается на 180 градусов: (p0, 0, 0) -> (-p0, 0, 0)
//     // потому что время симуляции в этом тесте специально установлено равным
//     // половине периода вращения частицы.
//     double p_final_x_analytical = -p0_mag;

//     // --- Сравнение ---
//     Vec3 r_final_sim = electron.pos;
//     Vec3 p_final_sim = electron.getMomentumPhys();

//     std::cout << "Конечное положение (симуляция): y = " << r_final_sim.y << std::endl;
//     std::cout << "Конечное положение (аналитика): y = " << r_final_y_analytical << std::endl;
//     std::cout << "Относительная погрешность (положение): " << std::abs(r_final_sim.y - r_final_y_analytical) / std::abs(r_final_y_analytical) << std::endl << std::endl;

//     std::cout << "Конечный импульс (симуляция): px = " << p_final_sim.x << std::endl;
//     std::cout << "Конечный импульс (аналитика): px = " << p_final_x_analytical << std::endl;
//     std::cout << "Относительная погрешность (импульс): " << std::abs(p_final_sim.x - p_final_x_analytical) / std::abs(p_final_x_analytical) << std::endl;
//     assert(std::abs(r_final_sim.y - r_final_y_analytical) / std::abs(r_final_y_analytical) < MAX_RELATIVE_ERROR);
//     assert(std::abs(p_final_sim.x - p_final_x_analytical) / std::abs(p_final_x_analytical) < MAX_RELATIVE_ERROR);
// }
// }  // namespace bp

// int main(int argc, char **argv) {
//     // В будущем здесь будет интеграция с Google Test
//     // For now, we run our simple checks.
//     bp::Test_RelativisticAcceleration();
//     bp::Test_MagneticOscillation();
//     return 0;
// }

#include "utests.hpp"

// Стандартная точка входа для сборки исполняемого файла Google Test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}