// ProducerConsumerCritical.c

#include <stdio.h>
#include <omp.h>

#define CANTIDAD_PRODUCTOS 5

int main() {
    int producto = 0;
    int hayProducto = 0;

    #pragma omp parallel num_threads(2) shared(producto, hayProducto)
    {
        int idHilo = omp_get_thread_num();

        if (idHilo == 0) {
            for (int i = 1; i <= CANTIDAD_PRODUCTOS; i++) {
                int producido = 0;

                while (producido == 0) {
                    #pragma omp critical(producto_compartido)
                    {
                        if (hayProducto == 0) {
                            producto = i;
                            hayProducto = 1;
                            producido = 1;
                            printf("Productor: produjo el producto %d\n", producto);
                        }
                    }
                }
            }
        }

        if (idHilo == 1) {
            for (int i = 1; i <= CANTIDAD_PRODUCTOS; i++) {
                int consumido = 0;

                while (consumido == 0) {
                    #pragma omp critical(producto_compartido)
                    {
                        if (hayProducto == 1) {
                            printf("Consumidor: consumio el producto %d\n", producto);
                            hayProducto = 0;
                            consumido = 1;
                        }
                    }
                }
            }
        }
    }

    return 0;
}