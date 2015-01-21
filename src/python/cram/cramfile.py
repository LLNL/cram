##############################################################################
# Copyright (c) 2014, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory.
#
# This file is part of Cram.
# Written by Todd Gamblin, tgamblin@llnl.gov, All rights reserved.
# LLNL-CODE-661100
#
# For details, see https://github.com/scalability-llnl/cram.
# Please also see the LICENSE file for our notice and the LGPL.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License (as published by
# the Free Software Foundation) version 2.1 dated February 1999.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and
# conditions of the GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
##############################################################################
"""
This module defines the CramFile class, which is used to store many of
job invocations to be run later in the same MPI job.  A job invocation
is simply the context needed to run a single MPI job: number of
processes, working directory, command-line arguments, and environment.

The format is designed to be read easily from a C program, so it only
writes simple ints and strings.  Ints are all unsigned. Strings start
with an integer length, after which all the characters are written out.

CramFiles use a very simple form of compression to store each job's
environment, since the environment can grow to be very large and is
usually quite redundant.  For each job appended to a CramFile after
the first, we compare its environment to the first job's environment,
and we only store the differences.

We could potentially get more compression out of comparing each
environment to its successor, but that would mean that you'd need to
read all preceding jobs to decode one.  We wanted a format that would
allow scattering jobs very quickly to many MPI processes.

Sample usage:

  cf = CramFile('file.cram', 'w')
  cf.pack(Job(64,             # number of processes
              os.getcwd(),    # working dir, as a string
              sys.argv[1:],   # cmdline args, as a list
              os.env))        # environment, as a dict.
  cf.close()

To read from a CramFile, use len and iterate:

  cf = CramFile('file.cram')
  num_jobs = len(cf)
  for job in cf:
      # do something with job
  cf.close()

Like regular files, CramFiles do not support indexing.  To do that,
read all jobs into a list first, then index the list.

Here is the CramFile format.  '*' below means that the section can be
repeated a variable number of times.

Type       Name
========================================================================
Header
------------------------------------------------------------------------
int(4)       0x6372616d ('cram')
int(4)       Version
int(4)       # of jobs
int(4)       # of processes
int(4)       Size of max job record in this file

* Job records
------------------------------------------------------------------------
  int(4)     Size of job record in bytes
  int(4)     Number of processes
  str        Working dir

  int(4)     Number of command line arguments
   * str       Command line arguments, in original order

  int(4)     Number of subtracted env var names (0 for first record)
   * str       Subtracted env vars in sorted order.
  int(4)     Number of added or changed env vars
   * str      Names of added/changed var
   * str      Corresponding value

Env vars are stored alternating keys and values, in sorted order by key.
========================================================================
"""
import os
import re

from collections import defaultdict
from contextlib import contextmanager, closing

from cram.serialization import *
import llnl.util.tty as tty

# Magic number goes at beginning of file.
_magic = 0x6372616d

# Increment this when the binary format changes (hopefully infrequent)
_version = 2

# Offsets of file header fields
_magic_offset   = 0
_version_offset = 4
_njobs_offset   = 8
_nprocs_offset  = 12
_max_job_offset = 16

# Default name for cram executable.
USE_APP_EXE = "<exe>"


@contextmanager
def save_position(stream):
    """Context for doing something while saving the current position in a
       file."""
    pos = stream.tell()
    yield
    stream.seek(pos)


def compress(base, modified):
    """Compare two dicts, modified and base, and return a diff consisting
       of two parts:

       1. missing: set of keys in base but not in modified
       2. changed: dict of key:value pairs either in modified but not in base,
          or that have different values in base and modified.

       This simple compression scheme is used to compress the environment
       in cramfiles, since that takes up the bulk of the space.
    """
    missing = set(base.keys()).difference(modified)
    changed = { k:v for k,v in modified.items()
                if k not in base or base[k] != v }
    return missing, changed


def decompress(base, missing, changed):
    """Given the base dict and the output of compress(), reconstruct the
       modified dict."""
    d = base.copy()
    for k in missing:
        del d[k]
    d.update(changed)
    return d


