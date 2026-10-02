/*----------------------------------------------------------------------
 * Universidad del Valle de Guatemala
 * Curso:     CC3169 - Computacion Paralela y Distribuida
 * Ejercicio: Ejercicio30Septiembre - Introduccion a Open MPI
 * Descripcion: simulacion de la recoleccion de temperaturas
 *              registradas en diferentes sucursales.
 *
 *              Cada proceso MPI representa una ubicacion diferente:
 *                  rank 0 -> Oficina central
 *                  rank 1 -> Sucursal 1
 *                  rank 2 -> Sucursal 2
 *                  rank 3 -> Sucursal 3
 *
 *              Cada proceso genera una temperatura local y la
 *              Oficina Central recopila todos los valores utilizando
 *              MPI_Gather().
 *----------------------------------------------------------------------*/

#include <stdio.h>
#include <mpi.h>

int main(int argc, char *argv[]) {

    int rank;
    int size;
    float temperatura;
    float temperaturas[4];

    // Inicializa el entorno MPI
    MPI_Init(&argc, &argv);

    // Obtener el identificador del proceso actual
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Obtener el numero total de procesos que participan
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Este ejercicio requiere exactamente 4 procesos
    if (size != 4) {

        if (rank == 0) {
            printf("Este programa requiere exactamente 4 procesos.\n");
        }

        MPI_Finalize();
        return 0;
    }

    // Cada proceso registra una temperatura local
    if (rank == 0) {
        temperatura = 24.5;
    } else if (rank == 1) {
        temperatura = 26.1;
    } else if (rank == 2) {
        temperatura = 23.8;
    } else {
        temperatura = 27.0;
    }

    printf("Proceso %d: temperatura registrada = %.1f C\n",
           rank, temperatura);

    // Reunir las temperaturas de todos los procesos en rank 0
    MPI_Gather(
        &temperatura,
        1,
        MPI_FLOAT,
        temperaturas,
        1,
        MPI_FLOAT,
        0,
        MPI_COMM_WORLD
    );

    // La Oficina Central muestra todas las temperaturas recibidas
    if (rank == 0) {

        printf("\nOficina Central: temperaturas recibidas\n");

        for (int i = 0; i < size; i++) {
            printf("Proceso %d: %.1f C\n", i, temperaturas[i]);
        }
    }

    // Finaliza correctamente el entorno MPI
    MPI_Finalize();

    return 0;
}