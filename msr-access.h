/*
 * msr-access.h
 *
 *  Created on: 20.05.2019
 *      Author: Oebbler
 *      This header uses modified code from Intel's msr-tools.
 */

/* ----------------------------------------------------------------------- *
 *   Intel's copyright:
 *
 *   Copyright 2000 Transmeta Corporation - All Rights Reserved
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>


/* returns true iff given function code is turbo-mode-related */
char turbo_mode_code(int function_code);
/* helper function which writes data to the MSR for turbo mode */
void write_turbo_multiplier(int function_code, uint64_t newval);
/* helper function for preprocessing data for writing it to the msr */
void write_data(int function_code, uint64_t newval);
/* read data from an msr; This code was originally written by Intel */
uint64_t rdmsr_on_cpu(uint32_t reg, int cpu);
/* write (modified) data to an msr; This code was originally written by Intel */
void wrmsr_on_cpu(uint32_t reg, int cpu, uint64_t data);
unsigned int highbit = 63, lowbit = 0;

char turbo_mode_code(int function_code) {
	return (function_code > 0 && function_code < 5) || function_code == 1234;
}

void write_turbo_multiplier(int function_code, uint64_t newval) {
	if (!turbo_mode_code(function_code)) {
		fprintf(stderr, "The given function code is not implemented. This should never happen.\n");
		exit(4);
	} else {
		uint64_t new_data;
		uint64_t regdata;
		/* TODO: change against values exposed by the processor's MSRs */
		if (newval < 1 || newval > 83) {
			fprintf(stderr, "New multiplier has to be between 1 (100 MHz) and 83 (8300 MHz).\n");
			return;
		}
		regdata = rdmsr_on_cpu(429, 0);
		switch(function_code) {
		case 1:
			new_data = regdata & 0b1111111111111111111111111111111111111111111111111111111100000000;
			break;
		case 2:
			new_data = regdata & 0b1111111111111111111111111111111111111111111111110000000011111111;
			break;
		case 3:
			new_data = regdata & 0b1111111111111111111111111111111111111111000000001111111111111111;
			break;
		case 4:
			new_data = regdata & 0b1111111111111111111111111111111100000000111111111111111111111111;
			break;
		case 1234:
			new_data = regdata & 0b1111111111111111111111111111111100000000000000000000000000000000;
			break;
		default:
			fprintf(stderr, "This functionality in method write_turbo_multiplier is not implemented. This should never happen.\n");
			exit(5);
			break;
		}
		if (function_code > 0 && function_code < 5) {
			new_data = new_data | (newval << ((function_code - 1) * 8));
		} else {
			/* 4 cores */
			new_data = new_data | (newval << 24);
			/* 3 cores */
			new_data = new_data | (newval << 16);
			/* 2 cores */
			new_data = new_data | (newval << 8);
			/* 1 core */
			new_data = new_data | newval;
		}
		wrmsr_on_cpu(429, 0, new_data);
	}
}

