# Note, that most likely after Go 1.10 dep will become part of standard Go toolset 
#   and this module will become obsolete
#
# The module defines the following variables:
#   GODEP_FOUND - true if the Go was found
#   GODEP_EXECUTABLE - path to the executable
#   GODEP_VERSION - Go version number
#   GODEP_PLATFORM - i.e. linux
#   GODEP_ARCH - i.e. amd64
# Example usage:
#   find_package(Godep 0.4 REQUIRED)

find_program(GODEP_EXECUTABLE dep PATHS ENV GOROOT GOPATH PATH_SUFFIXES bin)
if(GODEP_EXECUTABLE)
	execute_process(COMMAND ${GODEP_EXECUTABLE} version OUTPUT_VARIABLE GODEP_VERSION_OUTPUT OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(GODEP_VERSION_OUTPUT MATCHES ".+version.+: v([0-9]+\\.[0-9]+\\.?[0-9]*).+platform.+: ([^/]+)/(.*)")
        set(GODEP_VERSION ${CMAKE_MATCH_1})
        set(GODEP_PLATFORM ${CMAKE_MATCH_2})
        set(GODEP_ARCH ${CMAKE_MATCH_3})
    endif()
endif()
mark_as_advanced(GODEP_EXECUTABLE)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Godep REQUIRED_VARS GODEP_EXECUTABLE GODEP_PLATFORM GODEP_ARCH GODEP_VERSION VERSION_VAR GODEP_VERSION)
