//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Cram.
// Written by Todd Gamblin, tgamblin@llnl.gov, All rights reserved.
// LLNL-CODE-661100
//
// For details, see https://github.com/scalability-llnl/cram.
// Please also see the LICENSE file for our notice and the LGPL.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License (as published by
// the Free Software Foundation) version 2.1 dated February 1999.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and
// conditions of the GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//////////////////////////////////////////////////////////////////////////////
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>

#include "cram_file.h"

// Magic number goes at beginning of file
#define MAGIC 0x6372616d

// Tag for cram messages
#define CRAM_TAG 7675

// Offsets of file header fields
#define MAGIC_OFFSET       0
#define VERSION_OFFSET     4
#define NJOBS_OFFSET       8
#define NPROCS_OFFSET      12
#define MAX_JOB_OFFSET     16

// offset of first job record
#define JOB_RECORD_OFFSET  20

// max concurrent ranks to send job records to at once.
#define MAX_CONCURRENT_PEERS 512

// Ideal number of bytes to use for Lustre read buffers: 2MB.
#define LUSTRE_BUFFER_SIZE 2097152

// Default cram executable name: means we should use argv[0] for exe.
#define CRAM_DEFAULT_EXE "<exe>"


// ------------------------------------------------------------------------
// Globals used by fortran arg routines
// ------------------------------------------------------------------------
int cram_argc = 0;
const char **cram_argv = NULL;


// ------------------------------------------------------------------------
// Utility functions
// ------------------------------------------------------------------------

///
/// Find strings efficiently in a sorted array using bsearch.
///
static inline size_t index_of(size_t num_elts,
                              const char **sorted_array,
                              const char *string) {
  const char **pos = bsearch(string, sorted_array, num_elts, sizeof(char*),
                             (int(*)(const void*, const void*))strcmp);
  return (pos - sorted_array);
}


///
/// Free all the strings in a char**, then free the array itself.
///
static void free_string_array(int num_elts, const char **arr) {
  for (int i=0; i < num_elts; i++) {
    free((char*)arr[i]);
  }
  free(arr);
}


///
/// Create a copy of an entire string array.
///
static const char **dup_string_array(int num_elts, const char **src) {
  const char **arr = malloc(sizeof(char*) * num_elts);
  for (int i=0; i < num_elts; i++) {
    arr[i] = strdup(src[i]);
  }
  return arr;
}


///
/// Read a cram int from a FILE*.
///
static int file_read_int(const cram_file_t *file) {
  int buf;
  size_t ints = fread(&buf, sizeof(int), 1, file->fd);
  if (ints != 1) {
    fprintf(stderr, "Error reading cram file.  "
            "Expected one int but got read %zd ints.\n", ints);
    exit(1);
  }
  return ntohl(buf);
}


///
/// Read a cram int from a buffer
///
static int buf_read_int(const char *buf, size_t *offset) {
  const int *int_ptr = (int*)&buf[*offset];
  *offset += sizeof(int);
  return ntohl(*int_ptr);
}


///
/// Read a cram string from a buffer
///
static char *buf_read_string(const char *buf, size_t *offset) {
  size_t size = buf_read_int(buf, offset);
  char *string = strndup(&buf[*offset], size);
  *offset += size;
  return string;
}


static size_t get_cram_buffer_size() {
  const char *bufsize_string = getenv("CRAM_BUFFER_SIZE");
  if (!bufsize_string) {
    return LUSTRE_BUFFER_SIZE;
  }

  int rank;
  PMPI_Comm_rank(MPI_COMM_WORLD, &rank);

  char *endptr;
  size_t bufsize = strtoll(bufsize_string, &endptr, 10);
  if (*bufsize_string && *endptr == '\0') {
    if (rank == 0) {
      fprintf(stderr, "Using CRAM_BUFFER_SIZE=%d.\n", bufsize);
    }
    return bufsize;
  } else {
    if (rank == 0) {
      fprintf(stderr, "Warning: Invalid value for CRAM_BUFFER_SIZE: %s.  "
              "Using default of %d", bufsize_string, LUSTRE_BUFFER_SIZE);
    }
    return LUSTRE_BUFFER_SIZE;
  }
}


// ------------------------------------------------------------------------
// Public cram file interface
// ------------------------------------------------------------------------

