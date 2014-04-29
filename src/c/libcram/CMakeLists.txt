add_wrapped_file(cram.c cram.w)

set(CRAM_SOURCES cram_file.c cram.c)
add_static_and_shared_library(cram SHARED ${CRAM_SOURCES})

include_directories(${MPI_C_INCLUDE_PATH}
  ${PROJECT_SOURCE_DIR}/src/c/libcram)

install(TARGETS cram DESTINATION lib)
