/* Copyright 2011 Michael Berganovsky

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

#include <zlib-1.2.6\zlib.h>
#include <zlib-1.2.6\contrib\minizip\unzip.h>
#include <zlib-1.2.6\contrib\minizip\zip.h>
#include <zlib-1.2.6\contrib\minizip\iowin32.h>

#endif // __IMPORT_PRECOMP_H__
