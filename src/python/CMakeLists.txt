set(PY_VERSION "${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}")
set(PY_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/python${PY_VERSION}/site-packages")
set(PY_COMPILE_CMD "import compileall; compileall.compile_dir('${PY_INSTALL_DIR}')")

# Install all the python files into the prefix's site-packages directory
install(
  DIRECTORY cram llnl
  DESTINATION "${PY_INSTALL_DIR}"
  FILES_MATCHING PATTERN "*.py")

# Make Python precompile all the bytecode for the cram packages.
install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} -c \"${PY_COMPILE_CMD}\")")
