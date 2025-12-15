#include <stdio.h>
#include <stdlib.h>     // For malloc.
#include <string.h>
#include <assert.h>
#include "cli.h"
#include "init.h"
#include "argument.h"


parameter_t *param_buff = NULL;
arg_list_t args_buff = {};

static inline bool push_param_buff() {  // For parameters with arguments.
	assert(param_buff);
	bool ret = register_param(param_buff, &args_buff, CLI);
	reset_args_buffer(&args_buff);
	return ret;
}
static inline bool push_param_unbuff(parameter_t *param) {      // For parameters without arguments.
	if (param == NULL) param = param_buff;
	assert(param);
	bool ret = register_param(param, NULL, CLI);
	if (args_buff.ct) {
		fprintf(stderr, "Cli: pushing unbuffered parameter, but the argument buffer was not empty.\n");
		free_args(&args_buff);
	}
	assert(args_buff.args == NULL);
	return ret;
}
static inline bool push_param(parameter_t *param) {
	if (param == NULL) param = param_buff;
	assert(param);
	if (args_buff.ct == 0) return push_param_unbuff(param);
	return push_param_buff();
}
static inline bool push_param_if_terms_pending(const uint_fast8_t terms_pending, parameter_t *param) {
	if (terms_pending == 0) return true;
	if (args_buff.ct < param_buff->arg_params.min) {
		// A flag was not expected. Missing term(s).
		print_arg_mismatch(
			param_buff->handler_set.name,
			param_buff->arg_params,
			args_buff.ct
		);
		free_args(&args_buff);
		return false;
	}
	// Minimum acceptable number of terms for previous flag have been provided.
	if (!push_param(param_buff)) return false;
}

static inline bool process_matched_param(parameter_t *param) {
	assert(param);
	if ((terms_pending = param->arg_params.max) == 0) {
		// No need for push_param_buff, because there should be no arguments.
		bool ret = push_param_unbuff(param);
		param = NULL;
		return ret;
	}
	if (args_buff.ct) {
		fprintf(stderr, "args_buff already initialised. Investigate this!\n");
		return false;
	}
	assert(args_buff.args == NULL);
	return true;
}

static parameter_t * match_short_param(const short_flag_t arg) {
	// Support multi-character short flags (for now).
	for (
		parameter_t * check_param = params_known;
		check_param != params_known + num_params_known;
		check_param++
	) {
		if (
			check_param->flag_pair.short_flag != NULL
			&& !strcmp(check_param->flag_pair.short_flag, arg)
		) return check_param;
	}
	print_invalid(arg);
	return NULL;
}
static parameter_t * match_long_param(const long_flag_t arg) {
	for (
		parameter_t * check_param = params_known;
		check_param != params_known + num_params_known;
		check_param++
	) {
		if (
			check_param->flag_pair.long_flag != NULL
			&& !strcmp(check_param->flag_pair.long_flag, arg)
		) return check_param;
	}
	print_invalid(arg);
	return NULL;
}

bool parse_params(int argc, char** argv) {
	for (unsigned short i = 1; i < argc; i++) {
		//argv++;
		assert(argv[i]);
		assert(argv[i][0] != '\0');
		char *argvi = argv[i];
		if (argvi[0] != '-' || argvi[1] == '\0') {
			// The parameter is not a flag, it's a "term".
			// This includes cases where the entire argument is a single hyphen.
			if (terms_pending) {
				if (!param_buff) {
					fprintf(stderr, "C.L.I. term(s) pending and detected, but list is empty!");
					return false;
				}
				size_t len = strlen(argvi);
				if (len == 0) {
					fprintf(stderr, "Ignoring empty string parameter (%hu).\n", i);
					continue;
				}

				char *arg_buff = (char*)malloc(len+1);
				memcpy(arg_buff, argvi, len);
				arg_buff[len] = '\0';

				args_buff.args = (argument*)reallocarray(args_buff.args, args_buff.ct + 1, sizeof(argument));
				args_buff.args[args_buff.ct++] = arg_buff;

				if (--terms_pending <= 0) {
					if (!push_param_buff()) return false;
				}
				continue;
			}
			print_invalid(argvi);
			free_args(&args_buff);
			return false;
		}
		// We now know that the parameter is a flag-- it begins with (and is not only) a single hyphen.
		push_param_if_terms_pending(terms_pending, param_buff);
		if (argvi[1] == '-') {  // Long form flag ("--").
			// If parameter is only two hyphens, respect convention to stop processing parameters.
			if (argvi[2] == '\0') return true;

			assert(argvi[3] != '\0');
			long_flag_t arg = argvi+2;
			if (!(param_buff = match_long_param(arg))) {
				free_args(&args_buff);
				return false;
			}
		} else {        // Short-form flag ("-").
			short_flag_t arg = argvi+1;
			if (!(param_buff = match_short_param(arg))) {
				free_args(&args_buff);
				return false;
			}
		}
		if (!process_matched_param(param_buff)) {
			free_args(&args_buff);
			return false;
		}
	}
	// Done parsing.
	push_param_if_terms_pending(terms_pending, param_buff);
	return true;
}

