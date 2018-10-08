cmake_minimum_required(VERSION 3.7)
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
set(PRJ_VERSION_MAJOR 9)
set(PRJ_VERSION_MINOR 44)

if(WIN32 AND NOT DEFINED ENV{MSYSTEM})
    message(FATAL_ERROR "Currently unsuppored environment. Use MINGW for Windows builds. CMake willl exit now.")
endif()

if(CMAKE_CROSSCOMPILING OR WIN32)
    # Cannot win here - libwinpthread-1.dll will still be required on run-time (unless somebody rebuilds mingw compilers in MSYS2 with win32 threads)
    if($ENV{MSYSTEM} STREQUAL "MINGW32")
        set(DEST_DIR "${PROJECT_SOURCE_DIR}/bin_win32")
        find_path(WINPTHREAD_DLL_DIR libwinpthread-1.dll)
    elseif($ENV{MSYSTEM} STREQUAL "MINGW64")
        set(DEST_DIR "${PROJECT_SOURCE_DIR}/bin_win64")
        find_path(WINPTHREAD_DLL_DIR libwinpthread-1.dll)
    else()
        message(FATAL_ERROR "Currently unsuppored environment. CMake willl exit now.")
    endif()
    # When cross-compiling on Ubuntu libwinpthreadi.dll is not needed
    string(COMPARE EQUAL "WINPTHREAD_DLL_DIR-NOTFOUND" WINPTHREAD_DLL_DIR WINPTHREAD_DLL_FOUND)
    # I want static libraries everywhere (but for MySQL on Windows)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_SHARED_LIBRARY_SUFFIX}.a)
else()
    execute_process(COMMAND ${CMAKE_COMMAND} -E env uname -m OUTPUT_VARIABLE LINUX_BIN OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    set(DEST_DIR "${PROJECT_SOURCE_DIR}/bin_linux_${LINUX_BIN}")
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()
message(STATUS "Destination directory: ${DEST_DIR}")

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "** Debug mode")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -D_XML_DEBUG")
else()
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif()

find_package(Git REQUIRED)

if (EXISTS "${PROJECT_SOURCE_DIR}/.git" AND IS_DIRECTORY "${PROJECT_SOURCE_DIR}/.git")
    # if we can - get current state of the project
    add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/inpxc_git_ver
        COMMAND ${GIT_EXECUTABLE} rev-list -1 HEAD >${PROJECT_BINARY_DIR}/inpxc_git_ver.tmp
        COMMAND sh -c 'test -n \"\$\(shell ${GIT_EXECUTABLE} diff --shortstat 2> /dev/null | tail -n1\)\" && echo \"*\" >>${PROJECT_BINARY_DIR}/inpxc_git_ver.tmp || true'
        COMMAND tr -d '\\n' <${PROJECT_BINARY_DIR}/inpxc_git_ver.tmp >${PROJECT_BINARY_DIR}/inpxc_git_ver
        COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJECT_BINARY_DIR}/inpxc_git_ver.tmp
        COMMENT "Checking git status..."
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
else()
    add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/inpxc_git_ver
        COMMAND sh -c 'echo -n no-git-repo >${PROJECT_BINARY_DIR}/inpxc_git_ver'
        COMMENT "Checking git status..."
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()
add_custom_target(gen_git_ver 
        COMMAND sed -i '/define PRJ_GIT_HASH/ s/ \".*\"/ \"\$\(shell cat ${PROJECT_BINARY_DIR}/inpxc_git_ver\)\"/' ${PROJECT_BINARY_DIR}/version.h     
        COMMENT "Patching version.h"
        DEPENDS ${PROJECT_BINARY_DIR}/inpxc_git_ver)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost 1.58.0 REQUIRED COMPONENTS "date_time" "program_options" "filesystem")
find_package(Threads)
if(DEFINED ENV{MYSQL_PROPER})
    find_package(MySQL 5.7.17 REQUIRED EXACT)
else()
    find_package(MariaDB 10.1.26 REQUIRED EXACT)
endif()
find_package(ZLIB REQUIRED)
find_package(EXPAT REQUIRED)
find_package(Iconv REQUIRED)

