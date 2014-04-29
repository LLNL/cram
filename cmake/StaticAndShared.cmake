
function(add_static_and_shared_library target)
  get_property(supports_shared_libs GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS)
  if (supports_shared_libs)
    add_library(${target} SHARED ${ARGN})
  endif()
  add_library(${target}_static STATIC ${ARGN})
  set_target_properties(${target}_static PROPERTIES OUTPUT_NAME ${target})
endfunction()
