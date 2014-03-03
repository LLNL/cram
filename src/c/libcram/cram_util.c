#include <stdlib.h>
#include <string.h>
#include "cram_util.h"

void free_string_array(int num_elts, const char **arr) {
  for (int i=0; i < num_elts; i++) {
    free((char*)arr[i]);
  }
  free(arr);
}