get_filename_component(MYSQL_NAME ${MYSQL_LIBRARIES} NAME_WE)

configure_file("${PROJECT_SOURCE_DIR}/cmake/version.h.in" "${PROJECT_BINARY_DIR}/version.h")

# Prepare history file
execute_process(COMMAND ${CMAKE_COMMAND} -E env ${GIT_EXECUTABLE} changelog --final-tag ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR} --stdout OUTPUT_FILE ${PROJECT_BINARY_DIR}/history.txt ERROR_QUIET)

if(CMAKE_CROSSCOMPILING OR WIN32)
    set(SRCS_MINIZIP minizip/ioapi.c minizip/iowin32.c minizip/unzip.c minizip/zip.c)
else()
    set(SRCS_MINIZIP minizip/ioapi.c minizip/ioapi.c minizip/unzip.c minizip/zip.c)
endif()

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_BINARY_DIR}")

add_library(minizip ${SRCS_MINIZIP})

set(SRCS_LIB2INPX lib2inpx/util.cpp lib2inpx/lib2inpx.cpp)
add_executable(lib2inpx ${SRCS_LIB2INPX})
add_dependencies(lib2inpx gen_git_ver)
if(UNIX)
    set(ADD_LIBS "${CMAKE_THREAD_LIBS_INIT} -lm -lrt -ldl -lcrypt -laio")
    if(ICONV_LIBRARIES MATCHES "libc")
        target_link_libraries(lib2inpx minizip ${Boost_LIBRARIES} ${MYSQL_LIBRARIES} ${ZLIB_LIBRARIES} ${EXPAT_LIBRARIES} ${ADD_LIBS})
    else()
        target_link_libraries(lib2inpx minizip ${Boost_LIBRARIES} ${MYSQL_LIBRARIES} ${ZLIB_LIBRARIES} ${EXPAT_LIBRARIES} ${ICONV_LIBRARIES} ${ADD_LIBS})
    endif()
else()
    target_link_libraries(lib2inpx minizip ${Boost_LIBRARIES} ${MYSQL_LIBRARIES} ${ZLIB_LIBRARIES} ${EXPAT_LIBRARIES} ${ICONV_LIBRARIES})
endif()

