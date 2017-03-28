#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <sys/io.h>
#endif
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

#include <mysql.h>
#include <iconv.h>

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

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/erase.hpp>

#include <zlib.h>
#include <minizip/unzip.h>
#include <minizip/zip.h>
#ifdef _WIN32
#include <minizip/iowin32.h>
#else
#include <minizip/ioapi.h>
#endif
#include <expat.h>

#include "util.h"

#define STARTING_SIZE 0x400
#define ENDING_SIZE 0x10000

const char* wide_locale_name =
#ifdef _WIN32
    "UCS-2LE//IGNORE";
#else
    "UCS-4LE//IGNORE";
#endif

using namespace std;

#ifndef DO_NOT_INCLUDE_PARSER

bool      g_fix = false;
db_limits g_limits;

void initialize_limits(const string& config)
{
    g_limits.A_Name = g_limits.A_Middle = g_limits.A_Family = 128;
    g_limits.Title                                          = 150;
    g_limits.KeyWords                                       = 255;
    g_limits.S_Title                                        = 80;

    if (!config.empty() && (0 == access(config.c_str(), 4))) {
        using boost::property_tree::ptree;
        ptree pt;

        read_info(config, pt);

        g_limits.A_Name   = pt.get<size_t>("Limits.A_Name", g_limits.A_Name);
        g_limits.A_Middle = pt.get<size_t>("Limits.A_Middle", g_limits.A_Middle);
        g_limits.A_Family = pt.get<size_t>("Limits.A_Family", g_limits.A_Family);
        g_limits.Title    = pt.get<size_t>("Limits.Title", g_limits.Title);
        g_limits.KeyWords = pt.get<size_t>("Limits.KeyWords", g_limits.KeyWords);
        g_limits.S_Title  = pt.get<size_t>("Limits.S_Title", g_limits.S_Title);
    }
}

string fix_data(const string& str, size_t max_len)
{
    if (g_fix && (max_len > 0)) {

        // TODO: for compatibility with previous implementation, I think this is wrong
        size_t len = max_len - 1;

        string::value_type sym       = 0;
        string::size_type  last_byte = -1;
        string::size_type  utf_len   = 0;

        for (auto i = 0; i < str.size(); ++i) {

            sym = str[i];

            // ASCII character
            if ((sym & 0b10000000) == 0) {

                if (utf_len < len) {
                    last_byte = i;
                    utf_len++;
                    continue;
                }
                break;
            }
            // utf-8 code points
            if ((sym & 0b11000000) == 0b11000000) {

                if (utf_len < len) {
                    utf_len++;
                    continue;
                }
                break;
            }
            // What's left of code point
            last_byte = i;
        }

        string s;
        if (utf_len > 0) {
            s = str.substr(0, last_byte + 1);
            boost::algorithm::trim_right(s);
        }
        return s;
    }
    return str;
}

string cleanse(const string& s)
{
    string str(s);
    boost::algorithm::replace_all(str, "\r\n", " ");
    boost::algorithm::erase_all(str, "\n");
    boost::algorithm::replace_all(str, "\xC2\xA0", " ");
    return str;
}

string duplicate_quote(const string& str) { return boost::algorithm::replace_all_copy(str, "\'", "\'\'"); }

#endif // DO_NOT_INCLUDE_PARSER

bool is_numeric(const string& str)
{
    for (string::const_iterator it = str.begin(); it != str.end(); ++it) {
        if (!isdigit(*it)) {
            return false;
        }
    }
    return true;
}

const char* separate_file_name(char* buf)
{
    char* ptr = buf + strlen(buf);

    while (ptr != buf) {
        if ((*ptr == '\\') || (*ptr == '/')) {
            *ptr = '\0';
            ptr++;
            break;
        }
        ptr--;
    }
    return ptr;
}

void normalize_path(string& path, bool trailing)
{
    for (string::iterator it = path.begin(); it != path.end(); ++it) {
        if (*it == '\\') {
            *it = '/';
        }
    }
    if (trailing) {
        if (*(path.end() - 1) != '/') {
            path += '/';
        }
    }
}

