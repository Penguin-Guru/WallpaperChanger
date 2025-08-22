#pragma once

#include <stdint.h>

typedef uint8_t BYTE;

typedef struct {
	const char *file;
	unsigned int width;
	unsigned int height;

	size_t data_len;
	BYTE *pixels;
} image_t;


image_t* get_image_size(const char *wallpaper_file_path);

image_t* get_pixel_data(
	const char *wallpaper_file_path,
	const int screen_depth,
	const uint16_t screen_width,
	const uint16_t screen_height
);


