// parallel.cpp
// Minecraft Blocks Screensaver - Parallel version (OpenMP).
// Misma arquitectura y comportamiento que sequential.cpp; unicamente la
// actualizacion de fisica de los N bloques se distribuye entre hilos.
//
// Uso:
//   screensaver_parallel.exe <N> <threads>

#include <SDL.h>
#include <omp.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <vector>

static const int WINDOW_WIDTH  = 800;
static const int WINDOW_HEIGHT = 600;

static const int BLOCK_MIN_SIZE = 1;
static const int BLOCK_MAX_SIZE = 50;

static const float BLOCK_MIN_SPEED = 60.0f;
static const float BLOCK_MAX_SPEED = 180.0f;

// --- Mecanica de "depredacion" (igual que sequential.cpp) ---
// Un bloque se come a otro con el que se solapa si su lado lo supera por al
// menos BLOCK_EAT_MARGIN px; al comer, su area aumenta en una fraccion
// BLOCK_EAT_GROWTH del area de la presa, hasta un lado maximo BLOCK_EAT_MAX_SIZE.
static const int   BLOCK_EAT_MARGIN   = 2;
static const float BLOCK_EAT_GROWTH   = 0.6f;
static const int   BLOCK_EAT_MAX_SIZE = 160;


static const double FPS_UPDATE_INTERVAL = 0.5;

// M_PI no es estandar en C++; se define manualmente como en sequential.cpp.
static const float PI = 3.14159265358979323846f;

struct Block {
    float x, y;
    float vx, vy;
    int   size;
    Uint8 r, g, b;
};

struct PaletteColor { Uint8 r, g, b; };
static const PaletteColor BLOCK_PALETTE[] = {
    {121, 85, 58},    // tierra
    {90, 163, 61},    // pasto
    {128, 128, 128},  // piedra
    {156, 127, 78},   // madera
    {223, 209, 148},  // arena
    {66, 66, 66},     // carbón/piedra oscura
    {200, 60, 60},    // "redstone"
};
static const int PALETTE_SIZE = sizeof(BLOCK_PALETTE) / sizeof(BLOCK_PALETTE[0]);

static float randomRange(float minValue, float maxValue) {
    float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return minValue + t * (maxValue - minValue);
}

// Valida y convierte N y threads. Ambos deben ser enteros positivos; N ademas
// se limita a un rango razonable, igual que en sequential.cpp.
static bool parseArguments(int argc, char* argv[], int& outBlockCount, int& outThreadCount) {
    const char* usage = "Uso: screensaver_parallel.exe <N> <threads>\n";

    if (argc != 3) {
        std::fprintf(stderr, "Error: numero de argumentos invalido.\n%s", usage);
        return false;
    }

    errno = 0;
    char* endPtrN = nullptr;
    long parsedN = std::strtol(argv[1], &endPtrN, 10);
    if (endPtrN == argv[1] || *endPtrN != '\0' || errno == ERANGE) {
        std::fprintf(stderr, "Error: '%s' no es un entero valido para N.\n%s", argv[1], usage);
        return false;
    }
    if (parsedN <= 0 || parsedN > 1000000) {
        std::fprintf(stderr, "Error: N debe ser mayor que 0 (y razonable). Valor recibido: %ld\n%s",
                     parsedN, usage);
        return false;
    }

    errno = 0;
    char* endPtrThreads = nullptr;
    long parsedThreads = std::strtol(argv[2], &endPtrThreads, 10);
    if (endPtrThreads == argv[2] || *endPtrThreads != '\0' || errno == ERANGE) {
        std::fprintf(stderr, "Error: '%s' no es un entero valido para threads.\n%s", argv[2], usage);
        return false;
    }
    if (parsedThreads <= 0 || parsedThreads > 1024) {
        std::fprintf(stderr, "Error: threads debe ser mayor que 0 (y razonable). Valor recibido: %ld\n%s",
                     parsedThreads, usage);
        return false;
    }

    outBlockCount = static_cast<int>(parsedN);
    outThreadCount = static_cast<int>(parsedThreads);
    return true;
}