install(FILES ${PROJECT_BINARY_DIR}/history.txt DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
FILE(GLOB files "misc/*")
install(FILES ${files} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
FILE(GLOB scripts "scripts/*")
install(PROGRAMS ${scripts} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
if(CMAKE_CROSSCOMPILING OR WIN32)
    install(PROGRAMS ${MYSQL_LIB_DIR}/${MYSQL_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
    if(WINPTHREAD_DLL_FOUND)
        install(PROGRAMS ${WINPTHREAD_DLL_DIR}/libwinpthread-1.dll DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
    endif()
endif()
install(FILES ${MYSQL_LIB_DIR}/../share/english/errmsg.sys DESTINATION ${DEST_DIR}/language CONFIGURATIONS Release)
install(TARGETS lib2inpx DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
# for debugging purposes
if(CMAKE_CROSSCOMPILING OR WIN32)
    install(PROGRAMS ${MYSQL_LIB_DIR}/${MYSQL_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX} DESTINATION ${PROJECT_BINARY_DIR} CONFIGURATIONS Release Debug)
    if(WINPTHREAD_DLL_FOUND)
        install(PROGRAMS ${WINPTHREAD_DLL_DIR}/libwinpthread-1.dll DESTINATION ${PROJECT_BINARY_DIR} CONFIGURATIONS Release Debug)
    endif()
endif()
install(FILES ${MYSQL_LIB_DIR}/../share/english/errmsg.sys DESTINATION ${PROJECT_BINARY_DIR}/language CONFIGURATIONS Release Debug)

########################################################################################################
# go targets are always out of date, "go build" will figure it out
########################################################################################################

find_package(Go 1.11 REQUIRED)

set(GODEP_REQ_VER "0.5.0")
find_package(Godep "${GODEP_REQ_VER}")
if(NOT GODEP_FOUND)
    set(G_TAG "v${GODEP_REQ_VER}")
    set(G_ARCH "linux-amd64")
    set(G_SHA256 "287b08291e14f1fae8ba44374b26a2b12eb941af3497ed0ca649253e21ba2f83")

    message(STATUS "Installing godep (ver ${G_TAG}) into ${PROJECT_BINARY_DIR}")
    file(DOWNLOAD "https://github.com/golang/dep/releases/download/${G_TAG}/dep-${G_ARCH}" ${PROJECT_BINARY_DIR}/dep
        INACTIVITY_TIMEOUT 60
        TIMEOUT 300
        STATUS DOWNLOAD_RES
        EXPECTED_HASH SHA256=${G_SHA256})
    list(GET DOWNLOAD_RES 0 RES)
    list(GET DOWNLOAD_RES 1 MSG)
    if(RES)
        message(FATAL_ERROR, "${MSG}")
    endif()
    execute_process(COMMAND sh -c "chmod +x ${PROJECT_BINARY_DIR}/dep" WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

    if(EXISTS ${PROJECT_BINARY_DIR}/dep)
        set(GODEP_FOUND true)
        set(GODEP_EXECUTABLE ${PROJECT_BINARY_DIR}/dep)
        set(GODEP_VERSION ${GODEP_REQ_VER})
     else()
        message(FATAL_ERROR "Unable to obtain and install requred version of go dep. Exiting...")
     endif()

    message(STATUS "Vendoring for go")
    set(ENV{GOPATH} ${PROJECT_SOURCE_DIR})
    if(NOT EXISTS ${PROJECT_SOURCE_DIR}/src/inpxcreator/Gopkg.toml)
        message(FATAL_ERROR "Unable to find Gopkg.toml. Exiting...")
    endif()
    execute_process(COMMAND ${GODEP_EXECUTABLE} ensure -vendor-only WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/src/inpxcreator")
endif()

if(CMAKE_CROSSCOMPILING)
    # Set Go target environments
    if($ENV{MSYSTEM} STREQUAL "MINGW32")
        set(CROSS_GOOS windows)
        set(CROSS_GOARCH 386)
    elseif($ENV{MSYSTEM} STREQUAL "MINGW64")
        set(CROSS_GOOS windows)
        set(CROSS_GOARCH amd64)
    endif()
endif()

configure_file("${PROJECT_SOURCE_DIR}/cmake/version.go.in" "${PROJECT_SOURCE_DIR}/src/inpxcreator/misc/version.go")

add_custom_target(bin_libget2 ALL
    DEPENDS ${PROJECT_BINARY_DIR}/inpxc_git_ver
    COMMAND GOPATH=${PROJECT_SOURCE_DIR} GOOS=${CROSS_GOOS} GOARCH=${CROSS_GOARCH}
        ${GO_EXECUTABLE} build -o ${PROJECT_BINARY_DIR}/libget2${CMAKE_EXECUTABLE_SUFFIX}
        -ldflags='-X main.LastGitCommit=\$\(shell cat ${PROJECT_BINARY_DIR}/inpxc_git_ver\)'
        inpxcreator/cmd/libget2
    COMMENT "Building libget2..."
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/src/inpxcreator")
install(PROGRAMS ${PROJECT_BINARY_DIR}/libget2${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)

add_custom_target(bin_libmerge ALL
    DEPENDS ${PROJECT_BINARY_DIR}/inpxc_git_ver
    COMMAND GOPATH=${PROJECT_SOURCE_DIR} GOOS=${CROSS_GOOS} GOARCH=${CROSS_GOARCH}
        ${GO_EXECUTABLE} build -o ${PROJECT_BINARY_DIR}/libmerge${CMAKE_EXECUTABLE_SUFFIX}
        -ldflags='-X main.LastGitCommit=\$\(shell cat ${PROJECT_BINARY_DIR}/inpxc_git_ver\)'
        inpxcreator/cmd/libmerge
    COMMENT "Building libmerge..."
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/src/inpxcreator")
install(PROGRAMS ${PROJECT_BINARY_DIR}/libmerge${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
