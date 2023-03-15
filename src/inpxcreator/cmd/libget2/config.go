package main

import (
	"encoding/json"
	"os"
)

type Library struct {
	Name       string `json:"name"`
	NameLib    string `json:"nameLib"`
	Pattern    string `json:"pattern"`
	PatternSQL string `json:"patternSQL"`
	URL        string `json:"url"`
	UrlSQL     string `json:"urlSQL"`
	Proxy      string `json:"proxy"`
}

type Config struct {
	Libraries []Library `json:"libraries"`
}

var defconfig = `{
        "libraries":
        [
                {
                        "name":         "flibusta",
                        "pattern":      "<a\\s+href=\"(f(?:\\.fb2)*\\.[0-9]+-[0-9]+\\.zip)\">",
                        "patternSQL":   "<a\\s+href=\"(lib\\.lib\\w+\\.sql\\.gz)\">",
                        "url":          "http:\/\/flibusta.net\/daily",
                        "urlSQL":       "http:\/\/flibusta.net\/sql"
                },
                {
                        "name":         "librusec",
                        "pattern":      "<a\\s+href=\"([0-9]+-[0-9]+\\.zip)\">",
                        "patternSQL":   "<a\\s+href=\"(lib\\w+\\.sql\\.gz)\">",
                        "url":          "http:\/\/lib.rus.ec\/all\/",
                        "urlSQL":       "http:\/\/lib.rus.ec\/sql\/"
                }
        ]
}
`

func (c *Config) Find(name string) (lib *Library) {
	for _, l := range c.Libraries {
		if l.Name == name {
			return &l
		}
	}
	return nil
}

func ReadLibraries(path string) (*Config, error) {
	f, err := os.Open(path)
	if err != nil && os.IsNotExist(err) {
		err = os.WriteFile(path, []byte(defconfig), os.ModePerm)
		if err != nil {
			return nil, err
		}
		f, err = os.Open(path)
		if err != nil {
			return nil, err
		}
	}
	c := new(Config)
	return c, json.NewDecoder(f).Decode(c)
}
