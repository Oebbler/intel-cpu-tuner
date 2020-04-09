/*
 * main.c
 *
 *  Created on: 20.05.2019
 *      Author: Oebbler
 */

#include "msr-access.h"

#define VERSION_STRING "0.0.1-alpha6"

/* Number of MSR values stored in msr_values array */
#define MSR_VALS				6
/* define array positions of MSR values */
#define TURBO_RATIO_LIMIT		0
#define CACHE_RATIO_LIMIT		1
#define VOLTAGE					2
#define RAPL_POWER_UNIT			3
#define RAPL_POWER_LIMIT		4
#define TEMPERATURE_LIMIT		5
#define PLATFORM_POWER_LIMIT	6

/* This method checks if all conditions are met to start the program and loads the msr kernel module */
char setup();
/* stores the currently defined maximum turbo value in the defined global variable */
void get_turbo_vals(char **turbo_vals, uint64_t *msr_values);
/* This method frees memory and exits the program properly */
void exit_program(uint64_t **msr_values, int **value, int **input, char **turbo_vals);
/* This reads all needed values from the MSRs into the array */
void read_from_msrs(uint64_t **msr_values);
/* This is for parsing the command line arguments */
int parse_int(char *str, int argno);

int main(int argc, char *argv[]) {
	int arg1, arg2;
	int* input;
	int* value;
	uint64_t* msr_values;
	char* turbo_vals;

	/* Set up the program and exit on error */
	if (!setup()) {
		exit(3);
	}

	input = (int*) malloc(sizeof(int));
	value = (int*) malloc(sizeof(int));
	msr_values = (uint64_t*) malloc(sizeof(uint64_t) * MSR_VALS);
	turbo_vals = (char*) malloc(sizeof(char) * 4);
	if (input == NULL || value == NULL || msr_values == NULL || turbo_vals == NULL) {
		fprintf(stderr, "There is not enough memory to start the program. Exiting...\n");
		exit(1);
	}
	*input = -1;
	*value = -1;

	/* command line interface handling */
	if (argc == 3) {
		arg1 = parse_int(argv[1], 1);
		arg2 = parse_int(argv[2], 2);
		write_data(arg1, arg2);
		exit_program(&msr_values, &value, &input, &turbo_vals);
	} else if (argc != 1) {
		fprintf(stderr, "Usage: %s [optional function value]\nExample: %s 1 30 sets the 1 core turbo frequency to %d MHz.\n%s starts this program in an interactive mode.\n", argv[0], argv[0], 30*BCLK, argv[0]);
		exit_program(&msr_values, &value, &input, &turbo_vals);
	}

	/* main program; currently designed especially for i7-6820HK, but this works on other CPUs as well */
	for (;;) {

		printf("CPU tuning utility for Intel processors version ");
		printf(VERSION_STRING);
		printf("\nChoose an option to change or press 0 to exit.\n\n");

		read_from_msrs(&msr_values);

		/* turbo boost ratios */
		if (msr_values[TURBO_RATIO_LIMIT] == -1) {
			printf ("Core clock ratio settings are not supported on this platform.\n");
		} else {
			get_turbo_vals(&turbo_vals, msr_values);
			printf(" 1) Turbo frequency (1  core): %d MHz\n", (int) turbo_vals[0] * BCLK);
			printf(" 2) Turbo frequency (2 cores): %d MHz\n", (int) turbo_vals[1] * BCLK);
			printf(" 3) Turbo frequency (3 cores): %d MHz\n", (int) turbo_vals[2] * BCLK);
			printf(" 4) Turbo frequency (4 cores): %d MHz\n", (int) turbo_vals[3] * BCLK);
			printf("1234) Set turbo frequency for all cores\n");
		}

		/* cache ratio */
		if (msr_values[CACHE_RATIO_LIMIT] == -1) {
			printf ("Cache ratio settings are not supported on this platform.\n");
		} else {
			printf(" 5-6) Cache frequency (min/max): %d / %d MHz\n", (int)((msr_values[CACHE_RATIO_LIMIT] >> 8) & 0b011111111) * BCLK, (int)((msr_values[CACHE_RATIO_LIMIT] & 0b011111111) * BCLK));
		}

		/* current CPU voltage */
		if (msr_values[VOLTAGE] == -1) {
			printf("Voltage cannot be measured on this platform.\n");
		} else {
			printf(" 7) Current CPU voltage: %f V\n", (float)((msr_values[VOLTAGE] >> 32) & 0b01111111111111111) / (1 << 13));
		}

		/* read current power limit; not working at the moment, but in this way documented by Intel */
		/*uint64_t msr_values[POWER_LIMIT] = rdmsr_on_cpu(1628, 0);
		printf("Long Term Power Limit: %d\n", (int)(msr_values[POWER_LIMIT] & 0b0111111111111111));
		printf("Long Term Power Limit Enabled: %d\n", (int) (msr_values[POWER_LIMIT] >> 15) & 1);
		printf("Long Term Platform Clamping Limit Enabled: %d\n", (int) (msr_values[POWER_LIMIT] >> 16) & 1);
		printf("Long Term Power Limit time: %d\n", (int) (msr_values[POWER_LIMIT] >> 17) & 0b01111111);
		printf("Short Term Power Limit: %d\n", (int) (msr_values[POWER_LIMIT] >> 32) & 0b0111111111111111);
		printf("Short Term Power Limit Enabled: %d\n", (int) (msr_values[POWER_LIMIT] >> 47) & 1);
		printf("Short Term Platform Clamping Limit Enabled: %d\n", (int) (msr_values[POWER_LIMIT] >> 48) & 1);
		printf("Power Limit Settings blocked: %d\n", (int) (msr_values[POWER_LIMIT] >> 63) & 1);*/

		/* extract the currently only needed value from the power unit MSR */
		msr_values[RAPL_POWER_UNIT] = msr_values[RAPL_POWER_UNIT] & 0b01111;
		/*int energy_unit = (int)((raw_power(MSR 1542) >> 8) & 0b011111);*/
		/*int time_unit = (int)((raw_power(MSR 1542) >> 16) & 0b01111);*/

		/* power limit */
		if (msr_values[RAPL_POWER_LIMIT] == -1) {
			printf("power limit settings are not supported on this platform.\n");
		} else {
			if (((msr_values[RAPL_POWER_LIMIT] >> 63) & 1) == 1) {
				printf("Settings 8 to 13 are available for viewing only,\nbecause they are locked by the CPU or the BIOS.\n");
			}
			printf(" 8) Long Term Power Limit: %f W\n", (float)(msr_values[RAPL_POWER_LIMIT] & 0b0111111111111111) / (float) (1 << msr_values[RAPL_POWER_UNIT]));
			printf(" 9) Long Term Power Limit: %sabled\n", (int)((msr_values[RAPL_POWER_LIMIT] >> 15) & 1) == 1 ? "En" : "Dis");
			printf("10) Long Term Platform Clamping Limit: %sabled\n", (int)((msr_values[RAPL_POWER_LIMIT] >> 16) & 1) == 1 ? "En" : "Dis");
			/*printf("Long Term Power Limit time: %d\n", (int) ((1 << ((msr_values[POWER_LIMIT] >> 17) & 0b011111)) * (1.0 + ((msr_values[POWER_LIMIT] >> 54) & 0b011) / 4.0) * time_unit));*/
			printf("11) Short Term Power Limit: %f W\n", (float) ((msr_values[RAPL_POWER_LIMIT] >> 32) & 0b0111111111111111) / (float) (1 << msr_values[RAPL_POWER_UNIT]));
			printf("12) Short Term Power Limit: %sabled\n", ((msr_values[RAPL_POWER_LIMIT] >> 47) & 1) == 1 ? "En" : "Dis");
			printf("13) Short Term Platform Clamping Limit: %sabled\n", ((msr_values[RAPL_POWER_LIMIT] >> 48) & 1) == 1 ? "En" : "Dis");
			printf("14) Power Limit Settings status: %socked\n", ((msr_values[RAPL_POWER_LIMIT] >> 63) & 1) == 1 ? "L" : "Unl");
		}

		/* temperature limit */
		if (msr_values[TEMPERATURE_LIMIT] == -1) {
			printf("Temperature settings are not supported on this platform.\n");
		} else {
			char offset = (char) ((msr_values[TEMPERATURE_LIMIT] >> 24) & 0b0111111);
			printf("15) Temperature limit: %d °C\n", TEMP_LIMIT - offset);
		}
		printf(" 0) Exit program\n");

		/* read user input */
		printf("Which value should be changed (0 for exit)? ");
		scanf("%d", input);
		if (*input == 0) {
			exit_program(&msr_values, &value, &input, &turbo_vals);
		}
		/* if voltage / lockage should be set or any setting not in the list (input between 1 and 15 or 1234), don't write anything */
		if ((*input == 7 || *input == 14 || *input < 1 || *input > 15) && *input != 1234) {
			write_data(0, *input);
		}
		else {
			printf("Type new value: ");
			scanf("%d", value);
			write_data(*input, *value);
		}
	}
}

