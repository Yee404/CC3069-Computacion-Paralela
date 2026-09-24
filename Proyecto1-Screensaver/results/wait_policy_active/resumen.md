Equipo: 11th Gen Intel(R) Core(TM) i7-11800H @ 2.30GHz - 16 hilos lógicos - Linux 6.8.0-45-generic

Cada fila = promedio de 10 corridas de 120 frames (dt fijo 1/60 s, sin vsync, semilla 2026, OMP_WAIT_POLICY=active).

Checksums idénticos entre versiones: **sí**

| Versión | N | Hilos | Tiempo prom. (s) | Desv. (s) | FPS prom. | Update (ms) | Render (ms) | Speedup | Eficiencia |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Secuencial | 5000 | 1 | 1.4096 | 0.0755 | 85.4 | 11.286 | 0.451 | 1.00 | 100.0% |
| Paralela v1 | 5000 | 8 | 0.3501 | 0.0270 | 344.7 | 2.377 | 0.531 | 4.03 | 50.3% |
| Paralela v1 | 5000 | 16 | 0.4019 | 0.0277 | 299.9 | 2.654 | 0.680 | 3.51 | 21.9% |
| Paralela v2 | 5000 | 8 | 0.2822 | 0.0092 | 425.6 | 1.954 | 0.388 | 4.99 | 62.4% |
| Paralela v2 | 5000 | 16 | 0.3433 | 0.0378 | 352.9 | 2.250 | 0.597 | 4.11 | 25.7% |
