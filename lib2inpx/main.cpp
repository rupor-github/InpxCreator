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

#include <precomp.h>

#include <util.h>

using namespace std;
using namespace std::tr1;
using namespace boost::posix_time;
using namespace boost::gregorian;

namespace po = boost::program_options;

static bool g_no_import        = false;
static bool g_ignore_dump_date = false;
static bool g_clean_when_done  = false;

enum checking_type
{
   eFileExt = 0,
   eFileType,
   eIgnore
};
static checking_type g_strict  = eFileExt;

enum fb2_parsing
{
   eReadNone = 0,
   eReadLast,
   eReadAll
};
static fb2_parsing   g_read_fb2 = eReadNone;

enum processing_type
{
   eFB2 = 0,
   eUSR,
   eAll
};
static processing_type g_process = eFB2;

enum database_format
{
   eDefault = 0,
   e20100206,
   e20100317,
   e20100411
};
static database_format g_format = eDefault;

enum inpx_format
{
   e1X = 0,
   e2X
};
static inpx_format g_inpx_format = e1X;

static long   g_last_fb2 = 0;
static string g_update;
static string g_db_name  = "librusec";

static bool   g_clean_authors = false;
static bool   g_clean_aliases = false;
static bool   g_follow_links  = false;

static string sep  = "\x04";

//                    AUTHOR     ;    GENRE     ;     TITLE           ; SERIES ; SERNO ; FILE ;    SIZE   ;  LIBID    ;    DEL   ;    EXT     ;       DATE        ;    LANG    ; LIBRATE  ; KEYWORDS ;
static char* dummy = "dummy:" "\x04" "other:" "\x04" "dummy record" "\x04"   "\x04"  "\x04" "\x04" "1" "\x04" "%d" "\x04" "1" "\x04" "EXT" "\x04" "2000-01-01" "\x04" "en" "\x04" "0" "\x04"     "\x04" "\r\n";

static char* options_pattern[] = { "%s",
                                   "--defaults-file=%s/mysql.ini",
                                   "--datadir=%s/data",
                                   "--language=%s/language",
                                   "--skip-innodb",
                                   "--key-buffer-size=64M",
                                   NULL };

class mysql_connection : boost::noncopyable
{
   enum { num_options = sizeof( options_pattern ) / sizeof( char * ) };

   char* m_options[ num_options ];

   public:

      mysql_connection( const char* module_path, const char* name ) : m_mysql( NULL )
      {
         if( 0 == m_initialized )
         {
            for( int ni = 0; ni < num_options; ++ni )
            {
               char* pattern = options_pattern[ ni ];
               if( NULL == pattern )
               {
                  m_options[ ni ] = NULL;
                  break;
               }
               char* mem = new char[ MAX_PATH * 2 ] ;

               if( 0 == ni )
                  sprintf( mem, pattern, name );
               else
                  sprintf( mem, pattern, module_path );
               m_options[ ni ] = mem;
            }
            if( mysql_library_init( num_options - 1, m_options, NULL ) )
               throw runtime_error( tmp_str( "Could not initialize MySQL library (%s)", mysql_error( m_mysql ) ) );

            if( NULL == (m_mysql = mysql_init( NULL )) )
               throw runtime_error( "Not enough memory to initialize MySQL library" );

            mysql_options( m_mysql, MYSQL_READ_DEFAULT_GROUP, "embedded" );
            mysql_options( m_mysql, MYSQL_OPT_USE_EMBEDDED_CONNECTION, NULL );
         }
         ++m_initialized;

         if( NULL == mysql_real_connect( m_mysql, NULL, NULL, NULL, NULL, 0, NULL, 0 ) )
            throw runtime_error( tmp_str( "Unable to connect (%s)", mysql_error( m_mysql ) ) );
      }

      ~mysql_connection()
      {
         if( m_initialized > 0 )
         {
            m_initialized--;

            if( NULL != m_mysql )
               mysql_close( m_mysql );

            if( 0 == m_initialized )
            {
               for( int ni = 0; ni < num_options; ++ni )
               {
                  char* pattern = m_options[ ni ];
                  if( NULL == pattern )
                     break;
                  delete[] pattern;
               }
               mysql_library_end();
            }
         }
      }

      operator MYSQL*() const
         { return m_mysql; };

      operator bool() const
         { return NULL != m_mysql; }

      void query( const string& statement, bool throw_error = true ) const
      {
         int res = mysql_query( m_mysql, statement.c_str() );
         if( throw_error && res )
            throw runtime_error( tmp_str( "Query error (%d) %s\n%s", mysql_errno( m_mysql ), mysql_error( m_mysql ), statement.c_str() ) );
      }

      void real_query( const char* statement, long length, bool throw_error = true ) const
      {
         int res = mysql_real_query( m_mysql, statement, length );
         if( throw_error && res )
            throw runtime_error( tmp_str( "Real query error (%d) %s\n", mysql_errno( m_mysql ), mysql_error( m_mysql ) ) );
      }

   private:

      static
         int m_initialized;

      MYSQL* m_mysql;
};

int mysql_connection::m_initialized = 0;

class mysql_results : boost::noncopyable
{
   public:

      mysql_results( const mysql_connection& mysql ) : m_mysql( mysql ), m_results( NULL )
      {
         m_results = mysql_store_result( m_mysql );
      }

      ~mysql_results()
      {
         if( NULL != m_results )
            mysql_free_result( m_results );
      }

      MYSQL_ROW fetch_row() const
      {
         if( NULL != m_results )
            return mysql_fetch_row( m_results );
         else
            return NULL;
      }

   private:
      const      mysql_connection& m_mysql;
      MYSQL_RES* m_results;

};

bool is_after_last( const string& book_id )
{
   return (g_last_fb2 < atol( book_id.c_str() ));
}

