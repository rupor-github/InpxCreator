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
odir="$1/inpx"
udir="$1/upd_${name}"
glog="${mydir}/${name}_inpx_${cdate}.log"
wdir="$(ls -d ${adir}_* | sort -nr | head -n 1)"

exec 3>&1 4>&2
trap 'exec 2>&4 1>&3' 0 1 2 3 RETURN
exec 1>${glog} 2>&1

${mydir}/lib2inpx \
	--db-name=${name} \
	--process=fb2 \
	--read-fb2=all \
	--out-dir=${odir} \
	--quick-fix \
	--clean-when-done \
	--archives=${adir} \
	$wdir

if (( $? != 0 )); then
	echo "Unable to build INPX - $?"
	exit 1
fi
