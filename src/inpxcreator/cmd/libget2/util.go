package main

import (
	"archive/zip"
	"compress/gzip"
	"fmt"
	"io"
	"os"
	"regexp"
	"strconv"
	"strings"
)

func dissect(re *regexp.Regexp, name string) (bool, int, int, error) {

	var fst, snd int
	var err error
	var ok bool

	m := re.FindStringSubmatch(name)
	if ok = (m != nil); ok {
		if fst, err = strconv.Atoi(m[1]); err != nil {
			err = fmt.Errorf("dissecting %s: %w", name, err)
		} else if snd, err = strconv.Atoi(m[2]); err != nil {
			err = fmt.Errorf("dissecting %s: %w", name, err)
		}
	}
	return ok, fst, snd, err
}

func joinUrl(url, file string) string {
	if strings.HasSuffix(url, "/") {
		return url + file
	} else {
		return url + "/" + file
	}
}

func checkZip(file string) error {
	r, err := zip.OpenReader(file)
	if err != nil {
		return fmt.Errorf("checkZip: %w", err)
	}
	defer r.Close()

	for _, f := range r.File {
		rc, err := f.Open()
		if err != nil {
			return fmt.Errorf("checkZip: %w", err)
		}
		rc.Close()
	}
	return nil
}

func copyFileContents(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()

	out, err := os.Create(dst)
	if err != nil {
		return fmt.Errorf("copyFileContents: %w", err)
	}
	defer out.Close()

	if _, err = io.Copy(out, in); err != nil {
		return fmt.Errorf("copyFileContents: %w", err)
	}
	return out.Sync()
}

func unGzipFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return fmt.Errorf("unGzipFile: %w", err)
	}
	defer in.Close()

	r, err := gzip.NewReader(in)
	if err != nil {
		return fmt.Errorf("unGzipFile: %w", err)
	}
	defer r.Close()

	out, err := os.Create(dst)
	if err != nil {
		return fmt.Errorf("unGzipFile: %w", err)
	}
	defer out.Close()

	if _, err := io.Copy(out, r); err != nil {
		return fmt.Errorf("unGzipFile: %w", err)
	}
	return nil
}
