# Copyright (C) 1995-2007 MySQL AB
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# There are special exceptions to the terms and conditions of the GPL
# as it is applied to this software. View the full text of the exception
# in file LICENSE.exceptions in the top-level directory of this software
# distribution.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
#
# The MySQL Connector/ODBC is licensed under the terms of the
# GPL, like most MySQL Connectors. There are special exceptions
# to the terms and conditions of the GPL as it is applied to
# this software, see the FLOSS License Exception available on
# mysql.com.
IF (WIN32)
	IF ($ENV{MSYSTEM} STREQUAL "MINGW32")
		FILE(GLOB SEARCH_INCLUDES "${CMAKE_SOURCE_DIR}/mysql-*-win32/include" "$ENV{MYSQL_DIR}/include")
		FILE(GLOB SEARCH_LIBS "${CMAKE_SOURCE_DIR}/mysql-*-win32/lib" "$ENV{MYSQL_DIR}/lib")
	ELSEIF ($ENV{MSYSTEM} STREQUAL "MINGW64")
		FILE(GLOB SEARCH_INCLUDES "${CMAKE_SOURCE_DIR}/mysql-*-winx64/include" "$ENV{MYSQL_DIR}/include")
		FILE(GLOB SEARCH_LIBS "${CMAKE_SOURCE_DIR}/mysql-*-winx64/lib" "$ENV{MYSQL_DIR}/lib")
	ELSE()
		FILE(GLOB SEARCH_INCLUDES "$ENV{MYSQL_DIR}/include")
		FILE(GLOB SEARCH_LIBS "$ENV{MYSQL_DIR}/lib")
	ENDIF()	
ENDIF()

IF (WIN32)
	FIND_PATH(MYSQL_INCLUDE_DIR mysql.h PATHS ${SEARCH_INCLUDES})
ELSE (WIN32)
	FIND_PATH(MYSQL_INCLUDE_DIR mysql.h
		/usr/include/mysql
		/usr/local/include/mysql
		/opt/mysql/mysql/include
		/opt/mysql/mysql/include/mysql
		/opt/mysql/include
		/opt/local/include/mysql5
		/usr/local/mysql/include
		/usr/local/mysql/include/mysql)
ENDIF (WIN32)

IF (WIN32)
	FIND_LIBRARY(MYSQL_LIBRARIES NAMES mysqld PATHS ${SEARCH_LIBS})
ELSE (WIN32)
	FIND_LIBRARY(MYSQL_LIBRARIES NAMES mysqld
		PATHS
		/usr/lib/mysql
		/usr/local/lib/mysql
		/usr/local/mysql/lib
		/usr/local/mysql/lib/mysql
		/opt/local/mysql5/lib
		/opt/local/lib/mysql5/mysql
		/opt/mysql/mysql/lib/mysql
		/opt/mysql/lib/mysql)
ENDIF (WIN32)

IF(MYSQL_INCLUDE_DIR)
	FILE(READ "${MYSQL_INCLUDE_DIR}/mysql_version.h" _MYSQL_VERSION_CONTENTS)
	IF(_MYSQL_VERSION_CONTENTS)
		STRING(REGEX REPLACE "^.*MYSQL_SERVER_VERSION[\t\ ]+\"([0-9.]+).*$" "\\1" MYSQL_VERSION "${_MYSQL_VERSION_CONTENTS}")
	ENDIF()
ENDIF(MYSQL_INCLUDE_DIR)

IF(MYSQL_LIBRARIES)
	GET_FILENAME_COMPONENT(MYSQL_LIB_DIR ${MYSQL_LIBRARIES} PATH)
ENDIF(MYSQL_LIBRARIES)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MySQL REQUIRED_VARS MYSQL_LIBRARIES MYSQL_INCLUDE_DIR VERSION_VAR MYSQL_VERSION)

IF (MYSQL_FOUND)
	INCLUDE_DIRECTORIES(${MYSQL_INCLUDE_DIR})
ENDIF ()
