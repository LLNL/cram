// -*- c -*-
//
// This file defines MPI wrappers for cram.
//
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <mpi.h>

#include "cram_file.h"

// Local world communicator for each job run concurrently.
static MPI_Comm local_world;

// This function modifies its parameter by swapping it with local world.
// if it is MPI_COMM_WORLD.
#define swap_world(world) \
  do { \
    if (world == MPI_COMM_WORLD) { \
      world = local_world; \
    } \
  } while (0)

// MPI_Init does all the communicator setup
//
{{fn func MPI_Init}}{
  // First call PMPI_Init()
  {{callfn}}

  // Get this process's rank.
  int rank;
  PMPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // Tell the user they're running with cram.
  if (rank == 0) {
    fprintf(stderr,   "===========================================================\n");
    fprintf(stderr,   " This job is running with Cram.\n");
    fprintf(stderr,   "\n");
  }

  // Look for the CRAM_FILE environment variable to find where our input lives.
  const char *cram_filename = getenv("CRAM_FILE");
  if (!cram_filename) {
    if (rank == 0) {
      fprintf(stderr, " CRAM_FILE environment variable is not set.\n");
      fprintf(stderr, " Disabling Cram and running normally instead.\n");
      fprintf(stderr, "===========================================================\n");
    }
    local_world = MPI_COMM_WORLD;
    return MPI_SUCCESS;
  }

  // Read the whole file in on rank 1 (it's compressed, so this should scale fairly well,
  // e.g. out to ~1M jobs assuming 1GB RAM per process)
  cram_file_t cram_file;

  if (rank == 0) {
    if (!cram_file_open(cram_filename, &cram_file)) {
      fprintf(stderr, "Error: cram_file_open failed with error %d\n", errno);
      PMPI_Abort(MPI_COMM_WORLD, errno);
    }
    fprintf(stderr,   " Splitting this MPI job into %d jobs.\n", cram_file.num_jobs);
    fprintf(stderr,   " This will use %d total processes.\n", cram_file.total_procs);
  }

  // Receive our job from the root process.
  cram_job_t cram_job;
  int job_id;

  double start_time = MPI_Wtime();
  cram_file_bcast_jobs(&cram_file, 0, &cram_job, &job_id, MPI_COMM_WORLD);
  double bcast_time = MPI_Wtime();

  // Use the job id to split MPI_COMM_WORLD.
  PMPI_Comm_split(MPI_COMM_WORLD, job_id, rank, &local_world);
  double split_time = MPI_Wtime();

  // Throw away unneeded ranks.
  if (job_id == -1) {
    PMPI_Barrier(MPI_COMM_WORLD); // matches barrier later.
    PMPI_Finalize();
    exit(0);
  }

  // set up this job's environment based on the job descriptor.
  cram_job_setup(&cram_job, {{0}}, {{1}});
  double setup_time = MPI_Wtime();

  // Redirect I/O to a separate file for each cram job.
  // These files will be in the job's working directory.
  char out_file_name[1024], err_file_name[1024];
  sprintf(out_file_name, "cram.%d.out", job_id);
  sprintf(err_file_name, "cram.%d.err", job_id);

  // don't freopen on root until aftetr printing status.
  if (rank != 0) {
    freopen(out_file_name, "w", stdout);
    freopen(err_file_name, "w", stderr);
  }

  // wait for lots of files to open.
  PMPI_Barrier(MPI_COMM_WORLD);
  double freopen_time = MPI_Wtime();

  if (rank == 0) {
    fprintf(stderr,   "\n");
    fprintf(stderr,   " Successfully set up job:\n");
    fprintf(stderr,   "   Job broadcast:   %.6f sec\n", bcast_time   - start_time);
    fprintf(stderr,   "   MPI_Comm_split:  %.6f sec\n", split_time   - bcast_time);
    fprintf(stderr,   "   Job setup:       %.6f sec\n", setup_time   - split_time);
    fprintf(stderr,   "   File open:       %.6f sec\n", freopen_time - setup_time);
    fprintf(stderr,   "  --------------------------------------\n");
    fprintf(stderr,   "   Total:           %.6f sec\n", freopen_time - start_time);
    fprintf(stderr,   "  \n");
    fprintf(stderr,   "===========================================================\n");

    // reopen *last* on the zero rank.
    freopen(out_file_name, "w", stdout);
    freopen(err_file_name, "w", stderr);

    cram_file_close(&cram_file);
  }

  cram_job_free(&cram_job);
}{{endfn}}

// This generates interceptors that will catch every MPI routine
// *except* MPI_Init.  The interceptors just make sure that if
// they are called with an argument of type MPI_Comm that has a
// value of MPI_COMM_WORLD, they switch it to local_world.
{{fnall func MPI_Init}}{
  {{apply_to_type MPI_Comm swap_world}}
  {{callfn}}
}{{endfnall}}
