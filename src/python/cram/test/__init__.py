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
import sys
import unittest

import llnl.util.tty as tty

# Module where test packages are located
_test_module = 'cram.test'

# Names of tests to be included in the test suite
_test_names = ['serialization',
               'cramfile']


def list_tests():
    """Return names of all tests that can be run."""
    return _test_names


def run(names, verbose=False):
    """Run tests with the supplied names.  Names should be a list.  If
       it's empty, run ALL of the tests."""
    verbosity = 1 if not verbose else 2

    # If they didn't provide names of tests to run, then use the default
    # list above.
    if not names:
        names = _test_names
    else:
        for test in names:
            if test not in _test_names:
                tty.error("%s is not a valid test name." % test,
                          "Valid names are:")
                colify(_test_names, indent=4)
                sys.exit(1)

    runner = unittest.TextTestRunner(verbosity=verbosity)

    testsRun = errors = failures = skipped = 0
    for test in names:
        module = _test_module + '.' + test
        print module, test
        suite = unittest.defaultTestLoader.loadTestsFromName(module)

        tty.msg("Running test: %s" % test)
        result = runner.run(suite)
        testsRun += result.testsRun
        errors   += len(result.errors)
        failures += len(result.failures)
        skipped  += len(result.skipped)

    succeeded = not errors and not failures
    tty.msg("Tests Complete.",
            "%5d tests run" % testsRun,
            "%5d skipped" % skipped,
            "%5d failures" % failures,
            "%5d errors" % errors)

    if not errors and not failures:
        tty.info("OK", format='g')
    else:
        tty.info("FAIL", format='r')
        sys.exit(1)
