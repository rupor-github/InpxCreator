#!/bin/bash

set -e

case "$MSYSTEM" in
	MINGW32)
		arch=32
		dist=bin
	;;
	MINGW64)
		arch=64
		dist=bin64
	;;
	*)
		echo "Unsupported environment!"
		exit 1
	;;
esac

echo "Building Wind ${arch} release"

[ -d Release${arch} ] && rm -rf Release${arch}
mkdir -p Release${arch}

(
	cd Release${arch}
	cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release ..
	make install
)

(
	cd ${dist}
	7z a -r ../lib2inpx-win${arch} 
)

