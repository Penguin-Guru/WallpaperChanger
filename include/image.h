#pragma once

#include <stdint.h>

using BYTE = uint8_t;

typedef struct {
	const char *file;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int bytes_per_row = 0, scan_width = 0;	// Redundant. Fix later.

	size_t data_len = 0;
	BYTE *pixels = nullptr;
} image_t;


image_t* get_image_size(const char *wallpaper_file_path);

image_t* get_pixel_data(
	const char *wallpaper_file_path,
	const int screen_depth,
	const uint16_t screen_width,
	const uint16_t screen_height
);


