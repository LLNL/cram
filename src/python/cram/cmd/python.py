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
