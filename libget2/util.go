package main

import (
	"archive/zip"
	"compress/gzip"
	"errors"
	_ "fmt"
	"io"
	"os"
	"strconv"
	"strings"
)

func dissect(elem string) (fst, snd int, err error) {
	if m := reNumbers.FindStringSubmatch(elem); m != nil {
		if fst, err = strconv.Atoi(m[1]); err != nil {
			return
		}
		if snd, err = strconv.Atoi(m[2]); err != nil {
			return
		}
	} else {
		err = errors.New("Unable to parse archive name")
	}
	return
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
		return err
	}
	defer r.Close()

	for _, f := range r.File {
		rc, err := f.Open()
		if err != nil {
			return err
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
		return err
	}
	defer out.Close()

	if _, err = io.Copy(out, in); err != nil {
		return err
	}
	return out.Sync()
}

func unGzipFile(src, dst string) (err error) {
	in, err := os.Open(src)
	if err != nil {
		return
	}
	defer in.Close()

	r, err := gzip.NewReader(in)
	if err != nil {
		return
	}
	defer r.Close()

	out, err := os.Create(dst)
	if err != nil {
		return
	}
	defer out.Close()

	_, err = io.Copy(out, r)

	return
}
