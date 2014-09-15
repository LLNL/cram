//
// This test program prints out arguments.
//
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        for (int i=0; i < argc; i++) {
            printf("%s\n", argv[i]);
        }
    }

    MPI_Finalize();
}
