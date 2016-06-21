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
var inplace bool
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
			err = errors.Wrap(err, fmt.Sprintf("dissecting %s", name))
		} else if snd, err = strconv.Atoi(m[2]); err != nil {
			err = errors.Wrap(err, fmt.Sprintf("dissecting %s", name))
		}
	}
	return ok, fst, snd, err
}

var archivesFilt filter
var updatesFilt filter

type archive struct {
	info  os.FileInfo
	begin int
	end   int
}

func getLastArchive(files []os.FileInfo) (archive, error) {

	var a archive
	var err error

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
	return a, err
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

	flag.IntVar(&size, "size", 2000, "Individual archive size in MB")
	flag.BoolVar(&inplace, "inplace", false, "merge in-place and remove merged updates on completion")
	flag.BoolVar(&verbose, "verbose", false, "print detailed information")
	flag.StringVar(&dest, "destination", pwd, "path to archives")

	if archivesFilt.re, err = regexp.Compile("(?i)\\s*fb2-([0-9]+)-([0-9]+).zip"); err != nil {
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

	var sizeBytes int64 = int64(size) * 1024 * 1024

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

	updates, err := getUpdates(files, last.end)
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

	if !inplace {

		f, err = ioutil.TempFile(dest, "merge-")
		if err != nil {
			log.Fatal(err)
		}
		tmpOut = f.Name()
		w = zip.NewWriter(f)

		if (sizeBytes - last.info.Size()) > 0 {
			tmp := make([]archive, len(updates)+1, len(updates)+1)
			tmp[0] = last
			copy(tmp[1:], updates[0:])
			updates = tmp
		}

	} else {

		tmpOut = filepath.Join(dest, last.info.Name())

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
		firstBook = last.begin
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

				if err := w.Copy(file); err != nil {
					log.Printf("Error copying from %s (%s): %v", name, file.Name, err)
				} else {
					leftBytes -= int64(file.CompressedSize64)

					if leftBytes <= 0 {
						if err := w.Close(); err != nil {
							log.Fatal("Finishing zip file: %v", err)
						}
						if err := f.Close(); err != nil {
							log.Fatal("Finishing zip file: %v", err)
						}
						newName := fmt.Sprintf("fb2-%06d-%06d.zip", firstBook, lastBook)
						fmt.Printf("\t--> Finalizing archive: %s\n", newName)

						newName = filepath.Join(dest, newName)
						if err := os.Rename(tmpOut, newName); err != nil {
							log.Fatal("Renaming archive: %v", err)
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
			}
		}
		if err := rc.Close(); err != nil {
			log.Fatal("Closing update file: %v", err)
		}
	}

	if err := w.Close(); err != nil {
		log.Fatal("Finishing zip file: %v", err)
	}
	if err := f.Close(); err != nil {
		log.Fatal("Finishing zip file: %v", err)
	}
	newName := fmt.Sprintf("fb2-%06d-%06d.zip", firstBook, lastBook)
	fmt.Printf("\t--> Finalizing archive: %s\n", newName)

	newName = filepath.Join(dest, newName)
	if err := os.Rename(tmpOut, newName); err != nil {
		log.Fatal("Renaming archive: %v", err)
	}

	if !inplace {
		for _, u := range updates {
			name := filepath.Join(dest, u.info.Name())
			if verbose {
				fmt.Printf("Renaming archive: %s\n", u.info.Name())
			}
			if err := os.Rename(name, name+"_merged"); err != nil {
				log.Fatal("Renaming archive: %v", err)
			}
		}
	} else {
		for _, u := range updates {
			name := filepath.Join(dest, u.info.Name())
			if verbose {
				fmt.Printf("Removing archive: %s\n", u.info.Name())
			}
			if err := os.Remove(name); err != nil {
				log.Fatal("Removing archive: %v", err)
			}
		}
	}
}
