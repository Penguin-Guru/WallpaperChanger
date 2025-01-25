#include <stdio.h>	// Remove later.
#include <stdlib.h>	// For malloc.
#include <string.h>
#include <assert.h>
#include "cli.h"
#include "init.h"
#include "argument.h"


static inline void print_invalid(const char* const term) {
	fprintf(stderr, "Invalid parameter: %s\n", term);
}

static inline void print_arg_mismatch(param_name name, const param_arg_parameters_t arg_params, const param_arg_ct provided) {
	if (arg_params.min == arg_params.max) {
		fprintf(stderr, "Missing expected term for flag/parameter: \"%s\"\n"
				"\tTerms expected: \"%hu\"\n"
				"\tTerms provided: \"%hu\"\n"
			,
			name,
			arg_params.min,
			provided
		);
	} else {
		fprintf(stderr, "Missing expected term for flag/parameter: \"%s\"\n"
				"\tTerms expected: \"%hu -- %hu\"\n"
				"\tTerms provided: \"%hu\"\n"
			,
			name,
			arg_params.min,
			arg_params.max,
			provided
		);
	}
}


parameter_t *param_buff = NULL;
// Using the heap like this feel very inefficient, but I'm not sure of a better way to avoid arbitrary length limits.
arg_list_t args_buff = {};	// Heap currently realloc'd to accommodate each new argument per parameter.

static inline bool push_param_buff() {	// For parameters with arguments.
	bool ret = register_param(param_buff, &args_buff, CLI);
	reset_args_buffer(&args_buff);
	return ret;
}
static inline bool push_param_unbuff(parameter_t *param) {	// For parameters without arguments.
	if (param == NULL) param = param_buff;
	bool ret = register_param(param, NULL, CLI);
	if (args_buff.ct) {
		fprintf(stderr, "Cli: pushing unbuffered parameter, but the argument buffer was not empty.\n");
		free_args(&args_buff);
	}
	assert(args_buff.args == NULL);
	return ret;
}

static bool match_short_param(short_flag_t arg) {
	for (param_ct i = 0; i < num_params_known; i++) {	// Support multi-character short flags-- for now.
		parameter_t * const check_param = &params_known[i];
		if (
			check_param->flag_pair.short_flag != NULL
			&& !strcmp(check_param->flag_pair.short_flag, arg)
		) {
			// Matched parameter.
			if ((terms_pending = check_param->arg_params.max) == 0) {
				// No need for push_param_buff, because there should be no arguments.
				return push_param_unbuff(check_param);
			}
			param_buff = check_param;
			if (args_buff.ct) {
				fprintf(stderr, "args_buff already initialised. Investigate this!\n");
				return false;
			}
			assert(args_buff.args == NULL);
			return true;
		}
	}
	print_invalid(arg);
	return false;
}
static bool match_long_param(long_flag_t arg) {
	for (param_ct i = 0; i < num_params_known; i++) {
		parameter_t * const check_param = &params_known[i];
		if (
			check_param->flag_pair.long_flag != NULL
			&& !strcmp(check_param->flag_pair.long_flag, arg)
		) {
			// Matched parameter.
			if ((terms_pending = check_param->arg_params.max) == 0) {
				// No need for push_param_buff, because there should be no arguments.
				return push_param_unbuff(check_param);
			}
			param_buff = check_param;
			if (args_buff.ct) {
				fprintf(stderr, "args_buff already initialised. Investigate this!\n");
				return false;
			}
			assert(args_buff.args == NULL);
			return true;
		}
	}
	print_invalid(arg);
	return false;
}

bool parse_params(int argc, char** argv) {
	for (unsigned short i = 1; i < argc; i++) {
		//argv++;
		char *argvi = argv[i];
		if (argvi[0] != '-') {
			// The parameter is not a flag, it's a "term".
			if (terms_pending) {
				if (!param_buff) {
					fprintf(stderr, "C.L.I. term(s) pending and detected, but list is empty!");
					return false;
				}
				size_t len = strlen(argvi);
				// Assuming len > 0. Is this safe?

				char *arg_buff = (char*)malloc(len+1);
				/*char *pos;
				if (! (pos = mempcpy(arg_buff, argvi, len))) {
					fprintf(stderr, "Invalid mempcpy. Something has gone terribly wrong.\n");
					free_param_args(num_args_buffered, args_buff);
					return false;
				}
				*pos = '\0';*/
				*((char*)mempcpy(arg_buff, argvi, len)) = '\0';

				args_buff.args = (argument*)reallocarray(args_buff.args, args_buff.ct + 1, sizeof(argument));
				args_buff.args[args_buff.ct++] = arg_buff;

				if (--terms_pending <= 0) {
					if (!push_param_buff()) return false;
				}
				continue;
			} else {
				print_invalid(argvi);
				free_args(&args_buff);
				return false;
			}
		}
		// We now know that the parameter is a flag-- it begins with a '-' character.
		if (terms_pending) {	// It would be nice if this could be moved to init, so it could also be used when parsing config files.
			// A flag was not expected.
			print_arg_mismatch(
				param_buff->handler_set.name,
				param_buff->arg_params,
				args_buff.ct
			);
			free_args(&args_buff);
			return false;
		}
		if (argvi[1] == '-') {	// Long form flag ("--").
			if (argvi[2] == 0) return true;	// Parameter is only two hyphens-- respect convention to stop processing parameters.
			long_flag_t arg = strdup(argvi+2);
			if (!match_long_param(arg)) {
				free(arg);
				free_args(&args_buff);
				return false;
			}
			free(arg);
		} else {	// Short-form flag ("-").
			short_flag_t arg = strdup(argvi+1);
			if (!match_short_param(arg)) {
				free(arg);
				free_args(&args_buff);
				return false;
			}
			free(arg);
		}
	}
	// Done parsing.
	if (terms_pending) {	// It would be nice if this could be moved to init, so it could also be used when parsing config files.
		// A flag was not expected.
		print_arg_mismatch(
			param_buff->handler_set.name,
			param_buff->arg_params,
			args_buff.ct
		);
		free_args(&args_buff);
		return false;
	}
	return true;
}