bool cram_file_open(const char *filename, cram_file_t *file) {
  file->fd = fopen(filename, "r");
  if (file->fd == NULL) {
    return false;
  }

  // Try to use a large buffer to read the file fast.
  setvbuf(file->fd, NULL, _IOFBF, get_cram_buffer_size());

  // check magic number at start of header
  int magic = file_read_int(file);
  if (magic != MAGIC) {
    fprintf(stderr, "Error: %s is not a cram file!", filename);
    return false;
  }

  // read rest of header after magic check.
  file->version      = file_read_int(file);
  file->num_jobs     = file_read_int(file);
  file->total_procs  = file_read_int(file);
  file->max_job_size = file_read_int(file);

  file->cur_job_record_size = 0;
  file->cur_job_procs = 0;
  file->cur_job_id = -1;

  return true;
}


void cram_file_close(const cram_file_t *file) {
  fclose(file->fd);
}


bool cram_file_has_more_jobs(const cram_file_t *file) {
  return file->cur_job_id < (file->num_jobs - 1);
}


///
/// Helper for cram_job_decompress -- does the real work.
///
static inline void decompress(const cram_job_t *base,
                              int num_missing, const char **missing,
                              int num_changed, const char **changed_keys,
                              const char **changed_vals,
                              cram_job_t *job) {
  // Figure out how many environemnt variables overlap with base
  int num_overlap = 0;
  for (int i=0; i < num_changed; i++) {
    if (index_of(base->num_env_vars, base->keys, changed_keys[i])) {
      num_overlap++;
    }
  }

  // Allocate space for new vars.
  job->num_env_vars = base->num_env_vars
    + num_changed - num_overlap - num_missing;
  job->keys = malloc(job->num_env_vars * sizeof(const char*));
  job->values = malloc(job->num_env_vars * sizeof(const char*));

  // Merge base and changed values into job, removing missing keys.
  // These arrays are assumed to be sorted, so we can march through
  // them in O(n) time.
  size_t bx=0, cx=0, mx=0, jx=0;
  while (jx < job->num_env_vars) {
    int cmp = -1;
    if (cx < num_changed) {
      cmp = strcmp(base->keys[bx], changed_keys[cx]);
    }

    if (cmp == 0) {
      // base & changed are the same, take the changed value and skip base.
      job->keys[jx]   = strdup(changed_keys[cx]);
      job->values[jx] = strdup(changed_vals[cx]);
      bx++; cx++; jx++;

    } else if (cmp < 0) {
      if (mx < num_missing && strcmp(base->keys[bx], missing[mx]) == 0) {
        // This key is missing in job; skip it.
        bx++; mx++;

      } else {
        // base < changed: this key is preserved in the new job.
        job->keys[jx]   = strdup(base->keys[bx]);
        job->values[jx] = strdup(base->values[bx]);
        bx++; jx++;
      }

    } else { /* cmp > 0 */
      // This key was added, and isn't in base. Take it from changed.
      job->keys[jx]   = strdup(changed_keys[cx]);
      job->values[jx] = strdup(changed_vals[cx]);
      cx++; jx++;
    }
  }
}


