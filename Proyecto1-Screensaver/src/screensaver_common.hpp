// screensaver_common.hpp: codigo que comparten las tres versiones (no usa OpenMP).

#ifndef SCREENSAVER_COMMON_HPP
#define SCREENSAVER_COMMON_HPP

#include <SDL.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

// ---- Constantes (valores por defecto y limites de validacion) ----

// Canvas: la asignacion pide un minimo de 640x480.
static const int DEFAULT_WIDTH  = 800;
static const int DEFAULT_HEIGHT = 600;
static const int MIN_WIDTH      = 640;
static const int MIN_HEIGHT     = 480;
static const int MAX_WIDTH      = 3840;
static const int MAX_HEIGHT     = 2160;

// Radio (px) con el que nace cada particula de polvo.
static const float DEFAULT_MIN_RADIUS = 1.0f;
static const float DEFAULT_MAX_RADIUS = 4.0f;

// Protoestrellas: nacen con gravedad propia y sirven de semilla.
static const float PROTOSTAR_PROBABILITY = 0.03f;
static const float PROTOSTAR_MIN_RADIUS  = 5.0f;
static const float PROTOSTAR_MAX_RADIUS  = 8.0f;

// Rapidez inicial (px/s) de cada particula.
static const float DEFAULT_MIN_SPEED = 20.0f;
static const float DEFAULT_MAX_SPEED = 90.0f;
static const float MAX_ALLOWED_SPEED = 2000.0f;

// Constante G de la gravedad: a = G*m / (d^2 + eps^2).
static const float DEFAULT_GRAVITY = 250.0f;
static const float MAX_GRAVITY     = 100000.0f;

// Tiempo de vida (s) de una estrella antes de explotar.
static const float DEFAULT_LIFETIME = 6.0f;
static const float MIN_LIFETIME     = 0.5f;
static const float MAX_LIFETIME     = 600.0f;

// Limites para N y para la cantidad de hilos.
static const long MAX_PARTICLE_COUNT = 1000000;
static const long MAX_THREAD_COUNT   = 1024;
static const long MAX_BENCH_FRAMES   = 1000000;

// Solo las particulas con este radio o mas generan gravedad.
static const float GRAVITY_MIN_RADIUS = 5.0f;
// Suavizado (px^2) que evita aceleraciones infinitas cuando d -> 0.
static const float GRAVITY_SOFTENING2 = 36.0f;
// Rapidez maxima tras aplicar gravedad (evita efectos "honda" exagerados).
static const float SPEED_LIMIT = 450.0f;

// Absorcion: una estrella se come a quien tenga el centro dentro de ella.
static const float ABSORB_MARGIN  = 0.5f;
static const float ABSORB_GROWTH  = 0.8f;
static const float MAX_RADIUS     = 40.0f;

// Supernova: desde STAR_RADIUS la estrella envejece y al morir explota.
static const float STAR_RADIUS        = 12.0f;
static const int   MAX_FRAGMENTS      = 40;
static const float FRAGMENT_MIN_SPEED = 120.0f;
static const float FRAGMENT_MAX_SPEED = 320.0f;
// Fraccion del area de la estrella que se reparte entre los fragmentos.
static const float FRAGMENT_AREA_FRACTION = 0.9f;
// Segundos para que una particula absorbida vuelva como polvo.
static const float RESPAWN_DELAY = 1.5f;

// Cada cuanto (segundos) se actualiza el FPS mostrado en titulo y consola.
static const double FPS_UPDATE_INTERVAL = 0.5;

// dt fijo del benchmark, asi todas las versiones hacen el mismo trabajo.
static const float BENCH_FIXED_DT = 1.0f / 60.0f;

// Paso maximo en modo interactivo: evita saltos si un frame tarda mucho.
static const float MAX_INTERACTIVE_DT = 0.05f;

// Dibujo paralelo de v2: franjas por hilo y alto minimo de cada franja.
static const int RASTER_BANDS_PER_THREAD = 4;
static const int RASTER_MIN_BAND_ROWS    = 4;

// Densidad del fondo de estrellas fijas: 1 estrella por cada N pixeles.
static const int BACKGROUND_STAR_DENSITY = 1500;

// Color de fondo: azul muy oscuro, "espacio profundo".
static const Uint8 BACKGROUND_R = 5;
static const Uint8 BACKGROUND_G = 5;
static const Uint8 BACKGROUND_B = 18;

// Sin always_inline GCC no expande los kernels dentro de OpenMP y todo va ~2x mas lento.
#if defined(__GNUC__) || defined(__clang__)
#define KERNEL_INLINE inline __attribute__((always_inline))
#else
#define KERNEL_INLINE inline
#endif

// M_PI no es estandar en C++; se define manualmente.
static const float PI = 3.14159265358979323846f;

// ---- Tipos ----

// Parametros de ejecucion, todos configurables desde la linea de comandos.
struct Config {
    int      particleCount = 0;                   // N: cantidad de particulas (obligatorio).
    int      threadCount   = 1;                   // Hilos OpenMP (solo versiones paralelas).
    int      width         = DEFAULT_WIDTH;       // Ancho del canvas en px.
    int      height        = DEFAULT_HEIGHT;      // Alto del canvas en px.
    float    minRadius     = DEFAULT_MIN_RADIUS;  // Radio minimo al nacer.
    float    maxRadius     = DEFAULT_MAX_RADIUS;  // Radio maximo al nacer.
    float    minSpeed      = DEFAULT_MIN_SPEED;   // Rapidez minima inicial (px/s).
    float    maxSpeed      = DEFAULT_MAX_SPEED;   // Rapidez maxima inicial (px/s).
    float    gravity       = DEFAULT_GRAVITY;     // Constante gravitacional G.
    float    lifetime      = DEFAULT_LIFETIME;    // Vida media de una estrella (s).
    int      benchFrames   = 0;                   // >0 activa modo benchmark con ese # de frames.
    unsigned seed          = 0;                   // Semilla del generador pseudoaleatorio.
    bool     seedGiven     = false;               // true si el usuario paso --seed.
    bool     vsync         = true;                // Sincronizar con el monitor (~60 FPS max).
    std::string csvPath;                          // Si no esta vacio, se agrega una fila CSV.
};