// Construye un bloque pseudoaleatorio dentro del canvas. Se usa al inicializar
// y al "reciclar" un bloque comido, de modo que N permanece constante.
static Block makeRandomBlock() {
    Block block;
    block.size = static_cast<int>(randomRange(static_cast<float>(BLOCK_MIN_SIZE),
                                                static_cast<float>(BLOCK_MAX_SIZE)));

    block.x = randomRange(0.0f, static_cast<float>(WINDOW_WIDTH - block.size));
    block.y = randomRange(0.0f, static_cast<float>(WINDOW_HEIGHT - block.size));

    float speed = randomRange(BLOCK_MIN_SPEED, BLOCK_MAX_SPEED);
    float angle = randomRange(0.0f, 2.0f * PI);
    block.vx = speed * std::cos(angle);
    block.vy = speed * std::sin(angle);

    const PaletteColor& base = BLOCK_PALETTE[rand() % PALETTE_SIZE];
    auto jitterChannel = [](int channel) {
        int jitter = (rand() % 41) - 20;
        int result = channel + jitter;
        if (result < 0) result = 0;
        if (result > 255) result = 255;
        return static_cast<Uint8>(result);
    };
    block.r = jitterChannel(base.r);
    block.g = jitterChannel(base.g);
    block.b = jitterChannel(base.b);

    return block;
}

static std::vector<Block> initializeBlocks(int blockCount) {
    std::vector<Block> blocks;
    blocks.reserve(blockCount);
    for (int i = 0; i < blockCount; ++i) {
        blocks.push_back(makeRandomBlock());
    }
    return blocks;
}

// Fisica de un unico bloque. Identica a sequential.cpp: solo lee/escribe el
// bloque que recibe por referencia, por lo que es segura para ejecutarse en
// paralelo mientras cada hilo trabaje sobre un indice distinto.
static void updateBlockPhysics(Block& block, float deltaTimeSeconds) {
    block.x += block.vx * deltaTimeSeconds;
    block.y += block.vy * deltaTimeSeconds;

    if (block.x < 0.0f) {
        block.x = 0.0f;
        block.vx = -block.vx;
    } else if (block.x + block.size > WINDOW_WIDTH) {
        block.x = static_cast<float>(WINDOW_WIDTH - block.size);
        block.vx = -block.vx;
    }

    if (block.y < 0.0f) {
        block.y = 0.0f;
        block.vy = -block.vy;
    } else if (block.y + block.size > WINDOW_HEIGHT) {
        block.y = static_cast<float>(WINDOW_HEIGHT - block.size);
        block.vy = -block.vy;
    }
}

// ¿Se solapan las bolas a y b? Distancia entre centros < suma de radios.
// 'size' es el diametro; x/y siguen siendo la esquina de la caja contenedora,
// asi que el centro es (x + size/2, y + size/2).
static bool blocksOverlap(const Block& a, const Block& b) {
    float ar = a.size * 0.5f;
    float br = b.size * 0.5f;
    float dx = (a.x + ar) - (b.x + br);
    float dy = (a.y + ar) - (b.y + br);
    float radiusSum = ar + br;
    return dx * dx + dy * dy < radiusSum * radiusSum;
}

