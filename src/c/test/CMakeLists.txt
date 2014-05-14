include_directories(
  ${PROJECT_SOURCE_DIR}/src/c/libcram
  ${MPI_C_INCLUDE_PATH})

function(add_cram_test exe_name source_file)
  add_executable(${exe_name} ${source_file})
  target_link_libraries(${exe_name} cram ${MPI_C_LIBRARIES})
endfunction()

#
# Cram test executables
#
add_cram_test(cram-cat cram-cat.c)
add_cram_test(cram-read-file-test cram-read-file-test.c)
add_cram_test(cram-test cram-test.c)
install(TARGETS cram-test cram-read-file-test DESTINATION libexec/cram)

# This test runs a bash script to compare cram-cat output with cram info output.
add_test(cram-io-test
  ${PROJECT_SOURCE_DIR}/src/c/test/cram-io-test.sh ${PROJECT_SOURCE_DIR}/bin/cram cram-cat)
