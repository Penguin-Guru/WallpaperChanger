#include <stdio.h>	// Remove later.
#include <string.h>
#include <assert.h>
#include "cli.h"
#include "init.h"
#include "argument.h"

namespace init {
	extern const parameter_t params_known[];
	extern const uint_fast8_t num_params_known;
	extern handler_set_list_t init_type_params, run_mode_params;

	namespace cli {

		static inline void print_invalid(const char* const term) {
			fprintf(stderr, "Invalid parameter: %s\n", term);
		}

		static inline void print_arg_mismatch(param_name name , const param_arg_ct expected, const param_arg_ct provided) {
			fprintf(stderr, "Missing expected term for flag/parameter: \"%s\"\n"
					"\tTerms expected: \"%hu\"\n"
					"\tTerms provided: \"%hu\"\n"
				,
				name,
				expected,
				provided
			);
		}


		const parameter_t *param_buff = nullptr;
		// Using the heap like this feel very inefficient, but I'm not sure of a better way to avoid arbitrary length limits.
		arg_list_t args_buff;	// Heap currently realloc'd to accommodate each new argument per parameter.

		static inline bool push_param_buff() {	// For parameters with arguments.
			bool ret = init::register_param(param_buff, &args_buff);
			reset_args_buffer(&args_buff);
			return ret;
		}
		static inline bool push_param_unbuff(const parameter_t *param = param_buff) {	// For parameters without arguments.
			bool ret = init::register_param(param, nullptr);
			if (args_buff.ct) {
				fprintf(stderr, "Cli: pushing unbuffered parameter, but the argument buffer was not empty.\n");
				free_args(&args_buff);
			}
			assert(args_buff.args == nullptr);
			return ret;
		}

		static bool match_short_param(short_flag_t arg) {
			for (param_ct i = 0; i < num_params_known; i++) {	// Support multi-character short flags-- for now.
				const parameter_t *check_param = &params_known[i];
				if (
					check_param->flag_pair.short_flag != NULL
					&& !strcmp(check_param->flag_pair.short_flag, arg)
				) {
					// Matched parameter.
					if ((terms_pending = check_param->handler_set.arg_list->ct) == 0) {
						// No need for push_param_buff, because there should be no arguments.
						return push_param_unbuff(check_param);
					}
					param_buff = check_param;
					if (args_buff.ct) {
						fprintf(stderr, "args_buff already initialised. Investigate this!\n");
						return false;
					}
					assert(args_buff.args == nullptr);
					return true;
				}
			}
			print_invalid(arg);
			return false;
		}
		static bool match_long_param(long_flag_t arg) {
			for (param_ct i = 0; i < num_params_known; i++) {
				const parameter_t *check_param = &params_known[i];
				if (
					check_param->flag_pair.long_flag != NULL
					&& !strcmp(check_param->flag_pair.long_flag, arg)
				) {
					// Matched parameter.
					if ((terms_pending = check_param->handler_set.arg_list->ct) == 0) {
						// No need for push_param_buff, because there should be no arguments.
						return push_param_unbuff(check_param);
					}
					param_buff = check_param;
					if (args_buff.ct) {
						fprintf(stderr, "args_buff already initialised. Investigate this!\n");
						return false;
					}
					assert(args_buff.args == nullptr);
					return true;
				}
			}
			print_invalid(arg);
			return false;
		}

		bool parse_params(int argc, char** argv) {
			assert(sizeof(params_known) > 0);
			for (unsigned short i = 1; i < argc; i++) {
				//argv++;
				auto *argvi = argv[i];
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
							init::free_param_args(num_args_buffered, args_buff);
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
						param_buff->handler_set.arg_list->ct,
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
					param_buff->handler_set.arg_list->ct,
					args_buff.ct
				);
				free_args(&args_buff);
				return false;
			}
			return true;
		}

	}
}

