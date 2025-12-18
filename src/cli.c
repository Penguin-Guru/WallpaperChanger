#include <stdio.h>
#include <stdlib.h>     // For malloc.
#include <string.h>
#include <assert.h>
#include "cli.h"
#include "init.h"
#include "argument.h"


static inline bool push_param(parameter_t *param, arg_list_t * const args_buff) {
	assert(param);
	assert(args_buff);
	assert(	// Verify state of args_buff.
		(args_buff->ct == 0 && args_buff->args == NULL)
		|| (args_buff->ct > 0 && args_buff->args != NULL)
	);
	// Check whether term count is valid for the parameter.
	if (test_arg_mismatch(param->handler_set.name, param->arg_params, args_buff->ct)) {
		return false;
	}
	return register_param(param, (args_buff->ct ? args_buff : NULL), CLI);
}
static inline bool push_param_with_partial_terms(parameter_t *param, arg_list_t * const args_buff) {
	assert(args_buff);
	if (!param) return true;
	if (param->arg_params.max - args_buff->ct == 0) return true;
	assert(param);
	return push_param(param, args_buff);
}

static inline bool process_matched_param(parameter_t *param, arg_list_t * const args_buff) {
	assert(param);
	assert(args_buff);
	if (args_buff->ct > param->arg_params.max) {
		// Too many conjoined terms were provided.
		print_arg_mismatch(
			param->handler_set.name,
			param->arg_params,
			args_buff->ct
		);
		return false;
	}
	if (param->arg_params.max - args_buff->ct == 0) {
		// No further/non-conjoined terms are expected.
		const bool ret = push_param(param, args_buff);
		param = NULL;
		return ret;
	}
	return true;
}

bool add_arg_to_buffer(arg_list_t * const args_buff, const char * const arg) {
	assert(args_buff);
	assert(arg);
	const size_t len = strlen(arg);
	if (len == 0) {
		fprintf(stderr, "Encountered empty string argument. This is not currently supported.\n");
		return false;
	}

	char *arg_buff = (char*)malloc(len+1);
	memcpy(arg_buff, arg, len);
	arg_buff[len] = '\0';

	args_buff->args = (argument*)reallocarray(args_buff->args, args_buff->ct + 1, sizeof(argument));
	args_buff->args[args_buff->ct++] = arg_buff;

	return true;
}

static inline bool parse_conjoined_terms(flag_t * const flag, arg_list_t * const args_buff) {
	assert(flag);
	assert(args_buff);
	if (*flag->conjoined_terms == '\0') return true;	// String ends with a superfluous delimiter.

	const param_arg_ct * const max_terms = &flag->param->arg_params.max;

	// Iterate through conjoined terms delimited by any char from VALUE_DELIMS.
	assert(args_buff->ct == 0);
	char *buff = NULL, *saveptr;
	for(
		buff = strtok_r(flag->conjoined_terms, VALUE_DELIMS, &saveptr);
		buff != NULL;
		buff = strtok_r(NULL, VALUE_DELIMS, &saveptr)
	) {
		// Add the term to the argument buffer.
		if (!add_arg_to_buffer(args_buff, buff)) return false;
		// Fail if argument count exceeds max acceptable for the parameter.
		if (args_buff->ct > *max_terms) {
			print_arg_mismatch(
				flag->param->handler_set.name,
				flag->param->arg_params,
				args_buff->ct
			);
			return false;
		}
	}
	return true;	// Argument count validation is performed elsewhere.
}
bool parse_flag(const char * const argvi, flag_t * const flag, arg_list_t * const args_buff) {
	assert(argvi);
	assert(argvi[0] == '-');
	assert(flag);
	assert(args_buff);
	if (argvi[1] == '-') {  // Long form flag ("--").
		// If parameter is only two hyphens, respect convention to stop processing parameters.
		if (argvi[2] == '\0') return true;
		flag->str = argvi + 2;
		flag->type = FLAG_TYPE_LONG;
	} else {                // Short-form flag ("-").
		flag->str = argvi + 1;
		flag->type = FLAG_TYPE_SHORT;
	}

	reset_args_buffer(args_buff);

	// Identify first char in src_str matching any in KEY_VALUE_DELIMS.
	if ((flag->conjoined_terms = strpbrk(flag->str, KEY_VALUE_DELIMS))) {
		assert(*flag->conjoined_terms != '\0');
		// Add null to terminate src_str between flag string and conjoined terms
		*(flag->conjoined_terms++) = '\0';
	}
	// Parse param before conjoined terms, so we can limit by param's max terms.
	if (
		!(flag->param = match_param(flag))
		|| (
			// Parse conjoined terms if any were detected.
			flag->conjoined_terms
			&& !parse_conjoined_terms(flag, args_buff)
		)
	) return false;

	return true;
}

bool parse_params(int argc, char** argv) {
	parameter_t *param_buff = NULL;
	arg_list_t args_buff = {};

	const char *argvi, * const argv_end = argv[argc];
	while ((argvi = *(++argv)) != argv_end) {	// Intentionally skipping argv[0].
		assert(argvi);
		assert(*argvi != '\0');

		// Check whether the argument is a "flag" or a "term". By our definition, ...
		// 	"Flags" begin with (and are not only) a single hyphen.
		// 		They invoke functions.
		// 	"Terms" are everything else.
		// 		They are, with one exception, supplied as arguments to functions.
		// 		The one exception is a term of only and exactly two hyphens.
		// 			This is a conventional signal to positionally end C.L.I. parsing.
		if (argvi[0] != '-' || argvi[1] == '\0') {
			// We now know that the parameter is a "(non-conjoined) term".
			// Non-conjoined terms should be associated with a buffered parameter.
			// This includes cases where the entire argument is a single hyphen.

			// Do we expect another term for the buffered parameter?
			if (param_buff && param_buff->arg_params.max - args_buff.ct > 0) {
				assert(param_buff);	// A parameter should currently be buffered.
				if (!add_arg_to_buffer(&args_buff, argvi)) {
					return false;
				}
				if (param_buff->arg_params.max - args_buff.ct == 0) {
					if (!push_param(param_buff, &args_buff)) return false;
				}
				continue;
			}
			print_invalid(argvi, param_buff, &args_buff);
			return false;
		}
		// We now know that the parameter is a "flag".

		// Since it is not a "term" associated with a buffered parameter,
		// 	any parameters still buffered should be newly concluded.
		// Push any remaining buffer.
		if (!push_param_with_partial_terms(param_buff, &args_buff)) return false;

		// Now we parse the flag, including any conjoined term values.
		flag_t flag;
		if (!(
			parse_flag(argvi, &flag, &args_buff)
			&& process_matched_param((param_buff = flag.param), &args_buff)
		)) {
			fprintf(stderr, "Failed to parse flag: \"%s\"\n", argvi);
			return false;
		}
	}
	// Done parsing.
	// Push any remaining buffer.
	return push_param_with_partial_terms(param_buff, &args_buff);
}

