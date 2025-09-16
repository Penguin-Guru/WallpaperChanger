#ifdef __has_include
#	if ! __has_include(<FreeImage.h>)
#		warning "System does not appear to have a necessary library: \"<FreeImage.h>\""
#	endif   // <FreeImage.h>
#endif  // __has_include


#include <stdlib.h>     // For malloc and free.
#include <stdio.h>      // For (f)printf.
#include <FreeImage.h>
#include "image.h"
#include "verbosity.h"



/*/
 * This handler function is from FreeImage's documentation.
 * FreeImage error handler
 * @param fif Format / Plugin responsible for the error
 * @param message Error message
/*/
static void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** FreeImage error:\n");
	if (fif != FIF_UNKNOWN) printf("\tFormat: %s\n", FreeImage_GetFormatFromFIF(fif));
	printf("\t%s\n", message);
	printf("***\n\n");
}

image_t* get_image_size(const char *wallpaper_file_path) {
	if (!(wallpaper_file_path && *wallpaper_file_path)) {
		return NULL;
	}

	FreeImage_Initialise(true);     // True will only load local plugins.
	FreeImage_SetOutputMessage(FreeImageErrorHandler);
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(wallpaper_file_path, 0);
	FIBITMAP *bitmap = FreeImage_Load(fif, wallpaper_file_path, 0);
	if (!bitmap) {
		fprintf(stderr, "Failed to load file as image: \"%s\"\n", wallpaper_file_path);
		FreeImage_DeInitialise();
		return NULL;
	}

	image_t *img = (image_t*)malloc(sizeof(image_t));
	img->width = FreeImage_GetWidth(bitmap);
	img->height = FreeImage_GetHeight(bitmap);

	FreeImage_Unload(bitmap);
	FreeImage_DeInitialise();
	return img;
}

image_t* get_pixel_data(
	const char *wallpaper_file_path,
	const int screen_depth,
	const uint16_t screen_width,
	const uint16_t screen_height
) {
	// https://git.sr.ht/~exec64/imv/tree/master/item/src/backend_freeimage.c#L60
	// https://git.sr.ht/~exec64/imv/tree/master/item/src/canvas.c#L269
	FreeImage_Initialise(true);     // True will only load local plugins.
	FreeImage_SetOutputMessage(FreeImageErrorHandler);
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(wallpaper_file_path, 0);
	FIBITMAP *bitmap = FreeImage_Load(fif, wallpaper_file_path, 0);
	if (!bitmap) {
		fprintf(stderr, "Failed to load file as image: \"%s\"\n", wallpaper_file_path);
		FreeImage_DeInitialise();
		return NULL;
	}
	if (!FreeImage_HasPixels(bitmap)) {
		fprintf(stderr, "Image does not appear to contain pixel data.\n");
		FreeImage_Unload(bitmap);
		FreeImage_DeInitialise();
		return NULL;
	}
	FIBITMAP *prev_bitmap;
	prev_bitmap = bitmap;
	// See section in FreeImage documentation: "Choosing the right resampling filter"
	static const FREE_IMAGE_FILTER filter = FILTER_BICUBIC;
	bitmap = FreeImage_Rescale(bitmap, screen_width, screen_height, filter);
	FreeImage_Unload(prev_bitmap);
	unsigned img_x, img_y, img_z = FreeImage_GetBPP(bitmap);
	if (img_z != screen_depth) {
		if (verbosity > 0) printf("Converting depth.\n");
		switch (screen_depth) {
			case 4:
				prev_bitmap = bitmap;
				bitmap = FreeImage_ConvertTo4Bits(bitmap);
				FreeImage_Unload(prev_bitmap);
				break;
			case 8:
				prev_bitmap = bitmap;
				bitmap = FreeImage_ConvertTo8Bits(bitmap);
				FreeImage_Unload(prev_bitmap);
				break;
			case 24:
				prev_bitmap = bitmap;
				bitmap = FreeImage_ConvertTo24Bits(bitmap);
				FreeImage_Unload(prev_bitmap);
				break;
			case 32:
				prev_bitmap = bitmap;
				bitmap = FreeImage_ConvertTo32Bits(bitmap);
				FreeImage_Unload(prev_bitmap);
				break;
			default:
				fprintf(stderr, "Unsupported root screen depth.\n");
				FreeImage_Unload(bitmap);
				FreeImage_DeInitialise();
				return NULL;
		}
	}
	if (FreeImage_GetImageType(bitmap) != FIT_BITMAP) {
		if (verbosity > 0) printf("Converting to standard bitmap type.\n");
		prev_bitmap = bitmap;
		// True to scale linear integers rather than rounding pixels.
		bitmap = FreeImage_ConvertToStandardType(bitmap, true);
		FreeImage_Unload(prev_bitmap);
	}
	img_x = FreeImage_GetWidth(bitmap);
	img_y = FreeImage_GetHeight(bitmap);
	img_z = FreeImage_GetBPP(bitmap);
	unsigned scan_width = FreeImage_GetPitch(bitmap);       // A.K.A. "stride".
	unsigned bytes_per_row = FreeImage_GetLine(bitmap);     // Not sure if useful.
	unsigned bytes_per_pixel = bytes_per_row / img_x;
	if (verbosity >= 2) printf(
		"Source file:\n"
			"\twidth:  %u\n"
			"\theight: %u\n"
			"\tbits_per_pixel:  %u\n"
			"\tbytes_per_pixel: %u\n"
			"\tbytes_per_row / scan_width / row_stride: %u\n"
		,
		img_x,
		img_y,
		img_z,
		bytes_per_pixel,
		bytes_per_row
	);
	if (scan_width != bytes_per_row && verbosity >= 4) {
		fprintf(stderr,
			"scan_width doesn't match bytes_per_pixel. Investigate.\n"
				"\tbytes_per_pixel: %u\n"
				"\t     scan_width: %u\n"
			, bytes_per_pixel, scan_width
		);
		FreeImage_Unload(bitmap);
		FreeImage_DeInitialise();
		return NULL;
	}
	// See FreeImage_ConvertTo...
	// FIF_XPM

	image_t *img = (image_t*)malloc(sizeof(image_t));
	img->width = img_x;
	img->height = img_y;

	img->data_len = (bytes_per_row * img_y);

	BYTE *bits = (BYTE*)malloc(img->data_len);
	FreeImage_ConvertToRawBits(
		bits, bitmap,
		scan_width, screen_depth,
		//visual->red_mask, visual->green_mask, visual->blue_mask,
		//FreeImage_GetRedMask(bitmap), FreeImage_GetGreenMask(bitmap), FreeImage_GetBlueMask(bitmap),
		FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK,
		true    // Top-down. Set false to flip vertically.
	);
	if (!bits) {
		fprintf(stderr, "Failed to read image data.\n");
		free(img);
		FreeImage_Unload(bitmap);
		FreeImage_DeInitialise();
		return NULL;
	}

	FreeImage_Unload(bitmap);
	FreeImage_DeInitialise();

	img->pixels = bits;
	return img;
}
