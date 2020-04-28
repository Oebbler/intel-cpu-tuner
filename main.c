/*
 * main.c
 *
 *  Created on: 20.05.2019
 *      Author: Oebbler
 */

#include "msr-access.h"

/* Number of MSR values stored in msr_values array */
#define MSR_VALS					7
/* define array positions of MSR values */
#define TURBO_RATIO_LIMIT			0
#define CACHE_RATIO_LIMIT			1
#define VOLTAGE						2
#define RAPL_POWER_UNIT				3
#define RAPL_PKG_POWER_LIMIT		4
#define RAPL_PLATFORM_POWER_LIMIT	5
#define TEMPERATURE_LIMIT			6
#define PLATFORM_POWER_LIMIT		7


#define VERSION_STRING "0.1.0-alpha6"

/* ASM-function: This method extracts the bit at bitno from a 64-bit integer plus the 7 bits before */
extern char get_byte(uint64_t input, char bitno);
/* ASM-function: This method extracts "upper downto lower" from a 64-bit integer */
extern uint64_t downto(uint64_t input, char upper, char lower);
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

	for (;;) {
		read_from_msrs(&msr_values);
		get_turbo_vals(&turbo_vals, msr_values);

		printf("CPU tuning utility for Intel processors version ");
		printf(VERSION_STRING);
		printf("\nChoose an option to change or press 0 to exit.\n\n");
		if (msr_values[TURBO_RATIO_LIMIT] == -1) {
			printf("Turbo frequency settings are not supported on your CPU.\n");
		} else {
			printf("1234) Set turbo frequency for all cores\n");
			printf("   1) Turbo frequency (1  core):                        %4d MHz\n", turbo_vals[0] * BCLK);
			printf("   2) Turbo frequency (2 cores):                        %4d MHz\n", turbo_vals[1] * BCLK);
			printf("   3) Turbo frequency (3 cores):                        %4d MHz\n", turbo_vals[2] * BCLK);
			printf("   4) Turbo frequency (4 cores):                        %4d MHz\n", turbo_vals[3] * BCLK);
		}
		if (msr_values[CACHE_RATIO_LIMIT] == -1) {
			printf("Cache frequency settings are not supported on your CPU.\n");
		} else {
			printf("   5) Minimum Cache frequency :                         %4d MHz\n", (char)downto(msr_values[CACHE_RATIO_LIMIT], 14, 8) * BCLK);
			printf("   6) Maximum Cache frequency :                         %4d MHz\n", (char)downto(msr_values[CACHE_RATIO_LIMIT], 6, 0) * BCLK);
		}
		if (msr_values[VOLTAGE] == -1) {
			printf("Voltage measuring is not supported on your CPU.\n");
		} else {
			printf("   7) Current CPU voltage:                               %.3f V\n", (float) (downto(msr_values[VOLTAGE], 47, 32) * (float) 1 / (1 << 13)));
		}
		if (msr_values[RAPL_PKG_POWER_LIMIT] == -1) {
			printf("Package Power limit settings are not supported on your CPU.\n");
		} else {
			printf("   8) Package Power Limit settings status:               %socked\n", get_bit(msr_values[RAPL_PKG_POWER_LIMIT], 63) ? "L" : "Unl");
			printf("   9) Long Term Package Power Limit Status:              %sabled\n", get_bit(msr_values[RAPL_PKG_POWER_LIMIT], 15) ? "En" : "Dis");
			printf("  10) Long Term Package Clamping Limitation:             %sabled\n", get_bit(msr_values[RAPL_PKG_POWER_LIMIT], 16) ? "En" : "Dis");
			printf("  11) Long Term Package Power Limit:                     %.3f W\n", downto(msr_values[RAPL_PKG_POWER_LIMIT], 14, 0) / (float) (1 << downto(msr_values[RAPL_POWER_UNIT], 3, 0)));
			printf("  12) Short Term Package Power Limit Status:             %sabled\n", get_bit(msr_values[RAPL_PKG_POWER_LIMIT], 47) ? "En" : "Dis");
			printf("  13) Short Term Package Package Clamping Limitation:    %sabled\n", get_bit(msr_values[RAPL_PKG_POWER_LIMIT], 48) ? "En" : "Dis");
			printf("  14) Short Term Package Power Limit:                    %.3f W\n", downto(msr_values[RAPL_PKG_POWER_LIMIT], 46, 32) / (float) (1 << downto(msr_values[RAPL_POWER_UNIT], 3, 0)));
		}
		if (msr_values[TEMPERATURE_LIMIT] == -1) {
			printf("Temperature settings are not supported on your CPU.\n");
		} else {
			printf("  15) Temperature Limit:                                    %d°C\n", TEMP_LIMIT - (int)downto(msr_values[TEMPERATURE_LIMIT], 29, 24));

		}
		if (msr_values[RAPL_PLATFORM_POWER_LIMIT] == -1) {
			printf("Platform Power limit settings are not supported by your CPU.\n");
		} else {
			printf("  16) Platform Power Limit settings status:              %socked\n", get_bit(msr_values[RAPL_PLATFORM_POWER_LIMIT], 63) ? "L" : "Unl");
			printf("  17) Long Term Platform Power Limit Status:             %sabled\n", get_bit(msr_values[RAPL_PLATFORM_POWER_LIMIT], 15) ? "En" : "Dis");
			printf("  18) Long Term Platform Clamping Limitation:            %sabled\n", get_bit(msr_values[RAPL_PLATFORM_POWER_LIMIT], 16) ? "En" : "Dis");
			printf("  19) Long Term Platform Power Limit:                    %.3f W\n", (float) (downto(msr_values[RAPL_PLATFORM_POWER_LIMIT], 14, 0) / (float) (1 << downto(msr_values[RAPL_POWER_UNIT], 3, 0))));
			printf("  20) Short Term Platform Power Limit Status:            %sabled\n", get_bit(msr_values[RAPL_PLATFORM_POWER_LIMIT], 47) ? "En" : "Dis");
			printf("  21) Short Term Platform Clamping Limitation:           %sabled\n", get_bit(msr_values[RAPL_PLATFORM_POWER_LIMIT], 48) ? "En" : "Dis");
			printf("  22) Short Term Platform Power Limit:                   %.3f W\n", (float) (downto(msr_values[RAPL_PLATFORM_POWER_LIMIT], 46, 32) / (float) (1 << downto(msr_values[RAPL_POWER_UNIT], 3, 0))));
		}

		printf("Which setting should be changed? [0-22, 1234] ");
		scanf("%d", input);
		if (*input == 0) {
			exit_program(&msr_values, &value, &input, &turbo_vals);
			return 0;
		}
		if (*input == 7 || *input == 8 || *input == 16 || ((*input < 1 || *input > 22) && *input != 1234)) {
			write_data(0, *input);
		} else {
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
	NOTURBO_MAX_CLK = get_byte(PLATFORM_INFO, 8);
	NOTURBO_EFF_CLK = get_byte(PLATFORM_INFO, 40);
	TEMP_LIMIT = get_byte(rdmsr_on_cpu(418, 0), 16);
	TURBO_WRITEABLE = get_bit(PLATFORM_INFO, 28);
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
	/* turbo values for 1, 2, 3 and 4 cores */
	working_vals[0] = (get_byte(msr_values[TURBO_RATIO_LIMIT], 0));
	working_vals[1] = (get_byte(msr_values[TURBO_RATIO_LIMIT], 8));
	working_vals[2] = (get_byte(msr_values[TURBO_RATIO_LIMIT], 16));
	working_vals[3] = (get_byte(msr_values[TURBO_RATIO_LIMIT], 24));
	for (i = 0; i < 4; i++) {
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
	working_msr_values[RAPL_PKG_POWER_LIMIT] = rdmsr_on_cpu(1552, 0);
	working_msr_values[RAPL_PLATFORM_POWER_LIMIT] = rdmsr_on_cpu(1628, 0);
	working_msr_values[TEMPERATURE_LIMIT] = rdmsr_on_cpu(418, 0);
}

int parse_int(char *str, int argno) {
	char i;
	char success = 0;
	char result = 0;
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