bool is_fictionbook( const string& file )
{
   string name = file, ext;
   size_t pos  = name.rfind( "." );

   if( string::npos != pos )
   {
      ext = name.substr( pos + 1 );
      name.erase( pos );
   }

   return ((0 == _stricmp( ext.c_str(), "fb2")) && is_numeric( name ) );
}

void clean_directory( const char* path )
{
   _finddata_t fd;
   string      spec( path );

   spec += "*.*";

   auto_ffn dir( _findfirst( spec.c_str(), &fd ) );

   if( ! dir )
      throw runtime_error( tmp_str( "Unable to clean database directory \"%s\"", spec.c_str() ) );

   do
   {
      if( (0 != strcmp( fd.name, ".." )) && (0 != strcmp( fd.name, "." )) )
      {
         string name( path );
         name += fd.name;

         if( 0 != _unlink( name.c_str() ) )
            throw runtime_error( tmp_str( "Unable to delete file \"%s\"", name.c_str() ) );
      }
   }
   while( 0 == _findnext( dir, &fd ) );

   if( 0 != _rmdir( path ) )
      throw runtime_error( tmp_str( "Unable to delete directory \"%s\"", path ) );
}

void prepare_mysql( const char* path )
{
   bool   rc = true;
   string config;

   config = string( path ) + "/mysql.ini";

   if( 0 != _access( config.c_str(), 6 ) )
   {
      ofstream out( config.c_str() );

      if( out )
      {
         out << "[server]"   << endl;
         out << "[embedded]" << endl;
         out << "console"    << endl;
      }
      else
         throw runtime_error( tmp_str( "Unable to open file \"%s\"", config.c_str() ) );
   }

   config = string( path ) + "/data";

   if( 0 != _access( config.c_str(), 6 ) )
   {
      if( 0 != _mkdir( config.c_str() ) )
         throw runtime_error( tmp_str( "Unable to create directory \"%s\"", config.c_str() ) );
   }
}

string get_dump_date( const string& file )
{
   size_t       length;
   string       res, buf;
   ifstream     in( file.c_str() );
   stringstream ss;

   regex dump_date( "^--\\s*Dump\\scompleted\\son\\s(\\d{4}-\\d{2}-\\d{2}).*$" );
   match_results<string::const_iterator> mr;

   if( !in )
      throw runtime_error( tmp_str( "Cannot open file \"%s\"", file.c_str() ) );

   in.seekg( 0, ios::end );
   length = in.tellg();
   in.seekg( max( 0, (int)(length - 300)), ios::beg );

   ss << in.rdbuf();

   if( !in && !in.eof() )
      throw runtime_error( tmp_str( "Problem reading file \"%s\"", file.c_str() ) );

   buf = ss.str();

   if( regex_search( buf, mr, dump_date ) )
      res = mr[ 1 ];

   return res;
}

void get_book_author( const mysql_connection& mysql, const string& book_id, string& author )
{
   MYSQL_ROW record;

   author.erase();

   string str;

   str = (eDefault == g_format) ? "SELECT `AvtorId` FROM `libavtor` WHERE BookId=" :
                                  "SELECT `aid` FROM `libavtor` WHERE bid=";

   str += book_id;
   str += (e20100411 == g_format) ? " AND role=\"a\";" : ";";

   mysql.query( str );

   mysql_results avtor_ids( mysql );

   str = (eDefault == g_format) ? "SELECT `FirstName`,`MiddleName`,`LastName` FROM libavtorname WHERE AvtorId=" :
                                  "SELECT `FirstName`,`MiddleName`,`LastName` FROM libavtorname WHERE aid=" ;

   while( record = avtor_ids.fetch_row() )
   {
      string good_author_id( record[ 0 ] );

      mysql.query( string( "SELECT `GoodId`,`BadId` FROM libavtoraliase WHERE BadId=" ) + good_author_id + ";" );
      {
         mysql_results ids( mysql );

         if( record = ids.fetch_row() )
         {
            if( 0 != strlen( record[ 0 ] ) )
               good_author_id = record[ 0 ];
         }
      }

      mysql.query( str + good_author_id + ";" );
      {
         mysql_results author_name( mysql );

         if( record = author_name.fetch_row() )
         {
            author += fix_data( record[ 2 ], g_limits.A_Family ); author += ",";
            author += fix_data( record[ 0 ], g_limits.A_Name   ); author += ",";
            author += fix_data( record[ 1 ], g_limits.A_Middle ); author += ":";
         }
      }
   }
   if( author.size() < 4 )
      author = "неизвестный,автор,:";
   else
      remove_crlf( author );
}

void get_book_rate( const mysql_connection& mysql, const string& book_id, string& rate )
{
   MYSQL_ROW record;

   rate.erase();

   string str = (eDefault == g_format) ? "SELECT ROUND(AVG(Rate),0) FROM librate WHERE BookId =" :
                                         "SELECT ROUND(AVG(Rate),0) FROM librate WHERE bid =" ;

   mysql.query( str + book_id + ";" );

   mysql_results res( mysql );

   if( record = res.fetch_row() )
   {
      if( 0 != record[ 0 ] )
         rate = record[ 0 ];
   }
}

void get_book_genres( const mysql_connection& mysql, const string& book_id, string& genres )
{
   MYSQL_ROW  record;

   genres.erase();

   string str = (eDefault == g_format) ? "SELECT GenreID FROM libgenre WHERE BookId=" :
                                         "SELECT gid FROM libgenre WHERE bid=" ;

   mysql.query( str + book_id + ";" );

   mysql_results genre_ids( mysql );

   if     ( e20100206 == g_format ) { str = "SELECT GenreCode FROM libgenrelist WHERE gid=";     }
   else if( e20100317 == g_format ) { str = "SELECT GenreCode FROM libgenrelist WHERE gid=";     }
   else if( e20100411 == g_format ) { str = "SELECT code FROM libgenrelist WHERE gid=";          }
   else                             { str = "SELECT GenreCode FROM libgenrelist WHERE GenreId="; }

   while( record = genre_ids.fetch_row() )
   {
      string genre_id( record[ 0 ] );

      mysql.query( str + genre_id + ";" );

      mysql_results genre_code( mysql );

      if( record = genre_code.fetch_row() )
      {
         genres += record[ 0 ];
         genres += ":";
      }
   }

   remove_crlf( genres );

   if( genres.empty() )
      genres = "other:";
}

