#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <sys/io.h>
#include <sys/stat.h>
#endif
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>

#include <mysql.h>
#include <mysql_version.h>

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
#include <boost/algorithm/string/split.hpp>

#include <boost/filesystem.hpp>

#include <zlib.h>
#include <minizip/unzip.h>
#include <minizip/zip.h>
#ifdef _WIN32
#include <minizip/iowin32.h>
#else
#include <minizip/ioapi.h>
#endif
#include <expat.h>

#include <version.h>
#include "util.h"

using namespace std;
using namespace boost::posix_time;
using namespace boost::gregorian;
using namespace boost::algorithm;

namespace fs = boost::filesystem;
namespace po = boost::program_options;

static bool g_no_import        = false;
static bool g_ignore_dump_date = false;
static bool g_clean_when_done  = false;
static bool g_verbose          = false;

static bool g_mysql_cfg_created = false;

enum checking_type { eFileExt = 0, eFileType, eIgnore };
static checking_type g_strict = eFileExt;

enum fb2_parsing { eReadNone = 0, eReadLast, eReadAll };
static fb2_parsing g_read_fb2 = eReadNone;

enum fb2_preference { eIgnoreFB2 = 0, eMergeFB2, eComplementFB2, eReplaceFB2 };
static fb2_preference g_fb2_preference = eIgnoreFB2;

enum processing_type { eFB2 = 0, eUSR, eAll };
static processing_type g_process = eFB2;

enum database_format { eDefault = 0, e20100206, e20100317, e20100411, e20111106, e20170531 };
static database_format g_format = eDefault;

enum inpx_format { e2X = 0, eRUKS };
static inpx_format g_inpx_format = e2X;

enum series_type { eIgnoreST = 0, eAuthorST, ePublisherST };
static series_type g_series_type = eAuthorST;

static long   g_last_fb2 = 0;
static string g_update;
static string g_db_name = "flibusta";

static string g_outdir;

static bool g_have_alias_table = false;

static bool g_clean_authors = false;
static bool g_clean_aliases = false;

static string sep = "\x04";

// AUTHOR; GENRE; TITLE; SERIES; SERNO; FILE; SIZE; LIBID; DEL; EXT; DATE; LANG; LIBRATE; KEYWORDS;
static const char* dummy = "dummy:"
                           "\x04"
                           "other:"
                           "\x04"
                           "dummy record"
                           "\x04"
                           "\x04"
                           "\x04"
                           "\x04"
                           "1" //
                           "\x04"
                           "%d"
                           "\x04"
                           "1"
                           "\x04"
                           "EXT"
                           "\x04"
                           "2000-01-01"
                           "\x04"
                           "en"
                           "\x04"
                           "0"
                           "\x04"
                           "\x04"
                           "\r\n";

#ifdef MARIADB_BASE_VERSION
static const char* options_pattern[] = {"%s", "--defaults-file=%sdata/mysql.cfg", "--language=%s/language", "--datadir=%sdata", "--skip-grant-tables", "--innodb_data_home_dir=%sdata/dbtmp_%s",
    "--innodb_log_group_home_dir=%sdata/dbtmp_%s", "--innodb_tmpdir=%sdata/dbtmp_%s", "--innodb_stats_persistent=0", "--log_warnings=2", NULL};
#else
static const char* options_pattern[] = {"%s", "--defaults-file=%sdata/mysql.cfg", "--language=%s/language", "--datadir=%sdata", "--skip-grant-tables", "--innodb_data_home_dir=%s/data/dbtmp_%s",
    "--innodb_log_group_home_dir=%sdata/dbtmp_%s", "--innodb_tmpdir=%sdata/dbtmp_%s", "--log_syslog=0", NULL};
#endif

class mysql_connection {
    enum { num_options = sizeof(options_pattern) / sizeof(char*) };

    char* m_options[num_options];

  public:
    mysql_connection(const char* module_path, const char* path, const char* name, const char* dbname) : m_mysql(NULL)
    {
        if (0 == m_initialized) {

            if (g_verbose) {
                wcout << endl << "MySQL options: " << endl << flush;
            }

            for (int ni = 0; ni < num_options; ++ni) {
                const char* pattern = options_pattern[ni];
                if (NULL == pattern) {
                    m_options[ni] = NULL;
                    break;
                }
                char* mem = new char[PATH_MAX * 2];

                if (0 == ni) {
                    sprintf(mem, pattern, name);
                } else if (2 == ni) {
                    sprintf(mem, pattern, module_path);
                } else {
                    sprintf(mem, pattern, path, dbname);
                }
                m_options[ni] = mem;
                if (g_verbose) {
                    wcout << "Option " << setw(4) << ni << ": " << utf8_to_wchar(mem) << endl << flush;
                }
            }
            if (mysql_library_init(num_options - 1, m_options, NULL)) {
                throw runtime_error(tmp_str("Could not initialize MySQL library (%s)", mysql_error(m_mysql)));
            }

            if (NULL == (m_mysql = mysql_init(NULL))) {
                throw runtime_error("Not enough memory to initialize MySQL library");
            }

            mysql_options(m_mysql, MYSQL_READ_DEFAULT_GROUP, "embedded");
            mysql_options(m_mysql, MYSQL_OPT_USE_EMBEDDED_CONNECTION, NULL);
        }
        ++m_initialized;

        if (NULL == mysql_real_connect(m_mysql, NULL, NULL, NULL, NULL, 0, NULL, 0)) {
            throw runtime_error(tmp_str("Unable to connect (%s)", mysql_error(m_mysql)));
        }
    }

    ~mysql_connection()
    {
        if (m_initialized > 0) {
            m_initialized--;

            if (NULL != m_mysql) {
                mysql_close(m_mysql);
            }

            if (0 == m_initialized) {
                for (int ni = 0; ni < num_options; ++ni) {
                    char* pattern = m_options[ni];
                    if (NULL == pattern) {
                        break;
                    }
                    delete[] pattern;
                }
                mysql_library_end();
            }
        }
    }

    operator MYSQL*() const { return m_mysql; };

    operator bool() const { return NULL != m_mysql; }

    void query(const string& statement, bool throw_error = true) const
    {
        int res = mysql_query(m_mysql, statement.c_str());
        if (throw_error && res) {
            throw runtime_error(tmp_str("Query error (%d) %s\n%s", mysql_errno(m_mysql), mysql_error(m_mysql), statement.c_str()));
        }
    }

    void real_query(const char* statement, long length, bool throw_error = true) const
    {
        int res = mysql_real_query(m_mysql, statement, length);
        if (throw_error && res) {
            throw runtime_error(tmp_str("Real query error (%d) %s\n", mysql_errno(m_mysql), mysql_error(m_mysql)));
        }
    }

  private:
    static int m_initialized;

    MYSQL* m_mysql;
};

int mysql_connection::m_initialized = 0;

class mysql_results {
  public:
    mysql_results(const mysql_connection& mysql) : m_mysql(mysql), m_results(NULL) { m_results = mysql_store_result(m_mysql); }

    ~mysql_results()
    {
        if (NULL != m_results) {
            mysql_free_result(m_results);
        }
    }

