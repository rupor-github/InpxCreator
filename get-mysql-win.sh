#!/bin/bash

set -e

ver_major=5.7
ver_minor=10

# from_url="http://mysql.mirrors.pair.com/Downloads"
from_url="http://dev.mysql.com/get/Downloads"

MINGW_INSTALLS="${MINGW_INSTALLS:-mingw64 mingw32}"

for _mingw in ${MINGW_INSTALLS}; do
  if [ ! "${_mingw}" = 'mingw32' -a ! "${_mingw}" = 'mingw64' ]; then
    echo "Requested mingw installation '${_mingw}', but only 'mingw32' and 'mingw64' are allowed."
    exit 1
  fi
done

for _mingw in ${MINGW_INSTALLS}; do
	case ${_mingw} in
		mingw32)
			arch=win32
			_msystem=MINGW32
		;;
		mingw64)
			arch=winx64
			_msystem=MINGW64
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
		export MSYSTEM=${_msystem}
		export PATH=/${_mingw}/bin:$(echo $PATH | tr ':' '\n' | awk '$0 != "/opt/bin"' | paste -sd:)

		cd mysql-${ver_major}.${ver_minor}-${arch}
		cd lib
		for dll in *.dll; do 
			echo "Creating MINGW import library for ${dll}"
			gendef -a ${dll}
			dlltool -d ${dll%.*}.def -l ${dll}.a -k
			rm ${dll%.*}.def
		done
	)

done

exit 0