void get_book_squence( const mysql_connection& mysql, const string& book_id, string& sequence, string& seq_numb )
{
   MYSQL_ROW record;

   sequence.erase();
   seq_numb.erase();

   string str;

   if     ( e20100206 == g_format ) { str = "SELECT `sid`,`SeqNumb` FROM libseq WHERE bid=";      }
   else if( e20100317 == g_format ) { str = "SELECT `sid`,`sn` FROM libseq WHERE bid=";           }
   else if( e20100411 == g_format ) { str = "SELECT `sid`,`sn` FROM libseq WHERE bid=";           }
   else                             { str = "SELECT `SeqId`,`SeqNumb` FROM libseq WHERE BookId="; }

   mysql.query( str + book_id + ";" );

   mysql_results seq( mysql );

   if( record = seq.fetch_row() )
   {
      string seq_id( record[ 0] );

      seq_numb = record[ 1 ];

      str = (eDefault == g_format) ? "SELECT SeqName FROM libseqname WHERE SeqId=" :
                                     "SELECT SeqName FROM libseqname WHERE sid=" ;

      mysql.query( str + seq_id + ";" );

      mysql_results seq_name( mysql );

      if( record = seq_name.fetch_row() )
      {
         sequence += fix_data( record[ 0 ], g_limits.S_Title );
      }
   }
   remove_crlf( sequence );
}

void process_book( const mysql_connection& mysql, MYSQL_ROW record, const string& file_name, const string& ext, string& inp )
{
   inp.erase();

   string book_id      ( record[ 0 ] ),
          book_title   ( record[ 1 ] ),
          book_filesize( record[ 2 ] ),
          book_type    ( record[ 3 ] ),
          book_deleted ( record[ 4 ] ),
          book_time    ( record[ 5 ] ),
          book_lang    ( record[ 6 ] ),
          book_kwds    ( record[ 7 ] ),
          book_file    ( file_name   );

   string book_author,
          book_genres,
          book_sequence,
          book_sequence_num,
          book_rate;

   if( eFileExt == g_strict )
      book_type = ext;

   get_book_author ( mysql, book_id, book_author );
   get_book_genres ( mysql, book_id, book_genres );
   get_book_rate   ( mysql, book_id, book_rate );
   get_book_squence( mysql, book_id, book_sequence, book_sequence_num );

   if( remove_crlf( book_file ) )
      book_file = "";

   remove_crlf( book_title ); book_title = fix_data( book_title.c_str(), g_limits.Title );
   remove_crlf( book_kwds );  book_kwds  = fix_data( book_kwds.c_str(),  g_limits.KeyWords );

   book_time.erase( book_time.find( " " ) ); // Leave date only

   // AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;

   inp  = book_author;
   inp += sep;
   inp += book_genres;
   inp += sep;
   inp += book_title;
   inp += sep;
   inp += book_sequence;
   inp += sep;
   inp += book_sequence_num;
   inp += sep;
   inp += ((eIgnore == g_strict) && (0 != _stricmp( ext.c_str(), book_type.c_str() ))) ? "" : book_file;
   inp += sep;
   inp += book_filesize;
   inp += sep;
   inp += book_id;
   inp += sep;
   inp += book_deleted;
   inp += sep;
   inp += book_type;
   inp += sep;
   inp += book_time;
   inp += sep;
   inp += book_lang;
   inp += sep;
   inp += book_rate;
   inp += sep;
   inp += book_kwds;
   inp += sep;
   inp += "\r\n";
}

bool process_from_fb2( const unzip& uz, const string& book_id, string& inp, string& err )
{
   bool rc = false;

   DOUT(  printf( "   Processing %s\n", book_id.c_str() ); );

   const int buffer_size = 4096;
   boost::scoped_array< char > buffer( new char[ buffer_size ] );

   err.erase();
   inp.erase();

   try
   {
      unzip_reader    ur( uz );
      fb2_parser      fb;
      unz_file_info64 fi;

      uz.current ( fi );

      int  len                 = 0;
      bool continue_processing = true;

      while( continue_processing && (0 < (len = ur( buffer.get(), buffer_size ))) )
         continue_processing = fb( buffer.get(), len );

      if( continue_processing )
         fb( buffer.get(), 0, true );

      // AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;

      string authors, genres;
      for( vector< string >::const_iterator it = fb.m_authors.begin(); it != fb.m_authors.end(); ++it )
         authors += (*it) + ":";
      for( vector< string >::const_iterator it = fb.m_genres.begin(); it != fb.m_genres.end(); ++it )
         genres += (*it) + ":";

      inp  = authors;
      inp += sep;
      inp += genres;
      inp += sep;
      inp += fb.m_title;
      inp += sep;
      inp += fb.m_seq_name;
      inp += sep;
      inp += fb.m_seq;
      inp += sep;
      inp += book_id;
      inp += sep;
      inp += tmp_str( "%d", fi.uncompressed_size );
      inp += sep;
      inp += book_id;
      inp += sep;
//      inp += book_deleted;
      inp += sep;
      inp += "fb2";
      inp += sep;
      inp += to_iso_extended_string( date( ((fi.dosDate >> 25) & 0x7F) + 1980, ((fi.dosDate >> 21) & 0x0F), ((fi.dosDate >> 16) & 0x1F) ) ) ;
      inp += sep;
      inp += fb.m_language;
      inp += sep;
//      inp += book_rate;
      inp += sep;
      inp += fb.m_keywords;
      inp += sep;
      inp += "\r\n";

      rc = true;
   }
   catch( exception& e )
   {
      inp.erase();
      err = e.what();
   }
   return rc;
}

