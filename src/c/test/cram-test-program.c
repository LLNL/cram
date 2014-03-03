#include <stdio.h>
#include <assert.h>
#include <mpi.h>

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  // Get ranks and real ranks for this job.
  int rank, size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int real_rank, real_size;
  PMPI_Comm_size(MPI_COMM_WORLD, &real_size);
  PMPI_Comm_rank(MPI_COMM_WORLD, &real_rank);

  // Try a simple allreduce
  int sum;
  MPI_Allreduce(&rank, &sum, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

  // check the result against a sequential sum
  int check_sum = 0;
  for (int i=0; i < size; i++) {
    check_sum += i;
  }

  // Gather real ranks so we can write out the mapping.
  int real_ranks[size];
  MPI_Gather(&real_rank, 1, MPI_INT,
             real_ranks, 1, MPI_INT, 0, MPI_COMM_WORLD);

  if (rank == 0) {
    fprintf(stdout, "Can print to stdout.\n");
    if (sum == check_sum) {
      fprintf(stdout, "Allreduce checksum passed.\n");
    } else {
      fprintf(stdout, "Allreduce checksum failed!.\n");
      fprintf(stdout, "  Expected:  %d\n", check_sum);
      fprintf(stdout, "  Actual:    %d\n", sum);
    }
    fprintf(stdout, "Job size:      %d\n", size);
    fprintf(stdout, "Real job size: %d\n", real_size);
    fprintf(stdout, "\n");
    fprintf(stdout, "Rank mapping:\n");
    for (int i=0; i < size; i++) {
      fprintf(stdout, "    %5d <- %5d\n", i, real_ranks[i]);
    }

    fprintf(stderr, "Can print to stderr.\n");
  }

  MPI_Finalize();
}
