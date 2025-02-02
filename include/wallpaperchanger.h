#pragma once

#include <stdbool.h>
#include <stddef.h>	// For size_t.
#include "libxdgbasedir.h"
#include "util.h"

#define DEFAULT_DATA_FILE_NAME "wallpapers.log"
#define DEFAULT_WALLPAPER_DIR_NAME "wallpapers"


static file_path_t data_directory = NULL;

static file_path_t
	data_file_path = NULL,	// Path to database.
	wallpaper_path = NULL	// Wallpaper to operate on.
;

bool scale_for_wm = true;	// Scale wallpapers based on static, foreground windows.

// Cache used by some functions:
static size_t s_wallpapers_ct = 0;
static file_path_t *s_wallpapers = NULL;

// Used for tracking directory depth while walking filesystem:
bool follow_symlinks_beyond_specified_directory = false;
static int s_directory_depth_remaining = MAX_DIRECTORY_DEPTH;

