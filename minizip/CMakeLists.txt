project(minizip)

cmake_minimum_required(VERSION 2.6)

find_package(ZLIB REQUIRED)

set(SOURCES ioapi.c unzip.c zip.c)
set(HEADERS ioapi.h unzip.h zip.h)

if(APPLE OR ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
	add_definitions("-DUSE_FILE32API")
endif()

add_library(minizip ${SOURCES} ${HEADERS})
target_link_libraries(minizip z)
