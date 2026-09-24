// parallel.cpp: v1, paraleliza con parallel for la gravedad y la busqueda de absorciones.

#include <omp.h>
#include "screensaver_common.hpp"

static const char* VERSION_NAME = "Paralela v1";

// Avanza la simulacion un paso. 'threadCount' hilos en cada region paralela.
static void updateGalaxyParallelV1(Galaxy& galaxy, float deltaTimeSeconds,
                                   const Config& config, int threadCount) {
    // O(N), secuencial: el orden de los atractores debe ser determinista.
    buildAttractors(galaxy);

    std::vector<Particle>& particles = galaxy.particles;
    const std::vector<Attractor>& attractors = galaxy.attractors;
    const int n = static_cast<int>(particles.size());

    // Cada i solo escribe particles[i] y lee attractors, asi que no hay carreras.
    #pragma omp parallel for num_threads(threadCount) schedule(static) \
            default(none) shared(particles, attractors, config, deltaTimeSeconds, n)
    for (int i = 0; i < n; ++i) {
        updateParticle(particles[i], i, attractors, deltaTimeSeconds, config);
    }
    // (barrera implicita: todas las posiciones estan actualizadas)

    // Fase O(N^2), la mas pesada; cada j solo escribe predatorOf[j].
    std::vector<int>& predatorOf = galaxy.predatorOf;
    #pragma omp parallel for num_threads(threadCount) schedule(static) \
            default(none) shared(particles, predatorOf, n)
    for (int j = 0; j < n; ++j) {
        predatorOf[j] = findPredator(particles, j);
    }
    // (barrera implicita: predatorOf completo antes de aplicarlo)

    // Secuenciales: dependencias entre particulas y uso de rand().
    resolveAbsorption(galaxy, config);
    processSupernovas(galaxy, config);
}

// El dibujo se mantiene secuencial en esta version (se paraleliza en v2).
static void renderGalaxyParallelV1(Galaxy& galaxy, const Config& config) {
    rasterizeRows(galaxy, config, 0, config.height);
}

// Por defecto la mitad de los procesadores logicos (mas o menos los nucleos fisicos).
static int defaultThreadCount() {
    const int processors = omp_get_num_procs();
    return (processors > 1) ? processors / 2 : 1;
}

// Con hilos = procesadores la espera activa de OpenMP se degrada; avisamos al usuario.
static void warnIfOversubscribed(int threadCount) {
    if (threadCount >= omp_get_num_procs() && std::getenv("OMP_WAIT_POLICY") == nullptr) {
        std::fprintf(stderr, "Aviso: %d hilos >= %d procesadores logicos. Si los FPS caen, "
                             "ejecute con OMP_WAIT_POLICY=passive o use menos hilos.\n",
                     threadCount, omp_get_num_procs());
    }
}

int main(int argc, char* argv[]) {
    Config config;
    if (!parseArguments(argc, argv, true, defaultThreadCount(), config)) {
        return EXIT_FAILURE;
    }
    warnIfOversubscribed(config.threadCount);
    seedRandom(config);
    // Tambien se pasa num_threads() en cada pragma.
    omp_set_num_threads(config.threadCount);

    const int threadCount = config.threadCount;
    return runScreensaver(
        config, VERSION_NAME, true,
        [&config, threadCount](Galaxy& galaxy, float dt) {
            updateGalaxyParallelV1(galaxy, dt, config, threadCount);
        },
        [&config](Galaxy& galaxy) { renderGalaxyParallelV1(galaxy, config); });
}
