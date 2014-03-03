
configure_file(cram.dk.in cram.dk @ONLY)
install(
  FILES ${PROJECT_BINARY_DIR}/src/modules/cram.dk
  DESTINATION share/cram/dotkit)
