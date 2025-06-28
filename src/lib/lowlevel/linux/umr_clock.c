/*
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "umr.h"
#include <time.h>

/**
 * umr_read_clock - Read a clock information via sysfs
 */
int umr_read_clock(struct umr_asic *asic, char* clockname, struct umr_clock_source* clock)
{
	FILE* fp = NULL;
	char name[256];
	char* token;
	int ret = -1;
	snprintf(name, sizeof(name)-1, \
		"/sys/class/drm/card%d/device/pp_dpm_%s", asic->instance, clockname);
	clock->clock_level = 0;
	fp = fopen(name, "r");
	if (fp) {
		while (fgets(name, sizeof(name)-1, fp)) {

			if (strstr(name, "*"))
				clock->current_clock = clock->clock_level;

			token = strtok(name, " ");
			while(token != NULL){
			if (strstr(token,"Mhz")){
					sscanf(token, "%uMhz", &clock->clock_Mhz[clock->clock_level]);
					break;
				}
				token = strtok(NULL, " ");
			}
			clock->clock_level++;
		}
		fclose(fp);
		ret = 0;
	}

	return ret;
}

/**
 * umr_set_clock - set a clock value via sysfs
 */
int umr_set_clock(struct umr_asic *asic, const char* clock_name, void* value)
{
	char name[128];
	int fd;
	int input_flag = 0;
	char input[8];
	struct timespec wait = { 0 };
	uint32_t input_len = strlen(value);
	int ret = -1;

	umr_set_clock_performance(asic, "manual");
	strcpy(input, value);
	input[input_len] = ' ';
	
	wait.tv_nsec = 500 * 1000000; /* 500 ms */
	nanosleep(&wait, NULL);

	snprintf(name, sizeof(name)-1, \
		"/sys/class/drm/card%d/device/pp_dpm_%s", asic->instance, clock_name);
	fd = open(name, O_RDWR);
	if (fd) {
		write(fd, input, input_len+1);
		close(fd);
		input_flag = 1;
		ret = 0;
	}

	if (!input_flag)
		asic->err_msg("[ERROR]: Invalid input clock name!\n");

	return ret;
}

/**
 * umr_set_clock_performance - set power_dpm_force_performance_level via sysfs
 */
void umr_set_clock_performance(struct umr_asic *asic, const char* operation)
{
	char fname[128];
	char oper_string[16];
	int fd;
	uint32_t str_len = 0;

	strcpy(oper_string, operation);
	snprintf(fname, sizeof(fname)-1, \
		"/sys/class/drm/card%d/device/power_dpm_force_performance_level", asic->instance);
	fd = open(fname, O_RDWR);
	if (fd) {
		str_len = write(fd, oper_string, strlen(oper_string));
		close(fd);
	}
	if (str_len != strlen(oper_string))
		asic->err_msg("[ERROR]: Operate clock failed!\n");
}

/**
 * umr_check_clock_performance - check power_dpm_force_performance_level via sysfs
 */
int umr_check_clock_performance(struct umr_asic *asic, char* clockperformance, uint32_t len)
{

	FILE* fp = NULL;
	char name[256];
	int ret = 0;
	snprintf(name, sizeof(name)-1, \
		"/sys/class/drm/card%d/device/power_dpm_force_performance_level", asic->instance);
	fp = fopen(name, "r");
	if (fp) {
		fgets(clockperformance, len-1, fp);
		fclose(fp);
		ret = strlen(clockperformance);
	} else {
		ret = 0;
	}

	return ret;
}
