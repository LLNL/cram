project(Cram C)
cmake_minimum_required(VERSION 2.8)

# Required dependencies
find_package(MPI REQUIRED)
find_package(PythonInterp REQUIRED)
if (PYTHON_VERSION_STRING VERSION_LESS "2.7")
  message(FATAL_ERROR "Cram requires Python 2.7 or later.")
endif()

# Use wrap.py wrapper generator
set(WRAP ${PROJECT_SOURCE_DIR}/wrap/wrap.py)
include(${PROJECT_SOURCE_DIR}/wrap/WrapConfig.cmake)

# Enable CTest and add Cram's python unit tests to the suite
enable_testing()
add_test(cram-python-tests ${PROJECT_SOURCE_DIR}/bin/cram test)

# Everything that needs to be built is in src.
add_subdirectory(bin)
add_subdirectory(src)
