/* Copyright 2009 Michael Berganovsky

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef   __IMPORT_PRECOMP_H__
#define   __IMPORT_PRECOMP_H__

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <io.h>
#include <direct.h>
#include <time.h>

#ifdef WIN64
#  include <mysql-5.1.42-win64\include\mysql.h>
#else
#  include <mysql-5.1.42-win32\include\mysql.h>
#endif

#include <algorithm>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <vector>
#include <map>

#include <boost/utility.hpp>
#include <boost/scoped_array.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/property_iter_range.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>

#include <zlib-1.2.3\zlib.h>
#include <zlib-1.2.3\contrib\minizip\unzip.h>
#include <zlib-1.2.3\contrib\minizip\zip.h>
#include <zlib-1.2.3\contrib\minizip\iowin32.h>

#include <expat-2.0.1\expat.h>

#endif // __IMPORT_PRECOMP_H__
