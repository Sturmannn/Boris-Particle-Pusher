#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <omp.h>
#include <algorithm>
#include "BorisPusher.h"

namespace bp {

// =============================================================================
// Базовая фикстура для бенчмарков (аналог BaseFixture в pyHiChi)
// =============================================================================
class PusherBenchmarkFixture : public ::benchmark::Fixture {
protected:
    std::mt19937_64 rng;  // Современный и воспроизводимый генератор ПСЧ
    double dt;
    Field field;

    void SetUp(const ::benchmark::State& state) override {
        // Фиксируем сид (seed = 1) для строгой воспроизводимости между запусками
        rng.seed(1);
        dt = 0.001;  // Шаг по времени как в бенчмарках pyHiChi

        // Генерируем случайное однородное электромагнитное поле в диапазоне [-10, 10]
        field.E = {urand(-10.0, 10.0), urand(-10.0, 10.0), urand(-10.0, 10.0)};
        field.B = {urand(-10.0, 10.0), urand(-10.0, 10.0), urand(-10.0, 10.0)};
    }

    void TearDown(const ::benchmark::State&) override {
    }

    // Хелпер для генерации равномерно распределенных вещественных чисел
    double urand(double min, double max) {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng);
    }
};

// =============================================================================
// Фикстура для тестирования структуры данных AoS (Array of Structures)
// =============================================================================
class PusherAoS_Fixture : public PusherBenchmarkFixture {
public:
    std::vector<Particle> particles;

    void SetUp(const ::benchmark::State& state) override {
        PusherBenchmarkFixture::SetUp(state);
        size_t N = state.range(0);  // Количество частиц из аргументов бенчмарка
        particles.reserve(N);

        // Генерируем частицы с распределением параметров в точности как в pyHiChi
        for (size_t i = 0; i < N; ++i) {
            Vec3 pos = {urand(-10.0, 10.0), urand(-10.0, 10.0), urand(-10.0, 10.0)};
            Vec3 p_phys = {urand(-10.0, 10.0), urand(-10.0, 10.0), urand(-10.0, 10.0)};

            particles.emplace_back(pos, p_phys, Q_E, MASS_E);
        }
    }

    void TearDown(const ::benchmark::State& state) override {
        particles.clear();
        PusherBenchmarkFixture::TearDown(state);
    }
};

// =============================================================================
// Фикстура для тестирования структуры данных SoA (Structure of Arrays)
// =============================================================================
class PusherSoA_Fixture : public PusherBenchmarkFixture {
public:
    ParticlesSoA particles;

    void SetUp(const ::benchmark::State& state) override {
        PusherBenchmarkFixture::SetUp(state);
        size_t N = state.range(0);
        particles.resize(N);

        // Заполняем SoA-структуру теми же случайными частицами, что и в AoS.
        // Пересчитываем физический импульс в безразмерный u для SoA.
        for (size_t i = 0; i < N; ++i) {
            Vec3 pos = {urand(-10.0, 10.0), urand(-10.0, 10.0), urand(-10.0, 10.0)};
            Vec3 p_phys = {urand(-10.0, 10.0), urand(-10.0, 10.0), urand(-10.0, 10.0)};
            Vec3 u = p_phys / (MASS_E * C_LIGHT);

            particles.x[i] = pos.x;
            particles.y[i] = pos.y;
            particles.z[i] = pos.z;
            particles.ux[i] = u.x;
            particles.uy[i] = u.y;
            particles.uz[i] = u.z;
            particles.q[i] = Q_E;
            particles.m[i] = MASS_E;
        }
    }

    void TearDown(const ::benchmark::State& state) override {
        particles.resize(0);
        PusherBenchmarkFixture::TearDown(state);
    }
};

// Функция генерации аргументов (проходит по степеням 2 до max_threads)
static void CustomArguments(benchmark::internal::Benchmark* b) {
    int64_t countOfParticles = 1000000;  // 1 миллион частиц
    int64_t steps = 1000;                // 1000 шагов

    int max_threads = omp_get_max_threads();

    for (int threads = 1; threads <= max_threads; threads *= 2) {
        b->Args({countOfParticles, steps, threads});
    }

    // Если количество ядер не степень двойки, добавляем и его
    if ((max_threads & (max_threads - 1)) != 0) {
        b->Args({countOfParticles, steps, max_threads});
    }

    b->Iterations(1);
}

// =============================================================================
// ВАЖНО: Определяем и регистрируем бенчмарки ВНУТРИ namespace bp.
// Благодаря этому макросы Google Benchmark не будут использовать квалификатор "bp::"
// при конкатенации имени класса, что устранит все синтаксические ошибки компилятора.
// =============================================================================

// 1. Бенчмарк для AoS версии (OpenMP)
BENCHMARK_DEFINE_F(PusherAoS_Fixture, pusher)(benchmark::State& state) {
    size_t N = state.range(0);
    int64_t steps = state.range(1);
    int threads = state.range(2);
    omp_set_num_threads(threads);

    // Замеряем время работы только этого блока
    while (state.KeepRunning()) {
        for (int64_t iter = 0; iter < steps; ++iter) {
            bp::BorisPusherOMP(particles, field, dt);
        }
    }

    // Рассчитываем пропускную способность: (Частицы * Шаги) / Время
    state.counters["MParticles/s"] = benchmark::Counter((double)state.iterations() * N * steps / 1e6, benchmark::Counter::kIsRate);
}
BENCHMARK_REGISTER_F(PusherAoS_Fixture, pusher)->Apply(CustomArguments)->Unit(benchmark::kSecond);

// 2. Бенчмарк для SoA версии (OpenMP + SIMD)
BENCHMARK_DEFINE_F(PusherSoA_Fixture, pusher)(benchmark::State& state) {
    size_t N = state.range(0);
    int64_t steps = state.range(1);
    int threads = state.range(2);
    omp_set_num_threads(threads);

    while (state.KeepRunning()) {
        for (int64_t iter = 0; iter < steps; ++iter) {
            bp::BorisPusherSoA_OMP(particles, field, dt);
        }
    }

    state.counters["MParticles/s"] = benchmark::Counter((double)state.iterations() * N * steps / 1e6, benchmark::Counter::kIsRate);
}
BENCHMARK_REGISTER_F(PusherSoA_Fixture, pusher)->Apply(CustomArguments)->Unit(benchmark::kSecond);

}  // namespace bp

// Точка входа Google Benchmark в глобальной области видимости
BENCHMARK_MAIN();