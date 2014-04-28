import os
import re
import shutil
import errno
import getpass
from contextlib import contextmanager, closing

@contextmanager
def working_dir(dirname):
    orig_dir = os.getcwd()
    os.chdir(dirname)
    yield
    os.chdir(orig_dir)


def touch(path):
    with closing(open(path, 'a')) as file:
        os.utime(path, None)


def mkdirp(*paths):
    """Python equivalent of mkdir -p: make directory and any parent directories,
       and don't raise errors if any of these already exist."""
    for path in paths:
        try:
            os.makedirs(path)
        except OSError as exc:
            if (not exc.errno == errno.EEXIST or
                not os.path.isdir(path)):
                raise


def join_path(prefix, *args):
    path = str(prefix)
    for elt in args:
        path = os.path.join(path, str(elt))
    return path


def ancestor(dir, n=1):
    """Get the nth ancestor of a directory."""
    parent = os.path.abspath(dir)
    for i in range(n):
        parent = os.path.dirname(parent)
    return parent


def can_access(file_name):
    """True if we have read/write access to the file."""
    return os.access(file_name, os.R_OK|os.W_OK)
