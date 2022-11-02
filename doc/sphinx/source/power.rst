===============
Power and Clock
===============

The UMR tool can get real time power and clock information, dynamically
set clock and read powerplay table information.

::

	umr --power

The command reads real time power and clock information, including GPU
load, memory load, GPU temperature, GFX clock information and power
average. The refresh rate is one second.

-------------------
Check and set clock
-------------------

::

	umr --clock-scan <clock>

Command will show the current hierarchy value of each clock. Default will
list all the clock information, otherwise will show the corresponding
clock, eg. sclk.

::

	umr --clock-manual <clock> <value>

Command will set value of the corresponding clock. Default will set
dynamic power performance level to manual.

::

	umr --clock-high

Command will set dynamic power performance level to high.

::

	umr --clock-low

Command will set dynamic power performance level to low.

::

	umr --clock-auto

Command will set dynamic power performance level to auto.

---------------------------
Powerplay table information
---------------------------

::

	umr --ppt_read [ppt_field_name]

This Command will read all powerplay talbe information in default.
If has input string, print corresponding value in powerplay table.
