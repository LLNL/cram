"""\
This file contains utility functions for serializing data to be
ready by the cram backend.
"""

import struct
import array

# Use big endian, unsigned long long
_int_format = '>Q'

def write_int(stream, integer):
    packed = struct.pack(_int_format, integer)
    stream.write(packed)


def read_int(stream):
    size = struct.calcsize(_int_format)
    packed = stream.read(size)
    return struct.unpack(_int_format, packed)[0]


def write_string(stream, string):
    length = len(string)
    write_int(stream, length)
    stream.write(string)


def read_string(stream, string):
    length = read_int(stream)
    return stream.read(length)
