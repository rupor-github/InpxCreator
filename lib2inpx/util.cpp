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

#include <atlstr.h>
#include <mlang.h>

#define DO_NOT_INCLUDE_XML 1
#include <util.h>

#define STARTING_SIZE 0x400
#define ENDING_SIZE   0x10000

using namespace std;

#ifndef DO_NOT_INCLUDE_PARSER

bool g_fix = false;
db_limits g_limits;

void initialize_limits( const string& config )
{
   g_limits.A_Name    =
   g_limits.A_Middle  =
   g_limits.A_Family  = 128;
   g_limits.Title     = 150;
   g_limits.KeyWords  = 255;
   g_limits.S_Title   = 80;

   if( ! config.empty() && (0 == _access( config.c_str(), 4 )) )
   {
      using boost::property_tree::ptree;
      ptree pt;

      read_info( config, pt );

      g_limits.A_Name    = pt.get< size_t >( "Limits.A_Name",    g_limits.A_Name );
      g_limits.A_Middle  = pt.get< size_t >( "Limits.A_Middle",  g_limits.A_Middle );
      g_limits.A_Family  = pt.get< size_t >( "Limits.A_Family",  g_limits.A_Family );
      g_limits.Title     = pt.get< size_t >( "Limits.Title",     g_limits.Title );
      g_limits.KeyWords  = pt.get< size_t >( "Limits.KeyWords",  g_limits.KeyWords );
      g_limits.S_Title   = pt.get< size_t >( "Limits.S_Title",   g_limits.S_Title );
   }
}

string fix_data( const char* pstr, size_t max_len )
{
   if( g_fix )
   {
      wstring wstr = utf8_to_ucs2( pstr );

      if( wstr.size() >= max_len )
         wstr = wstr.substr( 0, max_len - 1 );

      return ucs2_to_utf8( wstr.c_str() );
   }
   else
      return string( pstr );
}

bool remove_crlf( string& str )
{
   bool   rc = false;
   size_t pos;

   while( string::npos != (pos = str.find( "\r\n" )) )
   {
      str.replace( pos, 2, string( " " ) );
      rc = true;
   }
   while( string::npos != (pos = str.find( "\n" )) )
   {
      str.erase( pos, 1 );
      rc = true;
   }
   return rc;
}

#endif // DO_NOT_INCLUDE_PARSER

bool is_numeric( const string& str )
{
   for( string::const_iterator it = str.begin(); it != str.end(); ++it )
   {
      if( ! isdigit( *it ) )
         return false;
   }
   return true;
}

const char *separate_file_name( char *buf )
{
char *ptr = buf + strlen( buf );

   while( ptr != buf )
   {
      if( (*ptr == '\\') || (*ptr == '/') )
      {
         *ptr = '\0';
         ptr++;
         break;
      }
      ptr--;
   }
   return ptr;
}

void normalize_path( string& path, bool trailing )
{
   for( string::iterator it = path.begin(); it != path.end(); ++it )
   {
      if( *it == '\\' )
         *it = '/';
   }
   if( trailing )
   {
      if( *(path.end() - 1) != '/' )
         path += '/';
   }
}

void normalize_path( char* path )
{
   for( int ni = 0; ni < strlen( path ); ++ni )
   {
      if( path[ ni ] == '\\' )
         path[ ni ] = '/';
   }
}

wstring utf8_to_ucs2( const char* ptr )
{
   size_t len = MultiByteToWideChar( CP_UTF8, 0, ptr, -1, NULL, 0 );

   boost::scoped_array< wchar_t > buf( new wchar_t[ len + 1 ] );

   MultiByteToWideChar( CP_UTF8, 0, ptr, -1, buf.get(), (int)len );

   buf.get()[ len ] = L'\0';

   return wstring( buf.get() );
}

string ucs2_to_utf8( const wchar_t* ptr )
{
   size_t len = WideCharToMultiByte( CP_UTF8, 0, ptr, -1, NULL, 0, NULL, NULL );

   boost::scoped_array< char > buf( new char[ len + 1 ] );

   WideCharToMultiByte( CP_UTF8, 0, ptr, -1, buf.get(), (int)len, NULL, NULL );

   buf.get()[ len ] = L'\0';

   return string( buf.get() );
}

