project(Cram C)
cmake_minimum_required(VERSION 2.8)
enable_testing()

find_package(MPI REQUIRED)

add_subdirectory(src)
