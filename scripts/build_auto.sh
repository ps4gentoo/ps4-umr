#!/bin/bash
make -C ../comp/

parse_reg_bits() {
	suf=""
	if [ -e ${pk}/include/asic_reg/$1_offset.h ]; then suf="offset"; fi
	if [ -e ${pk}/include/asic_reg/$1_d.h ]; then suf="d"; export UMR_NO_SOC15=1; fi
	if [ "${suf}" != "" ]; then
		../comp/compiler ${pk}/include/asic_reg/$1_${suf}.h ${pk}/include/asic_reg/$1_sh_mask.h > ../database/ip/$2
	fi
	unset UMR_NO_SOC15
}

cd ${pk}
git checkout amd-staging-drm-next
git reset --hard origin/amd-staging-drm-next
cd -

x=0

for f in ${pk}/include/asic_reg/*/*_offset.h ${pk}/include/asic_reg/*/*_d.h; do
	ipname=`echo ${f} | tr [\/] [\ ] | awk '{ print $(NF); }' | tr [_] [\ ] | awk '{print $1}'`
	dirname=`echo ${f} | tr [\/] [\ ] | awk '{ print $(NF - 1); }'`
	ipnamelen=`expr ${#ipname} + 2`
	revname=`echo ${f} | tr [\/] [\ ] | awk '{ print $NF; }' | cut -b${ipnamelen}- | sed -e 's/\.h//' -e 's/_d//' -e 's/_offset//'`
	basename=`echo ${f} | sed -e 's/_d//' -e 's/_offset//' -e 's/\.h//'`
	smnname=`echo ${f} | sed -e 's/_d/_smn/' -e 's/_offset/_smn/'`

	#older IP only had 2 parts to the name, add a '_0' in this case
	revnameparts=`echo ${revname} | tr [_] [\ ] | awk '{ print NF; }'`
	if [ ${revnameparts} == 2 ]; then
		outrevname=${revname}_0
	else
		outrevname=${revname}
	fi

	if [ -e ${smnname} ]; then
		cat ${smnname} ${f} > /tmp/delme.h
		../comp/compiler /tmp/delme.h `echo ${f} | sed -e 's/_d/_sh_mask/' -e 's/_offset/_sh_mask/'` > ../database/ip/${ipname}_${outrevname}.reg
		rm -f /tmp/delme.h
	else
		parse_reg_bits ${dirname}/${ipname}_${revname} ${ipname}_${outrevname}.reg
	fi
	x=`expr ${x} + 1`
done

# random bits
UMR_NO_SOC15=1 ../comp/compiler ${pk}/include/asic_reg/gca/gfx_7_0_d.h ${pk}/include/asic_reg/gca/gfx_7_2_sh_mask.h > ../database/ip/gfx_7_0_0.reg    # there is no shift/mask for 7.0.0

echo "Parsed ${x} register files..."

x=0
for f in ${pk}/include/*ip_offset.h; do
	asicname=`echo ${f} | tr [\/] [\ ] | awk '{ print $(NF); }' | sed -e 's/_ip_offset.h//'`
	if [ ${asicname} == arct ]; then
		asicname=arcturus
	fi
	grep ${asicname}.asic ../database/pci.did > /dev/null
	if [ $? == 0 ]; then
		../comp/compiler ${f} > ../database/${asicname}.soc15
		x=`expr ${x} + 1`
	fi
done
echo "Parsed ${x} IP offset files..."

make -C ../comp clean
