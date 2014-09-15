
      program farg_test
      implicit none
      integer argc, i, ierr
      character*64 :: arg
      integer*4 :: iargc

      call mpi_init(ierr)

      argc = iargc()
      do i=1,argc
         call getarg(i, arg)
         write (*, "(a)") arg
      end do

      call mpi_finalize(ierr)
      end program farg_test