// Particula de la galaxia: (x, y) es el centro y su masa es radius^2.
struct Particle {
    float  x, y;        // Centro (px).
    float  vx, vy;      // Velocidad (px/s).
    float  radius;      // Radio (px). Masa = radius^2.
    float  age;         // Segundos que lleva siendo estrella (radio >= STAR_RADIUS).
    float  lifetime;    // Edad a la que esta particula explota si es estrella.
    float  deadTime;    // Segundos desde que fue absorbida (si !active).
    Uint32 color;       // Color ARGB8888 empaquetado.
    bool   active;      // false = absorbida, esperando reutilizarse.
};

// Copia de una estrella para leer la gravedad sin carreras de datos.
struct Attractor {
    float x, y;    // Centro.
    float mass;    // radius^2.
    int   index;   // Indice de la particula original (para excluirse a si misma).
};

// Estrella fija del fondo (decorativa, no participa en la simulacion).
struct BackgroundStar {
    int    x, y;
    Uint32 color;
};

// Paleta base; cada particula le suma una pequena variacion aleatoria.
struct PaletteColor { Uint8 r, g, b; };
static const PaletteColor GALAXY_PALETTE[] = {
    {155, 176, 255},  // estrella azul (tipo O/B)
    {202, 215, 255},  // blanco azulado (tipo A)
    {255, 244, 234},  // blanco amarillento (tipo F)
    {255, 210, 161},  // amarillo (tipo G, como el Sol)
    {255, 160,  90},  // naranja (tipo K)
    {255, 100,  80},  // rojo (tipo M)
    {190, 120, 255},  // nebulosa violeta
    {110, 220, 230},  // nebulosa cian
    {255, 120, 200},  // nebulosa rosa
};
static const int PALETTE_SIZE = sizeof(GALAXY_PALETTE) / sizeof(GALAXY_PALETTE[0]);

// Recursos de SDL que se crean al inicio y se destruyen al final.
struct SdlContext {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture*  texture  = nullptr;  // Textura "streaming" donde se sube el framebuffer.
};

// Estado de la simulacion; los buffers se reservan una sola vez.
struct Galaxy {
    std::vector<Particle>       particles;         // Las N particulas.
    std::vector<Attractor>      attractors;        // Estrellas con gravedad (se reconstruye cada frame).
    std::vector<int>            predatorOf;        // predatorOf[j] = quien absorbe a j (-1 = nadie).
    std::vector<int>            freeSlots;         // Indices de particulas inactivas disponibles.
    std::vector<Uint32>         framebuffer;       // width*height pixeles ARGB8888.
    std::vector<BackgroundStar> backgroundStars;   // Fondo decorativo fijo.
};

// Acumuladores de tiempo para el reporte final (segundos).
struct FrameTimings {
    double updateSeconds = 0.0;  // Gravedad + absorcion + explosiones.
    double renderSeconds = 0.0;  // Rasterizado + subida de textura + present.
    int    frames        = 0;
};

// ---- Utilidades ----

// Empaqueta un color RGB opaco en formato ARGB8888.
static inline Uint32 packColor(Uint8 r, Uint8 g, Uint8 b) {
    return (0xFFu << 24) | (static_cast<Uint32>(r) << 16) |
           (static_cast<Uint32>(g) << 8) | static_cast<Uint32>(b);
}

// Mezcla lineal entre dos colores ARGB (t en [0,1]).
static inline Uint32 lerpColor(Uint32 from, Uint32 to, float t) {
    if (t <= 0.0f) return from;
    if (t >= 1.0f) return to;
    auto channel = [&](int shift) {
        float a = static_cast<float>((from >> shift) & 0xFF);
        float b = static_cast<float>((to >> shift) & 0xFF);
        return static_cast<Uint8>(a + (b - a) * t);
    };
    return packColor(channel(16), channel(8), channel(0));
}

// Tiempo monotono actual en segundos (independiente de OpenMP).
static inline double nowSeconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

// Flotante aleatorio en [min, max]; usa rand(), asi que solo desde un hilo.
static float randomRange(float minValue, float maxValue) {
    float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minValue + t * (maxValue - minValue);
}

// Color pseudoaleatorio de la paleta con variacion de [-25, 25] por canal.
static Uint32 randomGalaxyColor() {
    const PaletteColor& base = GALAXY_PALETTE[std::rand() % PALETTE_SIZE];
    auto jitterChannel = [](int channel) {
        int result = channel + (std::rand() % 51) - 25;
        if (result < 0) result = 0;
        if (result > 255) result = 255;
        return static_cast<Uint8>(result);
    };
    Uint8 r = jitterChannel(base.r);
    Uint8 g = jitterChannel(base.g);
    Uint8 b = jitterChannel(base.b);
    return packColor(r, g, b);
}

// ---- Lectura y validacion de argumentos (programacion defensiva) ----

// Convierte texto a entero dentro del rango; rechaza basura y overflow.
static bool parseLongInRange(const char* text, long minValue, long maxValue,
                             const char* name, long& outValue) {
    if (text == nullptr || *text == '\0') {
        std::fprintf(stderr, "Error: falta el valor de %s.\n", name);
        return false;
    }
    errno = 0;
    char* endPtr = nullptr;
    long parsed = std::strtol(text, &endPtr, 10);
    if (endPtr == text || *endPtr != '\0' || errno == ERANGE) {
        std::fprintf(stderr, "Error: '%s' no es un entero valido para %s.\n", text, name);
        return false;
    }
    if (parsed < minValue || parsed > maxValue) {
        std::fprintf(stderr, "Error: %s debe estar entre %ld y %ld (recibido: %ld).\n",
                     name, minValue, maxValue, parsed);
        return false;
    }
    outValue = parsed;
    return true;
}

