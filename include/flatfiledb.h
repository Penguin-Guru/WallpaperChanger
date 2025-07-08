#pragma once

#include <limits.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include "util.h"

#define MAX_TAG_LENGTH 10
// I would use all linebreak characters as line/entry delimiters but it isn't practical.
// I would use all non-breaking whitespace characters as column delimiters but space characters often appear in file names.
//#define LINEBREAK_CHARACTERS "\n\r\v\f"
//#define WHITESPACE_CHARACTERS " \t" LINEBREAK_CHARACTERS
// COLUMN_DELIMS and TAG_DELIMS:
// 	These are used when reading from the database file.
// 	They require \n for strtok_r to parse final segment of line.
// 	Any character in either will have the same effect (for columns or tags respectively).
#define COLUMN_DELIMS "\t\n"	// Must not appear within column fields.
#define TAG_DELIMS ",\n"	// Must not appear within tag labels.
// COLUMN_DELIM and TAG_DELIM:
// 	These are used when writing to the database file.
// 	The first character in the respective set above is preferred.
#define COLUMN_DELIM COLUMN_DELIMS[0]
#define TAG_DELIM TAG_DELIMS[0]

static const char Column_Names[][MAX_COLUMN_TITLE_LENGTH] = {	// Columns must be in (descending) order of appearance (when reading/writing).
	"timestamp",
	"monitor",
	"file_path",
	"tags"
};
static const uint_fast8_t NUM_COLUMNS = sizeof(Column_Names)/sizeof(Column_Names[0]);
static_assert(MAX_COLUMN_TITLE_LENGTH <= MAX_COLUMN_LENGTH);
static const uint_fast16_t MAX_ROW_LENGTH = NUM_COLUMNS * (MAX_COLUMN_LENGTH + 1);	// +1 for delimiters.

static const char Timestamp_Format[] = "%F@%T%z";
void print_current_timestamp(const char *TimestampFormat);	// Has default value.

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
static const uint_fast8_t Max_Tag_String_Len = TAGS_KNOWN_CT * (MAX_TAG_LENGTH + 1);	// +1 for commas.

typedef struct row_t {
	timestamp_t ts; 
	char *monitor_name;
	file_path_t file;
	tags_t tags;
} row_t;

/*typedef struct file_t {
	FILE *path;
	size_t length;
	bool is_open;
} file_t;*/

typedef struct rows_t {
	row_t** row;
	num_rows ct;
} rows_t;
void free_rows_contents(rows_t* target);	// Free stack allocations.
void free_rows(rows_t* target);			// Free heap allocations.

bool format_path(file_path_t path);
bool validate_string_value(const char *str);
bool validate_row(const row_t *r);


typedef uint_fast8_t token_length_t;
tags_t get_tag_mask(const char column_string[MAX_COLUMN_LENGTH]);
tags_t encode_tag(enum Tag tag);
void gen_tag_string(char *string, tags_t tags);
//inline tags_t sum_tags

row_t* get_row_if_match(
	const num_rows row_num,
	const char *row_string,
	tags_t *p_criteria,
	tags_t *n_criteria,	// Optional-- set to NULL.
	const char * const monitor_name	// Optional-- set to NULL.
);
rows_t* get_rows_by_tag(
	const file_path_t file_path,
	tags_t *p_criteria,
	tags_t *n_criteria,	// Optional-- set to NULL.
	const char * const monitor_name	// Optional-- set to NULL.
);
rows_t* get_current(
	const file_path_t file_path,
	const char * const monitor_name	// Optional-- set to NULL.
);

bool append_new_current(const file_path_t data_file_path, row_t *new_entry);

num_rows add_tag_by_tag(const file_path_t file_path, tags_t *criteria, tags_t *tags_mod);

rows_t* del_entry_by_tag(
	rows_t *ret_rows,
	const file_path_t file_path,
	tags_t *p_criteria,
	tags_t *n_criteria	// Optional-- set to NULL.
);
//bool del_entry_by_path(const file_path_t db_file_path, const file_path_t path_to_del);


/*enum Order {	// Terminology relative to beginning of returned array.
	ASCENDING = 1,	// Oldest to newest.
	DESCENDING	// Newest to oldest.
};
typedef struct ts_query {
	Order order;
	uint_fast8_t limit = 0;
	tags_t p_criteria = 0;
	tags_t n_criteria = 0;
} ts_query;
rows* get_rows_by_ts(const file_path_t file_path, ts_query *query);*/

