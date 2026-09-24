// sequential.cpp: version secuencial, la linea base para medir el speedup.

#include "screensaver_common.hpp"

static const char* VERSION_NAME = "Secuencial";

// Avanza la simulacion un paso de 'deltaTimeSeconds' segundos.
static void updateGalaxySequential(Galaxy& galaxy, float deltaTimeSeconds, const Config& config) {
    buildAttractors(galaxy);

    const int n = static_cast<int>(galaxy.particles.size());
    for (int i = 0; i < n; ++i) {
        updateParticle(galaxy.particles[i], i, galaxy.attractors, deltaTimeSeconds, config);
    }

    for (int j = 0; j < n; ++j) {
        galaxy.predatorOf[j] = findPredator(galaxy.particles, j);
    }

    resolveAbsorption(galaxy, config);
    processSupernovas(galaxy, config);
}

// Dibuja todo el canvas (todas las filas) en el framebuffer.
static void renderGalaxySequential(Galaxy& galaxy, const Config& config) {
    rasterizeRows(galaxy, config, 0, config.height);
}

int main(int argc, char* argv[]) {
    Config config;
    if (!parseArguments(argc, argv, false, 1, config)) {
        return EXIT_FAILURE;
    }
    seedRandom(config);

    return runScreensaver(
        config, VERSION_NAME, false,
        [&config](Galaxy& galaxy, float dt) { updateGalaxySequential(galaxy, dt, config); },
        [&config](Galaxy& galaxy) { renderGalaxySequential(galaxy, config); });
}