void name_to_bookid( const string& file, string& book_id, string& ext )
{
   book_id = file;
   size_t pos = book_id.rfind( "." );
   if( string::npos != pos )
   {
      ext = book_id.substr( pos + 1 );
      book_id.erase( pos );
   }
}

void process_local_archives( const mysql_connection& mysql, const zip& zz, const string& archives_path )
{
   _finddata_t       fd;
   vector< string >  files;
   string            spec( archives_path + "*.zip" );

   auto_ffn archives( _findfirst( spec.c_str(), &fd ) );

   if( ! archives )
      throw runtime_error( tmp_str( "Unable to process archive path (%d) \"%s\"", errno, spec.c_str() ) );

   do
   {
       if( g_follow_links )
       {
          if( 0 != (fd.attrib & FILE_ATTRIBUTE_REPARSE_POINT) )
             files.push_back( fd.name );
          else
          {
             if( fd.size > 22 )
                files.push_back( fd.name );
          }
       }
       else
       {
          if( (0 == (fd.attrib & FILE_ATTRIBUTE_REPARSE_POINT)) && (fd.size > 22) )
             files.push_back( fd.name );
       }
   }
   while( 0 == _findnext( archives, &fd ) );

   if( ! g_update.empty() )
   {
      vector< string >::iterator it;

      struct finder_fb2_t
      {
         bool operator()( const string& arg )
            { return (0 == _strnicmp( arg.c_str(), "fb2-", 4 )); }
      } finder_fb2;

      struct finder_usr_t
      {
         bool operator()( const string& arg )
            { return (0 == _strnicmp( arg.c_str(), "usr-", 4 )); }
      } finder_usr;

      for( it = find_if( files.begin(), files.end(), finder_usr ); it != files.end(); )
         files.erase( it, files.end() );

      sort( files.begin(), files.end() );

      it = find( files.begin(), files.end(), g_update + ".zip" );

      if( it == files.end() )
         throw runtime_error( tmp_str( "Unable to locate daily archive \"%s.zip\"", g_update.c_str() ) );

      if( it != files.begin() )
         files.erase( files.begin(), it );

      it  = find_if( files.begin(), files.end(), finder_fb2 );

      if( it != files.end() )
         files.erase( it, files.end() );
   }

   if( 0 == files.size() )
      throw runtime_error( tmp_str( "No archives are available for processing \"%s\"", archives_path.c_str() ) );

   cout << endl << "Archives processing - " << files.size() << " file(s) [" << archives_path << "]" << endl << endl;

   for( vector< string >::const_iterator it = files.begin(); it != files.end(); ++it )
   {
      vector< string > errors;
      string name = "\"" + *it  + "\"";
      name.append( max( 0, (int)(25 - name.length()) ), ' ' );

      string out_inp_name( *it );
      out_inp_name.replace( out_inp_name.end() - 3, out_inp_name.end(), string("inp") );

      long       records = 0, dummy_records = 0, fb2_records = 0;
      zip_writer zw( zz, out_inp_name, false );

      cout << "Processing - " << name ;

      timer ftd;
      unzip uz( (archives_path + *it).c_str() );

      for( unsigned int ni = 0; ni < uz.count(); ++ni )
      {
         string inp, book_id, ext, stmt;
         bool fdummy = false;
         bool fb2 = is_fictionbook( uz.current() );

         if( fb2 )
         {
            if( (g_process == eAll) || ((g_process == eFB2)) )
            {
               name_to_bookid( uz.current(), book_id, ext );
               if     ( e20100206 == g_format ) { stmt = "SELECT `bid`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook WHERE bid=" + book_id + ";";       }
               else if( e20100317 == g_format ) { stmt = "SELECT `bid`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook WHERE bid=" + book_id + ";";       }
               else if( e20100411 == g_format ) { stmt = "SELECT `bid`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook WHERE bid=" + book_id + ";";       }
               else                             { stmt = "SELECT `BookId`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook WHERE BookId=" + book_id + ";"; }
            }
            else
               fdummy = true;
         }
         else
         {
            if( (g_process == eAll) || ((g_process == eUSR)) )
            {
               name_to_bookid( uz.current(), book_id, ext );
               if     ( e20100206 == g_format ) { stmt = "SELECT B.bid, B.Title, B.FileSize, B.FileType, B.Deleted, B.Time, B.Lang, B.KeyWords FROM libbook B, libfilename F WHERE B.bid = F.BookID AND F.FileName = \"" + uz.current() + "\";";       }
               else if( e20100317 == g_format ) { stmt = "SELECT B.bid, B.Title, B.FileSize, B.FileType, B.Deleted, B.Time, B.Lang, B.KeyWords FROM libbook B, libfilename F WHERE B.bid = F.BookID AND F.FileName = \"" + uz.current() + "\";";       }
               else if( e20100411 == g_format ) { stmt = "SELECT B.bid, B.Title, B.FileSize, B.FileType, B.Deleted, B.Time, B.Lang, B.KeyWords FROM libbook B, libfilename F WHERE B.bid = F.BookID AND F.FileName = \"" + uz.current() + "\";";       }
               else                             { stmt = "SELECT B.BookId, B.Title, B.FileSize, B.FileType, B.Deleted, B.Time, B.Lang, B.KeyWords FROM libbook B, libfilename F WHERE B.BookId = F.BookID AND F.FileName = \"" + uz.current() + "\";"; }
            }
            else
               fdummy = true;
         }

         if( ! book_id.empty() )
         {
            MYSQL_ROW record;

            mysql.query( stmt );

            mysql_results book( mysql );

            if( record = book.fetch_row() )
            {
               process_book( mysql, record, book_id, ext, inp );
            }
            else
            {
               string err;

               if( fb2 && ((eReadAll == g_read_fb2) || ((eReadLast == g_read_fb2) && is_after_last(book_id))))
               {
                  if( ! process_from_fb2( uz, book_id, inp, err ) )
                     errors.push_back( "       Skipped " + book_id + ".fb2 in archive due to \"" + err + "\"" );
                  else
                     ++fb2_records;
               }
            }
         }

         if( 0 == inp.size() )
         {
            inp    = tmp_str( dummy, ni + 1 );
            fdummy = true;
         }

         if( fdummy )
            ++dummy_records;
         else
         {
            ++records;

            if( ! zw.is_open() )
               zw.open();

            for( ; dummy_records > 0; dummy_records-- )
               zw( tmp_str( dummy, ni - dummy_records + 1 ) );

            zw( inp );
         }

         if( ni < (uz.count() - 1) )
            uz.move_next();
      }

      zw.close();

      cout << " - done in " << ftd.passed() ;
      if( 0 == records )
         cout << " ==> Not in database!" << endl;
      else
         cout << " (" << records - fb2_records << ":" << fb2_records << ":" << dummy_records << " records)" << endl;

      for( vector< string >::const_iterator it = errors.begin(); it != errors.end(); ++it )
         cout << *it << endl;
   }
}

