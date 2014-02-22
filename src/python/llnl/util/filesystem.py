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
    for path in paths:
        if not os.path.exists(path):
            os.makedirs(path)
        elif not os.path.isdir(path):
            raise OSError(errno.EEXIST, "File alredy exists", path)


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
