#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/randr.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_errors.h>
//#include <xcb/xcbext.h>
#include <stdlib.h>	// For free.
#include <stdio.h>	// For fprintf.
#include <assert.h>
#include "graphics.h"
#include "image.h"
#include "verbosity.h"
//#include "util.h"


static const uint_fast8_t MAX_ATOM_NAME_LEN = 100;

extern bool scale_for_wm;

typedef struct atom_pair {
	const char *name;
	const xcb_atom_t id;
} atom_pair;

typedef struct atom_pack {
	atom_pair query_atom;
	atom_pair match_atom;
	atom_pair key_atom;
	atom_pair wm_desktop_atom;
} atom_pack;

typedef struct XY_Dimensions {
	uint_fast16_t width;
	uint_fast16_t height;
} XY_Dimensions;


static xcb_connection_t *conn = 0;
//static uint16_t max_req_len;
static xcb_generic_error_t *error;
static xcb_void_cookie_t cookie;
static xcb_screen_t *screen;

// https://www.x.org/releases/X11R7.6/doc/xproto/x11protocol.html#server_information
// 	xy-pixmap represents each plane as a bitmap, ordered from most significant to least in bit order with no padding between.
//const xcb_image_format_t image_format = XCB_IMAGE_FORMAT_Z_PIXMAP;
#define image_format XCB_IMAGE_FORMAT_Z_PIXMAP
static xcb_image_order_t byte_order;	// Component byte (or nybble if bpp=4) order for z-pixmap, otherwise byte order of scanline.
//const xcb_image_order_t bit_order = XCB_IMAGE_ORDER_LSB_FIRST;
//#define bit_order XCB_IMAGE_ORDER_LSB_FIRST;
static xcb_image_order_t bit_order;	// Bit order of each scanline unit for xy-bitmap and xy-pixmap. *Maybe* order of each pixel in z-pixmap?
//static const uint8_t scanline_pad = 32;	// Valid: 8, 16, 32.
static uint8_t scanline_pad;
	// Valid: 8, 16, 32.
	// Each (bitmap) scanline is padded to a multiple of this value.
static uint8_t scanline_unit;
	// Valid: 8, 16, 32.
	// bitmap-scanline-unit is always >= bitmap-scanline-pad.
/*static const uint8_t depth = 0;
	// Valid depths:
	// 	Z_PIXMAP: 1, 4, 8, 16, 24.
	// 	XY_BITMAP: 1.
	// 	XY_PIXMAP: any.*/
static uint8_t bits_per_pixel = 0;
	// Valid depths:
	// 	Z_PIXMAP: 1, 4, 8, 16, 24, 32.
	// 	XY_BITMAP: 1.
	// 	XY_PIXMAP: any.

static xcb_visualtype_t *visual = NULL;	// A visual is a description of a pixel.
static int depth;	// Number of bits per pixel. In this case, for the root window.
static xcb_atom_t esetroot_pmap_id, _xrootpmap_id;
static xcb_gcontext_t gc;


/* Utility functions: */

#define p_delete(mem_p)                              \
    do {                                             \
        void **__ptr = (void **) (mem_p);            \
        free(*__ptr);                                \
        *(void **)__ptr = NULL;                      \
    } while (0)

// https://github.com/awesomeWM/awesome/blob/ad0290bc1aac3ec2391aa14784146a53ebf9d1f0/globalconf.h#L45
//#define NO_EVENT_VALUE (uint32_t[]) { 0 }
#define ROOT_WINDOW_EVENT_MASK \
    (const uint32_t []) { \
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY \
      | XCB_EVENT_MASK_ENTER_WINDOW \
      | XCB_EVENT_MASK_LEAVE_WINDOW \
      | XCB_EVENT_MASK_STRUCTURE_NOTIFY \
      | XCB_EVENT_MASK_BUTTON_PRESS \
      | XCB_EVENT_MASK_BUTTON_RELEASE \
      | XCB_EVENT_MASK_FOCUS_CHANGE \
      | XCB_EVENT_MASK_PROPERTY_CHANGE \
    }


// https://gitlab.freedesktop.org/xorg/lib/libxcb-errors/-/blob/master
// https://github.com/sasdf/vcxsrv/blob/master/xcb-util-errors/src/xcb_errors.h
static void handle_error(xcb_connection_t *conn, xcb_generic_error_t *gen_err) {
	xcb_errors_context_t *err_cont;
	xcb_errors_context_new(conn, &err_cont);
	const char *major, *minor, *ext, *err;
	err = xcb_errors_get_name_for_error(err_cont, gen_err->error_code, &ext);
	major = xcb_errors_get_name_for_major_code(err_cont, gen_err->major_code);
	minor = xcb_errors_get_name_for_minor_code(err_cont, gen_err->major_code, gen_err->minor_code);
	printf("XCB Error: %s:%s, %s:%s, resource %u sequence %u\n",
		err,
		ext ? ext : "no_extension",
		major,
		minor ? minor : "no_minor",
		(unsigned int)gen_err->resource_id,
		(unsigned int)gen_err->sequence
	);
	xcb_errors_context_free(err_cont);
	free(gen_err);
}

void graphics_clean_up() {
	if (conn) xcb_disconnect(conn);
}

xcb_atom_t get_atom(xcb_connection_t* conn, const char *atom_name) {
	xcb_intern_atom_cookie_t atom_cookie;
	xcb_atom_t atom;
	xcb_intern_atom_reply_t *rep;

	atom_cookie = xcb_intern_atom(conn, 0, strlen(atom_name), atom_name);
	if (!(rep = xcb_intern_atom_reply(conn, atom_cookie, NULL))) {
		fprintf(stderr, "No I.D. found for atom: \"%s\"\n", atom_name);
		return 0;	// Null.
	}
	atom = rep->atom;
	free(rep);
	if (verbosity >= 6) printf("Received I.D. for atom: \"%s\" (%d)\n", atom_name, atom);
	return atom;
}
bool get_atom_name(xcb_connection_t *conn, const xcb_atom_t atom, char name[MAX_ATOM_NAME_LEN+1]) {
	xcb_generic_error_t *err;
	xcb_get_atom_name_cookie_t atom_c = xcb_get_atom_name(conn, atom);
	xcb_get_atom_name_reply_t *atom_r = xcb_get_atom_name_reply(conn, atom_c, &err);
	if (!atom_r) {
		static const char *text = "(No reply to atom name query)";
		assert(strlen(text) <= MAX_ATOM_NAME_LEN);
		strcpy(name, text);
		return false;
	} else {
		if (err) handle_error(conn, err);
		const int length = xcb_get_atom_name_name_length(atom_r);
		if (length > MAX_ATOM_NAME_LEN) {
			fprintf(stderr,
				"Name of atom %u exceeds maximum supported length.\n"
					"\tReported length: %d\n"
					"\tMax supported length: %hu\n"
				, atom
				, length
				, MAX_ATOM_NAME_LEN
			);
			return false;
		}
		//strcpy(name, xcb_get_atom_name_name(atom_r));
		//strncpy(name, xcb_get_atom_name_name(atom_r), length);
		//strncpy(name, xcb_get_atom_name_name(atom_r), MAX_ATOM_NAME_LEN);
		// strcpy and strncpy introduce erroneous values in some cases. I'm not sure why.
		strlcpy(name, xcb_get_atom_name_name(atom_r), length + 1);
		return true;
	}
}

