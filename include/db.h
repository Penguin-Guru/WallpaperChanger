#pragma once

/*
 * This is a transitional interface for database implementation(s).
 * Currently,
 * 	It defines generic data types and utility functions used by database implementation(s).
 * 	It does not declare all functions called by the main program.
 */

#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include "util.h"

#define MAX_TAG_LENGTH 10


static const char Timestamp_Format[] = "%F@%T%z";
//void print_current_timestamp(const char *TimestampFormat);	// Default: TimestampFormat = Timestamp_Format.


enum Tag {	// Bitmask of known tags. Index is the bitmask's offset.
	// Remember to update tags_known (below).
	TAG_CURRENT = 0,	// Last recorded wallpaper set (per monitor).
	TAG_HISTORIC,		// Not the first time the wallpaper has been set.
	TAG_FAVOURITE		// User has marked wallpaper as a personal favourite.
};
typedef struct tag_t {
	enum Tag id;
	const char text[MAX_TAG_LENGTH];
} tag_t;
// Tags must appear in same order as enum due to use in get_current(). Fix later.
static const tag_t tags_known[] = {	// Must be kept in sync with Tag enum. Fix later.
	(tag_t){TAG_CURRENT,	"current"},
	(tag_t){TAG_HISTORIC,	"historic"},
	(tag_t){TAG_FAVOURITE,	"favourite"}
};
#define TAGS_KNOWN_CT sizeof(tags_known)/sizeof(tags_known[0])
static_assert(	// Bitmask must offer one bit for each known tag.
	TAGS_KNOWN_CT <= 8*sizeof(tags_t),
	"Data type of tags_t is not big enough."
);


typedef struct row_t {
	timestamp_t ts;
	char *monitor_name;
	file_path_t file;
	tags_t tags;
} row_t;
typedef struct rows_t {
	row_t** row;
	num_rows ct;
} rows_t;

void free_row(row_t *r);
void free_rows_contents(rows_t* target);	// Free stack allocations.
void free_rows(rows_t* target);			// Free heap allocations.


bool format_path(file_path_t path);


/*enum Order {	// Terminology relative to beginning of returned array.
	ASCENDING = 1,	// Oldest to newest.
	DESCENDING	// Newest to oldest.
};
typedef struct ts_query {
	Order order;
	uint_fast8_t limit = 0;
	tags_t p_criteria = 0;
	tags_t n_criteria = 0;
} ts_query;*/

