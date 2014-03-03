project(Cram C)
cmake_minimum_required(VERSION 2.8)

# Use Cmake's find_mpi
find_package(MPI REQUIRED)

# Enable CTest and add Cram's python unit tests to the suite
enable_testing()
add_test(cram-python-tests ${PROJECT_SOURCE_DIR}/bin/cram test)

# Everything that needs to be built is in src.
add_subdirectory(src)
