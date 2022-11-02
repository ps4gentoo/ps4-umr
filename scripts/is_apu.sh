#!/bin/bash

#Search through database files and make sure APU flag matches what it should
for f in `find ../database -name \*.asic`; do
	name=`cat ${f} | head -n1 | awk '{print $1;}'`
	if [ ${name} = "raven1" ]; then name="raven"; fi
	flag=`cat ${f} | head -n1 | awk '{print $6;}'`
	if [ "${flag}" = "0" ]; then
		card="dGPU";
	else
		card="APU";
	fi
	grep -i "CHIP_${name}" ${pk}/amdgpu/amdgpu_drv.c  | grep IS_APU > /dev/null
	a=$?
	if [ "${a}" == "${flag}" ]; then
		echo "${f} is an ${card} and is mislabelled.";
	fi
done;
