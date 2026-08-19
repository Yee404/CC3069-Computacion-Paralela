// sequential.cpp
// Minecraft Blocks Screensaver - Sequential (baseline) version.
// Compila con g++ (MSYS2 UCRT64) + SDL2. No usa OpenMP ni threads.
//
// Uso:
//   screensaver_sequential.exe <N>
//
// N = cantidad de bloques a renderizar (entero positivo).

#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <vector>

// Tamaño del canvas. La asignación pide un mínimo de 640x480; usamos 800x600.
static const int WINDOW_WIDTH  = 800;
static const int WINDOW_HEIGHT = 600;

// Rango de tamaño de cada bloque (en píxeles), para que se vean como "bloques".
static const int BLOCK_MIN_SIZE = 20;
static const int BLOCK_MAX_SIZE = 45;

// Rango de rapidez (píxeles por segundo) usado al inicializar cada bloque.
static const float BLOCK_MIN_SPEED = 60.0f;
static const float BLOCK_MAX_SPEED = 180.0f;

// Cada cuánto tiempo (segundos) se actualiza el título de la ventana con el FPS.
static const double FPS_UPDATE_INTERVAL = 0.5;

// Reemplazo del M_PI pq no trae la extensión
static const float PI = 3.14159265358979323846f;


// Representa un bloque individual del screensaver.
// La velocidad se guarda como componentes vx/vy (derivadas de rapidez + ángulo
// mediante trigonometría) para que el rebote sea una simple reflexión de signo.
struct Block {
    float x, y;       // Posición de la esquina superior izquierda.
    float vx, vy;      // Velocidad en píxeles/segundo.
    int   size;        // Lado del cuadrado en píxeles.
    Uint8 r, g, b;      // Color RGB pseudoaleatorio.
};

// Colores inspirados en bloques de Minecraft (tierra, pasto, piedra, madera, etc.)
// Se usan como paleta base y luego se aplica una variación aleatoria leve.
struct PaletteColor { Uint8 r, g, b; };
static const PaletteColor BLOCK_PALETTE[] = {
    {121, 85, 58},   // tierra
    {90, 163, 61},    // pasto
    {128, 128, 128},  // piedra
    {156, 127, 78},   // madera
    {223, 209, 148},  // arena
    {66, 66, 66},     // carbón/piedra oscura
    {200, 60, 60},    // "redstone"
};
static const int PALETTE_SIZE = sizeof(BLOCK_PALETTE) / sizeof(BLOCK_PALETTE[0]);

// Devuelve un flotante pseudoaleatorio en [minValue, maxValue].
static float randomRange(float minValue, float maxValue) {
    float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return minValue + t * (maxValue - minValue);
}

// Valida argv y convierte N de forma segura. Retorna false ante cualquier
// argumento inválido, dejando un mensaje de uso claro en stderr.
static bool parseBlockCount(int argc, char* argv[], int& outBlockCount) {
    const char* usage = "Uso: screensaver_sequential.exe <N>\n";

    if (argc != 2) {
        std::fprintf(stderr, "Error: numero de argumentos invalido.\n%s", usage);
        return false;
    }

    errno = 0;
    char* endPtr = nullptr;
    long parsedValue = std::strtol(argv[1], &endPtr, 10);

    // endPtr debe apuntar al final del string para que sea un entero puro,
    // y strtol no debe haber señalado overflow/underflow (errno == ERANGE).
    if (endPtr == argv[1] || *endPtr != '\0' || errno == ERANGE) {
        std::fprintf(stderr, "Error: '%s' no es un entero valido.\n%s", argv[1], usage);
        return false;
    }

    if (parsedValue <= 0 || parsedValue > 1000000) {
        std::fprintf(stderr, "Error: N debe ser mayor que 0 (y razonable). Valor recibido: %ld\n%s",
                     parsedValue, usage);
        return false;
    }

    outBlockCount = static_cast<int>(parsedValue);
    return true;
}

// Crea e inicializa N bloques con posición, velocidad y color pseudoaleatorios.
// Las posiciones se generan de modo que el bloque completo quede dentro del canvas.
static std::vector<Block> initializeBlocks(int blockCount) {
    std::vector<Block> blocks;
    blocks.reserve(blockCount);

    for (int i = 0; i < blockCount; ++i) {
        Block block;
        block.size = static_cast<int>(randomRange(static_cast<float>(BLOCK_MIN_SIZE),
                                                    static_cast<float>(BLOCK_MAX_SIZE)));

        block.x = randomRange(0.0f, static_cast<float>(WINDOW_WIDTH - block.size));
        block.y = randomRange(0.0f, static_cast<float>(WINDOW_HEIGHT - block.size));

        // Dirección de movimiento generada con trigonometría a partir de un
        // ángulo aleatorio; el rebote luego solo invierte el signo apropiado.
        float speed = randomRange(BLOCK_MIN_SPEED, BLOCK_MAX_SPEED);
        float angle = randomRange(0.0f, 2.0f * PI);    // <- se cambió el M_PI por un static const float PI (al inicio del archivo), según entiendo es pq M_PI no forma parte del estándar C++, aunque algunos ocmpiladores lo ponen como extensión, en este caso no viene
        block.vx = speed * std::cos(angle);
        block.vy = speed * std::sin(angle);

        const PaletteColor& base = BLOCK_PALETTE[rand() % PALETTE_SIZE];
        // Pequeña variación de color para que no todos los bloques del mismo
        // tipo se vean idénticos.
        auto jitterChannel = [](int channel) {
            int jitter = (rand() % 41) - 20; // [-20, 20]
            int result = channel + jitter;
            if (result < 0) result = 0;
            if (result > 255) result = 255;
            return static_cast<Uint8>(result);
        };
        block.r = jitterChannel(base.r);
        block.g = jitterChannel(base.g);
        block.b = jitterChannel(base.b);

        blocks.push_back(block);
    }

    return blocks;
}

