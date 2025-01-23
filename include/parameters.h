#pragma once

#include "parameter.h"
#include "argument.h"


bool handle_set(init::arg_list_t *al);
bool handle_set_new(init::arg_list_t *al);
bool handle_set_fav(init::arg_list_t *al);

bool handle_fav_current(init::arg_list_t *al);
bool handle_delete_current(init::arg_list_t *al);

bool handle_print(init::arg_list_t *al);

bool handle_database_path(init::arg_list_t *al);
bool handle_wallpaper_path(init::arg_list_t *al);
bool handle_follow_symlinks_beyond_specified_directory(init::arg_list_t *al);

namespace init {
	bool handle_config_file(arg_list_t *al);	// Init namespace because it is application agnostic.
	bool handle_verbosity(arg_list_t *al);
	bool handle_print_help(arg_list_t *al);

	static const parameter_t params_known[] = {	// Accessible via both C.L.I. and config file.
		{
			.handler_set = handler_set_t{
				.name		= "set",
				.description	= "Set a specific wallpaper as active/current.",
				.fn		= &::handle_set,
				.arg_list	= (arg_list_t[]){{.ct = 1}}
			},
			.flag_pair = flag_pair_t{
				.long_flag  	= "set",
				.short_flag 	= "s"
			},
			.type = ParamType::RUN
		},
		{
			.handler_set = handler_set_t{
				.name		= "set-new",
				.description	= "Set a wallpaper not previously used.",
				.fn		= &::handle_set_new,
				.arg_list	= (arg_list_t[]){{.ct = 0}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "set-new",
				.short_flag	= "sn"
			},
			.type = ParamType::RUN
		},
		{
			.handler_set = handler_set_t{
				.name		= "set-favourite",
				.description	= "Set a wallpaper you have previously marked as a favourite.",
				.fn		= &handle_set_fav,
				.arg_list	= (arg_list_t[]){{.ct = 0}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "set-fav",
				.short_flag	= "sf"
			},
			.type = ParamType::RUN
		},


		{
			.handler_set = handler_set_t{
				.name		= "favourite",
				.description	= "Mark the current wallpaper as a favourite.",
				.fn		= &handle_fav_current,
				.arg_list	= (arg_list_t[]){{.ct = 0}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "fav",
				.short_flag	= "f"
			},
			.type = ParamType::RUN
		},
		{
			.handler_set = handler_set_t{
				.name		= "delete",
				.description	= "Delete the current wallpaper file and its database entry.",
				.fn		= &handle_delete_current,
				.arg_list	= (arg_list_t[]){{.ct = 0}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "del",
				.short_flag	= "d"
			},
			.type = ParamType::RUN
		},
	
		{
			.handler_set = handler_set_t{
				.name		= "print-current",
				.description	= "Print information about the current wallpaper file/image.",
				.fn		= &handle_print,
				.arg_list	= (arg_list_t[]){{.ct = 0}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "print",
				.short_flag	= "p"
			},
			.type = ParamType::RUN
		},
	
	
		{
			.handler_set = handler_set_t{
				.name		= "config-file",
				.description	= "Specify a (non-standard) config file to load.",
				.fn		= &handle_config_file,
				.arg_list	= (arg_list_t[]){{.ct = 1}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "config-file",
				.short_flag	= "c"
			},
			.type = ParamType::RUN
		},
		{
			.handler_set = handler_set_t{
				.name		= "database-path",
				.description	= "Specify (non-standard) path to database.",
				.fn		= &handle_database_path,
				.arg_list	= (arg_list_t[]){{.ct = 1}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "database-path",
				.short_flag	= "db"
			},
			.type = ParamType::INIT
		},
		{
			.handler_set = handler_set_t{
				.name		= "wallpaper-path",
				.description	= "Specify (non-standard) path to wallpaper(s).",
				.fn		= &handle_wallpaper_path,
				.arg_list	= (arg_list_t[]){{.ct = 1}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "wallpaper-path",
				.short_flag	= "w"
			},
			.type = ParamType::INIT
		},
		{
			.handler_set = handler_set_t{
				.name		= "follow_symlinks_beyond_specified_directory",
				.description	= 
					"Allow symlinks in wallpaper-path to reference directories outside of it."
						"\n\t\t\tNote: Symlinks to files are always allowed."
					,
				.fn		= &handle_follow_symlinks_beyond_specified_directory,
				.arg_list	= (arg_list_t[]){{.ct = 0}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "follow-symlinks",
				.short_flag	= "fs"
			},
			.type = ParamType::INIT
		},
		{
			.handler_set = handler_set_t{
				.name		= "verbosity",
				.description	= "Specify level of verbosity. A larger integer means more output.",
				.fn		= &handle_verbosity,
				.arg_list	= (arg_list_t[]){{.ct = 1}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "verbosity",
				.short_flag	= NULL
			},
			.type = ParamType::INIT
		},

		{
			.handler_set = handler_set_t{
				.name		= "print-help",
				.description	= "Print this help text.",
				.fn		= &handle_print_help,
				.arg_list	= (arg_list_t[]){{.ct = 0}}
			},
			.flag_pair = flag_pair_t{
				.long_flag	= "help",
				.short_flag	= "h"
			},
			.type = ParamType::INIT
		}
	};
}
