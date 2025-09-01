#ifdef __has_include
#	if ! __has_include(<unistd.h>)
#		error "System does not appear to have a necessary library: \"<unistd.h>\""
#	endif	// <unistd.h>
#endif	// __has_include


#include <string.h>
#include <stdio.h>	// Debugging.
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>	// For fixed width integer format macros.
#include "flatfiledb.h"



bool validate_string_value(const char *str) {
	char *found;
	if ((found = strpbrk(str, COLUMN_DELIMS))) {
		if (*found == '\n' && *(found+1) == '\0') return true;
		return false;
	}
	return true;
}
bool validate_row(const row_t *r) {
	validate_string_value(r->ts);
	validate_string_value(r->monitor_name);
	validate_string_value(r->file);
	// No need to validate tags, since there should be no potential for delimiters.
}

tags_t get_tag_mask(const char column_string[MAX_COLUMN_LENGTH]) {
	tags_t ret = 0;

	char writable_column_string[strnlen(column_string, MAX_COLUMN_LENGTH)+1];
	strcpy(writable_column_string, column_string);
	char *token[MAX_COLUMN_LENGTH], *buff = NULL, *saveptr;
	token_length_t token_len = 0;
	for (
			buff = strtok_r(writable_column_string, TAG_DELIMS, &saveptr)
		;
			buff != NULL
		;
			buff = strtok_r(NULL, TAG_DELIMS, &saveptr)
	) {
		//if (token_len == 0 || strcmp(buff, token[token_len-1])) token[token_len++] = buff;
		for (unsigned short i = 0; i < sizeof(tags_known)/sizeof(tags_known[0]); i++) {
			//if (token == tags_known[i].text) ret += 1<<tags_known[i].id;
			if (strcmp(tags_known[i].text, buff)) continue;
			//if (ret & tags_known[i].id) {
			if (ret & encode_tag(tags_known[i].id)) {
				fprintf(stderr, "Detected entry with duplicate tag: \"%s\"\n", tags_known[i].text);
				continue;
			}
			//ret += 1<<tags_known[i].id;
			ret += encode_tag(tags_known[i].id);
		}
	}

	return ret;
}
inline tags_t encode_tag(enum Tag tag) {
	return 1<<tag;
}
void gen_tag_string(char *string, tags_t tags) {
	unsigned short total_len = 0;
	string[0] = '\0';
	for (unsigned short i = 0; i < sizeof(tags_known)/sizeof(tags_known[0]); i++) {	// Will they all be populated?
		if (tags & encode_tag(i)) {
			if (string[0] == '\0') {
				unsigned short len = strlen(tags_known[i].text);	// Not including terminating null.
				memcpy(string, tags_known[i].text, len);
				total_len += len;
			} else {
				static_assert(sizeof(TAG_DELIM) == 1, "Expected single byte char.\n");
				memcpy(string + total_len++, &TAG_DELIM, 1);	// Incrementing.
				unsigned short len = strlen(tags_known[i].text);	// Not including terminating null.
				memcpy(string + total_len, tags_known[i].text, len);
				total_len += len;
			}
		}
		if (total_len > Max_Tag_String_Len) {
			{
				char * const clip = strrchr(string, TAG_DELIM);
				if (clip) *clip = '\0';
				else string[Max_Tag_String_Len] = '\0';
			}
			fprintf(stderr,
				"Tag string exceeds max column length! It will be truncated or ignored.\n"
					"\tMax length: %d\n"
					"\tString length: %hu\n"
					"\tString: \"%s\"\n"
				, Max_Tag_String_Len
				, total_len
				,string
			);
			return;
		}
	}
	string[total_len] = '\0';	// Terminate the string.
}