string utf8_to_ANSI( const char* ptr )
{
   size_t len = MultiByteToWideChar( CP_UTF8, 0, ptr, -1, NULL, 0 );

   boost::scoped_array< wchar_t > buf( new wchar_t[ len + 1 ] );

   MultiByteToWideChar( CP_UTF8, 0, ptr, -1, buf.get(), (int)len );

   buf.get()[ len ] = L'\0';

   len = ::WideCharToMultiByte( CP_ACP, 0, buf.get(), -1, NULL, 0, NULL, NULL );

   boost::scoped_array< char > buf2( new char[ len + 1 ] );

   WideCharToMultiByte( CP_ACP, 0, buf.get(), -1, buf2.get(), (int)len, NULL, NULL );

   buf2.get()[ len ] = L'\0';

   return string( buf2.get() );
}

string utf8_to_OEM( const char* ptr )
{
   size_t len = MultiByteToWideChar( CP_UTF8, 0, ptr, -1, NULL, 0 );

   boost::scoped_array< wchar_t > buf( new wchar_t[ len + 1 ] );

   MultiByteToWideChar( CP_UTF8, 0, ptr, -1, buf.get(), (int)len );

   buf.get()[ len ] = L'\0';

   len = WideCharToMultiByte( CP_OEMCP, 0, buf.get(), -1, NULL, 0, NULL, NULL );

   boost::scoped_array< char > buf2( new char[ len + 1 ] );

   WideCharToMultiByte( CP_OEMCP, 0, buf.get(), -1, buf2.get(), (int)len, NULL, NULL );

   buf2.get()[ len ] = L'\0';

   return string( buf2.get() );
}

void split( vector< string >& result, const char *str, const char *delim, bool combine_delimiters )
{
   result.clear();

   if( NULL == str  ||  '\0' == *str )
   {
      goto E_x_i_t;
   }

   if( NULL == delim  ||  '\0' == *delim )
   {
      result.push_back( str );
      goto E_x_i_t;
   }

   if( combine_delimiters )
   {
      size_t size = strlen( str ) + 1;
      boost::scoped_array< char > buffer( new char[ size ] );

      strcpy_s( buffer.get(), size, str );

      char *ctx;
      char *ptr = strtok_s( buffer.get(), delim, &ctx );

      while( NULL != ptr )
      {
         result.push_back( ptr );
         ptr = strtok_s( NULL, delim, &ctx );
      }
   }
   else
   {
      const char *ptr  = str,
                 *ptr1 = strpbrk( ptr, delim );

      while( NULL != ptr1 )
      {
         result.push_back( string( ptr, ptr1-ptr ) );
         ptr = ptr1 + 1;
         ptr1 = strpbrk( ptr, delim );
      }

      result.push_back( ptr );
   }

E_x_i_t:
   ;
}

tmp_str::tmp_str( const char *format, ... )
{
int     ni   = -1;
va_list marker;

   va_start( marker, format );
   for( int size = STARTING_SIZE; size <= ENDING_SIZE; size *= 2  )
   {
      boost::scoped_array< char > buf( new char[ size + 1 ] );
      ni = _vsnprintf( buf.get(), size, format, marker );
      if( -1 != ni  )
      {
         assign( buf.get(), ni );
         break;
      }
   }
   va_end( marker );
}

bool zip::m_func_set = false;
zlib_filefunc64_def zip::m_ffunc;

bool unzip::m_func_set = false;
zlib_filefunc64_def unzip::m_ffunc;

#ifndef DO_NOT_INCLUDE_PARSER

class multi_lang
{
   public:

      multi_lang() : m_initialized( false ), m_pml( NULL )
      {
         HRESULT hr = CoInitialize( NULL );
         if( ! SUCCEEDED( hr ) )
            throw std::runtime_error( tmp_str( "MLANG CoInitialize(%08x)", hr ) );
         m_initialized = true;

         hr = CoCreateInstance( CLSID_CMultiLanguage, NULL, CLSCTX_ALL, IID_IMultiLanguage, (void**)&m_pml );
         if( ! SUCCEEDED( hr ) )
            throw std::runtime_error( tmp_str( "MLANG CLSID_CMultiLanguage(%08x)", hr ) );
      }

     ~multi_lang()
      {
         if( NULL != m_pml ) m_pml->Release();
         if( m_initialized ) CoUninitialize();
      }

      UINT getCP( const char* name )
      {
         MIMECSETINFO mi;
         CComBSTR     bstr( name );
         HRESULT      hr = m_pml->GetCharsetInfo( bstr, &mi );

         return SUCCEEDED( hr ) ? mi.uiCodePage : 0 ;
      }