xcb_get_property_reply_t* get_property_reply(
	xcb_connection_t * conn, xcb_window_t * win,
	xcb_atom_t atom, const xcb_atom_enum_t type, const uint32_t length,
	const uint_fast8_t verbosity
) {
	xcb_generic_error_t *err;

	xcb_get_property_cookie_t prop_c = xcb_get_property(conn, false, *win, atom, type, 0, length ? length : ~0);
	if (!prop_c.sequence) {
		char atom_name[MAX_ATOM_NAME_LEN];
		get_atom_name(conn, atom, atom_name);
		fprintf(stderr,
			"Null or zero sequence returned by xcb_get_property. Not sure if normal.\n"
				"\tAtom name: %s"
				"\tAtom I.D.: %d"
			,
			atom_name,
			atom
		);
		return NULL;
	}
	xcb_aux_sync(conn);	// Not sure if necessary.
	xcb_flush(conn);	// Probably not necessary.

	xcb_get_property_reply_t *prop_r;
	if (!(prop_r = xcb_get_property_reply(conn, prop_c, &err))) {
		fprintf(stderr, "Property reply was not received.\n");
		if (err) handle_error(conn, err);
	} else {
		if (err) {
			fprintf(stderr, "Failed to query atom.\n");
			handle_error(conn, err);
			p_delete(&prop_r);
		} else if (prop_r->type == XCB_NONE) {
			if (verbosity > 1) printf("Atom query returned XCB_NONE.\n");
			p_delete(&prop_r);
		} else if (prop_r->type != type) {
			fprintf(stderr,
				"Atom property query returned an unexpected type: %d\n"
					"\tExpected type: %d\n"
				, prop_r->type
				, type
			);
			p_delete(&prop_r);
		} else if (length != 0 && xcb_get_property_value_length(prop_r) != length) {
			fprintf(stderr, "Atom property query returned an unexpected length: %d\n", xcb_get_property_value_length(prop_r));
			p_delete(&prop_r);
		}
	}
	return prop_r;
}


bool init_xcb() {
	// Initialise connection to X11 server:
	int screenNum;						// Assigned by xcb_connect().
	conn = xcb_connect(NULL, &screenNum);	// NULL uses DISPLAY env.

	// https://xcb.freedesktop.org/manual/structxcb__setup__t.html
	const xcb_setup_t *setup = xcb_get_setup(conn);
	//max_req_len = setup->maximum_request_length;	// Querying explicitly yields much larger value.
	{
		uint8_t tmp = setup->image_byte_order;
		if (!(tmp == 0 || tmp == 1)) {
			fprintf(stderr, "Error detecting image_byte_order. Value is: %hhd\n", tmp);
			return false;
		}
		/*byte_order = static_cast<xcb_image_order_t>(tmp);*/
		byte_order = (xcb_image_order_t)(tmp);

		tmp = setup->bitmap_format_bit_order;
		if (!(tmp == 0 || tmp == 1)) {
			fprintf(stderr, "Error detecting byte_order. Value is: %hhd\n", tmp);
			return false;
		}
		/*bit_order = static_cast<xcb_image_order_t>(tmp);*/
		bit_order = (xcb_image_order_t)(tmp);
	}
	scanline_pad = setup->bitmap_format_scanline_pad;
	scanline_unit = setup->bitmap_format_scanline_unit;

	// Get screen with number screenNum:
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
	for (int i = 0; i < screenNum; ++i) xcb_screen_next(&iter);
	screen = iter.data;

	// Get root visual:
	//xcb_visualtype_t *visual = NULL;	// A visual is a description of a pixel.
	{
		xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(screen);
		depth = depth_iter.data->depth;
		for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
			xcb_visualtype_iterator_t visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
			depth = depth_iter.data->depth;
			for (; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
				if (visual_iter.data->visual_id == screen->root_visual) {
					visual = visual_iter.data;
					break;
				}
			}
			if (visual) break;
		}
		if (!visual) {
			fprintf(stderr, "Failed to match root visual.\n");
			return false;
		}
		if (!(
			visual->_class == XCB_VISUAL_CLASS_TRUE_COLOR
			|| visual->_class == XCB_VISUAL_CLASS_DIRECT_COLOR
		)) {
			fprintf(stderr, "Unsupported root visual class.\n");
			return false;
		}
		/*printf("Depth: %i\nMasks:\n\tRed:   %08x\n\tGreen: %08x\n\tBlue:  %08x\n",
			depth, visual->red_mask, visual->green_mask, visual->blue_mask
		);*/

	}
	//bits_per_pixel = visual->bits_per_rgb_value;	// Does this account for transparency? What about non-RGB colour model encoding?
	//bits_per_pixel = screen->root_depth / 8;	// Does this account for transparency? What about non-RGB encoding?

	// Consider xcb_setup_pixmap_formats().
	// 	https://github.com/FFmpeg/FFmpeg/blob/9135dffd177d457a8a1781b9e6c6d400648165cb/libavdevice/xcbgrab.c#L524
	// https://xcb.pdx.freedesktop.narkive.com/0u3XxxGY/put-image
		// Line = ((x * format->bits_per_pixel) + 7) / 8;
		// Line = pad(Line, format->scanline_pad);
		// Also important, to identify which bits correspond to which colours:
		// 	setup->byte_order is important
		// 	visual->red_mask, green_mask, and blue_mask.
		// https://registry.khronos.org/EGL/extensions/EXT/EGL_EXT_platform_xcb.txt

	// https://xcb.freedesktop.org/manual/structxcb__screen__t.html
	// https://xcb.freedesktop.org/manual/structxcb__visualtype__t.html


	//
	// Get format:
	//
	/*xcb_format_t *fmt = xcb_setup_pixmap_formats(setup);
	bits_per_pixel = fmt->bits_per_pixel;	// 1?
	//scanline_pad = fmt->scanline_pad;	// 1?
	//depth = fmt->depth;*/
	//
	{
		/*xcb_format_t *fmt;
		fmt = find_format_by_depth(setup, depth);
		if (fmt) {
			bits_per_pixel = fmt->bits_per_pixel;
			scanline_pad = fmt->scanline_pad;
		} else {
			fprintf(stderr, "Null from find_format_by_depth(). Proceeding anyway." );
		}*/

		xcb_format_t *fmt = xcb_setup_pixmap_formats(setup);
		xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length(setup);
		unsigned short ct = 0;
		for(; fmt != fmtend; ++fmt) {
			//if ((fmt->depth == depth) && (fmt->bits_per_pixel == bpp)) {
			if (fmt->depth == depth) {
				ct++;
				if (verbosity > 2) printf("Format %p has depth %hhu, bpp %hhu, pad %hhu.\n",
					fmt, depth, fmt->bits_per_pixel, fmt->scanline_pad
				);
				if (verbosity > 1 && ct > 1 && bits_per_pixel != fmt->bits_per_pixel) {
					fprintf(stderr,
						"Conflicting values for bits_per_pixel:\n"
							"\tOld: %hhu\n"
							"\tNew: %hhu\n",
						bits_per_pixel, fmt->bits_per_pixel
					);
				}
				bits_per_pixel = fmt->bits_per_pixel;
				if (scanline_pad != fmt->scanline_pad) {
					if (verbosity > 1) fprintf(stderr,
						"Conflicting values for scanline_pad:\n"
							"\tOld: %hhu\n"
							"\tNew: %hhu\n",
						scanline_pad, fmt->scanline_pad
					);
					scanline_pad = fmt->scanline_pad;
				}
				//break;
			}
		}
		if (ct == 0) fprintf(stderr, "No matching formats!\n");
		else {
			if (ct > 1) fprintf(stderr, "Multiple matching formats!\n");
			/*if (fmt) {
				if (bits_per_pixel != fmt->bits_per_pixel) {
					fprintf(stderr,
						"Conflicting values for bits_per_pixel:\n"
							"\tbits_per_pixel (from somewhere): %hhu\n"
							"\tbits_per_pixel (from xcb_format_t): %hhu\n",
						bits_per_pixel, fmt->bits_per_pixel
					);
					bits_per_pixel = fmt->bits_per_pixel;
				}
				if (scanline_pad != fmt->scanline_pad) {
					fprintf(stderr,
						"Conflicting values for scanline_pad:\n"
							"\tbitmap_format_scanline_pad (from setup): %hhd\n"
							"\tscanline_pad (from xcb_format_t): %hhd\n",
						scanline_pad, fmt->scanline_pad
					);
					scanline_pad = fmt->scanline_pad;
				}
			} else fprintf(stderr, "Null pointer to xcb_format_t!.\n");*/
			else if (!fmt) fprintf(stderr, "Null pointer to xcb_format_t!.\n");
		}
	}

	char byte_order_text[30], bit_order_text[30];
	switch (byte_order) {
		case 0: strncpy(byte_order_text, "XCB_IMAGE_ORDER_LSB_FIRST", 30); break;
		case 1: strncpy(byte_order_text, "XCB_IMAGE_ORDER_MSB_FIRST", 30); break;
		default: strncpy(byte_order_text, "INVALID!", 30);
	}
	switch (bit_order) {
		case 0: strncpy(bit_order_text, "XCB_IMAGE_ORDER_LSB_FIRST", 30); break;
		case 1: strncpy(bit_order_text, "XCB_IMAGE_ORDER_MSB_FIRST", 30); break;
		default: strncpy(bit_order_text, "INVALID!", 30);
	}
	/*char image_format_text[30];
	switch (image_format) {
		case 0: strncpy(image_format_text, "XCB_IMAGE_FORMAT_XY_BITMAP", 30); break;
		case 1: strncpy(image_format_text, "XCB_IMAGE_FORMAT_XY_PIXMAP", 30); break;
		case 2: strncpy(image_format_text, "XCB_IMAGE_FORMAT_Z_PIXMAP", 30); break;
		default: strncpy(image_format_text, "INVALID!", 30);
	}*/
	if (verbosity > 2) printf(
		"X11 root screen:\n"
			//"\ttarget_width:   %d\n"
			//"\ttarget_height:  %d\n"
			"\tbits_per_pixel: %hhu\n"
			"\tscanline_pad:   %hhu\n"
			"\tdepth: %d\n"
			"\tunit:  %hhu\n"
			"\tbyte_order: %hhu (%s)\n"
			"\tbit_order:  %hhu (%s)\n"
			"\tmasks:\n"
				// Padding should be based on bits-per-rgb-value (max 16-bit).
				// Padding should be oriented based on bit_order.
				/*"\t\t  Red: %08x\n"
				"\t\tGreen: %08x\n"
				"\t\t Blue: %08x\n"*/
				"\t\t  Red: %06x\n"
				"\t\tGreen: %06x\n"
				"\t\t Blue: %06x\n"
		,
		//target_width,
		//target_height,
		bits_per_pixel,
		scanline_pad,
		depth,
		scanline_unit,
		byte_order, byte_order_text,
		bit_order, bit_order_text,
		visual->red_mask, visual->green_mask, visual->blue_mask
	);

	/*//
	// Make a window for testing:
	//
	xcb_window_t win;
	uint16_t
		width = 150,
		height = 150
	;
	win = xcb_generate_id(conn);	// Ask for our window's Id.
	// Create the window:
	xcb_create_window (conn,		// connection
		XCB_COPY_FROM_PARENT,		// depth (same as root)
		win,				// window Id
		screen->root,			// parent window
		0, 0,				// x, y
		150, 150,			// width, height
		10,				// border_width
		XCB_WINDOW_CLASS_INPUT_OUTPUT,	// class
		screen->root_visual,		// visual
		0, NULL  	                // masks, not used yet 
	);
	xcb_map_window(conn, win);	// Map the window on the screen.*/


	// Initialise atoms (needed below):
	// https://github.com/awesomeWM/awesome/blob/ad0290bc1aac3ec2391aa14784146a53ebf9d1f0/common/atoms.c
	// https://github.com/awesomeWM/awesome/blob/ad0290bc1aac3ec2391aa14784146a53ebf9d1f0/build-utils/atoms-int.sh#L12
	// https://github.com/awesomeWM/awesome/blob/ad0290bc1aac3ec2391aa14784146a53ebf9d1f0/build-utils/atoms-ext.sh#L6
	xcb_intern_atom_cookie_t iac;
	xcb_intern_atom_reply_t *iar;
	//
	iac = xcb_intern_atom_unchecked(conn, false, sizeof("ESETROOT_PMAP_ID")-1, "ESETROOT_PMAP_ID");
	if (!(iar = xcb_intern_atom_reply(conn, iac, NULL))) {
		fprintf(stderr, "Failed to get internal atom for ESETROOT_PMAP_ID.\n");
		return false;
	}
	esetroot_pmap_id = iar->atom;
	p_delete(&iar);
	//
	iac = xcb_intern_atom_unchecked(conn, false, sizeof("_XROOTPMAP_ID")-1, "_XROOTPMAP_ID");
	if (!(iar = xcb_intern_atom_reply(conn, iac, NULL))) {
		fprintf(stderr, "Failed to get internal atom for _XROOTMAP_ID.\n");
		return false;
	}
	_xrootpmap_id = iar->atom;
	p_delete(&iar);


	return true;
}