// Igual que parseLongInRange pero para numeros reales.
static bool parseFloatInRange(const char* text, float minValue, float maxValue,
                              const char* name, float& outValue) {
    if (text == nullptr || *text == '\0') {
        std::fprintf(stderr, "Error: falta el valor de %s.\n", name);
        return false;
    }
    errno = 0;
    char* endPtr = nullptr;
    float parsed = std::strtof(text, &endPtr);
    if (endPtr == text || *endPtr != '\0' || errno == ERANGE || !std::isfinite(parsed)) {
        std::fprintf(stderr, "Error: '%s' no es un numero valido para %s.\n", text, name);
        return false;
    }
    if (parsed < minValue || parsed > maxValue) {
        std::fprintf(stderr, "Error: %s debe estar entre %.2f y %.2f (recibido: %.2f).\n",
                     name, minValue, maxValue, parsed);
        return false;
    }
    outValue = parsed;
    return true;
}

// Imprime la ayuda de uso del programa.
static void printUsage(const char* programName, bool supportsThreads) {
    std::fprintf(stderr,
        "Uso: %s <N>%s [opciones]\n"
        "  N                    cantidad de particulas a renderizar (1..%ld)\n",
        programName, supportsThreads ? " [hilos]" : "", MAX_PARTICLE_COUNT);
    if (supportsThreads) {
        std::fprintf(stderr,
            "  hilos                hilos OpenMP (1..%ld, por defecto: mitad de los procesadores logicos)\n"
            "  -t, --threads T      igual que el argumento posicional 'hilos'\n",
            MAX_THREAD_COUNT);
    }
    std::fprintf(stderr,
        "Opciones:\n"
        "  -W, --width  W       ancho del canvas (%d..%d, por defecto %d)\n"
        "  -H, --height H       alto del canvas (%d..%d, por defecto %d)\n"
        "  --min-radius R       radio minimo al nacer (por defecto %.0f)\n"
        "  --max-radius R       radio maximo al nacer (por defecto %.0f)\n"
        "  --min-speed V        rapidez minima inicial px/s (por defecto %.0f)\n"
        "  --max-speed V        rapidez maxima inicial px/s (por defecto %.0f)\n"
        "  -g, --gravity G      constante gravitacional (0..%.0f, por defecto %.0f)\n"
        "  -l, --lifetime S     segundos de vida de una estrella antes de explotar\n"
        "                       (%.1f..%.0f, por defecto %.1f)\n"
        "  -s, --seed S         semilla pseudoaleatoria (por defecto: hora actual)\n"
        "  -f, --frames F       modo benchmark: simula F frames con dt fijo, sin vsync,\n"
        "                       imprime tiempos y termina\n"
        "  --no-vsync           desactiva la sincronizacion vertical (FPS sin limite)\n"
        "  --csv ARCHIVO        agrega una fila con los resultados a ARCHIVO (benchmark)\n"
        "  -h, --help           muestra esta ayuda\n"
        "Sin argumentos, el programa solicita N%s por consola.\n",
        MIN_WIDTH, MAX_WIDTH, DEFAULT_WIDTH, MIN_HEIGHT, MAX_HEIGHT, DEFAULT_HEIGHT,
        DEFAULT_MIN_RADIUS, DEFAULT_MAX_RADIUS, DEFAULT_MIN_SPEED, DEFAULT_MAX_SPEED,
        MAX_GRAVITY, DEFAULT_GRAVITY, MIN_LIFETIME, MAX_LIFETIME, DEFAULT_LIFETIME,
        supportsThreads ? " y los hilos" : "");
}

// Pide un entero por consola, con hasta 3 intentos.
static bool promptLong(const char* question, long minValue, long maxValue,
                       const char* name, long& outValue) {
    char line[128];
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::printf("%s", question);
        std::fflush(stdout);
        if (std::fgets(line, sizeof(line), stdin) == nullptr) {
            std::fprintf(stderr, "Error: no se recibio entrada.\n");
            return false;
        }
        line[std::strcspn(line, "\r\n")] = '\0';  // quita el salto de linea
        if (parseLongInRange(line, minValue, maxValue, name, outValue)) return true;
    }
    std::fprintf(stderr, "Error: demasiados intentos invalidos.\n");
    return false;
}

// Valida relaciones entre parametros que no se pueden revisar uno por uno.
static bool validateConfig(const Config& config) {
    if (config.minRadius > config.maxRadius) {
        std::fprintf(stderr, "Error: --min-radius (%.1f) no puede ser mayor que --max-radius (%.1f).\n",
                     config.minRadius, config.maxRadius);
        return false;
    }
    if (config.minSpeed > config.maxSpeed) {
        std::fprintf(stderr, "Error: --min-speed (%.1f) no puede ser mayor que --max-speed (%.1f).\n",
                     config.minSpeed, config.maxSpeed);
        return false;
    }
    if (!config.csvPath.empty() && config.benchFrames == 0) {
        std::fprintf(stderr, "Aviso: --csv solo tiene efecto en modo benchmark (--frames).\n");
    }
    return true;
}

