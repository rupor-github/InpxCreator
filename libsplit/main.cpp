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

#include <precomp.h>

#include <util.h>

using namespace std;
using namespace std::tr1;
using namespace boost::posix_time;
using namespace boost::gregorian;

namespace po = boost::program_options;

__int64 get_file_size( const string& name )
{
   __int64 res = 0;
   WIN32_FILE_ATTRIBUTE_DATA data;

   if( GetFileAttributesEx( name.c_str(), GetFileExInfoStandard, &data ) )
   {
      res = data.nFileSizeLow;
   }
   else
      throw runtime_error( tmp_str( "Unable to obtain file length \"%s\"", name.c_str() ) );

   return res;
}

int main( int argc, char *argv[] )
{
   int               rc = 1;

   string            from, to, spec;

   long              rsize;

   _finddata_t       fd;
   vector< long >    files;

   const char*       file_name;
   char              module_path[ MAX_PATH + 1 ];

   __int64           archive_size;

   long              total_count = 0;
   timer             td;

   try
   {
      ::GetModuleFileName( NULL, module_path, sizeof module_path );

      file_name = separate_file_name( module_path );

      po::options_description options( "options" );
      options.add_options()
         ( "help",                                               "Print help message"  )
         ( "from", po::value< string >(),                        "Directory with fb2 books" )
         ( "to",   po::value< string >(),                        "Directory to put resulting archives into" )
         ( "size", po::value<long>(&rsize)->default_value(2000), "Individual archive size in MB")
         ;

      po::variables_map vm;

      po::store( po::parse_command_line( argc, argv, options ), vm );
      po::notify( vm );

      if( vm.count( "help" ) || ! vm.count( "from" ) || ! vm.count( "to" ) )
      {
         cout << endl;
         cout << "Tool to prepare library archives" << endl;
         cout << "Version 1.0" << endl;
         cout << endl;
         cout << "Usage: " << file_name << " [options]" << endl << endl;
         cout << options << endl;
         rc = 0; goto E_x_i_t;
      }

      if( rsize > 2047 )
      {
         cout << endl << "Warning: size is too big, assuming 2047MB!" << endl;
         rsize = 2047;
      }

      archive_size = (__int64)1024*1024*rsize;

      if( vm.count( "from" ) )
         from = vm[ "from" ].as< string >();

      if( vm.count( "to" ) )
         to = vm[ "to" ].as< string >();

      if( ! from.empty() )
      {
         if( 0 != _access( from.c_str(), 4 ) )
            throw runtime_error( tmp_str( "Wrong path to books \"%s\"", from.c_str() ) );

          normalize_path( from );
      }

      if( ! to.empty() )
      {
         if( 0 != _access( to.c_str(), 6 ) )
            throw runtime_error( tmp_str( "Wrong path to archives \"%s\"", to.c_str() ) );

          normalize_path( to );
      }

      cout << endl << "Processing books...";

      spec  = from;
      spec += "*.fb2";

      auto_ffn books_dir( _findfirst( spec.c_str(), &fd ) );

      if( ! books_dir )
         throw runtime_error( tmp_str( "Unable to process books path (%d) \"%s\"", errno, spec.c_str() ) );

      do
      {
         string name( fd.name );

         name.erase( name.end() - 4, name.end() );

         if( is_numeric( name ) )
         {
            files.push_back( atol( name.c_str() ) );
         }
      }
      while( 0 == _findnext( books_dir, &fd ) );

      if( files.empty() )
         throw runtime_error( "No books found!" );

      sort( files.begin(), files.end() );

      cout << endl << files.size() << " books found. Range: " << *files.begin() << " - " << *(files.end() - 1) << endl;

      string temp_name = to + "temp.zip";

      vector< long >::const_iterator it = files.begin();
      string now = to_iso_extended_string( second_clock::universal_time() );

      while( it != files.end() )
      {
         __int64 current_size = archive_size;
         long  count = 0;
         timer ftd;
         bool  new_zip = true;

         long book_start = *it;
         long book_end   = 0;

         while( (current_size > 0) && (it != files.end()) )
         {           
            zip zz( temp_name, now, new_zip );

            new_zip = false;

            for( ; it != files.end(); ++it )
            {
               tmp_str      book_name( "%d.fb2", *it );
               ifstream     in( from + book_name );
               stringstream ss;

               book_end = *it;

               if( !in )
                  throw runtime_error( tmp_str( "Cannot open book file \"%s\"", book_name.c_str() ) );

               ss << in.rdbuf();

               if( !in && !in.eof() )
                  throw runtime_error( tmp_str( "Problem reading book file \"%s\"", book_name.c_str() ) );

               zip_writer zw( zz, book_name ); zw( ss.str() );

               count++;

               current_size -= ss.str().size() ;

               if( current_size <= 0 )
               {
                  ++it;
                  break;
               }
            }

            zz.close();

            current_size = archive_size - get_file_size( temp_name );
         }

         if( 0 != rename( temp_name.c_str(), tmp_str( "%sfb2-%06d-%06d.zip", to.c_str(), book_start, book_end ) ) )
            throw runtime_error( tmp_str( "Unable to rename temp.zip to \"fb2-%06d-%06d.zip\"", book_start, book_end ) );

         cout << endl << "Processed: " << setw(8) << count << " books. Range: "
                      << setw(8) << book_start << " - "
                      << setw(8) << book_end   << ". Done in - "
                      << ftd.passed();

         new_zip = true;

         total_count += count;
      }

      cout << "Processing of " << total_count << " books completed in " << td.passed() << endl;

      rc = 0;
   }
   catch( exception& e )
   {
      cerr << endl << endl << "***ERROR: " << e.what() << endl;
   }

E_x_i_t:

   return rc;
}