//monitor_info* const get_monitor_info(xcb_connection_t *conn, xcb_window_t win) {
monitor_info* const get_monitor_info() {
	if (!(conn || init_xcb())) return false;
	assert(screen);

	xcb_randr_get_screen_resources_current_reply_t *srcr;
	if (!(srcr = xcb_randr_get_screen_resources_current_reply(conn,
		xcb_randr_get_screen_resources_current(conn, screen->root),
		NULL
	))) {
		fprintf(stderr, "Failed to query monitor info with RandR extension\n");
		return NULL;
	}

	xcb_timestamp_t timestamp = srcr->config_timestamp;
	const int len = xcb_randr_get_screen_resources_current_outputs_length(srcr) /4; // 32b --> 8b.
	assert(len > 0);
	if (len != 1) fprintf(stderr,
		"Warning! This program has not been tested with multiple monitors.\n"
		"\tIt will probably not work as intended, if at all.\n"
	);
	xcb_randr_output_t *randr_outputs = xcb_randr_get_screen_resources_current_outputs(srcr);
	monitor_info * const monitors = calloc(len, sizeof(monitor_info));
	for (int i = 0; i < len; i++) {
		xcb_randr_get_output_info_reply_t *output;
		if (!(output = xcb_randr_get_output_info_reply(conn,
			xcb_randr_get_output_info(conn, randr_outputs[i], timestamp),
			NULL
		))) continue;

		if (
			   output->crtc == XCB_NONE
			|| output->connection == XCB_RANDR_CONNECTION_DISCONNECTED
		) continue;
		if (output->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
			fprintf(stderr, "xcb_randr_get_output_info_reply returned bad status: %hu\n", output->status);
		}

		xcb_randr_get_crtc_info_reply_t *crtc = xcb_randr_get_crtc_info_reply(conn,
			xcb_randr_get_crtc_info(conn, output->crtc, timestamp),
			NULL
		);
		switch (crtc->rotation) {
			case XCB_RANDR_ROTATION_ROTATE_0 :
				break;
			case XCB_RANDR_ROTATION_ROTATE_90 :
			case XCB_RANDR_ROTATION_ROTATE_180 :
			case XCB_RANDR_ROTATION_ROTATE_270 :
			case XCB_RANDR_ROTATION_REFLECT_X :
			case XCB_RANDR_ROTATION_REFLECT_Y :
				fprintf(stderr, "This program does not yet support rotation or reflection of wallpapers to match monitor.\n");
				break;
			default:
				fprintf(stderr, "RandR reported unknown rotation state: %u\n", crtc->rotation);
				free(crtc);
				free(output);
				free(monitors);
				free(srcr);
				return NULL;
		}

		monitors[i] = (monitor_info){
			.id = randr_outputs[i],
			.name = xcb_randr_get_output_info_name(output),
			.width = crtc->width,
			.height = crtc->height,
			.rotation = crtc->rotation
		};
		if (verbosity >= 4) {
			printf(
				"Detected monitor (%d): \"%s\"\n"
					"\t Width: %*u\n"
					"\tHeight: %*u\n"
				, i
				, monitors[i].name
				, 4, monitors[i].width
				, 4, monitors[i].height
			);
		}
		free(crtc);
		free(output);
	}

	free(srcr);
	return monitors;
}

/*uint32_t get_active_monitor() {
	assert(conn);
	assert(screen);

	xcb_randr_get_monitors_request
}*/