void normalize_path(char* path)
{
    for (int ni = 0; ni < strlen(path); ++ni) {
        if (path[ni] == '\\') {
            path[ni] = '/';
        }
    }
}

wstring utf8_to_wchar(const string& str)
{
    wstring        w;
    static iconv_t cd = NULL;

    if (NULL == cd) {
        if ((cd = iconv_open(wide_locale_name, "UTF-8")) == (iconv_t)-1) {
            DOUT("DBG*** iconv_open error: %d\n", errno);
            return w;
        }
    } else {
        iconv(cd, NULL, NULL, NULL, NULL);
    }

    size_t                       len = str.length();
    boost::scoped_array<wchar_t> buf(new wchar_t[len + 1]);

    char*  pin     = (char*)str.c_str();
    size_t inbytes = len;

    while (inbytes > 0) {

        size_t outbytes = len * sizeof(wchar_t);
        char*  pout     = (char*)buf.get();

        size_t res = iconv(cd, &pin, &inbytes, &pout, &outbytes);

        if (((size_t)-1 == res) && (errno != E2BIG)) {
            DOUT("DBG*** iconv error: %d\n", errno);
            return w;
        }

        buf[len - (outbytes / sizeof(wchar_t))] = L'\0';
        w += buf.get();
    }
    return w;
}

string wchar_to_utf8(const wstring& str)
{
    string         s;
    static iconv_t cd = NULL;

    if (NULL == cd) {
        if ((cd = iconv_open("UTF-8//IGNORE", wide_locale_name)) == (iconv_t)-1) {
            DOUT("DBG*** iconv_open error: %d\n", errno);
            return s;
        }
    } else {
        iconv(cd, NULL, NULL, NULL, NULL);
    }

    size_t len     = str.length();
    size_t len_out = max(len, sizeof(char) * 31); // iconv does not like output buffers for 1 char

    boost::scoped_array<char> buf(new char[len_out + 1]);

    char*  pin     = (char*)str.c_str();
    size_t inbytes = len * sizeof(wchar_t);

    while (inbytes > 0) {

        size_t outbytes = len_out;
        char*  pout     = buf.get();

        size_t res = iconv(cd, &pin, &inbytes, &pout, &outbytes);

        if (((size_t)-1 == res) && (errno != E2BIG)) {
            DOUT("DBG*** iconv error: %d\n", errno);
            return s;
        }

        buf[len_out - outbytes] = '\0';
        s += buf.get();
    }
    return s;
}

tmp_str::tmp_str(const char* format, ...)
{
    int     ni = -1;
    va_list marker;

    va_start(marker, format);
    for (int size = STARTING_SIZE; size <= ENDING_SIZE; size *= 2) {
        boost::scoped_array<char> buf(new char[size + 1]);
        ni = vsnprintf(buf.get(), size, format, marker);
        if (-1 != ni) {
            assign(buf.get(), ni);
            break;
        }
    }
    va_end(marker);
}

bool                zip::m_func_set = false;
zlib_filefunc64_def zip::m_ffunc;

bool                unzip::m_func_set = false;
zlib_filefunc64_def unzip::m_ffunc;

#ifndef DO_NOT_INCLUDE_PARSER

// TODO: for simplicity we are assuming Intel incoding
static int XMLCALL xml_convert(void* data, const char* s)
{
    size_t inbytes = 1;
    char   in      = *s;
    char*  pin     = &in;

    size_t outbytes = 4;
    char   out[4]   = {0, 0, 0, 0};
    char*  pout     = &out[0];

    size_t res = iconv(data, &pin, &inbytes, &pout, &outbytes);

    if ((size_t)-1 == res) {
        return -1;
    }

    int ret = 0;
    switch (4 - outbytes) {
        case 2:
            ret = *(short int*)&out[0];
            break;
        case 4:
            ret = *(int*)&out[0];
            break;
        default:
            ret = -1;
            DOUT("DBG*** unsupported conversion length %d for: %s -> [%02x,%02x,%02x,%02x]\n", 4 - outbytes, s, out[0], out[1], out[2], out[3]);
            break;
    }
    return ret;
}

static void xml_convert_release(void* data) { iconv_close(data); }