// Para cada bloque j calcula que bloque se lo come: el de mayor lado entre los
// que lo superan por al menos BLOCK_EAT_MARGIN px y se solapan con el (-1 si
// ninguno). Cada iteracion 'j' solo LEE el arreglo 'blocks' (inmutable en esta
// fase) y ESCRIBE predatorOf[j], una posicion exclusiva de la iteracion: no
// hay dependencias ni escrituras compartidas, asi que se reparte entre hilos
// sin 'critical' ni locks. El coste por 'j' es ~constante (recorre los N
// bloques), por lo que schedule(static) equilibra bien la carga. Coste O(N^2):
// es la fase mas pesada y la que mas se beneficia de los hilos.
static void computePredators(const std::vector<Block>& blocks, std::vector<int>& predatorOf,
                             int threadCount) {
    const int n = static_cast<int>(blocks.size());
    #pragma omp parallel for num_threads(threadCount) schedule(static)
    for (int j = 0; j < n; ++j) {
        int predator = -1;
        int predatorSize = blocks[j].size + BLOCK_EAT_MARGIN - 1;
        for (int i = 0; i < n; ++i) {
            if (i == j) continue;
            if (blocks[i].size > predatorSize && blocksOverlap(blocks[i], blocks[j])) {
                predator = i;
                predatorSize = blocks[i].size;
            }
        }
        predatorOf[j] = predator;
    }
}

// Aplica el resultado de computePredators: cada depredador crece segun el area
// de sus presas y cada presa se recicla en un bloque nuevo pequeno (N constante).
// Es O(N) y se mantiene SECUENCIAL: varias presas pueden acumular crecimiento
// sobre el mismo depredador (dependencia de lectura-escritura sobre blocks[i])
// y el reciclado debe leer el tamano de la presa antes de sobrescribirla.
static void resolveEating(std::vector<Block>& blocks, const std::vector<int>& predatorOf) {
    const int n = static_cast<int>(blocks.size());

    for (int j = 0; j < n; ++j) {
        const int i = predatorOf[j];
        if (i < 0) continue;

        float preyArea = static_cast<float>(blocks[j].size) * blocks[j].size;
        float predArea = static_cast<float>(blocks[i].size) * blocks[i].size;
        int newSize = static_cast<int>(std::sqrt(predArea + BLOCK_EAT_GROWTH * preyArea));
        if (newSize > BLOCK_EAT_MAX_SIZE) newSize = BLOCK_EAT_MAX_SIZE;
        blocks[i].size = newSize;

        if (blocks[i].x + blocks[i].size > WINDOW_WIDTH)
            blocks[i].x = static_cast<float>(WINDOW_WIDTH - blocks[i].size);
        if (blocks[i].y + blocks[i].size > WINDOW_HEIGHT)
            blocks[i].y = static_cast<float>(WINDOW_HEIGHT - blocks[i].size);
        if (blocks[i].x < 0.0f) blocks[i].x = 0.0f;
        if (blocks[i].y < 0.0f) blocks[i].y = 0.0f;
    }

    for (int j = 0; j < n; ++j) {
        if (predatorOf[j] >= 0) blocks[j] = makeRandomBlock();
    }
}

// Paralelizacion con OpenMP: cada iteracion 'i' solo lee y escribe blocks[i],
// sin dependencias entre iteraciones ni memoria compartida mutable entre
// ellas, por lo que se reparten los indices entre hilos sin necesidad de
// 'critical' ni locks. 'blocks' se comparte (shared, es el default para
// variables externas al bucle) porque cada hilo accede a una porcion
// disjunta del vector; 'deltaTimeSeconds' tambien es shared pero solo se lee.
// El indice 'i' es privado automaticamente por ser la variable de control
// del for. schedule(static) reparte bloques contiguos de indices entre
// hilos, adecuado porque cada bloque cuesta aproximadamente lo mismo.
//
// Tras la fisica se ejecuta la fase de "comer": computePredators (paralela,
// O(N^2)) y resolveEating (secuencial, O(N)).
static void updateAllBlocks(std::vector<Block>& blocks, float deltaTimeSeconds, int threadCount) {
    const int blockCount = static_cast<int>(blocks.size());
    #pragma omp parallel for num_threads(threadCount) schedule(static)
    for (int i = 0; i < blockCount; ++i) {
        updateBlockPhysics(blocks[i], deltaTimeSeconds);
    }

    std::vector<int> predatorOf(blocks.size(), -1);
    computePredators(blocks, predatorOf, threadCount);
    resolveEating(blocks, predatorOf);
}

