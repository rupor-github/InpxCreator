# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The module defines the following variables:
#   GLIDE_FOUND - true if the Glide was found
#   GLIDE_EXECUTABLE - path to the Glide executable
#   GLIDE_VERSION - Glide version number
# Example usage:
#   find_package(Glide 0.11 REQUIRED)


find_program(GLIDE_EXECUTABLE glide PATHS ENV GOROOT GOPATH PATH_SUFFIXES bin)
if(GLIDE_EXECUTABLE)
	execute_process(COMMAND sh -c "${GLIDE_EXECUTABLE} --version" OUTPUT_VARIABLE GLIDE_VERSION_OUTPUT OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(GLIDE_VERSION_OUTPUT MATCHES "glide version v([0-9]+\\.[0-9]+\\.?[0-9]*)")
		set(GLIDE_VERSION ${CMAKE_MATCH_1})
    endif()
endif()
mark_as_advanced(GO_EXECUTABLE)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Glide REQUIRED_VARS GLIDE_EXECUTABLE GLIDE_VERSION VERSION_VAR GLIDE_VERSION)
