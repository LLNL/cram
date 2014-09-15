//
// This test program causes process 0 to fail with exit(1)
//
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        exit(1);
    }

    MPI_Finalize();
}
