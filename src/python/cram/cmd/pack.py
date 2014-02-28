import argparse
from contextlib import closing

import llnl.util.tty as tty
from cram.cramfile import *

description = "Pack a command invocation into a cramfile"

def setup_parser(subparser):
    subparser.add_argument('-n', "--nprocs", type=int, dest='nprocs',
                           help="Number of processes to run with")
    subparser.add_argument('-f', "--file", dest='file', default='cram.job',
                           help="File to store command invocation in.  Default is 'cram.job'")
    subparser.add_argument('command', nargs=argparse.REMAINDER,
                           help="Command line to execute.")


def pack(parser, args):
    if not args.command:
        tty.die("You must supply a command line to cram pack.")

    if not args.nprocs:
        tty.die("You must supply a number of processes to run with.")

    with closing(CramFile(args.file, 'a')) as cf:
        cf.append(Job(args.nprocs, os.getcwd(), args.command, os.environ))