static inline bool is_fp_in_fp_rows(const char match[MAX_COLUMN_LENGTH], const rows_t* within) {
	for (num_rows i = 0; i < within->ct; i++) {
		if (!strcasecmp(match, within->row[i]->file)) return true;
	}
	return false;
}
row_t* get_row_if_match(const num_rows row_num, const char *row_string,
	tags_t *p_criteria, tags_t *n_criteria,
	const monitor_name_t monitor_name,
	const rows_t* p_file_path_rows
) {
	static_assert(strlen(COLUMN_DELIMS) > 0, "No column delimiters have been hard-coded.");
	assert(monitor_name == NULL || monitor_name[0] != '\0');
	assert(row_num && row_string);
	assert(!(n_criteria && p_criteria && (*p_criteria & *n_criteria)) && "Conflicting positive and negative match criteria.");

	int row_string_length = strnlen(row_string, MAX_ROW_LENGTH);
	char writable_row_string[row_string_length + 1];
	strcpy(writable_row_string, row_string);
	char *buff = NULL, *saveptr;
	char token[NUM_COLUMNS+1][MAX_COLUMN_LENGTH];	// +1 for overflow detection.
	token_length_t token_len = 0;
	for (
			buff = strtok_r(writable_row_string, COLUMN_DELIMS, &saveptr)
		;
			buff != NULL
		;
			buff = strtok_r(NULL, COLUMN_DELIMS, &saveptr)
	) {
		if (token_len == NUM_COLUMNS) {
			fprintf(stderr,
				"Skipping wallpaper log entry due to invalid format...\n"
					"\tString value(s) on line (#%lu) may contain column delimiter character(s).\n"
					"\tRow string: \"%s\"\n"
				, row_num
				, row_string
			);
			return NULL;
		}
		// Add to token array if first or different from previous.
		if (token_len == 0 || strcmp(buff, token[token_len-1])) strncpy(token[token_len++], buff, MAX_COLUMN_LENGTH);
	}

	// Note: The only column that may be missing from a valid entry is the tags.
	if (token_len < NUM_COLUMNS - 1 || token_len > NUM_COLUMNS) {
		// No warning for empty lines.
		if (token_len > 0) fprintf(stderr,
				"Invalid number of columns for row #%lu.\n"
					"\tColumns detected: %hu.\n"
					"\tColumns expected: %hu -- %hu.\n"
				, row_num, token_len
				, NUM_COLUMNS - 1, NUM_COLUMNS
			);
		return NULL;
	}
	// Since the last column is optional, its token value must be cleared.
	if (token_len == NUM_COLUMNS - 1) token[NUM_COLUMNS - 1][0] = '\0';

	if (p_criteria != NULL && token_len != NUM_COLUMNS) {
		// Rows with no tags will never match any specified tags.
		return NULL;
	}

	// Use tags_t as bitmask to check tags.
	tags_t tags = get_tag_mask(token[3]);
	if (
		(!p_criteria || (*p_criteria & tags))		// Positive match criteria.
		&& (! (n_criteria && (*n_criteria & tags)) )	// Negative match criteria.
		&& (monitor_name == NULL || !strcasecmp(token[1], monitor_name))
		&& (p_file_path_rows == NULL || is_fp_in_fp_rows(token[2], p_file_path_rows))
	) {
		row_t *row = (row_t*)malloc(sizeof(row_t));

		// Timestamp:
		// Assuming timestamp is present and appears first in parsing order.
		if (strnlen(token[0], MAX_TIMESTAMP_LENGTH) == MAX_TIMESTAMP_LENGTH) {
			fprintf(stderr, "Timestamp in database entry exceeds hard-coded maximum.\n");
			row->ts[0] = '\0';
		} else {
			strcpy(row->ts, token[0]);
		}

		size_t len;

		// Monitor name:
		if (!(len = strlen(token[1]))) {
			fprintf(stderr, "Monitor name field in database has zero length.\n");
		} else {
			row->monitor_name = (monitor_name_t)malloc(len+1);	// +1 for termination.
			strcpy(row->monitor_name, token[1]);
		}

		// File path:
		// Assume file path is present and appears third or second in parsing order.
		if (!(len = strlen(token[2]))) {
			// Load entries with empty file paths, but warn user.
			fprintf(stderr, "Invalid length for file path in database entry!\n");
		} else {
			row->file = (file_path_t)malloc(len+1);	// +1 for terminating null?
			strcpy(row->file, token[2]);
		}

		row->tags = tags;
		return row;
	}

	return NULL;
}