void bookid_to_name( const mysql_connection& mysql, const string& book_id, string& name, string& ext )
{
   MYSQL_ROW record;

   name.erase();
   ext.erase();

   string str = (eDefault == g_format) ? "SELECT `BookId`, `FileName` FROM libfilename WHERE BookId =" :
                                         "SELECT `bid`, `FileName` FROM libfilename WHERE BookId =" ;

   mysql.query( str + book_id + ";" );

   mysql_results names( mysql );

   if( record = names.fetch_row() )
   {
      name_to_bookid( record[ 1 ], name, ext );
   }
}

void process_database( const mysql_connection& mysql, const zip& zz )
{
   MYSQL_ROW  record;
   string     sep( "\x04" );

   string stmt, out_inp_name( "online.inp" );

   if( g_process == eAll )
      stmt = (eDefault == g_format) ? "SELECT `BookId`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook ORDER BY BookId;" :
                                      "SELECT `bid`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook ORDER BY bid;" ;
   else if( g_process == eFB2 )
      stmt = (eDefault == g_format) ? "SELECT `BookId`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook WHERE FileType = 'fb2' ORDER BY BookId;" :
                                      "SELECT `bid`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook WHERE FileType = 'fb2' ORDER BY bid;";
   else if( g_process = eUSR )
      stmt = (eDefault == g_format) ? "SELECT `BookId`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook WHERE FileType != 'fb2' ORDER BY BookId;" :
                                      "SELECT `bid`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords` FROM libbook WHERE FileType != 'fb2' ORDER BY bid;";

   long       current = 0;
   long       records = 0;
   zip_writer zw( zz, out_inp_name, false );

   cout << endl << "Database processing" << endl;

   timer ftd;

   mysql.query( stmt );

   mysql_results books( mysql );

   while( record = books.fetch_row() )
   {
      if( ++current % 3000 == 0 )
         cout << ".";

      string inp, file_name, ext( "fb2" );

      if( (0 == _stricmp( record[ 3 ], ext.c_str() )) && is_numeric( record[ 0 ] ) )
         file_name = record[ 0 ];
      else
         bookid_to_name( mysql, record[ 0 ], file_name, ext );

      process_book( mysql, record, file_name, ext, inp );

      if( 0 != inp.size() )
      {
         ++records;

         if( ! zw.is_open() )
            zw.open();

         zw( inp );
      }
   }

   cout << endl << current << " records ";

   zw.close();

   cout << "done in " << ftd.passed() << endl;
}