char setup() {
	int retval;
	/* check if executed as root and if SUID bit is set (security risk) */
	int uid = getuid();
	int euid = geteuid();
	if (euid != uid) {
		fprintf(stderr, "SUID bit is set. The program will not continue because this is a security risk.\nUnset the SUID bit to start the program.\n");
		return 0;
	} else if (uid != 0) {
		fprintf(stderr, "Please execute this program as root in order to access CPU data.\n");
		return 0;
	}
	/* load msr module from Linux */
	retval = system("modprobe msr");
	if (WEXITSTATUS(retval) != 0) {
		fprintf(stderr, "Module msr is needed for this program but cannot be loaded.\n");
		return 0;
	}
	/* initialize static CPU data as defined in msr-access.h */
	PLATFORM_INFO = rdmsr_on_cpu(206, 0);
	NOTURBO_MAX_CLK = (PLATFORM_INFO >> 8) & 0b011111111;
	NOTURBO_EFF_CLK = (PLATFORM_INFO >> 40) & 0b011111111;
	TEMP_LIMIT = (rdmsr_on_cpu(418, 0) >> 16) & 0b011111111;
	TURBO_WRITEABLE = (PLATFORM_INFO >> 28) & 1;
	return 1;
}

void get_turbo_vals(char **turbo_vals, uint64_t *msr_values) {
	int i;
	char* working_vals = *turbo_vals;
	if (turbo_vals == NULL || working_vals == NULL || msr_values == NULL) {
		fprintf(stderr, "Retrieving the turbo values failed. This should never happen.\n");
		exit(6);
	}
	CURR_MAX_TURBO = 0;
	for (i = 0; i < 4; i++) {
		/* turbo values for 1, 2, 3 and 4 cores */
		working_vals[i] = (msr_values[TURBO_RATIO_LIMIT] >> (i * 8)) & 0b011111111;
		/* maximum of all turbo values */
		if (working_vals[i] > CURR_MAX_TURBO) {
			CURR_MAX_TURBO = working_vals[i];
		}
	}
}