int fb2_parser::on_unknown_encoding(const XML_Char* name, XML_Encoding* info)
{
    iconv_t cd;

    if ((cd = iconv_open(wide_locale_name, name)) == (iconv_t)-1) {
        DOUT("DBG*** unsupported encoding: %s\n", name);
        return XML_STATUS_ERROR;
    }

    for (size_t i = 0; i < 256; i++) {

        iconv(cd, NULL, NULL, NULL, NULL);

        size_t inbytes = 1;
        char   in      = i;
        char*  pin     = &in;

        size_t outbytes = 4;
        char   out[4]   = {0, 0, 0, 0};
        char*  pout     = &out[0];

        size_t res = iconv(cd, &pin, &inbytes, &pout, &outbytes);

        if ((size_t)-1 == res) {
            if (errno == EINVAL) {
                // Really? I have no way of knowing what the lenght of variable encoding would be
                info->map[i] = -(int)sizeof(wchar_t);
            } else {
                info->map[i] = -1;
            }
        } else {
            switch (4 - outbytes) {
                case 2:
                    info->map[i] = *(short int*)&out[0];
                    break;
                case 4:
                    info->map[i] = *(int*)&out[0];
                    break;
                default:
                    info->map[i] = -1;
                    DOUT("DBG*** unsupported encoding (%s) length %d for: %02x -> [%02x,%02x,%02x,%02x]\n", name, 4 - outbytes, i, out[0], out[1], out[2], out[3]);
                    break;
            }
        }
    }
    info->data    = cd;
    info->convert = xml_convert;
    info->release = xml_convert_release;

    return XML_STATUS_OK;
}

bool fb2_parser::check_cardinality(int count, const cardinality_t& constrains)
{
    int from = constrains.first, to = constrains.second;

    // simplified to ignore extras

    return (to < 0) || (count <= to);
}

void fb2_parser::on_start_element(const char* name, const char** attrs)
{
    using namespace boost;

    if (m_stop_processing) {
        return;
    }

    name_map_t  name_map  = get(vertex_name, m_graph);
    props_map_t props_map = get(vertex_element_properties, m_graph);

    bool found = false;
    int  count = XML_GetSpecifiedAttributeCount(m_parser);

    graph_traits<graph_t>::adjacency_iterator ai, ai_end;
    for (tie(ai, ai_end) = adjacent_vertices(m_current, m_graph); ai != ai_end; ++ai) {
        if (name == name_map[*ai]) {
            element_props props(props_map[*ai]);

            if (check_cardinality(++props.count, props.cardinality)) {
                for (int ni = 0; (ni < count) && attrs[ni]; ni += 2) {
                    props.attributes.insert(make_pair(attrs[ni], attrs[ni + 1]));
                }

                props.text.erase();

                props_map[*ai] = props;

                m_path.push_back(m_current);
                m_current = *ai;
            } else {
                DOUT("DBG*** Cardinality broken: %s\n", name);
            }

            found = true;
            break;
        }
    }
    if (!found)
        DOUT("DBG*** Unexpected element starts: %s\n", name);
}

void fb2_parser::on_end_element(const char* name)
{
    using namespace boost;

    if (m_stop_processing) {
        return;
    }

    name_map_t  name_map  = get(vertex_name, m_graph);
    props_map_t props_map = get(vertex_element_properties, m_graph);

    if (name == name_map[m_current]) {
        graph_traits<graph_t>::adjacency_iterator ai, ai_end;
        for (tie(ai, ai_end) = adjacent_vertices(m_current, m_graph); ai != ai_end; ++ai) {
            if ("&end&" == name_map[*ai]) {
                m_stop_processing = true;
                break;
            }
        }

        if (!m_path.empty()) {
            element_props props(props_map[m_current]);

            if (NULL != props.data_handler) {
                ((*this).*(props.data_handler))(props.text, props.attributes);
            }

            graph_traits<graph_t>::out_edge_iterator e, e_end;
            for (tie(e, e_end) = out_edges(m_current, m_graph); e != e_end; ++e) {
                props_map[target(*e, m_graph)].count = 0;
            }

            m_current = m_path.back();
            m_path.pop_back();
        } else {
            DOUT("DBG*** Unexpected element ends (path empty): %s\n", name);
        }
    } else {
        DOUT("DBG*** Unexpected element ends: %s\n", name);
    }
}

