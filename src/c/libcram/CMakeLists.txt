add_wrapped_file(cram.c cram.w)

add_library(cram
  cram_file.c
  cram_serialization.c
  cram_util.c
  cram.c)

include_directories(${MPI_C_INCLUDE_PATH}
  ${PROJECT_SOURCE_DIR}/src/c/libcram)

install(TARGETS cram DESTINATION lib)
