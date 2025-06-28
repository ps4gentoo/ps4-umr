#!/bin/bash
if [ "${UMRTESTPATH}" == "" ]; then
	UMRTESTPATH="src/test/umrtest"
fi

if [ "${UMRAPPPATH}" == "" ]; then
	UMRAPPPATH="src/app/umr"
fi

if [ "${UMRVECTORSPATH}" == "" ]; then
	UMRVECTORSPATH="test/"
fi

if [ "${1}" != "norebuild" ]; then
	git clean . -dxf
	cmake ${1} .
	make -j

	if [ $? -ne 0 ]; then
		echo FAILED to build umr.  Try adding -DUMR_NO_GUI=ON to the command line.
		exit 1
	fi
fi

echo
echo
echo Running KAT tests...

# run more complicated KATs
for f in ${UMRVECTORSPATH}/kat/*.cmd; do
	txt=`echo $f | sed -e 's/.cmd/.txt/'`
	kat=`echo $f | sed -e 's/.cmd/.answer/'`
	cmd=`cat $f`
	cmd=`echo ${cmd} | sed -e "sTtest/T${UMRVECTORSPATH}T"`
	echo "Running: umr ${cmd}"
	${UMRAPPPATH} ${cmd} > /tmp/umr.test
	if [ $? -ne 0 ]; then
		echo "FAILED.   Running umr failed..."
		exit 1
	fi
	pass=0
	for kf in ${kat}*; do
		diff /tmp/umr.test ${kf} >/dev/null
		if [ $? -eq 0 ]; then
			pass=1;
			break;
		fi;
	done
	if [ ${pass} -eq 0 ]; then
		echo "FAILED.  Test ${txt} failed..."
		diff -ur $kat /tmp/umr.test
		exit 1;
	fi
done
echo PASSED.

# run simple KATs
echo
echo
echo Running simple KAT/VM tests...

${UMRTESTPATH} ${UMRVECTORSPATH}/vm/
if [ $? -ne 0 ]; then
	echo "FAILED."
	exit 1
fi
echo PASSED.

#run install test
echo
echo
echo Running install test...
git clean -dxf
mkdir install_tmp
cmake . -DCMAKE_INSTALL_PREFIX=`pwd`/install_tmp/ -DUMR_INSTALL_DEV=ON
make -j install
if [ $? -ne 0 ]; then
	echo "FAILED."
	exit 1;
fi
echo PASSED.

exit 0


