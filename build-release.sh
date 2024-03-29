#!/bin/bash -e

# Standard preambule
plain() {
  local mesg=$1; shift
  printf "    ${mesg}\n" "$@" >&2
}

print_warning() {
  local mesg=$1; shift
  printf "${YELLOW}=> WARNING: ${mesg}${ALL_OFF}\n" "$@" >&2
}

print_msg1() {
  local mesg=$1; shift
  printf "${GREEN}==> ${mesg}${ALL_OFF}\n" "$@" >&2
}

print_msg2() {
  local mesg=$1; shift
  printf "${BLUE}  -> ${mesg}${ALL_OFF}\n" "$@" >&2
}

print_error() {
  local mesg=$1; shift
  printf "${RED}==> ERROR: ${mesg}${ALL_OFF}\n" "$@" >&2
}

ALL_OFF='[00m'
BLUE='[38;5;04m'
GREEN='[38;5;02m'
RED='[38;5;01m'
YELLOW='[38;5;03m'

readonly ALL_OFF BOLD BLUE GREEN RED YELLOW
ARCH_INSTALLS="${ARCH_INSTALLS:-win64 linux}"

if [ ! -d ${HOME}/result ]; then
	print_error "directory for the result is not available. Exiting..."
	exit 1
fi

if not command -v cmake >/dev/null 2>&1; then
	print_error "No cmake found - please, install"
	exit 1
fi

for _mingw in ${ARCH_INSTALLS}; do
	
	case ${_mingw} in
		win32)
			_arch=win32
			_msystem=MINGW32
			_dist=bin_win32
			_gcc=i686-w64-mingw32-gcc
		;;
		win64)
			_arch=win64
			_msystem=MINGW64
			_dist=bin_win64
			_gcc=x86_64-w64-mingw32-gcc
		;;
		linux)
			_glibc=`ldd --version | head -n 1 | awk '{ print $5; }'`
			_msystem=
			_os=$(uname)
			_arch=${_os,,}_$(uname -m)
			_dist=bin_${_arch}
			_gcc=gcc
		;;
	esac

	if command -v ${_gcc} >/dev/null 2>&1; then
		
		print_msg1 "Building ${_arch} release"

		[ -d ${_dist} ] && rm -rf ${_dist}

		[ -d Release_${_arch} ] && rm -rf Release_${_arch}
		mkdir -p Release_${_arch}

		(
			cd  Release_${_arch}

			if [ -z ${_msystem} ]; then
				cmake -DCMAKE_BUILD_TYPE=Release ..
				make install
			else
				MSYSTEM=${_msystem} cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/${_arch}.toolchain ..
				MSYSTEM=${_msystem} make install
			fi
		)

		(
			if [ -z ${_msystem} ]; then
				[ -f ${HOME}/result/lib2inpx-${_arch}-glibc_${_glibc}.tar.xz ] && rm ${HOME}/result/lib2inpx-${_arch}-glibc_${_glibc}.tar.xz

				tar --directory ${_dist} --create --xz --file ${HOME}/result/lib2inpx-${_arch}-glibc_${_glibc}.tar.xz .
			else
				[ -f ${HOME}/result/lib2inpx-${_arch}.7z ] && rm ${HOME}/result/lib2inpx-${_arch}.7z

				cd ${_dist}
				7z a -r ${HOME}/result/lib2inpx-${_arch}
			fi
		)
	else
		print_warning "You don't have installed mingw-w64 toolchain for ${_mingw}."
	fi
done

exit 0

