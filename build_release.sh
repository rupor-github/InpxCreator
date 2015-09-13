#!/usr/bin/env bash

# Standard preambule
plain() {
  local mesg=$1; shift
  printf "${BOLD}    ${mesg}${ALL_OFF}\n" "$@" >&2
}

print_warning() {
  local mesg=$1; shift
  printf "${YELLOW}=> WARNING:${ALL_OFF}${BOLD} ${mesg}${ALL_OFF}\n" "$@" >&2
}

print_msg1() {
  local mesg=$1; shift
  printf "${GREEN}==> ${ALL_OFF}${BOLD} ${mesg}${ALL_OFF}\n" "$@" >&2
}

print_msg2() {
  local mesg=$1; shift
  printf "${BLUE}  ->${ALL_OFF}${BOLD} ${mesg}${ALL_OFF}\n" "$@" >&2
}

print_error() {
  local mesg=$1; shift
  printf "${RED}==> ERROR:${ALL_OFF}${BOLD} ${mesg}${ALL_OFF}\n" "$@" >&2
}

if /usr/bin/tput setaf 0 &>/dev/null; then
  ALL_OFF="$(/usr/bin/tput sgr0)"
  BOLD="$(/usr/bin/tput bold)"
  BLUE="${BOLD}$(/usr/bin/tput setaf 4)"
  GREEN="${BOLD}$(/usr/bin/tput setaf 2)"
  RED="${BOLD}$(/usr/bin/tput setaf 1)"
  YELLOW="${BOLD}$(/usr/bin/tput setaf 3)"
else
  ALL_OFF="\e[1;0m"
  BOLD="\e[1;1m"
  BLUE="${BOLD}\e[1;34m"
  GREEN="${BOLD}\e[1;32m"
  RED="${BOLD}\e[1;31m"
  YELLOW="${BOLD}\e[1;33m"
fi

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

