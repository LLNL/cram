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
import argparse
import math

import llnl.util.tty as tty
from cram.cramfile import *

description = "Display information about a cramfile."

def setup_parser(subparser):
    subparser.add_argument('-a', "--all", action='store_true', dest='all',
                           help="Print information on ALL jobs in the cram file.")
    subparser.add_argument('-j', "--job", type=int, dest='job',
                           help="Specific job id to display in more detail.")
    subparser.add_argument('-n', type=int, dest='num_lines', default=10,
                           help="Number of job lines to print")
    subparser.add_argument('cramfile', help="Cram file to display.")


def write_header(args, cf):
    print "Name:%25s"              % args.cramfile
    print "Number of Jobs:   %12d" % cf.num_jobs
    print "Total Procs:      %12d" % cf.num_procs
    print "Cram version:     %12d" % cf.version
    print "Max job record:   %12d" % cf.max_job_size


def write_job_summary(args, cf):
    print "First %d job command lines:" % args.num_lines

    for i, job in enumerate(cf):
        if i >= args.num_lines: break
        print "%5d  %5d procs    %s" % (i, job.num_procs, ' '.join(job.args))


def write_job_info(job):
    print "  Num procs: %d"   % job.num_procs
    print "  Working dir: %s" % job.working_dir
    print "  Arguments:"
    print "      " + ' '.join(job.args)
    print "  Environment:"
    for key in sorted(job.env):
        print "      '%s' : '%s'" % (key, job.env[key])


def info(parser, args):
    if not args.cramfile:
        tty.error("You must specify a file to display with cram info.")

    with closing(CramFile(args.cramfile, 'r')) as cf:
        if args.all:
            write_header(args, cf)
            print
            print "Job information:"
            for i, job in enumerate(cf):
                print "Job %d:" % i
                write_job_info(job)

        elif args.job:
            if args.job not in range(len(cf)):
                tty.die("No job %d in this cram file." % args.job)
            print "Job %d:" % args.job
            write_job_info(cf[args.job])

        else:
            write_header(args, cf)
            print
            write_job_summary(args, cf)