int main( int argc, char *argv[] )
{
   int               rc = 1;

   string            spec, path, inpx, comment, comment_fname, collection_comment, inp_path,
                     inpx_name, dump_date, full_date, db_name;

   vector< string >  archives_path;

   _finddata_t       fd;

   const char*       file_name;
   char              module_path[ MAX_PATH + 1 ];

   try
   {
      vector< string > files;
      timer            td;

      ::GetModuleFileName( NULL, module_path, sizeof module_path );

      file_name = separate_file_name( module_path );

      po::options_description options( "options" );
      options.add_options()
         ( "help",                               "Print help message"  )
         ( "ignore-dump-date",                   "Ignore date in the dump files, use current UTC date instead" )
         ( "clean-when-done",                    "Remove MYSQL database after processing" )
         ( "process",     po::value< string >(), "What to process - \"fb2\", \"usr\", \"all\" (default: fb2)" )
         ( "strict",      po::value< string >(), "What to put in INPX as file type - \"ext\", \"db\", \"ignore\" (default: ext). ext - use real file extension. db - use file type from database. ignore - ignore files with file extension not equal to file type" )
         ( "no-import",                          "Do not import dumps, just check dump time and use existing database" )
         ( "db-name",     po::value< string >(), "Name of MYSQL database (default: librusec)" )
         ( "archives",    po::value< string >(), "Path(s) to off-line archives. Multiple entries should be separated by ';'. Each path must be valid and must point to some archives, or processing would be aborted. (If not present - entire database in converted for online usage)" )
         ( "read-fb2",    po::value< string >(), "When archived book is not present in the database - try to parse fb2 in archive to get information. \"all\" - do it for all absent books, \"last\" - only process books with ids larger than last database id (If not present - no fb2 parsing)" )
         ( "inpx",        po::value< string >(), "Full name of output file (default: <db_name>_<db_dump_date>.inpx)" )
         ( "comment",     po::value< string >(), "File name of template (UTF-8) for INPX comment" )
         ( "update",      po::value< string >(), "Starting with \"<arg>.zip\" produce \"daily_update.zip\" (Works only for \"fb2\")" )
         ( "db-format",   po::value< string >(), "Database format, change date (YYYY-MM-DD). Supported: 2010-02-06, 2010-03-17, 2010-04-11. (Default - old librusec format before 2010-02-06)" )
         ( "clean-authors",                      "Clean duplicate authors in libavtorname table" )
         ( "clean-aliases",                      "Clean libavtoraliase table" )
         ( "follow-links",                       "Do not ignore symbolic links" )
         ( "inpx-format", po::value< string >(), "INPX format, Supported: 1.x, 2.x, (Default - old MyHomeLib format 1.x)" )
         ( "quick-fix",                          "Enforce MyHomeLib database size limits, works with fix-config parameter. (default: MyHomeLib 1.6.2 constrains)" )
         ( "fix-config",  po::value< string >(), "Allows to specify configuration file with MyHomeLib database size constrains" )
         ;

      po::options_description hidden;
      hidden.add_options()
         ( "dump-dir",  po::value< string >() )
         ;

      po::positional_options_description p;
      p.add( "dump-dir", -1 );

      po::options_description cmdline_options;
      cmdline_options.add( options ).add( hidden );

      po::variables_map vm;
      po::store( po::command_line_parser( argc, argv ).options( cmdline_options ).positional( p ).run(), vm );
      po::notify( vm );

      if( vm.count( "help" ) || ! vm.count( "dump-dir" ) )
      {
         cout << endl;
         cout << "Import file (INPX) preparation tool for MyHomeLib" << endl;
         cout << "Version 5.1 (MYSQL " << MYSQL_SERVER_VERSION << ")" << endl;
         cout << endl;
         cout << "Usage: " << file_name << " [options] <path to SQL dump files>" << endl << endl;
         cout << options << endl;
         rc = 0; goto E_x_i_t;
      }

      if( vm.count( "process" ) )
      {
         string opt = vm[ "process" ].as< string >();
         if( 0 == _stricmp( opt.c_str(), "fb2" ) )
            g_process = eFB2;
         else if( 0 == _stricmp( opt.c_str(), "usr" ) )
            g_process = eUSR;
         else if( 0 == _stricmp( opt.c_str(), "all" ) )
            g_process = eAll;
         else
         {
            cout << endl << "Warning: unknown processing type, assuming FB2 only!" << endl;
            g_process = eFB2;
         }
      }

      if( vm.count( "read-fb2" ) )
      {
         string opt = vm[ "read-fb2" ].as< string >();
         if( 0 == _stricmp( opt.c_str(), "all" ) )
            g_read_fb2 = eReadAll;
         else if( 0 == _stricmp( opt.c_str(), "last" ) )
            g_read_fb2 = eReadLast;
         else
         {
            cout << endl << "Warning: unknown read-fb2 action, assuming none!" << endl;
            g_read_fb2 = eReadNone;
         }
      }

      if( vm.count( "strict" ) )
      {
         string opt = vm[ "strict" ].as< string >();
         if( 0 == _stricmp( opt.c_str(), "ext" ) )
            g_strict = eFileExt;
         else if( 0 == _stricmp( opt.c_str(), "db" ) )
            g_strict = eFileType;
         else if( 0 == _stricmp( opt.c_str(), "ignore" ) )
            g_strict = eIgnore;
         else
         {
            cout << endl << "Warning: unknown strictness, will use file extensions!" << endl;
            g_strict = eFileExt;
         }
      }

      if( vm.count( "db-format" ) )
      {
         string opt = vm[ "db-format" ].as< string >();
         if( 0 == _stricmp( opt.c_str(), "2010-02-06" ) )
         {
            g_format = e20100206;
         }
         else if( 0 == _stricmp( opt.c_str(), "2010-03-17" ) )
         {
            g_format = e20100317;
         }
         else if( 0 == _stricmp( opt.c_str(), "2010-04-11" ) )
         {
            g_format = e20100411;
         }
         else
         {
            cout << endl << "Warning: unknown database format, will use default!" << endl;
            g_format = eDefault;
         }
      }

      if( vm.count( "inpx-format" ) )
      {
         string opt = vm[ "inpx-format" ].as< string >();
         if( 0 == _stricmp( opt.c_str(), "1.x" ) )
         {
            g_inpx_format = e1X;
         }
         else if( 0 == _stricmp( opt.c_str(), "2.x" ) )
         {
            g_inpx_format = e2X;
         }
         else
         {
            cout << endl << "Warning: unknown INPX format, will use default!" << endl;
            g_inpx_format = e1X;
         }
      }

      if( vm.count( "quick-fix" ) )
      {
         g_fix = true;

         string config;
         if( vm.count( "fix-config" ) )
            config = vm[ "fix-config" ].as< string >();

         initialize_limits( config );
      }

      if( vm.count( "ignore-dump-date" ) )
         g_ignore_dump_date = true;

      if( vm.count( "clean-when-done" ) )
         g_clean_when_done = true;

      if( vm.count( "no-import" ) )
         g_no_import = true;

      if( vm.count( "clean-authors" ) )
         g_clean_authors = true;

      if( vm.count( "clean-aliases" ) )
         g_clean_aliases = true;

      if( vm.count( "follow-links" ) )
         g_follow_links = true;

      if( vm.count( "dump-dir" ) )
         path = vm[ "dump-dir" ].as< string >();

      if( vm.count( "db-name" ) )
         g_db_name = vm[ "db-name" ].as< string >();

      db_name  = g_db_name;
      db_name += "_";

      if( vm.count( "comment" ) )
      {
         comment_fname = vm[ "comment" ].as< string >();

         if( 0 != _access( comment_fname.c_str(), 4 ) )
            cout << endl << "Warning: Ignoring wrong comment file: " << comment_fname.c_str() << endl;
         else
         {
            ifstream     in( comment_fname.c_str() );
            stringstream ss;

            if( !in )
               throw runtime_error( tmp_str( "Cannot open comment file \"%s\"", comment_fname.c_str() ) );

            ss << in.rdbuf();

            if( !in && !in.eof() )
               throw runtime_error( tmp_str( "Problem reading comment file \"%s\"", comment_fname.c_str() ) );

            comment = ss.str();
         }
      }

      if( vm.count( "archives" ) )
      {
         string tmp = vm[ "archives" ].as< string >();

         split( archives_path, tmp.c_str(), ";" );
      }

      if( ! archives_path.empty() )
      {
         for( vector< string >::iterator it = archives_path.begin(); it != archives_path.end(); ++it )
         {
            if( 0 != _access( (*it).c_str(), 4 ) )
               throw runtime_error( tmp_str( "Wrong path to archives \"%s\"", (*it).c_str() ) );

            normalize_path( (*it) );
         }

         if( (eFB2 == g_process) && vm.count( "update" ) )
         {
            if( 1 == archives_path.size() )
            {
               g_update = vm[ "update" ].as< string >();

               if( 0 != _access( (archives_path[ 0 ] + g_update + ".zip").c_str(), 4 ) )
                  throw runtime_error( tmp_str( "Unable to find daily archive \"%s.zip\"", g_update.c_str() ) );
            }
            else
               throw runtime_error( "daily_update.zip could only be built from a single archive path" );
         }
      }

      if( 0 != _access( path.c_str(), 4 ) )
            throw runtime_error( tmp_str( "Wrong source path \"%s\"", path.c_str() ) );

      normalize_path( path );

      spec  = path;
      spec += "*.sql";

      {
         auto_ffn dumps( _findfirst( spec.c_str(), &fd ) );

         if( ! dumps )
            throw runtime_error( tmp_str( "Unable to process source path (%d) \"%s\"", errno, spec.c_str() ) );

         do
         {
            files.push_back( fd.name );

            if( ! g_ignore_dump_date )
            {
               if( 0 == dump_date.size() )
               {
                  dump_date = get_dump_date( path + fd.name );
               }
               else
               {
                  string new_dump_date = get_dump_date( path + fd.name );

                  if( dump_date != new_dump_date )
                     throw runtime_error( tmp_str( "Source dump files do not have the same date (%s) \"%s\" (%s)", dump_date.c_str(), fd.name, new_dump_date.c_str() ) );
               }
            }
         }
         while( 0 == _findnext( dumps, &fd ) );
      }

      if( ! g_no_import && (0 == files.size()) )
         throw runtime_error( tmp_str( "No SQL dumps are available for importing \"%s\"", path.c_str() ) );

      full_date = (0 == dump_date.size()) ? to_iso_extended_string( date( day_clock::universal_day() ) ) : dump_date ;
      dump_date = full_date;

      dump_date.erase( 4, 1 );
      dump_date.erase( 6, 1 );

      inpx_name = db_name;
      db_name  += dump_date ;

      if( g_process == eUSR )
         inpx_name += "usr_" + dump_date;
      else if( g_process == eAll )
         inpx_name += "all_" + dump_date;
      else
      {
         if( ! archives_path.empty() )
            inpx_name += dump_date;
         else
            inpx_name += "online_" + dump_date;
      }

      if( ! g_update.empty() )
         inpx_name = "daily_update";

      normalize_path( module_path );
      prepare_mysql ( module_path );

      if( vm.count( "inpx" ) )
         inpx = vm[ "inpx" ].as< string >();
      else
      {
         inpx  = module_path;
         inpx += "/data/";
         inpx += inpx_name;
         inpx += g_update.empty() ? ".inpx" : ".zip";
      }

      {
         mysql_connection mysql( module_path, g_db_name.c_str() );

         if( ! g_no_import )
            cout << endl << "Creating MYSQL database \"" << db_name << "\"" << endl << endl;

         mysql.query( string( "CREATE DATABASE IF NOT EXISTS " + db_name + " CHARACTER SET=utf8;" ) );
         mysql.query( string( "USE " ) + db_name + ";" );
         mysql.query( string( "SET NAMES 'utf8';" ) );

         for( vector< string >::const_iterator it = files.begin(); (it != files.end()) && (! g_no_import) ; ++it )
         {
            string name = "\"" + *it  + "\"";
            name.append( max( 0, (int)(25 - name.length()) ), ' ' );

            cout << "Importing - " << name ;

            timer    ftd;

            string   buf, line;
            ifstream in( (path + *it).c_str() );
            regex    sl_comment( "^(--|#).*" );
            bool     authors = g_clean_authors ? (string::npos != it->find( "libavtorname" ))   : false;
            bool     aliases = g_clean_aliases ? (string::npos != it->find( "libavtoraliase" )) : false;

            if( !in )
               throw runtime_error( tmp_str( "Cannot open file \"%s\"", (*it).c_str() ) );

            getline( in, buf );
            while( in )
            {
               if( 0 != buf.size() )
               {
                  size_t pos = buf.find_first_not_of( " \t" );
                  if( pos != 0 )
                     buf.erase( 0, pos );

                  if( ! regex_match( buf, sl_comment ) && ! (authors && (0 == buf.find( "UNIQUE KEY `fullname` (`FirstName`,`MiddleName`,`LastName`,`NickName`)," ))) )
                  {
                     pos = buf.rfind( ';' );
                     if( pos == string::npos )
                     {
                        if( aliases && (0 == buf.find( "`AliaseId` int(11) NOT NULL auto_increment," )) )
                        {
                           line += buf;
                           line += "`dummyId` int(11) NOT NULL default '0',";
                        }
                        else
                        {
                           line += buf;
                        }
                        buf.erase();
                        goto C_o_n_t;
                     }
                     else
                     {
                        if( aliases && (0 == buf.find( "INSERT INTO `libavtoraliase`" )) )
                        {
                           size_t len = strlen( "INSERT INTO `libavtoraliase`" );
                           line += "INSERT INTO `libavtoraliase` (dummyId, BadId, GoodId)";
                           buf.erase( 0, len );
                           pos -= len;
                        }
                        line += buf.substr( 0, pos + 1 );
                        buf.erase( 0, pos + 1 );
                     }

                     mysql.real_query( line.c_str(), (int)line.size() );

                     line = buf;
                  }
               }
      C_o_n_t: getline( in, buf );
            }

            cout << " - done in " << ftd.passed() << endl;
         }

         if( g_clean_authors )
         {
            MYSQL_ROW record;

            mysql.query( "SELECT Firstname, Middlename, Lastname, Nickname FROM libavtorname GROUP BY Firstname, Middlename, Lastname, Nickname HAVING COUNT(*) > 1" );
            mysql_results dupes( mysql );

            cout << endl << "Processing duplicate authors" << endl;

            while( record = dupes.fetch_row() )
            {
               MYSQL_ROW record1;
               string    first;
               bool      not_first = false;

               mysql.query( tmp_str( "SELECT aid FROM libavtorname WHERE Firstname='%s' AND Middlename='%s' AND Lastname='%s' AND Nickname='%s' ORDER by aid;", record[ 0 ], record[ 1 ], record[ 2 ], record[ 3 ] ) );
               mysql_results aids( mysql );

               while( record1 = aids.fetch_row() )
               {
                  MYSQL_ROW record2;

                  if( first.empty() )
                     first = record1[ 0 ];
                  else
                     not_first = true;

                  mysql.query( string( "SELECT COUNT(*) FROM libavtor WHERE aid=" ) + record1[ 0 ] + ";" );
                  mysql_results books( mysql );

                  if( (record2 = books.fetch_row()) && (0 != strlen( record2[ 0 ] )) )
                  {
                     long count = atol( record2[ 0 ] );

                     if( not_first )
                     {
                        cout << "   De-duping author " << setw(8) << record1[ 0 ]
                                                       << " (" << setw(4) << count << ") : "
                                                       << utf8_to_OEM( record[ 2 ] ) << "-"
                                                       << utf8_to_OEM( record[ 0 ] ) << "-"
                                                       << utf8_to_OEM( record[ 1 ] ) << endl;
                        if( 0 < count )
                           mysql.query( tmp_str( "UPDATE libavtor SET aid=%s WHERE aid=%s;", first.c_str(), record1[ 0 ] ), false );
                        mysql.query( tmp_str( "DELETE FROM libavtorname WHERE aid=%s;", record1[ 0 ] ) );
                     }
                     else
                     {
                        if( 0 == count )
                        {
                           cout << "*  De-duping author " << setw(8) << record1[ 0 ]
                                                          << " (" << setw(4) << count << ") : "
                                                          << utf8_to_OEM( record[ 2 ] ) << "-"
                                                          << utf8_to_OEM( record[ 0 ] ) << "-"
                                                          << utf8_to_OEM( record[ 1 ] ) << endl;

                           mysql.query( tmp_str( "DELETE FROM libavtorname WHERE aid=%s;", record1[ 0 ] ) );
                           first.clear();
                        }
                     }
                  }
               }
            }
         }

         if( eReadLast == g_read_fb2 )
         {
            MYSQL_ROW record;

            string str = (eDefault == g_format) ? "SELECT MAX(`BookId`) FROM libbook WHERE FileType = 'fb2';" :
                                                  "SELECT MAX(`bid`) FROM libbook WHERE FileType = 'fb2';" ;

            mysql.query( str );

            mysql_results last( mysql );

            if( record = last.fetch_row() )
               g_last_fb2 = atol( record[ 0 ] );

            cout << endl << "Largest FB2 book id in database: " << g_last_fb2 << endl;
         }

         if( comment.empty() )
         {
            if( ! archives_path.empty() )
            {
               if( g_process == eFB2 )
                  comment = tmp_str( "%s FB2 - %s\r\n%s\r\n65536\r\nЛокальные архивы библиотеки %s (FB2) %s", g_db_name.c_str(), full_date.c_str(), inpx_name.c_str(), g_db_name.c_str(), full_date.c_str() );
               else if( g_process == eUSR )
                  comment = tmp_str( "%s USR - %s\r\n%s\r\n65537\r\nЛокальные архивы библиотеки %s (не-FB2) %s", g_db_name.c_str(), full_date.c_str(), inpx_name.c_str(), g_db_name.c_str(), full_date.c_str() );
               else if( g_process == eAll )
                  comment = tmp_str( "%s ALL - %s\r\n%s\r\n65537\r\nЛокальные архивы библиотеки %s (все) %s", g_db_name.c_str(), full_date.c_str(), inpx_name.c_str(), g_db_name.c_str(), full_date.c_str() );
            }
            else
            {
               comment = tmp_str( "%s FB2 online - %s\r\n%s\r\n134283264\r\nOnline коллекция %s %s", g_db_name.c_str(), full_date.c_str(), inpx_name.c_str(), g_db_name.c_str(), full_date.c_str() );
            }
         }
         else
         {
            comment = tmp_str( comment.c_str(), inpx_name.c_str() );
         }

         collection_comment  = "\xEF\xBB\xBF";
         collection_comment += comment;

         comment = utf8_to_ANSI( comment.c_str() );

         zip zz( inpx, comment );

         if( archives_path.empty() )
            process_database( mysql, zz );
         else
         {
            for( vector< string >::const_iterator it = archives_path.begin(); it != archives_path.end(); ++it )
               process_local_archives( mysql, zz, (*it) );
         }

         if( e2X == g_inpx_format  )
         {
            zip_writer zw( zz, "collection.info" );
            zw( collection_comment );
            zw.close();
         }
         else
         {
            zip_writer zw( zz, "structure.info" );
            zw( "AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;" );
            zw.close();
         }
         
         zip_writer zw( zz, "version.info" );
         zw( dump_date + "\r\n" );
         zw.close();        

         zz.close();

         cout << endl << "Complete processing took " << td.passed() << endl;
      }

      if( g_clean_when_done )
      {
         string db_dir( module_path ); db_dir += "/data/"; db_dir += db_name; db_dir += "/";
         clean_directory( db_dir.c_str() );
      }

      rc = 0;
   }
   catch( exception& e )
   {
      cerr << endl << endl << "***ERROR: " << e.what() << endl;
   }

E_x_i_t:

   return rc;
}
