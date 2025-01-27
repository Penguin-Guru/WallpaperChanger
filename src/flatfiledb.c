#include <string.h>	// Debugging.
#include <stdio.h>	// Debugging.
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include "flatfiledb.h"


void print_current_timestamp(const char *TimestampFormat) {
	if (TimestampFormat == NULL) TimestampFormat = Timestamp_Format;

	timestamp_t buffer;
	struct tm* tm_info;

	time_t timer = time(NULL);
	tm_info = localtime(&timer);

	size_t len = strftime(buffer, 26, TimestampFormat, tm_info);
	char
		*c = buffer+len-1,	// I thought strftime's return didn't include the null. Not sure why the -1 is needed.
		*cc = c-1
	;
	if ( (*c | *cc) == '0') {	// Remove trailing zeroes (in timezone offset) if exactly two (for minutes).
		*cc = *c = '\0';
	}
	printf("%s\n", buffer);
};

bool print_rows(const char* path) {
	FILE *f = fopen(path, "r"); if (f==0) return false;
	fseek(f,0,SEEK_END); long len = ftell(f); fseek(f,0,SEEK_SET);
	if (len == -1) {
		fclose(f);
		return false;
	}

	char *string = NULL;
	size_t size = 0;
	ssize_t chars_read;
	while (getline(&string, &size, f) > 0) {
		printf("%s", string);
	}
	free(string);

	fclose(f);
	return true;
}

void free_row(row_t *r) {
	if (!r) return;
	if (r->file) free(r->file);
	free(r);
}
void free_rows(rows_t *target) {
	if (target == NULL) return;
		for (num_rows i = 0; i < target->ct; i++) {
			free_row(target->row[i]);
		}
		if (target->row) free(target->row);	// Should always be true.
	free(target);
}

