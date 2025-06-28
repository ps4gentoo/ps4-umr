=============
Halting Waves
=============

To halt shaders/compute jobs that are currently being worked on
the following function can be used:

::

	int umr_sq_cmd_halt_waves(struct umr_asic *asic, enum umr_sq_cmd_halt_resume mode, int max_retries);

To halt jobs pass **UMR_SQ_CMD_HALT** as 'mode' and to resume pass
**UMR_SQ_CMD_RESUME**.  The 'max_retries' parameter specifies how many
times it should retry issuing the HALT command to halt the waves being
executed.

Note that this does not block the GPU from accepting new jobs it simply
halts (or resumes) jobs that are currently being processed.  


