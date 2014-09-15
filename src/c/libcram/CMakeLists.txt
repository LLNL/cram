
#
# This defines the C cram library
#
add_wrapped_file(cram.c cram.w)
set(CRAM_SOURCES
  cram.c
  cram_file.c)
add_static_and_shared_library(cram  ${CRAM_SOURCES})

#
# This build the Fortran cram library, with fortran arg handling and
# fortran MPI wrappers.
#
add_wrapped_file(cram_fortran.c cram.w -f -i pmpi_init)
set(CRAM_FORTRAN_SOURCES
  cram_fortran.c
  cram_fargs.c
  cram_file.c)
add_library(fcram STATIC ${CRAM_FORTRAN_SOURCES})

#
# This command post-processes the fortran library so that the various
# iargc/getarg functions will override compiler intrinsics
#
# Add support for additional compilers here by adding their getarg and
# iargc bindings.
#
add_custom_command(TARGET fcram POST_BUILD
  COMMAND 	objcopy
            --redefine-sym getarg_gnu=_gfortran_getarg_i4@@GFORTRAN_1.0
            --redefine-sym iargc_gnu=_gfortran_iargc@@GFORTRAN_1.0
            --redefine-sym getarg_xl=getarg@@XLF_1.0
            --redefine-sym iargc_xl=iargc@@XLF_1.0
             $<TARGET_FILE:fcram> tmp.a
  COMMAND  mv tmp.a $<TARGET_FILE:fcram>)

include_directories(${MPI_C_INCLUDE_PATH}
  ${PROJECT_SOURCE_DIR}/src/c/libcram)

install(TARGETS cram cram_static fcram DESTINATION lib)