// Lee y valida los argumentos; si no hay ninguno pide N (y los hilos) por consola.
static bool parseArguments(int argc, char* argv[], bool supportsThreads,
                           int defaultThreads, Config& config) {
    const char* programName = (argc > 0) ? argv[0] : "screensaver";
    config.threadCount = defaultThreads;
    long value = 0;
    float floatValue = 0.0f;

    // Sin argumentos: modo de ingreso de datos por consola.
    if (argc <= 1) {
        if (!promptLong("Ingrese la cantidad de particulas N: ", 1, MAX_PARTICLE_COUNT, "N", value))
            return false;
        config.particleCount = static_cast<int>(value);
        if (supportsThreads) {
            char question[96];
            std::snprintf(question, sizeof(question),
                          "Ingrese la cantidad de hilos (1..%ld): ", MAX_THREAD_COUNT);
            if (!promptLong(question, 1, MAX_THREAD_COUNT, "hilos", value)) return false;
            config.threadCount = static_cast<int>(value);
        }
        return true;
    }

    int positionalCount = 0;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        // Devuelve el valor de la opcion actual o nullptr si falta.
        auto nextValue = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : nullptr; };
        auto is = [&](const char* shortName, const char* longName) {
            return (shortName && std::strcmp(arg, shortName) == 0) || std::strcmp(arg, longName) == 0;
        };

        if (is("-h", "--help")) {
            printUsage(programName, supportsThreads);
            return false;
        } else if (is("-W", "--width")) {
            if (!parseLongInRange(nextValue(), MIN_WIDTH, MAX_WIDTH, "--width", value)) return false;
            config.width = static_cast<int>(value);
        } else if (is("-H", "--height")) {
            if (!parseLongInRange(nextValue(), MIN_HEIGHT, MAX_HEIGHT, "--height", value)) return false;
            config.height = static_cast<int>(value);
        } else if (is(nullptr, "--min-radius")) {
            if (!parseFloatInRange(nextValue(), 0.5f, STAR_RADIUS, "--min-radius", floatValue)) return false;
            config.minRadius = floatValue;
        } else if (is(nullptr, "--max-radius")) {
            if (!parseFloatInRange(nextValue(), 0.5f, STAR_RADIUS, "--max-radius", floatValue)) return false;
            config.maxRadius = floatValue;
        } else if (is(nullptr, "--min-speed")) {
            if (!parseFloatInRange(nextValue(), 0.0f, MAX_ALLOWED_SPEED, "--min-speed", floatValue)) return false;
            config.minSpeed = floatValue;
        } else if (is(nullptr, "--max-speed")) {
            if (!parseFloatInRange(nextValue(), 0.0f, MAX_ALLOWED_SPEED, "--max-speed", floatValue)) return false;
            config.maxSpeed = floatValue;
        } else if (is("-g", "--gravity")) {
            if (!parseFloatInRange(nextValue(), 0.0f, MAX_GRAVITY, "--gravity", floatValue)) return false;
            config.gravity = floatValue;
        } else if (is("-l", "--lifetime")) {
            if (!parseFloatInRange(nextValue(), MIN_LIFETIME, MAX_LIFETIME, "--lifetime", floatValue)) return false;
            config.lifetime = floatValue;
        } else if (is("-s", "--seed")) {
            if (!parseLongInRange(nextValue(), 0, 2147483647L, "--seed", value)) return false;
            config.seed = static_cast<unsigned>(value);
            config.seedGiven = true;
        } else if (is("-f", "--frames")) {
            if (!parseLongInRange(nextValue(), 1, MAX_BENCH_FRAMES, "--frames", value)) return false;
            config.benchFrames = static_cast<int>(value);
        } else if (is(nullptr, "--no-vsync")) {
            config.vsync = false;
        } else if (is(nullptr, "--csv")) {
            const char* path = nextValue();
            if (path == nullptr || *path == '\0') {
                std::fprintf(stderr, "Error: --csv requiere un nombre de archivo.\n");
                return false;
            }
            config.csvPath = path;
        } else if (supportsThreads && is("-t", "--threads")) {
            if (!parseLongInRange(nextValue(), 1, MAX_THREAD_COUNT, "hilos", value)) return false;
            config.threadCount = static_cast<int>(value);
        } else if (arg[0] == '-' && arg[1] != '\0' && !(arg[1] >= '0' && arg[1] <= '9')) {
            std::fprintf(stderr, "Error: opcion desconocida '%s'.\n", arg);
            printUsage(programName, supportsThreads);
            return false;
        } else {
            // Argumentos posicionales: N y (opcionalmente) hilos.
            if (positionalCount == 0) {
                if (!parseLongInRange(arg, 1, MAX_PARTICLE_COUNT, "N", value)) return false;
                config.particleCount = static_cast<int>(value);
            } else if (positionalCount == 1 && supportsThreads) {
                if (!parseLongInRange(arg, 1, MAX_THREAD_COUNT, "hilos", value)) return false;
                config.threadCount = static_cast<int>(value);
            } else {
                std::fprintf(stderr, "Error: argumento de sobra '%s'.\n", arg);
                printUsage(programName, supportsThreads);
                return false;
            }
            ++positionalCount;
        }
    }

    if (config.particleCount <= 0) {
        std::fprintf(stderr, "Error: falta el parametro obligatorio N.\n");
        printUsage(programName, supportsThreads);
        return false;
    }
    // En benchmark se desactiva vsync: se quiere medir el tiempo real de calculo.
    if (config.benchFrames > 0) config.vsync = false;
    return validateConfig(config);
}

// Inicializa la semilla pseudoaleatoria (la dada por el usuario o la hora).
static void seedRandom(Config& config) {
    if (!config.seedGiven) config.seed = static_cast<unsigned>(std::time(nullptr));
    std::srand(config.seed);
}

// ---- Creacion de particulas ----

// Mantiene el centro de la particula dentro del canvas.
static inline void clampInsideCanvas(Particle& p, int width, int height) {
    if (p.x < p.radius) p.x = p.radius;
    if (p.y < p.radius) p.y = p.radius;
    if (p.x > width - p.radius) p.x = width - p.radius;
    if (p.y > height - p.radius) p.y = height - p.radius;
}

// Reinicia la edad y le da una vida de lifetime +-30%.
static inline void resetLife(Particle& p, const Config& config) {
    p.age = 0.0f;
    p.deadTime = 0.0f;
    p.lifetime = config.lifetime * randomRange(0.7f, 1.3f);
    p.active = true;
}

// Radio inicial: polvo normal o, a veces, una protoestrella.
static float randomBirthRadius(const Config& config) {
    if (randomRange(0.0f, 1.0f) < PROTOSTAR_PROBABILITY) {
        return randomRange(PROTOSTAR_MIN_RADIUS, PROTOSTAR_MAX_RADIUS);
    }
    return randomRange(config.minRadius, config.maxRadius);
}

