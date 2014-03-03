#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#include "cram_file.h"
#include "cram_serialization.h"

// Magic number goes at beginning of file
#define MAGIC 0x6372616d

// Offsets of file header fields
#define MAGIC_OFFSET       0
#define VERSION_OFFSET     4
#define NJOBS_OFFSET       8
#define NPROCS_OFFSET      12

// offset of first job record
#define JOB_RECORD_OFFSET  16


bool cram_file_map(const char *filename, cram_file_t *file) {
  file->fd = open(filename, O_RDONLY);
  if (file->fd < 0) {
    return false;
  }

  struct stat status;
  fstat(file->fd, &status);
  file->bytes = status.st_size;

#ifdef __bgq__
  file->data = malloc(file->bytes);
  char *pos = file->data;
  ssize_t bytes_read = 0;
  while (bytes_read < file->bytes) {
    ssize_t bytes = read(file->fd, pos, file->bytes);
    if (bytes < 0) {
      return false;
    }
    bytes_read += bytes;
    pos += bytes;
  }
  file->mapped = false;

#else
  file->data = mmap(NULL, file->bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file->fd, 0);
  if (file->data == MAP_FAILED) {
    return false;
  }
  file->mapped = true;
#endif

  int magic = cram_read_int(file, MAGIC_OFFSET);
  if (magic != MAGIC) {
    fprintf(stderr, "Error: %s is not a cram file!", filename);
  }

  file->version     = cram_read_int(file, VERSION_OFFSET);
  file->num_jobs    = cram_read_int(file, NJOBS_OFFSET);
  file->total_procs = cram_read_int(file, NPROCS_OFFSET);

  return true;
}


void cram_file_bcast(cram_file_t *file, int root, MPI_Comm comm) {
  int rank, size;
  PMPI_Comm_rank(comm, &rank);
  PMPI_Comm_size(comm, &size);

  // Broadcast the struct, along with the file size.
  PMPI_Bcast(file, sizeof(cram_file_t), MPI_BYTE, root, comm);

  // Allocate and broadcast data out to all processes
  if (rank != root) {
    file->data = malloc(file->bytes);
    file->mapped = false;
    file->fd = -1;
  }
  PMPI_Bcast(file->data, file->bytes, MPI_BYTE, root, comm);
}


void cram_file_map_bcast(const char *filename, cram_file_t *file, int root,
                         MPI_Comm comm) {
  if (!cram_file_map(filename, file)) {
    fprintf(stderr, "Error: mmap failed with error %d\n", errno);
    PMPI_Abort(comm, errno);
  }

  int size;
  PMPI_Comm_size(comm, &size);
  if (file->total_procs > size) {
    fprintf(stderr, "Error: This cram file requires %d processes, "
            "but this communicator has only %d.\n", file->total_procs, size);
    PMPI_Abort(comm, 1);
  }

  cram_file_bcast(file, root, comm);
}



void cram_file_close(const cram_file_t *file) {
  if (file->mapped) {
    munmap(file->data, file->bytes);
    close(file->fd);
  } else {
    free(file->data);
  }
}


int cram_file_find_job(const cram_file_t *file, int rank, cram_job_t *job) {
  // Check whether we really need this rank to run all of our jobs.
  if (rank >= file->total_procs) {
    return -1;
  }

  // Skip through job records until we come to the one for this rank.
  size_t offset = JOB_RECORD_OFFSET;
  int total_procs = 0;
  int job_id = 0;

  // We need the first job to decompress the rest.
  cram_job_t first_job;

  while (job_id < file->num_jobs) {
    total_procs += cram_read_int(file, offset);
    if (total_procs > rank) {
      break;
    }

    // record first job so we can decompress
    if (job_id == 0) {
      cram_read_job(file, offset, NULL, &first_job);
    }

    // skips jobs until we find the one we're looking for.
    cram_skip_job(file, &offset);
    job_id++;
  }

  // Return -1 if this rank isn't needed to run all the jobs in the file.
  if (job_id == file->num_jobs) {
    return -1;
  }

  // We found our job.  Write it out to the struct, decompressing if necessary.
  if (job_id == 0) {
    cram_read_job(file, offset, NULL, job);
  } else {
    cram_read_job(file, offset, &first_job, job);
    cram_job_free(&first_job);
  }

  // set this so that cram can use it to split MPI_COMM_WORLD
  return job_id;
}


void cram_job_setup(const cram_job_t *job, int *argc, char ***argv) {
  // change working directory
  chdir(job->working_dir);

  // Replace command line arguments with those of the job.
  // TODO: what to do with existing args?
  *argc = job->num_args;
  *argv = malloc(job->num_args * sizeof(char**));
  for (int i=0; i < job->num_args; i++) {
    (*argv)[i] = strdup(job->args[i]);
  }

  // Set environment variables based on the job's key/val pairs.
  for (int i=0; i < job->num_env_vars; i++) {
    setenv(job->keys[i], job->values[i], 1);
  }
}


void cram_job_free(cram_job_t *job) {
  free((char*)job->working_dir);
  free_string_array(job->num_args, job->args);
  free_string_array(job->num_env_vars, job->keys);
  free_string_array(job->num_env_vars, job->values);
}


void cram_cat(const cram_file_t *file) {
  printf("Number of Jobs:   %12d\n", file->num_jobs);
  printf("Total Procs:      %12d\n", file->total_procs);
  printf("Cram version:     %12d\n", file->version);
  printf("\n");
  printf("Job information:\n");

  // Run through all job records and write each out.
  size_t offset = JOB_RECORD_OFFSET;
  int job_id = 0;

  // We need the first job to decompress the rest.
  cram_job_t first_job, other_job;

  while (job_id < file->num_jobs) {
    printf("Job %d:\n", job_id);

    const cram_job_t *job;

    // record first job so we can decompress
    if (job_id == 0) {
      cram_read_job(file, offset, NULL, &first_job);
      job = &first_job;
    } else {
      cram_read_job(file, offset, &first_job, &other_job);
      job = &other_job;
    }

    printf("  Num procs: %d\n", job->num_procs);
    printf("  Working dir: %s\n", job->working_dir);
    printf("  Arguments:\n");

    printf("      ");
    for (int i=0; i < job->num_args; i++) {
      if (i > 0) printf(" ");
      printf("%s", job->args[i]);
    }
    printf("\n");

    printf("  Environment:\n");
    for (int i=0; i < job->num_env_vars; i++) {
      printf("      '%s' : '%s'\n", job->keys[i], job->values[i]);
    }

    // skip to next job in the file.
    cram_skip_job(file, &offset);
    job_id++;
  }
}
