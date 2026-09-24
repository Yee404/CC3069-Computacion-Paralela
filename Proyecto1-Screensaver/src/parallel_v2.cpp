// parallel_v2.cpp: v2, una region paralela por frame, reparto dinamico y dibujo en paralelo.

#include <omp.h>
#include "screensaver_common.hpp"

static const char* VERSION_NAME = "Paralela v2";

// Iteraciones que toma un hilo cada vez en el reparto dinamico.
static const int PREDATOR_CHUNK = 64;

// Avanza la simulacion un paso dentro de una unica region paralela.
static void updateGalaxyParallelV2(Galaxy& galaxy, float deltaTimeSeconds,
                                   const Config& config, int threadCount) {
    std::vector<Particle>& particles = galaxy.particles;
    const std::vector<Attractor>& attractors = galaxy.attractors;
    std::vector<int>& predatorOf = galaxy.predatorOf;
    const int n = static_cast<int>(particles.size());

    #pragma omp parallel num_threads(threadCount) default(none) \
            shared(galaxy, particles, attractors, predatorOf, config, deltaTimeSeconds, n)
    {
        // Un hilo arma los atractores; la barrera de single hace que todos lo vean.
        #pragma omp single
        buildAttractors(galaxy);

        // Fase 2 (todos): gravedad y movimiento. i escribe solo particles[i].
        #pragma omp for schedule(static)
        for (int i = 0; i < n; ++i) {
            updateParticle(particles[i], i, attractors, deltaTimeSeconds, config);
        }
        // barrera implicita

        // Fase O(N^2); dynamic porque las particulas inactivas terminan al instante.
        #pragma omp for schedule(dynamic, PREDATOR_CHUNK)
        for (int j = 0; j < n; ++j) {
            predatorOf[j] = findPredator(particles, j);
        }
        // barrera implicita

        // Absorcion y supernovas en un solo hilo (usan rand()); los demas esperan en la barrera.
        #pragma omp single
        {
            resolveAbsorption(galaxy, config);
            processSupernovas(galaxy, config);
        }
    }
}

// Dibujo por franjas: cada una la pinta un solo hilo y la imagen sale igual que en secuencial.
static void renderGalaxyParallelV2(Galaxy& galaxy, const Config& config, int threadCount) {
    const int wantedBands = threadCount * RASTER_BANDS_PER_THREAD;
    int bandRows = (config.height + wantedBands - 1) / wantedBands;
    if (bandRows < RASTER_MIN_BAND_ROWS) bandRows = RASTER_MIN_BAND_ROWS;
    const int bandCount = (config.height + bandRows - 1) / bandRows;

    #pragma omp parallel for num_threads(threadCount) schedule(dynamic, 1) \
            default(none) shared(galaxy, config, bandCount, bandRows)
    for (int band = 0; band < bandCount; ++band) {
        const int rowBegin = band * bandRows;
        rasterizeRows(galaxy, config, rowBegin, rowBegin + bandRows);
    }
    // El present de SDL lo hace el hilo principal despues de esta barrera.
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
    omp_set_num_threads(config.threadCount);

    const int threadCount = config.threadCount;
    return runScreensaver(
        config, VERSION_NAME, true,
        [&config, threadCount](Galaxy& galaxy, float dt) {
            updateGalaxyParallelV2(galaxy, dt, config, threadCount);
        },
        [&config, threadCount](Galaxy& galaxy) {
            renderGalaxyParallelV2(galaxy, config, threadCount);
        });
}