void cram_job_decompress(const char *job_record,
                         const cram_job_t *base, cram_job_t *job) {
  // start at beginning of job record.
  size_t offset = 0;

  // num_procs
  job->num_procs = buf_read_int(job_record, &offset);

  // working directory
  job->working_dir = buf_read_string(job_record, &offset);

  // command line arguments
  int num_args = buf_read_int(job_record, &offset);
  job->num_args = num_args;
  job->args = (const char**) malloc(num_args * sizeof(char*));

  for (int i=0; i < num_args; i++) {
    job->args[i] = buf_read_string(job_record, &offset);
  }

  // Subtracted environment variables are not in this job but are
  // in the base job.
  int num_subtracted = buf_read_int(job_record, &offset);

  const char **subtracted_env_vars = NULL;
  if (num_subtracted) {
    // If there is no base job, then there can't be any subtracted vars.
    if (!base) {
      fprintf(stderr, "Cannot decompress this job without a base job!\n");
      PMPI_Abort(MPI_COMM_WORLD, 1);
    }

    subtracted_env_vars = malloc(num_subtracted * sizeof(const char*));
    for (int i=0; i < num_subtracted; i++) {
      subtracted_env_vars[i] = buf_read_string(job_record, &offset);
    }
  }

  // Changed environemnt vars were either added to the base or they're different
  // in job from in base.
  int num_changed = buf_read_int(job_record, &offset);

  const char **changed_keys = malloc(num_changed * sizeof(const char*));
  const char **changed_vals = malloc(num_changed * sizeof(const char*));
  for (int i=0; i < num_changed; i++) {
    changed_keys[i] = buf_read_string(job_record, &offset);

    changed_vals[i] = buf_read_string(job_record, &offset);
  }

  if (base) {
    // Apply diffs to the base job and write result into job.
    decompress(base, num_subtracted, subtracted_env_vars,
               num_changed, changed_keys, changed_vals, job);

    // free temporaries
    free_string_array(num_subtracted, subtracted_env_vars);
    free_string_array(num_changed, changed_keys);
    free_string_array(num_changed, changed_vals);

  } else {
    // If there is no base, we just copy the changed vals (first job)
    job->num_env_vars = num_changed;
    job->keys         = changed_keys;
    job->values       = changed_vals;
  }
}


bool cram_file_next_job(cram_file_t *file, char *job_record) {
  int job_record_size = file_read_int(file);
  if (job_record_size > file->max_job_size) {
    fprintf(stderr, "Error: Invalid job record size: %d > %d",
            job_record_size, file->max_job_size);
    return false;
  }

  file->cur_job_record_size = job_record_size;
  size_t bytes = fread(job_record, 1, job_record_size, file->fd);
  if (bytes != job_record_size) {
    fprintf(stderr, "Error: Expected to read %d bytes, but got %zd\n",
            job_record_size, bytes);
    return false;
  }

  size_t offset = 0;
  file->cur_job_procs = buf_read_int(job_record, &offset);
  file->cur_job_id++;

  return true;
}


void cram_file_bcast_jobs(cram_file_t *file, int root, cram_job_t *job, int *id,
                          MPI_Comm comm) {
  int rank, size;
  PMPI_Comm_rank(comm, &rank);
  PMPI_Comm_size(comm, &size);

  // check total procs and grab the max job size.
  int max_job_size;
  if (rank == root) {
    if (file->total_procs > size) {
      fprintf(stderr, "Error: This cram file requires %d processes, "
              "but this communicator has only %d.\n", file->total_procs, size);
      PMPI_Abort(comm, 1);
    }
    max_job_size = file->max_job_size;
  }

  // bcast max job size
  PMPI_Bcast(&max_job_size, 1, MPI_INT, root, comm);
  char *job_record = malloc(max_job_size);

  // read in compressed data for first job record
  if (rank == root) {
    if (!cram_file_next_job(file, job_record)) {
      fprintf(stderr, "Error reading job %d from cram file on rank %d\n",
              0, root);
      PMPI_Abort(comm, 1);
    }
  }

  // Bcast and decompress first job.
  cram_job_t first_job;
  PMPI_Bcast(job_record, max_job_size, MPI_CHAR, root, comm);
  cram_job_decompress(job_record, NULL, &first_job);

  // start by sending to the first rank in the second job.
  int cur_rank = first_job.num_procs;
  bool in_first_job = rank < cur_rank;

  if (rank == root) {
    // Root needs to send to all the other jobs
    // Start by iterating through remaining jobs in the file.
    while (cram_file_has_more_jobs(file)) {
      if (!cram_file_next_job(file, job_record)) {
        fprintf(stderr, "Error reading job %d from cram file on rank %d\n",
                file->cur_job_id + 1, root);
        PMPI_Abort(comm, 1);
      }

      // array of requests for all sends we'll do
      // max concurrent peers max number of ranks we'll send to at once.
      int max_requests = MAX_CONCURRENT_PEERS * 2;
      MPI_Request requests[max_requests];

      // iterate through all ranks in this job, incrementing first_rank as we go.
      int end_rank = cur_rank + file->cur_job_procs;
      while (cur_rank < end_rank) {
        int r = 0;
        while (r < max_requests && cur_rank < end_rank) {
          PMPI_Isend(&file->cur_job_id, 1, MPI_INT, cur_rank,
                     CRAM_TAG, comm, &requests[r++]);
          PMPI_Isend(job_record, file->cur_job_record_size, MPI_CHAR, cur_rank,
                     CRAM_TAG, comm, &requests[r++]);
          cur_rank++;
        }
        PMPI_Waitall(r, requests, MPI_STATUSES_IGNORE);
      }
    }

    // send a job id of -1 to any inactive ranks
    int inactive_rank_id = -1;
    for (; cur_rank < size; cur_rank++) {
      PMPI_Send(&inactive_rank_id, 1, MPI_INT, cur_rank, CRAM_TAG, comm);
    }

  } else if (!in_first_job) {
    // Ranks NOT in the first job need to receive their actual job record.
    // Ranks in the first job already have their job record.
    PMPI_Recv(id, 1, MPI_INT, root, CRAM_TAG, comm, MPI_STATUS_IGNORE);
    if (*id >= 0) {
      PMPI_Recv(job_record, max_job_size, MPI_CHAR, root, CRAM_TAG, comm,
                MPI_STATUS_IGNORE);
    }
    cram_job_decompress(job_record, &first_job, job);
  }

  // If this rank is in the first job, then just copy the first job we
  // got from the bcast.  And set the id to zero.
  if (in_first_job) {
    cram_job_copy(&first_job, job);
    *id = 0;
  }

  // Can free the first job now b/c we don't need it.
  cram_job_free(&first_job);
}


