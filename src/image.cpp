//#include <fstream>	// ?
#include <string.h>	// ?
#include <FreeImage.h>
#include "image.h"

//#ifdef INCLUDE_VERBOSITY
#include "verbosity.h"
//#endif



/*/
 * FreeImage error handler
 * @param fif Format / Plugin responsible for the error
 * @param message Error message
/*/
static void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if (fif != FIF_UNKNOWN) {
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));
	}
	//printf(message);
	printf("%s", message);
	printf(" ***\n");
}

image_t* get_image_size(const char *wallpaper_file_path) {
	if (!(wallpaper_file_path && *wallpaper_file_path)) {
		return nullptr;
	}

	FreeImage_Initialise();
	FreeImage_SetOutputMessage(FreeImageErrorHandler);
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(wallpaper_file_path);
	FIBITMAP *bitmap = FreeImage_Load(fif, wallpaper_file_path, 0);
	if (!bitmap) {
		fprintf(stderr, "Failed to load file as image.\n");
		FreeImage_DeInitialise();
		return nullptr;
	}

	image_t *img = (image_t*)malloc(sizeof(image_t));
	img->width = FreeImage_GetWidth(bitmap);
	img->height = FreeImage_GetHeight(bitmap);
	//img_z = FreeImage_GetBPP(bitmap);

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
	FreeImage_Initialise();
	FreeImage_SetOutputMessage(FreeImageErrorHandler);
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(wallpaper_file_path);
	FIBITMAP *bitmap = FreeImage_Load(fif, wallpaper_file_path, 0);
	if (!bitmap) {
		fprintf(stderr, "Failed to load file as image.\n");
		FreeImage_DeInitialise();
		return nullptr;
	}
	if (!FreeImage_HasPixels(bitmap)) {
		fprintf(stderr, "Image does not appear to contain pixel data.\n");
		FreeImage_Unload(bitmap);
		FreeImage_DeInitialise();
		return nullptr;
	}
	FIBITMAP *prev_bitmap;
	prev_bitmap = bitmap;
	//FREE_IMAGE_FILTER filter = BSPLINE;		// See "Choosing the right resampling filter".
	bitmap = FreeImage_Rescale(bitmap, screen_width, screen_height, FILTER_BICUBIC);
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
				return nullptr;
		}
	}
	if (FreeImage_GetImageType(bitmap) != FIT_BITMAP) {
		if (verbosity > 0) printf("Converting to standard bitmap type.\n");
		prev_bitmap = bitmap;
		bitmap = FreeImage_ConvertToStandardType(bitmap);
		FreeImage_Unload(prev_bitmap);
	}
	img_x = FreeImage_GetWidth(bitmap);
	img_y = FreeImage_GetHeight(bitmap);
	img_z = FreeImage_GetBPP(bitmap);
	unsigned scan_width = FreeImage_GetPitch(bitmap);	// A.K.A. "stride".
	unsigned bytes_per_row = FreeImage_GetLine(bitmap);	// Not sure if useful.
	unsigned bytes_per_pixel = bytes_per_row / img_x;
	if (verbosity > 1) printf(
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
	if (scan_width != bytes_per_row) {
		fprintf(stderr,
			"scan_width doesn't match bytes_per_pixel. Investigate.\n"
				"\tbytes_per_pixel: %u\n"
				"\t     scan_width: %u\n"
			, bytes_per_pixel, scan_width
		);
		FreeImage_Unload(bitmap);
		FreeImage_DeInitialise();
		return nullptr;
	}
	// See FreeImage_ConvertTo...
	// FIF_XPM
	//

	image_t *img = (image_t*)malloc(sizeof(image_t));
	img->width = img_x;
	img->height = img_y;
	img->bytes_per_row = bytes_per_row;
	img->scan_width = scan_width;

	//img->data_len = (bytes_per_row * img_y);
	img->data_len = (bytes_per_row * img_y);
	//img->data_len = (bytes_per_row * img_y)+1;
	//const size_t data_len = (bytes_per_row * img_y);
	//const size_t data_len = (scan_width * img_y);

	/*FIMEMORY *stream;
	FreeImage_SaveToMemory(FIF_XPM, bitmap, stream);*/
	//
	/*BYTE bits[data_len];
	memcpy(bits, FreeImage_GetBits(bitmap), data_len);*/
	//
	/*BYTE *bits = (BYTE*)malloc(data_len);
	if (! bits = FreeImage_GetBits(bitmap)) {
		fprintf(stderr, "Failed to read image data.\n");
		free(img);
		FreeImage_Unload(bitmap);
		FreeImage_DeInitialise();
		return nullptr;
	}*/
	//
	/*// Note: This assumes bit depth is 24. Testing.
	BYTE *bits = (BYTE*)FreeImage_GetBits(bitmap);
	for (size_t y = 0; y < img_y; y++) {
		BYTE *pixel = (BYTE*)bits;
		for (size_t x = 0; x < img_x; x++) {
			pixel[FI_RGBA_RED]	= 128;
			pixel[FI_RGBA_GREEN]	= 128;
			pixel[FI_RGBA_BLUE]	= 128;
			pixel += 3;
		}
		// Next line.
		//bits += pitch;
		bits += scan_width;
	}
	if (!bits) {
		fprintf(stderr, "Failed to read image data.\n");
		free(img);
		FreeImage_Unload(bitmap);
		FreeImage_DeInitialise();
		return nullptr;
	}*/
	//
	/*BYTE *bits = (BYTE*)malloc(data_len);
	for (unsigned y = 0; y < img_y; y++) {
		BYTE *byte = FreeImage_GetScanLine(bitmap, y);
		for (unsigned x = 0; x < img_x; x++) {
			// Set pixel colour to green.
			byte[FI_RGBA_RED]	= 0;
			byte[FI_RGBA_GREEN]	= 255;
			byte[FI_RGBA_BLUE]	= 0;
			byte[FI_RGBA_ALPHA]	= 128;
			byte += bytes_per_pixel;
		}
	}*/
	//
	/*BYTE *bits = (BYTE*)malloc(data_len);
	if (FreeImage_GetBPP(bitmap) != 8) fprintf(stderr, "Wrong BPP!");
	else {
		for (unsigned y = 0; y < img_y; y++) {
			BYTE *byte = (BYTE*)FreeImage_GetScanLine(bitmap, y);
			for (unsigned x = 0; x < img_x; x++) {
				byte[x] = 128;
			}
		}
	}*/
	/*for (unsigned y = 0; y < img_y; y++) {
		BYTE *byte = (BYTE*)FreeImage_GetScanLine(bitmap, y);
		for (unsigned x = 0; x < img_x; x++) {
			*(bits+(y*x)+x) = *byte;
		}
	}*/
	//
	BYTE *bits = (BYTE*)malloc(img->data_len);
	FreeImage_ConvertToRawBits(
		bits, bitmap,
		scan_width, screen_depth,
		//visual->red_mask, visual->green_mask, visual->blue_mask,
		//FreeImage_GetRedMask(bitmap), FreeImage_GetGreenMask(bitmap), FreeImage_GetBlueMask(bitmap),
		FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK,
		true	// topdown
		//false	// topdown
	);
	if (!bits) {
		fprintf(stderr, "Failed to read image data.\n");
		free(img);
		FreeImage_Unload(bitmap);
		FreeImage_DeInitialise();
		return nullptr;
	}
	//
	/*// https://stackoverflow.com/questions/5249644/cairo-load-image-from-data
	int stride = scan_width;
	int wid = img_x;
	int hgt = img_y;
	int n;
	//unsigned char data[n=stride*hgt];
	unsigned char data[n=stride*hgt*4];
	memset(data, 0, n);
	unsigned long *ldata = (void *)data;
	int spo = 8/bits;			// samples per octet
	int span = wid/spo + (wid%spo?1:0);	// byte width
	int i,j;
	for (i=0; i < hgt; i++) {
		for (j=0; j < wid; j++) {
			unsigned char t;
			t = ( ( samp[i*span + j/spo] >> ( (j%spo)) )
				& (0xFF >> (8-bits)) );
			if (bits < 8) t = amap[bits==1?0:bits==2?1:2][t];
			t = tmap[t]; // map value through the transfer map
			ldata[i*stride/4 + j] = t<<16 | t<<8 | t;
		}
	}*/
	//
	// RGBQUAD pixel = {0, 0, 255, 0};
	// RGBTRIPLE pixel = {0, 0, 255};
	//
	// I hope I don't need to do this: https://cr.yp.to/2004-494/libpng/libpng-1.2.5/contrib/gregbook/rpng-x.c

	FreeImage_Unload(bitmap);
	FreeImage_DeInitialise();

	img->pixels = bits;
	return img;
}