void fb2_parser::on_character_data(const char* s, int len)
{
    using namespace boost;

    if (m_stop_processing) {
        return;
    }

    props_map_t props_map = get(vertex_element_properties, m_graph);

    props_map[m_current].text += string(s, len);
}

void fb2_parser::prepare_xml_graph()
{
    using namespace boost;

    name_map_t  name_map  = get(vertex_name, m_graph);
    props_map_t props_map = get(vertex_element_properties, m_graph);

    // clang-format off
	element_t start = add_vertex(m_graph),            // 1
	    fictionbook = add_vertex(m_graph),            // 1
	    description = add_vertex(m_graph),            // 1
	    title_info  = add_vertex(m_graph),            // 1
	    genre       = add_vertex(m_graph),            // 1 - many
	    author      = add_vertex(m_graph),            // 1 - many
	    first_name  = add_vertex(m_graph),            // 0 - 1
	    middle_name = add_vertex(m_graph),            // 0 - 1
	    last_name   = add_vertex(m_graph),            // 0 - 1
	    // nickname    = add_vertex( m_graph ),       // 0 - 1
	    // home_page   = add_vertex( m_graph ),          // 0 - 1
	    // email       = add_vertex( m_graph ),          // 0 - 1
	    book_title  = add_vertex(m_graph),            // 1
	    // annotation     = add_vertex( m_graph ),       // 0 - 1
	    keywords    = add_vertex(m_graph),            // 0 - 1
	    date        = add_vertex(m_graph),            // 0 - 1
	    // coverpage      = add_vertex( m_graph ),       // 0 - 1
	    lang = add_vertex(m_graph),                   // 1
	    // src_lang       = add_vertex( m_graph ),       // 0 - 1
	    // translator     = add_vertex( m_graph ),       // 0 - many
	    sequence    = add_vertex(m_graph),            // 0 - many
	    end         = add_vertex(m_graph);            // 1

	name_map[start]        = "&start&";
	props_map[start]       = element_props(make_pair(1, 1));
	name_map[fictionbook]  = "FictionBook";
	props_map[fictionbook] = element_props(make_pair(1, 1));
	name_map[description]  = "description";
	props_map[description] = element_props(make_pair(1, 1));
	name_map[title_info]   = "title-info";
	props_map[title_info]  = element_props(make_pair(1, 1));
	name_map[genre]        = "genre";
	props_map[genre]       = element_props(make_pair(1, -1), &fb2_parser::on_genre);
	name_map[author]       = "author";
	props_map[author]      = element_props(make_pair(1, -1), &fb2_parser::on_author);
	name_map[first_name]   = "first-name";
	props_map[first_name]  = element_props(make_pair(0, 1), &fb2_parser::on_author_first_name);
	name_map[middle_name]  = "middle-name";
	props_map[middle_name] = element_props(make_pair(0, 1), &fb2_parser::on_author_middle_name);
	name_map[last_name]    = "last-name";
	props_map[last_name]   = element_props(make_pair(0, 1), &fb2_parser::on_author_last_name);
	// name_map[ nickname]    = "nickname";
	// props_map[nickname]    = element_props(make_pair(0, 1));
	// name_map[home_page]    = "home-page";
	// props_map[home_page]   = element_props(make_pair(0, 1));
	// name_map[email]        = "email";
	// props_map[email]       = element_props(make_pair(0, 1));
	name_map[book_title]   = "book-title";
	props_map[book_title]  = element_props(make_pair(1, 1), &fb2_parser::on_book_title);
	// name_map[annotation]   = "annotation";
	// props_map[annotation]  = element_props(make_pair(0, 1));
	name_map[keywords]     = "keywords";
	props_map[keywords]    = element_props(make_pair(0, 1), &fb2_parser::on_keywords);
	name_map[date]         = "date";
	props_map[date]        = element_props(make_pair(0, 1));
	// name_map[coverpage]    = "coverpage";
	// props_map[coverpage]   = element_props(make_pair(0, 1));
	name_map[lang]         = "lang";
	props_map[lang]        = element_props(make_pair(1, 1), &fb2_parser::on_lang);
	// name_map[src_lang]     = "src-lang";
	// props_map[src_lang]    = element_props(make_pair(0, 1));
	// name_map[translator]   = "translator";
	// props_map[translator]  = element_props(make_pair(0, -1));
	name_map[sequence]  = "sequence";
	props_map[sequence] = element_props(make_pair(0, -1), &fb2_parser::on_sequence);
	name_map[end]       = "&end&";
	props_map[end]      = element_props(make_pair(1, 1));
    // clang-format on

    graph_traits<graph_t>::edge_descriptor ed;
    bool                                   inserted;

    // clang-format off
	tie(ed, inserted) = add_edge(start, fictionbook, m_graph);
	tie(ed, inserted) = add_edge(fictionbook, description, m_graph);
	tie(ed, inserted) = add_edge(description, title_info, m_graph);
	tie(ed, inserted) = add_edge(title_info, genre, m_graph);
	tie(ed, inserted) = add_edge(title_info, author, m_graph);
	tie(ed, inserted) = add_edge(author, first_name, m_graph);
	tie(ed, inserted) = add_edge(author, middle_name, m_graph);
	tie(ed, inserted) = add_edge(author, last_name, m_graph);
	// tie(ed, inserted) = add_edge(author, nickname, m_graph);
	// tie(ed, inserted) = add_edge(author, home_page, m_graph);
	// tie(ed, inserted) = add_edge(author, email, m_graph);
	tie(ed, inserted) = add_edge(title_info, book_title, m_graph);
	// tie(ed, inserted ) = add_edge(title_info, annotation, m_graph);
	tie(ed, inserted) = add_edge(title_info, keywords, m_graph);
	tie(ed, inserted) = add_edge(title_info, date, m_graph);
	// tie(ed, inserted) = add_edge(title_info, coverpage, m_graph);
	tie(ed, inserted) = add_edge(title_info, lang, m_graph);
	// tie(ed, inserted) = add_edge(title_info, src_lang, m_graph);
	// tie(ed, inserted) = add_edge(title_info, translator, m_graph);
	tie(ed, inserted) = add_edge(title_info, sequence, m_graph);
	tie(ed, inserted) = add_edge(fictionbook, end, m_graph);
	tie(ed, inserted) = add_edge(description, end, m_graph); // do not process unnecessary information
	tie(ed, inserted) = add_edge(title_info, end, m_graph);  // do not process unnecessary information
    // clang-format on

    m_current = start;
}

