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
"""\
This file contains utility functions for serializing data to be
ready by the cram backend.
"""

import struct
import array

# Int formats from struct module.
_int_sizes = { 1 : '>B',
               2 : '>H',
               4 : '>I',
               8 : '>Q' }

# Use big endian, 32-bit unsigned int
_default_int_format = '>I'


def write_int(stream, integer, fmt=_default_int_format):
    if isinstance(fmt, int):
        fmt = _int_sizes[fmt]
    packed = struct.pack(fmt, integer)
    stream.write(packed)
    return len(packed)


def read_int(stream, fmt=_default_int_format):
    if isinstance(fmt, int):
        fmt = _int_sizes[fmt]
    size = struct.calcsize(fmt)
    packed = stream.read(size)
    if len(packed) < size:
        raise IOError("Premature end of file")
    return struct.unpack(fmt, packed)[0]


def write_string(stream, string):
    length = len(string)
    int_len = write_int(stream, length)
    stream.write(string)
    return len(string) + int_len


def read_string(stream):
    length = read_int(stream)
    string = stream.read(length)
    if len(string) < length:
        raise IOError("Premature end of file")
    return string
