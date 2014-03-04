Cram
=========================

Runs many small MPI jobs inside one large MPI job.

by Todd Gamblin, [tgamblin@llnl.gov](mailto:tgamblin@llnl.gov)


Overview
-------------------------

Suppose you have a supercomputer with 1 million cores, and you want to
run 1 million 1-process jobs.  If your resource manager is scalable,
that is most likely an easy thing to do.  You might just submit 1
million jobs:

    srun -n 1 my_mpi_application input.1.txt
    srun -n 1 my_mpi_application input.2.txt
    ...
    srun -n 1 my_mpi_application input.1048576.txt

Unfortunately, for some resource managers, that requires you to have 1
million `srun` processes running on the front-end node of your fancy
supercomputer, which quickly blows out either memory or the front-end
OS's process limit.

Cram fixes this problem by allowing you to lump all of those jobs into
one large submission.  You use Cram like this:

    cram pack -f cram.job -n 1 my_mpi_application input.1.txt
    cram pack -f cram.job -n 2 my_mpi_application input.2.txt
    ...
    cram pack -f cram.job -n 1048576 my_mpi_application input.1048576.txt

This packs all those job invocations into a file called `cram.job`,
and saves them for one big submission.  To use the file, you link your
application with `libcram.a`. Finally, you run your job like this:

    env CRAM_FILE=/path/to/cram.job srun -n 1048576 my_mpi_application

That's it. The job launches, it splits itself into a million pieces,
and each runs independently of the others, generates its own output
and error files, and terminates like it normally would.
All you need to make sure of is that the final, large submitted job
has at least as many processes as all the small jobs combined.

When the job completes, you should see output files like this:

    cram.0.out   cram.1.err    cram.3.out   cram.4.err
    cram.0.err   cram.2.out    cram.3.err   cram.5.out   ... etc ...
    cram.1.out   cram.2.err    cram.4.out   cram.5.err

If the jobs each had different working directories, then these files will
appear in the working directories.


Setup
-------------------------

To run Cram, you need Python 2.7.  Python and cram are available
through dotkits:

    use python-2.7.3
    use cram

Once you use cram, the `CRAM_HOME` environment variable is set for
you.  You can use this to locate `libcram.a`:

    $ ls $CRAM_HOME/lib
    libcram.a  python2.7/

That's the library you want to link your application with before you
submit your large job.

If you're on a machine where Cram isn't installed yet, see [Build &
Install](#Build & Install) to build your own.

Command Reference
-------------------------

Nearly everything in Cram is done through the `cram` command.  Like
`svn`, `git`, and various other UNIX commands, `cram` is made up of
many subcommands.  They're documented here.

### cram pack

The most basic cram command -- this packs command line invocations
into a file for batch submission.

    Usage: cram pack [-h] -n NPROCS -f FILE ...

* `-n NPROCS`
  Number of processes this job should run with.

* `-f FILE`
  File to store command invocation in.  Default is 'cram.job'

* `...`
  Command line of job to run, including the executable.  All command lines
  packed into one cram file **must** use the same executable.

### cram info

This command is useful if you are trying to debug your run and you
need to see what you actually submitted.

    Usage: cram info [-h] [-a] [-j JOB] [-n NUM_LINES] cramfile

There are three modes:

1. `cram info <cramfile>` will show you a summary of the cram file, e.g.:

       $ cram info test-cram.job
       Name:            test-cram.job
       Number of Jobs:              3
       Total Procs:                82
       Cram version:                1
       
       First 10 job command lines:
           0     35 procs    my_app foo bar 2 2 4
           1     35 procs    my_app foo bar 2 4 2
           2     12 procs    my_app foo bar 4 2 2

2. `cram info -j 5 <cramfile>` will show you information about the job
   with index 5 inside the cram file.  That includes:

    1. Size (number of processes) of the job
    2. Its working directory
    3. Its command line
    4. All of its environment variables.

   Example:
       cram info -j 2 test-cram.job
       Job 2:
         Num procs: 12
         Working dir: /g/g21/gamblin2/test
         Arguments:
             my_app foo bar 4 2 2
         Environment:
             'LOG_DIRECTORY' : '/p/lscratcha/my_app/output'
             ... etc ...

3. `cram info -a <cramfile>` will print out all information for all
   jobs in the file.  This can be very verbose, so use it carefully.

### cram test

    Usage: cram test [-h] [-l] [-v] [names [names ...]]

Running `cram test` will run Cram's Python test suite.  If you think
something is wrong wtih Cram, use this command to see if it catches
anything.


Build & Install
-------------------------

To build your own version of Cram, you need [CMake](http://cmake.org).
On LLNL machines, you can get cmake by typing `use cmake`.

### Basic build

To build, run this in the Cram root directory:

    cmake -D CMAKE_INSTALL_PREFIX=/path/to/install .
    make
    make install

`CMAKE_INSTALL_PREFIX` is the location you want to install Cram, and
`.` is the path to the top-level Cram source directory.

CMake supports out-of-source builds, so if you like to have separate
build directories for different machines, you can run the above
command in a subdirectory.  Just remember to change the path to source
to `..` or similar when you do this, e.g.:

    mkdir  $SYS_TYPE && cd $SYS_TYPE
    cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..

### Cross-compiling

On Blue Gene/Q, you also need to supply `-DCMAKE_TOOLCHAIN_FILE` to
get cross compilation working:

    cmake -DCMAKE_INSTALL_PREFIX=/path/to/install \
          -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain/BlueGeneQ-xl.cmake .
    make
    make install

If you want ot build with the GNU compilers, there is a
`BlueGeneQ-gnu.cmake` toolchain file too.  This package supports
parallel make, so you are free to use make -j<jobs> if you like.