void get_dock_size_recursively(xcb_connection_t *conn, xcb_window_t *win, const uint32_t vdesktop_id, const atom_pack * const atoms,  XY_Dimensions * const result) {
	xcb_query_tree_cookie_t qtc;
	xcb_query_tree_reply_t *qtr;
	xcb_generic_error_t *err;


	if (atoms->wm_desktop_atom.id) {	// Do not check windows that are reported to not be on the user's current virtual desktop:
		xcb_get_property_reply_t *wm_desktop_reply;
		if ((wm_desktop_reply = get_property_reply(conn, win,
			atoms->wm_desktop_atom.id,
			XCB_ATOM_CARDINAL,
			4,	// 8b --> 32b
			0	// Silent.
		))) {
			uint32_t *wm_desktop_atom_value = (uint32_t*){xcb_get_property_value(wm_desktop_reply)};
			if (*(xcb_atom_t*)wm_desktop_atom_value != vdesktop_id) {	// Reported not on current virtual desktop.
				p_delete(&wm_desktop_reply);
				return;	// Do not count this window or its children.
			}
			p_delete(&wm_desktop_reply);
		}
	}


	{	// Aggregate strut dimensions if present on the window:
		xcb_get_property_reply_t *query_atom_reply;
		if ((query_atom_reply = get_property_reply(conn, win,
			atoms->query_atom.id,
			XCB_ATOM_ATOM,
			(strlen(atoms->query_atom.name)*8)/32,	// 8b --> 32b
			0	// Silent.
		))) {
			uint32_t *query_atom_value = (uint32_t*){xcb_get_property_value(query_atom_reply)};
			if (*(xcb_atom_t*)query_atom_value == atoms->match_atom.id) {
				xcb_get_property_reply_t *strut_partial_reply;
				if ((strut_partial_reply = get_property_reply(conn, win,
					atoms->key_atom.id,
					XCB_ATOM_CARDINAL,
					12*4,	// CARDINAL[12], each 32bit.
					1
				))) {
					uint32_t *valuee = (uint32_t*){xcb_get_property_value(strut_partial_reply)};
					result->width += valuee[0] + valuee[1];		// Left + Right.
					result->height += valuee[2] + valuee[3];	// Top + Bottom.
					p_delete(&strut_partial_reply);
				} else {
					fprintf(stderr, "Failed to query dock window's strut dimensions.\n");
				}
				p_delete(&query_atom_reply);
				return;	// Assuming any child window struts would be included in the parent's.
			}
			p_delete(&query_atom_reply);
		}
	}



	// Recurse through children:

	qtc = xcb_query_tree(conn, *win);
	if (!(qtr = xcb_query_tree_reply(conn, qtc, &err))) {
		if (err) handle_error(conn, err);
		return;
	}

	xcb_window_t *children = xcb_query_tree_children(qtr);
	for (int i = 0; i < xcb_query_tree_children_length(qtr); i++) {
		get_dock_size_recursively(conn, &children[i], vdesktop_id, atoms, result);
	}

	free(qtr);
	return;
}
void get_dock_size(xcb_connection_t *conn, xcb_window_t *win, XY_Dimensions * const result) {
	uint32_t virtual_desktop_id = 0;
	bool has_current_desktop = false;
	const atom_pair Net_Current_Desktop = {
		.name = "_NET_CURRENT_DESKTOP",
		.id = get_atom(conn, "_NET_CURRENT_DESKTOP")
	};
	{
		xcb_get_property_reply_t *current_desktop_reply;
		if ((current_desktop_reply = get_property_reply(conn, win,
			Net_Current_Desktop.id, XCB_ATOM_CARDINAL, 4,
			0	// Silent.
		))) {
			uint32_t *current_desktop_value = (uint32_t*){xcb_get_property_value(current_desktop_reply)};
			// Should probably add data validation.
			virtual_desktop_id = *current_desktop_value;
			p_delete(&current_desktop_reply);
			has_current_desktop = true;
			if (verbosity >= 3) printf("Calculating scale dimensions for current virtual desktop (%u).\n", virtual_desktop_id);
		}
	}


	//
	// Test whether window manager supports "_NET_WORKAREA". If so, use that.
	//
	const atom_pair Net_Workarea = {
		.name = "_NET_WORKAREA",
		.id = get_atom(conn, "_NET_WORKAREA")
	};
	{
		xcb_get_property_reply_t *workarea_atom_reply;
		if ((workarea_atom_reply = get_property_reply(conn, win,
			Net_Workarea.id, XCB_ATOM_CARDINAL, 4*4,
			0	// Silent.
		))) {
			printf("Using \"%s\". This functionality is untested! Please report bugs.\n", Net_Workarea.name);
			uint32_t **workarea_atom_value = (uint32_t**){xcb_get_property_value(workarea_atom_reply)};
			result->width = workarea_atom_value[virtual_desktop_id][2];
			result->height = workarea_atom_value[virtual_desktop_id][3];
			p_delete(&workarea_atom_reply);
			return;
		}
	}


	//
	// "_NET_WORKAREA" is unavailable. Fall back on iterating through all windows...
	// To do: limit search to windows on the current monitor.
	// It might be more efficient to use "_NET_FRAME_EXTENTS". I'm not sure how reliable that is.
	//

	bool
		//has_window_type = false,	// Assuming support if other criteria are met.
		has_workarea = false,
		has_dock = false,
		has_strut = false,
		has_strut_partial = false
	;
	const atom_pair Net_Wm_Window_Type_Dock = {
		.name = "_NET_WM_WINDOW_TYPE_DOCK",
		.id = get_atom(conn, "_NET_WM_WINDOW_TYPE_DOCK")
	};
	const atom_pair Net_Wm_Strut = {
		.name = "_NET_WM_STRUT",
		.id = get_atom(conn, "_NET_WM_STRUT")
	};
	const atom_pair Net_Wm_Strut_Partial = {
		.name = "_NET_WM_STRUT_PARTIAL",
		.id = get_atom(conn, "_NET_WM_STRUT_PARTIAL")
	};
	{
		const char *support_atom_name = "_NET_SUPPORTED";
		const xcb_atom_t NET_SUPPORTED = get_atom(conn, support_atom_name);
		xcb_get_property_reply_t *support_atom_reply;
		if (!(support_atom_reply = get_property_reply(conn, win,
			NET_SUPPORTED, XCB_ATOM_ATOM, 0,
			1
		))) {
			fprintf(stderr, "Failed to query atom: \"%s\"\n", support_atom_name);
			return;	// Give up.
		}

		uint32_t *support_atom_value = (uint32_t*){xcb_get_property_value(support_atom_reply)};
		assert(sizeof(support_atom_value[0]) == 4);
		const int length = xcb_get_property_value_length(support_atom_reply);	// 8b count of 32b units.
		assert(length >= sizeof(support_atom_value[0]));
		if (verbosity >= 6) {
			printf("Window manager claims to support these features (atoms):\n");
		}
		for (int i = 0; i < length/sizeof(support_atom_value[0]); i++) {
			xcb_atom_t checker = support_atom_value[i];
			if (verbosity >= 6) {
				char atom_name[MAX_ATOM_NAME_LEN+1];
				if (get_atom_name(conn, checker, atom_name)) {
					printf("\t%s\n", atom_name);
				} else {
					printf("\tFailed to query name for atom with decimal I.D. %u.\n", (uint32_t)checker);
				}
			}
			if (checker == Net_Workarea.id) {
				has_workarea = true;
				//if (verbosity >= 5) printf("\t\tSufficient. Ending search.\n");
				//break;
			} else if (checker == Net_Wm_Window_Type_Dock.id) has_dock = true;
			else if (checker == Net_Wm_Strut.id) has_strut = true;
			else if (checker == Net_Wm_Strut_Partial.id) has_strut_partial = true;
		}

		p_delete(&support_atom_reply);
	}
	if (has_workarea) {
		fprintf(stderr, "Window manager claims to support _NET_WORKAREA but my guess at how to use it was incorrect.\n");
	}
	if (!(has_dock && (has_strut || has_strut_partial))) {
		fprintf(stderr,
			"Failed to query potential dock size. Window manager is probably unsupported.\n"
				"\t%s : %s\n"
				"\t%s : %s\n"
				"\t%s : %s\n"
			, Net_Wm_Window_Type_Dock.name, (has_dock ? "true" : "false")
			, Net_Wm_Strut.name, (has_strut ? "true" : "false")
			, Net_Wm_Strut_Partial.name, (has_strut_partial ? "true" : "false")
		);
		return;
	}
	const atom_pack atoms = {
		.query_atom = {
			.name = "_NET_WM_WINDOW_TYPE",
			.id = get_atom(conn, "_NET_WM_WINDOW_TYPE"),
		},
		.match_atom = Net_Wm_Window_Type_Dock,
		.key_atom = has_strut ? Net_Wm_Strut : Net_Wm_Strut_Partial,
		.wm_desktop_atom = {
			.name = "_NET_WM_DESKTOP",
			.id = has_current_desktop ? get_atom(conn, "_NET_WM_DESKTOP") : 0
		}
	};
	get_dock_size_recursively(conn, win, virtual_desktop_id, &atoms, result);
}


