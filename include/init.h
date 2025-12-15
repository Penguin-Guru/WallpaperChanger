#pragma once

#include <stdint.h>
#include "parameters.h"
#include "argument.h"
#include "verbosity.h"

#define DEFAULT_CONFIG_FILE_NAME "wallpaper-changer.conf"


extern handler_set_list_t run_mode_params;

extern uint_fast8_t num_config_files_loaded;


static inline void print_invalid(const char* const term) {
	fprintf(stderr, "Invalid parameter: %s\n", term);
}
static inline void print_arg_mismatch(const param_name name, const param_arg_parameters_t arg_params, const param_arg_ct provided) {
	if (arg_params.min == arg_params.max) {
		fprintf(stderr, "Missing expected term(s) for flag/parameter: \"%s\"\n"
				"\tTerms expected: %hu\n"
				"\tTerms provided: %hu\n"
			,
			name,
			arg_params.min,
			provided
		);
	} else {
		fprintf(stderr, "Missing expected term for flag/parameter: \"%s\"\n"
				"\tTerms expected: %hu -- %hu\n"
				"\tTerms provided: %hu\n"
			,
			name,
			arg_params.min,
			arg_params.max,
			provided
		);
	}
}
static inline parameter_t * match_short_param(const short_flag_t arg) {
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
static inline parameter_t * match_long_param(const long_flag_t arg) {
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


bool register_param(parameter_t *p, const arg_list_t * const al, const enum LoadSource load_source);
void free_param_args(param_arg_ct num_args, argument *args);
void free_params(handler_set_list_t *list);

bool init(int argc, char** argv);

