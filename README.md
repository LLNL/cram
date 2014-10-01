Cram
=========================
by Todd Gamblin, [tgamblin@llnl.gov](mailto:tgamblin@llnl.gov)

Cram runs many small MPI jobs inside one large MPI job.

Released under the GNU LGPL, `LLNL-CODE-661100`.  See the `LICENSE`
file for details.

### Contributors
John Gyllenhaal, [gyllen@llnl.gov](mailto:gyllen@llnl.gov)


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
    cram pack -f cram.job -n 1 my_mpi_application input.2.txt
    ...
    cram pack -f cram.job -n 1 my_mpi_application input.1048576.txt

This packs all those job invocations into a file called `cram.job`,
and saves them for one big submission.

To use the file, either:
   * Link your application with `libcram.a` if you are using C/C++.
   * Link your application with `libfcram.a` if you are using Fortran.

Then, run your job like this:

    env CRAM_FILE=/path/to/cram.job srun -n 1048576 my_mpi_application

That's it. The job launches, it splits itself into a million pieces.
You'll see some output like this from your srun invocation if Cram
is running properly:

    ===========================================================
     This job is running with Cram.

     Splitting this MPI job into 1048576 jobs.
     This will use  total processes.

     Successfully set up job:
       Job broadcast:   0.005161 sec
       MPI_Comm_split:  0.001973 sec
       Job broadcast:   0.078768 sec
       File open:       0.700172 sec
      --------------------------------------
       Total:           0.786074 sec

    ===========================================================

Each runs independently of the others, generates its own output
and error files, then terminates as it normally would.
All you need to make sure of is that the final, large submitted job
has at least as many processes as all the small jobs combined.

When the job completes, you should see output files like this:

    cram.0.out   cram.1.err    cram.3.out   cram.4.err
    cram.0.err   cram.2.out    cram.3.err   cram.5.out   ... etc ...
    cram.1.out   cram.2.err    cram.4.out   cram.5.err

If the jobs in your cram file had different working directories, then
these files will appear in the working directories.

**NOTE:** By default, only rank 0 in each job in your cram file will write
to the output and error files.  If you don't like this, Cram has other
options for output handling.  See **Output Options** below for more on this.



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

If you're on a machine where Cram isn't installed yet, see
**Build & Install** below to build your own.

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

#### 1. Summary mode

`cram info <cramfile>` will show you a summary of the cram file, e.g.:

    $ cram info test-cram.job
    Name:            test-cram.job
    Number of Jobs:              3
    Total Procs:                82
    Cram version:                1

    Job command lines:
        0     35 procs    my_app foo bar 2 2 4
        1     35 procs    my_app foo bar 2 4 2
        2     12 procs    my_app foo bar 4 2 2

#### 2. Job detail

`cram info -j 5 <cramfile>` will show you information about the job
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

#### 3. All data

`cram info -a <cramfile>` will print out all information for all
jobs in the file.  This can be very verbose, so use it carefully.

### cram test

    Usage: cram test [-h] [-l] [-v] [names [names ...]]

Running `cram test` will run Cram's Python test suite.  If you think
something is wrong wtih Cram, use this command to see if it catches
anything.


Packing lots of jobs
-------------------------

If you want to pack millions of jobs, you most likely do NOT want to
call ``cram`` millions of times.  Python starts up very slowly, and
you don't want to pay that cost for each pack invocation.  You can get
around this by writing a simple Python script.  Alongside the ``cram``
script, there is a ``cram-python`` script.  You can use this to make
an executable script like this one:

    #!/usr/bin/env cram-python

    import os
    from cram import *

    cf = CramFile('cram.job', 'w')
    env = os.environ

    # Pack cram invocations.
    # Usage:
    #   cf.pack(<num procs>, <working dir>, <command-line arguments>, <environment>)
    env["MY_SPECIAL_VAR"] = "my_value"
    cf.pack(1, '/home/youruser/ensemble/run-0', ['input.1.txt'], env)
    cf.pack(1, '/home/youruser/ensemble/run-1', ['input.2.txt'], env)
    # ...
    env["MY_SPECIAL_VAR"] = "another_value"
    cf.pack(1, '/home/youruser/ensemble/run-1048576', ['input.1048576.txt'], env)

    cf.close()

This script will create a cram file, just like all those invocations
of ``cram pack`` above, but it will run much faster because it runs in
a single python session.

Here's a more realistic one, for creating a million jobs with
different user-specific scratch directories:

    #!/usr/bin/env cram-python

    import os
    import getpass
    from cram import *

    env  = os.environ
    user = getpass.getuser()

    cf = CramFile('cram.job', 'w')
    for i in xrange(1048576):
        env["SCRATCH_DIR"] = "/p/lscratcha/%s/scratch-%08d" % (user, i)
        args = ["input.%08d" % i]
        cf.pack(1, '/home/%s/ensemble/run-%08d' % (user, i), args, env)
    cf.close()


Output Options
-------------------------
By default, Cram will redirect the `stdout` and `stderr` streams to
`cram.<jobid>.out` and  `cram.<jobid>.err` ONLY for rank 0 of each
cram job.  Other processes have their output and error redirected to
`/dev/null`.  You can control this behavior using the `CRAM_OUTPUT`
environment variable.  The options are:

  * `NONE`: No output.  All processes redirecto to `/dev/null`.
  * `RANK0`: Default behavior.  Rank 0 writes to `cram.<jobid>.out`
     and `cram.<jobid>.err`.  All other processes write to `/dev/null`.
  * `ALL`: All processes write to unique files. e.g., rank 4 in job 1 writes to
    `cram.1.4.out` and `cram.4.1.err`.
  * `SYSTEM`: Ouptut is not redirected and system defaults are used.

To set a particular output mode, set the `CRAM_OUTPUT` environment
variable to one of the above values when you run.  For example, to
make every process write an output and error file, you would run like
this:

    env CRAM_OUTPUT=ALL CRAM_FILE=/path/to/cram.job srun -n 1048576 my_mpi_application


Error reporting
-------------------------
You may notice that in the `NONE` and `RANK0` modes, some processes
don't have a place to report errors.  Cram tries its best to report
errors regardless of the output mode the user has chosen.

In particular, it tries to report non-zero `exit()` calls and
segmentation faults (`SIGSEGV`) even if not every process has an
output file.

To do this, Cram will write error messages to the original error
stream in all modes except `ALL`.  So, if a process dies or exits,
you'll see a message like this print out to the console:

    Rank 1 on cram job 4 died with signal 11.

If you are running in `ALL` mode, Cram will print error messages like
this out to the per-process `cram.<job>.<rank>.err` file.


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

Other notes
-------------------------
This tool would have been called "Clowncar", but it was decided that
it should have a more serious name.  Cram stands for "Clowncar Renamed
to Appease Management".


References
-------------------------

[1] J. Gyllenhaal, T. Gamblin, A. Bertsch, and R. Musselman.
[Enabling High Job Throughput for Uncertainty Quantification on BG/Q](http://spscicomp.org/wordpress/pages/enabling-high-job-throughput-for-uncertainty-quantification-on-bgq/). In _IBM HPC Systems Scientific Computing User Group (ScicomP’14)_, Chicago, IL, 2014.

[2] J. Gyllenhaal, T. Gamblin, A. Bertsch, and R. Musselman. ["Cramming" Sequoia Full of Jobs for Uncertainty Quantification](http://nnsa.energy.gov/sites/default/files/nnsa/09-14-inlinefiles/2014-09-12%20ASC%20eNews%20-%20June%202014.pdf). In _ASC eNews Quarterly Newsletter_. June 2014.
