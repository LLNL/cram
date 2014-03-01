#ifndef cram_serialization_h
#define cram_serialization_h

#include "cram_defs.h"
struct cram_file_t;
struct cram_job_t;

///
/// Read an integer at a particular offset within the cram file.
///
/// @param[in] file    Cram file to read from.
/// @param[in] offset  Offset in file.
///
EXTERN_C
int cram_read_int(const cram_file_t *file, size_t offset);

///
/// Read a string at a particular offset within the cram file.
///
/// @param[in] file    Cram file to read from.
/// @param[in] offset  Offset in file.
///
EXTERN_C
char *cram_read_string(const cram_file_t *file, size_t offset);

///
/// Move the supplied offset past an integer within a cram file.
///
/// @param[in]    file    Cram file to read from.
/// @param[inout] offset  Offset to read and advance.
///
EXTERN_C
void cram_skip_int(const cram_file_t *file, size_t *offset);

///
/// Move the supplied offset past a string within a cram file.
///
/// @param[in]    file    Cram file to read from.
/// @param[inout] offset  Offset to read and advance.
///
EXTERN_C
void cram_skip_string(const cram_file_t *file, size_t *offset);

///
/// Read an entire job record out of a cram file, starting at the supplied
/// offset.
///
/// For the first job in a cram file, supply NULL for base.  Subsequent jobs
/// have their environment compressed by comparing it to the first job's
/// environment.  For these jobs, pass a reference to the base job so that they
/// can be created by applying differences to the base.
///
/// @param[in]  file    Cram file to read from.
/// @param[in]  offset  Offset in file.
/// @param[in]  base    First job in the cram file, or NULL if we're reading it.
/// @param[out] job     Offset in file.
///
EXTERN_C
void cram_read_job(const cram_file_t *file, size_t offset,
                   const cram_job_t *base, cram_job_t *job);

///
/// Move the supplied offset past a job in a cram file.
///
/// @param[in]    file    Cram file to read from.
/// @param[inout] offset  Offset to read and advance.
///
EXTERN_C
void cram_skip_job(const cram_file_t *file, size_t *offset);

#endif // cram_serialization_h