// Polvo inicial en un disco con velocidad tangencial para que la galaxia gire.
static Particle makeGalaxyDust(const Config& config) {
    Particle p;
    p.radius = randomBirthRadius(config);

    const float centerX = config.width * 0.5f;
    const float centerY = config.height * 0.5f;
    const float diskRadius = 0.48f * static_cast<float>(config.width < config.height ? config.width : config.height);
    // sqrt(u) da densidad uniforme por area dentro del disco.
    const float distance = diskRadius * std::sqrt(randomRange(0.0f, 1.0f));
    const float angle = randomRange(0.0f, 2.0f * PI);
    p.x = centerX + distance * std::cos(angle);
    p.y = centerY + distance * std::sin(angle);

    // Velocidad perpendicular al radio (giro antihorario) mas algo de ruido.
    const float speed = randomRange(config.minSpeed, config.maxSpeed);
    const float swirl = angle + 0.5f * PI + randomRange(-0.3f, 0.3f);
    p.vx = speed * std::cos(swirl);
    p.vy = speed * std::sin(swirl);

    p.color = randomGalaxyColor();
    resetLife(p, config);
    clampInsideCanvas(p, config.width, config.height);
    return p;
}

// Reaparicion de polvo en un punto aleatorio del canvas, direccion aleatoria.
static void respawnAsDust(Particle& p, const Config& config) {
    p.radius = randomBirthRadius(config);
    p.x = randomRange(p.radius, config.width - p.radius);
    p.y = randomRange(p.radius, config.height - p.radius);
    const float speed = randomRange(config.minSpeed, config.maxSpeed);
    const float angle = randomRange(0.0f, 2.0f * PI);
    p.vx = speed * std::cos(angle);
    p.vy = speed * std::sin(angle);
    p.color = randomGalaxyColor();
    resetLife(p, config);
}

// Estrellas de fondo con un LCG propio para no mover la secuencia de rand().
static std::vector<BackgroundStar> makeBackgroundStars(int width, int height) {
    std::vector<BackgroundStar> stars;
    const int count = (width * height) / BACKGROUND_STAR_DENSITY;
    stars.reserve(static_cast<std::size_t>(count));
    unsigned state = 12345u;
    auto next = [&state]() { state = state * 1103515245u + 12345u; return (state >> 8) & 0xFFFFFF; };
    for (int i = 0; i < count; ++i) {
        BackgroundStar star;
        star.x = static_cast<int>(next() % static_cast<unsigned>(width));
        star.y = static_cast<int>(next() % static_cast<unsigned>(height));
        Uint8 brightness = static_cast<Uint8>(60 + next() % 120);
        star.color = packColor(brightness, brightness, static_cast<Uint8>(brightness + (255 - brightness) / 3));
        stars.push_back(star);
    }
    return stars;
}

// Crea las N particulas, reserva los buffers auxiliares y el framebuffer.
static void initializeGalaxy(Galaxy& galaxy, const Config& config) {
    const std::size_t n = static_cast<std::size_t>(config.particleCount);
    galaxy.particles.clear();
    galaxy.particles.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        galaxy.particles.push_back(makeGalaxyDust(config));
    }
    galaxy.attractors.reserve(n);
    galaxy.predatorOf.assign(n, -1);
    galaxy.freeSlots.reserve(n);
    galaxy.framebuffer.assign(static_cast<std::size_t>(config.width) * config.height, 0);
    galaxy.backgroundStars = makeBackgroundStars(config.width, config.height);
}

// ---- Fase 1: campo gravitacional ----

// Copia las estrellas con gravedad; secuencial para que el orden no cambie.
static void buildAttractors(Galaxy& galaxy) {
    galaxy.attractors.clear();
    const int n = static_cast<int>(galaxy.particles.size());
    for (int i = 0; i < n; ++i) {
        const Particle& p = galaxy.particles[i];
        if (p.active && p.radius >= GRAVITY_MIN_RADIUS) {
            Attractor a;
            a.x = p.x;
            a.y = p.y;
            a.mass = p.radius * p.radius;
            a.index = i;
            galaxy.attractors.push_back(a);
        }
    }
}

// Kernel paralelizable: gravedad, movimiento y rebote; solo escribe particles[i].
static KERNEL_INLINE void updateParticle(Particle& p, int index, const std::vector<Attractor>& attractors,
                                  float deltaTimeSeconds, const Config& config) {
    if (!p.active) {
        p.deadTime += deltaTimeSeconds;
        return;
    }

    float ax = 0.0f;
    float ay = 0.0f;
    const std::size_t attractorCount = attractors.size();
    for (std::size_t k = 0; k < attractorCount; ++k) {
        const Attractor& a = attractors[k];
        if (a.index == index) continue;  // una estrella no se atrae a si misma
        const float dx = a.x - p.x;
        const float dy = a.y - p.y;
        const float distance2 = dx * dx + dy * dy + GRAVITY_SOFTENING2;
        const float invDistance = 1.0f / std::sqrt(distance2);
        // |a| = G*m/d^2 ; direccion = (dx, dy)/d  =>  G*m*(dx,dy)/d^3
        const float strength = config.gravity * a.mass * invDistance * invDistance * invDistance;
        ax += strength * dx;
        ay += strength * dy;
    }

    p.vx += ax * deltaTimeSeconds;
    p.vy += ay * deltaTimeSeconds;

    const float speed2 = p.vx * p.vx + p.vy * p.vy;
    if (speed2 > SPEED_LIMIT * SPEED_LIMIT) {
        const float scale = SPEED_LIMIT / std::sqrt(speed2);
        p.vx *= scale;
        p.vy *= scale;
    }

    p.x += p.vx * deltaTimeSeconds;
    p.y += p.vy * deltaTimeSeconds;

    // Rebote contra los bordes: se refleja la componente de velocidad.
    if (p.x < p.radius) {
        p.x = p.radius;
        p.vx = -p.vx;
    } else if (p.x > config.width - p.radius) {
        p.x = config.width - p.radius;
        p.vx = -p.vx;
    }
    if (p.y < p.radius) {
        p.y = p.radius;
        p.vy = -p.vy;
    } else if (p.y > config.height - p.radius) {
        p.y = config.height - p.radius;
        p.vy = -p.vy;
    }

    if (p.radius >= STAR_RADIUS) p.age += deltaTimeSeconds;
}

