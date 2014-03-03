#ifndef cram_util_h
#define cram_util_h

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif // __cplusplus

void free_string_array(int num_elts, const char **arr);

#endif // cram_util_h
