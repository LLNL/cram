#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <mpi.h>
#include "cram_file.h"


void read_entire_cram_file(cram_file_t *file) {
  if (!cram_file_has_more_jobs(file)) {
    return;
  }

  // space for raw, compressed job record.
  char *job_record = malloc(file->max_job_size);

  // First job is special because we don't have to decompress
  cram_job_t first_job;
  cram_file_next_job(file, job_record);
  cram_job_decompress(job_record, NULL, &first_job);

  // Rest of jobs are based on first job.  Do not decompress any
  // of them. This is just a read benchmark.
  while (cram_file_has_more_jobs(file)) {
    cram_file_next_job(file, job_record);
  }

  // free everything up.
  free(job_record);
  cram_job_free(&first_job);
}


///
/// Simple test to read in a cram file.  Prints out entire time it took to
/// read.  Use this with CRAM_BUFFER_SIZE environment variable to test the
/// performance of various buffer sizes.
///
int main(int argc, char **argv) {
  PMPI_Init(&argc, &argv);

  int rank;
  PMPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (argc < 2) {
    if (rank == 0) {
      fprintf(stderr, "Usage: cram-read-file-test <cramfile>\n");
      fprintf(stderr, "  Reads an entire cram file using the C API and prints "
              "out the time it took.\n");
    }
    PMPI_Finalize();
    exit(1);
  }

  const char *filename = argv[1];

  cram_file_t file;
  if (!cram_file_open(filename, &file)) {
    printf("failed to map with errno %d: %s\n", errno, strerror(errno));
    PMPI_Finalize();
    exit(1);
  }

  printf("Reading file: %25s\n", filename);

  double start_time = PMPI_Wtime();
  read_entire_cram_file(&file);
  double end_time = PMPI_Wtime();

  double elapsed = end_time - start_time;
  printf("Read entire file in %.6f seconds\n", elapsed);

  PMPI_Finalize();
  exit(0);
}