      bool isMBCS( UINT cp )
         { return (932 == cp) || (936 == cp) || (949 == cp) || (950 == cp) || (1361 == cp); }

   private:

      bool            m_initialized;
      IMultiLanguage* m_pml;
};

static int XMLCALL xml_convert( void *data, const char *s )
{
   UINT    cp  = (UINT)data;
   wchar_t sym = 0;

   if( 0 == MultiByteToWideChar( cp, MB_ERR_INVALID_CHARS, s, 2, &sym, 1 ) )
      return -1;
   else
      return sym;
}

int fb2_parser::on_unknown_encoding( const XML_Char* name, XML_Encoding* info )
{
static multi_lang mlang;

   UINT cp = mlang.getCP( name );

   if( 0 == cp )
      return XML_STATUS_ERROR;

   bool mbcs = mlang.isMBCS( cp );

   info->data    = (void*)cp;
   info->convert = mbcs ? xml_convert : NULL;
   info->release = NULL;

   for( int ni = 0; ni < 256; ++ni )
   {
      if( mbcs && IsDBCSLeadByteEx( cp, ni ) )
      {
         info->map[ ni ] = -2;
      }
      else
      {
         wchar_t sym = 0;

         if( 0 == MultiByteToWideChar( cp, MB_ERR_INVALID_CHARS, (char*)&ni, 1, &sym, 1 ) )
            info->map[ ni ] = -1;
         else
            info->map[ ni ] = sym;
      }
   }
   return XML_STATUS_OK;
}

bool fb2_parser::check_cardinality( int count, const cardinality_t& constrains )
{
   int from = constrains.first,
       to   = constrains.second;

   // simplified to ignore extras

   return (to < 0) || (count <= to);
}

void fb2_parser::on_start_element( const char* name, const char** attrs )
{
   using namespace boost;

   if( m_stop_processing )
      return;

   name_map_t  name_map  = get( vertex_name,               m_graph );
   props_map_t props_map = get( vertex_element_properties, m_graph );

   bool found = false;
   int  count = XML_GetSpecifiedAttributeCount( m_parser );

   graph_traits< graph_t >::adjacency_iterator ai, ai_end;
   for( tie( ai, ai_end ) = adjacent_vertices( m_current, m_graph ); ai != ai_end; ++ai )
   {
      if( name == name_map[ *ai ] )
      {
         element_props props( props_map[ *ai ] );

         if( check_cardinality( ++props.count, props.cardinality ) )
         {
            for( int ni = 0; (ni < count) && attrs[ ni ]; ni += 2 )
               props.attributes.insert( make_pair( attrs[ ni ], attrs[ ni + 1 ] ) );

            props.text.erase();

            props_map[ *ai ] = props;

            m_path.push_back( m_current );
            m_current = *ai;
         }
         else
            DOUT( printf( "DBG*** Cardinality broken: %s\n", name ); );

         found = true;
         break;
      }
   }
   DOUT( if( ! found ) printf( "DBG*** Unexpected element starts: %s\n", name ); );
}

void fb2_parser::on_end_element( const char* name )
{
   using namespace boost;

   if( m_stop_processing )
      return;

   name_map_t  name_map  = get( vertex_name,               m_graph );
   props_map_t props_map = get( vertex_element_properties, m_graph );

   if( name == name_map[ m_current ] )
   {
      graph_traits< graph_t >::adjacency_iterator ai, ai_end;
      for( tie( ai, ai_end ) = adjacent_vertices( m_current, m_graph ); ai != ai_end; ++ai )
      {
         if( "&end&" == name_map[ *ai ] )
         {
            m_stop_processing = true;
            break;
         }
      }

      if( ! m_path.empty() )
      {
         element_props props( props_map[ m_current ] );

         if( NULL != props.data_handler )
            ((*this).*(props.data_handler))( props.text, props.attributes );

         graph_traits< graph_t >::out_edge_iterator e, e_end;
         for( tie( e, e_end ) = out_edges( m_current, m_graph ); e != e_end; ++e )
         {
            props_map[ target( *e, m_graph ) ].count = 0;
         }

         m_current = m_path.back();
         m_path.pop_back();
      }
      else
         DOUT( printf( "DBG*** Unexpected element ends (path empty): %s\n", name ); );
   }
   else
      DOUT( printf( "DBG*** Unexpected element ends: %s\n", name ); );
}

void fb2_parser::on_character_data( const char* s, int len )
{
   using namespace boost;

   if( m_stop_processing )
      return;

   props_map_t props_map = get( vertex_element_properties, m_graph );

   props_map[ m_current ].text += string( s, len );
}