// ---- Fase 2: absorcion ----

// Kernel paralelizable O(N): devuelve la estrella mas grande que contiene a j, o -1.
static KERNEL_INLINE int findPredator(const std::vector<Particle>& particles, int j) {
    const Particle& prey = particles[j];
    if (!prey.active) return -1;
    const int n = static_cast<int>(particles.size());
    int predator = -1;
    float predatorRadius = prey.radius + ABSORB_MARGIN;
    if (predatorRadius < GRAVITY_MIN_RADIUS) predatorRadius = GRAVITY_MIN_RADIUS;
    for (int i = 0; i < n; ++i) {
        const Particle& candidate = particles[i];
        // Filtro barato primero (radio) y luego la prueba de distancia.
        if (!candidate.active || candidate.radius < predatorRadius || i == j) continue;
        const float dx = candidate.x - prey.x;
        const float dy = candidate.y - prey.y;
        if (dx * dx + dy * dy < candidate.radius * candidate.radius) {
            predator = i;
            predatorRadius = candidate.radius;
        }
    }
    return predator;
}

// Aplica las absorciones; es secuencial porque varias presas pueden ir al mismo depredador.
static void resolveAbsorption(Galaxy& galaxy, const Config& config) {
    std::vector<Particle>& particles = galaxy.particles;
    const int n = static_cast<int>(particles.size());

    for (int j = 0; j < n; ++j) {
        const int i = galaxy.predatorOf[j];
        if (i < 0 || galaxy.predatorOf[i] >= 0) continue;
        Particle& predator = particles[i];
        Particle& prey = particles[j];
        if (!predator.active || !prey.active) continue;

        const float predatorMass = predator.radius * predator.radius;
        const float preyMass = prey.radius * prey.radius;
        const float totalMass = predatorMass + preyMass;
        predator.vx = (predator.vx * predatorMass + prey.vx * preyMass) / totalMass;
        predator.vy = (predator.vy * predatorMass + prey.vy * preyMass) / totalMass;

        float newRadius = std::sqrt(predatorMass + ABSORB_GROWTH * preyMass);
        if (newRadius > MAX_RADIUS) newRadius = MAX_RADIUS;
        predator.radius = newRadius;
        clampInsideCanvas(predator, config.width, config.height);

        prey.active = false;
        prey.deadTime = 0.0f;
    }
}

// ---- Fase 3: supernovas y reaparicion ----

// Explota la estrella en fragmentos que salen a angulos 2*pi*k/K.
static void explodeStar(Galaxy& galaxy, int starIndex, const Config& config) {
    Particle& star = galaxy.particles[starIndex];
    int fragmentCount = static_cast<int>(galaxy.freeSlots.size());
    if (fragmentCount > MAX_FRAGMENTS) fragmentCount = MAX_FRAGMENTS;

    const float starArea = star.radius * star.radius;
    float fragmentRadius = std::sqrt(FRAGMENT_AREA_FRACTION * starArea / (fragmentCount + 1));
    if (fragmentRadius < config.minRadius) fragmentRadius = config.minRadius;
    if (fragmentRadius > GRAVITY_MIN_RADIUS - 0.5f) fragmentRadius = GRAVITY_MIN_RADIUS - 0.5f;

    const float angleOffset = randomRange(0.0f, 2.0f * PI);
    for (int k = 0; k < fragmentCount; ++k) {
        const int slot = galaxy.freeSlots.back();
        galaxy.freeSlots.pop_back();
        Particle& fragment = galaxy.particles[slot];

        const float angle = angleOffset + 2.0f * PI * k / fragmentCount;
        const float burst = randomRange(FRAGMENT_MIN_SPEED, FRAGMENT_MAX_SPEED);
        const float spawnDistance = star.radius * 0.6f;
        fragment.radius = fragmentRadius * randomRange(0.6f, 1.2f);
        fragment.x = star.x + spawnDistance * std::cos(angle);
        fragment.y = star.y + spawnDistance * std::sin(angle);
        fragment.vx = star.vx + burst * std::cos(angle);
        fragment.vy = star.vy + burst * std::sin(angle);
        // Los fragmentos heredan el color de la estrella, levemente mezclado.
        fragment.color = lerpColor(star.color, randomGalaxyColor(), 0.35f);
        resetLife(fragment, config);
        clampInsideCanvas(fragment, config.width, config.height);
    }

    // La propia estrella queda como un remanente pequeno.
    star.radius = fragmentRadius;
    resetLife(star, config);
}

// Explota estrellas viejas y revive polvo; secuencial porque usa rand().
static void processSupernovas(Galaxy& galaxy, const Config& config) {
    std::vector<Particle>& particles = galaxy.particles;
    const int n = static_cast<int>(particles.size());

    galaxy.freeSlots.clear();
    for (int i = n - 1; i >= 0; --i) {
        if (!particles[i].active) galaxy.freeSlots.push_back(i);
    }

    for (int i = 0; i < n; ++i) {
        Particle& p = particles[i];
        if (p.active && p.radius >= STAR_RADIUS && p.age >= p.lifetime) {
            explodeStar(galaxy, i, config);
        }
    }

    for (std::size_t k = 0; k < galaxy.freeSlots.size(); ++k) {
        Particle& p = particles[galaxy.freeSlots[k]];
        if (!p.active && p.deadTime >= RESPAWN_DELAY) respawnAsDust(p, config);
    }
}

// Checksum del estado para comprobar que las tres versiones dan lo mismo.
static double computeChecksum(const std::vector<Particle>& particles) {
    double checksum = 0.0;
    for (std::size_t i = 0; i < particles.size(); ++i) {
        const Particle& p = particles[i];
        if (p.active) checksum += p.x + 3.0 * p.y + 7.0 * p.radius;
        else          checksum += 1.0;
    }
    return checksum;
}

