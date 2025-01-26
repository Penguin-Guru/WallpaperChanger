
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>	// For free.
#include <stdio.h>	// For fprintf.
#include <assert.h>
#include <math.h>	// For powf.
#include "config.h"
#include "init.h"
#include "verbosity.h"

#define WHITESPACE_CHARACTERS " \t\r\n\v\f"
#define KEY_VALUE_DELIMS "=:" WHITESPACE_CHARACTERS
#define VALUE_DELIMS ",;" WHITESPACE_CHARACTERS


bool parse_line(char *line) {
	char *op;
	if (op = strpbrk(line, KEY_VALUE_DELIMS)) {
		char label[op - line + 1];
		strlcpy(label, line, op - line + 1);

		// Identify the operation/parameter (label).
		parameter_t *param = NULL;
		for (param_ct i = 0; i < num_params_known; i++) {
			if (!strcmp(label, params_known[i].flag_pair.long_flag)) {
				param = &params_known[i];
				break;
			}
		}
		if (!param) return false;

		// Now we know how many terms/arguments the operation/parameter expects.
		param_arg_ct etc = param->arg_params.max;
		argument args[etc] = {};

		// Parse the terms/arguments.
		char *buff = NULL, *saveptr;
		param_arg_ct arg_ct = 0;
		for(
			buff = strtok_r(op+1, VALUE_DELIMS, &saveptr)
			;buff != NULL && arg_ct <= etc;
			buff = strtok_r(NULL, VALUE_DELIMS, &saveptr)
		) {
			// Skip key/value delims (that aren't whitespace) in case they were not separated by whitespace.
			static_assert(
				((unsigned short)floorf(powf(2,(sizeof(uint_fast8_t)*8)))) >= MAX_COLUMN_LENGTH,
				"skip data type must be sized to fit potential column length"
			);
			uint_fast8_t skip = strspn(buff, KEY_VALUE_DELIMS);
			if (strlen(buff) == skip) continue;
			args[arg_ct++] = buff + skip;
		}
		if (arg_ct < param->arg_params.min || arg_ct > etc) {
			fprintf(stderr,
				"Config: Invalid number of arguments for parameter \"%s\" (\"%s\").\n"
					"\tFound: %hu\n"
					"\tExpected: %hu\n"
				,
				param->flag_pair.long_flag, param->handler_set.name,
				arg_ct,
				etc
			);
			return false;
		}

		// Register the operation/parameter for execution.
		arg_list_t al = {
			.ct=arg_ct,
			.args=args
		};
		return register_param(param, &al, CONFIG_FILE);
	}
	// Unary operations/parameters are not currently supported.
	return false;
}

bool parse_file(const file_path_t file_path) {
	// Not currently offering to create the file.

	/*if (!access(file_path, F_OK)) {
		fprintf(stderr, "Config file not accessible: \"%s\"\n", file_path);
		return false;
	}*/

	struct stat info;
	if (stat(file_path, &info)) {
		fprintf(stderr, "Failed to read config file: \"%s\"\n", file_path);
		return false;
	}
	if (! (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode) )) {
		fprintf(stderr, "Supposed config file is not a regular file or symlink: \"%s\"\n", file_path);
		return false;
	}
	// Consider seeking default file name if non-default directory is specified without a file name.


	FILE *f = fopen(file_path, "r");
	if (!f){
		fprintf(stderr, "Failed to open config file: \"%s\"\n", file_path);
		return false;
	}
	fseek(f,0,SEEK_END); long len = ftell(f); fseek(f,0,SEEK_SET);
	if (len == -1) {
		fclose(f);
		return false;
	}
	
	if (verbosity > 1) printf("Loading config file: \"%s\"\n", file_path);

	char *line = NULL;
	size_t size = 0;
	while (getline(&line, &size, f) > 0) {
		if (line[0] == '#') continue;	// Ignore commented lines.
		if (!parse_line(line)) {
			fprintf(stderr, "Discontinuing the config parse.\n");
			fclose(f);
			return false;
		}
	}
	free(line);

	fclose(f);
	return true;
}
