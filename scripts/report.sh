#!/bin/bash
dbp="$1"
if [ "${dbp}" == "" ]; then
	dbp="./"
fi

echo -n "kernel version: "
uname -a
echo -n "umr version: "
umr | head -n 1

echo
echo
for f in `lspci  | grep VGA | grep AMD | awk '{ print $1; }'`; do
	lspci -vv -s $f
done
echo
echo
echo Reporting on DRI devices
for f in /sys/kernel/debug/dri/*; do
	n=`basename $f`
	nt=`echo ${n} | cut -b1-2`
	if [ "${n}" == "${nt}" ]; then
		echo "... Reporting instance [$n]"
		umr -i ${n} -c
		umr -dbp ${dbp} -O verbose -i ${n} -tl /tmp/delme -lb 2>&1
		echo
		echo
		echo "... Test log contents: "
		cat /tmp/delme
		echo "... Test log contents: Done"
		echo
		echo
		if [ -e /sys/class/drm/card${n}/device/ip_discovery ]; then
			tree -f /sys/class/drm/card${n}/device/ip_discovery/
			for t in `find /sys/class/drm/card${n}/device/ip_discovery/ -type f`; do
				cat $t | while read -r line; do
					echo "    ${t}: ${line}"
				done;
			done
		fi
		echo "... Done instance [$n]"
	fi
done
