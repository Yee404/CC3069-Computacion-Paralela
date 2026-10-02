/*----------------------------------------------------------------------
 * Universidad del Valle de Guatemala
 * Curso:     CC3169 - Computacion Paralela y Distribuida
 * Ejercicio: Ejercicio30Septiembre - Introduccion a Open MPI
 * Archivo:   Ejercicio1a_modificado.c  (Caso 1 - MPI_Gather)
 * Descripcion: simulacion de la recoleccion de temperaturas
 *              registradas en diferentes sucursales.
 *
 *              Cada proceso MPI representa una ubicacion diferente:
 *                  rank 0 -> Oficina central
 *                  rank 1 -> Sucursal 1
 *                  rank 2 -> Sucursal 2
 *                  rank 3 -> Sucursal 3
 *
 *              MODIFICACION: cada proceso registra DOS mediciones de
 *              temperatura y la Oficina Central reune todos los valores
 *              (4 procesos x 2 mediciones = 8 valores) con UNA sola
 *              llamada a MPI_Gather(), usando sendcount = recvcount = 2.
 *
 * Compilar:  mpicc Ejercicio1a_modificado.c -o ejercicio1a_mod
 * Ejecutar:  mpirun -np 4 ./ejercicio1a_mod
 *----------------------------------------------------------------------*/

#include <stdio.h>
#include <mpi.h>

#define NUM_PROCESOS   4
#define NUM_MEDICIONES 2   // mediciones registradas por cada proceso

int main(int argc, char *argv[]) {

    int rank;
    int size;
    float temperatura[NUM_MEDICIONES];                    // buffer local (envio)
    float temperaturas[NUM_PROCESOS * NUM_MEDICIONES];    // buffer del root (recepcion)

    // Inicializa el entorno MPI
    MPI_Init(&argc, &argv);

    // Obtener el identificador del proceso actual
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Obtener el numero total de procesos que participan
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Este ejercicio requiere exactamente 4 procesos
    if (size != NUM_PROCESOS) {

        if (rank == 0) {
            printf("Este programa requiere exactamente 4 procesos.\n");
        }

        MPI_Finalize();
        return 0;
    }

    // Cada proceso registra dos temperaturas locales
    if (rank == 0) {
        temperatura[0] = 24.5; temperatura[1] = 25.0;
    } else if (rank == 1) {
        temperatura[0] = 26.1; temperatura[1] = 26.4;
    } else if (rank == 2) {
        temperatura[0] = 23.8; temperatura[1] = 24.1;
    } else {
        temperatura[0] = 27.0; temperatura[1] = 27.3;
    }

    printf("Proceso %d: temperaturas registradas = %.1f C, %.1f C\n",
           rank, temperatura[0], temperatura[1]);

    // Reunir las temperaturas de todos los procesos en rank 0.
    // Cada proceso envia 2 floats y el root recibe 2 floats por proceso.
    MPI_Gather(
        temperatura,        // sendbuf: arreglo local con 2 mediciones
        NUM_MEDICIONES,     // sendcount: 2 (antes 1)
        MPI_FLOAT,          // sendtype
        temperaturas,       // recvbuf: arreglo de 8 posiciones en el root
        NUM_MEDICIONES,     // recvcount: 2 por proceso (antes 1)
        MPI_FLOAT,          // recvtype
        0,                  // root: Oficina Central
        MPI_COMM_WORLD      // communicator
    );

    // La Oficina Central muestra todas las temperaturas recibidas.
    // Los datos del proceso i quedan en temperaturas[i*2] y temperaturas[i*2+1].
    if (rank == 0) {

        printf("\nOficina Central: temperaturas recibidas\n");

        for (int i = 0; i < size; i++) {
            printf("Proceso %d: %.1f C, %.1f C\n", i,
                   temperaturas[i * NUM_MEDICIONES],
                   temperaturas[i * NUM_MEDICIONES + 1]);
        }
    }

    // Finaliza correctamente el entorno MPI
    MPI_Finalize();

    return 0;
}
