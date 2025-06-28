#!/bin/bash
if [ "$1" == "" ]; then
	echo "$0: Possible commands"
	echo "$0 ip1_offset.h ip1_sh_mask.h ip2_offset.h ip2_sh_mask.h [diff util]"
	echo "$0 ip1_ ip2_offset.h ip2_sh_mask.h [diff util]"
	echo "$0 ip1_offset.h ip1_sh_mask.h ip2_ [diff util]"
	echo "$0 ip1_ ip2_ [diff util]"
	exit 0
fi

make -C ../comp 


echo "$1" | grep [.]h >/dev/null
if [ $? == 0 ]; then
	echo "$1" | grep "offset" >/dev/null
	if [ $? == 0 ]; then 
		ip1_offset="$1"
		ip1_sh_mask="$2"
	else
		ip1_offset="$2"
		ip1_sh_mask="$1"
	fi
	shift
	shift
else
	ip1_offset="$1"offset.h
	ip1_sh_mask="$1"sh_mask.h
	shift
fi

echo "$1" | grep [.]h >/dev/null
if [ $? == 0 ]; then
	echo "$1" | grep "offset" >/dev/null
	if [ $? == 0 ]; then 
		ip2_offset="$1"
		ip2_sh_mask="$2"
	else
		ip2_offset="$2"
		ip2_sh_mask="$1"
	fi
	shift
	shift
else
	ip2_offset="$1"offset.h
	ip2_sh_mask="$1"sh_mask.h
	shift
fi

../comp/compiler "${ip1_offset}" "${ip1_sh_mask}" > /tmp/umr_diff_ip_1.tmp
../comp/compiler "${ip2_offset}" "${ip2_sh_mask}" > /tmp/umr_diff_ip_2.tmp

if [ "$@" == "" ]; then
	diff -ur -d /tmp/umr_diff_ip_1.tmp /tmp/umr_diff_ip_2.tmp;
else
	$@ /tmp/umr_diff_ip_1.tmp /tmp/umr_diff_ip_2.tmp;
fi

rm -f /tmp/umr_diff*
make -C ../comp clean