rows_t* get_rows_by_tag(const file_path_t file_path, tags_t *p_criteria, tags_t *n_criteria, const monitor_name_t monitor_name) {
	assert(monitor_name == NULL || monitor_name[0] != '\0');
	if (n_criteria && p_criteria && (*p_criteria & *n_criteria)) {
		fprintf(stderr, "Can't match row with conflicting positive and negative match criteria.\n");
		return NULL;
	}

	FILE *f = fopen(file_path, "r"); if (f==0) return NULL;
	fseek(f,0,SEEK_END); long len = ftell(f); fseek(f,0,SEEK_SET);
	if (len == -1) {
		fclose(f);
		return NULL;
	}

	num_rows
		num_rows_matched = 0,
		row_array_size = 4
	;
	row_t** row_array = (row_t**)malloc(row_array_size * sizeof(row_t*));

	char *string = NULL;
	size_t size = 0;
	ssize_t chars_read;
	num_rows row_num = 0;
	while (getline(&string, &size, f) > 0) {
		row_t *row;
		if ( row = get_row_if_match(++row_num, string, p_criteria, n_criteria, monitor_name, NULL) ) {
			if (++num_rows_matched > row_array_size) {
				row_array_size *= 2;
				row_array = (row_t**)realloc(row_array, row_array_size * sizeof(row_t*));
			}
			row_array[num_rows_matched-1] = row;
		}
	}
	free(string);

	fclose(f);

	if (num_rows_matched) {
		rows_t *ret = (rows_t*)malloc(sizeof(rows_t));
		ret->row = row_array;
		ret->ct = num_rows_matched;
		return ret;
	}
	free(row_array);
	return NULL;
}

rows_t* get_current(const file_path_t file_path, const monitor_name_t monitor_name) {
	assert(monitor_name == NULL || monitor_name[0] != '\0');

	tags_t criteria = encode_tag(TAG_CURRENT);
	rows_t *res = get_rows_by_tag(file_path, &criteria, NULL, monitor_name);
	if (res == NULL) {
		fprintf(stderr, "No matching rows found.\n");
		return NULL;
	}
	if (monitor_name == NULL && res->ct != 1) {	// There should only be one current entry.
		fprintf(stderr, "Unexpected number of rows matched.\n");
		free_rows(res);
		return NULL;
	}
	// Currently no sanity check to catch duplicate "current" rows per monitor.

	return res;
}


/*bool sanity_check(char *file_path) {
	rows *res = get_rows_by_tag(&criteria);
	if (res == nullptr) {
		std::cerr << "No matching rows found." << std::endl;
		return false;
	}

	if (res->ct < 2) {
		// No sanity to check.
		free_rows(res);
		return true;
	}

	for (num_rows i = 0; i < res->ct; i++) {
		// Report non-existant files?
		// Check integrity of existing files?
	}

	free_rows(res);
	return true;
}*/

