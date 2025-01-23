#include <string.h>
#include <assert.h>
#include "init.h"
#include "cli.h"
#include "config.h"

using namespace init;

namespace init {
	handler_set_list_t run_mode_params;
	static void clean_up();
}

bool init::handle_config_file(arg_list_t *al) {
	assert(al != nullptr);
	assert(al->ct > 0);
	assert(al->args[0] != nullptr);
	assert(al->args[al->ct-1] != nullptr);
	while (al->ct-- > 0) {
		// Note: al->ct now points to the file index, not one ahead.
		if (config::parse_file(al->args[al->ct])) num_config_files_loaded++;
		else return false;
	}
	return true;
}

bool init::handle_print_help(arg_list_t *al) {
	assert(al == nullptr);
	printf("Parameters:\n");
	for (param_ct i = 0; i < num_params_known; i++) {
		const parameter_t *param = &params_known[i];
		assert(param->handler_set.name != nullptr);
		printf("\n\t%s\n", param->handler_set.name);
		if (param->handler_set.description != nullptr)
			printf("\t\tDescription: %s\n", param->handler_set.description);

		printf("\t\tArguments expected: %hd\n", param->handler_set.arg_list->ct);

		const flag_pair_t *flags = &param->flag_pair;
		if (flags->long_flag != NULL)
			printf("\t\tLong flag:  --%s\n", flags->long_flag);
		if (flags->short_flag != NULL)
			printf("\t\tShort flag: -%s\n", flags->short_flag);
	}
	printf("\n");
	//return true;
	exit(0);
}

void init::free_params(handler_set_list_t *list) {
	for (param_ct p = 0; p < list->ct; p++) {
		free_args(list->hs[p]->arg_list);
	}
	free(list->hs);
}
bool init::register_param(const parameter_t *p, arg_list_t *al) {
	assert(al == nullptr || al->ct > 0);
	handler_set_list_t *list = nullptr;
	switch (p->type) {
		case ParamType::RUN :
			list = &run_mode_params;
			break;
		case ParamType::INIT :
			if (! p->handler_set.fn(al)) {
				if (verbosity >= 2) printf("Handling init type parameter: \"%s\"\n", p->handler_set.name);	// Improve failure message clarity.
				// Caller invokes clean_up().
				return false;
			}
			return true;
		default:
			fprintf(stderr, "Invalid parameter type for \"%s\".\n", p->flag_pair.long_flag);
			return false;
	}

	if (! (list->hs = (const handler_set_t**)reallocarray(list->hs, list->ct + 1, sizeof(handler_set_t*)))) {
		fprintf(stderr, "Falied to allocate memory on heap!\n");
		free_args(al);
		return false;
	}
	list->hs[list->ct++] = &p->handler_set;

	return true;
}

static void init::clean_up() {
	free_params(&run_mode_params);
}

//inline bool init::load_default_config_file() {
inline const char* init::load_default_config_file() {
//inline const char* init::load_default_config_file(const char* abort_msg) {
	char *config_dir = xdg::config::home();
	if (!config_dir) {
		/*fprintf(stderr, "Init: Failed to identify config directory.\n");
		return false;*/
		return "Failed to identify config directory.";
	}
	const char *config_file_name = "/" DEFAULT_CONFIG_FILE_NAME;
	//char *pos;
	size_t name_len = strlen(config_file_name);
	size_t dir_len = config_dir == nullptr ? 0 : strlen(config_dir);;
	if (dir_len == 0) {
		/*fprintf(stderr, "Init: Default config directory path is empty.\n");
		free(config_dir);
		return false;*/
		return "Default config directory path is empty.";
	}
	char config_path[dir_len + name_len + 1];
	// That +1 may or may not be necessary.
	// For some reason, the terminating null seems to be counted in name_len. Something about synthetic pointers.
	//stpcpy(config_path, config_dir);
	//strncat(config_path, 
	//pos = (char*)mempcpy(config_path, config_dir, dir_len);
	mempcpy(config_path, config_dir, dir_len);
	free(config_dir);

	//*pos++ = '/';
	/*pos = (char*)mempcpy(pos, config_file_name, name_len);
	*pos = '\0';*/
	//*((char*)mempcpy(pos, config_file_name, name_len)) = '\0';
	mempcpy(config_path + dir_len, config_file_name, name_len);
	config_path[dir_len + name_len] = '\0';
	//config::parse_file(config_path);
	//char *this_is_stupid = &config_path[0];
	file_path_t silly = config_path;
	arg_list_t this_is_stupid = {1, &silly};
	handle_config_file(&this_is_stupid);

	//return true;
	return nullptr;
}

bool init::init(int argc, char** argv) {
//int_fast8_t init::init(int argc, char** argv) {
	atexit(clean_up);
	//bool proceed = true;
	const char *abort_msg = nullptr;

	if (!cli::parse_params(argc, argv)) {	// Parsed first, so config file path can be overridden.
		//fprintf(stderr, "Init: Failed to parse command-line parameters.\n");
		//proceed = false;
		abort_msg = "Failed to parse command-line parameters.";
	}

	//if (proceed && quit_after_parsing_cli) {
	//if (!abort_msg && quit_after_parsing_cli) {

	// Note:
	// 	Config files must be handled before the handler loop (below).
	// 	This currently makes C.L.I. control of config file loading impossible.
	// 	Fix later.
	//if (proceed && num_config_files_loaded == 0) {	// Currently always true.
	if (!abort_msg && num_config_files_loaded == 0) {	// Currently always true.
		//if (!load_default_config_file) proceed = false;
		abort_msg = load_default_config_file();
		//load_default_config_file(abort_msg);
	}

	/*if (proceed) return true;
	if (abort_msg) printf("%s", abort_msg);*/
	if (abort_msg) {
		fprintf(stderr, "%s\nInitialisation failed.\n", abort_msg);
		return false;
	}
	return true;
}
