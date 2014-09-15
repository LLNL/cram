// -*- c -*-
//
// This file defines MPI wrappers for cram.
//
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
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

//
// Acceptable output modes for Cram.
//
typedef enum {
    cram_output_system,   // Stick with system's original stdout/stderr settings.
    cram_output_none,     // All processes freopen stdout and stderr to /dev/null
    cram_output_rank0,    // Rank 0 in each job opens its own stdout/stderr; others /dev/null
    cram_output_all,      // All ranks in all jobs open their own stdout/stderr
} cram_output_mode_t;


// Global for Cram output mode, set in MPI_Init.
static cram_output_mode_t cram_output_mode = cram_output_rank0;

// Original stderr pointer.  So that we can print last-ditch error messages.
static FILE *original_stderr = NULL;

// Some information about this job.
static int job_id = -1;
static int local_rank = -1;

//
// Gets the output mode from the CRAM_OUTPUT environment variable.
// Possible values are:
//
//   NONE   -> cram_output_none
//   RANK0  -> cram_output_rank0
//   ALL    -> cram_output_all
//
// These map to corresponding cram_output_mode_t values.
//
static cram_output_mode_t get_output_mode() {
  const char *mode = getenv("CRAM_OUTPUT");

  if (!mode || strcasecmp(mode, "rank0") == 0) {
      return cram_output_rank0;

  } else if (strcasecmp(mode, "system") == 0) {
      return cram_output_system;

  } else if (strcasecmp(mode, "none") == 0) {
      return cram_output_none;

  } else if (strcasecmp(mode, "all") == 0) {
      return cram_output_all;
  }
  return cram_output_rank0;
}


//
// Redirect I/O to the supplied output and error files, saving stderr in
// original_stderr (if possible on the particular platform).
//
static void redirect_io(const char *out, const char *err) {
    freopen(out, "w", stdout);

    // If each process has its own output stream, then write errors to the
    // per-process error stream, not to the original error stream.
    if (cram_output_mode == cram_output_all) {
        freopen(err, "w", stderr);
        original_stderr = stderr;
        return;
    }

    // in GLIBC, stdout and stderr are assignable.  Prefer assigning to stdout
    // and stderr if possible, becasue BG/Q does not allow dup, dup2, fcntl, etc.
    // to work on the built-in stderr.  There is therefore not a portable way
    // to do this if we care about BG/Q.
#ifdef __GLIBC__
    original_stderr = stderr;
    stderr = fopen(err, "w");
#else  // not __GLIBC__
    // dup the fd for stderr, underneath libc.  This doesn't work on BG/Q.
    int fd = dup(fileno(stderr));
    freopen(err, "w", stderr);
    original_stderr = fdopen(fd, "a");

    // if the fdopen fails for some reason, just use the new error stream for this.
    if (!original_stderr) {
        original_stderr = stderr;
        if (cram_output_mode == cram_output_rank0) {
            fprintf(stderr, "WARNING: Cram couldn't preserve the original stderr stream while opening a new one.\n");
            fprintf(stderr, "WARNING: You may not receive notifcations of errors if they do not happen on rank 0 in each job.");
        }
    }
#endif // not __GLIBC__
}


//
// Handler for SEGV prints to original stderr to tell the user which process
// died, then exits cleanly.
//
void segv_sigaction(int signal, siginfo_t *si, void *ctx) {
    fprintf(original_stderr, "Rank %d on cram job %d died with signal %d.\n",
            local_rank, job_id, signal);

    // Act like everything is ok.  Nothing to see here...
    int finalized;
    PMPI_Finalized(&finalized);
    if (!finalized) {
        PMPI_Finalize();
    }
    exit(0);
}

//
// Atexit handler that disallows processes exiting with codes other than 0.
// On some systems (BG/Q), this results in the entire MPI job being killed,
// and we'd rather most of our cram jobs live full and productive lives.
//
void on_exit_handler(int err, void *arg) {
    if (err != 0) {
        fprintf(original_stderr, "Rank %d on cram job %d exited with error %d.\n",
                local_rank, job_id, err);

        // Act like everything is ok.  Nothing to see here...
        int finalized;
        PMPI_Finalized(&finalized);
        if (!finalized) {
            PMPI_Finalize();
        }
        exit(0);
    }
}

//
// In a cram run, there are many simulatneous jobs, some of which may fail.
// This function sets up signal handlers and other handlers that attempt to
// keep the whole job from dying when a single process dies.
//
static void setup_crash_handlers() {
    // Set up signal handlers so that SEGV is called.
    struct sigaction sa;
    sa.sa_sigaction = segv_sigaction;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    int err = sigaction(SIGSEGV, &sa, NULL);

    // Register exit handler to mask procs that return with error.
    on_exit(on_exit_handler, NULL);
}


//
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
      fprintf(stderr, "Error: Failed to open cram file '%s'.\n", cram_filename);
      fprintf(stderr, "%s\n", strerror(errno));
      PMPI_Abort(MPI_COMM_WORLD, errno);
    }

    fprintf(stderr,   " Splitting this MPI job into %d jobs.\n", cram_file.num_jobs);
    fprintf(stderr,   " This will use %d total processes.\n", cram_file.total_procs);
  }

  // Receive our job from the root process.
  cram_job_t cram_job;
  double start_time = PMPI_Wtime();
  cram_file_bcast_jobs(&cram_file, 0, &cram_job, &job_id, MPI_COMM_WORLD);
  double bcast_time = PMPI_Wtime();

  // Use the job id to split MPI_COMM_WORLD.
  PMPI_Comm_split(MPI_COMM_WORLD, job_id, rank, &local_world);
  double split_time = PMPI_Wtime();

  // Throw away unneeded ranks.
  if (job_id == -1) {
    PMPI_Barrier(MPI_COMM_WORLD); // matches barrier later.
    PMPI_Finalize();
    exit(0);
  }

  // set up this job's environment based on the job descriptor.
  cram_job_setup(&cram_job, {{0}}, (const char***){{1}});
  PMPI_Comm_rank(local_world, &local_rank);
  double setup_time = PMPI_Wtime();

  cram_output_mode = get_output_mode();
  char out_file_name[1024];
  char err_file_name[1024];

  if (cram_output_mode != cram_output_system) {
      sprintf(out_file_name, "/dev/null");
      sprintf(err_file_name, "/dev/null");

      if (cram_output_mode == cram_output_rank0) {
          // Redirect I/O to a separate file for each cram job.
          // These files will be in the job's working directory.
          if (local_rank == 0) {
              sprintf(out_file_name, "cram.%d.out", job_id);
              sprintf(err_file_name, "cram.%d.err", job_id);
          }

      } else if (cram_output_mode == cram_output_all) {
          sprintf(out_file_name, "cram.%d.%d.out", job_id, local_rank);
          sprintf(err_file_name, "cram.%d.%d.err", job_id, local_rank);
      }

      // don't freopen on root until after printing status.
      if (rank != 0) {
          redirect_io(out_file_name, err_file_name);
      }
  }

  // wait for lots of files to open.
  PMPI_Barrier(MPI_COMM_WORLD);
  double freopen_time = PMPI_Wtime();

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

    if (cram_output_mode != cram_output_system) {
        // reopen *last* on the zero rank.
        redirect_io(out_file_name, err_file_name);
    }

    cram_file_close(&cram_file);
  }

  // Now that I/O is set up, register some handlers for crashes.
  setup_crash_handlers();

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