//bool append_new_current(const file_path_t data_file_path, const file_path_t wallpaper_file_path) {
/*bool append_new_current(const file_path_t data_file_path, row_t *new_entry) {
	if (!new_entry) return false;

	FILE *f = fopen(data_file_path, "a"); if (f==0) return false;
	fseek(f,0,SEEK_END); long len = ftell(f);// fseek(f,0,SEEK_SET);
	if (len == -1) {
		fclose(f);
		return false;
	}

	time_t timer;
	char buffer[MAX_TIMESTAMP_LENGTH];
	struct tm* tm_info;
	timer = time(NULL);
	tm_info = localtime(&timer);
	strftime(new_entry->ts, MAX_TIMESTAMP_LENGTH, Timestamp_Format, tm_info);

	char tag_string[Max_Tag_String_Len];
	gen_tag_string(tag_string, new_entry->tags);

	if (fprintf(f, "%s %s %s\n",
		new_entry->ts,
		new_entry->file,
		//gen_tag_string(new_entry->tags)
		tag_string
	) <= 0) {
		fprintf(stderr, "Failed to append new current entry to database.\n");
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}*/
bool append_new_current(const file_path_t data_file_path, row_t *new_entry) {
	assert(new_entry);
	assert(new_entry->monitor_name);
	assert(new_entry->monitor_name[0]);
	assert(new_entry->file);
	assert(new_entry->file[0]);
	assert(validate_row(new_entry));

	if (strpbrk(new_entry->file, COLUMN_DELIMS)) {
		fprintf(stderr, "Not adding entry to database. File path contains invalid characters-- probably a tab or new-line.\n");
		return false;
	}

	bool created_new_file;
	FILE *f = fopen(data_file_path, "r+b");
	if (f == 0) {
		// Assume database file does not exist. Try creating a new one.
		if (!(f = fopen(data_file_path, "wbx"))) return false;
		created_new_file = true;
	} else {
		fseek(f,0,SEEK_END); long len = ftell(f); fseek(f,0,SEEK_SET);
		if (len == -1) {
			fclose(f);
			return false;
		}
		created_new_file = false;
	}


	time_t timer;
	char buffer[MAX_TIMESTAMP_LENGTH];
	struct tm* tm_info;
	timer = time(NULL);
	tm_info = localtime(&timer);
	strftime(new_entry->ts, MAX_TIMESTAMP_LENGTH, Timestamp_Format, tm_info);

	char tag_string[Max_Tag_String_Len];
	gen_tag_string(tag_string, new_entry->tags);

	if (created_new_file) {
		if (fprintf(f, "%s%c%s%c%s%c%s\n",	// No need for end-of-file?
			new_entry->ts,
			COLUMN_DELIM,
			new_entry->monitor_name,
			COLUMN_DELIM,
			new_entry->file,
			COLUMN_DELIM,
			tag_string
		) <= 0) {
			fprintf(stderr, "Failed to append new current entry to database.\n");
			fclose(f);
			return false;
		}
		fclose(f);
		return true;
	}


	char tmp_path[] = "/tmp/fileXXXXXX";
	int fd = mkstemp(tmp_path);
	close(fd);
	if (!tmp_path) return false;
	FILE *tmp = fopen(tmp_path, "w");
	if (tmp==0) {
		fclose(f);
		return false;
	}


	char *string = NULL;
	size_t size = 0;
	/*tags_t current_mask = encode_tag(Tag::CURRENT);*/
	tags_t current_mask = encode_tag(TAG_CURRENT);
	num_rows row_num = 0;
	while (getline(&string, &size, f) > 0) {
		row_t *row;
		if ( row = get_row_if_match(++row_num, string, &current_mask, NULL, NULL, NULL) ) {
			char tag_string[Max_Tag_String_Len];	// Name takes precedence over previously defined.
			//row->tags ^= current_mask;
			//gen_tag_string(tag_string, row->tags & (~(1 << current_mask)));
			int status;
			if (row->tags == current_mask) {
				// Current is this entry's only tag.
				row->tags = '\0';
				status = fprintf(tmp, "%s%c%s%c%s\n",	// No need for end-of-file?
					row->ts,
					COLUMN_DELIM,
					row->monitor_name,
					COLUMN_DELIM,
					row->file
					// Tag field is not necessary.
				);
			} else {
				gen_tag_string(tag_string, row->tags & (~current_mask));
				status = fprintf(tmp, "%s%c%s%c%s%c%s\n",	// No need for end-of-file?
					row->ts,
					COLUMN_DELIM,
					row->monitor_name,
					COLUMN_DELIM,
					row->file,
					COLUMN_DELIM,
					tag_string
				);
			}
			if (status <= 0) {
				fprintf(stderr, "Failed to remove pre-existing current entry from database.\n");
				free_row(row);
				fclose(f);
				fclose(tmp);
				return false;
			}
			free_row(row);
		} else {
			fputs(string, tmp);
		}
	}
	free(string);
	if (fprintf(tmp, "%s%c%s%c%s%c%s\n",	// No need for end-of-file?
		new_entry->ts,
		COLUMN_DELIM,
		new_entry->monitor_name,
		COLUMN_DELIM,
		new_entry->file,
		COLUMN_DELIM,
		tag_string
	) <= 0) {
		fprintf(stderr, "Failed to append new current entry to database.\n");
		fclose(f);
		fclose(tmp);
		return false;
	}

	fclose(f);
	fclose(tmp);	// ?
	if (rename(tmp_path, data_file_path)) {
		fprintf(stderr, "Failed to replace database with temp file.\n\t%s\n\t%s\n", tmp_path, data_file_path);
		return false;
	}


	return true;
}


