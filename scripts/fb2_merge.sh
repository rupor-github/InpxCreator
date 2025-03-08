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
glog="${mydir}/${name}_merge_${cdate}.log"

exec 3>&1 4>&2
trap 'exec 2>&4 1>&3' 0 1 2 3 RETURN
exec 1>${glog} 2>&1

# Clean old database directories - we have at least one good download
find $1 -maxdepth 1 -type d -name "flibusta_*" | sort -nr | tail -n +6 | xargs -r -I {} rm -rf {}/

${mydir}/libmerge \
	--verbose \
    --full \
	--keep-updates \
	--destination "${adir};${udir}"

res=$?
if (( ${res} == 1 )); then
	echo "LIBMERGE error!"
	exit 1
fi

# Clean updates leaving last ones so libget2 would not download unnesessary updates next time
find ${udir} -type f | sort -nr | tail -n +11 | xargs -r -I {} rm -r {}
