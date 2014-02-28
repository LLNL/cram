import sys
import unittest
import string
import random
from StringIO import StringIO

from cram.serialization import *

class SerializationTest(unittest.TestCase):

    def test_integers(self):
        """Test writing and reading integers of different sizes from a
           stream."""
        stream = StringIO()
        sizes = [1, 2, 4, 8]

        for size in sizes:
            i = 2
            while i < (2**(size*8) - 1):
                write_int(stream, i, 8)
                write_int(stream, i + 1, 8)
                write_int(stream, i - 1, 8)
                i **= 2

        stream = StringIO(stream.getvalue())
        for size in sizes:
            i = 2
            while i < (2**(size*8) - 1):
                self.assertEqual(read_int(stream, 8), i)
                self.assertEqual(read_int(stream, 8), i + 1)
                self.assertEqual(read_int(stream, 8), i - 1)
                i **= 2


    def test_strings(self):
        """Test writing and reading strings from a stream."""
        # Make 100 random strings of widely varying length
        strings = []
        lengths = range(1, 4096)
        for n in range(100):
            strings.append(''.join(random.choice(string.ascii_uppercase)
                                   for i in range(random.choice(lengths))))

        # write all the strings then see if they're the same when we read them.
        stream = StringIO()

        for s in strings:
            write_string(stream, s)

        stream = StringIO(stream.getvalue())

        for s in strings:
            self.assertEqual(read_string(stream), s)
