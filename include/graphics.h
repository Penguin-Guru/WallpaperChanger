#pragma once

#include <stdbool.h>
#include "util.h"


typedef struct monitor_info {
	uint32_t id;	// xcb_randr_output_t
	unsigned char *name;
	uint16_t width;
	uint16_t height;
	unsigned int rotation;	// xcb_randr_rotation_t
} monitor_info;


monitor_info* const get_monitor_info();
bool set_wallpaper(const file_path_t wallpaper_file_path, const uint32_t monitor);
void graphics_clean_up();

