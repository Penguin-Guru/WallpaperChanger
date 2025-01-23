#pragma once

//#include <limits.h>
#include <stdint.h>

const uint_fast8_t
	MAX_COLUMN_TITLE_LENGTH = 10,
	MAX_COLUMN_LENGTH = 255,
	MAX_TIMESTAMP_LENGTH = 30,	// Hopefully safe.
	MAX_DIRECTORY_DEPTH = 15
;


// Used primarily in db::row_t:
using tags_t = unsigned char;	// Bitmask.
using timestamp_t = char[MAX_TIMESTAMP_LENGTH];
using file_path_t = char*;
using const_file_path_t = const char*;

using num_rows = uint_fast32_t;

