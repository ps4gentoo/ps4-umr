# bash completion for umr
# Copyright (C) Advanced Micro Devices 2022
#
# You should set UMR_DATABASE_PATH before sourcing this file in your
# .bashrc. To source it, do "source <location of this file>"--to see a
# usual location of this file, see the FILES section in umr(1).
#
# I recommend you "set show-all-if-ambiguous On" in your .inputrc, for
# matches to be listed immediately (on a first TAB key press) as
# opposed to ring the bell the first time and seeing matches on the
# second TAB key press. To load the keymap, do "bind -f ~/.inputrc" in
# your .bashrc, assuming your .inputrc is in your home directory.

if [[ ! -d $UMR_DATABASE_PATH ]]; then
    echo -n "UMR_DATABASE_PATH must be set for correct operation of umr bash completion."
    echo " See the FILES section in umr(1)."
fi

_umr_comp_option_flags()
{
    local FLAGS=(bits bitsfull empty_log follow no_follow_ib use_pci use_colour read_smc quiet no_kernel verbose halt_waves disasm_early_term no_disasm disasm_anyways wave64 full_shader no_fold_vm_decode)
    local F G CURR_OPTIONS
    local ACTIVE_OPTIONS=()
    local ACTIVE_FLAGS=()

    CURR_OPTIONS="`echo ${COMP_WORDS[*]} | sed -E -e 's/^.*-O[[:space:]]+(.*)$/\1/g'`"
    CURR_OPTIONS="`echo $CURR_OPTIONS | sed -E -e 's/,/ /g'`"

    # Get rid of user-input partial option string
    for F in $CURR_OPTIONS ; do
	if [[ $F == $cur ]] ; then
	   continue;
	fi
	ACTIVE_OPTIONS+=("$F")
    done

    # Offer only options which are not already part of --option/-O
    for F in ${FLAGS[*]} ; do
	for G in ${ACTIVE_OPTIONS[*]} ; do
	    if [[ $F == $G ]]; then
		continue 2;
	    fi
	done
	ACTIVE_FLAGS+=($F)
    done

    # If the user presses TAB again after full completion, as opposed
    # to say SPACE BAR, then give them comma and the rest of the
    # flags.
    COMPREPLY=( $(compgen -W "${ACTIVE_FLAGS[*]}" -- "$cur") )
    if [[ $cur == ${COMPREPLY[0]} ]] && (( ${#COMPREPLY[*]} == 1 )) && (( ${#ACTIVE_FLAGS[*]} > 1 )) ; then
	COMPREPLY=( $(compgen -S ',' -W "${ACTIVE_FLAGS[*]}" -- "$cur") )
    fi
    # Complete with a space after all options entered.
    if [[ $cur != ${COMPREPLY[0]} ]] || (( ${#COMPREPLY[*]} != 1 )) || (( ${#ACTIVE_FLAGS[*]} > 1 )) ; then
	compopt -o nospace
    fi
}

_umr_comp_instance()
{
    local INSTANCE=(`sudo \ls -1 /sys/kernel/debug/dri/`)

    COMPREPLY=( $(compgen -W "${INSTANCE[*]}" -- "$cur") )
}

_umr_comp_pci()
{
    local PCI_LIST=(`lspci -D | grep VGA | cut -f 1 -d \   | tr '\n' ' '`)

    COMPREPLY=( $(compgen -W "${PCI_LIST[*]}" -- "$cur") )
}

_umr_comp_prof()
{
    local PROFS=( "pixel=" "vertex=" "compute=" )

    COMPREPLY=( $(compgen -W "${PROFS[*]}" -- "$cur") )
    compopt -o nospace
}

_umr_comp_ring()
{
    local INSTANCE=`echo "$COMP_LINE" | sed -E -e 's/^.*(--instance|-i)[[:space:]]+([0-9]+).*$/\2/g'`

    if [[ ! $INSTANCE =~ ^[0-9]+$ ]]; then
	# The default instance in umr is 0. See umr(1) under "--instance".
	INSTANCE=0
    fi

    local RINGS=( `sudo $(which find) /sys/kernel/debug/dri/$INSTANCE -name amdgpu_ring_* | sed -E -e "s%/sys/kernel/debug/dri/$INSTANCE/amdgpu_ring_%%g"` )

    COMPREPLY=( $(compgen -W "${RINGS[*]}" -- "$cur") )
}

_umr_comp_clock_scan()
{
    local CLOCKS=( `sudo $(which umr) --clock-scan | grep ".*:$" | sed -E -e 's/[: ]//g'` )

    COMPREPLY=( $(compgen -W "${CLOCKS[*]}" -- "$cur") )
}

_umr_comp_force()
{
    local ASIC_NAMES=(`awk '{ print $2; }' $UMR_DATABASE_PATH/pci.did | sed -E -e 's/.asic//g' | sort | uniq`)
    if [[ $cur =~ ^\..* ]] ; then
	COMPREPLY=( $(compgen -P '.' -W "${ASIC_NAMES[*]}" -- "${cur#.}") )
    else
	COMPREPLY=( $(compgen -W "${ASIC_NAMES[*]}" -- "$cur") )
    fi
}

_umr_comp_gpu()
{
    local INSTANCE=( `sudo \ls -1 /sys/kernel/debug/dri/` )
    local PCI_BUS_DIR_NAMES=()
    local PCI_BUS_IDS=()
    local -A PCI_BUS_ASIC_NAME
    local SUGGEST=()
    local ASIC_NAMES=()
    local TEMP_ASIC_NAMES=()
    local INST_ASIC_NAME=()
    local GPU_NAME
    local F

    # We need to do it like this, so that this works
    # when run from non-root shell.
    for F in ${INSTANCE[*]} ; do
	PCI_BUS_DIR_NAMES+=("/sys/kernel/debug/dri/$F/name")
    done
    PCI_BUS_IDS=( `sudo cat ${PCI_BUS_DIR_NAMES[*]} | sed -E -e 's/^.* (dev=([:.[:xdigit:]]+)) .*$/\2/g' | sort | uniq` )

    for F in ${INSTANCE[*]} ; do
	local PCI_ID=`sudo cat  "/sys/kernel/debug/dri/$F/name" | sed -E -e 's/^.* (dev=([:.[:xdigit:]]+)) .*$/\2/g'`
	local DEV_ID=`cat /sys/class/pci_bus/${PCI_ID%:??.?}/device/$PCI_ID/device`
	local DEV_ID_NAME=`grep -i $DEV_ID ${UMR_DATABASE_PATH}/pci.did | awk '{ print $2; }' | sed -E -e 's/.asic//g'`
	TEMP_ASIC_NAMES+=($DEV_ID_NAME)
	PCI_BUS_ASIC_NAME["$PCI_ID"]="$DEV_ID_NAME"
	INST_ASIC_NAME["$F"]="$DEV_ID_NAME"
    done
    # Remove duplicates
    ASIC_NAMES=( `(for F in ${TEMP_ASIC_NAMES[*]} ; do echo $F; done) | sort | uniq` )

    if [[ ! $cur =~ .*@.* ]] && [[ ! $cur =~ .*=.* ]]; then
	COMPREPLY=( $(compgen -W "${ASIC_NAMES[*]}" -- "$cur") )
	if [[ ${#COMPREPLY[*]} == 1 ]]; then
	    GPU_NAME=${COMPREPLY[0]}
	    COMPREPLY=( $(compgen -W "${GPU_NAME}@ ${GPU_NAME}=" -- "$cur") )
	fi
	compopt -o nospace
	return
    fi

    if [[ $cur =~ .*@.* ]]; then
	# Find all instances of the selected ASIC name and offer them
	local INST_OFFER=()
	GPU_NAME="${cur%%@*}"
	for F in ${INSTANCE[*]} ; do
	    if [[ $GPU_NAME == ${INST_ASIC_NAME["$F"]} ]] ; then
		INST_OFFER+=("$F")
	    fi
	done
	SUGGEST=( $(compgen -P "${GPU_NAME}@" -W "${INST_OFFER[*]}") )
    elif [[ $cur =~ .*=.* ]]; then
	# Find all PCI bus IDs of the selected ASIC name and offer them
	local PCI_ID_OFFER=()
	GPU_NAME="${cur%%=*}"
	for F in ${PCI_BUS_IDS[*]} ; do
	    if [[ $GPU_NAME == ${PCI_BUS_ASIC_NAME["$F"]} ]] ; then
		PCI_ID_OFFER+=("$F")
	    fi
	done
	SUGGEST=( $(compgen -P "${GPU_NAME}=" -W "${PCI_ID_OFFER[*]}") )
    fi

    COMPREPLY=( $(compgen -W "${SUGGEST[*]}" -- $cur) )
}

# Sets GPU_NAME with the name of the GPU given on the command line by
# --force, --gpu or --instance, or default, and sets up IP_BLOCKS an
# array of the names of the IP blocks of GPU_NAME.
#
_umr_setup_gpu_ipblocks()
{
    # The regex we use to match an ASIC name is "[[:alnum:]_]+".
    local FORCE=`echo "$COMP_LINE" | sed -E -e 's/^.*(--force|-f)[[:space:]]+([[:alnum:]_]+).*$/\2/g'`
    local GPU=`echo "$COMP_LINE" | sed -E -e  's/^.*(--gpu|-g)[[:space:]]+([[:alnum:]_]+)(@|=)*.*$/\2/g'`
    local INSTANCE=`echo "$COMP_LINE" | sed -E -e 's/^.*(--instance|-i)[[:space:]]+([0-9]+).*$/\2/g'`

    # GPU_NAME is set only when given by a parameter on the command line.
    # GPU_NAME2 is always set, even when not specified on the command line.
    IP_BLOCKS=
    GPU_NAME=
    GPU_NAME2=
    DEFAULT_GPU_NAME=

    if [[ $COMP_LINE =~ (--force|-f) ]]; then
	GPU_NAME="$FORCE"
	IP_BLOCKS=( `sudo umr --force $GPU_NAME --list-blocks | sed -E -e "s/^[[:space:]]*${GPU_NAME}\.(.*)[[:space:]]+\(.*$/\1/g"` )
	GPU_NAME2="$GPU_NAME"
    elif [[ $COMP_LINE =~ (--gpu|-g) ]]; then
	GPU_NAME="$GPU"
	IP_BLOCKS=( `sudo umr --force $GPU_NAME --list-blocks | sed -E -e "s/^[[:space:]]*${GPU_NAME}\.(.*)[[:space:]]+\(.*$/\1/g"` )
	GPU_NAME2="$GPU_NAME"
    elif [[ $COMP_LINE =~ (--instance|-i) ]]; then
	GPU_NAME=`sudo umr --instance $INSTANCE --list-blocks | head -1 | cut -f 1 -d . | sed -E -e 's/[[:space:]]+//g'`
	IP_BLOCKS=( `sudo umr --instance $INSTANCE --list-blocks | sed -E -e "s/^[[:space:]]*${GPU_NAME}\.(.*) \(.*$/\1/g"` )
	GPU_NAME2="$GPU_NAME"
    else
	GPU_NAME2=`sudo umr --list-blocks | head -1 | cut -f 1 -d . | sed -E -e 's/[[:space:]]+//g'`
	IP_BLOCKS=( `sudo umr --list-blocks | sed -E -e "s/^[[:space:]]*.*\.(.*) \(.*$/\1/g"` )
    fi

    DEFAULT_GPU_NAME=`sudo umr --list-blocks | head -1 | cut -f 1 -d . | sed -E -e 's/[[:space:]]+//g'`
}

_umr_comp_lookup()
{
    # Should specify a GPU with --force or --gpu, of even --instance.
    # The --gpu option also specifies an IP block and is thus
    # redundant--better use --force.
    # Specifying a GPU is optional--a default detected
    # will be used.
    # The format is --lookup <IP block>.<register> <value>.

    _umr_setup_gpu_ipblocks
    local REGS
    local FINAL
    local F

    if [[ ! $cur =~ .*\. ]]; then
	COMPREPLY=( $(compgen -S '.' -W "${IP_BLOCKS[*]}" -- $cur) )
	compopt -o nospace
    else
	# Complete a register for the given IP block.
	# See _umr_comp_asic_ipblock_registers() below.
	F="${cur%.*}"
	if [[ -z $GPU_NAME ]] || [[ $GPU_NAME == $DEFAULT_GPU_NAME ]] ; then
	    REGS=( `sudo umr --list-regs ${F} | sed -E -e "s/(.*)\.(.*)\.(.*)[[:space:]]+\(.*$/\3/g"` )
	else
	    REGS=( `sudo umr --force $GPU_NAME --list-regs ${F%\{*} | sed -E -e "s/${GPU_NAME}\.(.*)\.(.*)[[:space:]]+\(.*$/\2/g"` )
	    if (( ${#REGS[*]} == 0 )); then
		REGS=( `sudo umr --force $GPU_NAME --list-regs ${F} | sed -E -e "s/${GPU_NAME}\.(.*)\.(.*)[[:space:]]+\(.*$/\2/g"` )
	    fi
	fi
	FINAL=( $(compgen -P "${F}." -W "${REGS[*]}") )
	COMPREPLY=( $(compgen -W "${FINAL[*]}" -- "$cur") )
    fi

    unset GPU_NAME GPU_NAME2 DEFAULT_GPU_NAME IP_BLOCKS
}

_umr_comp_ipblock()
{
    # Handle --list-regs

    _umr_setup_gpu_ipblocks

    # If compline specifes a GPU, then complete a block of that GPU;
    # else complete all GPUs and the current GPU's blocks in one list.

    if [[ $COMP_LINE =~ (--force|-f|--gpu|-g|--instance|-i) ]] ; then
	COMPREPLY=( $(compgen -W "${IP_BLOCKS[*]}" -- "$cur") )
    elif [[ $cur =~ .*\..* ]] ; then
	# The ASIC here is the default one recommended in the "else"
	# case below.  Because of this, we don't use "--force" with
	# "--list-block" and so also get the instance numbers of the
	# blocks. Then we combine that with the ASIC and offer it as
	# completion. See _umr_comp_asic_ipblock_registers() below.
	local asic=${cur%%.*}
	local blocks=( `sudo umr --list-blocks | sed -E -e "s/^[[:space:]]*.*\.(.*) \(.*$/\1/g"` )
	local F=( $(compgen -P "${asic}." -W "${blocks[*]}") )
	COMPREPLY=( $(compgen -W "${F[*]}" -- "$cur") )
    else
	COMPREPLY=( $(compgen -W "${DEFAULT_GPU_NAME}. ${IP_BLOCKS[*]}" -- "$cur") )
	# This uses the zero element
	if [[ $COMPREPLY =~ .*\. ]] ; then
	    compopt -o nospace
	fi
    fi

    unset GPU_NAME GPU_NAME2 DEFAULT_GPU_NAME IP_BLOCKS
}

_umr_comp_asic_ipblock_registers()
{
    # The format is --writebit asic.ipblock.regname,
    # the same as that of --write and --read.

    _umr_setup_gpu_ipblocks
    local ips=()
    local FINAL=()
    local REGS=()
    local F

    ips=( $(compgen -P "${GPU_NAME2}." -W "${IP_BLOCKS[*]}") )

    if [[ ! $cur =~ ${GPU_NAME2}\..*\. ]]; then
       COMPREPLY=( $(compgen -S '.' -W "${ips[*]}" -- $cur) )
       compopt -o nospace
    else
	# cur = asic.ipblock.*
	#
	# When using --force with the default (installed in the
	# system) ASIC and --list-regs together, --list-regs does not
	# take instance numbers, so to pass an instance number, we
	# need to know if the GPU was forced on the command line.
	# First however, get rid of user hints.
	F="${cur%.*}"
	if [[ -z $GPU_NAME ]] || [[ $GPU_NAME == $DEFAULT_GPU_NAME ]] ; then
	    REGS=( `sudo umr --list-regs ${F} | sed -E -e "s/(.*)\.(.*)\.(.*)[[:space:]]+\(.*$/\3/g"` )
	else
	    REGS=( `sudo umr --force $GPU_NAME --list-regs ${F%\{*} | sed -E -e "s/${GPU_NAME}\.(.*)\.(.*)[[:space:]]+\(.*$/\2/g"` )
	    if (( ${#REGS[*]} == 0 )); then
		REGS=( `sudo umr --force $GPU_NAME --list-regs ${F} | sed -E -e "s/${GPU_NAME}\.(.*)\.(.*)[[:space:]]+\(.*$/\2/g"` )
	    fi
	fi
	# Cannot combine the compgen since a match is performed
	# _before_ the prefix is applied! And if we want to match,
	# we need to tack the prefix prior to asking for a match.
	FINAL=( $(compgen -P "${F}." -W "${REGS[*]}") )
	COMPREPLY=( $(compgen -W "${FINAL[*]}" -- $cur) )
    fi

    unset GPU_NAME GPU_NAME2 DEFAULT_GPU_NAME IP_BLOCKS
}

_umr_comp_ring_stream()
{
    _umr_comp_ring
    compopt -o nospace
}

_umr_completion()
{
    local ALL_LONG_ARGS=(--database-path --option --gpu --instance --force --pci --gfxoff --vm-partition --bank --sbank --cbank --config --enumerate --list-blocks --list-regs --dump-discovery-table --lookup --write --writebit --read --logscan --top --waves --profiler --vm-decode --vm-read --vm-write --vm-write-word --vm-disasm --ring-stream --dump-ib --dump-ib-file --header-dump --power --clock-scan --clock-manual --clock-high --clock-low --clock-auto --ppt-read --gpu-metrics --power --vbios-info --test-log --test-harness --server --gui)

    local cur prev

    COMP_WORDBREAKS=" ,"

    COMPREPLY=()
    cur=$2
    prev=$3

    case $prev in
	--database-path|-dbp)
	    compopt -o default -o dirnames
	    ;;
	--option|-O|bits|bitsfull|empty_log|follow|no_follow_ib|use_pci|use_colour|read_smc|quiet|no_kernel|verbose|halt_waves|disasm_early_term|no_disasm|disasm_anyways|wave64|full_shader|no_fold_vm_decode|,)
	    _umr_comp_option_flags
	    ;;
	--force|-f)
	    _umr_comp_force
	    ;;
	--gpu|-g)
	    _umr_comp_gpu
	    ;;
	--instance|-i)
	    _umr_comp_instance
	    ;;
	--pci)
	    _umr_comp_pci
	    ;;
	--gfxoff|-go)
	    COMPREPLY=( $(compgen -W "0 1" -- "$cur") )
	    ;;
        -lr|--list-regs)
            _umr_comp_ipblock
            ;;
	--lookup|-lu)
	    _umr_comp_lookup
	    ;;
	-r|--read|-w|--write|--writebit|-wb)
	    _umr_comp_asic_ipblock_registers
	    ;;
	--waves|-wa)
	    _umr_comp_ring
	    ;;
	--profiler|-prof)
	    _umr_comp_prof
	    ;;
	pixel=*|vertex=*|compute=*)
	    _umr_comp_ring
	    ;;
	--dump-ib-file|-df|--test-log|-tl|--test-harness|-th)
	    compopt -o default -o filenames
	    ;;
	--clock-scan|-cs|--clock-manual|-cm)
	    _umr_comp_clock_scan
	    ;;
	--ring-stream|-RS)
	    _umr_comp_ring_stream
	    ;;
	*)
	    COMPREPLY=( $(compgen -W "${ALL_LONG_ARGS[*]}" -- $cur) )
	    ;;
    esac
    return 0
}

complete -F _umr_completion umr
complete -F _umr_completion umrgui
