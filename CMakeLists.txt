cmake_minimum_required(VERSION 2.6)
message(STATUS "Running cmake version ${CMAKE_VERSION}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

project(InpxCreator)
# Project version number
set(PRJ_VERSION_MAJOR 6)
set(PRJ_VERSION_MINOR 62)
configure_file("${PROJECT_SOURCE_DIR}/cmake/version.h.in" "${PROJECT_BINARY_DIR}/version.h")
include_directories("${PROJECT_BINARY_DIR}")

# Cannot win here - libwinpthread-1.dll will still be required on run-time (unless somebody rebuilds mingw compilers in MSYS2 with win32 threads)
if($ENV{MSYSTEM} STREQUAL "MINGW32")
	set(DEST_DIR "${PROJECT_SOURCE_DIR}/bin")
	find_path(WINPTHREAD_DLL_DIR libwinpthread-1.dll "/mingw32/bin")
ELSEIF($ENV{MSYSTEM} STREQUAL "MINGW64")
	set(DEST_DIR "${PROJECT_SOURCE_DIR}/bin64")
	find_path(WINPTHREAD_DLL_DIR libwinpthread-1.dll "/mingw64/bin")
ELSE()
	# cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release ..
	message(FATAL_ERROR "Currently unsuppored environment. CMake willl exit now.")
ENDIF()

# I want static libraries everywhere but for MySQL
set(CMAKE_FIND_LIBRARY_SUFFIXES .a .dll.a)
set(BUILD_SHARED_LIBS OFF)

if(NOT MVCS)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.60.0 REQUIRED COMPONENTS "date_time" "program_options")
find_package(MySQL REQUIRED)
find_package(ZLib REQUIRED)
find_package(Expat REQUIRED)
find_package(Git REQUIRED)

# Prepare history file
execute_process(COMMAND ${CMAKE_COMMAND} -E env ${GIT_EXECUTABLE} changelog --final-tag ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR} --stdout OUTPUT_FILE ${PROJECT_BINARY_DIR}/history.txt ERROR_QUIET)

set(SRCS_MINIZIP minizip/ioapi.c minizip/iowin32.c minizip/unzip.c minizip/zip.c)
add_library(minizip ${SRCS_MINIZIP})

set(SRCS_LIB2INPX lib2inpx/util.cpp lib2inpx/guid.cpp lib2inpx/lib2inpx.cpp)
add_executable(lib2inpx ${SRCS_LIB2INPX})
target_link_libraries(lib2inpx minizip ${Boost_LIBRARIES} ${MYSQL_LIBRARIES} ${ZLIB_LIBRARIES} ${EXPAT_LIBRARIES})

set(SRCS_LIBSPLIT lib2inpx/util.cpp lib2inpx/libsplit.cpp)
add_executable(libsplit ${SRCS_LIBSPLIT})
target_compile_definitions(libsplit PUBLIC DO_NOT_INCLUDE_PARSER)
target_link_libraries(libsplit minizip ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

FILE(GLOB files "misc/*")
install(FILES ${files} DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
install(FILES ${PROJECT_BINARY_DIR}/history.txt DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
install(PROGRAMS ${MYSQL_LIB_DIR}/libmysqld.dll DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
install(PROGRAMS ${WINPTHREAD_DLL_DIR}/libwinpthread-1.dll DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
install(FILES ${MYSQL_LIB_DIR}/../share/english/errmsg.sys DESTINATION ${DEST_DIR}/language CONFIGURATIONS Release)
install(TARGETS lib2inpx DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
install(TARGETS libsplit DESTINATION ${DEST_DIR} CONFIGURATIONS Release)

find_package(Go 1.6 REQUIRED)
find_package(Glide 0.11 REQUIRED)
# Proper go build structure will be here
set(GO_PATH "${CMAKE_BINARY_DIR}/go")
include_directories("${PROJECT_BINARY_DIR}")

message(STATUS "Vendoring for go")
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory go/src)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/glide.yaml go/src/.)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/glide.lock go/src/.)
execute_process(COMMAND ${CMAKE_COMMAND} -E env ${GLIDE_EXECUTABLE} install WORKING_DIRECTORY go/src)

configure_file("${PROJECT_SOURCE_DIR}/cmake/version.go.in" "${GO_PATH}/src/libget2/version.go")
file(GLOB SRCS_LIBGET2 RELATIVE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/libget2/*.go)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/libget2.exe
	DEPENDS ${SRCS_LIBGET2} "${GO_PATH}/src/libget2/version.go"
	# setting up source tree
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/libget2/ go/src/libget2
	# Building
	COMMAND ${CMAKE_COMMAND} -E env GOROOT=${GO_ROOT} GOPATH=${GO_PATH} ${GO_EXECUTABLE} build libget2
	COMMENT "Building GO program - libget2")
add_custom_target(libget2 ALL DEPENDS ${CMAKE_BINARY_DIR}/libget2.exe)
install(PROGRAMS ${CMAKE_BINARY_DIR}/libget2.exe DESTINATION ${DEST_DIR} CONFIGURATIONS Release)

configure_file ("${PROJECT_SOURCE_DIR}/cmake/version.go.in" "${GO_PATH}/src/libmerge/version.go")
file(GLOB SRCS_LIBMERGE RELATIVE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/libmerge/*.go)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/libmerge.exe
	DEPENDS ${SRCS_LIBMERGE} "${GO_PATH}/src/libget2/version.go"
	# setting up source tree
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/libmerge/ go/src/libmerge
	# Building
	COMMAND ${CMAKE_COMMAND} -E env GOROOT=${GO_ROOT} GOPATH=${GO_PATH} ${GO_EXECUTABLE} build libmerge
	COMMENT "Building GO program - libmerge")
add_custom_target(libmerge ALL DEPENDS ${CMAKE_BINARY_DIR}/libmerge.exe)
install(PROGRAMS ${CMAKE_BINARY_DIR}/libmerge.exe DESTINATION ${DEST_DIR} CONFIGURATIONS Release)
