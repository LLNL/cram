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
