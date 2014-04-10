import code
import os
import platform
from contextlib import closing

import cram

def setup_parser(subparser):
    subparser.add_argument('file', nargs='?', help="file to run")

description = "Launch an interpreter as cram would and run commands"

def python(parser, args):
    console = code.InteractiveConsole()

    if "PYTHONSTARTUP" in os.environ:
        startup_file = os.environ["PYTHONSTARTUP"]
        if os.path.isfile(startup_file):
            with closing(open(startup_file)) as startup:
                console.runsource(startup.read(), startup_file, 'exec')

    if args.file:
        with closing(open(args.file)) as file:
            console.runsource(file.read(), args.file, 'exec')
    else:
        console.interact("Cram version %s\nPython %s, %s %s"""
                         % (cram.cram_version, platform.python_version(),
                            platform.system(), platform.machine()))
