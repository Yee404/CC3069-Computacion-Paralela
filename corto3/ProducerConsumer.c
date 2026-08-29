// ProducerConsumer.c

#include <stdio.h>
#include <omp.h>

#define CANTIDAD_PRODUCTOS 5

int main() {
    int producto = 0;

    #pragma omp parallel num_threads(2) shared(producto)
    {
        int idHilo = omp_get_thread_num();

        for (int i = 1; i <= CANTIDAD_PRODUCTOS; i++) {

            if (idHilo == 0) {
                producto = i;
                printf("Productor: produjo el producto %d\n", producto);
            }

            #pragma omp barrier

            if (idHilo == 1) {
                printf("Consumidor: consumio el producto %d\n", producto);
            }

            #pragma omp barrier
        }
    }

    return 0;
}