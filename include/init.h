#pragma once

#include <stdint.h>
#include "parameters.h"
#include "argument.h"
#include "verbosity.h"

#define DEFAULT_CONFIG_FILE_NAME "wallpaper-changer.conf"


extern handler_set_list_t run_mode_params;


extern uint_fast8_t num_config_files_loaded;


bool register_param(parameter_t *p, const arg_list_t * const al, const enum LoadSource load_source);
void free_param_args(param_arg_ct num_args, argument *args);
void free_params(handler_set_list_t *list);

bool init(int argc, char** argv);