void fb2_parser::on_genre(const std::string& str, const attributes_t& attrs) { m_genres.push_back(fix_data(cleanse(str), g_limits.G_FB2Code)); }

void fb2_parser::on_author_first_name(const std::string& str, const attributes_t& attrs) { m_f = fix_data(cleanse(str), g_limits.A_Name); }

void fb2_parser::on_author_middle_name(const std::string& str, const attributes_t& attrs) { m_m = fix_data(cleanse(str), g_limits.A_Middle); }

void fb2_parser::on_author_last_name(const std::string& str, const attributes_t& attrs) { m_l = fix_data(cleanse(str), g_limits.A_Family); }

void fb2_parser::on_author(const std::string& str, const attributes_t& attrs) { m_authors.push_back(m_l + "," + m_f + "," + m_m); }

void fb2_parser::on_book_title(const std::string& str, const attributes_t& attrs) { m_title = fix_data(cleanse(str), g_limits.Title); }

void fb2_parser::on_keywords(const std::string& str, const attributes_t& attrs) { m_keywords = fix_data(cleanse(str), g_limits.KeyWords); }

void fb2_parser::on_lang(const std::string& str, const attributes_t& attrs) { m_language = str; }

void fb2_parser::on_sequence(const std::string& str, const attributes_t& attrs)
{
    attributes_t::const_iterator it;

    it = attrs.find("name");
    if (attrs.end() != it) {
        m_seq_name = fix_data(cleanse((*it).second), g_limits.S_Title);
    }

    it = attrs.find("number");
    if (attrs.end() != it) {
        m_seq = (*it).second;
    }
}

#endif // DO_NOT_INCLUDE_PARSER
