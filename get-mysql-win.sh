#!/bin/bash

set -e

ver_major=5.6
ver_minor=26

# from_url="http://mysql.mirrors.pair.com/Downloads"
from_url="http://dev.mysql.com/get/Downloads"

case "$MSYSTEM" in
	MINGW32)
		arch=win32
	;;
	MINGW64)
		arch=winx64
	;;
	*)
		echo "Unsupported environment!"
		exit 1
	;;
esac

echo "Processing ${arch}..."
if [ ! -f mysql-${ver_major}.${ver_minor}-${arch}.zip ]; then
	curl -L -O ${from_url}/MySQL-${ver_major}/mysql-${ver_major}.${ver_minor}-${arch}.zip
else
	echo "Have mysql-${ver_major}.${ver_minor}-${arch}.zip locally."
fi

[ -d mysql-${ver_major}.${ver_minor}-${arch} ] && rm -rf mysql-${ver_major}.${ver_minor}-${arch}
7z x mysql-${ver_major}.${ver_minor}-${arch}.zip \*/include \*/lib/libmysqld.dll \*/share/english/errmsg.sys

(
	cd mysql-${ver_major}.${ver_minor}-${arch}
	cd lib
	for dll in *.dll; do 
		echo "Creating MINGW import library for ${dll}"
		gendef -a ${dll}
		dlltool -d ${dll%.*}.def -l ${dll}.a -k
		rm ${dll%.*}.def
	done
)
