Cram
=========================
Packs lots of small MPI jobs into a large one.

Build & Install
----------------

To build and install cram, you will need CMake on your machine.
Simple build commands are as follows:

    cmake .
    make
    make install

On Blue Gene/Q, you need to supply -DCMAKE_TOOLCHAIN_FILE to get cross
compilation working:

    cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain/BlueGeneQ-xl.cmake .
    make
    make install

This package supports parallel make, so you are free to use make
-j<jobs> if you like.


Documentation
----------------
Documentation is forthcoming.


Authors
----------------
Todd Gamblin, tgamblin@llnl.gov.
