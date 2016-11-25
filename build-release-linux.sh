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

_arch=$(uname -m)
_os=$(uname)
_system=${_os,,}
_dist=bin_${_system}_${_arch}

print_msg1 "Building ${_os} ${_arch} release"
(
	[ -d Release_${_system}_${_arch} ] && rm Release_${_system}_${_arch}
	mkdir -p Release_${_system}_${_arch}

	(
		cd Release_${_system}_${_arch}
		cmake -DCMAKE_BUILD_TYPE=Release ..
		make install
	)

	(
		tar --directory ${_dist} --create --gzip --file lib2inpx_${_system}_${_arch}.tar.gz .
	)
)

exit 0

