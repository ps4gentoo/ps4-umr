#!/usr/bin/bash

#figure out where scripts are installed and source functions
dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

#We need halt_if_hws_hang=1 for the logs to be valid
if [ "$(cat /sys/module/amdgpu/parameters/halt_if_hws_hang)" -ne 1 ]; then
        echo "halt_if_hws_hang must be set to 1. Exiting"
        echo "Please run: echo 1 > /sys/module/amdgpu/parameters/halt_if_hws_hang"
        exit 1
fi

#relaunch ourself as root
if [ `whoami` != root ]; then
	sudo ${dir}/dump_all_cpc_info.sh "$@"
	exit 0
fi

#save current dir
where=`pwd`

# build a local copy of umr
apt install build-essential libpciaccess-dev libncurses-dev cmake git -y
cd ${dir}/../../
git clean -dxf
cmake -DUMR_NO_GUI=on -DUMR_NO_DRM=ON -DUMR_NO_LLVM=ON -DUMR_NO_SERVER=ON .
make -j umr
export PATH=`pwd`/src/app:${PATH}
cd ${dir}

prefix="$(hostname)_$(date +"%Y-%m-%d_%H_%M")"
kfddbg="/sys/kernel/debug/kfd"
cat "${kfddbg}/rls" 2>&1 >"${prefix}_rls.txt"
cat "${kfddbg}/mqds" 2>&1 >"${prefix}_mqds.txt"

# start dumping data
source ${dir}/diag_functions.sh
iter_over_gpu_xcc dump_cpc
iter_over_gpu_xcc dump_waves

#These aren't needed for most debug cases
if [ "$1" == "all" ]; then
	iter_over_gpu_xcc dump_cpc_scratch_mems
	iter_over_gpu_xcc dump_cp_regs
	iter_over_gpu_xcc dump_headers
elif [ "$1" == "mec" ]; then
	iter_over_gpu_xcc dump_headers
fi

echo "Generating ${prefix}_dmesg.txt"
journalctl -b > "${prefix}_dmesg.txt"

#collect results
tar -czvf ${where}/${prefix}_logs.tgz *.txt

#return user to directory they started in
cd ${where}

echo
echo "Logs were tar'ed up in ${where}/${prefix}_logs.tgz"
echo
