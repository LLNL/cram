include_directories(
  ${PROJECT_SOURCE_DIR}/src/c/libcram
  ${MPI_C_INCLUDE_PATH})

function(add_test exe_name source_file)
  add_executable(${exe_name} ${source_file})
  target_link_libraries(${exe_name} cram ${MPI_C_LIBRARIES})
endfunction()

add_test(cram-cat cram-cat.c)
