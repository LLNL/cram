#!/bin/sh
#
# This test builds a cram file and compares the output of cram info
# with the output of cram-cat.  The test cram file is structured to
# cover all I/O decompression cases.
#
# This is essentially a test of the full path from the Python cramfile
# output, and the C input and decompression functions.  it is assumed
# that the Python cram file reader code works: that is tested by
# cram's Python test suite.
#

if [ -z "$(which cram-cat)" ]; then
    echo "Error: Couldn't find cram-cat in your PATH.  "
    echo "       Did you run this in the build output directory?"
    exit 1
fi

if [ -z "$(which cram)" ]; then
    echo "Error: The cram command needs to be in your PATH."
    exit 1
fi

export TEST_VAR1='foo'
export TEST_VAR2='bar'
export TEST_VAR3='baz'
cram pack -f test-cram.job -n 35 foo bar baz

export TEST_VAR1='bar'
export TEST_VAR2='baz'
export TEST_VAR3='quux'
cram pack -f test-cram.job -n 24 foo bar

unset  TEST_VAR1
export TEST_VAR1
export TEST_VAR3='bar'
cram pack -f test-cram.job -n 12 foo bar baz quux

unset TEST_VAR2
export TEST_VAR2
cram pack -f test-cram.job -n 35 foo

cram info -a test-cram.job > cram-info.txt
cram-cat test-cram.job > cram-cat.txt

diff=$(diff cram-info.txt cram-cat.txt)
if [ ! -z "$diff" ]; then
    echo "FAILED"
    echo "cram-info.txt and cram-cat.txt differ.  diff output:"
    echo "$diff"
    exit 1
else
    echo "SUCCESS"
    rm -f cram-info.txt cram-cat.txt test-cram.job
    exit 0
fi