static bool handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            return false;
        }
    }
    return true;
}

// Renderizado secuencial: SDL_Renderer no es thread-safe, por lo que esta
// funcion se mantiene fuera de cualquier region paralela y se llama desde
// el hilo principal como en sequential.cpp.
// Dibuja un circulo relleno por barrido de lineas horizontales: para cada fila
// 'dy' dentro del radio, el ancho es sqrt(r^2 - dy^2). SDL2 no trae primitiva
// de circulo relleno, asi que se hace a mano.
static void renderFilledCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = static_cast<int>(std::sqrt(static_cast<double>(radius) * radius - dy * dy));
        SDL_RenderDrawLine(renderer, centerX - dx, centerY + dy, centerX + dx, centerY + dy);
    }
}

static void renderFrame(SDL_Renderer* renderer, const std::vector<Block>& blocks) {
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_RenderClear(renderer);

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const Block& block = blocks[i];
        int radius = block.size / 2;
        int centerX = static_cast<int>(block.x) + radius;
        int centerY = static_cast<int>(block.y) + radius;

        SDL_SetRenderDrawColor(renderer, block.r, block.g, block.b, 255);
        renderFilledCircle(renderer, centerX, centerY, radius);
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    int blockCount = 0;
    int threadCount = 0;
    if (!parseArguments(argc, argv, blockCount, threadCount)) {
        return EXIT_FAILURE;
    }

    // Fija el numero de hilos usados por las regiones "parallel for" de este
    // programa; se pasa ademas explicitamente via num_threads() en el bucle
    // de fisica para que quede documentado en el propio sitio de uso.
    omp_set_num_threads(threadCount);

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Minecraft Blocks Screensaver (Parallel)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN);

    if (window == nullptr) {
        std::fprintf(stderr, "Error al crear la ventana: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (renderer == nullptr) {
        std::fprintf(stderr, "Error al crear el renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::vector<Block> blocks = initializeBlocks(blockCount);

    bool running = true;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const Uint64 perfFrequency = SDL_GetPerformanceFrequency();

    double fpsTimer = 0.0;
    int framesSinceLastUpdate = 0;

    while (running) {
        Uint64 currentCounter = SDL_GetPerformanceCounter();
        float deltaTimeSeconds = static_cast<float>(currentCounter - previousCounter) /
                                  static_cast<float>(perfFrequency);
        previousCounter = currentCounter;

        running = handleEvents();

        updateAllBlocks(blocks, deltaTimeSeconds, threadCount);

        renderFrame(renderer, blocks);

        framesSinceLastUpdate++;
        fpsTimer += deltaTimeSeconds;
        if (fpsTimer >= FPS_UPDATE_INTERVAL) {
            double fps = framesSinceLastUpdate / fpsTimer;
            char titleBuffer[160];
            std::snprintf(titleBuffer, sizeof(titleBuffer),
                          "Minecraft Blocks Screensaver | Parallel | N=%d | Threads=%d | FPS: %.1f",
                          blockCount, threadCount, fps);
            SDL_SetWindowTitle(window, titleBuffer);
            fpsTimer = 0.0;
            framesSinceLastUpdate = 0;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}


// NOTAS:
// Se comprueba con:
// g++ -std=c++17 -O2 -Wall -fopenmp -IC:\msys64\ucrt64\include\SDL2 src\parallel.cpp -o bin\screensaver_parallel.exe -LC:\msys64\ucrt64\lib -lmingw32 -lSDL2main -lSDL2

// Se ejecuta cambiando la cantidad de Threads con:
// .\bin\screensaver_parallel.exe 100 1
// .\bin\screensaver_parallel.exe 100 2
// .\bin\screensaver_parallel.exe 100 4