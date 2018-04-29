#ifndef __IMPORT_UTIL_H__
#define __IMPORT_UTIL_H__

#ifdef _XML_DEBUG
#define DOUT(format, ...) wprintf(L##format, __VA_ARGS__)
#else
#define DOUT(...) (void)0
#endif

#ifndef DO_NOT_INCLUDE_PARSER

struct db_limits {
    size_t A_Name; // Author
    size_t A_Middle;
    size_t A_Family;
    size_t Title; // Book title
    size_t KeyWords;
    size_t S_Title;   // Series title
    size_t G_FB2Code; // Genre
};

extern db_limits g_limits;
extern bool      g_fix;

void initialize_limits(const std::string& config);
std::string fix_data(const std::string& str, size_t max_len);
std::string cleanse(const std::string& s);
std::string cleanse_lang(const std::string& s);

#endif // DO_NOT_INCLUDE_PARSER

bool is_numeric(const std::string& str);
const char* separate_file_name(char* buf);

void normalize_path(std::string& path, bool trailing = true);
void normalize_path(char* path);

std::wstring utf8_to_wchar(const std::string&);
std::string  wchar_to_utf8(const std::wstring&);
std::string  duplicate_quote(const std::string&);

class tmp_str : public std::string {
  public:
    tmp_str(const char* format, ...);

    operator const char*() const { return c_str(); }
};

class timer {
  public:
    timer() : m_start(boost::posix_time::second_clock::local_time()) {}
    ~timer() {}

    void restart() { m_start = boost::posix_time::second_clock::local_time(); }

    std::string passed() { return boost::posix_time::to_simple_string(boost::posix_time::time_duration(boost::posix_time::second_clock::local_time() - m_start)); }

  private:
    boost::posix_time::ptime m_start;
};

class zip : boost::noncopyable {
    friend class zip_writer;

  public:
    zip(const std::string& name, const std::string& comment = std::string(), bool create = true) : m_comment(comment), m_opened(false), m_name(name), m_zf(NULL)
    {
        if (!m_func_set) {
            m_func_set = true;
#ifdef _WIN32
            fill_win32_filefunc64A(&m_ffunc);
#else
            fill_fopen64_filefunc(&m_ffunc);
#endif
        }

        if (NULL == (m_zf = zipOpen2_64(m_name.c_str(), create ? APPEND_STATUS_CREATE : APPEND_STATUS_ADDINZIP, NULL, &m_ffunc))) {
            throw std::runtime_error(tmp_str("Unable to create archive zipOpen2(\"%s\")", m_name.c_str()));
        }
        m_opened = true;
    }

    void close(bool do_not_throw = false)
    {
        if (m_opened) {
            m_opened = false;

            const char* pc = m_comment.empty() ? NULL : m_comment.c_str();

            if (ZIP_OK != zipClose(m_zf, pc)) {
                if (!do_not_throw) {
                    throw std::runtime_error(tmp_str("Unable to close archive zipClose(\"%s\")", m_name.c_str()));
                }
            }
        }
    }

    ~zip() { close(true); }

    static zlib_filefunc64_def* ffunc() { return &m_ffunc; }

  private:
    bool        m_opened;
    std::string m_name;
    std::string m_comment;
    zipFile     m_zf;

    static bool                m_func_set;
    static zlib_filefunc64_def m_ffunc;
};

class zip_writer : boost::noncopyable {
  public:
    zip_writer(const zip& zip, const std::string& name, bool auto_open = true, bool zip64 = false) : m_opened(false), m_ziperr(ZIP_OK), m_zip(zip), m_name(name)
    {
        memset(&m_zi, 0, sizeof(m_zi));
        filltime(m_zi.tmz_date);

        if (auto_open) {
            open(zip64);
        }
    }

