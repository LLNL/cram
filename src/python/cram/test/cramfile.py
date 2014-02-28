import os
from  tempfile import mktemp
import unittest
import random
from contextlib import contextmanager, closing

import cram.cramfile as cramfile
from cram.cramfile import CramFile, Job

@contextmanager
def tempfile():
    tmp = mktemp('.tmp', 'cramfile-test-')
    yield tmp
    os.unlink(tmp)


class CramFileTest(unittest.TestCase):

    def test_open_for_write(self):
        """Test writing out a header and reading it back in."""
        with tempfile() as tmp:
            with closing(CramFile(tmp, 'w')) as cf:
                self.assertEqual(cf.num_jobs, 0)
                self.assertEqual(cf.version, cramfile._version)

            with closing(CramFile(tmp, 'r')) as cf:
                self.assertTrue(cf.num_jobs == 0)


    def test_append(self):
        num_procs = 64
        working_dir = '/foo/bar/baz'
        args = ['foo', 'bar', 'baz']
        env = { 'foo' : 'bar',
                'bar' : 'baz',
                'baz' : 'quux' }

        with tempfile() as tmp:
            with closing(CramFile(tmp, 'w')) as cf:
                cf.append(Job(num_procs, working_dir, args, env))
                self.assertEqual(cf.num_jobs, 1)

            with closing(CramFile(tmp, 'r')) as cf:
                self.assertEqual(cf.num_jobs, 1)

                job = cf[0]
                self.assertEqual(num_procs,   job.num_procs)
                self.assertEqual(working_dir, job.working_dir)
                self.assertEqual(args,        job.args)
                self.assertEqual(env,         job.env)


    def test_append_a_lot(self):
        with tempfile() as tmp:
            jobs = []

            with closing(CramFile(tmp, 'w')) as cf:
                for i in range(4096):
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

                    job = Job(num_procs, working_dir, args, env)
                    jobs.append(job)
                    cf.append(job)

            with closing(CramFile(tmp, 'r')) as cf:
                self.assertListEqual(jobs, [j for j in cf])