tags_t get_tag_mask(const char column_string[MAX_COLUMN_LENGTH]) {
	tags_t ret = 0;

	char writable_column_string[strnlen(column_string, MAX_COLUMN_LENGTH)];
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

row_t* get_row_if_match(const num_rows row_num, const char *row_string, tags_t *p_criteria, tags_t *n_criteria) {
	static_assert(strlen(COLUMN_DELIMS) > 0, "No column delimiters have been hard-coded.");
	assert(row_num && row_string);
	assert(!(n_criteria && p_criteria && (*p_criteria & *n_criteria)) && "Conflicting positive and negative match criteria.");

	int row_string_length = strnlen(row_string, MAX_ROW_LENGTH);
	char writable_row_string[row_string_length + 1];
	strcpy(writable_row_string, row_string);
	char *buff = NULL, *saveptr;
	char token[NUM_COLUMNS][MAX_COLUMN_LENGTH];
	token_length_t token_len = 0;
	for (
			buff = strtok_r(writable_row_string, COLUMN_DELIMS, &saveptr)
		;
			buff != NULL
		;
			buff = strtok_r(NULL, COLUMN_DELIMS, &saveptr)
	) {
		// Add to token array if first or different from previous.
		if (token_len == 0 || strcmp(buff, token[token_len-1])) strncpy(token[token_len++], buff, MAX_COLUMN_LENGTH);
	}
	if (token_len < 2 || token_len > sizeof(Column_Names)/sizeof(Column_Names[0])) {
		// No warning for empty lines.
		if (token_len > 0) fprintf(stderr, "Invalid number of columns for row #%lu. Columns detected: %hu.\n", row_num, token_len);
		return NULL;
	}

	if (p_criteria == NULL || (token_len == 3)) {	// The only column that may be missing from a valid entry is the tags.
		// Use tags_t as bitmask to check tags.
		tags_t tags;
		if (
			(!p_criteria || (*p_criteria & (tags = get_tag_mask(token[2]))))	// Positive match criteria.
			&& ! (n_criteria && (*n_criteria & tags))		// Negative match criteria.
		) {
			row_t *row = (row_t*)malloc(sizeof(row_t));	// Why does cast need to be a pointer?
			if (strnlen(token[0], MAX_TIMESTAMP_LENGTH) == MAX_TIMESTAMP_LENGTH) {
				fprintf(stderr, "Timestamp in database entry exceeds hard-coded maximum.\n");
				row->ts[0] = '\0';
			} else {
				strcpy(row->ts, token[0]);
			}
			const size_t len = strlen(token[1]);
			if (len) {
				row->file = (file_path_t)malloc(len+1);	// +1 for terminating null?
				strcpy(row->file, token[1]);
			} else {
				// Load entries with empty file paths, but warn user.
				fprintf(stderr, "Invalid length for file path in database entry!\n");
			}
			row->tags = tags;
			return row;
		}
	}
	return NULL;
}

rows_t* get_rows_by_tag(const file_path_t file_path, tags_t *p_criteria, tags_t *n_criteria) {
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
		if ( row = get_row_if_match(++row_num, string, p_criteria, n_criteria) ) {
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

row_t* get_current(const file_path_t file_path) {
	tags_t criteria = encode_tag(TAG_CURRENT);
	rows_t *res = get_rows_by_tag(file_path, &criteria, NULL);
	if (res == NULL) {
		fprintf(stderr, "No matching rows found.\n");
		return NULL;
	}
	if (res->ct != 1) {	// There should only be one current entry.
		fprintf(stderr, "Unexpected number of rows matched.\n");
		free_rows(res);
		return NULL;
	}

	row_t *ret = res->row[0];
	// Free only container, not the (one) row.
	free(res->row);	// Eliminate later.
	free(res);
	return ret;
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
	if (!new_entry) return false;

	if (strpbrk(new_entry->file, COLUMN_DELIMS)) {
		fprintf(stderr, "Not adding entry to database. File path contains invalid characters-- probably a semicolon or newline.\n");
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
		if (fprintf(f, "%s%c%s%c%s\n",	// No need for end-of-file?
			new_entry->ts,
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
		/*if ( row_t *row = get_row_if_match(++row_num, string, &current_mask) ) {*/
		row_t *row;
		if ( row = get_row_if_match(++row_num, string, &current_mask, NULL) ) {
			char tag_string[Max_Tag_String_Len];	// Name takes precedence over previously defined.
			//row->tags ^= current_mask;
			//gen_tag_string(tag_string, row->tags & (~(1 << current_mask)));
			gen_tag_string(tag_string, row->tags & (~current_mask));
			if (fprintf(tmp, "%s%c%s%c%s\n",	// No need for end-of-file?
				row->ts,
				COLUMN_DELIM,
				row->file,
				COLUMN_DELIM,
				tag_string
			) <= 0) {
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
	if (fprintf(tmp, "%s%c%s%c%s\n",	// No need for end-of-file?
		new_entry->ts,
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

void gen_tag_string(char *string, tags_t tags) {
	unsigned short total_len = 0;
	string[0] = '\0';
	for (unsigned short i = 0; i < sizeof(tags_known)/sizeof(tags_known[0]); i++) {	// Will they all be populated?
		if (tags & encode_tag(i)) {
			if (string[0] == '\0') {
				unsigned short len = strlen(tags_known[i].text);
				strcpy(string, tags_known[i].text);
				total_len += len;
			} else {
				unsigned short len = strlen(tags_known[i].text);
				total_len += len + 1;	// +1 for TAG_DELIM.
				assert(total_len <= MAX_COLUMN_TITLE_LENGTH);
				static_assert(sizeof(TAG_DELIM) == 1, "Expected single byte char.\n");
				char *pos = mempcpy(string, &TAG_DELIM, 1);
				mempcpy(pos, tags_known[i].text, len);
			}
		}
	}
}

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
		if ( row = get_row_if_match(++row_num, string, criteria, NULL) ) {
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
			fprintf(tmp, "%s%c%s%c%s\n",
				row->ts,
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

bool del_entry_by_tag(rows_t *ret_rows, const file_path_t file_path, tags_t *p_criteria, tags_t *n_criteria) {
	assert(!(n_criteria && (*p_criteria & *n_criteria)) && "Conflicting positive and negative match criteria.");
	assert(ret_rows != NULL);

	FILE *f = fopen(file_path, "r"); if (f==0) return false;
	fseek(f,0,SEEK_END); long len = ftell(f); fseek(f,0,SEEK_SET);
	if (len == -1) {
		fclose(f);
		return false;
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

	num_rows num_rows_deleted = 0;

	char *string = NULL;
	size_t size = 0;
	num_rows row_num = 0;
	while (getline(&string, &size, f) > 0) {
		row_t *row;
		if ( row = get_row_if_match(++row_num, string, p_criteria, n_criteria) ) {
			if (ret_rows->ct == 0) {
				if (! (ret_rows->row = (row_t**)malloc(sizeof(row_t*)))) {
					fprintf(stderr, "Failed to allocate initial memory for row #%lu. Terminating prematurely.\n", row_num);
					free(string);
					fclose(f);
					return false;
				}
			} else {
				void *tmp = reallocarray(ret_rows->row, ret_rows->ct+1, sizeof(row_t*));
				if (tmp) {
					ret_rows->row = (row_t**)tmp;
				} else {
					fprintf(stderr, "Failed to reallocate memory for row #%lu. Terminating prematurely.\n", row_num);
					free(string);
					fclose(f);
					return false;
				}
			}
			ret_rows->row[ret_rows->ct++] = row;
			continue;	// Do not write matches to temp file.
		}
		fputs(string, tmp);
	}
	free(string);

	fclose(f);
	if (rename(tmp_path, file_path)) {
		fprintf(stderr, "Failed to replace database with temp file.\n\t%s\n\t%s\n", tmp_path, file_path);
		return false;
	}
	return num_rows_deleted;
}

