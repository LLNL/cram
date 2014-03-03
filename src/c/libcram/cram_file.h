#ifndef cram_cram_file_h
#define cram_cram_file_h

#include <mpi.h>

#include <stdlib.h>
#include <stdbool.h>
#include "cram_util.h"

///
/// cram_file_t is used to read in and broadcast raw cram file data.
///
struct cram_file_t {
  int num_jobs;      //!< Total number of jobs in cram file
  int total_procs;   //!< Total number of processes in all jobs.
  int version;       //!< Version of cram that wrote this file.

  int fd;            //!< Unix file descriptor for mapped file.
  char *data;        //!< Pointer to mapped/bcast cram file data.
  size_t bytes;      //!< number of bytes allocated for data.
  bool mapped;       //!< 1 if data is mapped, 0 if it is malloc'd.
};
typedef struct cram_file_t cram_file_t;


///
/// Represents a single job in a cram file.  This can be extracted from a
/// cram_file using cram_file_find_job.
///
struct cram_job_t {
  int num_procs;            //!< Number of processes in this job.
  int job_id;               //!< Index of this job within its cram file.
  const char *working_dir;  //!< Working directory to use for job

  int num_args;             //!< Number of command line arguments
  const char **args;        //!< Array of arguments.

  int num_env_vars;         //!< Number of environment variables.
  const char **keys;        //!< Array of keys of length <num_env_vars>
  const char **values;      //!< Array of corresponding values
};
typedef struct cram_job_t cram_job_t;


///
/// Map a cram file into memory and write it into the supplied
/// struct.  This is a local operation.
///
/// @param[in]  filename   Name of file to open
/// @param[out] file       Descriptor for the mapped cram file.
///
/// @return true if the map was successful, false otherwise.
///
EXTERN_C
bool cram_file_map(const char *filename, cram_file_t *file);


///
/// Broadcast a local cram file to all processes on a communicator.
/// This is a collective operation.
///
/// @param[inout] file   Broadcast file buffer.
/// @param[in]    root   Root rank of the broadcast.
/// @param[in]    comm   Communicator on which file should be bcast.
///
EXTERN_C
void cram_file_bcast(cram_file_t *file, int root, MPI_Comm comm);


///
/// Map a cram file into memory and bcast it to all processes on the supplied
/// communicator. This is a combination of cram_file_map and cram_file_bcast.
///
/// @param[in]    filename   Name of file to open
/// @param[inout] file       Descriptor for the mapped cram file.
/// @param[in]    root       Root rank for the bcast
/// @param[in]    comm       Communicator to bcast on.
///
EXTERN_C
void cram_file_map_bcast(const char *filename, cram_file_t *file, int root,
                         MPI_Comm comm);


///
/// Free any resources (i.e. memory) associated with the cram file.
/// After calling, the file is invalid and should no longer be accessed.
///
/// This is not collective and can be done at any time after cram_file_open.
///
/// @param[in] file   Cram file to close.
///
EXTERN_C
void cram_file_close(const cram_file_t *file);


///
/// Given a process rank on a larger communicators (e.g., MPI_COMM_WORLD), find
/// the job that rank should be a part of and write it out.
///
/// Ranks are currently allocated to jobs in order.  i.e. if job 0 in the cram
/// file has 64 processes, rank 7 will be the 7th process in that job.  If job 1
/// has 16 processes, global rank 67 will be rank 2 in job 1, and so on.
///
/// This is not collective and does a linear local search of the cram file.
///
/// @param[in]   Cram file containing all job descriptors.
/// @param[in]   Rank of this process on MPI_COMM_WORLD.
/// @param[out]  Job that this rank will be a part of.
///
EXTERN_C
void cram_file_find_job(const cram_file_t *file, int rank, cram_job_t *job);


///
/// Write out entire contents of cram file to the supplied file descriptor.
///
EXTERN_C
void cram_cat(const cram_file_t *file);


///
/// Deallocate all memory for a job output by cram_file_find_job.
///
EXTERN_C
void cram_job_free(cram_job_t *job);



#endif // cram_cram_file_h
