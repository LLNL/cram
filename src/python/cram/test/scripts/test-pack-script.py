#!/usr/bin/env cram-python

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
