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
#include "umrapp.h"
#include <ncurses.h>
#include <time.h>

enum sensor_maps {
	SENSOR_IDENTITY = 0,
	SENSOR_D1000,
	SENSOR_D100,
	SENSOR_WAIT,
};

struct power_bitfield{
	char *regname;
	uint32_t value;
	enum amd_pp_sensors sensor_id;
	enum sensor_maps map;
};

static struct power_bitfield p_info[] = {
	{"GFX_SCLK", 0, AMDGPU_PP_SENSOR_GFX_SCLK, SENSOR_D100 },
	{"GFX_MCLK", 0, AMDGPU_PP_SENSOR_GFX_MCLK, SENSOR_D100 },
	{"UVD_VCLK", 0, AMDGPU_PP_SENSOR_UVD_VCLK, SENSOR_D100 },
	{"UVD_DCLK", 0, AMDGPU_PP_SENSOR_UVD_DCLK, SENSOR_D100 },
	{"VCE_ECCLK", 0, AMDGPU_PP_SENSOR_VCE_ECCLK, SENSOR_D100 },
	{"AVG_GPU",  0, AMDGPU_PP_SENSOR_GPU_POWER, SENSOR_WAIT },
	{"GPU_LOAD", 0, AMDGPU_PP_SENSOR_GPU_LOAD, SENSOR_IDENTITY },
	{"MEM_LOAD", 0, AMDGPU_PP_SENSOR_MEM_LOAD, SENSOR_IDENTITY },
	{"GPU_TEMP", 0, AMDGPU_PP_SENSOR_GPU_TEMP, SENSOR_D1000 },
	{NULL, 0, 0, 0},
};

static volatile uint32_t gpu_power_data[32];

static void print_temperature(uint32_t temp)
{
	if (options.use_colour){
		if (temp <= 40){
			attron(COLOR_PAIR(1)|A_BOLD);
			printw("%u C\n", temp);
			attroff(COLOR_PAIR(1)|A_BOLD);
		} else if ( temp > 40 && temp <= 70) {
			attron(COLOR_PAIR(2)|A_BOLD);
			printw("%u C\n", temp);
			attroff(COLOR_PAIR(2)|A_BOLD);
		} else if (temp > 70){
			attron(COLOR_PAIR(3)|A_BOLD);
			printw("%u C\n", temp);
			attroff(COLOR_PAIR(3)|A_BOLD);
		}
	} else {
		printw("%u C\n", temp);
	}
}

static void print_gpu_load(uint32_t gpu_load)
{
	if (options.use_colour){
		if (gpu_load <= 40){
			attron(COLOR_PAIR(1)|A_BOLD);
			printw("%u %%\n", gpu_load);
			attroff(COLOR_PAIR(1)|A_BOLD);
		} else if ( gpu_load > 40 && gpu_load <= 70) {
			attron(COLOR_PAIR(2)|A_BOLD);
			printw("%u %%\n", gpu_load);
			attroff(COLOR_PAIR(2)|A_BOLD);
		} else if (gpu_load > 70){
			attron(COLOR_PAIR(3)|A_BOLD);
			printw("%u %%\n", gpu_load);
			attroff(COLOR_PAIR(3)|A_BOLD);
		}
	} else {
		printw("%u %%\n", gpu_load);
	}
}

static uint32_t parse_sensor_value(enum sensor_maps map, uint32_t value)
{
	uint32_t result = 0;

	switch(map) {
		case SENSOR_IDENTITY:
			result = value;
			break;
		case SENSOR_D1000:
			result = value / 1000;
			break;
		case SENSOR_D100:
			result = value / 100;
			break;
		case SENSOR_WAIT:
			result = ((value >> 8) * 1000);
			if ((value & 0xFF) < 100)
				result += (value & 0xFF) * 10;
			else
				result += value;
			result /= 10;
			break;
		default:
			printf("invalid input value!\n");
			break;
	}
	return result;
}

