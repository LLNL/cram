#ifndef cram_defs_h
#define cram_defs_h

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif // __cplusplus

inline void free_string_array(int num_elts, const char **arr) {
  for (int i=0; i < num_elts; i++) {
    free((char*)arr[i]);
  }
  free(arr);
}


#endif // cram_defs_h
