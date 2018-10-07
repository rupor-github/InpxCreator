#!/bin/bash -e

ver_major=5.7
ver_minor=17

# from_url="http://mysql.mirrors.pair.com/Downloads"
from_url="http://dev.mysql.com/get/Downloads"

ARCH_INSTALLS="${ARCH_INSTALLS:-win32 win64 linux}"

for _mingw in ${ARCH_INSTALLS}; do
	case ${_mingw} in
		win32)
			arch=win32
			file=mysql-${ver_major}.${ver_minor}-${arch}.zip
			prefix=i686
		;;
		win64)
			arch=winx64
			file=mysql-${ver_major}.${ver_minor}-${arch}.zip
			prefix=x86_64
		;;
		linux)
			arch=$(uname -m)
			file=mysql-${ver_major}.${ver_minor}-linux-glibc2.5-${arch}.tar.gz
			prefix=
		;;
		*)
			echo "Unsupported environment!"
			exit 1
		;;
	esac

	echo "Processing ${arch}..."
	if [ ! -f ${file} ]; then
		curl -L -O ${from_url}/MySQL-${ver_major}/${file}
	else
		echo "Have ${file} locally."
	fi

	if [ x"${_mingw}" == x"linux" ]; then
		[ -d mysql-${ver_major}.${ver_minor}-linux-glibc2.5-${arch} ] && rm -rf mysql-${ver_major}.${ver_minor}-linux-glibc2.5-${arch}
		tar -xvf ${file} --wildcards \*/include \*/lib/libmysqld.a \*/share/english/errmsg.sys
	else
		[ -d mysql-${ver_major}.${ver_minor}-${arch} ] && rm -rf mysql-${ver_major}.${ver_minor}-${arch}
		7z x ${file} \*/include \*/lib/libmysqld.dll \*/share/english/errmsg.sys

		(
			export PATH=/usr/${prefix}-w64-mingw32/bin:${PATH}

			cd mysql-${ver_major}.${ver_minor}-${arch}
			cd lib
			for dll in *.dll; do
				echo "Creating MINGW import library for ${dll}"
				gendef -a ${dll}
				dlltool -d ${dll%.*}.def -l ${dll}.a -k
				rm ${dll%.*}.def
			done
		)
	fi
        rm ${file}

done

exit 0
