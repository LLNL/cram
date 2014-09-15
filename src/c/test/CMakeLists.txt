include_directories(
  ${PROJECT_SOURCE_DIR}/src/c/libcram
  ${MPI_C_INCLUDE_PATH})

function(add_cram_test exe_name source_file)
  add_executable(${exe_name} ${source_file})
  target_link_libraries(${exe_name} cram ${MPI_C_LIBRARIES})
endfunction()

function(add_fcram_test exe_name source_file)
  add_executable(${exe_name} ${source_file})
  target_link_libraries(${exe_name} fcram ${MPI_Fortran_LIBRARIES})

  if (CMAKE_Fortran_COMPILER_ID STREQUAL GNU)
    # Allow multiple defs to override fortran intrinsics
    set_target_properties(${exe_name} PROPERTIES LINK_FLAGS -Wl,-zmuldefs)
  endif()
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


add_fcram_test(print-args-fortran print-args.f)
add_cram_test(print-args-c print-args.c)

add_cram_test(crash-test crash-test.c)
add_cram_test(exit-test crash-test.c)
