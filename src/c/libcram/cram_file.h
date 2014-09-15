#ifndef cram_cram_file_h
#define cram_cram_file_h

#include <mpi.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif // __cplusplus

///
/// cram_file_t is used to read in and broadcast raw cram file data.
///
struct cram_file_t {
  int num_jobs;            //!< Total number of jobs in cram file
  int total_procs;         //!< Total number of processes in all jobs.
  int version;             //!< Version of cram that wrote this file.
  int max_job_size;        //!< Size of largest job record in this file.
  FILE *fd;                //!< C file pointer for the cram file.

  int cur_job_record_size; //!< Size of the current job record
  int cur_job_procs;       //!< Number of proceses in the current job.
  int cur_job_id;          //!< Id of the current job.
};
typedef struct cram_file_t cram_file_t;


///
/// Represents a single job in a cram file.  This can be extracted from a
/// cram_file using cram_file_find_job.
///
struct cram_job_t {
  int num_procs;            //!< Number of processes in this job.
  const char *working_dir;  //!< Working directory to use for job

  int num_args;             //!< Number of command line arguments
  const char **args;        //!< Array of arguments.

  int num_env_vars;         //!< Number of environment variables.
  const char **keys;        //!< Array of keys of length <num_env_vars>
  const char **values;      //!< Array of corresponding values
};
typedef struct cram_job_t cram_job_t;


///
/// Open a cram file into memory and read its header information
/// into the supplied struct.  This is a local operation.
///
/// @param[in]  filename   Name of file to open
/// @param[out] file       Descriptor for the opened cram file.
///
/// @return true if successful, false otherwise.
///
EXTERN_C
bool cram_file_open(const char *filename, cram_file_t *file);


///
/// Free buffers and files associated with the cram file object.
/// After calling, the file is invalid and should no longer be accessed.
///
/// This is not collective and can be done at any time after cram_file_open.
///
/// @param[in] file   Cram file to close.
///
EXTERN_C
void cram_file_close(const cram_file_t *file);


///
/// Whether the cram file has remaining job records to read.
///
/// @param[in] file   A cram file.
///
bool cram_file_has_more_jobs(const cram_file_t *file);


///
/// Read the next job into the job_record buffer.  Metadata about
/// the job can be found in the file buffer after this call.
///
/// Return true if successful, false on error.
///
/// @param[in]    file        Cram file to advance.
/// @param[out]   job_record  Buffer to store uncompressed bytes in.
///
/// Job record should be of size file->max_job_record.
///
bool cram_file_next_job(cram_file_t *file, char *job_record);


///
/// Broadcast a local cram file to all processes on a communicator.
/// This is a collective operation.
///
/// @param[in]  file   File to broadcast jobs from.  Should be newly opened.
/// @param[in]  root   Rank where the file is valid (i.e. root of bcast).
/// @param[out] job    The job this process should execute.
/// @param[out] id     Unique id for this process's job (use it to split world)
/// @param[in]  comm   Communicator on which file should be bcast.
///
EXTERN_C
void cram_file_bcast_jobs(cram_file_t *file, int root, cram_job_t *job, int *id,
                          MPI_Comm comm);


///
/// Write out entire contents of cram file to the supplied file descriptor.
/// After this operation the cram file is completely read.
///
/// @param[in] file   A cram file.
///
EXTERN_C
void cram_file_cat(cram_file_t *file);


///
/// Decompress raw bytes from a job record into a cram_job_decompress.
///
/// For the first job in a cram file, supply NULL for base.  Subsequent jobs
/// have their environment compressed by comparing it to the first job's
/// environment.  For these jobs, pass in a pointer to the base job so that
/// this function can apply differences to the first job.
///
/// @param[in]  job_record  Compressed job record from a cram file.
/// @param[in]  offset      Offset in file.
/// @param[in]  base        First job in the cram file.  Pass NULL to
///                         read the first job out of the file.
///
EXTERN_C
void cram_job_decompress(const char *job_record,
                         const cram_job_t *base, cram_job_t *job);


///
/// Set up cram environment based on the supplied cram job.
/// This will:
/// 1. Change working directory to the job's working directory.
/// 2. Munge command line arguments to be equal to those of the job.
/// 3. Set environment variables per those defined in the job.
///
EXTERN_C
void cram_job_setup(const cram_job_t *job, int *argc, const char ***argv);


///
/// Print metadata for a cram job.  Includes:
///  1. Number of processes
///  2. Working dir
///  3. Arguments
///  4. Environment
///
EXTERN_C
void cram_job_print(const cram_job_t *job);


///
/// Deep copy one cram job into another.
///
/// @param[in]   src    Source job to copy from
/// @param[out]  dest   Destination job to copy to
///
EXTERN_C
void cram_job_copy(const cram_job_t *src, cram_job_t *dest);


///
/// Deallocate all memory for a job output by cram_file_find_job.
///
EXTERN_C
void cram_job_free(cram_job_t *job);



#endif // cram_cram_file_h
