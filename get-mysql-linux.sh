#!/bin/bash

set -e

ver_major=5.7
ver_minor=17

# from_url="http://mysql.mirrors.pair.com/Downloads"
from_url="http://dev.mysql.com/get/Downloads"

arch=$(uname -m)
file=mysql-${ver_major}.${ver_minor}-linux-glibc2.5-${arch}.tar.gz

echo "Processing ${arch}..."
if [ ! -f ${file} ]; then
	curl -L -O ${from_url}/MySQL-${ver_major}/${file}
else
	echo "Have ${file} locally."
fi

[ -d mysql-${ver_major}.${ver_minor}-linux-glibc2.5-${arch} ] && rm -rf mysql-${ver_major}.${ver_minor}-linux-glibc2.5-${arch}
tar -xvf ${file} --wildcards \*/include \*/lib/libmysqld.a \*/share/english/errmsg.sys

exit 0
