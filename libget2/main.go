package main

import (
	"errors"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"golang.org/x/net/proxy"
	"log"
	"net"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"
)

const emptyString = ""

var library string
var retry int
var timeout int
var noSql bool
var noRedirect bool
var dest string
var destSql string
var configPath string
var verbose bool
var ranges bool

var reNumbers *regexp.Regexp
var lastBook int
var proxyUrl *url.URL

func init() {
	pwd, err := os.Getwd()
	if err != nil {
		log.Fatal(err)
	}
	flag.StringVar(&library, "library", "flibusta", "name of the library profile")
	flag.IntVar(&retry, "retry", 3, "number of re-tries")
	flag.IntVar(&timeout, "timeout", 20, "communication timeout in seconds")
	flag.BoolVar(&noSql, "nosql", false, "do not download database")
	flag.BoolVar(&noRedirect, "sticky", false, "(hack) ignore http redirects, stick with original host address")
	flag.BoolVar(&verbose, "verbose", false, "print detailed information")
	flag.BoolVar(&ranges, "continue", false, "continue getting a partially-downloaded file")
	flag.StringVar(&dest, "to", pwd, "destination directory for archives")
	flag.StringVar(&destSql, "tosql", emptyString, "destination directory for database files")
	flag.StringVar(&configPath, "config", filepath.Join(pwd, "libget2.conf"), "configuration file")
	if reNumbers, err = regexp.Compile("(?i)\\s*([0-9]+)-([0-9]+).zip"); err != nil {
		log.Fatal(err)
	}
}

func getLastBook(path string) (book int, err error) {
	var archives []os.FileInfo
	if archives, err = ioutil.ReadDir(path); err != nil {
		return
	}
	for _, f := range archives {
		if _, snd, err := dissect(f.Name()); err == nil {
			if book < snd {
				book = snd
			}
		} else {
			return 0, err
		}
	}
	return
}

func acceptsRanges(url string) (accepts bool, err error) {
	resp, err := httpReq(url, "HEAD", 1)
	if err != nil {
		return
	}
	defer resp.Body.Close()

	_, err = ioutil.ReadAll(resp.Body)
	if err != nil {
		return
	}

	return resp.StatusCode == http.StatusPartialContent, nil
}

func httpReq(hostAddr, verb string, start int64) (*http.Response, error) {
	hostUrl, err := url.Parse(hostAddr)
	if err != nil {
		return nil, err
	}

	var redirect = func(req *http.Request, via []*http.Request) error {
		if len(via) >= 10 {
			return errors.New("stopped after 10 redirects")
		}
		if verbose {
			fmt.Printf("Detected redirect from \"%s\" to \"%s\"", hostUrl.Host, req.URL.Host)
		}
		if noRedirect {
			if verbose {
				fmt.Printf("\t...Ignoring")
			}
			req.URL.Host = hostUrl.Host
		}
		if verbose {
			fmt.Println()
		}
		return nil
	}

	var transport *http.Transport
	if proxyUrl != nil {
		switch proxyUrl.Scheme {
		case "socks5":
			var a *proxy.Auth
			if proxyUrl.User != nil && len(proxyUrl.User.Username()) > 0 {
				p, _ := proxyUrl.User.Password()
				a = &proxy.Auth{User: proxyUrl.User.Username(), Password: p}
			}
			p, err := proxy.SOCKS5("tcp4", proxyUrl.Host, a, proxy.Direct)
			if err != nil {
				return nil, err
			}
			transport = &http.Transport{Dial: p.Dial, DisableKeepAlives: true}
		case "http":
			transport = &http.Transport{Dial: net.Dial, DisableKeepAlives: true}
		default:
			return nil, errors.New("Unsupported proxy scheme: " + proxyUrl.Scheme)
		}
	} else {
		transport = &http.Transport{DisableKeepAlives: true}
	}
	client := &http.Client{CheckRedirect: redirect, Timeout: time.Second * time.Duration(timeout), Transport: transport}

	req, err := http.NewRequest(verb, hostAddr, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("Referer", hostAddr)
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows; en-US)")
	req.Header.Set("Cache-Control", "no-cache, no-store, must-revalidate")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Expires", "0")
	if ranges && start > 0 {
		req.Header.Set("Range", "bytes="+strconv.FormatInt(start, 10)+"-")
	}
	return client.Do(req)
}