void fb2_parser::prepare_xml_graph()
{
   using namespace boost;

   name_map_t  name_map  = get( vertex_name,               m_graph );
   props_map_t props_map = get( vertex_element_properties, m_graph );

   element_t
      start                      = add_vertex( m_graph ), // 1
         fictionbook             = add_vertex( m_graph ), // 1
            description          = add_vertex( m_graph ), // 1
               title_info        = add_vertex( m_graph ), // 1
                  genre          = add_vertex( m_graph ), // 1 - many
                  author         = add_vertex( m_graph ), // 1 - many
                     first_name  = add_vertex( m_graph ), // 0 - 1
                     middle_name = add_vertex( m_graph ), // 0 - 1
                     last_name   = add_vertex( m_graph ), // 0 - 1
//                     nickname    = add_vertex( m_graph ), // 0 - 1
//                     home_page   = add_vertex( m_graph ), // 0 - 1
//                     email       = add_vertex( m_graph ), // 0 - 1
                  book_title     = add_vertex( m_graph ), // 1
//                  annotation     = add_vertex( m_graph ), // 0 - 1
                  keywords       = add_vertex( m_graph ), // 0 - 1
                  date           = add_vertex( m_graph ), // 0 - 1
//                  coverpage      = add_vertex( m_graph ), // 0 - 1
                  lang           = add_vertex( m_graph ), // 1
//                  src_lang       = add_vertex( m_graph ), // 0 - 1
//                  translator     = add_vertex( m_graph ), // 0 - many
                  sequence       = add_vertex( m_graph ), // 0 - many
      end                        = add_vertex( m_graph ); // 1

   name_map[ start       ] = "&start&";     props_map[ start       ] = element_props( make_pair( 1,  1 ) );
   name_map[ fictionbook ] = "FictionBook"; props_map[ fictionbook ] = element_props( make_pair( 1,  1 ) );
   name_map[ description ] = "description"; props_map[ description ] = element_props( make_pair( 1,  1 ) );
   name_map[ title_info  ] = "title-info";  props_map[ title_info  ] = element_props( make_pair( 1,  1 ) );
   name_map[ genre       ] = "genre";       props_map[ genre       ] = element_props( make_pair( 1, -1 ), &fb2_parser::on_genre );
   name_map[ author      ] = "author";      props_map[ author      ] = element_props( make_pair( 1, -1 ), &fb2_parser::on_author );
   name_map[ first_name  ] = "first-name";  props_map[ first_name  ] = element_props( make_pair( 0,  1 ), &fb2_parser::on_author_first_name );
   name_map[ middle_name ] = "middle-name"; props_map[ middle_name ] = element_props( make_pair( 0,  1 ), &fb2_parser::on_author_middle_name );
   name_map[ last_name   ] = "last-name";   props_map[ last_name   ] = element_props( make_pair( 0,  1 ), &fb2_parser::on_author_last_name );
//   name_map[ nickname    ] = "nickname";    props_map[ nickname    ] = element_props( make_pair( 0,  1 ) );
//   name_map[ home_page   ] = "home-page";   props_map[ home_page   ] = element_props( make_pair( 0,  1 ) );
//   name_map[ email       ] = "email";       props_map[ email       ] = element_props( make_pair( 0,  1 ) );
   name_map[ book_title  ] = "book-title";  props_map[ book_title  ] = element_props( make_pair( 1,  1 ), &fb2_parser::on_book_title );
//   name_map[ annotation  ] = "annotation";  props_map[ annotation  ] = element_props( make_pair( 0,  1 ) );
   name_map[ keywords    ] = "keywords";    props_map[ keywords    ] = element_props( make_pair( 0,  1 ), &fb2_parser::on_keywords );
   name_map[ date        ] = "date";        props_map[ date        ] = element_props( make_pair( 0,  1 ) );
//   name_map[ coverpage   ] = "coverpage";   props_map[ coverpage   ] = element_props( make_pair( 0,  1 ) );
   name_map[ lang        ] = "lang";        props_map[ lang        ] = element_props( make_pair( 1,  1 ), &fb2_parser::on_lang );
//   name_map[ src_lang    ] = "src-lang";    props_map[ src_lang    ] = element_props( make_pair( 0,  1 ) );
//   name_map[ translator  ] = "translator";  props_map[ translator  ] = element_props( make_pair( 0, -1 ) );
   name_map[ sequence    ] = "sequence";    props_map[ sequence    ] = element_props( make_pair( 0, -1 ), &fb2_parser::on_sequence );
   name_map[ end         ] = "&end&";       props_map[ end         ] = element_props( make_pair( 1,  1 ) );

   graph_traits< graph_t >::edge_descriptor ed;
   bool inserted;

   tie( ed, inserted ) = add_edge( start,       fictionbook, m_graph );
   tie( ed, inserted ) = add_edge( fictionbook, description, m_graph );
   tie( ed, inserted ) = add_edge( description, title_info,  m_graph );
   tie( ed, inserted ) = add_edge( title_info,  genre,       m_graph );
   tie( ed, inserted ) = add_edge( title_info,  author,      m_graph );
   tie( ed, inserted ) = add_edge( author,      first_name,  m_graph );
   tie( ed, inserted ) = add_edge( author,      middle_name, m_graph );
   tie( ed, inserted ) = add_edge( author,      last_name,   m_graph );
//   tie( ed, inserted ) = add_edge( author,      nickname,    m_graph );
//   tie( ed, inserted ) = add_edge( author,      home_page,   m_graph );
//   tie( ed, inserted ) = add_edge( author,      email,       m_graph );
   tie( ed, inserted ) = add_edge( title_info,  book_title,  m_graph );
//   tie( ed, inserted ) = add_edge( title_info,  annotation,  m_graph );
   tie( ed, inserted ) = add_edge( title_info,  keywords,    m_graph );
   tie( ed, inserted ) = add_edge( title_info,  date,        m_graph );
//   tie( ed, inserted ) = add_edge( title_info,  coverpage,   m_graph );
   tie( ed, inserted ) = add_edge( title_info,  lang,        m_graph );
//   tie( ed, inserted ) = add_edge( title_info,  src_lang,    m_graph );
//   tie( ed, inserted ) = add_edge( title_info,  translator,  m_graph );
   tie( ed, inserted ) = add_edge( title_info,  sequence,    m_graph );
   tie( ed, inserted ) = add_edge( fictionbook, end,         m_graph );
   tie( ed, inserted ) = add_edge( description, end,         m_graph ); // do not process unnecessary information
   tie( ed, inserted ) = add_edge( title_info,  end,         m_graph ); // do not process unnecessary information

   m_current = start;
}