    void open(bool zip64 = false)
    {
        if (!m_opened) {
            if (ZIP_OK != (m_ziperr = zipOpenNewFileInZip3_64(
                               m_zip.m_zf, m_name.c_str(), &m_zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, 9, 0, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, NULL, 0, zip64 ? 1 : 0))) {
                throw std::runtime_error(tmp_str("Error writing to INPX zipOpenNewFileInZip3(%d) \"%s\"", m_ziperr, m_name.c_str()));
            }
            m_opened = true;
        }
    }

    void close(bool do_not_throw = false)
    {
        if (m_opened) {
            m_opened = false;

            if (ZIP_OK != (m_ziperr = zipCloseFileInZip(m_zip.m_zf))) {
                if (!do_not_throw) {
                    throw std::runtime_error(tmp_str("Error writing to INPX zipCloseFileInZip(%d) \"%s\"", m_ziperr, m_name.c_str()));
                }
            }
        }
    }

    ~zip_writer() { close(true); }

    bool is_open() const { return m_opened; }

    operator bool() const { return m_ziperr == ZIP_OK; }

    void operator()(const std::string& buf)
    {
        if (ZIP_OK != (m_ziperr = zipWriteInFileInZip(m_zip.m_zf, buf.c_str(), (int)buf.size()))) {
            throw std::runtime_error(tmp_str("Error writing to INPX zipWriteInFileInZip(%d) \"%s\"", m_ziperr, m_name.c_str()));
        }
    }

  private:
    void filltime(tm_zip& tmz)
    {
        time_t rawtime;

        time(&rawtime);

        struct tm* ptm = gmtime(&rawtime);

        tmz.tm_sec  = ptm->tm_sec;
        tmz.tm_min  = ptm->tm_min;
        tmz.tm_hour = ptm->tm_hour;
        tmz.tm_mday = ptm->tm_mday;
        tmz.tm_mon  = ptm->tm_mon;
        tmz.tm_year = ptm->tm_year + 1900;
    }

    bool         m_opened;
    int          m_ziperr;
    std::string  m_name;
    const zip&   m_zip;
    zip_fileinfo m_zi;
};

class unzip : boost::noncopyable {
    friend class unzip_reader;

  public:
    unzip(const std::string& name) : m_opened(false), m_name(name), m_uf(NULL)
    {
        if (!m_func_set) {
            m_func_set = true;
#ifdef _WIN32
            fill_win32_filefunc64A(&m_ffunc);
#else
            fill_fopen64_filefunc(&m_ffunc);
#endif
        }

        if (NULL == (m_uf = unzOpen2_64(m_name.c_str(), &m_ffunc))) {
            throw std::runtime_error(tmp_str("Unable to open archive unzOpen2(\"%s\")", m_name.c_str()));
        }

        int zip_err = unzGetGlobalInfo(m_uf, &m_gi);

        if (UNZ_OK != zip_err) {
            throw std::runtime_error(tmp_str("Problem processing archive unzGetGlobalInfo(%d) \"%s\"", zip_err, m_name.c_str()));
        }

        m_opened = true;
    }

    void close()
    {
        if (m_opened) {
            m_opened = false;

            unzCloseCurrentFile(m_uf);
        }
    }

    ~unzip() { close(); }

    unsigned int count() const { return m_gi.number_entry; }

    std::string current() const
    {
        char name_inzip[PATH_MAX + 1];

        int zip_err = unzGetCurrentFileInfo64(m_uf, NULL, name_inzip, sizeof(name_inzip) - 1, NULL, 0, NULL, 0);

        if (UNZ_OK != zip_err) {
            throw std::runtime_error(tmp_str("Problem processing archive unzGetCurrentFileInfo(%d) \"%s\"", zip_err, m_name.c_str()));
        }

        return std::string(name_inzip);
    }

    void current(unz_file_info64& fi) const
    {
        int zip_err = unzGetCurrentFileInfo64(m_uf, &fi, NULL, 0, NULL, 0, NULL, 0);

        if (UNZ_OK != zip_err) {
            throw std::runtime_error(tmp_str("Problem processing archive unzGetCurrentFileInfo(%d) \"%s\"", zip_err, m_name.c_str()));
        }
    }

