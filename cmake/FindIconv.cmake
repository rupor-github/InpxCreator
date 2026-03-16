# - Try to find Iconv
# Once done this will define
#
#  ICONV_FOUND - system has Iconv
#  ICONV_INCLUDE_DIR - the Iconv include directory
#  ICONV_LIBRARIES - Link these to use Iconv
#  ICONV_SECOND_ARGUMENT_IS_CONST - the second argument for iconv() is const
#
include(CheckCXXSourceCompiles)

if(ICONV_INCLUDE_DIR AND ICONV_LIBRARIES)
  # Already in cache, be silent
  set(ICONV_FIND_QUIETLY TRUE)
endif()

if(NOT CMAKE_CROSSCOMPILING AND NOT WIN32 AND EXISTS "/opt/libiconv-static")
  find_path(ICONV_INCLUDE_DIR iconv.h PATHS /opt/libiconv-static/include NO_DEFAULT_PATH)
  find_library(ICONV_LIBICONV NAMES libiconv.a PATHS /opt/libiconv-static/lib NO_DEFAULT_PATH)
  find_library(ICONV_LIBCHARSET NAMES libcharset.a PATHS /opt/libiconv-static/lib NO_DEFAULT_PATH)

  if(ICONV_INCLUDE_DIR AND ICONV_LIBICONV AND ICONV_LIBCHARSET)
    set(ICONV_LIBRARIES ${ICONV_LIBICONV} ${ICONV_LIBCHARSET})
  endif()
endif()

if(NOT ICONV_INCLUDE_DIR)
  find_path(ICONV_INCLUDE_DIR iconv.h)
endif()

if(NOT ICONV_LIBRARIES)
  find_library(ICONV_LIBRARIES NAMES iconv libiconv libiconv-2 c)
endif()

if(ICONV_INCLUDE_DIR AND ICONV_LIBRARIES)
  set(ICONV_FOUND TRUE)
endif()

set(CMAKE_REQUIRED_INCLUDES ${ICONV_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${ICONV_LIBRARIES})

if(ICONV_FOUND)
  check_cxx_source_compiles("
  #include <iconv.h>
  int main(){
    iconv_t conv = 0;
    const char* in = 0;
    size_t ilen = 0;
    char* out = 0;
    size_t olen = 0;
    iconv(conv, &in, &ilen, &out, &olen);
    return 0;
  }
" ICONV_SECOND_ARGUMENT_IS_CONST )
endif()

set(CMAKE_REQUIRED_INCLUDES)
set(CMAKE_REQUIRED_LIBRARIES)

if(ICONV_FOUND)
  if(NOT ICONV_FIND_QUIETLY)
    message(STATUS "Found Iconv: ${ICONV_LIBRARIES}")
  endif()
else()
  if(Iconv_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find Iconv")
  endif()
endif()

mark_as_advanced(
  ICONV_INCLUDE_DIR
  ICONV_LIBICONV
  ICONV_LIBCHARSET
  ICONV_LIBRARIES
  ICONV_SECOND_ARGUMENT_IS_CONST
)
