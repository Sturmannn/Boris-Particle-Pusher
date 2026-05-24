#include "BorisPusher.h"
#include <iostream>
#include <iomanip>

void Run_MagneticOscillation_Sample() {
    std::cout << "--- Sample: Осцилляция в статическом магнитном поле B ---\n";

    // --- Начальные условия ---
    // B0 в СГС (Гаусс). 1 Тесла = 10^4 Гаусс
    double B0 = 1.0 * 10000.0;  // Гаусс
    size_t N = 10000;           // Количество шагов
    // Начальный импульс p_start = (p0, 0, 0)
    double p0_mag = bp::MASS_E * bp::C_LIGHT * 1.0;

    bp::Particle electron({0, 0, 0}, {p0_mag, 0, 0}, bp::Q_E, bp::MASS_E);
    bp::Field field = {{0, 0, 0}, {0, 0, B0}};  // B по оси Z

    // --- Выбор dt ---
    // Период вращения в СГС T = 2*pi*gamma*m*c / (|q|*B)
    double initial_gamma = electron.gamma;
    double period = (2.0 * bp::PI * initial_gamma * bp::MASS_E * bp::C_LIGHT) / (std::abs(bp::Q_E) * B0);
    // Один полный оборот
    double total_time = period;
    double dt = total_time / N;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Начальные параметры:\n"
              << "  Положение: (" << electron.pos.x << ", " << electron.pos.y << ", " << electron.pos.z << ")\n"
              << "  Импульс:   (" << electron.getMomentumPhys().x << ", " << electron.getMomentumPhys().y << ", " << electron.getMomentumPhys().z << ")\n"
              << "  Гамма-фактор: " << electron.gamma << "\n";

    // --- Цикл симуляции ---
    for (size_t i = 0; i < N; ++i) {
        bp::BorisPusher(electron, field, dt);
    }

    std::cout << "\nПараметры после одного оборота:\n"
              << "  Положение: (" << electron.pos.x << ", " << electron.pos.y << ", " << electron.pos.z << ")\n"
              << "  Импульс:   (" << electron.getMomentumPhys().x << ", " << electron.getMomentumPhys().y << ", " << electron.getMomentumPhys().z << ")\n"
              << "  Гамма-фактор: " << electron.gamma << "\n";

    std::cout << "\nЧастица должна была вернуться в начальную точку, т.к. прошел один период.\n";
}

int main() {
    std::cout << omp_get_max_threads() << " OpenMP threads will be used.\n\n";
    Run_MagneticOscillation_Sample();
    return 0;
}
