#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "cram_file.h"


int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: cram-cat <cramfile>\n");
    fprintf(stderr, "  Prints out the entire contents of a cram file "
            "as cram info would.\n");
  }
  const char *filename = argv[1];

  cram_file_t file;
  if (!cram_file_map(filename, &file)) {
    printf("failed to map with errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  printf("Name:%25s\n", filename);
  cram_cat(&file);
}
