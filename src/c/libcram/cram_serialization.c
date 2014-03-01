#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include <mpi.h>

#include "cram_file.h"

int cram_read_int(const cram_file_t *file, size_t offset) {
  const int *int_ptr = (int*)&file->data[offset];
  return ntohl(*int_ptr);
}


char *cram_read_string(const cram_file_t *file, size_t offset) {
  size_t size = cram_read_int(file, offset);
  return strndup(&file->data[offset + sizeof(int)], size);
}


void cram_skip_int(const cram_file_t *file, size_t *offset) {
  *offset += sizeof(int);
}


void cram_skip_string(const cram_file_t *file, size_t *offset) {
  size_t size = cram_read_int(file, *offset);
  cram_skip_int(file, offset);
  *offset += size;
}


void cram_skip_job(const cram_file_t *file, size_t *offset) {
  cram_skip_int(file, offset);       // num_procs
  cram_skip_string(file, offset);    // working_dir

  // command line arguments
  int num_args = cram_read_int(file, *offset);
  cram_skip_int(file, offset);
  for (int i=0; i < num_args; i++) {
    cram_skip_string(file, offset);
  }

  // subtracted environment variables
  int subtracted_env_vars = cram_read_int(file, *offset);
  cram_skip_int(file, offset);
  for (int i=0; i < subtracted_env_vars; i++) {
    cram_skip_string(file, offset);
  }

  // added/changed environment variables
  int changed_env_vars = cram_read_int(file, *offset);
  cram_skip_int(file, offset);
  for (int i=0; i < changed_env_vars; i++) {
    cram_skip_string(file, offset);
    cram_skip_string(file, offset);
  }
}


static inline size_t index_of(size_t num_elts,
                              const char **sorted_array,
                              const char *string) {
  const char **pos = bsearch(string, sorted_array, num_elts, sizeof(char*),
                             (int(*)(const void*, const void*))strcmp);
  return (pos - sorted_array);
}


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
  // These arrays are sorted, so we can just march through them.
  size_t bx=0, cx=0, mx=0, jx=0;
  while (jx < job->num_env_vars) {
    int cmp = strcmp(base->keys[bx], changed_keys[cx]);
    if (cmp == 0) {
      // base & changed are the same, take the changed value and skip base.
      job->keys[jx]   = strdup(changed_keys[cx]);
      job->values[jx] = strdup(changed_vals[cx]);
      bx++; cx++; jx++;

    } else if (cmp < 0) {
      if (strcmp(base->keys[bx], missing[mx]) == 0) {
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


void cram_read_job(const cram_file_t *file, size_t offset,
                   cram_job_t *base, cram_job_t *job) {
  // num_procs
  job->num_procs = cram_read_int(file, offset);
  cram_skip_int(file, &offset);

  // working directory
  job->working_dir = cram_read_string(file, offset);
  cram_skip_string(file, &offset);

  // command line arguments
  int num_args = cram_read_int(file, offset);
  job->num_args = num_args;
  job->args = (const char**) malloc(num_args * sizeof(char*));
  cram_skip_int(file, &offset);

  for (int i=0; i < num_args; i++) {
    job->args[i] = cram_read_string(file, offset);
    cram_skip_string(file, &offset);
  }

  // Subtracted environment variables are not in this job but are
  // in the base job.
  int num_subtracted = cram_read_int(file, offset);
  cram_skip_int(file, &offset);

  const char **subtracted_env_vars = malloc(num_subtracted * sizeof(const char*));
  if (num_subtracted) {
    // If there is no base job, then there can't be any subtracted vars.
    if (!base) {
      fprintf(stderr, "Cannot decompress this job without a base job!\n");
      PMPI_Abort(MPI_COMM_WORLD, 1);
    }

    for (int i=0; i < num_subtracted; i++) {
      subtracted_env_vars[i] = cram_read_string(file, offset);
      cram_skip_string(file, &offset);
    }
  }

  // Changed environemnt vars were either added to the base or they're different
  // in job from in base.
  int num_changed = cram_read_int(file, offset);
  cram_skip_int(file, &offset);

  const char **changed_keys = malloc(num_changed * sizeof(const char*));
  const char **changed_vals = malloc(num_changed * sizeof(const char*));
  for (int i=0; i < num_changed; i++) {
    changed_keys[i] = cram_read_string(file, offset);
    cram_skip_string(file, &offset);

    changed_vals[i] = cram_read_string(file, offset);
    cram_skip_string(file, &offset);
  }

  // Apply diffs to the base job and write result into job.
  decompress(base, num_subtracted, subtracted_env_vars,
             num_changed, changed_keys, changed_vals, job);

  // free temporaries
  free_string_array(num_subtracted, subtracted_env_vars);
  free_string_array(num_changed, changed_keys);
  free_string_array(num_changed, changed_vals);
}