static void power_print(struct umr_asic *asic)
{
	int i;
	time_t tt;
	tt = time(NULL);
	printw("[%s] -- %s\n", asic->asicname, ctime(&tt));

	printw("GFX Clocks and Power:\n\n");
	for ( i = 0; p_info[i].regname != NULL; i++ ){
		switch(p_info[i].sensor_id){
			case AMDGPU_PP_SENSOR_GFX_SCLK:
				if (p_info[i].value != 0){
					printw("\t\tGFX_SCLK:  %u MHz\n", p_info[i].value);
				}
				break;
			case AMDGPU_PP_SENSOR_GFX_MCLK:
				if (p_info[i].value != 0){
					printw("\t\tGFX_MCLK:  %u MHz\n", p_info[i].value);
				}
				break;
			case AMDGPU_PP_SENSOR_UVD_VCLK:
				if (p_info[i].value != 0){
					printw("\t\tUVD_VCLK:  %u MHz\n", p_info[i].value);
				}
				break;
			case AMDGPU_PP_SENSOR_UVD_DCLK:
				if (p_info[i].value != 0){
					printw("\t\tUVD_DCLK:  %u MHz\n", p_info[i].value);
				}
				break;
			case AMDGPU_PP_SENSOR_VCE_ECCLK:
				if (p_info[i].value != 0){
					printw("\t\tVCE_ECCLK:  %u MHz\n", p_info[i].value);
				}
				break;
			case AMDGPU_PP_SENSOR_GPU_POWER:
				if (p_info[i].value != 0){
					printw("\t\taverage GPU:");
					printw("%3d.%02d W \n", p_info[i].value/100, p_info[i].value%100);
				}
				break;
			case AMDGPU_PP_SENSOR_GPU_LOAD:
				printw("\n= = = = = = = = = = = = = = = = = = = = = = = = = = =\n");
				printw("\nGPU Load:\t\t");
				print_gpu_load(p_info[i].value);
				break;
			case AMDGPU_PP_SENSOR_MEM_LOAD:
				printw("\nMEM Load:\t\t");
				print_gpu_load(p_info[i].value);
				break;
			case AMDGPU_PP_SENSOR_GPU_TEMP:
				printw("\nGPU Temperature:\t");
				print_temperature(p_info[i].value);
				break;
			default:
				break;
		}
	}
	printw("\n Print (q) to quit.\n");
}

void umr_power(struct umr_asic *asic)
{
	int i, size, quit = 0;
	struct timespec req;
	char fname[64];

	initscr();
	start_color();
	cbreak();
	nodelay(stdscr, 1);
	noecho();

	init_pair(1, COLOR_GREEN, COLOR_BLACK);
	init_pair(2, COLOR_YELLOW, COLOR_BLACK);
	init_pair(3, COLOR_RED, COLOR_BLACK);

	// setup loop, fix refresh rate 1 second
	req.tv_sec = 1;
	req.tv_nsec = 0;

	while (!quit) {
		snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_sensors", asic->instance);
		asic->fd.sensors = open(fname, O_RDWR);
		if (asic->fd.sensors) {
			for ( i = 0; p_info[i].regname != NULL; i++){
				size = 4;
				p_info[i].value = 0;
				gpu_power_data[0] = 0;
				umr_read_sensor(asic, p_info[i].sensor_id, (uint32_t*)&gpu_power_data[0], &size);
				if (gpu_power_data[0] != 0){
					p_info[i].value = gpu_power_data[0];
					p_info[i].value = parse_sensor_value(p_info[i].map, p_info[i].value);
				}
			}
			close(asic->fd.sensors);
		} else {
			printf("failed to open amdgpu_sensors!");
			break;
		}

		nanosleep(&req, NULL);
		move(0, 0);
		clear();

		power_print(asic);

		if ((i = wgetch(stdscr)) != ERR) {
			if (i == 'q') {
				quit = 1;
			}
		}
		refresh();
	}
	endwin();
}