class Job(object):
    """Simple class to represent one job invocation packed into a cramfile.
       This contains all environmental context needed to launch the job
       later from within MPI.
    """
    def __init__(self, num_procs, working_dir, args, env):
        """Construct a new Job object.

        Arguments:
        num_procs   -- number of processes to run on.
        working_dir -- path to working directory for job.
        args        -- sequence of arguments, INCLUDING the executable name.
        env         -- dict containng environment.

        """

        self.num_procs = num_procs
        self.working_dir = working_dir

        # Be lenient about the args.  Let the user pass a string if he
        # wants, and split it like the shell would do.
        if isinstance(args, basestring):
            args = re.split(r'\s+', args)
        self.args = args

        self.env = env


    def __eq__(self, other):
        return (self.num_procs == other.num_procs and
                self.working_dir == other.working_dir and
                self.args == other.args and
                self.env == other.env)


    def __ne__(self, other):
        return not (self == other)


class CramFile(object):
    """A CramFile compactly stores a number of Jobs, so that they can
       later be run within the same MPI job by cram.
    """
    def __init__(self, filename, mode='r'):
        """The CramFile constructor functions much like open().

           The constructor takes a filename and an I/O mode, which can
           be 'r', 'w', or 'a', for read, write, or append.

           Opening a CramFile for writing will create a file with a
           simple header containing no jobs.
        """
        # Save the first job from the file.
        self.first_job = None

        self.mode = mode
        if mode not in ('r', 'w', 'a'):
            raise ValueError("Mode must be 'r', 'w', or 'a'.")

        if mode == 'r':
            if not os.path.exists(filename) or os.path.isdir(filename):
                tty.die("No such file: %s" % filename)

            self.stream = open(filename, 'rb')
            self._read_header()

        elif mode == 'w' or (mode == 'a' and not os.path.exists(filename)):
            self.stream = open(filename, 'wb')
            self.version = _version
            self.num_jobs = 0
            self.num_procs = 0
            self.max_job_size = 0
            self._write_header()

        elif mode == 'a':
            self.stream = open(filename, 'rb+')
            self._read_header()
            self.stream.seek(0, os.SEEK_END)


    def _read_header(self):
        """Jump to the beginning of the file and read the header.  The cursor
           will be at the end of the header on completion, so you will need to
           save it if you want to end up somewhere else."""
        self.stream.seek(0)

        magic = read_int(self.stream, 4)
        if magic != _magic:
            raise IOError("%s is not a Cramfile!")

        self.version = read_int(self.stream, 4)
        if self.version != _version:
            raise IOError(
                "Version mismatch: File has version %s, but this is version %s"
                % (self.version, _version))

        self.num_jobs = read_int(self.stream, 4)
        self.num_procs = read_int(self.stream, 4)
        self.max_job_size = read_int(self.stream, 4)

        # read in the first job automatically if it is there, since
        # it is used for compression of subsequent jobs.
        if self.num_jobs > 0:
            self._read_job()


    def _write_header(self):
        """Jump to the beginning of the file and write the header."""
        self.stream.seek(0)
        write_int(self.stream, _magic, 4)
        write_int(self.stream, self.version, 4)
        write_int(self.stream, self.num_jobs, 4)
        write_int(self.stream, self.num_procs, 4)
        write_int(self.stream, self.max_job_size, 4)


    def _pack(self, job):
        """Appends a job to a cram file, compressing the environment in the
           process."""
        if self.mode == 'r':
            raise IOError("Cannot pack into CramFile opened for reading.")

        # Save offset and write 0 for size to start with
        start_offset = self.stream.tell()
        size = 0
        write_int(self.stream, 0, 4)

        # Number of processes
        size += write_int(self.stream, job.num_procs, 4)

        # Working directory
        size += write_string(self.stream, job.working_dir)

        # Command line arguments
        size += write_int(self.stream, len(job.args), 4)
        for arg in job.args:
            size += write_string(self.stream, arg)

        # Compress using first dict
        missing, changed = compress(
            self.first_job.env if self.first_job else {}, job.env)

        # Subtracted env var names
        size += write_int(self.stream, len(missing), 4)
        for key in sorted(missing):
            size += write_string(self.stream, key)

        # Changed environment variables
        size += write_int(self.stream, len(changed), 4)
        for key in sorted(changed.keys()):
            size += write_string(self.stream, key)
            size += write_string(self.stream, changed[key])

        with save_position(self.stream):
            # Update job chunk size
            self.stream.seek(start_offset)
            write_int(self.stream, size, 4)

            # Update total number of jobs in file.
            self.num_jobs += 1
            self.stream.seek(_njobs_offset)
            write_int(self.stream, self.num_jobs, 4)

            # Update total number of processes in all jobs.
            self.num_procs += job.num_procs
            self.stream.seek(_nprocs_offset)
            write_int(self.stream, self.num_procs, 4)

            # Update max job size if necessary
            if size > self.max_job_size:
                self.max_job_size = size
                self.stream.seek(_max_job_offset)
                write_int(self.stream, self.max_job_size, 4)

        # Discard all but hte first job after writing.  This conserves
        # memory when writing cram files.
        if not self.first_job:
            self.first_job = Job(
                job.num_procs, job.working_dir, list(job.args), job.env.copy())


    def pack(self, *args, **kwargs):
        """Pack a Job into a cram file.

        Takes either a Job object, or Job constructor params.
        """
        if len(args) == 1:
            job = args[0]
            if not isinstance(job, Job):
                raise TypeError(
                    "Must pass a Job object, or (nprocs, working_dir, args, env)")
            if kwargs:
                raise TypeError("Cannot pass kwargs with cramfile.pack(Job)")
            self._pack(job)

        elif len(args) == 4:
            nprocs, working_dir, args, env = args

            # Split strings for the caller.
            if isinstance(args, basestring):
                args = re.split(r'\s+', args)

            # By default, cram takes app's exe name.
            exe = kwargs.pop('exe', USE_APP_EXE)
            if kwargs:
                raise ValueError("%s is an invalid keyword arg for this function!"
                                 % next(iter(kwargs.keys())))

            args = [exe] + list(args)
            self._pack(Job(nprocs, working_dir, args, env))


    def _read_job(self):
        """Read the next job out of the CramFile.

           This is an internal method because it's used to load stuff
           that isn't already in memory.  Client code should use
           len(), [], or iterate to read jobs from CramFiles.
        """
        # Size of job record
        job_bytes   = read_int(self.stream, 4)
        start_pos = self.stream.tell()

        # Number of processes
        num_procs   = read_int(self.stream, 4)

        # Working directory
        working_dir = read_string(self.stream)

        # Command line arguments
        num_args    = read_int(self.stream, 4)
        args        = []
        for i in xrange(num_args):
            args.append(read_string(self.stream))

        # Subtracted environment variables
        num_missing = read_int(self.stream, 4)
        missing     = []
        for i in xrange(num_missing):
            missing.append(read_string(self.stream))

        # Changed environment variables
        num_changed = read_int(self.stream, 4)
        changed = {}
        for i in xrange(num_changed):
            key = read_string(self.stream)
            val = read_string(self.stream)
            changed[key] = val

        # validate job record size
        actual_size = self.stream.tell() - start_pos
        if actual_size != job_bytes:
            raise Exception("Cram file job record size is invalid! "+
                            "Expected %d, found %d" % (job_bytes, actual_size))

        # Decompress using first dictionary
        env = decompress(self.first_job.env if self.first_job else {},
                         missing, changed)

        job = Job(num_procs, working_dir, args, env)
        if not self.first_job:
            self.first_job = job
        return job


    def __iter__(self):
        """Iterate over all jobs in the CramFile."""
        if self.mode != 'r':
            raise IOError("Cramfile is not opened for reading.")

        yield self.first_job
        for i in xrange(1, self.num_jobs):
            yield self._read_job()


    def __len__(self):
        """Number of jobs in the file."""
        return self.num_jobs


    def close(self):
        """Close the underlying file stream."""
        self.stream.close()
