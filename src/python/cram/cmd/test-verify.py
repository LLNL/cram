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
import re
import glob
from contextlib import closing

import llnl.util.tty as tty
from llnl.util.filesystem import mkdirp

from cram import *

description = "Check a test created by cram test-gen."

def setup_parser(subparser):
    subparser.add_argument(
        "directory", type=str, help="Test directory created by cram test-gen.")


def check_file(file_name, *tests):
    """Check a file for some important values and die if they're not
       as expected."""
    results = [None] * len(tests)
    with closing(open(file_name)) as f:
        for line in f:
            count = 0
            for i, (regex, t, e, m) in enumerate(tests):
                match = re.search(regex, line)
                if match:
                    results[i] = t(match.group(1))
                    count += 1

            if count == len(tests):
                break

    for i, (regex, t, expected, msg) in enumerate(tests):
        if results[i] is None or results[i] != expected:
            tty.die(msg % (file_name, expected))


def verify_test(test_dir):
    cfname = os.path.join(test_dir, "cram.job")

    test_dir = test_dir.rstrip('/')
    match = re.search(r'/(\d+)/(\d+)', test_dir)
    if not match:
        tty.die("This directory was not created by cram test-gen")

    num_procs = int(match.group(1))
    job_size  = int(match.group(2))

    tty.msg("Checking test with %d procs and %d procs per job." % (
            num_procs, job_size))

    cf = CramFile(cfname, 'r')
    if not cf.num_procs == num_procs:
        tty.die("Cram file had %d procs but expected %d" % (cf.num_procs, num_procs))
    cf.close()

    os.chdir(test_dir)
    files = glob.glob('*/cram*out')

    short_names = set(os.path.basename(f) for f in files)
    for i, rank in enumerate(xrange(0, num_procs, job_size)):
        name = "cram.%d.out" % i
        if not name in short_names:
            tty.die("Couldn't find this output file: %s.")

    for file_name in files:
        match = re.search(r'cram.(\d+).out', file_name)
        assert(match)
        job_id = int(match.group(1))

        results = check_file(
            file_name,
            # regex with group         type  expected   error msg if not equal to expected
            (r'CRAM_JOB_ID=(\d+)',     int,  job_id,    "%s does not contain CRAM_JOB_ID=%d"),
            (r'foo bar baz (\d+)',     int,  job_id,    "%s does not contain args 'foo bar baz %d'"),
            (r'Job size: +(\d+)',      int,  job_size,  "%s has incorrect job size: %d"),
            (r'Real job size: +(\d+)', int,  num_procs, "%s has incorrect job size: %d"))

def test_verify(parser, args):
    verify_test(args.directory)
    tty.msg("Success! All jobs look ok.")
