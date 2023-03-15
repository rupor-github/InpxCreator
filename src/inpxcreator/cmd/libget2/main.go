package main

import (
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"time"

	"golang.org/x/net/proxy"

	"inpxcreator/misc"
)

const emptyString = ""

var library string
var retry int
var timeout int
var chunkSize int
var noSQL bool
var noRedirect bool
var dest string
var destSQL string
var configPath string
var verbose bool
var ranges bool

var reNumbers *regexp.Regexp
var reMerge *regexp.Regexp
var lastBook int
var proxyURL *url.URL

// LastGitCommit is used during build to inject git sha
var LastGitCommit string

func init() {
	pwd, err := os.Getwd()
	if err != nil {
		log.Fatal(err)
	}
	flag.StringVar(&library, "library", "flibusta", "name of the library profile")
	flag.IntVar(&retry, "retry", 3, "number of re-tries")
	flag.IntVar(&timeout, "timeout", 20, "communication timeout in seconds")
	flag.IntVar(&chunkSize, "chunksize", 10, "Size of the chunk (megabytes) to be received in timeout time")
	flag.BoolVar(&noSQL, "nosql", false, "do not download database")
	flag.BoolVar(&noRedirect, "sticky", false, "(hack) ignore http redirects, stick with original host address")
	flag.BoolVar(&verbose, "verbose", false, "print detailed information")
	flag.BoolVar(&ranges, "continue", false, "continue getting a partially-downloaded file (until retry limit is reached)")
	flag.StringVar(&dest, "to", pwd, "destination directory for archives")
	flag.StringVar(&destSQL, "tosql", emptyString, "destination directory for database files")
	flag.StringVar(&configPath, "config", filepath.Join(pwd, "libget2.conf"), "configuration file")
	if reNumbers, err = regexp.Compile("(?i)\\s*([0-9]+)-([0-9]+).zip"); err != nil {
		log.Fatal(err)
	}
	if reMerge, err = regexp.Compile("(?i)\\s*fb2-([0-9]+)-([0-9]+).merging"); err != nil {
		log.Fatal(err)
	}
}

func getLastBook(path string) (int, error) {

	archives, err := os.ReadDir(path)
	if err != nil {
		return 0, fmt.Errorf("getLastBook: %w", err)
	}

	var lastBegin, lastEnd, mergeBegin, mergeEnd int

	for _, f := range archives {

		if ok, fst, snd, err := dissect(reNumbers, f.Name()); ok {
			if lastEnd < snd {
				lastBegin = fst
				lastEnd = snd
			}
		} else if err != nil {
			return 0, fmt.Errorf("getLastBook: %w", err)
		}
	}

	var count int
	for _, f := range archives {

		if ok, fst, snd, err := dissect(reMerge, f.Name()); ok {
			mergeBegin = fst
			mergeEnd = snd
			count++
		} else if err != nil {
			return 0, fmt.Errorf("getLastBook: %w", err)
		}
	}
	if count > 1 {
		return 0, errors.New("getLastBook: there could only be single merge archive")
	} else if count == 0 {
		return lastEnd, nil
	} else if mergeBegin < lastBegin || mergeBegin > lastBegin && mergeBegin <= lastEnd || mergeEnd < lastEnd {
		return 0, fmt.Errorf("getLastBook: merge (%d:%d) and last (%d:%d) archive do not match", mergeBegin, mergeEnd, lastBegin, lastEnd)
	} else {
		return mergeEnd, nil
	}
}

