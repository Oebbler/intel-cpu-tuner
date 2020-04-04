/*
 * xtu.c
 *
 *  Created on: 20.05.2019
 *      Author: Oebbler
 */

#include "msr-access.h"

#define VERSION_STRING "0.0.1-alpha3"

/* currently no known way to read out BCLK so this is statically set */
#define BCLK 100

/* This method frees memory and exits the program properly. */
void exit_program(int **value, int **input, int retval);
/* This is for parsing the command line arguments */
int parse_int(char *str, int argno);

int main(int argc, char *argv[]) {
	int arg1, arg2;
	uint64_t turbo_modes_raw;
	uint64_t cache_ratio_raw;
	uint64_t current_voltage_raw;
	uint64_t power_unit_raw;
	int power_unit;
	uint64_t power_limit_raw;
	uint64_t temp_limit_raw;
	uid_t uid, euid;
	int retval;
	int* input = malloc(sizeof(int));
	int* value = malloc(sizeof(int));

	if (input == NULL || value == NULL) {
		fprintf(stderr, "There is not enough memory to start the program. Exiting...\n");
		exit(1);
	}
	*input = -1;
	*value = -1;
	uid = getuid();
	euid = geteuid();

	/* perform some checks to ensure correct functionality */
	if (euid != uid) {
		fprintf(stderr, "SUID bit is set. The program will not continue because this is a security risk.\nUnset the SUID bit to start the program.\n");
		exit(1);
	} else if (uid != 0) {
		fprintf(stderr, "Please execute this program as root in order to access CPU data.\n");
		exit(1);
	}
	/* load msr module from Linux */
	retval = system("modprobe msr");
	if (WEXITSTATUS(retval) != 0) {
		fprintf(stderr, "Module msr is needed for this program but cannot be loaded.\n");
		exit(127);
	}
	/* command line interface handling */
	if (argc == 3) {
		arg1 = parse_int(argv[1], 1);
		arg2 = parse_int(argv[2], 2);
		write_data(arg1, arg2);
		exit_program(&value, &input, retval);
	} else if (argc != 1) {
		fprintf(stderr, "Usage: %s [function value]\nExample: %s 1 30 sets the 1 core turbo frequency to %d MHz.\n", argv[0], argv[0], 30*BCLK);
		exit_program(&value, &input, retval);
	}

	/* main program; currently designed especially for i7-6820HK, but this works on other CPUs as well */
	for (;;) {

		printf("CPU tuning utility for Intel processors version ");
		printf(VERSION_STRING);
		printf("\nChoose an option to change or press 0 to exit.\n\n");
		/* read turbo boost ratios */
		turbo_modes_raw = rdmsr_on_cpu(429, 0);
		if (turbo_modes_raw == -1) {
			printf ("Core clock ratio settings are not supported on this platform.\n");
		} else {
			printf(" 1) Turbo frequency (1  core): %d MHz\n", (int)(turbo_modes_raw & 0b011111111) * BCLK);
			printf(" 2) Turbo frequency (2 cores): %d MHz\n", (int)((turbo_modes_raw >> 8) & 0b011111111) * BCLK);
			printf(" 3) Turbo frequency (3 cores): %d MHz\n", (int)((turbo_modes_raw >> 16) & 0b011111111) * BCLK);
			printf(" 4) Turbo frequency (4 cores): %d MHz\n", (int)((turbo_modes_raw >> 24) & 0b011111111) * BCLK);
			printf("1234) Set turbo frequency for all cores\n");
		}

		/* read current cache ratio */
		cache_ratio_raw = rdmsr_on_cpu(1568, 0);
		if (cache_ratio_raw == -1) {
			printf ("Cache ratio settings are not supported on this platform.\n");
		} else {
			printf(" 5-6) Cache frequency (min/max): %d / %d MHz\n", (int)((cache_ratio_raw >> 8) & 0b011111111) * BCLK, (int)((cache_ratio_raw & 0b011111111) * BCLK));
		}

		/* read current CPU voltage (read only) */
		current_voltage_raw = rdmsr_on_cpu(408, 0);
		if (current_voltage_raw == -1) {
			printf("Voltage cannot be measured on this platform.\n");
		} else {
			printf(" 7) Current CPU voltage: %f V\n", (float)((current_voltage_raw >> 32) & 0b01111111111111111) / (1 << 13));
		}

		/* read current power limit; not working at the moment, but in this way documented by Intel */
		/*uint64_t power_limit_raw = rdmsr_on_cpu(1628, 0);
		printf("Long Term Power Limit: %d\n", (int)(power_limit_raw & 0b0111111111111111));
		printf("Long Term Power Limit Enabled: %d\n", (int) (power_limit_raw >> 15) & 1);
		printf("Long Term Platform Clamping Limit Enabled: %d\n", (int) (power_limit_raw >> 16) & 1);
		printf("Long Term Power Limit time: %d\n", (int) (power_limit_raw >> 17) & 0b01111111);
		printf("Short Term Power Limit: %d\n", (int) (power_limit_raw >> 32) & 0b0111111111111111);
		printf("Short Term Power Limit Enabled: %d\n", (int) (power_limit_raw >> 47) & 1);
		printf("Short Term Platform Clamping Limit Enabled: %d\n", (int) (power_limit_raw >> 48) & 1);
		printf("Power Limit Settings blocked: %d\n", (int) (power_limit_raw >> 63) & 1);*/

		/* read RAPL power unit */
		power_unit_raw = rdmsr_on_cpu(1542, 0);
		power_unit = (int)((power_unit_raw) & 0b01111);
		/*int energy_unit = (int)((power_unit_raw >> 8) & 0b011111);*/
		/*int time_unit = (int)((power_unit_raw >> 16) & 0b01111);*/

		/* read current power limit */
		power_limit_raw = rdmsr_on_cpu(1552, 0);
		if (power_limit_raw == -1) {
			printf("power limit settings are not supported on this platform.\n");
		} else {
			printf(" 8) Long Term Power Limit: %f W\n", (float)(power_limit_raw & 0b0111111111111111) / (float) (1 << power_unit));
			printf(" 9) Long Term Power Limit: %sabled\n", (int)((power_limit_raw >> 15) & 1) == 1 ? "En" : "Dis");
			printf("10) Long Term Platform Clamping Limit: %sabled\n", (int)((power_limit_raw >> 16) & 1) == 1 ? "En" : "Dis");
			/*printf("Long Term Power Limit time: %d\n", (int) ((1 << ((power_limit_raw >> 17) & 0b011111)) * (1.0 + ((power_limit_raw >> 54) & 0b011) / 4.0) * time_unit));*/
			printf("11) Short Term Power Limit: %f W\n", (float) ((power_limit_raw >> 32) & 0b0111111111111111) / (float) (1 << power_unit));
			printf("12) Short Term Power Limit: %sabled\n", ((power_limit_raw >> 47) & 1) == 1 ? "En" : "Dis");
			printf("13) Short Term Platform Clamping Limit: %sabled\n", ((power_limit_raw >> 48) & 1) == 1 ? "En" : "Dis");
			printf("14) Power Limit Settings status: %socked\n", ((power_limit_raw >> 63) & 1) == 1 ? "L" : "Unl");
		}

		/* read current temperature limit (minimum 37°C statically set, as on i7-6820HK) */
		temp_limit_raw = rdmsr_on_cpu(418, 0);
		if (temp_limit_raw == -1) {
			printf("Temperature settings are not supported on this platform.\n");
		} else {
			char abs_temp_limit = (char) ((temp_limit_raw >> 16) & 0b011111111);
			char offset = (char) ((temp_limit_raw >> 24) & 0b0111111);
			printf("15) Temperature limit: %d °C\n", abs_temp_limit - offset);
		}
		printf(" 0) Exit program\n");

		/* read user input */
		printf("Which value should be changed (0 for exit)? ");
		scanf("%d", input);
		if (*input == 0) {
			exit_program(&value, &input, retval);
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

void exit_program(int **value, int **input, int retval) {
	/* free allocated memory and unload msr */
	free(*value);
	free(*input);
	retval = system("modprobe -r msr");
	/* If msr was not unloaded, this probably means msr is builtin */
	if (WEXITSTATUS(retval) != 0) {
		printf("Module msr was not successfully unloaded.\nThis is not necessarily a problem.\n");
	}
	exit(0);
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


