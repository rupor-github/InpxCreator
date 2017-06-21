#!/bin/bash -e

ver_major=10.2
ver_minor=6

from_url="https://downloads.mariadb.org/interstitial"

ARCH_INSTALLS="${ARCH_INSTALLS:-win32 win64 linux}"

for _mingw in ${ARCH_INSTALLS}; do
	case ${_mingw} in
		win32)
			arch=win32
			file=mariadb-${ver_major}.${ver_minor}-${arch}.zip
			prefix=i686
			packagedir=${arch}-packages
		;;
		win64)
			arch=winx64
			file=mariadb-${ver_major}.${ver_minor}-${arch}.zip
			prefix=x86_64
			packagedir=${arch}-packages
		;;
		linux)
			arch=$(uname -m)
			file=mariadb-${ver_major}.${ver_minor}-linux-glibc_214-${arch}.tar.gz
			prefix=
			packagedir=bintar-linux-glibc_214-${arch}
		;;
		*)
			echo "Unsupported environment!"
			exit 1
		;;
	esac

	echo "Processing ${arch}..."
	if [ ! -f ${file} ]; then
		curl -L -O ${from_url}/mariadb-${ver_major}.${ver_minor}/${packagedir}/${file}
	else
		echo "Have ${file} locally."
	fi

	if [ x"${_mingw}" == x"linux" ]; then
		[ -d  mariadb-${ver_major}.${ver_minor}-linux-glibc_214-${arch} ] && rm -rf mariadb-${ver_major}.${ver_minor}-linux-glibc_214-${arch}
		tar -xvf ${file} --wildcards --exclude=\*/mysql-test \*/include \*/lib/libmysqld.a \*/share/english/errmsg.sys
	else
		[ -d mariadb-${ver_major}.${ver_minor}-${arch} ] && rm -rf mariadb-${ver_major}.${ver_minor}-${arch}
		7z x ${file} \*/include \*/lib/libmysqld.dll \*/share/english/errmsg.sys

		(
			export PATH=/usr/${prefix}-w64-mingw32/bin:${PATH}

			cd mariadb-${ver_major}.${ver_minor}-${arch}
			cd lib
			for dll in *.dll; do
				echo "Creating MINGW import library for ${dll}"
				gendef -a ${dll}
				dlltool -d ${dll%.*}.def -l ${dll}.a -k
				rm ${dll%.*}.def
			done
		)
	fi

done

exit 0
