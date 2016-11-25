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
if(WIN32)
	if($ENV{MSYSTEM} STREQUAL "MINGW32")
		file(GLOB SEARCH_INCLUDES "${CMAKE_SOURCE_DIR}/mysql-*-win32/include" "$ENV{MYSQL_DIR}/include")
		file(GLOB SEARCH_LIBS "${CMAKE_SOURCE_DIR}/mysql-*-win32/lib" "$ENV{MYSQL_DIR}/lib")
	elseif($ENV{MSYSTEM} STREQUAL "MINGW64")
		file(GLOB SEARCH_INCLUDES "${CMAKE_SOURCE_DIR}/mysql-*-winx64/include" "$ENV{MYSQL_DIR}/include")
		file(GLOB SEARCH_LIBS "${CMAKE_SOURCE_DIR}/mysql-*-winx64/lib" "$ENV{MYSQL_DIR}/lib")
	else()
		file(GLOB SEARCH_INCLUDES "$ENV{MYSQL_DIR}/include")
		file(GLOB SEARCH_LIBS "$ENV{MYSQL_DIR}/lib")
	endif()
else()
	file(GLOB SEARCH_INCLUDES "${CMAKE_SOURCE_DIR}/mysql-*-linux-glibc2.5-*/include" "$ENV{MYSQL_DIR}/include")
	file(GLOB SEARCH_LIBS "${CMAKE_SOURCE_DIR}/mysql-*-linux-glibc2.5-*/lib" "$ENV{MYSQL_DIR}/lib")
endif()

if(WIN32)
    find_path(MYSQL_INCLUDE_DIR NAMES mysql.h PATHS ${SEARCH_INCLUDES})
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

if(WIN32)
	find_library(MYSQL_LIBRARIES NAMES mysqld PATHS ${SEARCH_LIBS})
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
		string(REGEX REPLACE "^.*MYSQL_SERVER_VERSION[\t\ ]+\"([0-9.]+).*$" "\\1" MYSQL_VERSION "${_MYSQL_VERSION_CONTENTS}")
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