func fetchString(url string) (string, error) {
	resp, err := httpReq(url, "GET", 0)
	if err != nil {
		return emptyString, err
	}
	defer resp.Body.Close()

	bodyb, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return emptyString, err
	}
	return string(bodyb), nil
}

func getLinks(url, pattern string) (links []string, err error) {
	var body string
	for ni := 1; ni <= retry; ni++ {
		fmt.Printf("\nDownloading index for %-30.30s (%d) ", url, ni)
		body, err = fetchString(url)
		if err == nil {
			break
		}
		fmt.Print(err)
	}
	// fmt.Println()
	if err != nil {
		err = errors.New("Unable to get index html for new archives")
		return
	}
	var re *regexp.Regexp
	if re, err = regexp.Compile(pattern); err != nil {
		return
	}
	if m := re.FindAllStringSubmatch(body, -1); m != nil {
		links = make([]string, 0, len(m))
		for _, v := range m {
			if fst, _, err := dissect(v[1]); err == nil && lastBook < fst {
				links = append(links, v[1])
			}
		}
	} else {
		err = errors.New("No sutable links found for new archives")
	}
	return
}

func getSqlLinks(url, pattern string) (links []string, err error) {
	var body string
	for ni := 1; ni <= retry; ni++ {
		fmt.Printf("\nDownloading SQL tables for %-30.30s (%d) ", url, ni)
		body, err = fetchString(url)
		if err == nil {
			break
		}
		fmt.Print(err)
	}
	// fmt.Println()
	if err != nil {
		err = errors.New("Unable to get index html for SQL tables")
		return
	}
	var re *regexp.Regexp
	if re, err = regexp.Compile(pattern); err != nil {
		return
	}
	if m := re.FindAllStringSubmatch(body, -1); m != nil {
		links = make([]string, len(m))
		for ni, v := range m {
			links[ni] = v[1]
		}
	} else {
		err = errors.New("No sutable links found for SQL tables")
	}
	return
}

func fetchFile(url, tmpIn string, start int64) (tmpOut string, size int64, err error) {
	var out *os.File

	tmpOut = tmpIn
	size = start

	if len(tmpIn) > 0 {
		if start > 0 {
			out, err = os.OpenFile(tmpIn, os.O_RDWR|os.O_APPEND, 0666)
			if err != nil {
				return
			}
			_, err = out.Seek(start, os.SEEK_SET)
			if err != nil {
				return
			}
		} else {
			out, err = os.Create(tmpIn)
			if err != nil {
				return
			}
		}
	} else {
		out, err = ioutil.TempFile("", "libget")
		if err != nil {
			return
		}
		tmpOut = out.Name()
	}
	defer out.Close()

	resp, err := httpReq(url, "GET", start)
	if err != nil {
		return
	}
	defer resp.Body.Close()

	size, err = io.Copy(out, resp.Body)
	size = size + start

	return
}

func processFile(tmp, file string) (err error) {
	ext := filepath.Ext(file)
	switch ext {
	case ".zip":
		if err = checkZip(tmp); err == nil {
			err = copyFileContents(tmp, file)
		}
	case ".gz":
		err = unGzipFile(tmp, strings.TrimSuffix(file, ext))
	default:
		err = errors.New("Unknown file extension")
	}
	return
}

