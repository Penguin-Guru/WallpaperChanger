#include "stddef.h"
#include "parameters.h"
//#include "assert.h"


bool handle_set(const arg_list_t * const al);
bool handle_set_new(const arg_list_t * const al);
bool handle_set_fav(const arg_list_t * const al);

bool handle_fav_current(const arg_list_t * const al);
bool handle_delete_current(const arg_list_t * const al);

bool handle_print(const arg_list_t * const al);

bool handle_database_path(const arg_list_t * const al);
bool handle_wallpaper_path(const arg_list_t * const al);
bool handle_follow_symlinks_beyond_specified_directory(const arg_list_t * const al);

bool handle_config_file(const arg_list_t * const al);
bool handle_verbosity(const arg_list_t * const al);
bool handle_print_help(const arg_list_t * const al);


parameter_t params_known[] = {	// Accessible via both C.L.I. and config file.
	{
		.handler_set = (handler_set_t){
			.name		= "set",
			.description	= "Set a specific wallpaper as active/current.",
			.fn		= handle_set,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag  	= "set",
			.short_flag 	= "s"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 1,
			.max		= 1
		},
		.type = RUN,
		.previous_load = NONE
	},
	{
		.handler_set = (handler_set_t){
			.name		= "set-new",
			.description	= "Set a wallpaper not previously used.",
			.fn		= handle_set_new,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "set-new",
			.short_flag	= "sn"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 0,
			.max		= 0
		},
		.type = RUN,
		.previous_load = NONE
	},
	{
		.handler_set = (handler_set_t){
			.name		= "set-favourite",
			.description	= "Set a wallpaper you have previously marked as a favourite.",
			.fn		= handle_set_fav,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "set-fav",
			.short_flag	= "sf"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 0,
			.max		= 0
		},
		.type = RUN,
		.previous_load = NONE
	},


	{
		.handler_set = (handler_set_t){
			.name		= "favourite",
			.description	= "Mark the current wallpaper as a favourite.",
			.fn		= handle_fav_current,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "fav",
			.short_flag	= "f"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 0,
			.max		= 0
		},
		.type = RUN,
		.previous_load = NONE
	},
	{
		.handler_set = (handler_set_t){
			.name		= "delete",
			.description	= "Delete the current wallpaper file and its database entry.",
			.fn		= handle_delete_current,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "del",
			.short_flag	= "d"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 0,
			.max		= 0
		},
		.type = RUN,
		.previous_load = NONE
	},

	{
		.handler_set = (handler_set_t){
			.name		= "print-current",
			.description	= "Print information about the current wallpaper file/image.",
			.fn		= handle_print,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "print",
			.short_flag	= "p"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 0,
			.max		= 0
		},
		.type = RUN,
		.previous_load = NONE
	},


	{
		.handler_set = (handler_set_t){
			.name		= "config-file",
			.description	= "Specify a (non-standard) config file to load.",
			.fn		= handle_config_file,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "config-file",
			.short_flag	= "c"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 1,
			.max		= 1
		},
		.type = RUN,
		.previous_load = NONE
	},
	{
		.handler_set = (handler_set_t){
			.name		= "database-path",
			.description	= "Specify (non-standard) path to database.",
			.fn		= handle_database_path,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "database-path",
			.short_flag	= "db"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 1,
			.max		= 1
		},
		.type = INIT,
		.previous_load = NONE
	},
	{
		.handler_set = (handler_set_t){
			.name		= "wallpaper-path",
			.description	= "Specify (non-standard) path to wallpaper(s).",
			.fn		= handle_wallpaper_path,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "wallpaper-path",
			.short_flag	= "w"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 1,
			.max		= 1
		},
		.type = INIT,
		.previous_load = NONE
	},
	{
		.handler_set = (handler_set_t){
			.name		= "follow_symlinks_beyond_specified_directory",
			.description	= 
				"Allow symlinks in wallpaper-path to reference directories outside of it."
					"\n\t\t\tNote: Symlinks to files are always allowed."
				,
			.fn		= handle_follow_symlinks_beyond_specified_directory,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "follow-symlinks",
			.short_flag	= "fs"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 0,
			.max		= 0
		},
		.type = INIT,
		.previous_load = NONE
	},
	{
		.handler_set = (handler_set_t){
			.name		= "verbosity",
			.description	= "Specify level of verbosity. A larger integer means more output.",
			.fn		= handle_verbosity,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "verbosity",
			.short_flag	= NULL
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 1,
			.max		= 1
		},
		.type = INIT,
		.previous_load = NONE
	},

	{
		.handler_set = (handler_set_t){
			.name		= "print-help",
			.description	= "Print this help text.",
			.fn		= handle_print_help,
			.arg_list	= 0	// Null.
		},
		.flag_pair = (flag_pair_t){
			.long_flag	= "help",
			.short_flag	= "h"
		},
		.arg_params = (param_arg_parameters_t){
			.min		= 0,
			.max		= 0
		},
		.type = INIT,
		.previous_load = NONE
	}
};

const uint_fast8_t num_params_known = sizeof(params_known)/sizeof(params_known[0]);
//static_assert(num_params_known > 0);

