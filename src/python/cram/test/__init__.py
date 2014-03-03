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
        module = 'cram.test.' + test
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
