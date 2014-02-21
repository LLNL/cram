import sys
import os
import re

# ignore backupes and such when listing modules
_ignore_modules = [r'^\.#', '~$']

def die(msg):
    """Print an error message and quit cram."""
    print msg
    sys.exit(1)


def list_modules(directory, **kwargs):
    """Lists all of the modules, excluding __init__.py, in a
       particular directory.  Listed packages have no particular
       order."""
    list_directories = kwargs.setdefault('directories', True)

    for name in os.listdir(directory):
        if name == '__init__.py':
            continue

        path = os.path.join(directory, name)
        if list_directories and os.path.isdir(path):
            init_py = os.path.join(path, '__init__.py')
            if os.path.isfile(init_py):
                yield name

        elif name.endswith('.py'):
            if not any(re.search(pattern, name) for pattern in _ignore_modules):
                yield re.sub('.py$', '', name)


def ancestor(dir, n=1):
    """Get the nth ancestor of a directory."""
    parent = os.path.abspath(dir)
    for i in range(n):
        parent = os.path.dirname(parent)
    return parent


def attr_setdefault(obj, name, value):
    """Like dict.setdefault, but for objects."""
    if not hasattr(obj, name):
        setattr(obj, name, value)
    return getattr(obj, name)


