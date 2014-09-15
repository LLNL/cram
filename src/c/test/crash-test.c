//
// This test program causes process 0 to fail with a SEGV.
//
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

//    int rank;
//    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

//    if (rank == 0) {
        int *bad_pointer = NULL;
        int v = *bad_pointer;
        printf("Value was %d.\n", v);
//    }

    MPI_Finalize();
}
