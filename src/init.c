#include <string.h>
#include <assert.h>
#include <stdlib.h>     // For free.
#include <stdio.h>      // For fprintf.
#include "init.h"
#include "cli.h"
#include "config.h"
#include "libxdgbasedir.h"

handler_set_list_t run_mode_params;
uint_fast8_t num_config_files_loaded = 0;
app_components_t app_components = COMPONENT_NONE;


bool handle_config_file(const arg_list_t * const al) {
	assert(al != NULL);
	assert(al->ct > 0);
	assert(al->args[0] != NULL);
	assert(al->args[al->ct-1] != NULL);
	param_arg_ct remaining = al->ct;
	while (remaining-- > 0) {
		// Note: "remaining" now points to the file index, not one ahead.
		if (parse_config_file(al->args[remaining], false)) num_config_files_loaded++;
		else return false;
	}
	return true;
}

bool handle_print_help(const arg_list_t * const al) {
	// Bool return type only due to type of handler function pointer. Not used.
	assert(al == NULL);
	printf("Parameters:\n");
	for (param_ct i = 0; i < num_params_known; i++) {
		const parameter_t *param = &params_known[i];
		assert(param->handler_set.name != NULL);
		printf("\n\t%s\n", param->handler_set.name);
		if (param->handler_set.description != NULL)
			printf("\t\tDescription: %s\n", param->handler_set.description);

		if (param->arg_params.min == param->arg_params.max)
			printf("\t\tArguments expected: %hd\n", param->arg_params.min);
		else
			printf("\t\tArguments expected: %hd -- %hd\n",
				param->arg_params.min,
				param->arg_params.max
			);

		const flag_pair_t *flags = &param->flag_pair;
		if (flags->long_flag != NULL)
			printf("\t\tLong flag:  --%s\n", flags->long_flag);
		if (flags->short_flag != NULL)
			printf("\t\tShort flag: -%s\n", flags->short_flag);
	}
	printf("\n");
	exit(0);        // EXIT_SUCCESS.
}

static inline void free_params(handler_set_list_t *list) {
	for (param_ct p = 0; p < list->ct; p++) {
		free_args(list->hs[p]->arg_list);
	}
	free(list->hs);
}
bool register_param(parameter_t *p, const arg_list_t * const al, const enum LoadSource load_source) {
	assert(al == NULL || al->ct > 0);       // Don't pass around empty containers.
	assert(!(al == NULL && p->arg_params.min != 0));
	assert(
		al == NULL
		|| ((p->arg_params.min <= al->ct) && (al->ct <= p->arg_params.max))
	);

	// Sort parameters into their respective categories:
	handler_set_list_t *list = NULL;        // Not useful at the moment, but may be in the future.
	switch (p->type) {
		case RUN :
			list = &run_mode_params;
			if ((app_components & p->requirements) != p->requirements) app_components |= p->requirements;
			break;
		case INIT :
			assert(p->requirements == COMPONENT_NONE);
			// Init type parameters can be handled immediately.
			// 	This means they are positional.
			if (load_source == CONFIG_FILE && p->previous_load == CLI) {
				if (verbosity) printf("C.L.I. parameter over-riding config setting: \"%s\"\n", p->handler_set.name);
				return true;    // This is not a failure case.
			}
			if (! p->handler_set.fn(al)) {
				if (verbosity >= 2) printf("Handling init type parameter: \"%s\"\n", p->handler_set.name);      // Improve failure message clarity.
				// Caller invokes clean_up().
				return false;
			}
			p->previous_load = load_source;
			return true;
		default:
			fprintf(stderr, "Invalid parameter type for \"%s\".\n", p->flag_pair.long_flag);
			return false;
	}
	p->previous_load = load_source;

	// Resize list to accommodate the new handler_set:
	if (! (list->hs = (handler_set_t**)reallocarray(list->hs, list->ct + 1, sizeof(handler_set_t*)))) {
		fprintf(stderr, "Falied to allocate memory on heap!\n");
		return false;
	}

	// Allocate space on heap to store contents of the provided arg_list buffer.
	if (al) {
		if (! (p->handler_set.arg_list = malloc(sizeof(*al)))) {
			fprintf(stderr, "Failed to allocate memory on heap!\n");
			return false;
		}
		p->handler_set.arg_list->ct = al->ct;
		p->handler_set.arg_list->args = al->args;
	}

	// Load the handler_set into the list:
	list->hs[list->ct++] = &p->handler_set;

	return true;
}

static void clean_up_init() {
	free_params(&run_mode_params);
}

static inline const char* load_default_config_file() {
	char *config_dir = get_xdg_config_home();
	if (!config_dir) {
		return "Failed to identify config directory.";
	}
	const char *config_file_name = "/" DEFAULT_CONFIG_FILE_NAME;
	size_t name_len = strlen(config_file_name);
	size_t dir_len = config_dir == NULL ? 0 : strlen(config_dir);;
	if (dir_len == 0) {
		free(config_dir);
		return "Default config directory path is empty.";
	}
	char config_path[dir_len + name_len + 1];
	// That +1 may or may not be necessary.
	// For some reason, the terminating null seems to be counted in name_len. Something about synthetic pointers.

	memcpy(config_path, config_dir, dir_len);
	free(config_dir);

	memcpy(config_path + dir_len, config_file_name, name_len);
	config_path[dir_len + name_len] = '\0';

	file_path_t silly = config_path;
        /*
	 * arg_list_t this_is_stupid = {1, &silly};
	 * handle_config_file(&this_is_stupid);
        */
	parse_config_file(silly, true);

	return NULL;    // Success condition: no error message.
}

bool init(int argc, char** argv) {
	atexit(clean_up_init);
	const char *abort_msg = NULL;

	if (!parse_params(argc, argv)) {        // Parsed first, so config file path can be overridden.
		abort_msg = "Failed to parse command-line parameters.";
	}

	if (!abort_msg && num_config_files_loaded == 0) {       // Currently always true.
		// No need to pass pointer because all potential messages are literals.
		abort_msg = load_default_config_file();
	}

	if (abort_msg) {
		fprintf(stderr, "%s\nInitialisation failed.\n", abort_msg);
		return false;
	}
	return true;
}
