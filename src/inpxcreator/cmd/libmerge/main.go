package main

import (
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strconv"
	"strings"

	"inpxcreator/internal/zip"
	"inpxcreator/misc"
)

var size int
var sizeBytes int64
var verbose bool
var keepUpdates bool
var dest string

// LastGitCommit is used during build to inject git sha
var LastGitCommit string

func name2id(name string) int {
	base := strings.TrimSuffix(name, filepath.Ext(name))
	if s, err := strconv.Atoi(base); err == nil {
		return s
	}
	return -1
}

type filter struct {
	re *regexp.Regexp
}

func (f *filter) dissectName(name string) (bool, int, int, error) {

	var fst, snd int
	var err error
	var ok bool

	m := f.re.FindStringSubmatch(name)
	if ok = (m != nil); ok {
		if fst, err = strconv.Atoi(m[1]); err != nil {
			err = fmt.Errorf("dissecting %s: %w", name, err)
		} else if snd, err = strconv.Atoi(m[2]); err != nil {
			err = fmt.Errorf("dissecting %s: %w", name, err)
		}
	}
	return ok, fst, snd, err
}

var archivesFilt filter
var mergeFilt filter
var updatesFilt filter

type archive struct {
	dir   string
	info  os.FileInfo
	begin int
	end   int
}

type byName []archive

func (a byName) Len() int           { return len(a) }
func (a byName) Swap(i, j int)      { a[i], a[j] = a[j], a[i] }
func (a byName) Less(i, j int) bool { return a[i].info.Name() < a[j].info.Name() }

func getLastArchive(files []archive) (archive, error) {

	var a archive

	for _, f := range files {
		if ok, fst, snd, err := archivesFilt.dissectName(f.info.Name()); ok {
			if a.end < snd {
				a.begin = fst
				a.end = snd
				a.info = f.info
				a.dir = f.dir
			}
		} else if err != nil {
			return a, fmt.Errorf("getLastArchive: %w", err)
		}
	}
	return a, nil
}

func getMergeArchive(files []archive) (archive, error) {

	var a archive
	var count int

	for _, f := range files {
		if ok, fst, snd, err := mergeFilt.dissectName(f.info.Name()); ok {
			a.begin = fst
			a.end = snd
			a.info = f.info
			a.dir = f.dir
			count++
		} else if err != nil {
			return a, fmt.Errorf("getMergeArchive: %w", err)
		}
	}
	if count <= 1 {
		return a, nil
	}
	return a, errors.New("getMergeArchive: there could only be single merge archive")
}

func getUpdates(files []archive, last int) ([]archive, error) {

	updates := []archive{}
	var err error

	for _, f := range files {
		if ok, fst, snd, err := updatesFilt.dissectName(f.info.Name()); ok {
			if last < fst {
				updates = append(updates, archive{begin: fst, end: snd, info: f.info, dir: f.dir})
			}
		} else if err != nil {
			return updates, fmt.Errorf("getUpdates: %w", err)
		}
	}
	return updates, err
}

func init() {
	pwd, err := os.Getwd()
	if err != nil {
		log.Fatal(err)
	}

	flag.IntVar(&size, "size", 2000, "Individual archive size in MB (metric, not binary)")
	flag.BoolVar(&verbose, "verbose", false, "print detailed information")
	flag.BoolVar(&keepUpdates, "keep-updates", false, "Keep merged updates")
	flag.StringVar(&dest, "destination", pwd, "path to archives and updates (separated by \";\", merge archive will be created where \"last\" archive is)")

	if archivesFilt.re, err = regexp.Compile("(?i)\\s*fb2-([0-9]+)-([0-9]+).zip"); err != nil {
		log.Fatal(err)
	}
	if mergeFilt.re, err = regexp.Compile("(?i)\\s*fb2-([0-9]+)-([0-9]+).merging"); err != nil {
		log.Fatal(err)
	}
	if updatesFilt.re, err = regexp.Compile("(?i)\\s*fb2.([0-9]+)-([0-9]+).zip"); err != nil {
		log.Fatal(err)
	}
}

