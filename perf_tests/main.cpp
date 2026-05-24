#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <omp.h>
#include "BorisPusher.h"

// Хелпер для генерации данных
void SetupData(size_t N, std::vector<bp::Particle>& particles, bp::Field& f) {
    particles.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        particles.emplace_back(bp::Vec3{0, 0, 0}, bp::Vec3{0, 0, 0}, bp::Q_E, bp::MASS_E);
    }
    f = {{0, 0, 0}, {0, 0, 10000}};
}

// Хелпер для SoA
void SetupDataSoA(size_t N, bp::ParticlesSoA& particles, bp::Field& f) {
    particles.resize(N);
    // Заполнение нулями для теста скорости ок
    std::fill(particles.q.begin(), particles.q.end(), bp::Q_E);
    std::fill(particles.m.begin(), particles.m.end(), bp::MASS_E);
    f = {{0, 0, 0}, {0, 0, 10000}};
}

// 1. Базовая версия (AoS) - OpenMP управляется через Args
static void BM_AoS_OpenMP(benchmark::State& state) {
    size_t N = state.range(0);     // Количество частиц
    int threads = state.range(1);  // Количество потоков

    // Устанавливаем потоки явно для бенчмарка
    omp_set_num_threads(threads);

    std::vector<bp::Particle> particles;
    bp::Field f;
    SetupData(N, particles, f);
    double dt = 1e-11;

    for (auto _ : state) {
        bp::BorisPusherOMP(particles, f, dt);
    }

    // Метрика: миллионов частиц в секунду
    state.counters["MParticles/s"] = benchmark::Counter((double)state.iterations() * N / 1e6, benchmark::Counter::kIsRate);
}

// 2. Оптимизированная версия (SoA + SIMD)
static void BM_SoA_SIMD(benchmark::State& state) {
    size_t N = state.range(0);
    int threads = state.range(1);

    omp_set_num_threads(threads);

    bp::ParticlesSoA particles;
    bp::Field f;
    SetupDataSoA(N, particles, f);
    double dt = 1e-11;

    for (auto _ : state) {
        bp::BorisPusherSoA_OMP(particles, f, dt);
    }

    state.counters["MParticles/s"] = benchmark::Counter((double)state.iterations() * N / 1e6, benchmark::Counter::kIsRate);
}

// Регистрируем бенчмарки
// Аргументы: Range(N_min, N_max), Range(Threads_min, Threads_max)

// Тест масштабируемости AoS (1M частиц, 1, 2, 4, 8 потоков)
int64_t countOfParticles = 100000000;  // 1 миллиард частиц
BENCHMARK(BM_AoS_OpenMP)->Args({countOfParticles, 1})->Args({countOfParticles, 2})->Args({countOfParticles, 4})->Args({countOfParticles, 8})->Unit(benchmark::kSecond);

// Тест масштабируемости SoA (1M частиц, 1, 2, 4, 8 потоков)
BENCHMARK(BM_SoA_SIMD)->Args({countOfParticles, 1})->Args({countOfParticles, 2})->Args({countOfParticles, 4})->Args({countOfParticles, 8})->Unit(benchmark::kSecond);
BENCHMARK_MAIN();