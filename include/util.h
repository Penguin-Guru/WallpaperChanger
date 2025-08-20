#pragma once

//#include <limits.h>
#include <stdint.h>

#define MAX_COLUMN_TITLE_LENGTH 10
#define MAX_COLUMN_LENGTH 255
#define MAX_TIMESTAMP_LENGTH 30	// Hopefully safe.
//#define MAX_MONITOR_NAME_LENGTH 30	// Arbitraty.
#define MAX_DIRECTORY_DEPTH 15


// Used primarily in db::row_t:
typedef unsigned char tags_t;	// Bitmask.
typedef char timestamp_t[MAX_TIMESTAMP_LENGTH];
typedef char* file_path_t;
typedef char* const monitor_name_t;

typedef uint_fast32_t num_rows;