// Actualiza la física (posición y rebotes) de un único bloque.
// Aislada en su propia función para que, en la version paralela, cada
// iteración del bucle sobre bloques pueda repartirse entre hilos sin tocar
// esta lógica.
static void updateBlockPhysics(Block& block, float deltaTimeSeconds) {
    block.x += block.vx * deltaTimeSeconds;
    block.y += block.vy * deltaTimeSeconds;

    // Rebote contra los bordes izquierdo/derecho.
    if (block.x < 0.0f) {
        block.x = 0.0f;
        block.vx = -block.vx;
    } else if (block.x + block.size > WINDOW_WIDTH) {
        block.x = static_cast<float>(WINDOW_WIDTH - block.size);
        block.vx = -block.vx;
    }

    // Rebote contra los bordes superior/inferior.
    if (block.y < 0.0f) {
        block.y = 0.0f;
        block.vy = -block.vy;
    } else if (block.y + block.size > WINDOW_HEIGHT) {
        block.y = static_cast<float>(WINDOW_HEIGHT - block.size);
        block.vy = -block.vy;
    }
}

// Recorre todos los bloques y actualiza su física. Este es el punto exacto
// que la version paralela reemplazara por un bucle con OpenMP.
static void updateAllBlocks(std::vector<Block>& blocks, float deltaTimeSeconds) {
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        updateBlockPhysics(blocks[i], deltaTimeSeconds);
    }
}

// Procesa eventos de SDL (cierre de ventana, tecla ESC). Retorna false si el
// programa debe terminar.
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

// Dibuja el fondo oscuro y todos los bloques en su posición actual.
static void renderFrame(SDL_Renderer* renderer, const std::vector<Block>& blocks) {
    // Fondo oscuro (casi negro, con un leve tinte azulado tipo "cueva").
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_RenderClear(renderer);

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const Block& block = blocks[i];
        SDL_Rect rect;
        rect.x = static_cast<int>(block.x);
        rect.y = static_cast<int>(block.y);
        rect.w = block.size;
        rect.h = block.size;

        SDL_SetRenderDrawColor(renderer, block.r, block.g, block.b, 255);
        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    int blockCount = 0;
    if (!parseBlockCount(argc, argv, blockCount)) {
        return EXIT_FAILURE;
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Minecraft Blocks Screensaver (Sequential)",
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

    // Acumuladores para calcular y mostrar FPS periodicamente en el titulo.
    double fpsTimer = 0.0;
    int framesSinceLastUpdate = 0;

    while (running) {
        Uint64 currentCounter = SDL_GetPerformanceCounter();
        float deltaTimeSeconds = static_cast<float>(currentCounter - previousCounter) /
                                  static_cast<float>(perfFrequency);
        previousCounter = currentCounter;

        // Manejo de eventos (entrada de usuario / cierre de ventana).
        running = handleEvents();

        // Actualizacion de fisica (candidato a paralelizar con OpenMP).
        updateAllBlocks(blocks, deltaTimeSeconds);

        // Renderizado (se mantiene secuencial: SDL_Renderer no es thread-safe).
        renderFrame(renderer, blocks);

        // Calculo y despliegue de FPS en el titulo de la ventana.
        framesSinceLastUpdate++;
        fpsTimer += deltaTimeSeconds;
        if (fpsTimer >= FPS_UPDATE_INTERVAL) {
            double fps = framesSinceLastUpdate / fpsTimer;
            char titleBuffer[128];
            std::snprintf(titleBuffer, sizeof(titleBuffer),
                          "Minecraft Blocks Screensaver | Sequential | N=%d | FPS: %.1f",
                          blockCount, fps);
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



// Notas:
// error de -Dmain=main en:
// g++ -std=c++17 -O2 -Wall -IC:\msys64\ucrt64\include\SDL2 -Dmain=main src\sequential.cpp -o bin\screensaver_sequential.exe -LC:\msys64\ucrt64\lib -lmingw32 -lSDL2main -lSDL2
// pq el sdl2 ya maneja la entrada con SDL_main.h y se define en → "int main(int argc, char* argv[])" 
// Corrección: g++ -std=c++17 -O2 -Wall -IC:\msys64\ucrt64\include\SDL2 src\sequential.cpp -o bin\screensaver_sequential.exe -LC:\msys64\ucrt64\lib -lmingw32 -lSDL2main -lSDL2

// EJECUTAR
// Se ejecuta con:
// PS C:\Users\mjyee\Downloads\CC3069-Computacion-Paralela\Proyecto1-Screensaver> g++ -std=c++17 -O2 -Wall -IC:\msys64\ucrt64\include\SDL2 src\sequential.cpp -o bin\screensaver_sequential.exe -LC:\msys64\ucrt64\lib -lmingw32 -lSDL2main -lSDL2 PS C:\Users\mjyee\Downloads\CC3069-Computacion-Paralela\Proyecto1-Screensaver>
// y luego se agrega el valor de N con:
//.\bin\screensaver_sequential.exe 100