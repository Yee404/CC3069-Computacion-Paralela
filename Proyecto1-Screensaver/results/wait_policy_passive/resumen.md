Equipo: 11th Gen Intel(R) Core(TM) i7-11800H @ 2.30GHz - 16 hilos lógicos - Linux 6.8.0-45-generic

Cada fila = promedio de 10 corridas de 120 frames (dt fijo 1/60 s, sin vsync, semilla 2026, OMP_WAIT_POLICY=passive).

Checksums idénticos entre versiones: **sí**

| Versión | N | Hilos | Tiempo prom. (s) | Desv. (s) | FPS prom. | Update (ms) | Render (ms) | Speedup | Eficiencia |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Secuencial | 5000 | 1 | 1.3403 | 0.0608 | 89.7 | 10.732 | 0.427 | 1.00 | 100.0% |
| Paralela v1 | 5000 | 8 | 0.3699 | 0.0193 | 325.2 | 2.619 | 0.455 | 3.62 | 45.3% |
| Paralela v1 | 5000 | 16 | 0.2936 | 0.0083 | 409.0 | 1.929 | 0.508 | 4.57 | 28.5% |
| Paralela v2 | 5000 | 8 | 0.2934 | 0.0142 | 409.8 | 2.049 | 0.387 | 4.57 | 57.1% |
| Paralela v2 | 5000 | 16 | 0.2478 | 0.0060 | 484.5 | 1.631 | 0.424 | 5.41 | 33.8% |
