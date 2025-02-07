#pragma once

#include <stdbool.h>
#include "util.h"


typedef struct monitor_info {
	uint32_t id;	// xcb_randr_output_t
	char *name;	// Do not free.
	uint16_t width, height;
	uint16_t offset_x, offset_y;
	unsigned int rotation;	// xcb_randr_rotation_t
} monitor_info;
typedef struct monitor_list {
	monitor_info *monitor;
	uint_fast16_t ct;
} monitor_list;


monitor_list const get_monitor_info();
bool set_wallpaper(const file_path_t wallpaper_file_path, const monitor_info * const monitor);
void graphics_clean_up();