static void arg_copy(const cram_job_t *job,
                     int *dest_argc, const char ***dest_argv) {

    *dest_argc = job->num_args;
    *dest_argv = malloc(job->num_args * sizeof(char**));
    for (int i=0; i < job->num_args; i++) {
        (*dest_argv)[i] = strdup(job->args[i]);
    }
}


void cram_job_setup(const cram_job_t *job, int *argc, const char ***argv) {
  // change working directory
  chdir(job->working_dir);

  // save argv[0] so that we can use it for the first arg.
  const char *exe_name = NULL;
  if (*argc > 0 && *argv) {
    exe_name = (*argv)[0];
  }

  // Replace command line arguments with those of the job.
  arg_copy(job, argc, argv);

  // set argv[0] to the actual exe name
  if (strcmp(job->args[0], CRAM_DEFAULT_EXE) == 0) {
    if (exe_name) {
      free((void*)(*argv)[0]);
      (*argv)[0] = strdup(exe_name);
    }
  }

  // Also copy arguments into globals for Fortran arg interceptors to access.
  arg_copy(job, &cram_argc, &cram_argv);

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


void cram_job_copy(const cram_job_t *src, cram_job_t *dest) {
  dest->num_procs    = src->num_procs;
  dest->working_dir  = strdup(src->working_dir);

  dest->num_args     = src->num_args;
  dest->args         = dup_string_array(src->num_args, src->args);

  dest->num_env_vars = src->num_env_vars;
  dest->keys         = dup_string_array(src->num_env_vars, src->keys);
  dest->values       = dup_string_array(src->num_env_vars, src->values);
}


void cram_job_print(const cram_job_t *job) {
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
}


void cram_file_cat(cram_file_t *file) {
  printf("Number of Jobs:   %12d\n", file->num_jobs);
  printf("Total Procs:      %12d\n", file->total_procs);
  printf("Cram version:     %12d\n", file->version);
  printf("Max job record:   %12d\n", file->max_job_size);
  printf("\n");
  printf("Job information:\n");

  if (!cram_file_has_more_jobs(file)) {
    return;
  }

  // space for raw, compressed job record.
  char *job_record = malloc(file->max_job_size);

  // First job is special because we don't have to decompress
  cram_job_t first_job;
  cram_file_next_job(file, job_record);
  cram_job_decompress(job_record, NULL, &first_job);

  // print first job
  printf("Job %d:\n", file->cur_job_id);
  cram_job_print(&first_job);

  // Rest of jobs are based on first job.
  cram_job_t job;
  while (cram_file_has_more_jobs(file)) {
    cram_file_next_job(file, job_record);
    cram_job_decompress(job_record, &first_job, &job);

    // print each subsequent job
    printf("Job %d:\n", file->cur_job_id);
    cram_job_print(&job);

    cram_job_free(&job);
  }

  free(job_record);
  cram_job_free(&first_job);
}