    void move_next()
    {
        int zip_err = unzGoToNextFile(m_uf);
        if (UNZ_OK != zip_err) {
            throw std::runtime_error(tmp_str("Problem processing archive unzGoToNextFile(%d) \"%s\"", zip_err, m_name.c_str()));
        }
    }

    static zlib_filefunc64_def* ffunc() { return &m_ffunc; }

    bool is_open() const { return m_opened; }

  private:
    bool            m_opened;
    std::string     m_name;
    unzFile         m_uf;
    unz_global_info m_gi;

    static bool                m_func_set;
    static zlib_filefunc64_def m_ffunc;
};

class unzip_reader : boost::noncopyable {
  public:
    unzip_reader(const unzip& unz, bool auto_open = true) : m_opened(false), m_ziperr(UNZ_OK), m_unz(unz)
    {
        if (auto_open) {
            open();
        }
    }

    void open()
    {
        if (!m_opened) {
            if (UNZ_OK != (m_ziperr = unzOpenCurrentFile(m_unz.m_uf))) {
                throw std::runtime_error(tmp_str("Problem reading archive unzOpenCurrentFile(%d) \"%s\"", m_ziperr, m_unz.m_name.c_str()));
            }

            m_opened = true;
        }
    }

    void close(bool do_not_throw = false)
    {
        // placeholder

        if (m_opened) {
            m_opened = false;

            if (UNZ_OK != (m_ziperr = unzCloseCurrentFile(m_unz.m_uf))) {
                if (!do_not_throw) {
                    throw std::runtime_error(tmp_str("Error reading archive unzCloseCurrentFile(%d) \"%s\"", m_ziperr, m_unz.m_name.c_str()));
                }
            }
        }
    }

    ~unzip_reader() { close(true); }

    bool is_open() const { return m_opened; }

    operator bool() const { return m_ziperr == ZIP_OK; }

    int operator()(void* buf, int len)
    {
        int res = 0;

        if (0 > (res = unzReadCurrentFile(m_unz.m_uf, buf, (unsigned)len))) {
            m_ziperr = res;
            throw std::runtime_error(tmp_str("Error reading archive (%d) unzReadCurrentFile\"%s\"", m_ziperr, m_unz.m_name.c_str()));
        }
        return res;
    }

  private:
    bool         m_opened;
    int          m_ziperr;
    const unzip& m_unz;
};

#ifndef DO_NOT_INCLUDE_PARSER

#ifdef XML_LARGE_SIZE
#define XML_FMT_INT_MOD "ll"
#else
#define XML_FMT_INT_MOD "l"
#endif

namespace boost {
enum vertex_element_properties_t { vertex_element_properties };
BOOST_INSTALL_PROPERTY(vertex, element_properties);
}

class fb2_parser {
    typedef std::pair<int, int>                cardinality_t;
    typedef std::map<std::string, std::string> attributes_t;
    typedef void (fb2_parser::*data_handler_t)(const std::string&, const attributes_t&);

    struct element_props {
        cardinality_t  cardinality;
        int            count;
        data_handler_t data_handler;
        std::string    text;
        attributes_t   attributes;

        element_props() : cardinality(std::make_pair(0, 0)), data_handler(NULL), count(0) {}

        element_props(const cardinality_t& c, data_handler_t h = NULL) : cardinality(c), data_handler(h), count(0) {}

        element_props(const element_props& from) : cardinality(from.cardinality), count(from.count), data_handler(from.data_handler), text(from.text), attributes(from.attributes) {}
    };

    typedef boost::adjacency_list<boost::listS, boost::listS, boost::directedS,
        boost::property<boost::vertex_name_t, std::string, boost::property<boost::vertex_element_properties_t, element_props>>>
        graph_t;

    typedef boost::property_map<graph_t, boost::vertex_name_t>::type               name_map_t;
    typedef boost::property_map<graph_t, boost::vertex_element_properties_t>::type props_map_t;

