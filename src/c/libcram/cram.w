// -*- c -*-
//
// This file defines MPI wrappers for cram.
//
#include <stdlib.h>
#include <stdio.h>
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

  // Look for the CRAM_FILE environment variable to find where our input lives.
  const char *cram_filename = getenv("CRAM_FILE");
  if (!cram_filename) {
    fprintf(stderr, "ERROR: CRAM_FILE environment variable was not set. Aborting.\n");
    PMPI_Abort(MPI_COMM_WORLD, 1);
  }

  // Read the whole file in on rank 1 (it's compressed, so this should scale fairly well,
  // e.g. out to ~1M jobs assuming 1GB RAM per process)
  cram_file_t cram_file;
  cram_file_map_bcast(cram_filename, &cram_file, 0, MPI_COMM_WORLD);

  // Find our job descriptor in the cram file.
  cram_job_t cram_job;
  int job_id = cram_file_find_job(&cram_file, rank, &cram_job);

  // Use the job id to split MPI_COMM_WORLD.
  PMPI_Comm_split(MPI_COMM_WORLD, job_id, rank, &local_world);

  // set up this job's environment based on the job descriptor.
  cram_job_setup(&cram_job, {{0}}, {{1}});

  // Throw away unneeded ranks.
  if (job_id == -1) {
    PMPI_Finalize();
    exit(0);
  }

  // Free up resources for the application to use.
  cram_job_free(&cram_job);
  cram_file_close(&cram_file);
}{{endfn}}

// This generates interceptors that will catch every MPI routine
// *except* MPI_Init.  The interceptors just make sure that if
// they are called with an argument of type MPI_Comm that has a
// value of MPI_COMM_WORLD, they switch it to local_world.
{{fnall func MPI_Init}}{
  {{apply_to_type MPI_Comm swap_world}}
  {{callfn}}
}{{endfnall}}
