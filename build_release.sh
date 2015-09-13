#!/usr/bin/env bash

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
MINGW_INSTALLS="${MINGW_INSTALLS:-mingw64 mingw32}"

for _mingw in ${MINGW_INSTALLS}; do
  if [ ! "${_mingw}" = 'mingw32' -a ! "${_mingw}" = 'mingw64' ]; then
    print_error "Requested mingw installation '${_mingw}', but only 'mingw32' and 'mingw64' are allowed."
    exit 1
  fi
done

# real staff

for _mingw in ${MINGW_INSTALLS}; do
	case ${_mingw} in
		mingw32)
			_arch=32
			_msystem=MINGW32
			_dist=bin
		;;
		mingw64)
			_arch=64
			_msystem=MINGW64
			_dist=bin64
		;;
	esac

	if [ -f "/${_mingw}/bin/gcc.exe" ]; then
		print_msg1 "Building Win ${_arch} release"
		(
			export MSYSTEM=${_msystem}
			export PATH=/${_mingw}/bin:$(echo $PATH | tr ':' '\n' | awk '$0 != "/opt/bin"' | paste -sd:)

			[ -d Release${_arch} ] && rm -rf Release${_arch}
			mkdir -p Release${_arch}

			(
				cd Release${_arch}
				cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release ..
				make install
			)

			(
				cd ${_dist}
				7z a -r ../lib2inpx-win${_arch} 
			)
		)
	else
		print_warning "You don't have installed mingw-w64 toolchain for architecture ${_arch}."
		print_warning "To install it run: 'pacman -S mingw-w64-${_arch}-toolchain'"
	fi
done

exit 0

