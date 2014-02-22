import argparse
import llnl.util.tty as tty

description = "Pack a command invocation into a cramfile"

def setup_parser(subparser):
    subparser.add_argument('-n', "--nprocs", type=int, dest='nprocs',
                           help="Number of processes to run with")
    subparser.add_argument('-f', "--file", dest='file',
                           help="File to store command invocation in.  Default is 'cram.job'")
    subparser.add_argument('command', nargs=argparse.REMAINDER,
                           help="Command line to execute.")


def pack(parser, args):
    if not args.command:
        tty.error("You must supply a command line to cram pack.")


