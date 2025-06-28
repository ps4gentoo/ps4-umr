==============
Basic Commands
==============

UMR accepts commands from the command line in a left-to-right
order.  This means that options and device selection should take
place before operations.

----------------
Device Selection
----------------

By default umr tries to issue commands for the device in the 0'th
slot under **/sys/kernel/debug/dri/**.  This is called the 0'th
*instance*.  A new instance can be selected with the *--instance*
flag.

'''''''''''''''''''''
Selecting by Instance
'''''''''''''''''''''

::

	umr --instance 1 ...

or with the *-i* shorthand

::

	umr -i 1 ...

would both issue commands on the device in the #1 slot.


'''''''''''''''''
Selecting by Name
'''''''''''''''''

Devices may also be selected by name which is useful on
systems with multiple GPUs but only one instance of a specific
device.  This is accomplished with the *--force* or *-f* flags.

::

	umr -f vega10 ...

Would issue a command on the first vega10 it finds.

'''''''''''''''''''''''
Selecting by PCI bus ID
'''''''''''''''''''''''

For even more precision or using umr when the kernel module is not
yet loaded the *--pci* flag can be used.

::

	umr --pci 0000:00:01.0 ...

Would issue a command typically on the first integrated GPU found
on a system.

''''''''''''''''''''''''''''''''''
Selecting by name and instance/pci
''''''''''''''''''''''''''''''''''

To simplify the selection of a device even further you can compound the
asic model and instance/pci device selection into one command with the *--gpu* flag.

::

	umr --gpu vega10@0

For instance, will tell umr that the 0'th instanced DRI device is a vega10.

::

	umr --gpu vega10=0000:00:01.0

For instance, will tell umr that the PCI device at bus address 0000:00:01.0 is a
vega10 asic.

''''''''''''''''''''''''''
Selecting a virtual device
''''''''''''''''''''''''''

There are times where you want to issue a command as if you had
a device in your system (for instance --lookup to decode a register
on a specific device).  This can be accomplished with the virtual
device name specifier:

::

	umr --force .vega10 ...

Would issue a command as if a vega10 were present. 


-------
Options
-------

Options are specified with the *-O* command and are parsed as they
appear on the command line which means they are typically specified
before umr commands.  Multiple options can be issued at once using a
comma separator.

The options available are:

+-------------------+-------------------------------------------------------------------------+
| **Option**        | **Description**                                                         |
+-------------------+-------------------------------------------------------------------------+
| quiet             | Disable various informative outputs that are not required for           |
|                   | functionality.                                                          |
+-------------------+-------------------------------------------------------------------------+
| read_smc          | Enable scanning of SMC registers when issuing a --scan command          |
+-------------------+-------------------------------------------------------------------------+
| bits              | Enables the display of bitfields when registers are presented           |
+-------------------+-------------------------------------------------------------------------+
| bitsfull          | When printing bits use the full path to the bitfield                    |
+-------------------+-------------------------------------------------------------------------+
| empty_log         | Empty MMIO tracer after reading it                                      |
+-------------------+-------------------------------------------------------------------------+
| no_follow_ib      | Instructs the --ring-stream  command to not follow IBs pointed to by    |
|                   | the ring                                                                |
+-------------------+-------------------------------------------------------------------------+
| use_pci           | Enables direct PCI access bypassing the kernels debugfs entries.        |
+-------------------+-------------------------------------------------------------------------+
| use_colour        | Enables colourful output in various commands.  Also accepts use_color   |
+-------------------+-------------------------------------------------------------------------+
| no_kernel         | Attempts to avoid kernel access methods.  Implies *use_pci*.            |
+-------------------+-------------------------------------------------------------------------+
| verbose           | Enables verbose output, for instance in VM decoding                     |
+-------------------+-------------------------------------------------------------------------+
| halt_waves        | Halt active waves while reading wave status data                        |
+-------------------+-------------------------------------------------------------------------+
| wave64            | Scan for 64 or 32 threads per wave                                      |
+-------------------+-------------------------------------------------------------------------+
| disasm_early_term | Terminate disassembly early for UMDs that don't use 0xBF9F0000 marker   |
+-------------------+-------------------------------------------------------------------------+
| no_disasm         | Disable LLVM disassembly.  Used on systems with outdated llvm-dev       |
+-------------------+-------------------------------------------------------------------------+
| disasm_anyways    | Enable disassembly in --waves even if rings are not halted.             |
+-------------------+-------------------------------------------------------------------------+
| no_fold_vm_decode | Disable folding of PDEs when VM decoding multiple pages of memory.      |
|                   | By default, when subsequent pages are decoded if PDEs match previous    |
|                   | pages they are omitted to cut down on the verbosity of the output.      |
|                   | This option disables this and will print the full chain of PDEs for     |
|                   | every page decoded.                                                     |
+-------------------+-------------------------------------------------------------------------+
| force_asic_file   | Force the use of static .asic files instead of trying IP discovery      |
+-------------------+-------------------------------------------------------------------------+
| export_model      | Export a mostly complete .soc15/.asic model from IP discovered data     |
+-------------------+-------------------------------------------------------------------------+
| full_shader       | Always print the full shader in --waves and --ring-stream  output       |
+-------------------+-------------------------------------------------------------------------+

------------------
Device Information
------------------

Device information for the selected device can be printed out with the
--config (-c) command.

::

	$ umr -c
        asic.instance == 0
        asic.devname == 0000:01:00.0

        umr.version == 551b5972da1b
	...<snip>...

Devices can be enumerated with the --enumerated (-e) command.