void write_data(int function_code, uint64_t newval) {
	uint64_t regdata;
	uint64_t new_data;
	uint64_t units_raw;
	int power_unit;
	int abs_temp_limit;
	/* read the main menu for meanings of fuction codes */
	switch (function_code) {
	case 0:
		/* error handling for invalid input */
		switch (newval) {
		case 7:
			fprintf(stderr, "CPU voltage is read-only.\n");
			break;
		case 14:
			fprintf(stderr, "This program will not lock or unlock the power limit settings.\n");
			break;
		default:
			fprintf(stderr, "No valid input.\n");
			break;
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 1234:
		write_turbo_multiplier(function_code, newval);
		break;
	case 5:
		if (newval < 1 || newval > 83) {
			fprintf(stderr, "New multiplier has to be between 1 (100 MHz) and 83 (8300 MHz).\n");
			return;
		}
		regdata = rdmsr_on_cpu(1568, 0);
		new_data = regdata & 0b1111111111111111111111111111111111111111111111110000000011111111;
		new_data = new_data | (newval << 8);
		wrmsr_on_cpu(1568, 0, new_data);
		break;
	case 6:
		if (newval < 1 || newval > 83) {
			fprintf(stderr, "New multiplier has to be between 1 (100 MHz) and 83 (8300 MHz).\n");
			return;
		}
		regdata = rdmsr_on_cpu(1568, 0);
		new_data = regdata & 0b1111111111111111111111111111111111111111111111111111111100000000;
		new_data = new_data | newval;
		wrmsr_on_cpu(1568, 0, new_data);
		break;
	case 8:
		if (newval < 1 || newval > 4095) {
			fprintf(stderr, "New power limit exceeds range (4095 watts) or is negative.\n");
			return;
		}
		regdata = rdmsr_on_cpu(1552, 0);
		units_raw = rdmsr_on_cpu(1542, 0);
		power_unit = (int)((units_raw) & 0b01111);
		new_data = regdata & 0b1111111111111111111111111111111111111111111111111000000000000000;
		newval = (newval * (1 << power_unit)) & 0b0111111111111111;
		new_data = new_data | newval;
		wrmsr_on_cpu(1552, 0, new_data);
		break;
	case 9:
		if (newval != 1 && newval != 0) {
			fprintf(stderr, "This value can only be set to 0 (disabled) or 1 (enabled).\n");
			return;
		}
		regdata = rdmsr_on_cpu(1552, 0);
		new_data = regdata & 0b1111111111111111111111111111111111111111111111110111111111111111;
		if (newval == 1) {
			new_data = new_data | ((uint64_t)1 << 15);
		}
		wrmsr_on_cpu(1552, 0, new_data);
		break;
	case 10:
		if (newval != 1 && newval != 0) {
			fprintf(stderr, "This value can only be set to 0 (disabled) or 1 (enabled).\n");
			return;
		}
		regdata = rdmsr_on_cpu(1552, 0);
		new_data = regdata & 0b1111111111111111111111111111111111111111111111101111111111111111;
		if (newval == 1) {
			new_data = new_data | ((uint64_t)1 << 16);
		} else if (newval != 0) {
			break;
		}
		wrmsr_on_cpu(1552, 0, new_data);
		break;
	case 11:
		if (newval < 1 || newval > 4095) {
			fprintf(stderr, "New power limit exceeds range (4095 watts) or is negative.\n");
			return;
		}
		regdata = rdmsr_on_cpu(1552, 0);
		units_raw = rdmsr_on_cpu(1542, 0);
		power_unit = (int)((units_raw) & 0b01111);
		new_data = regdata & 0b1111111111111111100000000000000011111111111111111111111111111111;
		newval = ((newval * (1 << power_unit)) & 0b0111111111111111) << 32;
		new_data = new_data | newval;
		wrmsr_on_cpu(1552, 0, new_data);
		break;
	case 12:
		if (newval != 1 && newval != 0) {
			fprintf(stderr, "This value can only be set to 0 (disabled) or 1 (enabled).\n");
			return;
		}
		regdata = rdmsr_on_cpu(1552, 0);
		new_data = regdata & 0b1111111111111111011111111111111111111111111111111111111111111111;
		if (newval == 1) {
			new_data = new_data | ((uint64_t)1 << 47);
		} else if (newval != 0) {
			break;
		}
		wrmsr_on_cpu(1552, 0, new_data);
		break;
	case 13:
		if (newval != 1 && newval != 0) {
			fprintf(stderr, "This value can only be set to 0 (disabled) or 1 (enabled).\n");
			return;
		}
		regdata = rdmsr_on_cpu(1552, 0);
		new_data = regdata & 0b1111111111111110111111111111111111111111111111111111111111111111;
		if (newval == 1) {
			new_data = new_data | ((uint64_t)1 << 48);
		} else if (newval != 0) {
			break;
		}
		wrmsr_on_cpu(1552, 0, new_data);
		break;
	case 15:
		regdata = rdmsr_on_cpu(418, 0);
		new_data = regdata & 0b1111111111111111111111111111111111000000111111111111111111111111;
		abs_temp_limit = ((regdata >> 16) & 0b011111111);
		newval = abs_temp_limit - newval;
		new_data = new_data | (newval << 24);
		wrmsr_on_cpu(418, 0, new_data);
		break;
	default:
		fprintf(stderr, "This functionality is not implemented.\n");
		break;
	}
}

uint64_t rdmsr_on_cpu(uint32_t reg, int cpu)
{
	uint64_t data;
	int fd;
	char msr_file_name[64];
	unsigned int bits;

	sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
	fd = open(msr_file_name, O_RDONLY);
	if (fd < 0) {
		if (errno == ENXIO) {
			fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
			exit(2);
		} else if (errno == EIO) {
			fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
				cpu);
			exit(3);
		} else {
			/* This msr is unreadable, so return -1 instead of quitting */
			return -1;
		}
	}

	if (pread(fd, &data, sizeof data, reg) != sizeof data) {
		if (errno == EIO) {
			fprintf(stderr, "CPU %d cannot read "
				"MSR 0x%08"PRIx32"\n",
				cpu, reg);
			return -1;
		} else {
			exit(127);
		}
	}

	close(fd);

	bits = highbit - lowbit + 1;
	if (bits < 64) {
		/* Show only part of register */
		data >>= lowbit;
		data &= (1ULL << bits) - 1;
	}

	return data;
}

void wrmsr_on_cpu(uint32_t reg, int cpu, uint64_t data)
{
	int fd;
	char msr_file_name[64];

	sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
	fd = open(msr_file_name, O_WRONLY);
	if (fd < 0) {
		if (errno == ENXIO) {
			fprintf(stderr, "wrmsr: No CPU %d\n", cpu);
			exit(2);
		} else if (errno == EIO) {
			fprintf(stderr, "wrmsr: CPU %d doesn't support MSRs\n",
				cpu);
			exit(3);
		} else {
			perror("wrmsr: open");
			exit(127);
		}
	}

	if (pwrite(fd, &data, sizeof data, reg) != sizeof data) {
		if (errno == EIO) {
			fprintf(stderr, "Your CPU does not accept this value on this setting. Please use another value.\n");
		} else {
			perror("wrmsr: pwrite");
			exit(127);
		}
	}

	close(fd);

	return;
}