/*tags_t db::sum_tags(tags_t a, tags_t b) {
	return a & b;
}*/

/*bool add_tag_to_row(row_t *target_row, Tag target_tag) {
	return true;
}*/

num_rows add_tag_by_tag(const file_path_t file_path, tags_t *criteria, tags_t *tags_mod) {
	FILE *f = fopen(file_path, "r+b"); if (f==0) return 0;
	fseek(f,0,SEEK_END); long len = ftell(f); fseek(f,0,SEEK_SET);
	if (len == -1) {
		fclose(f);
		return 0;
	}

	char tmp_path[] = "/tmp/fileXXXXXX";
	int fd = mkstemp(tmp_path);
	close(fd);
	if (!tmp_path) return 0;
	FILE *tmp = fopen(tmp_path, "w");
	if (tmp==0) {
		fclose(f);
		return 0;
	}

	num_rows
		num_rows_matched = 0,
		row_array_size = 4
	;

	char *string = NULL;
	size_t size = 0;
	num_rows row_num = 0;
	while (getline(&string, &size, f) > 0) {
		row_t *row;
		if ( row = get_row_if_match(++row_num, string, criteria, NULL, NULL, NULL) ) {
			if ( (row->tags & *tags_mod) == *tags_mod) {
				// All tags requested are already associated with the row.
				fprintf(stderr, "Database entry already associated with all requested tags. Ignoring.\n");
				fputs(string, tmp);
				continue;
			}

			if (++num_rows_matched > row_array_size) {
				row_array_size *= 2;
				//row_array = (row_t**)realloc(row_array, row_array_size * sizeof(row_t*));
			}
			tags_t result = row->tags | *tags_mod;
			char tag_string[Max_Tag_String_Len];
			gen_tag_string(tag_string, result);
			fprintf(tmp, "%s%c%s%c%s%c%s\n",
				row->ts,
				COLUMN_DELIM,
				row->monitor_name,
				COLUMN_DELIM,
				row->file,
				COLUMN_DELIM,
				tag_string
			);
		} else {
			fputs(string, tmp);
		}
	}
	free(string);



	fclose(f);
	//fclose(tmp);
	if (rename(tmp_path, file_path)) {
		fprintf(stderr, "Failed to replace database with temp file.\n\t%s\n\t%s\n", tmp_path, file_path);
	}

	return num_rows_matched;
}
/*bool add_tag_to_current(Tag target_tag) {
	return true;
}*/

