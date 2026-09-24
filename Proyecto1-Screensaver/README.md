# Proyecto 1 – Screensaver "Galaxia" (OpenMP)

CC3069 Computación Paralela y Distribuida · Universidad del Valle de Guatemala · Semestre 2, 2026

Screensaver en C++17 + SDL2. Simula una galaxia de **N partículas** y se paraleliza con **OpenMP**.

- **Polvo estelar:** partículas pequeñas de colores pseudoaleatorios que nacen en un disco girando alrededor del centro del canvas.
- **Gravedad:** las partículas grandes (radio ≥ 5 px) generan un campo gravitacional proporcional a su masa (`m = r²`). La aceleración sobre cada partícula es `a = G·m / (d² + ε²)` en dirección a la estrella.
- **Absorción:** cuando el centro de una partícula más pequeña queda dentro de una estrella, la estrella la absorbe. Crece en área y conserva el momento: `v = (m₁v₁ + m₂v₂)/(m₁ + m₂)`.
- **Supernova:** una estrella con radio ≥ 12 px empieza a envejecer y su núcleo se vuelve blanco. Al cumplir su tiempo de vida (`--lifetime`) explota en hasta 40 fragmentos pequeños, lanzados radialmente en ángulos `2πk/K`.
- **Rebotes** contra los bordes del canvas (reflexión de la componente de velocidad).
- **N constante:** las partículas absorbidas se reutilizan como fragmentos de supernova o reaparecen como polvo tras 1.5 s.
- **FPS** en el título de la ventana y en consola.

## Estructura

```
Proyecto1-Screensaver/
├── Makefile
├── src/
│   ├── screensaver_common.hpp   # código compartido: args, física, render, SDL, FPS, benchmark
│   ├── sequential.cpp           # versión secuencial (línea base, sin OpenMP)
│   ├── parallel.cpp             # versión paralela v1 (parallel for en física y absorción)
│   └── parallel_v2.cpp          # versión paralela v2 (una región por frame + render paralelo)
├── scripts/benchmark.py         # bitácora de pruebas: 10+ corridas, speedup, eficiencia, gráficas
├── results/                     # mediciones y resumen generados por el script
├── scripts/build_report.py      # genera docs/Informe.pdf (formato de informe UVG)
└── docs/                        # Informe.md (fuente), Informe.pdf, diagrama de flujo y capturas
```

## Compilación

### Linux / macOS

Requisitos: `g++` con soporte OpenMP y SDL2 (`sudo apt install libsdl2-dev`).

```bash
make            # genera bin/screensaver_sequential, bin/screensaver_parallel, bin/screensaver_parallel_v2
make clean
```

### Windows (MSYS2 UCRT64)

```powershell
g++ -std=c++17 -O2 -Wall -IC:\msys64\ucrt64\include\SDL2 src\sequential.cpp  -o bin\screensaver_sequential.exe  -LC:\msys64\ucrt64\lib -lmingw32 -lSDL2main -lSDL2
g++ -std=c++17 -O2 -Wall -fopenmp -IC:\msys64\ucrt64\include\SDL2 src\parallel.cpp    -o bin\screensaver_parallel.exe    -LC:\msys64\ucrt64\lib -lmingw32 -lSDL2main -lSDL2
g++ -std=c++17 -O2 -Wall -fopenmp -IC:\msys64\ucrt64\include\SDL2 src\parallel_v2.cpp -o bin\screensaver_parallel_v2.exe -LC:\msys64\ucrt64\lib -lmingw32 -lSDL2main -lSDL2
```

(No usar `-Dmain=main`: SDL2 redefine `main` a través de `SDL_main.h`.)

## Ejecución

```bash
./bin/screensaver_sequential  <N> [opciones]
./bin/screensaver_parallel    <N> [hilos] [opciones]
./bin/screensaver_parallel_v2 <N> [hilos] [opciones]
```

Si se ejecuta **sin argumentos**, el programa pide N (y los hilos) por consola. `ESC` o cerrar la ventana termina el programa.