    MYSQL_ROW fetch_row() const
    {
        if (NULL != m_results) {
            return mysql_fetch_row(m_results);
        } else {
            return NULL;
        }
    }

  private:
    const mysql_connection& m_mysql;
    MYSQL_RES*              m_results;
};

bool is_after_last(const string& book_id) { return (g_last_fb2 < atol(book_id.c_str())); }

bool is_fictionbook(const string& file)
{
    string name = file, ext;
    size_t pos  = name.rfind(".");

    if (string::npos != pos) {
        ext = name.substr(pos + 1);
        name.erase(pos);
    }

    return ((0 == strcasecmp(ext.c_str(), "fb2")) && is_numeric(name));
}

bool is_backup(const string& file)
{
    string name = file, ext;
    size_t pos  = name.rfind(".");

    if (string::npos != pos) {
        ext = name.substr(pos + 1);
    }

    return 0 == strcasecmp(ext.c_str(), "org");
}

void prepare_mysql(const char* path, const string& dbname)
{
    bool rc = true;

    if (0 == access(path, 6)) {
        fs::create_directories(path);
    }

    string config = string(path) + "/data";
    if (0 != access(config.c_str(), 6)) {
        fs::create_directories(config.c_str());
    }

    config = string(path) + "/data/mysql.cfg";
    if (0 != access(config.c_str(), 6)) {
        ofstream out(config.c_str());

        if (out) {
            out << "[server]" << endl;
            out << "[embedded]" << endl;
            out << "console" << endl;
        } else {
            throw runtime_error(tmp_str("Unable to open file \"%s\"", config.c_str()));
        }
        g_mysql_cfg_created = true;
    }

#ifndef _WIN32
    if (g_mysql_cfg_created) {
        chmod(config.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }
#endif

    config = string(path) + "/data/dbtmp_" + dbname;
    if (0 != access(config.c_str(), 6)) {
        fs::create_directories(config.c_str());
    }
}

string get_dump_date(const string& file)
{
    size_t       length;
    string       res, buf;
    ifstream     in(file.c_str());
    stringstream ss;

    regex                                 dump_date("--\\s*Dump\\scompleted\\son\\s(\\d{4}-\\d{2}-\\d{2})");
    match_results<string::const_iterator> mr;

    if (!in) {
        throw runtime_error(tmp_str("Cannot open file \"%s\"", file.c_str()));
    }

    in.seekg(0, ios::end);
    length = in.tellg();
    in.seekg(max(0, (int)(length - 300)), ios::beg);

    ss << in.rdbuf();

    if (!in && !in.eof()) {
        throw runtime_error(tmp_str("Problem reading file \"%s\"", file.c_str()));
    }

    buf = ss.str();

    if (regex_search(buf, mr, dump_date)) {
        res = mr[1];
    }

    return res;
}

bool authoraliases_table_exist(const mysql_connection& mysql)
{
    bool      rc = false;
    MYSQL_ROW record;

    string str = "SHOW TABLES like 'libavtoraliase';";

    mysql.query(str);

    mysql_results last(mysql);

    rc = (record = last.fetch_row()) != NULL;

    return rc;
}

bool filenames_table_exist(const mysql_connection& mysql)
{
    bool      rc = false;
    MYSQL_ROW record;

    string str = "SHOW TABLES like 'libfilename';";

    mysql.query(str);

    mysql_results last(mysql);

    rc = (record = last.fetch_row()) != NULL;

    return rc;
}

long get_last_filename_id(const mysql_connection& mysql)
{
    long      rc = -1;
    MYSQL_ROW record;

    string str = (eDefault == g_format) ? "SELECT MAX(`BookId`) FROM libfilename" : "SELECT MAX(`bid`) FROM libfilename";

    mysql.query(str);

    mysql_results last(mysql);

    if (record = last.fetch_row()) {
        rc = atol(record[0]);
    }

    return rc;
}

void get_book_author(const mysql_connection& mysql, const string& book_id, string& author)
{
    MYSQL_ROW record;

    author.erase();

    string str;

    if (eDefault != g_format) {
        str = "SELECT `aid` FROM `libavtor` WHERE bid=";
    } else {
        str = "SELECT `AvtorId` FROM `libavtor` WHERE BookId=";
    }

    str += book_id;
    str += ((e20100411 == g_format) || (e20111106 == g_format) || (e20170531 == g_format)) ? " AND role=\"a\";" : ";";

    mysql.query(str);

    if ((e20100206 == g_format) || (e20100317 == g_format) || (e20100411 == g_format)) {
        str = "SELECT `FirstName`,`MiddleName`,`LastName` FROM libavtorname WHERE aid=";
    } else if ((e20111106 == g_format) || (e20170531 == g_format)) {
        str = "SELECT `FirstName`,`MiddleName`,`LastName` FROM libavtors WHERE aid=";
    } else {
        str = "SELECT `FirstName`,`MiddleName`,`LastName` FROM libavtorname WHERE AvtorId=";
    }

    mysql_results avtor_ids(mysql);
    while (record = avtor_ids.fetch_row()) {
        string good_author_id(record[0]);

        if ((e20111106 == g_format) || (e20170531 == g_format)) {
            mysql.query(string("SELECT `main` FROM libavtors WHERE aid=") + good_author_id + ";");
            {
                mysql_results ids(mysql);

                if (record = ids.fetch_row()) {
                    if ((0 != strlen(record[0])) && (0 < atol(record[0]))) {
                        good_author_id = record[0];
                    }
                }
            }
        } else {
            if (g_have_alias_table) {
                mysql.query(string("SELECT `GoodId`,`BadId` FROM libavtoraliase WHERE BadId=") + good_author_id + ";");
                {
                    mysql_results ids(mysql);

                    if (record = ids.fetch_row()) {
                        if (0 != strlen(record[0])) {
                            good_author_id = record[0];
                        }
                    }
                }
            }
        }

        mysql.query(str + good_author_id + ";");
        {
            mysql_results author_name(mysql);

            if (record = author_name.fetch_row()) {
                author += fix_data(cleanse(record[2]), g_limits.A_Family);
                author += ",";
                author += fix_data(cleanse(record[0]), g_limits.A_Name);
                author += ",";
                author += fix_data(cleanse(record[1]), g_limits.A_Middle);
                author += ":";
            }
        }
    }
    if (author.size() < 4) {
        author = "неизвестный,автор,:";
    }
}

void get_book_rate(const mysql_connection& mysql, const string& book_id, string& rate)
{
    MYSQL_ROW record;

    rate.erase();

    string str = (eDefault == g_format) ? "SELECT ROUND(AVG(Rate),0) FROM librate WHERE BookId =" : "SELECT ROUND(AVG(Rate),0) FROM librate WHERE bid =";

    mysql.query(str + book_id + ";");

    mysql_results res(mysql);

    if (record = res.fetch_row()) {
        if (0 != record[0]) {
            rate = record[0];
        }
    }
}

void get_book_genres(const mysql_connection& mysql, const string& book_id, string& genres)
{
    MYSQL_ROW record;

    genres.erase();

    string str = (eDefault == g_format) ? "SELECT GenreID FROM libgenre WHERE BookId=" : "SELECT gid FROM libgenre WHERE bid=";

    mysql.query(str + book_id + ";");

    mysql_results genre_ids(mysql);

    if ((e20100206 == g_format) || (e20100317 == g_format)) {
        str = "SELECT GenreCode FROM libgenrelist WHERE gid=";
    } else if (e20100411 == g_format) {
        str = "SELECT code FROM libgenrelist WHERE gid=";
    } else if ((e20111106 == g_format) || (e20170531 == g_format)) {
        str = "SELECT code FROM libgenres WHERE gid=";
    } else {
        str = "SELECT GenreCode FROM libgenrelist WHERE GenreId=";
    }

    while (record = genre_ids.fetch_row()) {
        string genre_id(record[0]);

        mysql.query(str + genre_id + ";");

        mysql_results genre_code(mysql);

        if (record = genre_code.fetch_row()) {
            genres += cleanse(record[0]);
            genres += ":";
        }
    }

    if (genres.empty()) {
        genres = "other:";
    }
}

void get_book_squence(const mysql_connection& mysql, const string& book_id, string& sequence, string& seq_numb)
{
    MYSQL_ROW record;

    sequence.erase();
    seq_numb.erase();

    string str;

    if (e20100206 == g_format) {
        str = "SELECT `sid`,`SeqNumb` FROM libseq WHERE bid=" + book_id;
    } else if ((e20100317 == g_format) || (e20100411 == g_format) || (e20111106 == g_format) || (e20170531 == g_format)) {
        str = "SELECT `sid`,`sn` FROM libseq WHERE bid=" + book_id;
    } else {
        str = "SELECT `SeqId`,`SeqNumb` FROM libseq WHERE BookId=" + book_id;
        if (g_series_type == eAuthorST) {
            str += " ORDER BY Type ASC, Level ASC";
        } else if (g_series_type == ePublisherST) {
            str += " ORDER BY Type DESC, Level ASC";
        }
    }

    mysql.query(str + ";");

    mysql_results seq(mysql);

    while (record = seq.fetch_row()) {
        string seq_id(record[0]);

        // Make sure this is integer
        seq_numb = tmp_str("%d", (int)atof(record[1])).c_str();

        if ((e20100206 == g_format) || (e20100317 == g_format) || (e20100411 == g_format)) {
            str = "SELECT SeqName FROM libseqname WHERE sid=";
        } else if ((e20111106 == g_format) || (e20170531 == g_format)) {
            str = "SELECT SeqName FROM libseqs WHERE sid=";
        } else {
            str = "SELECT SeqName FROM libseqname WHERE SeqId=";
        }

        str = str + seq_id;

        if ((e20111106 == g_format) || (e20170531 == g_format)) {
            if (g_series_type == eAuthorST) {
                str += " AND type='a'";
            } else if (g_series_type == ePublisherST) {
                str += " AND type='p'";
            }
        }

        mysql.query(str + ";");

        mysql_results seq_name(mysql);

        if (record = seq_name.fetch_row()) {
            sequence += fix_data(cleanse(record[0]), g_limits.S_Title);
            break;
        }
    }
}

bool read_fb2(const unzip& uz, const string& book_id, fb2_parser& fb, unz_file_info64& fi, string& err)
{
    bool rc = false;

    DOUT("\n---------->Reading %s\n", book_id.c_str());

    const int                 buffer_size = 4096;
    boost::scoped_array<char> buffer(new char[buffer_size]);

    try {
        unzip_reader ur(uz);

        uz.current(fi);

        int  len                 = 0;
        bool continue_processing = true;

        while (continue_processing && (0 < (len = ur(buffer.get(), buffer_size)))) {
            continue_processing = fb(buffer.get(), len);
        }

        if (continue_processing) {
            fb(buffer.get(), 0, true);
        }

        rc = true;
    } catch (exception& e) {
        err = e.what();
    }
    return rc;
}

void process_book(const mysql_connection& mysql, MYSQL_ROW record, const string& file_name, const string& ext, const unzip* puz, string& inp)
{
    inp.erase();

    string book_id(record[0]);
    string book_title = fix_data(cleanse(record[1]), g_limits.Title);
    string book_filesize(record[2]);
    string book_type(record[3]);
    string book_deleted(record[4]);
    string book_time(record[5]);
    string book_lang(cleanse_lang(record[6]));
    string book_kwds = (record[7] == NULL) ? "" : fix_data(cleanse(record[7]), g_limits.KeyWords);

    string book_md5;
    if ((g_inpx_format == eRUKS) && (record[8] != NULL)) {
        book_md5 = record[8];
    }
    string book_replaced;
    if ((g_inpx_format == eRUKS) && (g_format == e20170531) && (record[9] != NULL)) {
        book_replaced = record[9];
    }

    string book_file(cleanse(file_name));

    string book_author, book_genres, book_sequence, book_sequence_num, book_rate, err;

    if (eFileExt == g_strict) {
        book_type = ext;
    }

    get_book_author(mysql, book_id, book_author);
    get_book_genres(mysql, book_id, book_genres);
    get_book_rate(mysql, book_id, book_rate);

    fb2_parser      fb;
    unz_file_info64 fi;

    if (g_fb2_preference == eReplaceFB2) {
        if ((puz != NULL) && read_fb2(*puz, book_id, fb, fi, err)) {
            book_sequence     = fb.m_seq_name;
            book_sequence_num = fb.m_seq;
        }
    } else {
        get_book_squence(mysql, book_id, book_sequence, book_sequence_num);

        if (g_fb2_preference == eMergeFB2) {
            if ((puz != NULL) && read_fb2(*puz, book_id, fb, fi, err)) {
                if (fb.m_seq_name.size() > 0) {
                    book_sequence     = fb.m_seq_name;
                    book_sequence_num = fb.m_seq;
                }
            }
        } else if (g_fb2_preference == eComplementFB2) {
            if (book_sequence.size() == 0) {
                if ((puz != NULL) && read_fb2(*puz, book_id, fb, fi, err)) {
                    book_sequence     = fb.m_seq_name;
                    book_sequence_num = fb.m_seq;
                }
            }
        }
    }

    book_time.erase(book_time.find(" ")); // Leave date only

    // AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;

    inp = book_author;
    inp += sep;
    inp += book_genres;
    inp += sep;
    inp += book_title;
    inp += sep;
    inp += book_sequence;
    inp += sep;
    inp += book_sequence_num;
    inp += sep;
    inp += ((eIgnore == g_strict) && (0 != strcasecmp(ext.c_str(), book_type.c_str()))) ? "" : book_file;
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
    if (g_inpx_format == eRUKS) {
        inp += book_md5;
        inp += sep;
        inp += book_replaced;
        inp += sep;
    }
    inp += "\r\n";
}

bool process_from_fb2(const unzip& uz, const string& book_id, string& inp, string& err)
{
    bool rc = false;

    DOUT("\n---------->Processing %s\n", book_id.c_str());

    err.erase();
    inp.erase();

    try {
        fb2_parser      fb;
        unz_file_info64 fi;

        if (read_fb2(uz, book_id, fb, fi, err)) {
            // AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;

            string authors, genres;
            for (vector<string>::const_iterator it = fb.m_authors.begin(); it != fb.m_authors.end(); ++it) {
                authors += (*it) + ":";
            }
            for (vector<string>::const_iterator it = fb.m_genres.begin(); it != fb.m_genres.end(); ++it) {
                genres += (*it) + ":";
            }

            inp = authors;
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
            inp += tmp_str("%d", fi.uncompressed_size);
            inp += sep;
            inp += book_id;
            inp += sep;
            //          inp += book_deleted;
            inp += sep;
            inp += "fb2";
            inp += sep;
            inp += to_iso_extended_string(date(((fi.dosDate >> 25) & 0x7F) + 1980, ((fi.dosDate >> 21) & 0x0F), ((fi.dosDate >> 16) & 0x1F)));
            inp += sep;
            inp += fb.m_language;
            inp += sep;
            //          inp += book_rate;
            inp += sep;
            inp += fb.m_keywords;
            inp += sep;
            if (g_inpx_format == eRUKS) {
                inp += sep;
            }
            inp += "\r\n";

            rc = true;
        }
    } catch (exception& e) {
        inp.erase();
        err = e.what();
    }
    return rc;
}

void name_to_bookid(const string& file, string& book_id, string& ext)
{
    book_id    = file;
    size_t pos = book_id.rfind(".");
    if (string::npos != pos) {
        ext = book_id.substr(pos + 1);
        book_id.erase(pos);
    }
}

void process_local_archives(const mysql_connection& mysql, const zip& zz, const string& archives_path)
{
    vector<string> files;

    for (auto&& x : fs::directory_iterator(archives_path)) {
        if (strcasecmp(x.path().extension().string().c_str(), ".zip") == 0) {
            files.push_back(x.path().filename().string());
        }
    }

    if (!g_update.empty()) {
        vector<string>::iterator it;

        struct finder_fb2_t {
            bool operator()(const string& arg) { return (0 == strncasecmp(arg.c_str(), "fb2-", 4)); }
        } finder_fb2;

        struct finder_usr_t {
            bool operator()(const string& arg) { return (0 == strncasecmp(arg.c_str(), "usr-", 4)); }
        } finder_usr;

        for (it = find_if(files.begin(), files.end(), finder_usr); it != files.end();) {
            files.erase(it, files.end());
        }

        sort(files.begin(), files.end());

        it = find(files.begin(), files.end(), g_update + ".zip");

        if (it == files.end()) {
            throw runtime_error(tmp_str("Unable to locate daily archive \"%s.zip\"", g_update.c_str()));
        }

        if (it != files.begin()) {
            files.erase(files.begin(), it);
        }

        it = find_if(files.begin(), files.end(), finder_fb2);

        if (it != files.end()) {
            files.erase(it, files.end());
        }
    }

    if (0 == files.size()) {
        throw runtime_error(tmp_str("No archives are available for processing \"%s\"", archives_path.c_str()));
    }
    wcout << endl << "Archives processing - " << files.size() << " file(s) [" << utf8_to_wchar(archives_path) << "]" << endl << endl << flush;

    sort(files.begin(), files.end());

    for (vector<string>::const_iterator it = files.begin(); it != files.end(); ++it) {
        vector<string> errors;
        string         name = "\"" + *it + "\"";
        name.append(max(0, (int)(25 - name.length())), ' ');

        string out_inp_name(*it);
        out_inp_name.replace(out_inp_name.end() - 3, out_inp_name.end(), string("inp"));

        long       records = 0, dummy_records = 0, fb2_records = 0;
        zip_writer zw(zz, out_inp_name, false);

        wcout << "Processing - " << utf8_to_wchar(name) << flush;

        timer ftd;
        unzip uz((archives_path + *it).c_str());

        for (unsigned int ni = 0; ni < uz.count(); ++ni) {

            if (is_backup(uz.current())) {
                if (ni < (uz.count() - 1)) {
                    uz.move_next();
                }
                continue;
            }

            string      inp, book_id, ext, stmt;
            const char* raw_stmt;
            bool        fdummy = false;
            bool        fb2    = is_fictionbook(uz.current());

            if (fb2) {
                if ((g_process == eAll) || ((g_process == eFB2))) {
                    name_to_bookid(uz.current(), book_id, ext);
                    if (eDefault != g_format) {
                        raw_stmt = "SELECT `bid`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords`%s FROM libbook WHERE bid=%s;";
                    } else {
                        raw_stmt = "SELECT `BookId`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords`%s FROM libbook WHERE BookId=%s;";
                    }
                    if (g_inpx_format != eRUKS) {
                        stmt = tmp_str(raw_stmt, "", book_id.c_str());
                    } else {
                        if (e20170531 == g_format) {
                            stmt = tmp_str(raw_stmt, ",`md5`,`ReplacedBy`", book_id.c_str());
                        } else {
                            stmt = tmp_str(raw_stmt, ",`md5`", book_id.c_str());
                        }
                    }
                } else {
                    fdummy = true;
                }
            } else {
                if ((g_process == eAll) || ((g_process == eUSR))) {
                    name_to_bookid(uz.current(), book_id, ext);
                    if (is_numeric(book_id) && ((e20100411 == g_format) || (e20111106 == g_format) || (e20170531 == g_format))) {
                        raw_stmt = "SELECT `bid`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords`%s FROM libbook WHERE bid=%s;";
                        if (g_inpx_format != eRUKS) {
                            stmt = tmp_str(raw_stmt, "", book_id.c_str());
                        } else {
                            if (e20170531 == g_format) {
                                stmt = tmp_str(raw_stmt, ",`md5`,`ReplacedBy`", book_id.c_str());
                            } else {
                                stmt = tmp_str(raw_stmt, ",`md5`", book_id.c_str());
                            }
                        }
                    } else {
                        if (eDefault != g_format) {
                            raw_stmt = "SELECT B.bid, B.Title, B.FileSize, B.FileType, B.Deleted, B.Time, B.Lang, B.KeyWords%s "
                                       "FROM libbook B, libfilename F WHERE B.bid = F.bid AND F.FileName = \"%s\";";
                        } else {
                            raw_stmt = "SELECT B.BookId, B.Title, B.FileSize, B.FileType, B.Deleted, B.Time, B.Lang, "
                                       "B.KeyWords%s FROM libbook B, libfilename F WHERE B.BookId = F.BookID AND F.FileName = \"%s\";";
                        }
                        if (g_inpx_format != eRUKS) {
                            stmt = tmp_str(raw_stmt, "", uz.current().c_str());
                        } else {
                            if (e20170531 == g_format) {
                                stmt = tmp_str(raw_stmt, ", B.md5, B.ReplacedBy", uz.current().c_str());
                            } else {
                                stmt = tmp_str(raw_stmt, ", B.md5", uz.current().c_str());
                            }
                        }
                    }
                } else {
                    fdummy = true;
                }
            }

            if (!book_id.empty()) {
                string    err;
                MYSQL_ROW record;

                mysql.query(stmt);

                mysql_results book(mysql);

                if (record = book.fetch_row()) {
                    process_book(mysql, record, book_id, ext, &uz, inp);
                } else {
                    if (fb2 && ((eReadAll == g_read_fb2) || ((eReadLast == g_read_fb2) && is_after_last(book_id)))) {
                        if (!process_from_fb2(uz, book_id, inp, err)) {
                            errors.push_back("       Skipped " + book_id + ".fb2 in archive due to \"" + err + "\"");
                        } else {
                            ++fb2_records;
                        }
                    }
                }
            }

            if (0 == inp.size()) {
                inp    = tmp_str(dummy, ni + 1);
                fdummy = true;
            }

            if (fdummy) {
                ++dummy_records;
            } else {
                ++records;

                if (!zw.is_open()) {
                    zw.open();
                }

                for (; dummy_records > 0; dummy_records--) {
                    zw(tmp_str(dummy, ni - dummy_records + 1));
                }

                zw(inp);
            }

            if (ni < (uz.count() - 1)) {
                uz.move_next();
            }
        }

        zw.close();

        wcout << " - done in " << utf8_to_wchar(ftd.passed()) << flush;
        if (0 == records) {
            wcout << " ==> Not in database!" << endl << flush;
        } else {
            wcout << " (" << records - fb2_records << ":" << fb2_records << ":" << dummy_records << " records)" << endl << flush;
        }

        for (vector<string>::const_iterator it = errors.begin(); it != errors.end(); ++it) {
            wcout << utf8_to_wchar(*it) << endl << flush;
        }
    }
}

void bookid_to_name(const mysql_connection& mysql, const string& book_id, string& name, string& ext)
{
    MYSQL_ROW record;

    name.erase();
    ext.erase();

    string str = (eDefault == g_format) ? "SELECT `BookId`, `FileName` FROM libfilename WHERE BookId =" : "SELECT `bid`, `FileName` FROM libfilename WHERE bid =";

    mysql.query(str + book_id + ";");

    mysql_results names(mysql);

    if (record = names.fetch_row()) {
        name_to_bookid(record[1], name, ext);
    }
}

void process_database(const mysql_connection& mysql, const zip& zz)
{
    MYSQL_ROW record;
    string    sep("\x04");

    string stmt, out_inp_name("online.inp");

    const char* raw_stmt = (eDefault == g_format) ? "SELECT `BookId`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords`%s FROM libbook %s ORDER BY BookId;"
                                                  : "SELECT `bid`,`Title`,`FileSize`,`FileType`,`Deleted`,`Time`,`Lang`,`keywords`%s FROM libbook %s ORDER BY bid;";

    const char* extra = (g_inpx_format == eRUKS) ? ((e20170531 == g_format) ? ",`md5`,`ReplacedBy`" : ",`md5`") : "";
    const char* where = (g_process == eFB2) ? "WHERE FileType = 'fb2'" : ((g_process == eUSR) ? "WHERE FileType != 'fb2'" : "");

    stmt = tmp_str(raw_stmt, extra, where);

    long       current = 0;
    long       records = 0;
    zip_writer zw(zz, out_inp_name, false);

    wcout << endl << "Database processing" << endl << flush;

    timer ftd;

    long last_filename_id = -1;

    if ((e20100411 == g_format) || (e20111106 == g_format) || (e20170531 == g_format)) {
        if (filenames_table_exist(mysql)) {
            last_filename_id = get_last_filename_id(mysql);

            wcout << endl << "Largest book id which has \"old\" filename is: " << last_filename_id << endl << flush;
        }
    }

    mysql.query(stmt);

    mysql_results books(mysql);

    while (record = books.fetch_row()) {
        if (++current % 3000 == 0) {
            wcout << "." << flush;
        }

        string inp, file_name, ext("fb2");

        if (0 == strcasecmp(record[3], ext.c_str())) {
            file_name = record[0];
        } else {
            if ((e20100411 == g_format) || (e20111106 == g_format) || (e20170531 == g_format)) {
                if ((last_filename_id < 0) || (last_filename_id < atol(record[0]))) {
                    file_name = record[0];
                    ext       = record[3];
                } else {
                    bookid_to_name(mysql, record[0], file_name, ext);
                }
            } else {
                bookid_to_name(mysql, record[0], file_name, ext);
            }
        }

        process_book(mysql, record, file_name, ext, NULL, inp);

        if (0 != inp.size()) {
            ++records;

            if (!zw.is_open()) {
                zw.open();
            }

            zw(inp);
        }
    }

    wcout << endl << current << " records " << flush;

    zw.close();

    wcout << "done in " << utf8_to_wchar(ftd.passed()) << endl << flush;
}

wostream& operator<<(wostream& os, const po::options_description& desc)
{
    stringstream buf;
    buf << desc;
    return os << utf8_to_wchar(buf.str());
}

int main(int argc, char* argv[])
{
    int rc = 1;

#ifdef _WIN32
    fflush(stdout);
    _setmode(_fileno(stdout), _O_U8TEXT);
    fflush(stderr);
    _setmode(_fileno(stderr), _O_U8TEXT);
#else
    setlocale(LC_ALL, "");
    wcout.sync_with_stdio(false);
    wcout.imbue(locale());
    wcerr.sync_with_stdio(false);
    wcerr.imbue(locale());
#endif

    string path, inpx, comment, comment_fname, collection_comment, inp_path, inpx_name, dump_date, full_date, db_name;

    vector<string> archives_path;

    const char* file_name;
    char        module_path[PATH_MAX + 1];

    try {
        vector<string> files;
        timer          td;

#ifdef _WIN32
        GetModuleFileName(NULL, module_path, sizeof module_path);
#else
        ssize_t t = readlink("/proc/self/exe", module_path, sizeof module_path);
#endif

        file_name = separate_file_name(module_path);

        // clang-format off
		po::options_description options("options");
		options.add_options()
		("help",                               "Print help message")
		("ignore-dump-date",                   "Ignore date in the dump files, use current UTC date instead")
		("clean-when-done",                    "Remove MYSQL database after processing")
		("process",     po::value< string >(), "What to process - \"fb2\", \"usr\", \"all\" (default: fb2)")
		("strict",      po::value< string >(), "What to put in INPX as file type - \"ext\", \"db\", \"ignore\" (default: ext). ext - use real file extension. db - use file type from database. ignore - ignore files with file extension not equal to file type")
		("no-import",                          "Do not import dumps, just check dump time and use existing database")
		("db-name",     po::value< string >(), "Name of MYSQL database (default: flibusta)")
		("archives",    po::value< string >(), "Path(s) to off-line archives. Multiple entries should be separated by ';'. Each path must be valid and must point to some archives, or processing would be aborted. (If not present - entire database is converted for online usage)")
		("read-fb2",    po::value< string >(), "When archived book is not present in the database - try to parse fb2 in archive to get information. \"all\" - do it for all absent books, \"last\" - only process books with ids larger than last database id (If not present - no fb2 parsing)")
		("prefer-fb2",  po::value< string >(), "Parse fb2 in archive to get sequence information (default: ignore). \"ignore\" - ignore information from fb2, \"merge\" - prefer info from fb2, \"complement\" - prefer info from database, \"replace\" - always use info from fb2")
		("sequence",    po::value< string >(), "How to process sequence types from database (default: author). \"ignore\" - don't do any processing. For librusec database format 2011-11-06: \"author\" - always select author's book sequence, \"publisher\" - always select publisher's book sequence. For flibusta: \"author\" - prefer author's book sequence, \"publisher\" - prefer publisher's book sequence")
		("out-dir",     po::value< string >(), "Where to put resulting inpx file and temporary MySQL database (default: <program_path>)")
		("inpx",        po::value< string >(), "Full name of output file (default: <program_path>/data/<db_name>_<db_dump_date>.inpx)")
		("comment",     po::value< string >(), "File name of template (UTF-8) for INPX comment")
		("update",      po::value< string >(), "Starting with \"<arg>.zip\" produce \"daily_update.zip\" (Works only for \"fb2\")")
		("db-format",   po::value< string >(), "Database format, change date (YYYY-MM-DD). Supported: 2010-02-06, 2010-03-17, 2010-04-11, 2011-11-06, 2017-05-31. (Default - old librusec format before 2010-02-06)")
		("clean-authors",                      "Clean duplicate authors (librusec)")
		("clean-aliases",                      "Fix libavtoraliase table (flibusta)")
		("inpx-format", po::value< string >(), "INPX format, Supported: 2.x, ruks (Default - MyHomeLib format 2.x)")
		("quick-fix",                          "Enforce MyHomeLib database size limits, works with fix-config parameter. (default: MyHomeLib 1.6.2 constrains)")
		("fix-config",  po::value< string >(), "Allows to specify configuration file with MyHomeLib database size constrains")
		("verbose",                            "More output... (default: off)")
		;
        // clang-format on

        po::options_description hidden;
        hidden.add_options()("dump-dir", po::value<string>());

        po::positional_options_description p;
        p.add("dump-dir", -1);

        po::options_description cmdline_options;
        cmdline_options.add(options).add(hidden);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("help") || !vm.count("dump-dir")) {
            wcout << endl;
            wcout << "Import file (INPX) preparation tool for MyHomeLib" << endl;
            wcout << "Version " << PRJ_VERSION_MAJOR << "." << PRJ_VERSION_MINOR << " (MYSQL " << MYSQL_SERVER_VERSION << ")" << endl;
            wcout << endl;
            wcout << "Usage: " << file_name << " [options] <path to SQL dump files>" << endl << endl;
            wcout << options << endl << flush;
            rc = 0;
            goto E_x_i_t;
        }

        if (vm.count("process")) {
            string opt = vm["process"].as<string>();
            if (0 == strcasecmp(opt.c_str(), "fb2")) {
                g_process = eFB2;
            } else if (0 == strcasecmp(opt.c_str(), "usr")) {
                g_process = eUSR;
            } else if (0 == strcasecmp(opt.c_str(), "all")) {
                g_process = eAll;
            } else {
                wcout << endl << "Warning: unknown processing type, assuming FB2 only!" << endl << flush;
                g_process = eFB2;
            }
        }

        if (vm.count("out-dir")) {
            g_outdir = vm["out-dir"].as<string>();
        } else {
            g_outdir = module_path;
        }
        normalize_path(g_outdir);

        if (0 != access(g_outdir.c_str(), 6)) {
            throw runtime_error(tmp_str("Unable to use output directory \"%s\"", g_outdir.c_str()));
        }

        if (vm.count("read-fb2")) {
            string opt = vm["read-fb2"].as<string>();
            if (0 == strcasecmp(opt.c_str(), "all")) {
                g_read_fb2 = eReadAll;
            } else if (0 == strcasecmp(opt.c_str(), "last")) {
                g_read_fb2 = eReadLast;
            } else {
                wcout << endl << "Warning: unknown read-fb2 action, assuming none!" << endl << flush;
                g_read_fb2 = eReadNone;
            }
        }

        if (vm.count("prefer-fb2")) {
            string opt = vm["prefer-fb2"].as<string>();
            if (0 == strcasecmp(opt.c_str(), "ignore")) {
                g_fb2_preference = eIgnoreFB2;
            } else if (0 == strcasecmp(opt.c_str(), "merge")) {
                g_fb2_preference = eMergeFB2;
            } else if (0 == strcasecmp(opt.c_str(), "complement")) {
                g_fb2_preference = eComplementFB2;
            } else if (0 == strcasecmp(opt.c_str(), "replace")) {
                g_fb2_preference = eReplaceFB2;
            } else {
                wcout << endl << "Warning: unknown prefer-fb2 action, assuming ignore!" << endl << flush;
                g_fb2_preference = eIgnoreFB2;
            }
        }

        if (vm.count("sequence")) {
            string opt = vm["sequence"].as<string>();
            if (0 == strcasecmp(opt.c_str(), "author")) {
                g_series_type = eAuthorST;
            } else if (0 == strcasecmp(opt.c_str(), "publisher")) {
                g_series_type = ePublisherST;
            } else if (0 == strcasecmp(opt.c_str(), "ignore")) {
                g_series_type = eIgnoreST;
            } else {
                wcout << endl << "Warning: unknown sequence type, assuming author's sequence!" << endl << flush;
                g_series_type = eAuthorST;
            }
        }

        if (vm.count("strict")) {
            string opt = vm["strict"].as<string>();
            if (0 == strcasecmp(opt.c_str(), "ext")) {
                g_strict = eFileExt;
            } else if (0 == strcasecmp(opt.c_str(), "db")) {
                g_strict = eFileType;
            } else if (0 == strcasecmp(opt.c_str(), "ignore")) {
                g_strict = eIgnore;
            } else {
                wcout << endl << "Warning: unknown strictness, will use file extensions!" << endl << flush;
                g_strict = eFileExt;
            }
        }

        if (vm.count("db-format")) {
            string opt = vm["db-format"].as<string>();
            if (0 == strcasecmp(opt.c_str(), "2010-02-06")) {
                g_format = e20100206;
            } else if (0 == strcasecmp(opt.c_str(), "2010-03-17")) {
                g_format = e20100317;
            } else if (0 == strcasecmp(opt.c_str(), "2010-04-11")) {
                g_format = e20100411;
            } else if (0 == strcasecmp(opt.c_str(), "2011-11-06")) {
                g_format = e20111106;
            } else if (0 == strcasecmp(opt.c_str(), "2017-05-31")) {
                g_format = e20170531;
            } else {
                wcout << endl << "Warning: unknown database format, will use default!" << endl << flush;
                g_format = eDefault;
            }
        }

        if (vm.count("inpx-format")) {
            string opt = vm["inpx-format"].as<string>();
            if (0 == strcasecmp(opt.c_str(), "2.x")) {
                g_inpx_format = e2X;
            } else if (0 == strcasecmp(opt.c_str(), "ruks")) {
                g_inpx_format = eRUKS;
            } else {
                wcout << endl << "Warning: unknown INPX format, will use default!" << endl << flush;
                g_inpx_format = e2X;
            }
        }

        if (vm.count("quick-fix")) {
            g_fix = true;

            string config;
            if (vm.count("fix-config")) {
                config = vm["fix-config"].as<string>();
            }

            initialize_limits(config);
        }

        if (vm.count("ignore-dump-date")) {
            g_ignore_dump_date = true;
        }

        if (vm.count("clean-when-done")) {
            g_clean_when_done = true;
        }

        if (vm.count("no-import")) {
            g_no_import = true;
        }

        if (vm.count("clean-authors")) {
            g_clean_authors = true;
        }

        if (vm.count("clean-aliases"))
            if ((e20111106 != g_format) && (e20170531 != g_format)) {
                g_clean_aliases = true;
            }

        if (vm.count("dump-dir")) {
            path = vm["dump-dir"].as<string>();
        }

        if (vm.count("db-name")) {
            g_db_name = vm["db-name"].as<string>();
        }

        db_name = g_db_name;
        db_name += "_";

        if (vm.count("comment")) {
            comment_fname = vm["comment"].as<string>();

            if (0 != access(comment_fname.c_str(), 4)) {
                wcout << endl << "Warning: Ignoring wrong comment file: " << utf8_to_wchar(comment_fname) << endl << flush;
            } else {
                ifstream     in(comment_fname.c_str());
                stringstream ss;

                if (!in) {
                    throw runtime_error(tmp_str("Cannot open comment file \"%s\"", comment_fname.c_str()));
                }

                ss << in.rdbuf();

                if (!in && !in.eof()) {
                    throw runtime_error(tmp_str("Problem reading comment file \"%s\"", comment_fname.c_str()));
                }

                comment = ss.str();
            }
        }

        if (vm.count("archives")) {
            string tmp = vm["archives"].as<string>();
            split(archives_path, tmp, is_any_of(";"), token_compress_on);
        }

        if (!archives_path.empty()) {
            for (vector<string>::iterator it = archives_path.begin(); it != archives_path.end(); ++it) {
                if (0 != access((*it).c_str(), 4)) {
                    throw runtime_error(tmp_str("Wrong path to archives \"%s\"", (*it).c_str()));
                }

                normalize_path((*it));
            }

            if ((eFB2 == g_process) && vm.count("update")) {
                if (1 == archives_path.size()) {
                    g_update = vm["update"].as<string>();

                    if (0 != access((archives_path[0] + g_update + ".zip").c_str(), 4)) {
                        throw runtime_error(tmp_str("Unable to find daily archive \"%s.zip\"", g_update.c_str()));
                    }
                } else {
                    throw runtime_error("daily_update.zip could only be built from a single archive path");
                }
            }
        }

        if (vm.count("verbose")) {
            g_verbose = true;
        }

        if (0 != access(path.c_str(), 4)) {
            throw runtime_error(tmp_str("Wrong source path \"%s\"", path.c_str()));
        }

        normalize_path(path);

        for (auto&& x : fs::directory_iterator(path)) {
            if (strcasecmp(x.path().extension().string().c_str(), ".sql") == 0) {

                files.push_back(x.path().filename().string());

                if (!g_ignore_dump_date) {
                    if (0 == dump_date.size()) {
                        dump_date = get_dump_date(path + x.path().filename().string());
                    } else {
                        string new_dump_date = get_dump_date(path + x.path().filename().string());

                        if (dump_date != new_dump_date) {
                            throw runtime_error(
                                tmp_str("Source dump files do not have the same date (%s) \"%s\" (%s)", dump_date.c_str(), x.path().filename().string().c_str(), new_dump_date.c_str()));
                        }
                    }
                }
            }
        }

        if (!g_no_import && (0 == files.size())) {
            throw runtime_error(tmp_str("No SQL dumps are available for importing \"%s\"", path.c_str()));
        }

        wcout << endl << "Detected MySQL dump date: " << utf8_to_wchar((0 == dump_date.size()) ? "none" : dump_date) << endl << flush;

        full_date = (0 == dump_date.size()) ? to_iso_extended_string(date(day_clock::universal_day())) : dump_date;
        dump_date = full_date;

        dump_date.erase(4, 1);
        dump_date.erase(6, 1);

        inpx_name = db_name;
        db_name += dump_date;

        if (g_process == eUSR) {
            inpx_name += "usr_" + dump_date;
        } else if (g_process == eAll) {
            inpx_name += "all_" + dump_date;
        } else {
            if (!archives_path.empty()) {
                inpx_name += dump_date;
            } else {
                inpx_name += "online_" + dump_date;
            }
        }

        if (!g_update.empty()) {
            inpx_name = "daily_update";
        }

        prepare_mysql(g_outdir.c_str(), db_name);

        if (vm.count("inpx")) {
            inpx = vm["inpx"].as<string>();
        } else {
            inpx = g_outdir;
            // Keep compatible with old versions
            if (!vm.count("out-dir")) {
                inpx += "/data/";
            }
            inpx += inpx_name;
            inpx += g_update.empty() ? ".inpx" : ".zip";
        }

        {
            string table_name = ((e20111106 == g_format) || (e20170531 == g_format)) ? "libavtors" : "libavtorname";

            mysql_connection mysql(module_path, g_outdir.c_str(), g_db_name.c_str(), db_name.c_str());

            if (!g_no_import) {
                wcout << endl << "Creating MYSQL database \"" << utf8_to_wchar(db_name) << "\"" << endl << endl << flush;
            }

            mysql.query(string("CREATE DATABASE IF NOT EXISTS " + db_name + " CHARACTER SET=utf8;"));
            mysql.query(string("USE ") + db_name + ";");
            mysql.query(string("SET NAMES 'utf8';"));

            for (vector<string>::const_iterator it = files.begin(); (it != files.end()) && (!g_no_import); ++it) {
                string name = "\"" + *it + "\"";
                name.append(max(0, (int)(25 - name.length())), ' ');

                wcout << "Importing - " << utf8_to_wchar(name) << flush;

                timer ftd;

                string   buf, line;
                ifstream in((path + *it).c_str());
                regex    sl_comment("^(--|#).*");
                bool     authors = g_clean_authors ? (string::npos != it->find(table_name)) : false;
                bool     aliases = g_clean_aliases ? (string::npos != it->find("libavtoraliase")) : false;

                if (!in) {
                    throw runtime_error(tmp_str("Cannot open file \"%s\"", (*it).c_str()));
                }

                getline(in, buf);
                while (in) {
                    if (0 != buf.size()) {
                        size_t pos = buf.find_first_not_of(" \t");
                        if (pos != 0) {
                            buf.erase(0, pos);
                        }

                        if (!regex_match(buf, sl_comment) && !(authors && (0 == buf.find("UNIQUE KEY `fullname` (`FirstName`,`MiddleName`,`LastName`,`NickName`),")))) {
                            pos = buf.rfind(';');
                            if (pos == string::npos) {
                                if (aliases && ((0 == buf.find("`AliaseId` int(11) NOT NULL auto_increment,")) || (0 == buf.find("`AliaseId` int(11) NOT NULL AUTO_INCREMENT,")))) {
                                    line += buf;
                                    line += "`dummyId` int(11) NOT NULL default '0',";
                                } else {
                                    line += buf;
                                }
                                buf.erase();
                                goto C_o_n_t;
                            } else {
                                if (aliases && (0 == buf.find("INSERT INTO `libavtoraliase`"))) {
                                    size_t len = strlen("INSERT INTO `libavtoraliase`");
                                    line += "INSERT INTO `libavtoraliase` (dummyId, BadId, GoodId)";
                                    buf.erase(0, len);
                                    pos -= len;
                                }
                                line += buf.substr(0, pos + 1);
                                buf.erase(0, pos + 1);
                            }

                            mysql.real_query(line.c_str(), (int)line.size());

                            line = buf;
                        }
                    }
                C_o_n_t:
                    getline(in, buf);
                }

                wcout << " - done in " << utf8_to_wchar(ftd.passed()) << endl << flush;
            }

            // Starting from 03/19/2017 flibusta stopped exporting lib.libavtoraliase.sql
            if ((e20111106 != g_format) && (e20170531 != g_format)) {
                g_have_alias_table = authoraliases_table_exist(mysql);
                if (!g_have_alias_table) {
                    wcout << endl << "Warning: absent \"lib.libavtoraliase.sql\"! For some authors names could be incorrect..." << endl << flush;
                }
            }

            if (g_clean_authors) {
                MYSQL_ROW record;

                mysql.query(tmp_str("SELECT Firstname, Middlename, Lastname, Nickname FROM %s GROUP BY Firstname, Middlename, Lastname, Nickname HAVING COUNT(*) > 1", table_name.c_str()));
                mysql_results dupes(mysql);

                wcout << endl << "Processing duplicate authors" << endl << flush;

                while (record = dupes.fetch_row()) {
                    MYSQL_ROW record1;
                    string    first;
                    bool      not_first = false;

                    mysql.query(tmp_str("SELECT aid FROM %s WHERE Firstname='%s' AND Middlename='%s' AND Lastname='%s' AND Nickname='%s' ORDER by aid;", table_name.c_str(),
                        duplicate_quote(record[0]).c_str(), duplicate_quote(record[1]).c_str(), duplicate_quote(record[2]).c_str(), duplicate_quote(record[3]).c_str()));
                    mysql_results aids(mysql);

                    while (record1 = aids.fetch_row()) {
                        MYSQL_ROW record2;

                        if (first.empty()) {
                            first = record1[0];
                        } else {
                            not_first = true;
                        }

                        mysql.query(string("SELECT COUNT(*) FROM libavtor WHERE aid=") + record1[0] + ";");
                        mysql_results books(mysql);

                        if ((record2 = books.fetch_row()) && (0 != strlen(record2[0]))) {
                            long count = atol(record2[0]);

                            if (not_first) {
                                if (g_verbose)
                                    wcout << "   De-duping author " << setw(8) << record1[0] << " (" << setw(4) << count << ") : " << utf8_to_wchar(record[2]) << "-"
                                          << utf8_to_wchar(record[0]) << "-" << utf8_to_wchar(record[1]) << endl
                                          << flush;
                                if (0 < count) {
                                    mysql.query(tmp_str("UPDATE libavtor SET aid=%s WHERE aid=%s;", first.c_str(), record1[0]), false);
                                }
                                mysql.query(tmp_str("DELETE FROM %s WHERE aid=%s;", table_name.c_str(), record1[0]));
                            } else {
                                if (0 == count) {
                                    if (g_verbose)
                                        wcout << "*  De-duping author " << setw(8) << record1[0] << " (" << setw(4) << count << ") : " << utf8_to_wchar(record[2]) << "-"
                                              << utf8_to_wchar(record[0]) << "-" << utf8_to_wchar(record[1]) << endl
                                              << flush;

                                    mysql.query(tmp_str("DELETE FROM %s WHERE aid=%s;", table_name.c_str(), record1[0]));
                                    first.clear();
                                }
                            }
                        }
                    }
                }
            }

            if (eReadLast == g_read_fb2) {
                MYSQL_ROW record;

                string str = (eDefault == g_format) ? "SELECT MAX(`BookId`) FROM libbook WHERE FileType = 'fb2';" : "SELECT MAX(`bid`) FROM libbook WHERE FileType = 'fb2';";

                mysql.query(str);

                mysql_results last(mysql);

                if (record = last.fetch_row()) {
                    g_last_fb2 = atol(record[0]);
                }

                wcout << endl << "Largest FB2 book id in database: " << g_last_fb2 << endl << flush;
            }

            if (comment.empty()) {
                if (!archives_path.empty()) {
                    if (g_process == eFB2) {
                        comment = tmp_str("%s FB2 - %s\r\n%s\r\n65536\r\nЛокальные архивы библиотеки %s (FB2) %s", g_db_name.c_str(), full_date.c_str(), inpx_name.c_str(), g_db_name.c_str(),
                            full_date.c_str());
                    } else if (g_process == eUSR) {
                        comment = tmp_str("%s USR - %s\r\n%s\r\n65537\r\nЛокальные архивы библиотеки %s (не-FB2) %s", g_db_name.c_str(), full_date.c_str(), inpx_name.c_str(),
                            g_db_name.c_str(), full_date.c_str());
                    } else if (g_process == eAll) {
                        comment = tmp_str("%s ALL - %s\r\n%s\r\n65537\r\nЛокальные архивы библиотеки %s (все) %s", g_db_name.c_str(), full_date.c_str(), inpx_name.c_str(), g_db_name.c_str(),
                            full_date.c_str());
                    }
                } else {
                    comment = tmp_str(
                        "%s FB2 online - %s\r\n%s\r\n134283264\r\nOnline коллекция %s %s", g_db_name.c_str(), full_date.c_str(), inpx_name.c_str(), g_db_name.c_str(), full_date.c_str());
                }
            } else {
                comment = tmp_str(comment.c_str(), inpx_name.c_str());
            }

            collection_comment = "\xEF\xBB\xBF";
            collection_comment += comment;

            zip zz(inpx, tmp_str("%s - %s", g_db_name.c_str(), full_date.c_str()));

            if (archives_path.empty()) {
                process_database(mysql, zz);
            } else {
                for (vector<string>::const_iterator it = archives_path.begin(); it != archives_path.end(); ++it) {
                    process_local_archives(mysql, zz, (*it));
                }
            }

            {
                zip_writer zw(zz, "collection.info");
                zw(collection_comment);
                zw.close();
            }
            {
                zip_writer zw(zz, "version.info");
                zw(dump_date + "\r\n");
                zw.close();
            }
            zz.close();

            wcout << endl << "Complete processing took " << utf8_to_wchar(td.passed()) << endl << endl << flush;
        }

        if (g_clean_when_done) {
            string data_dir(g_outdir);
            data_dir += "/data/";

            string db_tmp_dir = data_dir + "dbtmp_" + db_name + "/";
            fs::remove_all(db_tmp_dir.c_str());
            string db_dir = data_dir + db_name + "/";
            fs::remove_all(db_dir.c_str());

            string file_to_del = data_dir + "/auto.cnf";
            fs::remove(file_to_del);

            if (g_mysql_cfg_created) {
                file_to_del.assign(data_dir + "/mysql.cfg");
                fs::remove(file_to_del);
            }

#ifdef MARIADB_BASE_VERSION
            file_to_del.assign(data_dir + "/aria_log.00000001");
            fs::remove(file_to_del);
            file_to_del.assign(data_dir + "/aria_log_control");
            fs::remove(file_to_del);
#endif

            if (fs::is_empty(data_dir)) {
                fs::remove(data_dir);
            }
        }

        rc = 0;

    } catch (exception& e) {
        wcerr << endl << endl << "***ERROR: " << utf8_to_wchar(e.what()) << endl << flush;
    }

E_x_i_t:

    return rc;
}