func main() {

	var code int

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "\nTool to merge library updates\nVersion %s (%s) %s\n\n", misc.GetVersion(), runtime.Version(), LastGitCommit)
		fmt.Fprintf(os.Stderr, "Usage: %s [options]\n\n", os.Args[0])
		flag.PrintDefaults()
	}
	if flag.Parse(); flag.NFlag() == 0 {
		flag.Usage()
		os.Exit(code)
	}

	fmt.Printf("\nProcessing archives from \"%s\"\n", dest)

	var sizeBytes int64 = int64(size) * 1000 * 1000
	var err error
	var archives []archive

	paths := strings.Split(dest, ";")

	for _, path := range paths {

		if path, err = filepath.Abs(path); err != nil {
			log.Fatalf("Problem with path: %v", err)
		}

		var files []os.FileInfo
		if files, err = ioutil.ReadDir(path); err != nil {
			log.Fatalf("Unable to read directory: %v", err)
		}

		for _, file := range files {
			archives = append(archives, archive{info: file, dir: path})
		}
	}
	sort.Sort(byName(archives))

	last, err := getLastArchive(archives)
	if err != nil {
		log.Fatalf("Unable to find last archive: %v", err)
	}
	if last.info == nil {
		last.dir = paths[0]
		fmt.Printf("Unable to find last archive, assuming destination directory: %s\n", last.dir)
	} else if verbose {
		fmt.Printf("Last archive: %s - from %d to %d size %d\n", filepath.Join(last.dir, last.info.Name()), last.begin, last.end, last.info.Size())
	}

	merge, err := getMergeArchive(archives)
	if err != nil {
		log.Fatalf("Unable to find merge archive: %v", err)
	}

	if merge.info != nil {
		if (merge.begin < last.begin || merge.begin > last.begin && merge.begin <= last.end) || merge.end < last.end {
			log.Fatalf("Merge archive (%s) and last archive (%s) do not match", filepath.Join(merge.dir, merge.info.Name()), filepath.Join(last.dir, last.info.Name()))
		}
		if verbose {
			fmt.Printf("Merge archive: %s - from %d to %d size %d\n", filepath.Join(merge.dir, merge.info.Name()), merge.begin, merge.end, merge.info.Size())
		}
	} else {
		merge.begin, merge.end = last.begin, last.end
		if verbose {
			var sz int64
			if last.info != nil {
				sz = last.info.Size()
			}
			fmt.Printf("Merge archive: %s - from %d to %d size %d\n", "**new**", merge.begin, merge.end, sz)
		}
	}

	updates, err := getUpdates(archives, merge.end)
	if err != nil {
		log.Fatalf("Unable to find updates: %v", err)
	}

	if len(updates) == 0 {
		fmt.Printf("No updates, nothing to do...\n")
		os.Exit(code)
	} else if verbose {
		fmt.Printf("Found updates: %v\n", len(updates))
		for _, u := range updates {
			fmt.Printf("\t--> %s\n", filepath.Join(u.dir, u.info.Name()))
		}
	}

	var f *os.File
	var w *zip.Writer
	var tmpOut string

	var firstBook, lastBook int
	leftBytes := sizeBytes

	var skipFirst bool

	if merge.info == nil {

		f, err = ioutil.TempFile(last.dir, "merge-")
		if err != nil {
			log.Fatalf("Unable to create temp file: %v", err)
		}
		tmpOut = f.Name()
		w = zip.NewWriter(f)

		if last.info != nil && (sizeBytes-last.info.Size()) > 0 {
			fmt.Printf("Merging last archive, possibly fist time processing: %s\n", filepath.Join(last.dir, last.info.Name()))
			skipFirst = true
			tmp := make([]archive, len(updates)+1, len(updates)+1)
			tmp[0] = last
			copy(tmp[1:], updates[0:])
			updates = tmp
		}

	} else {

		tmpOut = filepath.Join(merge.dir, merge.info.Name())

		f, err = os.OpenFile(tmpOut, os.O_RDWR, 0)
		if err != nil {
			log.Fatalf("Unable to open merge archive: %v", err)
		}
		size, err := f.Seek(0, 2)
		if err != nil {
			log.Fatalf("Seek failed (1): %v", err)
		}
		r, err := zip.NewReader(f, size)
		if err != nil {
			log.Fatalf("Unable to create zip reader: %v", err)
		}
		if _, err := f.Seek(r.AppendOffset(), 0); err != nil {
			log.Fatalf("Seek failed (2): %v", err)
		}
		w = r.Append(f)
		leftBytes -= r.AppendOffset()
		firstBook = merge.begin
	}

	for _, u := range updates {
		name := filepath.Join(u.dir, u.info.Name())
		rc, err := zip.OpenReader(name)
		if err != nil {
			log.Printf("\tError opening update file %s, skipping: %v", filepath.Join(u.dir, u.info.Name()), err)
			continue
		}
		fmt.Printf("\tProcessing update: %s\n", filepath.Join(u.dir, u.info.Name()))
		for _, file := range rc.File {

			if file.FileInfo().Size() == 0 {
				log.Printf("\t\tWrong book size - %d, skipping: \"%s\"\n", file.FileInfo().Size(), file.FileInfo().Name())
				continue
			}
			id := int(0)
			if id = name2id(file.FileInfo().Name()); id <= 0 {
				log.Printf("\t\tWrong book name, skipping: \"%s\"\n", file.FileInfo().Name())
				continue
			}

			if firstBook == 0 {
				firstBook = id
			}
			lastBook = id

			// I know this is wrong, leftBytes could already be negative, but to repeat what libsplit did
			// always copy first file...

			if err := w.Copy(file); err != nil {
				log.Printf("Error copying from %s (%s): %v", name, file.FileInfo().Name(), err)
			} else {

				leftBytes -= int64(file.CompressedSize64)

				if leftBytes <= 0 {
					if err := w.Close(); err != nil {
						log.Fatalf("Finishing zip file: %v", err)
					}
					if err := f.Close(); err != nil {
						log.Fatalf("Finishing zip file: %v", err)
					}
					newName := fmt.Sprintf("fb2-%06d-%06d.zip", firstBook, lastBook)
					fmt.Printf("\t--> Finalizing archive: %s\n", newName)

					newName = filepath.Join(last.dir, newName)
					if err := os.Rename(tmpOut, newName); err != nil {
						log.Fatalf("Renaming archive: %v", err)
					}

					last.info, err = os.Stat(newName)
					if err != nil {
						log.Fatalf("Stat failed: %v", err)
					}
					last.begin = firstBook
					last.end = lastBook
					fmt.Printf("\t--> New last archive: %s\n", newName)

					// We may want to rebuild inpx - have new "last" archive ready
					code = 2

					f, err = ioutil.TempFile(last.dir, "merge-")
					if err != nil {
						log.Fatalf("Unable to create temp file: %v", err)
					}
					tmpOut = f.Name()
					w = zip.NewWriter(f)
					leftBytes = sizeBytes
					firstBook = 0
				}
			}
		}
		if err := rc.Close(); err != nil {
			log.Fatalf("Closing update file: %v", err)
		}
	}

	if err := w.Close(); err != nil {
		log.Fatalf("Finishing zip file: %v", err)
	}
	if err := f.Close(); err != nil {
		log.Fatalf("Finishing zip file: %v", err)
	}

	if firstBook == 0 {

		if err := os.Remove(tmpOut); err != nil {
			log.Fatalf("Removing empty archive: %v", err)
		}

	} else {

		newName := fmt.Sprintf("fb2-%06d-%06d.merging", firstBook, lastBook)
		fmt.Printf("\t--> Finalizing archive: %s\n", newName)

		newName = filepath.Join(last.dir, newName)
		if err := os.Rename(tmpOut, newName); err != nil {
			log.Fatalf("Renaming archive: %v", err)
		}
	}

	if !keepUpdates {
		for i, u := range updates {
			if i == 0 && skipFirst {
				continue
			}
			name := filepath.Join(u.dir, u.info.Name())
			if verbose {
				fmt.Printf("Removing archive: %s\n", filepath.Join(u.dir, u.info.Name()))
			}
			if err := os.Remove(name); err != nil {
				log.Fatalf("Removing archive: %v", err)
			}
		}
	}
	os.Exit(code)
}
