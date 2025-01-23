#pragma once

#include <stdint.h>
#include "libxdgbasedir.h"
#include "parameters.h"
#include "argument.h"
#include "verbosity.h"

#define DEFAULT_CONFIG_FILE_NAME "wallpaper-changer.conf"


namespace init {
	static const uint_fast8_t num_params_known = sizeof(params_known)/sizeof(params_known[0]);

	extern handler_set_list_t run_mode_params;


	static uint_fast8_t num_config_files_loaded = 0;


	bool register_param(const parameter_t *p, arg_list_t *al);
	void free_param_args(param_arg_ct num_args, argument *args);
	void free_params(handler_set_list_t *list);

	inline const char* load_default_config_file();
	bool init(int argc, char** argv);
}

