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
from tempfile import mktemp
import unittest
import random
from contextlib import contextmanager, closing

import cram.cramfile as cramfile
from cram.cramfile import CramFile, Job

many_jobs = 4096

@contextmanager
def tempfile():
    tmp = mktemp('.tmp', 'cramfile-test-')
    yield tmp
    os.unlink(tmp)


def random_jobs(num_jobs):
    """Generate num_jobs random jobs.  The jobs' environments and command
       line arguments will differ slightly"""
    jobs = []
    for i in range(num_jobs):
        # randomly construct a working environment
        num_procs = random.choice((1,2,4,8,16))
        working_dir = '/path/to/run-%d-%d' % (num_procs, i)
        args = ['-n', str(num_procs), 'foo', 'bar', 'baz', str(i)]

        env = os.environ.copy()
        env['WORKING_DIR'] = working_dir
        env['INDEX'] = str(i)
        if i == 0:
            if 'PATH' not in env:
                env['PATH'] = "this/is/a:/fake/path"
        elif random.choice((0,1,2)):
            del env['PATH']

        jobs.append(Job(num_procs, working_dir, args, env))

    return jobs


class CramFileTest(unittest.TestCase):

    def test_open_for_write(self):
        """Test writing out a header and reading it back in."""
        with tempfile() as tmp:
            with closing(CramFile(tmp, 'w')) as cf:
                self.assertEqual(cf.num_jobs, 0)
                self.assertEqual(cf.version, cramfile._version)

            with closing(CramFile(tmp, 'r')) as cf:
                self.assertTrue(cf.num_jobs == 0)


    def check_write_single_record(self, mode):
        """Test that packing a single, simple job record works in both write
           mode and append mode."""
        num_procs = 64
        working_dir = '/foo/bar/baz'
        args = ['foo', 'bar', 'baz']
        env = { 'foo' : 'bar',
                'bar' : 'baz',
                'baz' : 'quux' }

        with tempfile() as tmp:
            with closing(CramFile(tmp, mode)) as cf:
                cf.pack(Job(num_procs, working_dir, args, env))
                self.assertEqual(cf.num_jobs, 1)
                self.assertEqual(cf.num_procs, 64)

            with closing(CramFile(tmp, 'r')) as cf:
                self.assertEqual(cf.num_jobs, 1)
                self.assertEqual(cf.num_procs, 64)

                job = next(iter(cf))
                self.assertEqual(num_procs,   job.num_procs)
                self.assertEqual(working_dir, job.working_dir)
                self.assertEqual(args,        job.args)
                self.assertEqual(env,         job.env)


    def test_write_single_record(self):
        self.check_write_single_record('w')


    def test_write_single_record(self):
        self.check_write_single_record('a')


    def test_write_a_lot(self):
        jobs = random_jobs(many_jobs)
        total_procs = 0

        with tempfile() as tmp:
            with closing(CramFile(tmp, 'w')) as cf:
                for job in jobs:
                    cf.pack(job)
                    total_procs += job.num_procs

            with closing(CramFile(tmp, 'r')) as cf:
                self.assertEqual(many_jobs, cf.num_jobs)
                self.assertEqual(total_procs, cf.num_procs)
                self.assertListEqual(jobs, [j for j in cf])


    def test_append_a_lot(self):
        jobs = random_jobs(many_jobs)
        total_procs = 0

        with tempfile() as tmp:
            for job in jobs:
                with closing(CramFile(tmp, 'a')) as cf:
                    cf.pack(job)
                total_procs += job.num_procs

            with closing(CramFile(tmp, 'r')) as cf:
                self.assertEqual(many_jobs, cf.num_jobs)
                self.assertEqual(total_procs, cf.num_procs)
                self.assertListEqual(jobs, [j for j in cf])
