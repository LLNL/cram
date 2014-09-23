#!/usr/bin/env cram-python
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
import sys
from tempfile import mktemp
from contextlib import closing

from cram import *
from cram.test.cramfile import tempfile

#
# Set up some test jobs.
#
env = os.environ
jobs = [
    Job(2,   '/home/gamblin2/ensemble/run.00', [ 'arg1', 'arg2', 'arg3' ], env.copy()),
    Job(4,   '/home/gamblin2/ensemble/run.01', ['a', 'b', 'c'], env.copy()),
    Job(64,  '/home/gamblin2/ensemble/run.02', ['d', 'e', 'f'], env.copy()),
    Job(128, '/home/gamblin2/ensemble/run.03', ['g', 'h', 'i'], env.copy()),
    Job(2,   '/home/gamblin2/ensemble/run.04', ['j', 'k', 'l'], env.copy()),
    Job(1,   '/home/gamblin2/ensemble/run.05', ['m', 'n', 'o'], env.copy()),
    Job(3,   '/home/gamblin2/ensemble/run.06', ['p', 'q', 'r'], env.copy())]

jobs[0].env['SPECIAL_VALUE']  = "foo"
jobs[3].env['SPECIAL_VALUE2'] = "bar"
jobs[5].env['SPECIAL_VALUE3'] = "baz"


with tempfile() as file_name:
    # Make a test cram file.
    with closing(CramFile(file_name, 'w')) as cf:
        for job in jobs:
            cf.pack(job)

    # Verify the test cram file.
    with closing(CramFile(file_name, 'r')) as cf:
        for i, job in enumerate(cf):
            if jobs[i] != job:
                print "ERROR: validation failed."
                print "Expected: %s" % jobs[i]
                print "Got:      %s" % cf
                sys.exit(1)
