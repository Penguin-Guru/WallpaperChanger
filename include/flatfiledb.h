#pragma once

#include <limits.h>
#include <stdint.h>
#include <assert.h>
#include "util.h"

#define WHITESPACE_CHARACTERS " \t\r\n\v\f"

namespace db {
	const char Column_Delims[] = WHITESPACE_CHARACTERS;	// Any character in this array has the same effect.
	const char Column_Names[][MAX_COLUMN_TITLE_LENGTH] = {	// Columns must be in (descending) order of appearance (when reading/writing).
		"timestamp",
		"file_path",
		"tags"
	};
	const uint_fast8_t NUM_COLUMNS = sizeof(Column_Names)/sizeof(Column_Names[0]);
	static_assert(MAX_COLUMN_TITLE_LENGTH <= MAX_COLUMN_LENGTH);
	const uint_fast16_t MAX_ROW_LENGTH = NUM_COLUMNS * (MAX_COLUMN_LENGTH + 1);	// +1 for delimiters.

	const char Timestamp_Format[] = "%F@%T%z";
	void print_current_timestamp(const char *TimestampFormat = Timestamp_Format);

	enum Tag {	// Index is offset in bitmask.
		CURRENT = 0,	// Currently the active wallpaper.
		HISTORIC,	// Not the first time the wallpaper has been set.
		FAVOURITE	// A personal favourite.
	};
	const uint_fast8_t Max_Tag_Len = 10;
	typedef struct tag_t {
		Tag id;
		const char text[Max_Tag_Len];
	} tag_t;
	// Tags must appear in same order as enum due to use in get_current(). Fix later.
	// Number limited by bitmask (sizeof(tags_t)*CHAR_BIT).
	const tag_t tags_known[] = {	// Must be keps in sync with Tag enum. Fix later.
		tag_t{CURRENT, "current"},
		tag_t{HISTORIC, "historic"},
		//tag_t{NEW, "new"},
		tag_t{FAVOURITE, "favourite"}
	};
	const uint_fast8_t Max_Tag_String_Len = sizeof(tags_known)/sizeof(tags_known[0]) * (Max_Tag_Len + 1);	// +1 for commas.
	const char Tag_Delims[] = ",;" WHITESPACE_CHARACTERS;	// Any character in this array has the same effect.

	typedef struct row_t {
		timestamp_t ts = {0};
		file_path_t file;	// Not flexible. Appears in file order.
		tags_t tags;
	} row_t;
	bool print_rows(const char* path);	// Only useful for testing?

	typedef struct rows_t {
		row_t** row;
		num_rows ct = 0;
	} rows_t;
	void free_row(row_t* r);
	void free_rows(rows_t* target);



	using token_length_t = uint_fast8_t;
	tags_t get_tag_mask(const char column_string[MAX_COLUMN_LENGTH]);
	inline tags_t encode_tag(Tag tag) {return 1<<tag;};

	row_t* get_row_if_match(const num_rows row_num, const char *row_string, tags_t *p_criteria, tags_t *n_criteria = nullptr);
	rows_t* get_rows_by_tag(const file_path_t file_path, tags_t *p_criteria, tags_t *n_criteria = nullptr);
	row_t* get_current(const file_path_t file_path);

	bool append_new_current(const file_path_t data_file_path, row_t *new_entry);

	void gen_tag_string(char *string, tags_t tags);
	num_rows add_tag_by_tag(const file_path_t file_path, tags_t *criteria, tags_t *tags_mod);

	bool del_entry_by_tag(rows_t *ret_rows, const file_path_t file_path, tags_t *p_criteria, tags_t *n_criteria = nullptr);
}