// ---- Renderizado por software ----

// Pinta un disco relleno, solo en las filas [rowBegin, rowEnd).
static KERNEL_INLINE void fillDiscRows(std::vector<Uint32>& pixels, int width, float centerXf, float centerYf,
                                float radiusf, Uint32 color, int rowBegin, int rowEnd) {
    const int centerX = static_cast<int>(centerXf);
    const int centerY = static_cast<int>(centerYf);
    int radius = static_cast<int>(radiusf + 0.5f);
    if (radius < 1) radius = 1;

    int yStart = centerY - radius;
    int yEnd   = centerY + radius + 1;
    if (yEnd <= rowBegin || yStart >= rowEnd) return;
    if (yStart < rowBegin) yStart = rowBegin;
    if (yEnd > rowEnd) yEnd = rowEnd;

    const int radiusSquared = radius * radius;
    for (int y = yStart; y < yEnd; ++y) {
        const int dy = y - centerY;
        const int halfWidth = static_cast<int>(std::sqrt(static_cast<float>(radiusSquared - dy * dy)));
        int xStart = centerX - halfWidth;
        int xEnd   = centerX + halfWidth + 1;
        if (xStart < 0) xStart = 0;
        if (xEnd > width) xEnd = width;
        Uint32* row = &pixels[static_cast<std::size_t>(y) * width];
        for (int x = xStart; x < xEnd; ++x) row[x] = color;
    }
}

// Dibuja las filas [rowBegin, rowEnd); rangos distintos se pueden dibujar en paralelo.
static void rasterizeRows(Galaxy& galaxy, const Config& config, int rowBegin, int rowEnd) {
    std::vector<Uint32>& pixels = galaxy.framebuffer;
    const int width = config.width;
    if (rowBegin < 0) rowBegin = 0;
    if (rowEnd > config.height) rowEnd = config.height;

    const Uint32 background = packColor(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    for (int y = rowBegin; y < rowEnd; ++y) {
        Uint32* row = &pixels[static_cast<std::size_t>(y) * width];
        for (int x = 0; x < width; ++x) row[x] = background;
    }
    for (std::size_t s = 0; s < galaxy.backgroundStars.size(); ++s) {
        const BackgroundStar& star = galaxy.backgroundStars[s];
        if (star.y >= rowBegin && star.y < rowEnd) {
            pixels[static_cast<std::size_t>(star.y) * width + star.x] = star.color;
        }
    }

    const Uint32 white = packColor(255, 255, 255);
    const std::size_t n = galaxy.particles.size();
    for (std::size_t k = 0; k < n; ++k) {
        const Particle& p = galaxy.particles[k];
        if (!p.active) continue;
        // Salta las particulas que no tocan estas filas (el halo mide 1.6*r).
        const float reach = p.radius * 1.6f + 1.0f;
        if (p.y + reach < rowBegin || p.y - reach >= rowEnd) continue;

        if (p.radius >= GRAVITY_MIN_RADIUS) {
            // Halo: color de la estrella oscurecido (campo gravitacional).
            const Uint32 halo = lerpColor(p.color, background, 0.75f);
            fillDiscRows(pixels, width, p.x, p.y, p.radius * 1.6f, halo, rowBegin, rowEnd);
            fillDiscRows(pixels, width, p.x, p.y, p.radius, p.color, rowBegin, rowEnd);
            // Nucleo: se blanquea conforme la estrella envejece.
            const float lifeFraction = (p.radius >= STAR_RADIUS) ? p.age / p.lifetime : 0.0f;
            const Uint32 core = lerpColor(lerpColor(p.color, white, 0.5f), white, lifeFraction);
            fillDiscRows(pixels, width, p.x, p.y, p.radius * 0.5f, core, rowBegin, rowEnd);
        } else {
            fillDiscRows(pixels, width, p.x, p.y, p.radius, p.color, rowBegin, rowEnd);
        }
    }
}

// Sube el framebuffer y lo muestra; SDL solo se usa desde el hilo principal.
static void presentFramebuffer(SdlContext& sdl, const std::vector<Uint32>& pixels, int width) {
    SDL_UpdateTexture(sdl.texture, nullptr, pixels.data(), width * static_cast<int>(sizeof(Uint32)));
    SDL_RenderClear(sdl.renderer);
    SDL_RenderCopy(sdl.renderer, sdl.texture, nullptr, nullptr);
    SDL_RenderPresent(sdl.renderer);
}

// ---- SDL: inicializacion, eventos y liberacion de recursos ----

// Libera SDL en orden inverso; aguanta punteros nulos.
static void destroySdl(SdlContext& sdl) {
    if (sdl.texture)  { SDL_DestroyTexture(sdl.texture);   sdl.texture = nullptr; }
    if (sdl.renderer) { SDL_DestroyRenderer(sdl.renderer); sdl.renderer = nullptr; }
    if (sdl.window)   { SDL_DestroyWindow(sdl.window);     sdl.window = nullptr; }
    SDL_Quit();
}

// Crea ventana, renderer y textura; si algo falla libera lo que ya se creo.
static bool initSdl(const Config& config, const char* title, SdlContext& sdl) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        return false;
    }
    sdl.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  config.width, config.height, SDL_WINDOW_SHOWN);
    if (sdl.window == nullptr) {
        std::fprintf(stderr, "Error al crear la ventana: %s\n", SDL_GetError());
        destroySdl(sdl);
        return false;
    }
    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
    if (config.vsync) rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    sdl.renderer = SDL_CreateRenderer(sdl.window, -1, rendererFlags);
    if (sdl.renderer == nullptr) {
        // Equipos sin aceleracion (VMs, escritorio remoto): renderer por software.
        sdl.renderer = SDL_CreateRenderer(sdl.window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (sdl.renderer == nullptr) {
        std::fprintf(stderr, "Error al crear el renderer: %s\n", SDL_GetError());
        destroySdl(sdl);
        return false;
    }
    sdl.texture = SDL_CreateTexture(sdl.renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, config.width, config.height);
    if (sdl.texture == nullptr) {
        std::fprintf(stderr, "Error al crear la textura: %s\n", SDL_GetError());
        destroySdl(sdl);
        return false;
    }
    return true;
}

// Devuelve false si se cerro la ventana o se presiono ESC.
static bool handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return false;
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) return false;
    }
    return true;
}

