# Copied form MySQL.cmake

if(CMAKE_CROSSCOMPILING OR WIN32)
	if($ENV{MSYSTEM} STREQUAL "MINGW32")
		file(GLOB SEARCH_INCLUDES "${CMAKE_SOURCE_DIR}/mariadb-*-win32/include/mysql" "$ENV{MYSQL_DIR}/include")
		file(GLOB SEARCH_LIBS "${CMAKE_SOURCE_DIR}/mariadb-*-win32/lib" "$ENV{MYSQL_DIR}/lib")
	elseif($ENV{MSYSTEM} STREQUAL "MINGW64")
		file(GLOB SEARCH_INCLUDES "${CMAKE_SOURCE_DIR}/mariadb-*-winx64/include/mysql" "$ENV{MYSQL_DIR}/include")
		file(GLOB SEARCH_LIBS "${CMAKE_SOURCE_DIR}/mariadb-*-winx64/lib" "$ENV{MYSQL_DIR}/lib")
	else()
		file(GLOB SEARCH_INCLUDES "$ENV{MYSQL_DIR}/include")
		file(GLOB SEARCH_LIBS "$ENV{MYSQL_DIR}/lib")
	endif()
else()
	file(GLOB SEARCH_INCLUDES "${CMAKE_SOURCE_DIR}/mariadb-*-linux-glibc*/include/mysql" "$ENV{MYSQL_DIR}/include")
	file(GLOB SEARCH_LIBS "${CMAKE_SOURCE_DIR}/mariadb-*-linux-glibc*/lib" "$ENV{MYSQL_DIR}/lib")
endif()

if(CMAKE_CROSSCOMPILING OR WIN32)
    find_path(MYSQL_INCLUDE_DIR NAMES mysql.h PATHS ${SEARCH_INCLUDES} NO_CMAKE_FIND_ROOT_PATH)
else()
    find_path(MYSQL_INCLUDE_DIR NAMES mysql.h
        PATHS ${SEARCH_INCLUDES}
		/usr/include/mysql
		/usr/local/include/mysql
		/opt/mysql/mysql/include
		/opt/mysql/mysql/include/mysql
		/opt/mysql/include
		/opt/local/include/mysql5
		/usr/local/mysql/include
		/usr/local/mysql/include/mysql)
endif()

if(CMAKE_CROSSCOMPILING OR WIN32)
	find_library(MYSQL_LIBRARIES NAMES mysqld PATHS ${SEARCH_LIBS} NO_CMAKE_FIND_ROOT_PATH)
else()
	find_library(MYSQL_LIBRARIES NAMES mysqld
		PATHS ${SEARCH_LIBS}
		/usr/lib/mysql
		/usr/local/lib/mysql
		/usr/local/mysql/lib
		/usr/local/mysql/lib/mysql
		/opt/local/mysql5/lib
		/opt/local/lib/mysql5/mysql
		/opt/mysql/mysql/lib/mysql
		/opt/mysql/lib/mysql)
endif()

if(MYSQL_INCLUDE_DIR)
	file(READ "${MYSQL_INCLUDE_DIR}/mysql_version.h" _MYSQL_VERSION_CONTENTS)
	if(_MYSQL_VERSION_CONTENTS)
        string(REGEX REPLACE "^.*MYSQL_SERVER_VERSION[\t\ ]+\"([0-9.]+-MariaDB).*$" "\\1" MYSQL_VERSION "${_MYSQL_VERSION_CONTENTS}")
	Endif()
endif(MYSQL_INCLUDE_DIR)

if(MYSQL_LIBRARIES)
	get_filename_component(MYSQL_LIB_DIR ${MYSQL_LIBRARIES} PATH)
endif(MYSQL_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL REQUIRED_VARS MYSQL_LIBRARIES MYSQL_INCLUDE_DIR VERSION_VAR MYSQL_VERSION)

if(MYSQL_FOUND)
	include_directories(${MYSQL_INCLUDE_DIR})
endif()
