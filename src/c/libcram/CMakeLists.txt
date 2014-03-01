
add_library(cram
  cram_file.c
  cram_serialization.c)

include_directories(${MPI_C_INCLUDE_PATH})