void fb2_parser::on_genre( const std::string& str, const attributes_t& attrs )
{
   string temp = str;
   remove_crlf( temp );
   m_genres.push_back( fix_data( temp.c_str(), g_limits.G_FB2Code ) );
}

void fb2_parser::on_author_first_name( const std::string& str, const attributes_t& attrs )
{
   m_f = str;
   remove_crlf( m_f );
   m_f = fix_data( m_f.c_str(), g_limits.A_Name );
}

void fb2_parser::on_author_middle_name( const std::string& str, const attributes_t& attrs )
{
   m_m = str;
   remove_crlf( m_m );
   m_m = fix_data( m_m.c_str(), g_limits.A_Middle );
}

void fb2_parser::on_author_last_name( const std::string& str, const attributes_t& attrs )
{
   m_l = str;
   remove_crlf( m_l );
   m_l = fix_data( m_l.c_str(), g_limits.A_Family );
}

void fb2_parser::on_author( const std::string& str, const attributes_t& attrs )
{
   m_authors.push_back( m_l + "," + m_f + "," + m_m );
}

void fb2_parser::on_book_title( const std::string& str, const attributes_t& attrs )
{
   m_title = str;
   remove_crlf( m_title );
   m_title = fix_data( m_title.c_str(), g_limits.Title );
}

void fb2_parser::on_keywords( const std::string& str, const attributes_t& attrs )
{
   m_keywords = str;
   remove_crlf( m_keywords );
   m_keywords = fix_data( m_keywords.c_str(), g_limits.KeyWords );
}

void fb2_parser::on_lang( const std::string& str, const attributes_t& attrs )
{
   m_language = str;
}

void fb2_parser::on_sequence( const std::string& str, const attributes_t& attrs )
{
   attributes_t::const_iterator it;

   it = attrs.find( "name" );
   if( attrs.end() != it )
   {
      m_seq_name = (*it).second;
      remove_crlf( m_seq_name );
      m_seq_name = fix_data( m_seq_name.c_str(), g_limits.S_Title );
   }

   it = attrs.find( "number" );
   if( attrs.end() != it ) m_seq = (*it).second;
}

#endif // DO_NOT_INCLUDE_PARSER
