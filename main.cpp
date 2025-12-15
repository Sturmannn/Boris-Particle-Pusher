#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

namespace bp
{

// ==========================================
//  Базовые структуры
// ==========================================

// Физические константы в СИ
constexpr double C_LIGHT = 299792458.0;       // Скорость света, м/с
constexpr double MASS_E = 9.10938356e-31;     // Масса электрона, кг
constexpr double Q_E = -1.60217662e-19;       // Заряд электрона, Кл
constexpr double PI = 3.141592653589793;

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(double scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    Vec3 operator/(double scalar) const { return {x / scalar, y / scalar, z / scalar}; }
    
    // Унарный минус
    Vec3 operator-() const { return {-x, -y, -z}; }

    double norm2() const { return x*x + y*y + z*z; }
    double norm() const { return std::sqrt(norm2()); }
};

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// ==========================================
//  Структура Частицы
// ==========================================
struct Particle {
    Vec3 pos;   // Позиция r
    Vec3 u;     // Нормированный импульс u = p_phys / (m * c) = gamma * v / c
    double q;   // Заряд
    double m;   // Масса
    double gamma; // Гамма-фактор

    Particle(Vec3 position, Vec3 momentum_phys, double charge, double mass) 
        : pos(position), q(charge), m(mass) 
    {
        u = momentum_phys / (m * C_LIGHT);
        updateGamma();
    }

    void setU(const Vec3& newU) {
        u = newU;
        updateGamma();
    }

    void updateGamma() {
        gamma = std::sqrt(1.0 + u.norm2());
    }

    Vec3 getVelocity() const {
        return u * (C_LIGHT / gamma);
    }
    
