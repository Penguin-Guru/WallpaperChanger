#pragma once

#include <stdint.h>
#include "parameters.h"
#include "argument.h"
#include "verbosity.h"

#define DEFAULT_CONFIG_FILE_NAME "wallpaper-changer.conf"
#define SPACE_AND_TAB " \t"
#define WHITESPACE_CHARACTERS SPACE_AND_TAB "\r\n\v\f"
#define KEY_VALUE_DELIMS "=:" WHITESPACE_CHARACTERS
#define VALUE_DELIMS ",;" WHITESPACE_CHARACTERS


extern handler_set_list_t run_mode_params;

extern uint_fast8_t num_config_files_loaded;


static inline parameter_t * match_param(flag_t * const flag);	// Defined later in this file.
static inline void print_invalid(const char* const term, const parameter_t * const param, const arg_list_t * const args_buff) {
	fprintf(stderr, "Invalid parameter: %s\n", term);
	{
		// Test whether the term would have been valid as a flag (long or short).
		flag_t would_be_flag = {.str = term, .type = FLAG_TYPE_UNKNOWN};
		const parameter_t * const would_be_param = match_param(&would_be_flag);
		if (would_be_param) {
			assert(
				would_be_flag.type == FLAG_TYPE_LONG
				|| would_be_flag.type == FLAG_TYPE_SHORT
			);
			fprintf(stderr,
				"\tIf you meant for this to be a flag invoking \"%s\", you must prefix it with %s.\n"
				, would_be_param->handler_set.name
				, (would_be_flag.type == FLAG_TYPE_LONG ? "two hyphens (\"--\")" : "one hyphen (\"-\")")
			);
			// If it would have been a valid flag, it probably wasn't intended as a term for the previous.
			return;
		}
	}
	if (param) {
		fprintf(stderr,
			"\tWas this supposed to be a term provided to the previous flag's operation (\"%s\")?\n"
			, param->handler_set.name
		);
		fprintf(stderr,
			"\t\tMaximum terms accepted for that operation: %hu\n"
			, param->arg_params.max
		);
		if (args_buff) {
			assert(args_buff->ct == param->arg_params.max);
			fprintf(stderr,
				"\t\tThis would be term number: %hu\n"
				, args_buff->ct + 1
			);
		}
	}
}
static inline void print_arg_mismatch(const param_name name, const param_arg_parameters_t arg_params, const param_arg_ct provided) {
	assert(provided > arg_params.max || provided < arg_params.min);

	const bool valance_was_excess = provided > arg_params.max;
	const char *valance_str;
	if (valance_was_excess) valance_str = "Excess";
	else valance_str = "Missing";

	if (arg_params.min == arg_params.max) {
		// One specific number of terms was expected; not a range.
		fprintf(stderr, "%s term(s) for flag/parameter: \"%s\"\n"
				"\tTerms expected: %hu\n"
				"\tTerms provided: %s%hu\n"
			, valance_str
			, name
			, arg_params.min
			, (valance_was_excess ? ">= " : "")	// Parsing ends on excess.
			, provided
		);
		return;
	}
	// The number of terms expected is a range.

	// Assemble expected range as string:
	static_assert(sizeof(arg_params.min)-2 < sizeof(int), "The int used here must be able to contain \"arg_params.min\".");
	static_assert(sizeof(arg_params.max)-2 < sizeof(int), "The int used here must be able to contain \"arg_params.max\".");
	constexpr char RangeDelim[] = {" -- "};
	const int min_str_len = snprintf(NULL, 0, "%d", arg_params.min);
	const int max_str_len = snprintf(NULL, 0, "%d", arg_params.max);
	// Subtracting two because we only buffer a single terminating null.
	char expectation_str[ min_str_len + sizeof(RangeDelim) + max_str_len - 2 ];
	{
		char *pos = expectation_str;
		if (snprintf(pos, min_str_len + 1, "%hu", arg_params.min) != min_str_len) return;
		pos += min_str_len;
		if (snprintf(pos, sizeof(RangeDelim), "%s", RangeDelim) != sizeof(RangeDelim) - 1) return;
		pos += sizeof(RangeDelim) - 1;
		if (snprintf(pos, max_str_len + 1, "%hu", arg_params.max) != max_str_len) return;
		assert(*(pos + max_str_len) == '\0');
	}

	fprintf(stderr, "%s term(s) for flag/parameter: \"%s\"\n"
			//"\tTerms expected: %hu -- %hu\n"
			//"\tTerms expected: %hu%s\n"
			"\tTerms expected: %s\n"
			"\tTerms provided: %s%hu\n"
		, valance_str
		, name
		, expectation_str
		, (valance_was_excess ? ">= " : "")	// Parsing ends on excess.
		, provided
	);
}
static inline bool test_arg_mismatch(const param_name name, const param_arg_parameters_t arg_params, const param_arg_ct provided) {
	if (!(provided > arg_params.max || provided < arg_params.min)) return false;
	print_arg_mismatch(name, arg_params, provided);
	return true;
}

static inline parameter_t * match_long_flag(const long_flag_t match_str) {
	for (
		parameter_t * check_param = params_known;
		check_param != params_known + num_params_known;
		check_param++
	) {
		if (
			check_param->flag_pair.long_flag != NULL
			&& !strcmp(check_param->flag_pair.long_flag, match_str)
		) return check_param;

	}
	return NULL;
}
static inline parameter_t * match_short_flag(const short_flag_t match_str) {
	for (
		parameter_t * check_param = params_known;
		check_param != params_known + num_params_known;
		check_param++
	) {
		if (
			check_param->flag_pair.short_flag != NULL
			&& !strcmp(check_param->flag_pair.short_flag, match_str)
		) return check_param;

	}
	return NULL;
}
static inline parameter_t * match_param(flag_t * const flag) {
	const flag_str_t match_str = flag->str;	// Avoid pointer dereferences that scale with num_params_known.
	switch (flag->type) {
		case FLAG_TYPE_UNKNOWN :
		case FLAG_TYPE_LONG :
			//if (!(flag->param = match_long_flag(flag->str))) return NULL;
			if ((flag->param = match_long_flag(flag->str))) {
				if (flag->type == FLAG_TYPE_UNKNOWN) flag->type = FLAG_TYPE_LONG;
				break;
			// If type was (and is still) unknown, flow into next case.
			} else if (flag->type != FLAG_TYPE_UNKNOWN) break;
		case FLAG_TYPE_SHORT :
			if (flag->param = match_short_flag(flag->str)) {
				if (flag->type == FLAG_TYPE_UNKNOWN) flag->type = FLAG_TYPE_SHORT;
			}
			break;	// This case was the last valid type for UNKNOWN to check.
		default:
			fprintf(stderr, "Invalid flag type. Probably a hardware glitch.\n");
			assert(flag->param == NULL);
		// No need for default case. Function end returns failure.
	}
	assert(
			flag->param != NULL && (
				flag->type == FLAG_TYPE_LONG
				|| flag->type == FLAG_TYPE_SHORT
			)
		||
			flag->param == NULL
			&& flag->type == FLAG_TYPE_UNKNOWN
	);
	return flag->param;	// May be NULL if initial case was FLAG_TYPE_UNKNOWN.
}


bool register_param(parameter_t *p, const arg_list_t * const al, const enum LoadSource load_source);

bool init(int argc, char** argv);

