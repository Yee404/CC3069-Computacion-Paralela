// ProducerConsumerAtomic.c

#include <stdio.h>
#include <omp.h>

#define CANTIDAD_PRODUCTOS 5

int main() {
    int producto = 0;

    #pragma omp parallel num_threads(2) shared(producto)
    {
        int idHilo = omp_get_thread_num();

        if (idHilo == 0) {
            for (int i = 1; i <= CANTIDAD_PRODUCTOS; i++) {

                #pragma omp atomic write
                producto = i;

                printf("Productor: produjo el producto %d\n", i);
            }
        }

        if (idHilo == 1) {
            for (int i = 1; i <= CANTIDAD_PRODUCTOS; i++) {
                int productoConsumido;

                #pragma omp atomic read
                productoConsumido = producto;

                printf("Consumidor: consumio el producto %d\n", productoConsumido);
            }
        }
    }

    return 0;
}