bool write_to_pixmap(xcb_pixmap_t p, image_t *img) {
	// References:
	// 	http://metan.ucw.cz/blog/things-i-wanted-to-know-about-libxcb.html
	// 	https://gitlab.freedesktop.org/cairo/cairo/-/blob/8d6586f49f1c977318af7f7f9e4f24221c9122fc/src/cairo-xcb-connection-core.c#L104-144
	// 	https://github.com/ImageMagick/cairo/blob/5633024ccf6a34b6083cba0a309955a91c619dff/src/cairo-xcb-connection-core.c#L105
	// 	https://github.com/ImageMagick/cairo/blob/5633024ccf6a34b6083cba0a309955a91c619dff/src/cairo-xcb-connection-core.c#L227
	//	https://stackoverflow.com/questions/76184139/drawing-an-image-using-xcb
	// 		https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/208d550954c7266fa8093b02a2a97047e1478c00/src/PutImage.c#L884-886
	// 	https://lists.freedesktop.org/archives/xcb/2019-January/011197.html
	// 	https://cpp.hotexamples.com/examples/-/-/xcb_image_create/cpp-xcb_image_create-function-examples.html
	// 	https://github.com/etale-cohomology/xcb/blob/master/img.c
	// 	Bit shifting is because X11 almost always encodes lengths in denomination of 32-bit words-- not 8-bit.
	// 		This is apparently also why the max length is 262kB (65<<2).
	// 		https://www.x.org/releases/current/doc/bigreqsproto/bigreq.html
	// 	https://github.com/gnsmrky/wsl-vulkan-mesa
	const uint32_t req_len = sizeof(xcb_put_image_request_t);
	//const uint32_t req_len = 18;
	//size_t data_len = (img->scan_width * img->height);	// In 8-bit bytes!
	//uint32_t data_len = (img->scan_width * img->height);	// Is my scan_width equivalent to Cairo's stride?
	//uint32_t data_len = (img->bytes_per_row * img->height);	// Is my bytes_per_row equivalent to Cairo's stride?
	//size_t data_len = (img->scan_width * img->height);	// Is my scan_width equivalent to Cairo's stride?
	//const uint32_t len = (req_len + data_len) >> 2;	// Bit shift converts number of 8-bit bytes to number of 32-bit words.
	const size_t len = (req_len + img->data_len) >> 2;	// Bit shift converts number of 8-bit bytes to number of 32-bit words.
	//const size_t len = req_len + data_len;
	//const uint64_t max_req_len = xcb_get_maximum_request_length(conn);
	//printf("max_req_len: %u\n", max_req_len);
	const uint32_t max_req_len = xcb_get_maximum_request_length(conn);	// This is the length in 8-bit bytes?


	// Testing:
	//bits_per_pixel = img->scan_width / img->width;
	scanline_unit = 0;	// Testing based on ZPixmap case in xcb_image_create_native().
		// xcb_image_t definition in xcb_image.h says: ZPixmap (bpp!=1): will be max(8,bpp). Must be >= bpp.
	//scanline_pad = 24;
	//byte_order = XCB_IMAGE_ORDER_MSB_FIRST;
	bit_order = XCB_IMAGE_ORDER_MSB_FIRST;	// Texting based on ZPixmap case in xcb_image_create_native().
	char byte_order_text[30], bit_order_text[30];
	switch (byte_order) {
		case 0: strncpy(byte_order_text, "XCB_IMAGE_ORDER_LSB_FIRST", 30); break;
		case 1: strncpy(byte_order_text, "XCB_IMAGE_ORDER_MSB_FIRST", 30); break;
		default: strncpy(byte_order_text, "INVALID!", 30);
	}
	switch (bit_order) {
		case 0: strncpy(bit_order_text, "XCB_IMAGE_ORDER_LSB_FIRST", 30); break;
		case 1: strncpy(bit_order_text, "XCB_IMAGE_ORDER_MSB_FIRST", 30); break;
		default: strncpy(bit_order_text, "INVALID!", 30);
	}
	char image_format_text[30];
	switch (image_format) {
		case 0: strncpy(image_format_text, "XCB_IMAGE_FORMAT_XY_BITMAP", 30); break;
		case 1: strncpy(image_format_text, "XCB_IMAGE_FORMAT_XY_PIXMAP", 30); break;
		case 2: strncpy(image_format_text, "XCB_IMAGE_FORMAT_Z_PIXMAP", 30); break;
		default: strncpy(image_format_text, "INVALID!", 30);
	}
	if (verbosity > 1) printf(
		"xcb_image_create("
			"\n\twidth:  %u"
			"\n\theight: %u"
			"\n\tformat: %u (%s)"
			"\n\tdepth: %u"
			"\n\tbits_per_pixel: %hhu"
			"\n\tscanline_unit:  %hhu"
			"\n\tscanline_pad: %hhu"
			"\n\tbyte_order: %u (%s)"
			"\n\tbit_order:  %u (%s)"
			"\n\tsize: %zd"
			"\n)\n",
		img->width,
		img->height,
		image_format, image_format_text,
		depth,
		bits_per_pixel,
		scanline_unit,
		scanline_pad,
		byte_order, byte_order_text,
		bit_order, bit_order_text,
		img->data_len
	);
	xcb_image_t *image = xcb_image_create(
		img->width, img->height,	// Both in pixels-- should not include padding.
		image_format,
		scanline_pad,
		depth,
		bits_per_pixel,
		scanline_unit,
		byte_order,
		bit_order,
		img->pixels,	// Freed with xcb_image_destroy().
		img->data_len,	// Not sure if safe to use source format rather than destination format.
		img->pixels
	);
	/*xcb_image_t *image = xcb_image_create_native(conn,
		img->width, img->height,	// Width, Height.
		image_format,			// Format.
		depth,				// Depth.
		/*NULL, 				// "base".
		0,			 	// Data length (bytes).
		NULL				// Data.*//*
		img->pixels,			// "base".
		img->data_len,		 	// Data length (bytes).
		img->pixels			// Data.
	);*/
	if (!image) {
		fprintf(stderr, "Failed to load image.\n");
		return false;
	}
	//image->data = img->pixels;
	if (!image->data) {
		fprintf(stderr, "Failed to get image data.\n");
		xcb_image_destroy(image);	// Not sure if this should be used.
		return false;
	}
	//xcb_image_native(conn, image, true);
	if (xcb_image_native(conn, image, false)) {
		if (verbosity > 1) printf("Image loaded in native format.\n");
	} else {
		if (verbosity > 0) fprintf(stderr, "Image loaded is not in native format.\n");

		/*if (xcb_image_t *tmp = xcb_image_native(conn, image, true)) {*/
		xcb_image_t *tmp;
		if (tmp = xcb_image_native(conn, image, true)) {
			if (tmp != image) {
				if (verbosity > 0 && tmp->data == image->data) printf("tmp->data == image->data\n");
				xcb_image_destroy(image);
				image = tmp;
				if (!image->data) {
					fprintf(stderr, "image->data deleted. Aborting.\n");
					return false;
				}
				if (verbosity > 1) printf(
					"Converting:\n"
						"\t       width: %hu --> %hu\n"
						"\t      height: %hu --> %hu\n"
						"\t      format: %u --> %u\n"
						"\tscanline_pad: %hhu --> %hhu\n"
						"\t       depth: %hhu --> %hhu\n"
						"\t         bpp: %hhu --> %hhu\n"
						"\t        unit: %hhu --> %hhu\n"
						"\t  plane_mask: %u --> %u\n"
						"\t  byte_order: %u --> %u\n"
						"\t   bit_order: %u --> %u\n"
						"\t      stride: %u --> %u\n"
						"\t        size: %u --> %u\n"
					,
					image->width, tmp->width,
					image->height, tmp->height,
					image->format, tmp->format,
					image->scanline_pad, tmp->scanline_pad,
					image->depth, tmp->depth,
					image->bpp, tmp->bpp,
					image->unit, tmp->unit,
					image->plane_mask, tmp->plane_mask,
					image->byte_order, tmp->byte_order,
					image->bit_order, tmp->bit_order,
					image->stride, tmp->stride,
					image->size, tmp->size
				);
			}
		} else {
			fprintf(stderr, "Conversion failed.\n");
			xcb_image_destroy(image);	// Not sure if this should be used.
			return false;
		}
		if (xcb_image_native(conn, image, false)) {	// Double check validity, since I'm not confident I understand how this works.
			if (verbosity > 1) printf("Conversion succeeded.\n");
		} else {
			fprintf(stderr, "Conversion failed.\n");
			xcb_image_destroy(image);	// Not sure if this should be used.
			return false;
		}
	}


	/*unsigned short dst_x = 0, dst_y = 0;
	if (img->width != target_width) {
		if (img->width < target_width) dst_x = (target_width - img->width) / 2;
		else {
			fprintf(stderr, "Target image is too large for the screen.\n");
			return false;
		}
	}
	if (img->height != target_height) {
		if (img->height < target_height) dst_y = (target_height - img->height) / 2;
		else {
			fprintf(stderr, "Target image is too large for the screen.\n");
			return false;
		}
	}*/


	if (verbosity > 2) {
		printf("Unshifted:\n\tmax: %ju\n\tlen: %zd (max-%zd)\n",	// Would represent the 8-bit data byte size?
			(uintmax_t)max_req_len,
			len,
			(size_t)max_req_len - len
		);
		printf("Shifted:\n\tmax: %ju\n\tlen: %zd (max-%zd)\n",		// Would represent X11's 32-bit word size?
			(uintmax_t)max_req_len <<2,
			len<<2,
			((size_t)max_req_len - len)<<2
		);
	}
	if (len < max_req_len) {
	//if (false) {
		if (verbosity > 1) printf("Using non-buffered mode (len < max_req_len).\n");

		//
		// This method does not require data length or base to be specified.
		//
		// https://opensource.apple.com/source/X11libs/X11libs-60/xcb-util/xcb-util-0.3.6/image/xcb_image.h.auto.html
		// https://opensource.apple.com/source/X11libs/X11libs-60/xcb-util/xcb-util-0.3.6/image/xcb_image.c.auto.html
		/*xcb_image_t *image = xcb_image_create_native(conn,
			img->width, img->height,	// Width, Height.
			XCB_IMAGE_FORMAT_Z_PIXMAP,	// Format.
			screen->root_depth,		// Depth.
			*//*NULL, 				// "base".
			0,			 	// Data length (bytes).
			NULL				// Data.*//*
			(uint8_t*)img->pixels,		// "base".
			data_len,		 	// Data length (bytes).
			(uint8_t*)img->pixels		// Data.
		);
		if (!image) {
			fprintf(stderr, "Failed to load image.\n");
			return false;
		}
		//image->data = img->pixels;
		if (!image->data) {
			fprintf(stderr, "Failed to get image data.\n");
			return false;
		}*/

		/*cookie = xcb_image_put(conn,	// Load image into pixmap.
			p,
			//screen->default_colormap,
			gc,
			image,
			0, 0,		// x, y
			0
		);*/
		cookie = xcb_put_image_checked(conn,	// Load image into pixmap.
			image_format,			// format
			p,				// drawable
			//screen->default_colormap,	// gc
			gc,				// gc
			//img->width, img->height,	// width, height
			//img->scan_width, img->height,	// width, height
			image->width, image->height,	// width, height
			0, 0,				// dst_x, dst_y
			0,				// left_pad
			//screen->root_depth,		// depth
			image->depth,			// depth
			//img->data_len,			// data_len
			image->size,			// data_len
			//img->pixels			// data
			image->data			// data
		);
		if ((error = xcb_request_check(conn, cookie))) {
			fprintf(stderr, "Failed to put image.\n");
			handle_error(conn, error);
			xcb_image_destroy(image);
			return false;
		}
		//xcb_flush(conn);
		//xcb_image_destroy(image);	// Not sure if this should be used.


		// 
		// This method does not use create_native.
		//
		/*cookie = xcb_put_image_checked(conn,	// Load image into pixmap.
			XCB_IMAGE_FORMAT_Z_PIXMAP,	// format
			p,				// drawable
			//screen->default_colormap,	// gc
			gc,				// gc
			img->width, img->height,	// width, height
			0, 0,				// dst_x, dst_y
			0,				// left_pad
			screen->root_depth,		// depth
			data_len,			// data_len
			img->pixels			// data
		);
		if ((error = xcb_request_check(conn, cookie))) {
			fprintf(stderr, "Failed to put image.\n");
			*//*unsigned long i = 0;
			//while (i < len) {
			while (i < data_len) {
				//printf("%lu: %x\n", i, (unsigned int)*(bits+i));
				printf("%lu: %02x %02x %02x\n", i, (unsigned int)*(img->pixels+i++), (unsigned int)*(img->pixels+i++), (unsigned int)*(img->pixels+i++));
				//i += bytes_per_pixel;
			}*//*
			//printf("%lu: %x\n", i, (unsigned int)*(bits+i));
			handle_error(conn, error);
			fprintf(stderr, "len < max_req_len\n");
			std::cerr << "data_len: " << std::to_string(data_len) << std::endl;
			std::cerr << "req_len: " << std::to_string(req_len) << std::endl;
			std::cerr << "len: " << std::to_string(len) << std::endl;
			return false;
		}*/
	} else {	// len >= max_req_len
		if (verbosity > 1) printf("Using buffered mode (len >= max_req_len).\n");

		//
		// This method does not require data length or base to be specified.
		//
		/*xcb_image_t *tmp = xcb_image_create_native(conn,
			img->width, img->height,	// Width, Height.
			XCB_IMAGE_FORMAT_Z_PIXMAP,	// Format.
			screen->root_depth,		// Depth.
			NULL, 				// "base".
			0,			 	// Data length (bytes).
			NULL				// Data.
		);
		if (!tmp) {
			fprintf(stderr, "Failed to load image.\n");
			return false;
		}
		tmp->data = img->pixels;
		if (!tmp->data) {
			fprintf(stderr, "Failed to get image data.\n");
			return false;
		}*/

		/*cookie = xcb_image_put(conn,	// Load image into pixmap.
			p,
			screen->default_colormap,
			tmp,
			0, 0,		// x, y
			0
		);
		if ((error = xcb_request_check(conn, cookie))) {
			fprintf(stderr, "Failed to put image.\n");
			handle_error(conn, error);
		}
		xcb_image_destroy(tmp);	// Not sure if this should be used.*/


		// 
		// This method does not use create_native.
		//
		//const int rows_per_request = (max_req_len<<2 - req_len) / img->scan_width;
		//const int rows_per_request = (max_req_len - req_len) / img->bytes_per_row;
		//int y_off;
		//int dst_y = 0;
		//unsigned char *data_src = img->pixels;
		BYTE *data_src = image->data;
		//int this_req_len, this_req_width, this_req_height;	// I'm hesitant to make this unsigned while testing.
		int rows_remaining = image->height;
		//unsigned int bytes_remaining = img->width * img->height * screen->root_depth;
		//unsigned int bytes_remaining = data_len;
		unsigned short req_num = 0;
		//for (y_off = 0; y_off + rows_per_request < img_y; y_off += rows_per_request) {

		//int rows = (max_req_len - req_len - 4) / img->scan_width;	// 32-bit word is 4* 8-bit bytes.
		//int rows = (max_req_len<<2 - req_len - 4) / img->scan_width;	// 32-bit word is 4* 8-bit bytes.
		//int rows = ( (max_req_len<<2) - req_len - 4) / img->scan_width;	// 32-bit word is 4* 8-bit bytes.
		int rows = ( (max_req_len<<2) - req_len - 4) / image->stride;	// 32-bit word is 4* 8-bit bytes.
			// Is my req_len equivalent to Cairo's req_size?
			// Is my scan_width equivalent to Cairo's stride?
		//for (y_off = 0; y_off + rows_per_request <= img->height; y_off += rows_per_request) {
		if (rows <= 0) {
			fprintf(stderr, "Error: rows <= 0\n");
			xcb_image_destroy(image);
			return false;
		}
		do {
			//if (rows > img->height) rows = img->height;
			if (rows_remaining < rows) rows = rows_remaining;
			//size_t data_len = rows * img->scan_width;	// rows*stride
			size_t data_len = rows * image->stride;	// rows*stride
			if (data_len + req_len >= max_req_len<<2) {
				fprintf(stderr, "Data length is too long. Aborting.");
				xcb_image_destroy(image);
				return false;
			}

			req_num++;
			if (xcb_connection_has_error(conn)) {
				fprintf(stderr, "Connection has error! (put request #%d)\n", req_num);
				//xcb_disconnect(conn);
				//free(img->pixels);
					// The free() above causes an error. Is it in glibc?
					// "corrupted size vs. prev_size while consolidating"
				//free(img);
				xcb_image_destroy(image);
				return false;
			}

			/*if (y_off + rows_per_request <= img_y) {
				this_req_height = rows_per_request;
				this_req_width = img_x;
				this_req_len = bytes_per_row * rows_per_request;
			} else {	// This never happens.
				//this_req_len = img_y - y_off;
				//this_req_len = ((img_y - y_off) * bytes_per_row) + req_len;
				this_req_height = img_y - y_off;
				this_req_width = img_x;
				//this_req_width = this_req_height == 1 ? bytes_per_row -  : img_x;
				this_req_len = (this_req_height * bytes_per_row);
				//this_req_len = (this_req_height * bytes_per_row) + req_len;
			}*/
			/*if (bytes_remaining > img->width) {
				this_req_width = img->width;
				this_req_height = rows_per_request;
			} else {
				this_req_width = bytes_remaining;
				this_req_height = 1;
			}
			//this_req_len = img->bytes_per_row * this_req_height;
			this_req_len = this_req_width * this_req_height * screen->root_depth;
			//this_req_len = img->bytes_per_row * rows_remaining;
			//this_req_len = (y_off + this_req_height) * img->bytes_per_row;
			cookie = xcb_put_image_checked(conn,		// Load image into pixmap.
				XCB_IMAGE_FORMAT_Z_PIXMAP,			// format
				//XCB_IMAGE_FORMAT_XY_PIXMAP,			// format.
				p,						// drawable
				//screen->default_colormap,			// gc
				gc,						// gc
				this_req_width, this_req_height,		// width, height
				//this_req_width, rows_remaining,			// width, height
				0, y_off,					// dst_x, dst_y
				0,						// left_pad
				screen->root_depth,				// depth
				this_req_len,					// data_len
				img->pixels + (y_off * img->bytes_per_row)	// data
			);*/
			cookie = xcb_put_image_checked(conn,		// Load image into pixmap.
				image_format,					// format.
				p,						// drawable
				gc,						// gc
				//img->width, rows,				// width, height
				image->width, rows,				// width, height
				0, image->height - rows_remaining,		// dst_x, dst_y
				0,						// left_pad
				screen->root_depth,				// depth
				data_len,					// data_len
				//img->pixels + (data_len * req_num)		// data
				data_src					// data
			);
			if ((error = xcb_request_check(conn, cookie))) {
				fprintf(stderr, "Failed to put image.\n");
				/*unsigned long i = 0;
				//while (i < len) {
				while (i < data_len) {
					//printf("%lu: %x\n", i, (unsigned int)*(bits+i));
					printf("%lu: %02x %02x %02x\n", i, (unsigned int)*(img->pixels+i++), (unsigned int)*(img->pixels+i++), (unsigned int)*(img->pixels+i++));
					//i += bytes_per_pixel;
				}*/
				//printf("%lu: %x\n", i, (unsigned int)*(bits+i));
				handle_error(conn, error);
				fprintf(stderr, "len >= max_req_len\n");
				fprintf(stderr, "Number of requests (up to and including failure): %d\n", req_num);
				fprintf(stderr, "data_len: %ld (max-%ld)\n", data_len, max_req_len - data_len);
				fprintf(stderr, "req_len: %d\n", req_len);
				/*std::cerr << "len: " << std::to_string(len)
					<< "  (max-" << std::to_string(max_req_len - len) << ")"
				<< std::endl;*/
				fprintf(stderr, "max_req_len:  %d\n", max_req_len);
				//std::cerr << "this_req_len: " << std::to_string(this_req_len) << std::endl;
				//std::cerr << "rows_per_request: " << std::to_string(rows_per_request) << std::endl;
				fprintf(stderr, "rows requested: %d\n", rows);
				fprintf(stderr, "rows_remaining: %d\n", rows_remaining);
				//std::cerr << "dst_y: " << std::to_string(dst_y) << std::endl;
				fprintf(stderr, "dst_y: %d\n", img->height - rows_remaining);
				fprintf(stderr, "pixel_count: %d - %d / %d\n",
					img->width * (img->height - rows_remaining),
					rows * img->width,
					img->width * img->height
				);
				xcb_image_destroy(image);
				return false;
			}
			//rows_remaining -= this_req_height;
			//bytes_remaining -= this_req_len;
			rows_remaining -= rows;
			//dst_y += rows;
			data_src += data_len;
			//data_src = (unsigned char*)data_src + data_len;
		} while (rows_remaining);
		if (verbosity > 2) printf("Number of requests: %u\n", req_num);
	}
	/*xcb_render_composite_checked(conn,
		XCB_RENDER_PICT_OP_OVER,		// Operation (PICTOP).
		img,					// Source (PICTURE).
		img,					// Mask (PICTURE or NONE).
		screen->root,				// Destination (PICTURE).
		0, 0,					// Source start coordinates (INT16).
		0, 0,					// Mask start coordinates (INT16)?
		0, 0,					// Destination start coordinates (INT16).
		//width, height				// Source dimensions to copy.
		x, y					// Source dimensions to copy.
	);
	if ((err = xcb_request_check(conn, cookie))) {
		cerr << "Failed to render composite image." << endl;
		continue;
	}*/

	xcb_image_destroy(image);
	return true;
}
bool set_wallpaper(const file_path_t wallpaper_file_path, const xcb_randr_output_t monitor) {
	if (!wallpaper_file_path || *wallpaper_file_path == '\0') return false;
	if (!(conn || init_xcb())) return false;
	assert(screen);


	// 
	// Get target dimensions for wallpaper:
	//

	uint16_t
		target_width = screen->width_in_pixels,
		target_height = screen->height_in_pixels
	;

	// Adjust target dimensions to account for visible background area.
	if (scale_for_wm) {
		XY_Dimensions dock = {0};
		// Should probably specify the target monitor.
		get_dock_size(conn, &screen->root, &dock);
		target_width -= dock.width;
		target_height -= dock.height;
	}

	if (verbosity >= 3) printf(
		"Target dimensions for wallpaper:\n"
			"\t Width:  %d\n"
			"\tHeight:  %d\n"
		, target_width
		, target_height
	);


	//
	// Prepare the wallpaper data:
	//

	// https://stackoverflow.com/a/77404684

	// Create a pixmap buffer.
	xcb_pixmap_t p = xcb_generate_id(conn);
	xcb_create_pixmap(conn, screen->root_depth, p, screen->root, target_width, target_height);
	if (verbosity > 2) printf("New pixmap (p): \n\t  %u\n\t0x%x\n", p, p);
	xcb_aux_sync(conn);	// Not sure if necessary.
	//xcb_flush(conn);

	// Make a graphical context.
	// https://www.x.org/releases/current/doc/man/man3/xcb_change_gc.3.xhtml
	gc = xcb_generate_id(conn);
	const uint32_t gc_values[] = {screen->black_pixel, screen->white_pixel};
	xcb_create_gc(conn, gc, p,
		XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
		gc_values
	);

	if (xcb_connection_has_error(conn)) {
		fprintf(stderr, "Connection has error! (-10)\n");
		xcb_disconnect(conn);
		//init_xcb();
		return false;
	}


	image_t *img = get_pixel_data(
		wallpaper_file_path,
		bits_per_pixel,
		target_width,
		target_height
	);

	if (!write_to_pixmap(p, img)) {
		//free(img->pixels);	// Handled by xcb_image_destroy().
		free(img);
		xcb_disconnect(conn);
		return false;
	}
	//free(img->pixels);	// Handled by xcb_image_destroy().
	free(img);
	//
	// https://opensource.apple.com/source/X11libs/X11libs-60/xcb-util/xcb-util-0.3.6/image/xcb_image.h.auto.html
	// https://opensource.apple.com/source/X11libs/X11libs-60/xcb-util/xcb-util-0.3.6/image/xcb_image.c.auto.html
	/*image_t *img = get_pixel_data(wallpaper_file_path);
	gc = xcb_generate_id(conn);
	xcb_pixmap_t p = xcb_create_pixmap_from_bitmap_data(conn,
		screen->root,
		img->pixels,
		img->width, img->height,
		//(unsigned char*)neko_bits,
		//neko_width, neko_height,
		//img->width * 3, img->height * 3,
		//img->width * 24, img->height * 24,	// Segfaults.
		//img->scan_width, img->height,
		//img->width * 4, img->height * 4,
		screen->root_depth,
		screen->black_pixel,	// fg
		screen->white_pixel,	// bg
		&gc
	);
	if (p) {
		xcb_flush(conn);
		free(img);
		printf("New pixmap (p): \n\t  %u\n\t0x%x\n", p, p);
	} else {
		free(img);
		fprintf(stderr, "Failed to create pixmap from bitmap data.\n");
		return false;
	}*/


	if (xcb_connection_has_error(conn)) {
		fprintf(stderr, "Connection has error! (-3)\n");
		xcb_disconnect(conn);
		//init_xcb();
		return false;
	}

	// https://github.com/awesomeWM/awesome/blob/7ed4dd620bc73ba87a1f88e6f126aed348f94458/root.c#L91
	// https://github.com/awesomeWM/awesome/blob/7ed4dd620bc73ba87a1f88e6f126aed348f94458/root.c#L57

	//
	// Prevent sending PropertyNotify event:
	//
	xcb_grab_server(conn);
	const uint32_t values[] = {0};
	xcb_change_window_attributes(conn,
		screen->root,
		XCB_CW_EVENT_MASK,
		//(uint32_t[]) { 0 }
		//NULL
		//NO_EVENT_VALUE
		values
	);

	if (xcb_connection_has_error(conn)) {
		fprintf(stderr, "Connection has error! (-2)\n");
		xcb_disconnect(conn);
		//init_xcb();
		return false;
	}

	//
	// Set pixmap as the root window's background:
	//
	//xcb_change_window_attributes(conn, screen->root, XCB_CW_BACK_PIXMAP, &p);
	cookie = xcb_change_window_attributes_checked(conn, screen->root, XCB_CW_BACK_PIXMAP, &p);
	if ((error = xcb_request_check(conn, cookie))) {
		fprintf(stderr, "Failed to change window attribute.\n");
		handle_error(conn, error);
	}
	//xcb_clear_area(conn, 0, screen->root, 0, 0, 0, 0);
	cookie = xcb_clear_area_checked(conn, 0, screen->root, 0, 0, 0, 0);
	if ((error = xcb_request_check(conn, cookie))) {
		fprintf(stderr, "Failed to clear area.\n");
		handle_error(conn, error);
	}
	//xcb_change_window_attributes(conn, win, XCB_CW_BACK_PIXMAP, &p);
	//xcb_clear_area(conn, 0, win, 0, 0, 0, 0);
	//xcb_aux_sync(conn);	// Not sure if necessary.
	//xcb_flush(conn);

	if (xcb_connection_has_error(conn)) {
		fprintf(stderr, "Connection has error! (-1)\n");
		xcb_disconnect(conn);
		//init_xcb();
		return false;
	}


	//
	// Get old wallpaper, to free later.
	//
	// echo $(($(xprop -root ESETROOT_PMAP_ID | cut -d\  -f5)))
	// xprop -root | grep -E 'ESETROOT_PMAP_ID|_XROOTPMAP_ID'
	//xcb_get_property_cookie_t prop_c = xcb_get_property_unchecked(conn, false,
	xcb_get_property_cookie_t prop_c = xcb_get_property(conn, false,
	//cookie = xcb_get_property(conn, false,
		//screen->root, ESETROOT_PMAP_ID, XCB_ATOM_PIXMAP, 0, 1
		screen->root, esetroot_pmap_id, XCB_ATOM_PIXMAP, 0, 1
		//win, ESETROOT_PMAP_ID, XCB_ATOM_PIXMAP, 0, 1
	);
	/*//if ((error = xcb_request_check(conn, cookie))) {
	if ((error = xcb_request_check(conn, prop_c))) {
		fprintf(stderr, "Failed to get property.\n");
		handle_error(conn, error);
	}*/
	//if (! (prop_c || prop_c.sequence)) {
	if (prop_c.sequence == 0) {
		char atom_name[MAX_ATOM_NAME_LEN+1];
		get_atom_name(conn, esetroot_pmap_id, atom_name);
		fprintf(stderr,
			"Null or zero sequence returned by xcb_get_property. Not sure if normal.\n"
				"\tAtom name: %s"
				"\tAtom I.D.: %d"
				"\tCookie sequence: %d"
			,
			atom_name,
			esetroot_pmap_id,
			prop_c.sequence
		);
	}


	//
	// Set properties, so clients can reference for pseudo-transparency:
	//
	//xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root, _XROOTPMAP_ID, XCB_ATOM_PIXMAP, 32, 1, &p);
	//xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root, ESETROOT_PMAP_ID, XCB_ATOM_PIXMAP, 32, 1, &p);
	//cookie = xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root, _xrootpmap_id, XCB_ATOM_PIXMAP, 32, 1, &p);
	cookie = xcb_change_property_checked(conn, XCB_PROP_MODE_REPLACE, screen->root, _xrootpmap_id, XCB_ATOM_PIXMAP, 32, 1, &p);
	if ((error = xcb_request_check(conn, cookie))) {
		fprintf(stderr, "Failed to change window property: _XROOTPMAP_ID\n");
		handle_error(conn, error);
	}
	//cookie = xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root, esetroot_pmap_id, XCB_ATOM_PIXMAP, 32, 1, &p);
	cookie = xcb_change_property_checked(conn, XCB_PROP_MODE_REPLACE, screen->root, esetroot_pmap_id, XCB_ATOM_PIXMAP, 32, 1, &p);
	if ((error = xcb_request_check(conn, cookie))) {
		fprintf(stderr, "Failed to change window property: ESETROOT_PMAP_ID\n");
		handle_error(conn, error);
	}


	//
	// Free old wallpaper (only ESETROOT_PMAP_ID):
	//
	//xcb_get_property_reply_t *prop_r = xcb_get_property_reply(conn, prop_c, NULL);
	xcb_get_property_reply_t *prop_r = xcb_get_property_reply(conn, prop_c, &error);
	//xcb_poll_for_reply(conn, prop_c, &prop_r, &error);	// https://xcb.freedesktop.org/ProtocolExtensionApi/
	xcb_aux_sync(conn);	// Not sure if necessary.
	xcb_flush(conn);	// Probably not necessary.
	//if (prop_r && prop_r->value_len) {
	if (prop_r) {
		if (error) {
			fprintf(stderr, "Failed to delete old pixmap from server. Processing property reply error...\n");
			handle_error(conn, error);
		/*} else if (prop_r->type == XCB_NONE) {
			fprintf(stderr, "Failed to delete old pixmap from server. Property reply type is XCB_NONE.\n");
		} else if (xcb_get_property_value_length(prop_r)) {*/
		} else if (prop_r->type == XCB_NONE) {
			if (verbosity > 1) printf("No pre-existing wallpaper was detected.\n");
			// Ignore case where no previous wallpaper was active.
		} else {
			if (xcb_get_property_value_length(prop_r)) {
				// https://github.com/awesomeWM/awesome/blob/7ed4dd620bc73ba87a1f88e6f126aed348f94458/root.c#L83
				//xcb_pixmap_t *rootpix = xcb_get_property_value(prop_r);
				xcb_pixmap_t *rootpix = (xcb_pixmap_t*)xcb_get_property_value(prop_r);
				//if (rootpix) xcb_kill_client(conn, *rootpix);
				if (rootpix) {
					//printf("Deleting old pixmap from server: \n\t  %u\n\t0x%x\n", *rootpix, *rootpix);
					if (verbosity > 3) printf("Deleting old pixmap from server: \n\t  %u\n\t%#x\n", *rootpix, *rootpix);
					xcb_kill_client(conn, *rootpix);
				}
			} else {
				fprintf(stderr, "Failed to delete old pixmap from server. No error information provided with property reply.\n");
			}
		}
	} else {
		fprintf(stderr, "Failed to delete old pixmap from server. Property reply was not received.\n");
	}
	p_delete(&prop_r);



	// Reset sending PropertyNotify event:
	xcb_change_window_attributes(conn,
		screen->root,
		XCB_CW_EVENT_MASK,
		ROOT_WINDOW_EVENT_MASK
	);
	xcb_ungrab_server(conn);

	// Make sure pixmap is not destroyed on disconnect:
	xcb_set_close_down_mode(conn, XCB_CLOSE_DOWN_RETAIN_PERMANENT);

	// Clean up and complete.
	xcb_aux_sync(conn);	// Not sure if necessary.
	xcb_flush(conn);
	return true;
}

