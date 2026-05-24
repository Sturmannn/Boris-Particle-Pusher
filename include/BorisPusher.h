#pragma once

#include <cmath>
#include <vector>
#include <omp.h>

namespace bp {

// Физические константы в СГС
constexpr double C_LIGHT = 29979245800.0;  // см/с
constexpr double MASS_E = 9.10938356e-28;  // грамм
constexpr double Q_E = -4.80320427e-10;    // статкулон
constexpr double PI = 3.141592653589793;

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    // Реализуем методы прямо здесь для инлайнинга
    Vec3 operator+(const Vec3 &other) const {
        return {x + other.x, y + other.y, z + other.z};
    }
    Vec3 operator-(const Vec3 &other) const {
        return {x - other.x, y - other.y, z - other.z};
    }
    Vec3 operator*(double scalar) const {
        return {x * scalar, y * scalar, z * scalar};
    }
    Vec3 operator/(double scalar) const {
        return {x / scalar, y / scalar, z / scalar};
    }
    Vec3 operator-() const {
        return {-x, -y, -z};
    }

    double norm2() const {
        return x * x + y * y + z * z;
    }
    double norm() const {
        return std::sqrt(norm2());
    }
};

inline Vec3 cross(const Vec3 &a, const Vec3 &b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

struct Particle {
    Vec3 pos;
    Vec3 u;
    double q;
    double m;
    double gamma;

    Particle(Vec3 position, Vec3 momentum_phys, double charge, double mass) : pos(position), q(charge), m(mass) {
        u = momentum_phys / (m * C_LIGHT);
        updateGamma();
    }

    void setU(const Vec3 &newU) {
        u = newU;
        updateGamma();
    }

    void updateGamma() {
        gamma = std::sqrt(1.0 + u.norm2());
    }

    Vec3 getVelocity() const {
        return u * (C_LIGHT / gamma);
    }

    Vec3 getMomentumPhys() const {
        return u * (m * C_LIGHT);
    }
};

struct ParticlesSoA {
    std::vector<double> x, y, z;
    std::vector<double> ux, uy, uz;
    std::vector<double> q, m;
    size_t size = 0;

    void resize(size_t n) {
        size = n;
        x.resize(n);
        y.resize(n);
        z.resize(n);
        ux.resize(n);
        uy.resize(n);
        uz.resize(n);
        q.resize(n);
        m.resize(n);
    }

    void add(const Particle &p) {
        x.push_back(p.pos.x);
        y.push_back(p.pos.y);
        z.push_back(p.pos.z);
        ux.push_back(p.u.x);
        uy.push_back(p.u.y);
        uz.push_back(p.u.z);
        q.push_back(p.q);
        m.push_back(p.m);
        size++;
    }
};

struct Field {
    Vec3 E;
    Vec3 B;
};

// === ГЛАВНОЕ ИЗМЕНЕНИЕ: Реализация BorisPusher здесь ===
inline void BorisPusher(Particle &p, const Field &f, double dt) {
    // Работаем с вектором u (u = gamma * v / c).
    // Уравнение движения в СГС: du/dt = (q / mc) * (E + (v/c) x B)
    // v/c = u / gamma
    // du/dt = (q / mc) * E  +  (q / (mc*gamma)) * (u x B)

    // Коэффициент для электрического поля (влияет на u): alpha = q * dt / (2 * m * c)
    double alpha = (p.q * dt) / (2.0 * p.m * C_LIGHT);

    // Коэффициент для магнитного поля (влияет на вращение): beta_coeff (СГС) = q * dt / (2 * m * c)
    // (будет поделен на gamma позже для получения вектора t)
    double beta_coeff = (p.q * dt) / (2.0 * p.m * C_LIGHT);

    // 1. Первый полу-толчок электрическим полем
    Vec3 u_minus = p.u + f.E * alpha;

    // 2. Вычисление гаммы на промежуточном шаге
    double gamma_minus = std::sqrt(1.0 + u_minus.norm2());

    // 3. Магнитное вращение
    // Вектор вращения (СГС) t = q * B * dt / (2 * m * gamma * c)
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
    p.setU(u_new);  // setU сама пересчитает gamma внутри

    // Обновляем позицию: r_new = r_old + v_new * dt
    // (Используем новую скорость, что соответствует схеме leapfrog, если считать u_new как u_{n+1/2})
    p.pos = p.pos + p.getVelocity() * dt;
}

// Объявление OMP функции (реализация останется в .cpp)
void BorisPusherOMP(std::vector<Particle> &particles, const Field &f, double dt);

// Объявление функции для SoA
void BorisPusherSoA_OMP(ParticlesSoA &p, const Field &f, double dt);

}  // namespace bp