void exit_program(uint64_t **msr_values, int **value, int **input, char **turbo_vals) {
	int retval;
	/* free allocated memory and unload msr */
	free(*msr_values);
	free(*value);
	free(*input);
	free(*turbo_vals);
	retval = system("modprobe -r msr");
	/* If msr was not unloaded, this probably means msr is builtin */
	if (WEXITSTATUS(retval) != 0) {
		printf("Module msr was not successfully unloaded.\nThis is not necessarily a problem.\n");
	}
	exit(0);
}

void read_from_msrs(uint64_t **msr_values) {
	uint64_t* working_msr_values = *msr_values;
	if (msr_values == NULL || working_msr_values == NULL) {
		fprintf(stderr, "Error while initializing the array for reading in the MSR values. This should never happen.\n");
		exit(2);
	}
	working_msr_values[TURBO_RATIO_LIMIT] = rdmsr_on_cpu(429, 0);
	working_msr_values[CACHE_RATIO_LIMIT] = rdmsr_on_cpu(1568, 0);
	working_msr_values[VOLTAGE] = rdmsr_on_cpu(408, 0);
	working_msr_values[RAPL_POWER_UNIT] = rdmsr_on_cpu(1542, 0);
	working_msr_values[RAPL_POWER_LIMIT] = rdmsr_on_cpu(1552, 0);
	working_msr_values[TEMPERATURE_LIMIT] = rdmsr_on_cpu(418, 0);
	/* experimental */
	/*working_msr_values[PLATFORM_POWER_LIMIT] = rdmsr_on_cpu(1628, 0);*/
}

int parse_int(char *str, int argno) {
	int i;
	int success = 0;
	int result = 0;
	for (i = 0; i < 11; i++) {
		if (str[i] == 0) {
			success = 1;
			break;
		}
		if (str[i] < '0' || str[i] > '9') {
			break;
		}
		result *= 10;
		result += str[i] - '0';
	}
	if (success) {
		return result;
	} else {
		fprintf (stderr, "Argument %d is no valid integer.\n", argno);
		exit(1);
	}
}


