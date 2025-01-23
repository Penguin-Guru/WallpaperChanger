#pragma once

#include "libxdgbasedir.h"
#include "util.h"

#define DEFAULT_DATA_FILE_NAME "wallpapers.log"
#define DEFAULT_WALLPAPER_DIR_NAME "wallpapers"


static file_path_t data_directory = nullptr;

static file_path_t
	data_file_path = nullptr,	// Path to database.
	wallpaper_path = nullptr	// Wallpaper to operate on.
;
static size_t s_wallpapers_ct = 0;
static file_path_t *s_wallpapers = nullptr;	// Cache used by some functions.

bool follow_symlinks_beyond_specified_directory = false;
static int s_directory_depth_remaining = MAX_DIRECTORY_DEPTH;


/* Misc. intermediary functions: */

bool set_new_current(const file_path_t wallpaper_file_path, tags_t tags = 0);

