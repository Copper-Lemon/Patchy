# Writes the generated build-stamp header with the build machine's current
# local wall clock. The build graph runs this only after a build input changes
# or packaging explicitly refreshes the stamp. An unchanged build keeps its
# existing binary and timestamp.
if(NOT PATCHY_BUILD_STAMP_HEADER)
  message(FATAL_ERROR "PATCHY_BUILD_STAMP_HEADER was not passed with -D.")
endif()
string(TIMESTAMP PATCHY_BUILD_STAMP_NOW "%Y-%m-%d %H:%M")
file(WRITE "${PATCHY_BUILD_STAMP_HEADER}" "#pragma once

// Generated after build inputs change or packaging requests a refresh.
// Never edit or commit. Include it only from build_info.cpp.
#define PATCHY_BUILD_STAMP \"${PATCHY_BUILD_STAMP_NOW}\"
")
