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
import os
import llnl.util.tty as tty
from llnl.util.filesystem import mkdirp

from cram import *

description = "Generate directory structure and a cram file for a test problem."

def setup_parser(subparser):
    subparser.add_argument("nprocs", type=int, help="Number of processes to run with.")
    subparser.add_argument("job_size", type=int, help="Size of each test job.")
    subparser.add_argument("--print-mem-usage", action='store_true', dest='print_mem',
                           default=False, help="Print memory usage when done.")
    subparser.add_argument("--jobs-per-dir", type=int, dest='jobs_per_dir', default=1024,
                           help="Number of jobs per directory")


def make_test(num_procs, job_size, jobs_per_dir):
    test_dir = "%s/cram-test-outputs/%s/%s" % (
        os.getcwd(), num_procs, job_size)
    mkdirp(test_dir)

    cfname = os.path.join(test_dir, "cram.job")

    cf = CramFile(cfname, 'w')
    for i, rank in enumerate(xrange(0, num_procs, job_size)):
        os.environ["CRAM_JOB_ID"] = str(i)
        args = ['exe', 'foo', 'bar', 'baz', str(i)]

        if i % jobs_per_dir == 0:
            wdir = "%s/wdir.%d" % (test_dir, i / jobs_per_dir)
            mkdirp(wdir)

        cf.pack(job_size, wdir, args, os.environ)

    del os.environ["CRAM_JOB_ID"]
    cf.close()

    return test_dir, cfname


def test_gen(parser, args):
    test_dir, cram_file = make_test(args.nprocs, args.job_size, args.jobs_per_dir)
    tty.msg("Created a test directory:", test_dir)
    tty.msg("And a cram file:", cram_file)
    tty.msg("To check that everything works:",
            "1. Run $CRAM_PREFIX/libexec/cram/cram-test with CRAM_FILE=%s." % cram_file,
            "2. Run cram test-verify on the directory.")
    if args.print_mem:
        import resource
        kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        tty.msg("Memory usage: %d KiB" % kb)


