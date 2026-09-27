#include <riscv_vector.h>
#include <vector>
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

    // Предвычисляем скалярные константы, чтобы не делать это в каждом потоке
    double Ex = f.E.x;
    double Ey = f.E.y;
    double Ez = f.E.z;
    double Bx = f.B.x;
    double By = f.B.y;
    double Bz = f.B.z;

    double two_c = 2.0 * C_LIGHT;
    double c_dt = C_LIGHT * dt;

#ifdef __riscv_vector
// =========================================================================
// ВЕКТОРНЫЙ ВАРИАНТ (Компилируется при -march=rv64gcv)
// =========================================================================
// Открываем параллельную секцию
#pragma omp parallel
    {
        // 1. Узнаем, кто мы такие в пуле потоков
        int tid = omp_get_thread_num();
        int num_threads = omp_get_num_threads();

        // 2. Вычисляем границы нашего куска массива (целочисленное деление с правильным округлением)
        size_t chunk_size = N / num_threads;
        size_t remainder = N % num_threads;

        // Распределяем остаток по первым потокам для идеального баланса
        size_t start = tid * chunk_size + (tid < remainder ? tid : remainder);
        size_t end = start + chunk_size + (tid < remainder ? 1 : 0);

        // 3. Векторный RVV-цикл работает только внутри локальных границ (от start до end)
        size_t vl;
        for (size_t i = start; i < end; i += vl) {
            // Запрашиваем длину вектора для оставшихся элементов в НАШЕМ куске
            vl = __riscv_vsetvl_e64m1(end - i);

            // --- ЗАГРУЗКА ДАННЫХ ИЗ ПАМЯТИ ---
            vfloat64m1_t q_vec = __riscv_vle64_v_f64m1(&q[i], vl);
            vfloat64m1_t m_vec = __riscv_vle64_v_f64m1(&m[i], vl);
            vfloat64m1_t ux_vec = __riscv_vle64_v_f64m1(&ux[i], vl);
            vfloat64m1_t uy_vec = __riscv_vle64_v_f64m1(&uy[i], vl);
            vfloat64m1_t uz_vec = __riscv_vle64_v_f64m1(&uz[i], vl);

            // --- ПРЕДВАРИТЕЛЬНЫЕ КОЭФФИЦИЕНТЫ ---
            vfloat64m1_t num = __riscv_vfmul_vf_f64m1(q_vec, dt, vl);
            vfloat64m1_t denom = __riscv_vfmul_vf_f64m1(m_vec, two_c, vl);
            vfloat64m1_t coeff = __riscv_vfdiv_vv_f64m1(num, denom, vl);

            // --- 1. ПЕРВЫЙ ПОЛУ-ТОЛЧОК E (u_minus) ---
            vfloat64m1_t um_x = __riscv_vfmacc_vf_f64m1(ux_vec, Ex, coeff, vl);
            vfloat64m1_t um_y = __riscv_vfmacc_vf_f64m1(uy_vec, Ey, coeff, vl);
            vfloat64m1_t um_z = __riscv_vfmacc_vf_f64m1(uz_vec, Ez, coeff, vl);

            // --- 2. ГАММА ---
            vfloat64m1_t gamma2 = __riscv_vfmv_v_f_f64m1(1.0, vl);
            gamma2 = __riscv_vfmacc_vv_f64m1(gamma2, um_x, um_x, vl);
            gamma2 = __riscv_vfmacc_vv_f64m1(gamma2, um_y, um_y, vl);
            gamma2 = __riscv_vfmacc_vv_f64m1(gamma2, um_z, um_z, vl);

            vfloat64m1_t gamma = __riscv_vfsqrt_v_f64m1(gamma2, vl);
            vfloat64m1_t gamma_inv = __riscv_vfrdiv_vf_f64m1(gamma, 1.0, vl);

            // --- 3. МАГНИТНОЕ ВРАЩЕНИЕ ---
            vfloat64m1_t t_scale = __riscv_vfmul_vv_f64m1(coeff, gamma_inv, vl);

            vfloat64m1_t tx = __riscv_vfmul_vf_f64m1(t_scale, Bx, vl);
            vfloat64m1_t ty = __riscv_vfmul_vf_f64m1(t_scale, By, vl);
            vfloat64m1_t tz = __riscv_vfmul_vf_f64m1(t_scale, Bz, vl);

            vfloat64m1_t t_sq = __riscv_vfmv_v_f_f64m1(0.0, vl);
            t_sq = __riscv_vfmacc_vv_f64m1(t_sq, tx, tx, vl);
            t_sq = __riscv_vfmacc_vv_f64m1(t_sq, ty, ty, vl);
            t_sq = __riscv_vfmacc_vv_f64m1(t_sq, tz, tz, vl);

            vfloat64m1_t one_plus_tsq = __riscv_vfadd_vf_f64m1(t_sq, 1.0, vl);
            vfloat64m1_t s_scale = __riscv_vfrdiv_vf_f64m1(one_plus_tsq, 2.0, vl);

            vfloat64m1_t sx = __riscv_vfmul_vv_f64m1(tx, s_scale, vl);
            vfloat64m1_t sy = __riscv_vfmul_vv_f64m1(ty, s_scale, vl);
            vfloat64m1_t sz = __riscv_vfmul_vv_f64m1(tz, s_scale, vl);

            vfloat64m1_t up_x = __riscv_vfmacc_vv_f64m1(um_x, um_y, tz, vl);
            up_x = __riscv_vfnmsac_vv_f64m1(up_x, um_z, ty, vl);
            vfloat64m1_t up_y = __riscv_vfmacc_vv_f64m1(um_y, um_z, tx, vl);
            up_y = __riscv_vfnmsac_vv_f64m1(up_y, um_x, tz, vl);
            vfloat64m1_t up_z = __riscv_vfmacc_vv_f64m1(um_z, um_x, ty, vl);
            up_z = __riscv_vfnmsac_vv_f64m1(up_z, um_y, tx, vl);

            vfloat64m1_t uplus_x = __riscv_vfmacc_vv_f64m1(um_x, up_y, sz, vl);
            uplus_x = __riscv_vfnmsac_vv_f64m1(uplus_x, up_z, sy, vl);
            vfloat64m1_t uplus_y = __riscv_vfmacc_vv_f64m1(um_y, up_z, sx, vl);
            uplus_y = __riscv_vfnmsac_vv_f64m1(uplus_y, up_x, sz, vl);
            vfloat64m1_t uplus_z = __riscv_vfmacc_vv_f64m1(um_z, up_x, sy, vl);
            uplus_z = __riscv_vfnmsac_vv_f64m1(uplus_z, up_y, sx, vl);

            // --- 4. ВТОРОЙ ПОЛУ-ТОЛЧОК E (u_new) ---
            vfloat64m1_t unew_x = __riscv_vfmacc_vf_f64m1(uplus_x, Ex, coeff, vl);
            vfloat64m1_t unew_y = __riscv_vfmacc_vf_f64m1(uplus_y, Ey, coeff, vl);
            vfloat64m1_t unew_z = __riscv_vfmacc_vf_f64m1(uplus_z, Ez, coeff, vl);

            __riscv_vse64_v_f64m1(&ux[i], unew_x, vl);
            __riscv_vse64_v_f64m1(&uy[i], unew_y, vl);
            __riscv_vse64_v_f64m1(&uz[i], unew_z, vl);

            // --- 5. ОБНОВЛЕНИЕ ПОЗИЦИИ ---
            vfloat64m1_t gamma_new2 = __riscv_vfmv_v_f_f64m1(1.0, vl);
            gamma_new2 = __riscv_vfmacc_vv_f64m1(gamma_new2, unew_x, unew_x, vl);
            gamma_new2 = __riscv_vfmacc_vv_f64m1(gamma_new2, unew_y, unew_y, vl);
            gamma_new2 = __riscv_vfmacc_vv_f64m1(gamma_new2, unew_z, unew_z, vl);

            vfloat64m1_t gamma_new = __riscv_vfsqrt_v_f64m1(gamma_new2, vl);
            vfloat64m1_t gamma_new_inv = __riscv_vfrdiv_vf_f64m1(gamma_new, 1.0, vl);
            vfloat64m1_t v_scale = __riscv_vfmul_vf_f64m1(gamma_new_inv, c_dt, vl);

            vfloat64m1_t x_vec = __riscv_vle64_v_f64m1(&x[i], vl);
            vfloat64m1_t y_vec = __riscv_vle64_v_f64m1(&y[i], vl);
            vfloat64m1_t z_vec = __riscv_vle64_v_f64m1(&z[i], vl);

            x_vec = __riscv_vfmacc_vv_f64m1(x_vec, unew_x, v_scale, vl);
            y_vec = __riscv_vfmacc_vv_f64m1(y_vec, unew_y, v_scale, vl);
            z_vec = __riscv_vfmacc_vv_f64m1(z_vec, unew_z, v_scale, vl);

            __riscv_vse64_v_f64m1(&x[i], x_vec, vl);
            __riscv_vse64_v_f64m1(&y[i], y_vec, vl);
            __riscv_vse64_v_f64m1(&z[i], z_vec, vl);
        }
    }  // Конец #pragma omp parallel
#else
// =========================================================================
// СКАЛЯРНЫЙ РЕЗЕРВНЫЙ ВАРИАНТ (Компилируется при -march=rv64gc)
// =========================================================================
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
#endif
}
}  // namespace bp