// ---- FPS y reporte de resultados ----

// Calcula el FPS cada medio segundo y lo muestra en el titulo y la consola.
struct FpsCounter {
    double elapsed = 0.0;   // Segundos acumulados desde la ultima actualizacion.
    int    frames  = 0;     // Frames desde la ultima actualizacion.
    double lastFps = 0.0;   // Ultimo FPS calculado.

    void tick(double frameSeconds, SDL_Window* window, const char* versionName,
              const Config& config, bool showThreads, bool printToConsole) {
        ++frames;
        elapsed += frameSeconds;
        if (elapsed < FPS_UPDATE_INTERVAL) return;

        lastFps = frames / elapsed;
        char title[192];
        if (showThreads) {
            std::snprintf(title, sizeof(title), "Galaxia | %s | N=%d | Hilos=%d | FPS: %.1f",
                          versionName, config.particleCount, config.threadCount, lastFps);
        } else {
            std::snprintf(title, sizeof(title), "Galaxia | %s | N=%d | FPS: %.1f",
                          versionName, config.particleCount, lastFps);
        }
        SDL_SetWindowTitle(window, title);
        if (printToConsole) std::printf("FPS= %.2f\n", lastFps);
        elapsed = 0.0;
        frames = 0;
    }
};

// Imprime el resumen del benchmark y agrega una fila al CSV si se pidio.
static void reportBenchmark(const char* versionName, const Config& config, int threads,
                            double totalSeconds, const FrameTimings& timings, double checksum) {
    const double frames = (timings.frames > 0) ? timings.frames : 1;
    const double avgFps = timings.frames / totalSeconds;
    const double updateMs = 1000.0 * timings.updateSeconds / frames;
    const double renderMs = 1000.0 * timings.renderSeconds / frames;

    std::printf("[%s] N=%d hilos=%d frames=%d tiempo=%.4f s FPS=%.2f "
                "update=%.3f ms/frame render=%.3f ms/frame checksum=%.4f\n",
                versionName, config.particleCount, threads, timings.frames, totalSeconds,
                avgFps, updateMs, renderMs, checksum);

    if (config.csvPath.empty()) return;
    FILE* csv = std::fopen(config.csvPath.c_str(), "a");
    if (csv == nullptr) {
        std::fprintf(stderr, "Error: no se pudo abrir '%s' para escribir: %s\n",
                     config.csvPath.c_str(), std::strerror(errno));
        return;
    }
    // Encabezado solo si el archivo esta vacio.
    std::fseek(csv, 0, SEEK_END);
    if (std::ftell(csv) == 0) {
        std::fprintf(csv, "version,N,hilos,frames,tiempo_total_s,fps_promedio,"
                          "update_ms_por_frame,render_ms_por_frame,checksum\n");
    }
    std::fprintf(csv, "%s,%d,%d,%d,%.6f,%.3f,%.4f,%.4f,%.4f\n", versionName, config.particleCount,
                 threads, timings.frames, totalSeconds, avgFps, updateMs, renderMs, checksum);
    std::fclose(csv);
}

// ---- Ciclo principal (identico para las tres versiones) ----

// Ciclo principal comun; cada version solo aporta su update y su render.
template <typename UpdateFn, typename RenderFn>
static int runScreensaver(const Config& config, const char* versionName, bool showThreads,
                          UpdateFn updateStep, RenderFn renderStep) {
    char title[128];
    std::snprintf(title, sizeof(title), "Galaxia | %s", versionName);
    SdlContext sdl;
    if (!initSdl(config, title, sdl)) return EXIT_FAILURE;

    Galaxy galaxy;
    initializeGalaxy(galaxy, config);

    const bool benchmark = config.benchFrames > 0;
    std::printf("Galaxia %s: N=%d%s, canvas %dx%d, semilla=%u, %s\n", versionName,
                config.particleCount, showThreads ? "" : " (1 hilo)", config.width, config.height,
                config.seed, benchmark ? "modo benchmark" : "ESC o cerrar ventana para salir");
    if (showThreads) std::printf("Hilos OpenMP: %d\n", config.threadCount);

    FpsCounter fpsCounter;
    FrameTimings timings;
    bool running = true;
    const double startTime = nowSeconds();
    double previousTime = startTime;

    while (running) {
        const double frameStart = nowSeconds();
        const double realDelta = frameStart - previousTime;
        previousTime = frameStart;

        running = handleEvents();

        // dt fijo en benchmark y dt real en modo interactivo.
        float deltaTime = benchmark ? BENCH_FIXED_DT : static_cast<float>(realDelta);
        if (deltaTime > MAX_INTERACTIVE_DT) deltaTime = MAX_INTERACTIVE_DT;

        const double updateStart = nowSeconds();
        updateStep(galaxy, deltaTime);
        const double renderStart = nowSeconds();
        renderStep(galaxy);
        presentFramebuffer(sdl, galaxy.framebuffer, config.width);
        const double frameEnd = nowSeconds();

        timings.updateSeconds += renderStart - updateStart;
        timings.renderSeconds += frameEnd - renderStart;
        timings.frames++;

        fpsCounter.tick(realDelta, sdl.window, versionName, config, showThreads, !benchmark);
        if (benchmark && timings.frames >= config.benchFrames) running = false;
    }

    const double totalSeconds = nowSeconds() - startTime;
    if (benchmark) {
        reportBenchmark(versionName, config, showThreads ? config.threadCount : 1,
                        totalSeconds, timings, computeChecksum(galaxy.particles));
    }
    destroySdl(sdl);
    return EXIT_SUCCESS;
}

#endif  // SCREENSAVER_COMMON_HPP
