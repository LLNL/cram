#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "cram_file.h"

// Magic number goes at beginning of file
#define MAGIC 0x6372616d

// Offsets of file header fields
#define MAGIC_OFFSET       0
#define VERSION_OFFSET     4
#define NJOBS_OFFSET       8
#define NPROCS_OFFSET      12
#define MAX_JOB_OFFSET     16

// offset of first job record
#define JOB_RECORD_OFFSET  20

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
/// Read a cram int from a FILE*.
///
static int file_read_int(const cram_file_t *file) {
  int buf;
  size_t bytes = fread(&buf, sizeof(int), 1, file->fd);
  assert(bytes == sizeof(int));
  return ntohl(buf);
}


///
/// Read a cram string from a FILE*.
///
static char *file_read_string(const cram_file_t *file) {
  size_t len = file_read_int(file);
  char *str = malloc(sizeof(char) * len + 1);
  size_t bytes = fread(str, sizeof(char), len, file->fd);
  assert(bytes == sizeof(char) * len);
  str[len-1] = '\0';
  return str;
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


// ------------------------------------------------------------------------
// Public cram file interface
// ------------------------------------------------------------------------

bool cram_file_open(const char *filename, cram_file_t *file) {
  file->fd = fopen(filename, "r");
  if (file->fd < 0) {
    return false;
  }

  int magic = file_read_int(file);
  if (magic != MAGIC) {
    fprintf(stderr, "Error: %s is not a cram file!", filename);
    return false;
  }

  file->version      = file_read_int(file);
  file->num_jobs     = file_read_int(file);
  file->total_procs  = file_read_int(file);
  file->max_job_size = file_read_int(file);

  file->cur_job_record = malloc(file->max_job_size);
  file->cur_job_record_size = 0;
  file->cur_job_procs = 0;
  file->cur_job_id = -1;

  return true;
}


void cram_file_close(const cram_file_t *file) {
  fclose(file->fd);
  free(file->cur_job_record);
}


bool cram_file_has_more_jobs(const cram_file_t *file) {
  return file->cur_job_id < (file->num_jobs - 1);
}


///
/// Helper for cram_read_job -- does the decompression work.
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


void cram_read_job(const char *job_record,
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


bool cram_file_next_job(cram_file_t *file) {
  int job_record_size = file_read_int(file);
  if (job_record_size > file->max_job_size) {
    fprintf(stderr, "Error: Invalid job record size: %d > %d",
            job_record_size, file->max_job_size);
    return false;
  }

  file->cur_job_record_size = job_record_size;
  size_t bytes = fread(file->cur_job_record, 1, job_record_size, file->fd);
  assert(bytes == job_record_size);

  int *num_procs = (int*)(file->cur_job_record);
  file->cur_job_procs = ntohl(*num_procs);
  file->cur_job_id++;

  return true;
}


void cram_bcast_jobs(cram_file_t *file, cram_job_t *job, int root, MPI_Comm comm) {
  int rank, size;
  PMPI_Comm_rank(comm, &rank);
  PMPI_Comm_size(comm, &size);

  // Broadcast the struct, along with the file size.
//  PMPI_Bcast(file, sizeof(cram_file_t), MPI_BYTE, root, comm);

  // Allocate and broadcast data out to all processes
  if (rank != root) {
//    file->data = malloc(file->bytes);
//    file->fd = -1;
  }
//  PMPI_Bcast(file->data, file->bytes, MPI_BYTE, root, comm);
}


void cram_file_open_bcast(const char *filename, cram_file_t *file, int root,
                         MPI_Comm comm) {
  if (!cram_file_open(filename, file)) {
    fprintf(stderr, "Error: cram_file_open failed with error %d\n", errno);
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

  // First job is special because we don't have to decompress
  cram_job_t first_job;
  cram_file_next_job(file);
  cram_read_job(file->cur_job_record, NULL, &first_job);

  // print first job
  printf("Job %d:\n", file->cur_job_id);
  cram_job_print(&first_job);

  // Rest of jobs are based on first job.
  cram_job_t job;
  while (cram_file_has_more_jobs(file)) {
    cram_file_next_job(file);
    cram_read_job(file->cur_job_record, &first_job, &job);

    // print each subsequent job
    printf("Job %d:\n", file->cur_job_id);
    cram_job_print(&job);

    cram_job_free(&job);
  }

  cram_job_free(&first_job);
}