    typedef boost::graph_traits<graph_t>::vertex_descriptor element_t;
    typedef boost::graph_traits<graph_t>::vertex_iterator   element_iter_t;
    typedef std::list<element_t>                            element_list_t;

  public:
    fb2_parser() : m_parser(NULL), m_stop_processing(false), m_unicode(false)
    {
        prepare_xml_graph();

        m_parser = XML_ParserCreate(NULL);
        if (NULL == m_parser) {
            throw std::runtime_error("Unable to create XML parser - XML_ParserCreate()");
        }

        const XML_Feature* f = XML_GetFeatureList();
        while (f->feature != XML_FEATURE_END) {
            if ((f->feature == XML_FEATURE_UNICODE) || (f->feature == XML_FEATURE_UNICODE_WCHAR_T)) {
                m_unicode = true;
            }
            f++;
        }
        XML_SetUserData(m_parser, this);
        XML_SetUnknownEncodingHandler(m_parser, unknownEncoding, this);
        XML_SetElementHandler(m_parser, startElement, endElement);
        XML_SetCharacterDataHandler(m_parser, characterData);

        assert(m_unicode == false);
    }

    ~fb2_parser()
    {
        if (NULL != m_parser) {
            XML_ParserFree(m_parser);
        }

        m_graph.clear();
    }

    bool is_unicode() const { return m_unicode; }

    bool operator()(const char* buf, int length, bool done = false)
    {
        if (!m_stop_processing && (XML_STATUS_ERROR == XML_Parse(m_parser, buf, length, done ? 1 : 0)))
            throw std::runtime_error(tmp_str("%s at line %" XML_FMT_INT_MOD "u", XML_ErrorString(XML_GetErrorCode(m_parser)), XML_GetCurrentLineNumber(m_parser)));
        return !m_stop_processing;
    }

    std::vector<std::string> m_genres;
    std::vector<std::string> m_authors;
    std::string              m_title;
    std::string              m_keywords;
    std::string              m_date;
    std::string              m_language;
    std::string              m_seq_name;
    std::string              m_seq;

  private:
    int on_unknown_encoding(const XML_Char* name, XML_Encoding* info);
    void on_start_element(const char* name, const char** attrs);
    void on_end_element(const char* name);
    void on_character_data(const char* s, int len);

    static int XMLCALL unknownEncoding(void* encodingHandlerData, const XML_Char* name, XML_Encoding* info)
    {
        return static_cast<fb2_parser*>(encodingHandlerData)->on_unknown_encoding(name, info);
    }

    static void XMLCALL startElement(void* userData, const char* name, const char** attrs) { static_cast<fb2_parser*>(userData)->on_start_element(name, attrs); }

    static void XMLCALL endElement(void* userData, const char* name) { static_cast<fb2_parser*>(userData)->on_end_element(name); }

    static void XMLCALL characterData(void* userData, const XML_Char* s, int len) { static_cast<fb2_parser*>(userData)->on_character_data(s, len); }

    void prepare_xml_graph();
    bool check_cardinality(int count, const cardinality_t& constrains);

    void on_genre(const std::string& str, const attributes_t& attrs);
    void on_author_first_name(const std::string& str, const attributes_t& attrs);
    void on_author_middle_name(const std::string& str, const attributes_t& attrs);
    void on_author_last_name(const std::string& str, const attributes_t& attrs);
    void on_author(const std::string& str, const attributes_t& attrs);
    void on_book_title(const std::string& str, const attributes_t& attrs);
    void on_keywords(const std::string& str, const attributes_t& attrs);
    void on_lang(const std::string& str, const attributes_t& attrs);
    void on_sequence(const std::string& str, const attributes_t& attrs);

    std::string m_f, m_m, m_l; // temps

    element_list_t m_path;
    graph_t        m_graph;

    element_t m_current;

    XML_Parser m_parser;
    bool       m_stop_processing;
    bool       m_unicode;
};

#endif // DO_NOT_INCLUDE_PARSER

#endif // __IMPORT_UTIL_H__
