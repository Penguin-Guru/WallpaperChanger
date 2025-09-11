#include <stdlib.h>
#include <string.h>
#include <stdio.h>	// Debugging.
#include <inttypes.h>	// For fixed width integer format macros.
#include <assert.h>
#include "db.h"

/*
 * void print_current_timestamp(const char *TimestampFormat) {
 * 	if (TimestampFormat == NULL) TimestampFormat = Timestamp_Format;
 *
 * 	timestamp_t buffer;
 * 	struct tm* tm_info;
 *
 * 	time_t timer = time(NULL);
 * 	tm_info = localtime(&timer);
 *
 * 	size_t len = strftime(buffer, 26, TimestampFormat, tm_info);
 * 	char
 * 		*c = buffer+len-1,	// I thought strftime's return didn't include the null. Not sure why the -1 is needed.
 * 		*cc = c-1
 * 	;
 * 	if ( (*c | *cc) == '0') {	// Remove trailing zeroes (in timezone offset) if exactly two (for minutes).
 * 		*cc = *c = '\0';
 * 	}
 * 	printf("%s\n", buffer);
 * };
*/


inline void free_row(row_t *r) {
	if (!r) return;
	if (r->file) free(r->file);
	if (r->monitor_name) free(r->monitor_name);
	free(r);
}
inline void free_rows_contents(rows_t *target) {
	assert(target);
	assert(target->row);
	for (num_rows i = 0; i < target->ct; i++) {
		assert(target->row[i]);
		free_row(target->row[i]);
	}
	free(target->row);
	target->ct = 0;
	// Not attempting to free target.
}
void free_rows(rows_t *target) {
	free_rows_contents(target);
	free(target);
}
inline void free_entries_operated(db_entries_operated_t *target) {
	free_rows_contents(&target->rows);
	free(target);
}


bool format_path(file_path_t path) {
	// Remove duplicate, consecutive slashes.
	static const char *double_slashes = "//";
	char *pos = strstr(path, double_slashes);
	size_t length_remaining;
	uint_fast8_t ct = 0;
	while (pos != NULL) {
		ct++;
		length_remaining = strlen(pos);
		memmove(pos, pos + 1, length_remaining);
		pos = strstr(pos, double_slashes);
	}
	if (ct) fprintf(stderr, "Removed %" PRIuFAST8 " duplicate, consecutive slashes from path.\n", ct);
	return true;
}

