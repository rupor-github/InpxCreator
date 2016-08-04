package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"

	"github.com/pkg/errors"
	"libmerge/internal/zip"
)

var size int
var sizeBytes int64
var verbose bool
var dest string

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
			err = errors.Wrapf(err, "dissecting %s", name)
		} else if snd, err = strconv.Atoi(m[2]); err != nil {
			err = errors.Wrapf(err, "dissecting %s", name)
		}
	}
	return ok, fst, snd, err
}

var archivesFilt filter
var mergeFilt filter
var updatesFilt filter

type archive struct {
	info  os.FileInfo
	begin int
	end   int
}

func getLastArchive(files []os.FileInfo) (archive, error) {

	var a archive

	for _, f := range files {
		if ok, fst, snd, err := archivesFilt.dissectName(f.Name()); ok {
			if a.end < snd {
				a.begin = fst
				a.end = snd
				a.info = f
			}
		} else if err != nil {
			return a, errors.Wrap(err, "getLastArchive")
		}
	}
	return a, nil
}

func getMergeArchive(files []os.FileInfo) (archive, error) {

	var a archive
	var count int

	for _, f := range files {
		if ok, fst, snd, err := mergeFilt.dissectName(f.Name()); ok {
			a.begin = fst
			a.end = snd
			a.info = f
			count++
		} else if err != nil {
			return a, errors.Wrap(err, "getMergeArchive")
		}
	}
	if count <= 1 {
		return a, nil
	}
	return a, errors.Errorf("getMergeArchive: there could only be single merge archive")
}

func getUpdates(files []os.FileInfo, last int) ([]archive, error) {

	updates := []archive{}
	var err error

	for _, f := range files {
		if ok, fst, snd, err := updatesFilt.dissectName(f.Name()); ok {
			if last < fst {
				updates = append(updates, archive{begin: fst, end: snd, info: f})
			}
		} else if err != nil {
			return updates, errors.Wrap(err, "getiUpdates")
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
	flag.StringVar(&dest, "destination", pwd, "path to archives")

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

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "\nTool to merge library updates\nVersion %s\n\n", getVersion())
		fmt.Fprintf(os.Stderr, "Usage: %s [options]\n\n", os.Args[0])
		flag.PrintDefaults()
	}
	if flag.Parse(); flag.NFlag() == 0 {
		flag.Usage()
		os.Exit(0)
	}

	fmt.Printf("\nProcessing archives from \"%s\"\n", dest)

	var err error
	if dest, err = filepath.Abs(dest); err != nil {
		log.Fatal(err)
	}

	var sizeBytes int64 = int64(size) * 1000 * 1000

	var files []os.FileInfo
	if files, err = ioutil.ReadDir(dest); err != nil {
		log.Fatal(err)
	}

	last, err := getLastArchive(files)
	if err != nil {
		log.Fatal(err)
	}
	if verbose {
		fmt.Printf("Last archive: %s - from %d to %d size %d\n", last.info.Name(), last.begin, last.end, last.info.Size())
	}

	merge, err := getMergeArchive(files)
	if err != nil {
		log.Fatal(err)
	}

	if merge.info != nil {
		if merge.begin != last.begin {
			log.Fatalf("Merge archive (%s) and last archive (%s) do not match", merge.info.Name(), last.info.Name())
		}
		if verbose {
			fmt.Printf("Merge archive: %s - from %d to %d size %d\n", merge.info.Name(), merge.begin, merge.end, merge.info.Size())
		}
	} else {
		merge.begin, merge.end = last.begin, last.end
		if verbose {
			fmt.Printf("Merge archive: %s - from %d to %d size %d\n", "**new**", merge.begin, merge.end, last.info.Size())
		}
	}

	updates, err := getUpdates(files, merge.end)
	if err != nil {
		log.Fatal(err)
	}

	if len(updates) == 0 {
		fmt.Printf("No updates, nothing to do...")
		os.Exit(0)
	} else if verbose {
		fmt.Printf("Found updates: %v\n", len(updates))
		for _, u := range updates {
			fmt.Printf("\t--> %s\n", u.info.Name())
		}
	}

	var f *os.File
	var w *zip.Writer
	var tmpOut string

	var firstBook, lastBook int
	leftBytes := sizeBytes

	var skipFirst bool

	if merge.info == nil {

		f, err = ioutil.TempFile(dest, "merge-")
		if err != nil {
			log.Fatal(err)
		}
		tmpOut = f.Name()
		w = zip.NewWriter(f)

		if (sizeBytes - last.info.Size()) > 0 {
			skipFirst = true
			tmp := make([]archive, len(updates)+1, len(updates)+1)
			tmp[0] = last
			copy(tmp[1:], updates[0:])
			updates = tmp
		}

	} else {

		tmpOut = filepath.Join(dest, merge.info.Name())

		f, err = os.OpenFile(tmpOut, os.O_RDWR, 0)
		if err != nil {
			log.Fatal(err)
		}
		size, err := f.Seek(0, 2)
		if err != nil {
			log.Fatal(err)
		}
		r, err := zip.NewReader(f, size)
		if err != nil {
			log.Fatal(err)
		}
		if _, err := f.Seek(r.AppendOffset(), 0); err != nil {
			log.Fatal(err)
		}
		w = r.Append(f)
		leftBytes -= r.AppendOffset()
		firstBook = merge.begin
	}

	for _, u := range updates {
		name := filepath.Join(dest, u.info.Name())
		rc, err := zip.OpenReader(name)
		if err != nil {
			log.Print("\tError opening update file %s, skipping: %v", u.info.Name(), err)
			continue
		}
		fmt.Printf("\tProcessing update: %s\n", u.info.Name())
		for _, file := range rc.File {
			if id := name2id(file.Name); id > 0 {

				if firstBook == 0 {
					firstBook = id
				}
				lastBook = id

				// I know this is wrong, leftBytes could already be negative, but to repeat what libsplit does
				// always copy first file...

				if err := w.Copy(file); err != nil {
					log.Printf("Error copying from %s (%s): %v", name, file.Name, err)
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

						newName = filepath.Join(dest, newName)
						if err := os.Rename(tmpOut, newName); err != nil {
							log.Fatalf("Renaming archive: %v", err)
						}

						fmt.Printf("\t--> Removing old last archive: %s\n", last.info.Name())
						if err := os.Remove(filepath.Join(dest, last.info.Name())); err != nil {
							log.Fatalf("Removing old last archive: %v", err)
						}

						f, err = ioutil.TempFile(dest, "merge-")
						if err != nil {
							log.Fatal(err)
						}
						tmpOut = f.Name()
						w = zip.NewWriter(f)
						leftBytes = sizeBytes
						firstBook = 0
					}
				}
			} else {
				log.Printf("\t\tWrong book name, skipping: \"%s\"\n", file.Name)
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

		newName = filepath.Join(dest, newName)
		if err := os.Rename(tmpOut, newName); err != nil {
			log.Fatalf("Renaming archive: %v", err)
		}
	}

	for i, u := range updates {
		if i == 0 && skipFirst {
			continue
		}
		name := filepath.Join(dest, u.info.Name())
		if verbose {
			fmt.Printf("Removing archive: %s\n", u.info.Name())
		}
		if err := os.Remove(name); err != nil {
			log.Fatalf("Removing archive: %v", err)
		}
	}
}