func httpReq(hostAddr, verb string, start int64) (*http.Response, *time.Timer, error) {

	hostURL, err := url.Parse(hostAddr)
	if err != nil {
		return nil, nil, fmt.Errorf("httpReq: %w", err)
	}

	var redirect = func(req *http.Request, via []*http.Request) error {

		if len(via) >= 10 {
			return errors.New("httpReq: stopped after 10 redirects")
		}
		if verbose {
			fmt.Printf("Detected redirect from \"%s\" to \"%s\"", hostURL.Host, req.URL.Host)
		}
		if noRedirect {
			if verbose {
				fmt.Printf("\t...Ignoring")
			}
			req.URL.Host = hostURL.Host
		}
		if verbose {
			fmt.Println()
		}
		return nil
	}

	var transport *http.Transport

	if proxyURL != nil {
		switch proxyURL.Scheme {
		case "socks5":
			var a *proxy.Auth
			if proxyURL.User != nil && len(proxyURL.User.Username()) > 0 {
				p, _ := proxyURL.User.Password()
				a = &proxy.Auth{User: proxyURL.User.Username(), Password: p}
			}
			p, err := proxy.SOCKS5("tcp4", proxyURL.Host, a, proxy.Direct)
			if err != nil {
				return nil, nil, fmt.Errorf("httpReq: %w", err)
			}
			transport = &http.Transport{Dial: p.Dial, DisableKeepAlives: true}
		case "http":
			transport = &http.Transport{DisableKeepAlives: true}
		default:
			return nil, nil, errors.New("httpReq: Unsupported proxy scheme: " + proxyURL.Scheme)
		}
	} else {
		transport = &http.Transport{DisableKeepAlives: true}
	}
	client := &http.Client{CheckRedirect: redirect, Timeout: 0, Transport: transport}

	c := make(chan struct{})
	timer := time.AfterFunc(time.Duration(timeout)*time.Second, func() {
		close(c)
	})

	req, err := http.NewRequest(verb, hostAddr, nil)
	if err != nil {
		timer.Stop()
		return nil, nil, fmt.Errorf("httpReq: %w", err)
	}
	req.Cancel = c

	req.Header.Set("Referer", hostAddr)
	req.Header.Set("User-Agent", fmt.Sprintf("libget2/%s", misc.GetVersion()))
	req.Header.Set("Cache-Control", "no-cache, no-store, must-revalidate")
	req.Header.Set("Pragma", "no-cache")
	req.Header.Set("Expires", "0")
	if ranges && start > 0 {
		req.Header.Set("Range", "bytes="+strconv.FormatInt(start, 10)+"-")
	}

	resp, err := client.Do(req)
	if err != nil {
		timer.Stop()
		return nil, nil, fmt.Errorf("httpReq: %w", err)
	}
	if resp.StatusCode != http.StatusOK {
		timer.Stop()
		return nil, nil, errors.New("httpReq: status [" + resp.Status + "]")
	}
	return resp, timer, nil
}

func acceptsRanges(url string) (bool, error) {

	resp, _, err := httpReq(url, "HEAD", 1)
	if err != nil {
		return false, fmt.Errorf("acceptsRanges: %w", err)
	}
	defer resp.Body.Close()

	_, err = io.ReadAll(resp.Body)
	if err != nil {
		return false, fmt.Errorf("acceptsRanges: %w", err)
	}

	return resp.StatusCode == http.StatusPartialContent, nil
}

func fetchString(url string) (string, error) {

	resp, _, err := httpReq(url, "GET", 0)
	if err != nil {
		return emptyString, fmt.Errorf("fetchString: %w", err)
	}
	defer resp.Body.Close()

	bodyb, err := io.ReadAll(resp.Body)
	if err != nil {
		return emptyString, fmt.Errorf("fetchString: %w", err)
	}
	return string(bodyb), nil
}

func getLinks(url, pattern string) ([]string, error) {

	var err error
	var links []string
	var body string

	for ni := 1; ni <= retry; ni++ {
		fmt.Printf("\nDownloading index for %-30.30s (%d) ", url, ni)
		body, err = fetchString(url)
		if err == nil {
			break
		}
		fmt.Print(err)
	}

	if err != nil {
		return nil, fmt.Errorf("unable to get index html for new archives: %w", err)
	}

	var re *regexp.Regexp
	if re, err = regexp.Compile(pattern); err != nil {
		return nil, fmt.Errorf("getLinks: %w", err)
	}

	if m := re.FindAllStringSubmatch(body, -1); m != nil {
		links = make([]string, 0, len(m))
		for _, v := range m {
			if ok, fst, _, err := dissect(reNumbers, v[1]); err == nil && ok && lastBook < fst {
				links = append(links, v[1])
			}
		}
	} else {
		err = errors.New("getLinks: No sutable links found for new archives")
	}
	return links, err
}

func getSQLLinks(url, pattern string) ([]string, error) {

	var err error
	var links []string
	var body string

	for ni := 1; ni <= retry; ni++ {

		fmt.Printf("\nDownloading SQL tables for %-30.30s (%d) ", url, ni)
		if body, err = fetchString(url); err == nil {
			break
		}
		fmt.Print(err)
	}

	if err != nil {
		return nil, fmt.Errorf("unable to get index html for SQL tables: %w", err)
	}

	var re *regexp.Regexp
	if re, err = regexp.Compile(pattern); err != nil {
		return nil, fmt.Errorf("getSQLLinks: %w", err)
	}

	if m := re.FindAllStringSubmatch(body, -1); m != nil {
		links = make([]string, len(m))
		for ni, v := range m {
			links[ni] = v[1]
		}
	} else {
		err = errors.New("getSQLLinks: No sutable links found for SQL tables")
	}

	return links, err
}

