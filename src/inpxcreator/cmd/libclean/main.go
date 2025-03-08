package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"time"

	"inpxcreator/internal/zip"
	"inpxcreator/misc"
)

var (
	verbose, newMode, deleteOriginal bool
	dest, LastGitCommit              string
)

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

var updatesFilt filter

type archive struct {
	dir   string
	info  os.FileInfo
	begin int
	end   int
}

type byName []archive

func (a byName) Len() int { return len(a) }

func (a byName) Swap(i, j int) { a[i], a[j] = a[j], a[i] }

func (a byName) Less(i, j int) bool { return a[i].info.Name() < a[j].info.Name() }

func getUpdates(files []archive) ([]archive, error) {

	updates := []archive{}
	var err error

	for _, f := range files {
		if ok, fst, snd, err := updatesFilt.dissectName(f.info.Name()); ok {
			updates = append(updates, archive{begin: fst, end: snd, info: f.info, dir: f.dir})
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

	flag.BoolVar(&verbose, "verbose", false, "print detailed information")
	flag.BoolVar(&deleteOriginal, "delete-original", false, "Do not create backup copies of original files")
	flag.StringVar(&dest, "destination", pwd, "path to archives (separated by \";\", updated archive will be created where original archive is)")
	flag.BoolVar(&newMode, "full", false, "Use full integer length in file names (by default id numbers in file names limited to 6 positions")

	if updatesFilt.re, err = regexp.Compile("(?i)\\s*fb2-([0-9]+)-([0-9]+).zip"); err != nil {
		log.Fatal(err)
	}
}

func main() {

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "\nTool to clean library archives\nVersion %s (%s) %s\n\n", misc.GetVersion(), runtime.Version(), LastGitCommit)
		fmt.Fprintf(os.Stderr, "Presently deletes zero sized book entries correcting file name if necessary\n\n")
		fmt.Fprintf(os.Stderr, "Usage: %s [options]\n\n", os.Args[0])
		flag.PrintDefaults()
	}
	if flag.Parse(); flag.NFlag() == 0 {
		flag.Usage()
		return
	}

	fmt.Printf("\nProcessing archives from \"%s\"\n", dest)

	var err error
	var archives []archive

	paths := strings.Split(dest, ";")

	for _, path := range paths {

		if path, err = filepath.Abs(path); err != nil {
			log.Fatalf("Problem with path: %v", err)
		}

		var files []os.DirEntry
		if files, err = os.ReadDir(path); err != nil {
			log.Fatalf("Unable to read directory: %v", err)
		}

		for _, file := range files {
			if info, err := file.Info(); err != nil {
				log.Fatalf("Unable to get file info: %v", err)
			} else {
				archives = append(archives, archive{info: info, dir: path})
			}
		}
	}
	sort.Sort(byName(archives))

	updates, err := getUpdates(archives)
	if err != nil {
		log.Fatalf("Unable to find archives: %v", err)
	}

	if len(updates) == 0 {
		fmt.Printf("No archives, nothing to do...\n")
		return
	} else if verbose {
		fmt.Printf("\tFound archives: %v\n", len(updates))
		for _, u := range updates {
			fmt.Printf("\t\t--> %s\n", filepath.Join(u.dir, u.info.Name()))
		}
	}

	corrected := 0
	for _, u := range updates {

		start := time.Now()

		name := filepath.Join(u.dir, u.info.Name())
		rc, err := zip.OpenReader(name)
		if err != nil {
			log.Printf("\tError opening update file %s, skipping: %v", filepath.Join(u.dir, u.info.Name()), err)
			continue
		}

		// quick check
		toProcess := false
		for _, file := range rc.File {

			if file.FileInfo().Size() == 0 {
				toProcess = true
				break
			}
			if id := name2id(file.FileInfo().Name()); id <= 0 {
				toProcess = true
				break
			}
		}
		if !toProcess {
			fmt.Printf("\tIgnoring archive: %s (elapsed %s)\n", name, time.Since(start))
			continue
		}

		fmt.Printf("\tProcessing archive: %s\n", name)

		f, err := os.CreateTemp(u.dir, "clean-")
		if err != nil {
			log.Fatalf("Unable to create temp file: %v", err)
		}
		tmpOut := f.Name()

		var (
			firstBook, lastBook int
			updated             bool
		)

		w := zip.NewWriter(f)
		for _, file := range rc.File {

			if file.FileInfo().Size() == 0 {
				updated = true
				fmt.Printf("\t\t--> Wrong book size: %d, skipping: %s\n", file.FileInfo().Size(), file.FileInfo().Name())
				continue
			}
			id := int(0)
			if id = name2id(file.FileInfo().Name()); id <= 0 {
				updated = true
				fmt.Printf("\t\t--> Wrong book name, skipping: %s\n", file.FileInfo().Name())
				continue
			}

			if firstBook == 0 {
				firstBook = id
			}
			lastBook = id

			if err := w.Copy(file); err != nil {
				log.Printf("Error copying from %s (%s): %v", name, file.FileInfo().Name(), err)
			}
		}

		if err := rc.Close(); err != nil {
			log.Fatalf("Closing update file: %v", err)
		}
		if err := w.Close(); err != nil {
			log.Fatalf("Finishing zip file: %v", err)
		}
		if err := f.Close(); err != nil {
			log.Fatalf("Finishing zip file: %v", err)
		}

		if firstBook == 0 || !updated {
			if err := os.Remove(tmpOut); err != nil {
				log.Fatalf("Removing temporary archive: %v", err)
			}
		}
		if updated {
			corrected++
			if deleteOriginal {
				if err := os.Remove(name); err != nil {
					log.Fatalf("Removing original archive: %v", err)
				}
			} else {
				// rename original archive from .zip to .orig.zip. This is a backup copy.
				ext := filepath.Ext(name)
				bkpName := name[:len(name)-len(ext)] + ".orig" + ext
				if err := os.Rename(name, bkpName); err != nil {
					log.Fatalf("Renaming original archive: %v", err)
				}
			}
			format := "fb2-%06d-%06d.zip"
			if newMode {
				format = "fb2-%010d-%010d.zip"
			}
			newName := fmt.Sprintf(format, firstBook, lastBook)
			fmt.Printf("\t\t--> Updating archive: %s (elapsed %s)\n", newName, time.Since(start))
			newName = filepath.Join(u.dir, newName)
			if err := os.Rename(tmpOut, newName); err != nil {
				log.Fatalf("Renaming archive: %v", err)
			}
		} else {
			fmt.Printf("\t\t--> Ignoring archive: %s (elapsed %s)\n", name, time.Since(start))
		}
	}
	fmt.Printf("Corrected %d out of %d archives\n", corrected, len(updates))
}
