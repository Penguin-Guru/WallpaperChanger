#pragma once

#include <limits.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include "util.h"

#define MAX_TAG_LENGTH 10
//#define WHITESPACE_CHARACTERS " \t\r\n\v\f"
// COLUMN_DELIMS and TAG_DELIMS:
// 	These are used when reading from the database file.
// 	They require \n for strtok_r to parse final segment of line.
// 	Any character in either will have the same effect (for columns or tags respectively).
#define COLUMN_DELIMS ";\n"
#define TAG_DELIMS ",\n"
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

enum Tag {	// Index is offset in bitmask.
	TAG_CURRENT = 0,	// Currently the active wallpaper.
	TAG_HISTORIC,	// Not the first time the wallpaper has been set.
	TAG_FAVOURITE	// A personal favourite.
};
typedef struct tag_t {
	enum Tag id;
	//const char text[Max_Tag_Len];
	const char text[MAX_TAG_LENGTH];
} tag_t;
// Tags must appear in same order as enum due to use in get_current(). Fix later.
// Number limited by bitmask (sizeof(tags_t)*CHAR_BIT).
static const tag_t tags_known[] = {	// Must be keps in sync with Tag enum. Fix later.
	(tag_t){TAG_CURRENT,	"current"},
	(tag_t){TAG_HISTORIC,	"historic"},
	//(tag_t){TAG_NEW,	"new"},
	(tag_t){TAG_FAVOURITE,	"favourite"}
};
static const uint_fast8_t Max_Tag_String_Len = sizeof(tags_known)/sizeof(tags_known[0]) * (MAX_TAG_LENGTH + 1);	// +1 for commas.

//typedef char[MAX_TIMESTAMP_LENGTH] timestamp_t;
//using tags = Tag[];
typedef struct row_t {
	timestamp_t ts; 
	char *monitor_name;
	file_path_t file;	// Not flexible anymore. Appears in file order.
	tags_t tags;
	//file_path_t file;	// Flexible-- must be at end of struct.
} row_t;
bool print_rows(const char* path);	// Only useful for testing?

/*typedef struct file_t {
	FILE *path;
	size_t length;
	bool is_open;
} file_t;*/

typedef struct rows_t {
	row_t** row;
	num_rows ct;
} rows_t;
void free_row(row_t* r);
void free_rows(rows_t* target);



typedef uint_fast8_t token_length_t;
tags_t get_tag_mask(const char column_string[MAX_COLUMN_LENGTH]);
tags_t encode_tag(enum Tag tag);
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

//char* gen_tag_string(tags_t tags);
void gen_tag_string(char *string, tags_t tags);
num_rows add_tag_by_tag(const file_path_t file_path, tags_t *criteria, tags_t *tags_mod);

bool del_entry_by_tag(
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

