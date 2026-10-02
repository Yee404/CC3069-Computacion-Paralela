/*----------------------------------------------------------------------
 * Universidad del Valle de Guatemala
 * Curso:     CC3169 - Computacion Paralela y Distribuida
 * Ejercicio: Ejercicio30Septiembre - Introduccion a Open MPI
 * Archivo:   Ejercicio1b_modificado.c  (Caso 2 - MPI_Reduce)
 * Descripcion: simulacion del calculo del consumo electrico total
 *              de las diferentes sucursales.
 *
 *              Cada proceso MPI representa una ubicacion diferente.
 *              Cada proceso registra su consumo electrico local y
 *              la Oficina Central obtiene el consumo total utilizando
 *              MPI_Reduce().
 *
 *              MODIFICACION: ademas del consumo total (MPI_SUM), la
 *              Oficina Central obtiene el consumo maximo (MPI_MAX) y
 *              el consumo minimo (MPI_MIN) con dos nuevas llamadas a
 *              MPI_Reduce().
 *
 * Compilar:  mpicc Ejercicio1b_modificado.c -o ejercicio1b_mod
 * Ejecutar:  mpirun -np 4 ./ejercicio1b_mod
 *----------------------------------------------------------------------*/

#include <stdio.h>
#include <mpi.h>

int main(int argc, char *argv[]) {

    int rank;
    int size;
    int consumo;
    int consumo_total;
    int consumo_maximo;
    int consumo_minimo;

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

    // Cada proceso registra su consumo electrico
    if (rank == 0) {
        consumo = 150;
    } else if (rank == 1) {
        consumo = 120;
    } else if (rank == 2) {
        consumo = 180;
    } else {
        consumo = 100;
    }

    printf("Proceso %d: consumo = %d kWh\n", rank, consumo);

    // Sumar los consumos de todos los procesos
    MPI_Reduce(&consumo, &consumo_total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    // Obtener el mayor consumo registrado entre todos los procesos
    MPI_Reduce(&consumo, &consumo_maximo, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);

    // Obtener el menor consumo registrado entre todos los procesos
    MPI_Reduce(&consumo, &consumo_minimo, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);

    // La Oficina Central muestra los resultados de las reducciones
    if (rank == 0) {
        printf("\nConsumo total: %d kWh\n", consumo_total);
        printf("Consumo maximo: %d kWh\n", consumo_maximo);
        printf("Consumo minimo: %d kWh\n", consumo_minimo);
    }

    // Finaliza correctamente el entorno MPI
    MPI_Finalize();

    return 0;
}
