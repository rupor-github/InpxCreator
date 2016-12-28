cmake_minimum_required(VERSION 2.6)
message(STATUS "Running cmake version ${CMAKE_VERSION}")

project(InpxCreator)

message(STATUS "Running cmake version ${CMAKE_VERSION}")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "default to release build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# Project version number
set(PRJ_VERSION_MAJOR 8)
set(PRJ_VERSION_MINOR 2)
configure_file("${PROJECT_SOURCE_DIR}/cmake/version.h.in" "${PROJECT_BINARY_DIR}/version.h")
include_directories("${PROJECT_BINARY_DIR}")

if(WIN32 AND DEFINED ENV{MSYSTEM})
    # Cannot win here - libwinpthread-1.dll will still be required on run-time (unless somebody rebuilds mingw compilers in MSYS2 with win32 threads)
    if($ENV{MSYSTEM} STREQUAL "MINGW32")
        set(DEST_DIR "${PROJECT_SOURCE_DIR}/bin")
        find_path(WINPTHREAD_DLL_DIR libwinpthread-1.dll "/mingw32/bin")
    elseif($ENV{MSYSTEM} STREQUAL "MINGW64")
        set(DEST_DIR "${PROJECT_SOURCE_DIR}/bin64")
        find_path(WINPTHREAD_DLL_DIR libwinpthread-1.dll "/mingw64/bin")
    else()
        # cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release ..
        message(FATAL_ERROR "Currently unsuppored environment. CMake willl exit now.")
    endif()
else()
    execute_process(COMMAND ${CMAKE_COMMAND} -E env uname -m OUTPUT_VARIABLE LINUX_BIN OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    set(DEST_DIR "${PROJECT_SOURCE_DIR}/bin_linux_${LINUX_BIN}")
endif()
message(STATUS "Destination directory: ${DEST_DIR}")

# I want static libraries everywhere (but for MySQL on Windows)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_SHARED_LIBRARY_SUFFIX}.a)
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "** Debug mode")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_XML_DEBUG")
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost 1.62.0 REQUIRED COMPONENTS "date_time" "program_options" "filesystem")
find_package(Threads)
find_package(MySQL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Iconv REQUIRED)
find_package(EXPAT REQUIRED)
find_package(Git REQUIRED)

get_filename_component(MYSQL_NAME ${MYSQL_LIBRARIES} NAME_WE)

# Prepare history file
execute_process(COMMAND ${CMAKE_COMMAND} -E env ${GIT_EXECUTABLE} changelog --final-tag ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR} --stdout OUTPUT_FILE ${PROJECT_BINARY_DIR}/history.txt ERROR_QUIET)

if(WIN32)
    set(SRCS_MINIZIP minizip/ioapi.c minizip/iowin32.c minizip/unzip.c minizip/zip.c)
else()
    set(SRCS_MINIZIP minizip/ioapi.c minizip/ioapi.c minizip/unzip.c minizip/zip.c)
endif()
include_directories("${PROJECT_SOURCE_DIR}")
add_library(minizip ${SRCS_MINIZIP})

set(SRCS_LIB2INPX lib2inpx/util.cpp lib2inpx/lib2inpx.cpp)
add_executable(lib2inpx ${SRCS_LIB2INPX})
if(UNIX)
    set(ADD_LIBS "${CMAKE_THREAD_LIBS_INIT} -lm -lrt -ldl -lcrypt -laio")
endif()
target_link_libraries(lib2inpx minizip ${Boost_LIBRARIES} ${MYSQL_LIBRARIES} ${ZLIB_LIBRARIES} ${EXPAT_LIBRARIES} ${ICONV_LIBRARIES} ${ADD_LIBS})

install(FILES ${PROJECT_BINARY_DIR}/history.txt DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
FILE(GLOB files "misc/*")
install(FILES ${files} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
FILE(GLOB scripts "scripts/*")
install(PROGRAMS ${scripts} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
if(WIN32)
    install(PROGRAMS ${MYSQL_LIB_DIR}/${MYSQL_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
    install(PROGRAMS ${WINPTHREAD_DLL_DIR}/libwinpthread-1.dll DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
endif()
install(FILES ${MYSQL_LIB_DIR}/../share/english/errmsg.sys DESTINATION ${DEST_DIR}/language CONFIGURATIONS Release)
install(TARGETS lib2inpx DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
# for debugging purposes
if(WIN32)
    install(PROGRAMS ${MYSQL_LIB_DIR}/${MYSQL_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX} DESTINATION ${PROJECT_BINARY_DIR} CONFIGURATIONS Release Debug)
    install(PROGRAMS ${WINPTHREAD_DLL_DIR}/libwinpthread-1.dll DESTINATION ${PROJECT_BINARY_DIR} CONFIGURATIONS Release Debug)
endif()
install(FILES ${MYSQL_LIB_DIR}/../share/english/errmsg.sys DESTINATION ${PROJECT_BINARY_DIR}/language CONFIGURATIONS Release Debug)

find_package(Go 1.7 REQUIRED)
find_package(Glide 0.12 REQUIRED)
# Proper go build structure will be here
set(GO_PATH "${CMAKE_BINARY_DIR}/go")

message(STATUS "Vendoring for go")
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory go/src)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/glide.yaml go/src/.)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/glide.lock go/src/.)
set(ENV{GOPATH} ${GO_PATH})
execute_process(COMMAND ${CMAKE_COMMAND} -E env ${GLIDE_EXECUTABLE} install WORKING_DIRECTORY go/src)

configure_file("${PROJECT_SOURCE_DIR}/cmake/version.go.in" "${GO_PATH}/src/libget2/version.go")
file(GLOB SRCS_LIBGET2 RELATIVE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/libget2/*.go)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/libget2${CMAKE_EXECUTABLE_SUFFIX}
	DEPENDS ${SRCS_LIBGET2} "${GO_PATH}/src/libget2/version.go"
	# setting up source tree
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/libget2/ go/src/libget2
	# Building
	COMMAND ${CMAKE_COMMAND} -E env GOROOT=${GO_ROOT} GOPATH=${GO_PATH} ${GO_EXECUTABLE} build libget2
	COMMENT "Building GO program - libget2")
add_custom_target(libget2_exe ALL DEPENDS ${CMAKE_BINARY_DIR}/libget2${CMAKE_EXECUTABLE_SUFFIX})
install(PROGRAMS ${CMAKE_BINARY_DIR}/libget2${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)

configure_file ("${PROJECT_SOURCE_DIR}/cmake/version.go.in" "${GO_PATH}/src/libmerge/version.go")
file(GLOB SRCS_LIBMERGE RELATIVE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/libmerge/*.go)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/libmerge${CMAKE_EXECUTABLE_SUFFIX}
	DEPENDS ${SRCS_LIBMERGE} "${GO_PATH}/src/libget2/version.go"
	# setting up source tree
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/libmerge/ go/src/libmerge
	# Building
	COMMAND ${CMAKE_COMMAND} -E env GOROOT=${GO_ROOT} GOPATH=${GO_PATH} ${GO_EXECUTABLE} build libmerge
	COMMENT "Building GO program - libmerge")
add_custom_target(libmerge_exe ALL DEPENDS ${CMAKE_BINARY_DIR}/libmerge${CMAKE_EXECUTABLE_SUFFIX})
install(PROGRAMS ${CMAKE_BINARY_DIR}/libmerge${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)

