#pragma once


#include <features.h>
#ifndef __linux__
#	warning "This program has only been tested on Linux."
#endif
#ifndef __GNUC__
#	error "This program requires GNU C extensions."
#endif
#if (__STDC_VERSION__ < 202000)
#	warning "This program was written for C23."
#endif
#if !( defined(__FreeBSD__) || defined(__NetBSD__) || (defined(__GLIBC__) && ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 38) || __GLIBC__ > 2)) )
#	error "System does not appear to offer strchrnul."
#endif


#include <stdbool.h>
#include <stddef.h>	// For size_t.
#include "libxdgbasedir.h"
#include "graphics.h"	// For monitor_info.
#include "util.h"

#define DEFAULT_DATA_FILE_NAME "wallpapers.log"
#define DEFAULT_WALLPAPER_DIR_NAME "wallpapers"



static file_path_t data_directory = NULL;

static file_path_t
	data_file_path = NULL,	// Path to database.
	wallpaper_path = NULL	// Wallpaper to operate on.
;

bool scale_for_wm = true;	// Scale wallpapers based on static, foreground windows.



static monitor_list s_monitors;
static uint32_t s_target_monitor_id = 0;	// Monitor index, not xcb_randr_output_t.
static const char *s_target_monitor_name = NULL;	// Reference user input after loading component.

// Cache used by some functions:
static size_t s_wallpapers_ct = 0;
static file_path_t *s_wallpapers = NULL;

// Used for tracking directory depth while walking filesystem:
bool follow_symlinks_beyond_specified_directory = false;
static int s_directory_depth_remaining = MAX_DIRECTORY_DEPTH;

