
add_library(cram
  cram_file.c
  cram_serialization.c
  cram_util.c)

include_directories(${MPI_C_INCLUDE_PATH})