| Parámetro | Descripción | Rango | Defecto |
|---|---|---|---|
| `N` | cantidad de partículas (obligatorio) | 1 – 1 000 000 | — |
| `hilos`, `-t/--threads` | hilos OpenMP (solo versiones paralelas) | 1 – 1024 | mitad de los procesadores lógicos |
| `-W/--width`, `-H/--height` | tamaño del canvas | ≥ 640×480 | 800×600 |
| `--min-radius`, `--max-radius` | radio de nacimiento del polvo (px) | 0.5 – 12 | 1 – 4 |
| `--min-speed`, `--max-speed` | rapidez inicial (px/s) | 0 – 2000 | 20 – 90 |
| `-g/--gravity` | constante gravitacional G | 0 – 100 000 | 250 |
| `-l/--lifetime` | vida de una estrella antes de explotar (s) | 0.5 – 600 | 6 |
| `-s/--seed` | semilla pseudoaleatoria | entero ≥ 0 | hora actual |
| `-f/--frames` | **modo benchmark**: simula F frames con dt fijo y sin vsync, imprime tiempos y sale | ≥ 1 | — |
| `--no-vsync` | quita el límite de FPS del monitor | | |
| `--csv archivo` | agrega una fila de resultados al CSV (benchmark) | | |
| `-h/--help` | ayuda | | |

Ejemplos:

```bash
./bin/screensaver_parallel_v2 3000 8                  # interactivo, 3000 partículas, 8 hilos
./bin/screensaver_sequential 2000 --gravity 600 -l 3  # más gravedad, supernovas más frecuentes
./bin/screensaver_parallel 5000 4 --seed 7 --frames 300   # medición
```

Todas las entradas se validan (enteros/reales bien formados, rangos, opciones desconocidas, argumentos de sobra, `min > max`). Ante un error, el programa muestra el motivo y la ayuda y termina con código 1.

## Paralelización (resumen)

| Fase por frame | Costo | Secuencial | Paralela v1 | Paralela v2 |
|---|---|---|---|---|
| Construir arreglo de atractores | O(N) | ✔ | secuencial | `omp single` |
| Gravedad + movimiento + rebotes | O(N·E) | ✔ | `parallel for static` | `omp for static` |
| Búsqueda de depredador (absorción) | O(N²) | ✔ | `parallel for static` | `omp for dynamic,64` |
| Aplicar absorciones + supernovas | O(N) | ✔ | secuencial | `omp single` |
| Rasterizado del framebuffer | O(píxeles) | ✔ | secuencial | `parallel for` por franjas (4 por hilo) |
| Subir textura / presentar (SDL) | — | hilo principal | hilo principal | hilo principal |

- **Sincronización:** barreras implícitas al final de cada `for`/`single`. `single` también sirve de exclusión mutua para las fases que usan `rand()` (no es thread-safe) o modifican partículas arbitrarias.
- **Memoria compartida sin carreras:** en las fases paralelas cada iteración escribe solo su propio índice (`particles[i]`, `predatorOf[j]` o las filas de su franja) y lee datos inmutables durante esa fase (el arreglo `attractors`, la copia de estrellas).
- **Correctitud:** con la misma semilla, las tres versiones producen exactamente el mismo `checksum` final.

## Mediciones (speedup y eficiencia)

```bash
make
python3 scripts/benchmark.py                 # 10 corridas × config. (N = 2000, 5000, 10000; hilos 1..nproc)
python3 scripts/benchmark.py --quick         # prueba rápida
python3 scripts/benchmark.py --sizes 2000 8000 --threads 1 2 4 8 --runs 12
```

El script genera en `results/`: `mediciones.csv` (cada corrida), `resumen.csv` / `resumen.md` (promedios, desviación, FPS, `speedup = T_sec / T_par`, `eficiencia = speedup / hilos`), `speedup.png`, `eficiencia.png`, `fps.png` y `salida_consola.txt`.

## Informe

`docs/Informe.pdf` se genera a partir de `docs/Informe.md` y toma las tablas de `results/`:

```bash
pip install markdown websocket-client      # requiere además Google Chrome o Chromium
python3 scripts/build_report.py
```

El script pagina el documento con Paged.js (incluido en `scripts/vendor/`) y lo imprime con Chrome headless.

## Integrantes

- Sebastian Estrada – 21405
- Maria Jose Yee – 231193
