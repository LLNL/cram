#include <stdio.h>
#include <assert.h>
#include <mpi.h>

extern const char **environ;

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
    // Print out arguments
    // Print out status of various tests
    fprintf(stdout, "=========================================================\n");
    fprintf(stdout, "Test results\n");
    fprintf(stdout, "=========================================================\n");
    if (sum == check_sum) {
      fprintf(stdout, "Allreduce checksum passed.\n");
    } else {
      fprintf(stdout, "Allreduce checksum failed!.\n");
      fprintf(stdout, "  Expected:  %d\n", check_sum);
      fprintf(stdout, "  Actual:    %d\n", sum);
    }
    fprintf(stdout, "\n\n");

    // Print out job size information, as well as real size info.
    fprintf(stdout, "=========================================================\n");
    fprintf(stdout, "Job info\n");
    fprintf(stdout, "=========================================================\n");
    fprintf(stdout, "  Job size:      %d\n", size);
    fprintf(stdout, "  Real job size: %d\n", real_size);
    fprintf(stdout, "\n");
    fprintf(stdout, "  Arguments\n");
    fprintf(stdout, "---------------------------------------------------------\n");
    fprintf(stdout, "      ");
    for (int i=0; i < argc; i++) {
      if (i != 0) fprintf(stdout, " ");
      fprintf(stdout, "%s", argv[i]);
    }
    fprintf(stdout, "\n\n");

    // Rank mapping
    fprintf(stdout, "  Rank mapping\n");
    fprintf(stdout, "---------------------------------------------------------\n");
    for (int i=0; i < size; i++) {
      fprintf(stdout, "    %5d <- %5d\n", i, real_ranks[i]);
    }
    fprintf(stdout, "\n\n");

    // Print out environment
    fprintf(stdout, "  Environment variables\n");
    fprintf(stdout, "---------------------------------------------------------\n");
    for (const char **var=environ; *var; var++) {
      fprintf(stdout, "    %s\n", *var);
    }

    // Print something to stderr just to make sure that works too.
    fprintf(stderr, "Can print to stderr.\n");
  }

  MPI_Finalize();
}