    // Метод для получения физического импульса p_phys = u * m * c
    Vec3 getMomentumPhys() const {
        return u * (m * C_LIGHT);
    }
};

struct Field {
    Vec3 E;
    Vec3 B;
};

// ==========================================
//  Алгоритм Бориса (Pusher)
// ==========================================
void BorisPusher(Particle& p, const Field& f, double dt) 
{
    // Работаем с вектором u (u = gamma * v / c).
    // Уравнение движения: du/dt = (q / (m*c)) * (E + v x B)
    // v = u * c / gamma
    // du/dt = (q / (m*c)) * E  +  (q / (m*gamma)) * (u x B)

    // Коэффициент для электрического поля (влияет на u): alpha = q * dt / (2 * m * c)
    double alpha = (p.q * dt) / (2.0 * p.m * C_LIGHT);
    
    // Коэффициент для магнитного поля (влияет на вращение): beta_coeff = q * dt / (2 * m)
    // (будет поделен на gamma позже для получения вектора t)
    double beta_coeff = (p.q * dt) / (2.0 * p.m);

    // 1. Первый полу-толчок электрическим полем
    Vec3 u_minus = p.u + f.E * alpha;

    // 2. Вычисление гаммы на промежуточном шаге
    double gamma_minus = std::sqrt(1.0 + u_minus.norm2());
    
    // 3. Магнитное вращение
    // Вектор вращения t = q * B * dt / (2 * m * gamma)
    Vec3 t = f.B * (beta_coeff / gamma_minus);
    
    // Вектор s = 2t / (1 + t^2)
    Vec3 s = t * 2.0 / (1.0 + t.norm2());
    
    // Вращение: u' = u- + (u- x t)
    Vec3 u_prime = u_minus + cross(u_minus, t);
    // u+ = u- + (u' x s)
    Vec3 u_plus = u_minus + cross(u_prime, s);

    // 4. Второй полу-толчок электрическим полем
    Vec3 u_new = u_plus + f.E * alpha;

    // 5. Обновляем импульс u и позицию
    p.setU(u_new); // setU сама пересчитает gamma внутри
    
    // Обновляем позицию: r_new = r_old + v_new * dt
    // (Используем новую скорость, что соответствует схеме leapfrog, если считать u_new как u_{n+1/2})
    p.pos = p.pos + p.getVelocity() * dt;
}

// ==========================================
//  Тестовые функции
// ==========================================

void Test_RelativisticAcceleration() {
    std::cout << "--- Тест 1: Релятивистское ускорение в статическом поле E ---\n";

    // --- Начальные условия ---
    double E0 = 1e8; // В/м
    size_t N = 10000;   // Количество шагов

    Particle electron({0,0,0}, {0,0,0}, Q_E, MASS_E);
    Field field = { {E0, 0, 0}, {0, 0, 0} }; // E по оси X

    // --- Выбор dt ---
    // Цель: разогнать до p = mc (gamma = sqrt(2))
    // t = p / (qE) = mc / (qE)
    double total_time_analytical = (MASS_E * C_LIGHT) / (std::abs(Q_E) * E0);
    double dt = total_time_analytical / N;

    // --- Цикл симуляции ---
    for(size_t i = 0; i < N; ++i) {
        BorisPusher(electron, field, dt);
    }

    // --- Аналитический результат ---
    // x = (mc^2 / qE) * (gamma - 1)
    // Так как q < 0, а E > 0, то x будет отрицательным.
    // gamma_final = sqrt(2)
    double factor = (MASS_E * C_LIGHT * C_LIGHT) / (Q_E * E0); // Отрицательное число
    double r_final_x_analytical = factor * (std::sqrt(2.0) - 1.0);
    
    // Импульс p = q * E * t. Направлен против оси X (так как q < 0)
    double p_final_x_analytical = Q_E * E0 * total_time_analytical; 
    // Это должно быть равно -mc

    // --- Сравнение ---
    Vec3 r_final_sim = electron.pos;
    Vec3 p_final_sim = electron.getMomentumPhys();

    std::cout << "Конечное положение (симуляция): x = " << r_final_sim.x << std::endl;
    std::cout << "Конечное положение (аналитика): x = " << r_final_x_analytical << std::endl;
    std::cout << "Относительная погрешность (положение): " << std::abs(r_final_sim.x - r_final_x_analytical) / std::abs(r_final_x_analytical) << std::endl << std::endl;

    std::cout << "Конечный импульс (симуляция): px = " << p_final_sim.x << std::endl;
    std::cout << "Конечный импульс (аналитика): px = " << p_final_x_analytical << std::endl;
    std::cout << "Относительная погрешность (импульс): " << std::abs(p_final_sim.x - p_final_x_analytical) / std::abs(p_final_x_analytical) << std::endl;
}

void Test_MagneticOscillation() {
    std::cout << "\n--- Тест 2: Осцилляция в статическом магнитном поле B ---\n";

    // --- Начальные условия ---
    double B0 = 1.0; // Тесла
    size_t N = 10000;   // Количество шагов
    // Начальный импульс p_start = (p0, 0, 0)
    double p0_mag = MASS_E * C_LIGHT * 1.0; 

    Particle electron({0,0,0}, {p0_mag, 0, 0}, Q_E, MASS_E);
    Field field = { {0, 0, 0}, {0, 0, B0} }; // B по оси Z

    // --- Выбор dt ---
    // Период вращения T = 2*pi*gamma*m / (|q|*B)
    // Это формула для релятивистского циклотронного периода
    double initial_gamma = electron.gamma;
    double period = (2.0 * PI * initial_gamma * MASS_E) / (std::abs(Q_E) * B0);
    // Половина оборота
    double total_time = period / 2.0;
    double dt = total_time / N;
    
    // --- Цикл симуляции ---
    for(size_t i = 0; i < N; ++i) {
        BorisPusher(electron, field, dt);
    }
    
    // --- Аналитический результат (для половины оборота) ---
    // Электрон начинает в (0,0), v=(v0, 0, 0). B=(0, 0, B0).
    // Сила F = q(v x B). q<0. v x B = -y. F направлена в +y.
    double r_final_y_analytical = 2.0 * p0_mag / (std::abs(Q_E) * B0);
    
    // Импульс разворачивается на 180 градусов: (p0, 0, 0) -> (-p0, 0, 0)
    // потому что время симуляции в этом тесте специально установлено равным половине периода вращения частицы.
    double p_final_x_analytical = -p0_mag;

    // --- Сравнение ---
    Vec3 r_final_sim = electron.pos;
    Vec3 p_final_sim = electron.getMomentumPhys();

    std::cout << "Конечное положение (симуляция): y = " << r_final_sim.y << std::endl;
    std::cout << "Конечное положение (аналитика): y = " << r_final_y_analytical << std::endl;
    std::cout << "Относительная погрешность (положение): " << std::abs(r_final_sim.y - r_final_y_analytical) / std::abs(r_final_y_analytical) << std::endl << std::endl;

    std::cout << "Конечный импульс (симуляция): px = " << p_final_sim.x << std::endl;
    std::cout << "Конечный импульс (аналитика): px = " << p_final_x_analytical << std::endl;
    std::cout << "Относительная погрешность (импульс): " << std::abs(p_final_sim.x - p_final_x_analytical) / std::abs(p_final_x_analytical) << std::endl;
}

} // namespace bp

int main(int argc, char** argv) {
    bp::Test_RelativisticAcceleration();
    bp::Test_MagneticOscillation();

    return 0;
}