rows_t* del_entries(rows_t *ret_rows, const file_path_t db_file_path,
	tags_t *p_criteria, tags_t *n_criteria,
	const monitor_name_t monitor_name
) {
	assert(ret_rows == NULL || ret_rows->ct == 0);
	assert(db_file_path && db_file_path[0]);
	assert(!(n_criteria && (*p_criteria & *n_criteria)) && "Conflicting positive and negative match criteria.");
	assert(monitor_name == NULL || monitor_name[0] != '\0');

	bool allocated_ret;
	if (ret_rows == NULL) {
		if (!(ret_rows = malloc(sizeof(rows_t)))) {
			fprintf(stderr, "Failed to allocate memory on heap.\n");
			return NULL;
		}
		allocated_ret = true;
	} else allocated_ret = false;
	assert(ret_rows);

	FILE *f = fopen(db_file_path, "r"); if (f==0) return NULL;
	fseek(f,0,SEEK_END); long len = ftell(f); fseek(f,0,SEEK_SET);
	if (len == -1) {
		fclose(f);
		return NULL;
	}

	char *string = NULL;
	size_t size = 0;
	num_rows row_num = 0;

	//
	// File is, unfortunately, read through twice.
	// This is to detect earlier entries referring to the same files as later entries.
	//

	// Stage 1: Cache matching entries.
	// Heap is used because we want to return the matched entries to the calling function anyway.
	while (getline(&string, &size, f) > 0) {
		row_t *row;
		if ((row = get_row_if_match(++row_num, string, p_criteria, n_criteria, monitor_name, NULL))) {
			// Row matches query-- cache for targeting in next stage.
			if (ret_rows->ct == 0) {
				if (!(ret_rows->row = (row_t**)malloc(sizeof(row_t*)))) {
					fprintf(stderr, "Failed to allocate initial memory for row #%lu. Terminating prematurely.\n", row_num);
					free(string);
					fclose(f);
					if (allocated_ret) free_rows(ret_rows);
					else free_rows_contents(ret_rows);
					return NULL;
				}
			} else {
				void *tmp;
				if (!(tmp = reallocarray(ret_rows->row, ret_rows->ct+1, sizeof(row_t*)))) {
					fprintf(stderr, "Failed to reallocate memory for row #%lu. Terminating prematurely.\n", row_num);
					free(string);
					fclose(f);
					if (allocated_ret) free_rows(ret_rows);
					else free_rows_contents(ret_rows);
					return NULL;
				}
				if (tmp != ret_rows->row) ret_rows->row = (row_t**)tmp;
			}
			ret_rows->row[ret_rows->ct++] = row;
		}
	}

	if (ret_rows->ct == 0) {
		fclose(f);
		return ret_rows;
	}

	// Stage 2: parse from beginning again, to match preceeding entries refering to the same files matched in Stage 1.
	// Note: This pass is still subject to the specified negative tag and monitor name criteria.
	// 	(We simply match based on file paths instead of positive tag criteria.)
	// This time we are also writing to a temp file.

	char tmp_path[] = "/tmp/fileXXXXXX";
	int fd = mkstemp(tmp_path);
	close(fd);
	if (!tmp_path) return NULL;
	FILE *tmp = fopen(tmp_path, "w+");
	if (tmp==0) {
		fclose(f);
		return NULL;
	}

	rewind(f);
	while (getline(&string, &size, f) > 0) {
		row_t *row;
		if (!(row = get_row_if_match(++row_num, string, NULL, n_criteria, monitor_name, ret_rows))) {
			// Row does not match. Ignore it.
			fputs(string, tmp);
			continue;
		}
		// Row matches query-- do not write it to the temp file.
		// Instead, we will load it onto the heap for return to caller.
		void *tmp;
		if (!(tmp = reallocarray(ret_rows->row, ret_rows->ct+1, sizeof(row_t*)))) {
			fprintf(stderr, "Failed to reallocate memory for row #%lu. Terminating prematurely.\n", row_num);
			free(string);
			fclose(f);
			if (allocated_ret) free_rows(ret_rows);
			else free_rows_contents(ret_rows);
			return NULL;
		}
		if (tmp != ret_rows->row) ret_rows->row = (row_t**)tmp;
		ret_rows->row[ret_rows->ct++] = row;
	}

	free(string);
	fclose(f);
	fclose(tmp);
	if (rename(tmp_path, db_file_path)) {
		fprintf(stderr, "Failed to replace database with temp file.\n\t%s\n\t%s\n", tmp_path, db_file_path);
		return NULL;
	}
	return ret_rows;
}

