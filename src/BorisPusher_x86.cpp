#include "BorisPusher.h"

namespace bp {

void BorisPusherOMP(std::vector<Particle>& particles, const Field& f, double dt) {
    // omp_set_num_threads(1);
#pragma omp parallel for schedule(static)
    for (size_t i = 0; i < particles.size(); ++i) {
        BorisPusher(particles[i], f, dt);
    }
}

void BorisPusherSoA_OMP(ParticlesSoA& p, const Field& f, double dt) {
    size_t N = p.size;

    double* __restrict x = p.x.data();
    double* __restrict y = p.y.data();
    double* __restrict z = p.z.data();

    double* __restrict ux = p.ux.data();
    double* __restrict uy = p.uy.data();
    double* __restrict uz = p.uz.data();

    const double* __restrict q = p.q.data();
    const double* __restrict m = p.m.data();

    // Константы поля
    double Ex = f.E.x;
    double Ey = f.E.y;
    double Ez = f.E.z;
    double Bx = f.B.x;
    double By = f.B.y;
    double Bz = f.B.z;

#pragma omp parallel for simd schedule(static)
    for (size_t i = 0; i < N; ++i) {
        // Локальные копии для регистров
        double q_val = q[i];
        double m_val = m[i];

        // Предварительные вычисления коэффициентов
        // alpha = q * dt / (2 * m * c)
        double coeff = (q_val * dt) / (2.0 * m_val * C_LIGHT);

        // 1. u_minus
        double um_x = ux[i] + Ex * coeff;
        double um_y = uy[i] + Ey * coeff;
        double um_z = uz[i] + Ez * coeff;

        // 2. gamma
        double gamma2 = 1.0 + um_x * um_x + um_y * um_y + um_z * um_z;
        double gamma_inv = 1.0 / std::sqrt(gamma2);  // Деление медленное, лучше умножать на обратное

        // 3. Вращение t
        double t_scale = coeff * gamma_inv;
        double tx = Bx * t_scale;
        double ty = By * t_scale;
        double tz = Bz * t_scale;

        double t_sq = tx * tx + ty * ty + tz * tz;

        // s = 2t / (1 + t^2)
        double s_scale = 2.0 / (1.0 + t_sq);
        double sx = tx * s_scale;
        double sy = ty * s_scale;
        double sz = tz * s_scale;

        // u_prime = u_minus + cross(u_minus, t)
        double up_x = um_x + (um_y * tz - um_z * ty);
        double up_y = um_y + (um_z * tx - um_x * tz);
        double up_z = um_z + (um_x * ty - um_y * tx);

        // u_plus = u_minus + cross(u_prime, s)
        double uplus_x = um_x + (up_y * sz - up_z * sy);
        double uplus_y = um_y + (up_z * sx - up_x * sz);
        double uplus_z = um_z + (up_x * sy - up_y * sx);

        // 4. u_new
        double unew_x = uplus_x + Ex * coeff;
        double unew_y = uplus_y + Ey * coeff;
        double unew_z = uplus_z + Ez * coeff;

        // Сохраняем импульс
        ux[i] = unew_x;
        uy[i] = unew_y;
        uz[i] = unew_z;

        // 5. Обновляем позицию
        // v = u * c / gamma_new
        // gamma нужно пересчитать для новой скорости
        double gamma_new_inv = 1.0 / std::sqrt(1.0 + unew_x * unew_x + unew_y * unew_y + unew_z * unew_z);
        double v_scale = C_LIGHT * gamma_new_inv * dt;

        x[i] += unew_x * v_scale;
        y[i] += unew_y * v_scale;
        z[i] += unew_z * v_scale;
    }
}
}  // namespace bp
