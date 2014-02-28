"""\
This file contains utility functions for serializing data to be
ready by the cram backend.
"""

import struct
import array

# Use big endian, unsigned long long
_default_int_format = '>I'
_int_sizes = { 1 : '>B', 2 : '>H', 4 : '>I', 8 : '>Q' }


def write_int(stream, integer, fmt=_default_int_format):
    if isinstance(fmt, int):
        fmt = _int_sizes[fmt]
    packed = struct.pack(fmt, integer)
    stream.write(packed)


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
    write_int(stream, length)
    stream.write(string)


def read_string(stream):
    length = read_int(stream)
    string = stream.read(length)
    if len(string) < length:
        raise IOError("Premature end of file")
    return string