func getFiles(files []string, url, dest string) (err error) {
	for _, f := range files {
		var start int64
		var tmp string
		var with_ranges bool
		for ni := 1; ni <= retry; ni++ {
			if ranges && start > 0 {
				for nj := 1; nj <= retry; nj++ {
					with_ranges, err = acceptsRanges(joinUrl(url, f))
					if err == nil {
						if verbose {
							if with_ranges {
								fmt.Print("   ...Server supports ranges")
							} else {
								fmt.Print("   ...Server does not support ranges")
							}
						}
						break
					}
				}
			}
			if !with_ranges {
				start = 0
			}
			fmt.Printf("\nDownloading file %-35.35s (%-3d:%012d) ", f, ni, start)
			tmp, start, err = fetchFile(joinUrl(url, f), tmp, start)
			if err == nil {
				break
			}
			fmt.Print(err)
		}
		defer os.Remove(tmp)

		//fmt.Println()
		if err != nil {
			return errors.New("Unable to download files")
		}

		err = processFile(tmp, filepath.Join(dest, f))
		if err != nil {
			break
		}
	}
	return
}

func main() {
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "\nTool to download library updates\nVersion %s\n\n", getVersion())
		fmt.Fprintf(os.Stderr, "Usage: %s [options]\n\n", os.Args[0])
		flag.PrintDefaults()
	}
	if flag.Parse(); flag.NFlag() == 0 {
		flag.Usage()
		os.Exit(0)
	}

	conf, err := ReadLibraries(configPath)
	if err != nil {
		log.Fatal(err)
	}
	lib := conf.Find(library)
	if lib == nil {
		log.Fatalf("Unable to find configuration: \"%s\"\n", library)
	}

	// Allow library name to be different from configuration name
	libName := lib.Name
	if len(lib.NameLib) > 0 {
		libName = lib.NameLib
	}

	fmt.Printf("\nProcessing library \"%s\" from configuration \"%s\"\n", libName, library)

	if dest, err = filepath.Abs(dest); err != nil {
		log.Fatal(err)
	}
	if len(destSql) == 0 {
		destSql = libName + "_" + time.Now().UTC().Format("20060102_150405")
	}
	if destSql, err = filepath.Abs(destSql); err != nil {
		log.Fatal(err)
	}

	if lastBook, err = getLastBook(dest); err != nil {
		log.Fatal(err)
	}
	if verbose {
		fmt.Printf("Last book id: %d\n", lastBook)
	}

	if len(lib.Proxy) > 0 {
		if proxyUrl, err = url.Parse(lib.Proxy); err != nil {
			log.Fatal(err)
		}
		if verbose {
			// Hide user:password if any
			u := &url.URL{Scheme: proxyUrl.Scheme, Host: proxyUrl.Host}
			fmt.Printf("Using proxy: \"%s\"\n", u.String())
		}
	}

	var links []string
	if links, err = getLinks(lib.URL, lib.Pattern); err != nil {
		log.Fatal(err)
	}
	if err = os.MkdirAll(dest, 0777); err != nil {
		log.Fatal(err)
	}
	if err = getFiles(links, lib.URL, dest); err != nil {
		log.Fatal(err)
	}
	newArchives := len(links)
	if newArchives > 0 {
		fmt.Printf("\nProcessed %d new archive(s)\n", newArchives)
	} else {
		fmt.Println()
	}
	if noSql {
		fmt.Println("\nDone...")
		os.Exit(0)
	}

	if links, err = getSqlLinks(lib.UrlSQL, lib.PatternSQL); err != nil {
		log.Fatal(err)
	}
	if err = os.MkdirAll(destSql, 0777); err != nil {
		log.Fatal(err)
	}
	if err = getFiles(links, lib.UrlSQL, destSql); err != nil {
		log.Fatal(err)
	}
	if len(links) > 0 {
		fmt.Printf("\nProcessed %d SQL table(s)\n", len(links))
	} else {
		fmt.Println()
	}
	fmt.Println("\nDone...")
	os.Exit(-newArchives)
}
