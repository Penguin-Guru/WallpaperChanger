#pragma once

#include <stdbool.h>
#include "util.h"


typedef uint32_t monitor_id;
typedef struct monitor_info {
	monitor_id id;	// Monitor index, not xcb_randr_output_t.
	char *name;
	uint16_t width, height;
	uint16_t offset_x, offset_y;
	//unsigned int rotation;	// xcb_randr_rotation_t
} monitor_info;
typedef uint_fast16_t monitor_ct;
typedef struct monitor_list {
	monitor_info *monitor;
	monitor_ct ct;
} monitor_list;


monitor_list const get_monitor_info();
bool set_wallpaper(const file_path_t wallpaper_file_path, const monitor_info * const monitor);
void graphics_clean_up();

