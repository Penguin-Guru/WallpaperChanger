#pragma once

#include <limits.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include "db.h"
#include "util.h"

// I would use all linebreak characters as line/entry delimiters but it isn't practical.
// I would use all non-breaking whitespace characters as column delimiters but space characters often appear in file names.
//#define LINEBREAK_CHARACTERS "\n\r\v\f"
//#define WHITESPACE_CHARACTERS " \t" LINEBREAK_CHARACTERS
// COLUMN_DELIMS and TAG_DELIMS:
// 	These are used when reading from the database file.
// 	Both require \n for strtok_r to parse final segment of line.
// 	Any character in either relevant set will have the same effect when reading.
// 	Only the first character in either relevant set will be used when writing.
#define COLUMN_DELIMS "\t\n"    // Must not appear within column fields.
#define TAG_DELIMS ",\n"        // Must not appear within tag labels.
// COLUMN_DELIM and TAG_DELIM:
// 	These are used when writing to the database file.
// 	The first character in the respective set above is preferred.
#define COLUMN_DELIM COLUMN_DELIMS[0]
#define TAG_DELIM TAG_DELIMS[0]

static const char Column_Names[][MAX_COLUMN_TITLE_LENGTH] = {   // Columns must be in (descending) order of appearance (when reading/writing).
	"timestamp",
	"monitor",
	"file_path",
	"tags"
};
static const uint_fast8_t NUM_COLUMNS = sizeof(Column_Names)/sizeof(Column_Names[0]);
static_assert(MAX_COLUMN_TITLE_LENGTH <= MAX_COLUMN_LENGTH);
static const uint_fast16_t MAX_ROW_LENGTH = NUM_COLUMNS * (MAX_COLUMN_LENGTH + 1);      // +1 for delimiters.

static const uint_fast8_t Max_Tag_String_Len = TAGS_KNOWN_CT * (MAX_TAG_LENGTH + 1);    // +1 for commas.

/*typedef struct file_t {
	FILE *path;
	size_t length;
	bool is_open;
} file_t;*/


bool validate_string_value(const char *str);
bool validate_row(const row_t *r);


typedef uint_fast8_t token_length_t;
tags_t get_tag_mask(const char column_string[MAX_COLUMN_LENGTH]);
#define encode_tag(t) (encode_tag(t))
tags_t (encode_tag)(enum Tag tag);	// Compile-time macro should be preferred.
void gen_tag_string(char *string, tags_t tags);

row_t* get_row_if_match(
	const num_rows row_num,
	const char *row_string,
	const tags_t * const p_criteria,
	const tags_t * const n_criteria,        // Optional-- set to NULL.
	const monitor_name_t monitor_name,      // Optional-- set to NULL.
	const rows_t* p_file_path_rows          // Optional-- set to NULL.
);
rows_t* get_rows_by_tag(
	const file_path_t file_path,
	const tags_t * const p_criteria,
	const tags_t * const n_criteria,        // Optional-- set to NULL.
	const monitor_name_t monitor_name       // Optional-- set to NULL.
);
rows_t* get_current(
	const file_path_t file_path,
	const monitor_name_t monitor_name       // Optional-- set to NULL.
);

bool append_new_current(const file_path_t data_file_path, row_t *new_entry);

num_rows add_tag_by_tag(const file_path_t file_path, tags_t *criteria, tags_t *tags_mod);

db_entries_operated_t* del_entries(
	db_entries_operated_t *ret,
	const file_path_t file_path,
	const tags_t * const p_criteria,
	const tags_t * const n_criteria,        // Optional-- set to NULL.
	const monitor_name_t monitor_name       // Optional-- set to NULL.
);

