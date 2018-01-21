#!/bin/bash -e

echo "10.1.26 seems to be the last version supporting libmysqld.a"

ver_major=10.1
ver_minor=26

declare -A checkmap
from_url="https://downloads.mariadb.com/MariaDB"

ARCH_INSTALLS="${ARCH_INSTALLS:-win32 win64 linux}"

for _mingw in ${ARCH_INSTALLS}; do
	case ${_mingw} in
		win32)
			arch=win32
			file=mariadb-${ver_major}.${ver_minor}-${arch}.zip
			prefix=i686
			packagedir=${arch}-packages
			checkmap[${arch}]="08b546950f448763459ac66f8f81e0097c8cd4378a3e0075b6e50275682bd7ed"
		;;
		win64)
			arch=winx64
			file=mariadb-${ver_major}.${ver_minor}-${arch}.zip
			prefix=x86_64
			packagedir=${arch}-packages
			checkmap[${arch}]="ece635977ec4f1d1c192fb156cef02229cff1925bd76cab488a48e63d8cf97fc"
		;;
		linux)
			arch=$(uname -m)
			file=mariadb-${ver_major}.${ver_minor}-linux-glibc_214-${arch}.tar.gz
			prefix=
			packagedir=bintar-linux-glibc_214-${arch}
			checkmap[${arch}]="b993c521f283bacec18760ae72b7d768c3e032007a244e0a8b13032c0278508f"
		;;
		*)
			echo "Unsupported environment!"
			exit 1
		;;
	esac

	echo "Processing ${arch}..."
	if [ ! -f ${file} ]; then
		echo
		echo "--> ${from_url}/mariadb-${ver_major}.${ver_minor}/${packagedir}/${file}"
		echo
		curl -L -O ${from_url}/mariadb-${ver_major}.${ver_minor}/${packagedir}/${file}
	else
		echo "Have ${file} locally."
	fi
	sum=`sha256sum ${file} | awk '{ print $1; }'` 
	if [ ${checkmap[${arch}]} != ${sum} ]; then
		echo "Bad checksum for ${file}: ${checkmap[${arch}]} != ${sum}"
		exit 1
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
				echo "Creating Windows export library for ${dll}"
				gendef -a ${dll}
				${prefix}-w64-mingw32-dlltool -d ${dll%.*}.def -l ${dll}.a -k
				rm ${dll%.*}.def
			done
		)
	fi

done

exit 0
