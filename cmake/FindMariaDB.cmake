if(CMAKE_CROSSCOMPILING OR WIN32)
	if($ENV{MSYSTEM} STREQUAL "MINGW32")
		file(GLOB SEARCH_INCLUDES LIST_DIRECTORIES true "$ENV{HOME}/mariadb-*-win32/include/mysql" "${CMAKE_SOURCE_DIR}/mariadb-*-win32/include/mysql" "$ENV{MYSQL_DIR}/include")
		file(GLOB SEARCH_LIBS LIST_DIRECTORIES true "$ENV{HOME}/mariadb-*-win32/lib" "${CMAKE_SOURCE_DIR}/mariadb-*-win32/lib" "$ENV{MYSQL_DIR}/lib")
	elseif($ENV{MSYSTEM} STREQUAL "MINGW64")
		file(GLOB SEARCH_INCLUDES LIST_DIRECTORIES true "$ENV{HOME}/mariadb-*-winx64/include/mysql" "${CMAKE_SOURCE_DIR}/mariadb-*-winx64/include/mysql" "$ENV{MYSQL_DIR}/include")
		file(GLOB SEARCH_LIBS LIST_DIRECTORIES true "$ENV{HOME}/mariadb-*-winx64/lib" "${CMAKE_SOURCE_DIR}/mariadb-*-winx64/lib" "$ENV{MYSQL_DIR}/lib")
	else()
		file(GLOB SEARCH_INCLUDES LIST_DIRECTORIES true "$ENV{MYSQL_DIR}/include")
		file(GLOB SEARCH_LIBS LIST_DIRECTORIES true "$ENV{MYSQL_DIR}/lib")
	endif()
else()
	file(GLOB SEARCH_INCLUDES LIST_DIRECTORIES true "$ENV{HOME}/mariadb-*-linux-*/include/mysql" "${CMAKE_SOURCE_DIR}/mariadb-*-linux-*/include/mysql" "$ENV{MYSQL_DIR}/include")
	file(GLOB SEARCH_LIBS LIST_DIRECTORIES true "$ENV{HOME}/mariadb-*-linux-*/lib" "${CMAKE_SOURCE_DIR}/mariadb-*-linux-*/lib" "$ENV{MYSQL_DIR}/lib")
endif()

if(CMAKE_CROSSCOMPILING OR WIN32)
	find_path(MARIADB_INCLUDE_DIR NAMES mysql.h 
		PATHS ${SEARCH_INCLUDES} NO_CMAKE_FIND_ROOT_PATH)
else()
	find_path(MARIADB_INCLUDE_DIR NAMES mysql.h
		PATHS ${SEARCH_INCLUDES}
			/usr/include/mysql
			/usr/local/include/mysql
			/opt/mysql/mysql/include
			/opt/mysql/mysql/include/mysql
			/opt/mysql/include
			/opt/local/include/mysql5
			/usr/local/mysql/include
			/usr/local/mysql/include/mysql
			NO_DEFAULT_PATH)
endif()

if(CMAKE_CROSSCOMPILING OR WIN32)
	find_library(MARIADB_LIBRARIES NAMES mysqld
		PATHS ${SEARCH_LIBS} NO_CMAKE_FIND_ROOT_PATH)
else()
	find_library(MARIADB_LIBRARIES NAMES mysqld
        	PATHS ${SEARCH_LIBS}
			/usr/lib/mysql
			/usr/local/lib/mysql
			/usr/local/mysql/lib
			/usr/local/mysql/lib/mysql
			/opt/local/mysql5/lib
			/opt/local/lib/mysql5/mysql
			/opt/mysql/mysql/lib/mysql
			/opt/mysql/lib/mysql
			NO_DEFAULT_PATH)
endif()

if(MARIADB_INCLUDE_DIR)
	file(READ "${MARIADB_INCLUDE_DIR}/mysql_version.h" _MARIADB_VERSION_CONTENTS)
	if(_MARIADB_VERSION_CONTENTS)
        	string(REGEX REPLACE "^.*MYSQL_SERVER_VERSION[\t\ ]+\"([0-9.]+)-MariaDB.*$" "\\1" MARIADB_VERSION "${_MARIADB_VERSION_CONTENTS}")
	Endif()
endif()

if(MARIADB_LIBRARIES)
	get_filename_component(MARIADB_LIB_DIR ${MARIADB_LIBRARIES} PATH)
endif()
mark_as_advanced(MARIADB_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MariaDB FOUND_VAR MARIADB_FOUND REQUIRED_VARS MARIADB_LIBRARIES MARIADB_INCLUDE_DIR MARIADB_VERSION VERSION_VAR MARIADB_VERSION)

if(MARIADB_FOUND)
	include_directories(${MARIADB_INCLUDE_DIR})
    set(MYSQL_LIB_DIR "${MARIADB_LIB_DIR}")
    set(MYSQL_LIBRARIES "${MARIADB_LIBRARIES}")
    set(MYSQL_INCLUDE_DIR "${MARIADB_INCLUDE_DIR}")
    set(MYSQL_FOUND 1)
endif()