func fetchFile(url, tmpIn string, start int64) (tmpOut string, size int64, err error) {

	var out *os.File

	tmpOut = tmpIn
	size = start

	if len(tmpIn) > 0 {
		if start > 0 {
			out, err = os.OpenFile(tmpIn, os.O_RDWR|os.O_APPEND, 0666)
			if err != nil {
				err = fmt.Errorf("fetchFile: %w", err)
				return
			}
			_, err = out.Seek(start, os.SEEK_SET)
			if err != nil {
				err = fmt.Errorf("fetchFile: %w", err)
				return
			}
		} else {
			out, err = os.Create(tmpIn)
			if err != nil {
				err = fmt.Errorf("fetchFile: %w", err)
				return
			}
		}
	} else {
		out, err = os.CreateTemp("", "libget")
		if err != nil {
			err = fmt.Errorf("fetchFile: %w", err)
			return
		}
		tmpOut = out.Name()
	}
	defer out.Close()

	resp, timer, err := httpReq(url, "GET", start)
	if err != nil {
		err = fmt.Errorf("fetchFile: %w", err)
		return
	}
	defer resp.Body.Close()

	for {
		timer.Reset(time.Duration(timeout) * time.Second)
		rcvd, e := io.CopyN(out, resp.Body, int64(chunkSize)*1024*1024)
		size += rcvd
		if e == nil {
			fmt.Print("+")
			continue
		}
		if e == io.EOF {
			break
		} else {
			err = fmt.Errorf("fetchFile: %w", e)
			break
		}
	}
	return
}

func processFile(tmp, file string) error {

	var err error

	ext := filepath.Ext(file)
	switch ext {
	case ".zip":
		if err = checkZip(tmp); err == nil {
			err = copyFileContents(tmp, file)
		}
	case ".gz":
		err = unGzipFile(tmp, strings.TrimSuffix(file, ext))
	default:
		err = errors.New("processFile: Unknown file extension")
	}

	if err != nil {
		return fmt.Errorf("processFile: %w", err)
	}

	return nil
}

func getFiles(files []string, url, dest string) error {

	var err error

	for _, f := range files {

		var start int64
		var tmp string
		var withRanges bool

		for ni := 1; ni <= retry; ni++ {
			if ranges && start > 0 {
				for nj := 1; nj <= retry; nj++ {
					withRanges, err = acceptsRanges(joinUrl(url, f))
					if err == nil {
						if verbose {
							if withRanges {
								fmt.Print("   ...Server supports ranges")
							} else {
								fmt.Print("   ...Server does not support ranges")
							}
						}
						break
					}
				}
			}
			if !withRanges {
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

		if err != nil {
			return fmt.Errorf("unable to download files: %w", err)
		}

		if err = processFile(tmp, filepath.Join(dest, f)); err != nil {
			break
		}
	}
	return err
}

func main() {

	var code int

	log.SetPrefix("\n*** ")

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "\nTool to download library updates\nVersion %s (%s) %s\n\n", misc.GetVersion(), runtime.Version(), LastGitCommit)
		fmt.Fprintf(os.Stderr, "Usage: %s [options]\n\n", os.Args[0])
		flag.PrintDefaults()
	}
	if flag.Parse(); flag.NFlag() == 0 {
		flag.Usage()
		os.Exit(code)
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
	if len(destSQL) == 0 {
		destSQL = libName + "_" + time.Now().UTC().Format("20060102_150405")
	}
	if destSQL, err = filepath.Abs(destSQL); err != nil {
		log.Fatal(err)
	}

	if lastBook, err = getLastBook(dest); err != nil {
		log.Fatal(err)
	}
	if verbose {
		fmt.Printf("Last book id: %d\n", lastBook)
	}

	if len(lib.Proxy) > 0 {
		if proxyURL, err = url.Parse(lib.Proxy); err != nil {
			log.Fatal(err)
		}
		if verbose {
			// Hide user:password if any
			u := &url.URL{Scheme: proxyURL.Scheme, Host: proxyURL.Host}
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

		// We may want to "merge" updates
		code = 2

		fmt.Printf("\nProcessed %d new archive(s)\n", newArchives)
	} else {
		fmt.Printf("\nProcessed no new archive(s)\n")
	}
	if noSQL {
		fmt.Println("\nDone...")
		os.Exit(code)
	}

	if links, err = getSQLLinks(lib.UrlSQL, lib.PatternSQL); err != nil {
		log.Fatal(err)
	}
	if err = os.MkdirAll(destSQL, 0777); err != nil {
		log.Fatal(err)
	}
	if err = getFiles(links, lib.UrlSQL, destSQL); err != nil {
		log.Fatal(err)
	}
	if len(links) > 0 {
		fmt.Printf("\nProcessed %d SQL table(s)\n", len(links))
	} else {
		fmt.Println()
	}
	fmt.Println("\nDone...")
	os.Exit(code)
}
