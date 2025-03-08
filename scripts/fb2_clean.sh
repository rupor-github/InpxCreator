#!/bin/bash

# Synology task scheduler has a problem running scripts under non-root user

if [ "$2" != "" ]; then
	user_dir=`eval echo "~$2"`
	if [ "${user_dir}" != "" ]; then
    	cd ${user_dir}
	fi
fi

# -----------------------------------------------------------------------------
# Following variables could be changed
# -----------------------------------------------------------------------------

name="flibusta"

# -----------------------------------------------------------------------------
# Main body
# -----------------------------------------------------------------------------

cdate="$(date +%Y%m%d_%H%M%S)"
mydir=$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)
adir="$1/${name}"
glog="${mydir}/${name}_clean_${cdate}.log"

exec 3>&1 4>&2
trap 'exec 2>&4 1>&3' 0 1 2 3 RETURN
exec 1>${glog} 2>&1

${mydir}/libclean \
	--verbose \
    --full \
	--destination "${adir}"

res=$?
if (( ${res} == 1 )); then
	echo "LIBCLEAN error!"
	exit 1
fi
