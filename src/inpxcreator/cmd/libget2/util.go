package main

import (
	"archive/zip"
	"compress/gzip"
	"io"
	"os"
	"regexp"
	"strconv"
	"strings"

	"github.com/pkg/errors"
)

func dissect(re *regexp.Regexp, name string) (bool, int, int, error) {

	var fst, snd int
	var err error
	var ok bool

	m := re.FindStringSubmatch(name)
	if ok = (m != nil); ok {
		if fst, err = strconv.Atoi(m[1]); err != nil {
			err = errors.Wrapf(err, "dissecting %s", name)
		} else if snd, err = strconv.Atoi(m[2]); err != nil {
			err = errors.Wrapf(err, "dissecting %s", name)
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
		return errors.Wrap(err, "checkZip")
	}
	defer r.Close()

	for _, f := range r.File {
		rc, err := f.Open()
		if err != nil {
			return errors.Wrap(err, "checkZip")
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
		return errors.Wrap(err, "copyFileContents")
	}
	defer out.Close()

	if _, err = io.Copy(out, in); err != nil {
		return errors.Wrap(err, "copyFileContents")
	}
	return out.Sync()
}

func unGzipFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return errors.Wrap(err, "unGzipFile")
	}
	defer in.Close()

	r, err := gzip.NewReader(in)
	if err != nil {
		return errors.Wrap(err, "unGzipFile")
	}
	defer r.Close()

	out, err := os.Create(dst)
	if err != nil {
		return errors.Wrap(err, "unGzipFile")
	}
	defer out.Close()

	if _, err := io.Copy(out, r); err != nil {
		return errors.Wrap(err, "unGzipFile")
	}

	return nil
}
