#define _GNU_SOURCE	// For strchrnul.


#ifdef __has_include
#	if ! __has_include(<ftw.h>)
#		error "System does not appear to have a necessary library: \"<ftw.h>\""
#	endif	// <ftw.h>
#	if ! __has_include(<magic.h>)
#		error "System does not appear to have a necessary library: \"<magic.h>\""
#	endif	// <magic.h>
#	if ! __has_include(<unistd.h>)
#		error "System does not appear to have a necessary library: \"<unistd.h>\""
#	endif	// <unistd.h>
#endif	// __has_include


#include <string.h>
#include <ftw.h>	// Used for finding new wallpapers.
//#include <dirent.h>	// Used for finding new wallpapers.
//`man fts` is another option...
#include <magic.h>	// Used for detecting MIME content types.
#include <unistd.h>	// Used for reading symlinks.
//#include <ctime>	// ?
#include <time.h>
#include <errno.h>
#include <stdlib.h>	// For free.
#include <stdio.h>	// For fprintf.
#include "wallpaperchanger.h"
#include "init.h"
#include "image.h"
#include "flatfiledb.h"


extern app_components_t app_components;


void clean_up() {
	if (s_wallpapers) {	// Depends on run mode.
		for (size_t i = 0; i < s_wallpapers_ct; i++) free(s_wallpapers[i]);
		free(s_wallpapers);
	}
	if (s_monitors.ct) {
		for (uint_fast16_t i = 0; i < s_monitors.ct; i--) {
			free(s_monitors.monitor[i].name);
			free(s_monitors.monitor + i);
		}
	} else assert(!s_monitors.monitor);
	if (data_file_path) free(data_file_path);	// Should always be true.
	if (data_directory) free(data_directory);	// Almost always true.
	if (wallpaper_path) free(wallpaper_path);	// Depends on run mode.
}

bool is_text_true(const char  * const string) {
	static const char * const Keywords[] = {
		"true",
		"yes"
	};
	static const uint_fast8_t Keywords_ct = sizeof(Keywords) / sizeof(Keywords[0]);
	//for (const char * const * it = keywords[0]; 
	for (uint_fast8_t i = 0; i < Keywords_ct; i++) {
		if (!strcasecmp(string, Keywords[i])) return true;
	}
	return false;
}

static inline uint32_t get_monitor_id_from_name(const char * const name) {
	assert(s_monitors.ct > 0);
	for (uint_fast16_t i = 0; i < s_monitors.ct; i++) {
		// Hopefully strcoll catches any signed mismatches, otherwise use strcmp.
		if (!strcoll((char*)(s_monitors.monitor[i].name), name)) continue;
		return s_monitors.monitor[i].id;
	}
	fprintf(stderr, "Failed to match monitor name: \"%s\"\n", name);
	return 0;
}
static inline char * const get_monitor_name_from_id(const uint32_t id) {
	assert(s_monitors.ct > 0);
	for (uint_fast16_t i = 0; i < s_monitors.ct; i++) {
		if (s_monitors.monitor[i].id == id) return (char*)(s_monitors.monitor[i].name);
	}
	fprintf(stderr, "Failed to match monitor I.D.: %u\n", s_target_monitor_id);
	return 0;
}
static inline monitor_info* const get_monitor_by_id(const uint32_t id) {
	assert(s_monitors.ct > 0);
	for (uint_fast16_t i = 0; i < s_monitors.ct; i++) {
		if (s_monitors.monitor[i].id == id) return &(s_monitors.monitor[i]);
	}
	fprintf(stderr, "Failed to match monitor I.D.: %u\n", s_target_monitor_id);
	return 0;
}


/* Misc. intermediary functions: */

bool set_new_current(const file_path_t wallpaper_file_path, tags_t tags) {
	// To do: use active monitor as default.
	if (!s_target_monitor_id) {
		fprintf(stderr, "No target monitor. Aborting.\n");
		return false;
	}
	monitor_info *target_monitor;
	if (!(target_monitor = get_monitor_by_id(s_target_monitor_id))) {
		fprintf(stderr, "Aborting.\n");
		return false;
	}
	assert(target_monitor->name);
	assert(target_monitor->name[0]);

	if (!set_wallpaper(wallpaper_file_path, target_monitor)) {
		fprintf(stderr, "Failed to set new wallpaper.\n");
		return false;
	}

	row_t new_entry;
	const size_t len = strlen(wallpaper_file_path);
	if (!len) {
		fprintf(stderr, "Invalid length for file path.\n");
		return false;
	}
	new_entry.monitor_name = target_monitor->name;
	new_entry.file = wallpaper_file_path;
	new_entry.tags = tags | encode_tag(TAG_CURRENT);	// Make sure it's tagged as current.
	assert(data_file_path);
	if (!append_new_current(data_file_path, &new_entry)) {
		fprintf(stderr, "Failed to append new current wallpaper to database.\n");
		return false;
	}
	return true;
}

const file_path_t get_start_of_relative_path(const file_path_t full_path) {
	assert(full_path);
	assert(full_path[0]);
	// Relative paths are used to preserve functionality in the event that a user moves their root wallpaper directory.
	//const char *start_of_relative_path = strstr(full_path, "/" DEFAULT_WALLPAPER_DIR_NAME "/");
	char *start_of_relative_path = strstr(full_path, "/" DEFAULT_WALLPAPER_DIR_NAME "/");
	if (!start_of_relative_path) {
		fprintf(stderr,
			"Failed to parse wallpaper directory (\"%s\") from path: \"%s\"\n",
			"/" DEFAULT_WALLPAPER_DIR_NAME "/", full_path
		);
		return NULL;
	}
	start_of_relative_path += sizeof(DEFAULT_WALLPAPER_DIR_NAME);
	//start_of_relative_path += sizeof(DEFAULT_WALLPAPER_DIR_NAME) + 1;
	if (start_of_relative_path - full_path < 0) return NULL;	// Quick bounds check.
	return start_of_relative_path;
	// +2 for the two slashes.
}
/*const char *get_start_of_file_name(const file_path_t full_path) {
	// Note: This can not handle file names that include a '/' character!
	const char *start_of_file_name = strrchr(full_path, '/');
	if (!start_of_file_name) {
		fprintf(stderr,
			"Failed to parse file name from path: \"%s\"\n",
			full_path
		);
		return nullptr;
	}
	if (*(++start_of_file_name) == '\0') return nullptr;
	return start_of_file_name;
}*/

/*inline void decrement_static_wallpapers() {
	void *tmp = realloc(s_wallpapers, sizeof(file_path_t)*(--s_wallpapers_ct));
	if (tmp) {
		s_wallpapers = (file_path_t*)tmp;
	} else {
		fprintf(stderr, "Failed to reallocate memory.\n");
	}
}*/
//short wallpaper_is_new(const file_path_t wallpaper_file_path) {
short wallpaper_is_new(const file_path_t wallpaper_file_path) {
	if (wallpaper_file_path == NULL || *wallpaper_file_path == '\0') {
		fprintf(stderr, "wallpaper_is_new was provided an empty string.\n");
		return -1;
	}
	if (s_wallpapers == NULL) {	// Populate the cache if not already done from previous call.
		tags_t n_criteria = encode_tag(TAG_HISTORIC);	// Do not bias toward wallpapers set more often.
		rows_t *rows = get_rows_by_tag(data_file_path, NULL, &n_criteria, NULL);
		if (!rows || rows->ct <= 0) {
			//fprintf(stderr, "No matching entries in database.\n");
			if (rows) free_rows(rows);
			//return -1;
			return 1;	// Seems new.
		}
		/*if (!rows) {
			fprintf(stderr, "No matching entries in database.\n");
			if (rows) free_rows(rows);
			return -1;
		}
		if (rows->ct <= 0) return 1;	// Seems new.*/
		s_wallpapers = (file_path_t*)malloc(sizeof(file_path_t)*rows->ct);
		// Allocating memory for all rows is inefficient. Fix later.
		for (unsigned int i = 0; i < rows->ct; i++) {
			const file_path_t fp = rows->row[i]->file;
			const file_path_t start_of_relative_path = get_start_of_relative_path(fp);
			if (!start_of_relative_path) {
				// This detects mismatched path structures, since simply matching the file names may be insufficiently unique.
				fprintf(stderr,
					"File path is not in wallpaper directory. Have you migrated?\n"
						"\tFile path: \"%s\"\n"
						"\tWallpaper directory: \"%s\"\n"
					,
					fp,
					"/" DEFAULT_WALLPAPER_DIR_NAME "/"
				);
				//decrement_static_wallpapers();
				free_rows(rows);
				return -1;
				//continue;
			}
			const size_t len = strlen(start_of_relative_path);
			if (!len) {
				fprintf(stderr, "Invalid path length!\n");
				//decrement_static_wallpapers();
				free_rows(rows);
				continue;
			}
			if (! (s_wallpapers[s_wallpapers_ct] = (file_path_t)malloc(len+1))) {	// +1 for terminating null.
				fprintf(stderr, "Failed to allocate memory. Aborting.\n");
				free_rows(rows);
				return -1;
			}
			//strcpy(s_wallpapers[s_wallpapers_ct++], start_of_relative_path);
			memcpy(s_wallpapers[s_wallpapers_ct++], start_of_relative_path, len+1);
		}
		free_rows(rows);
	}
	for (unsigned int i = 0; i < s_wallpapers_ct; i++) {
		const file_path_t start_of_relative_path = get_start_of_relative_path(wallpaper_file_path);
		if (!start_of_relative_path) return -1;
		if (!strcmp(s_wallpapers[i], start_of_relative_path)) return 0;	// Not new (continue search).
	}
	return 1;	// Seems new.
}

short check_mime_type(const file_path_t filepath) {
	struct magic_set *magic = magic_open(MAGIC_MIME_TYPE|MAGIC_CHECK);
	magic_load(magic,NULL);
	const char *mime_type = magic_file(magic, filepath);
	//magic_close(magic);
	if (mime_type == NULL || *mime_type == '\0') {
		fprintf(stderr, "Failed to detect mime type for file: \"%s\"\n", filepath);
		magic_close(magic);
		return 0;	// Continue search.
	}
	//printf("Magic: %s\n", mime_type);
	const char *end_of_type = strchrnul(mime_type, '/');
	// Should I check for other delimiters? '+' or ';'?
	unsigned short primary_type_len = end_of_type - mime_type;
	/*char primary_type[primary_type_len];
	strncpy(primary_type, mime_type, primary_type_len);
	//printf("Primary type: %s\n", primary_type);
	if (strcmp(primary_type, "image")) {*/
	if (strncmp(mime_type, "image", primary_type_len)) {
		//printf("Type is not \"image\". Ignoring...\n");
		magic_close(magic);
		return 0;	// Continue search.
	}
	magic_close(magic);
	return 1;
}
short test_file(const file_path_t filepath) {
	switch (check_mime_type(filepath)) {
		case 1: break;		// Proceed.
		case 0: return 0;	// Continue search.
		default:
			fprintf(stderr, "Unexpected return status from check_mime_type(). Aborting.\n");
			return -1;	// Fail.
	}

	switch (wallpaper_is_new((const file_path_t)filepath)) {
		case 1:
			if (verbosity > 0) printf("Found new wallpaper: \"%s\"\n", filepath);
			set_new_current((const file_path_t)filepath, 0);
			// Purposefully flows into next case.
		case -1: return 1;	// Stop the search.
		case 0: return 0;	// Continue search.
		default:
			fprintf(stderr, "Unexpected return status from wallpaper_is_new(). Aborting.\n");
			return -1;	// Fail.
	}
	assert("Flow should not have reached this point.");
}
bool is_path_within_path(const file_path_t a, const file_path_t b) {
	// Tests whether path A is path B or is contained within directory at path B.
	char *start_of_subpath = strstr(b, a);
	if (!start_of_subpath) return false;
	if (start_of_subpath - a > 0) return false;
	return true;
}
int process_inode(
//FTW_ACTIONRETVAL process_inode(
	const char *filepath,
	const struct stat *info,
	const int typeflag,
	struct FTW *pathinfo
) {
	assert(pathinfo->level >= 0);
	if (info->st_size == 0) return 0;	// Continue search.

	switch (typeflag) {
		case FTW_D:
		case FTW_DP:
			//return FTW_CONTINUE;
			break;	// Continue search.
		case FTW_F: {
			return test_file((const file_path_t)filepath);
			/*switch (test_file((const file_path_t)filepath)) {
				case 0:
					return FTW_CONTINUE;	// Continue search.
				case 1:
					return FTW_STOP;	// Done.
				case -1:
					fprintf(stderr, "Aborting search due to error in test_file().\n");
					return FTW_STOP;	// Fail loudly.
				default:
					fprintf(stderr, "Unexpected return value from test_file(). Aborting.\n");
					return FTW_STOP;	// Fail loudly.
			}*/
		}
		case FTW_SL: {
			char   *target;
			target = realpath(filepath, target);
			if (target == NULL) {
				fprintf(stderr, "Failed to parse realpath for symlink: \"%s\"\n", filepath);
				return -1;	// Fail loudly.
			}

			int ret;
			struct stat statbuff;
			if (ret = stat(target, &statbuff)) {
				fprintf(stderr, "Failed to stat target of symlink: \"%s\"\n", filepath);
				free(target);
				if (verbosity > 1) {
					perror(NULL);
					strerror(errno);
				}
				return -1;	// Fail loudly.
			}
			assert(ret == 0);
			switch (statbuff.st_mode & S_IFMT) {
				case S_IFDIR: {
					if (!is_path_within_path(target, wallpaper_path)) {
							if (!follow_symlinks_beyond_specified_directory) {
								if (verbosity) printf("Ignoring symlink to directory outside wallpaper directory: \"%s\"\n", filepath);
								break;	// Continue search.
							}
							if (verbosity > 3) printf("Following symlink beyond wallpaper directory, as requested.\n");
					}
					if (s_directory_depth_remaining <= 0) {
						fprintf(stderr, "Reached maximum directory depth. Skipping directory: \"%s\"\n", target);
						break;	// Continue search.
					}
					ret = nftw(target, process_inode, --s_directory_depth_remaining, FTW_PHYS);
					s_directory_depth_remaining++;	// This is ugly.
					assert(0 <= s_directory_depth_remaining <= MAX_DIRECTORY_DEPTH);
					break;
				}
				case S_IFREG:
					if (verbosity > 1) printf("%s -> %s\n", filepath, target);
					ret = test_file(target);
					break;
				default:
					fprintf(stderr, "Detected symlink to unsupported inode file type: \"%s\"\n", filepath);
					break;	// Continue search.
			}

			free(target);
			return ret;
		}
		case FTW_SLN:
			fprintf(stderr, "Dangling symlink: \"%s\"\n", filepath);
			//return FTW_CONTINUE;
			break;	// Continue search.
		case FTW_DNR:
			fprintf(stderr, "Unreadable directory: \"%s\"\n", filepath);
			//return FTW_SKIP_SUBTREE;
			break;	// Continue search.
		default:
			fprintf(stderr, "Unknown type of thing: \"%s\"\n", filepath);
			//return FTW_CONTINUE;
			break;	// Continue search.
	}

	//return FTW_CONTINUE;
	return 0;	// Continue search.
}

/* Parameter handlers: */

bool handle_set(const arg_list_t * const al) {	// It would be nice if this weren't necessary.
	//return set_new_current((const file_path_t)arg[0]);
	assert(al->ct == 1);
	assert(al->args[0]);
	return set_new_current(al->args[0], 0);
	//return did_something = true;
}
bool handle_set_new(const arg_list_t * const al) {
	// Invalid directory path?
	//if (dirpath == NULL || *dirpath == '\0') return errno = EINVAL;
	//if (wallpaper_path == NULL || *wallpaper_path == '\0') {
	/*if (!wallpaper_path) {
		size_t len = data_directory.length();
		//size_t len = strlen(data_directory);
		if (len > MAX_PATH_LENGTH) {
			fprintf(stderr, "data_directory exceeds MAX_PATH_LENGTH.\n");
			return false;
		}
		strcpy(wallpaper_path, data_directory.c_str());
		//strcpy(wallpaper_path, data_directory);
		strncat(wallpaper_path, "/", MAX_PATH_LENGTH - ++len);
		strncat(wallpaper_path, DEFAULT_WALLPAPER_DIR_NAME, MAX_PATH_LENGTH - len);
		if (wallpaper_path == NULL || *wallpaper_path == '\0') {	// Sanity check.
			fprintf(stderr, "wallpaper_path is empty.\n");
			return false;
		}
	}*/
	if (!wallpaper_path) {
		if (!data_directory) data_directory = get_xdg_data_home();
		if (!data_directory || strlen(data_directory) == 0) {
			fprintf(stderr, "Failed to get X.D.G. data_directory. Aborting.\n");
			clean_up();
			return false;
		}
		//const size_t len = sizeof(data_directory.length())+1+sizeof(DEFAULT_WALLPAPER_DIR_NAME);
		const size_t len = strlen(data_directory)+1+sizeof(DEFAULT_WALLPAPER_DIR_NAME);
		wallpaper_path = (file_path_t)malloc(len+1);
		//snprintf(wallpaper_path, len, "%s%c%s\0", data_directory.c_str(), '/', DEFAULT_WALLPAPER_DIR_NAME);
		snprintf(wallpaper_path, len, "%s%s\0", data_directory, "/" DEFAULT_WALLPAPER_DIR_NAME);
	}

	s_directory_depth_remaining = MAX_DIRECTORY_DEPTH;
	switch (nftw(wallpaper_path, process_inode, MAX_DIRECTORY_DEPTH, FTW_PHYS)) {
	//switch (nftw(wallpaper_path, process_inode, MAX_DIRECTORY_DEPTH, FTW_PHYS | FTW_ACTIONRETVAL)) {
		case 1:
			// Wallpaper should have been set successfully.
			return true;
		case 0:
			fprintf(stderr, "Failed to find a new wallpaper.\n");
			//return false;
			break;
		case -1:
			fprintf(stderr, "nftw returned an error status.\n");
			//return false;
			break;
		default:
			fprintf(stderr, "Unexpected return status from nftw.\n");
			//return false;
			//break;
	}

	//return set_new_current(fp);
	return false;
}

/*bool handle_set_recent(param_arg_ct argcnt, argument *arg) {
	tags_t n_criteria = encode_tag(TAG_HISTORIC);	// Do not bias toward wallpapers set more often.
	ts_query query = {
		.order = Order::DESCENDING,
		.limit = 1,
		//.n_criteria = encode_tag(TAG_HISTORIC)
		.n_criteria = n_criteria	// HISTORIC
	};
	options = get_rows_by_ts(data_file_path, query);
	if (options == nullptr) {
		fprintf(stderr, "get_rows_by_ts() returned nullptr. Is this ok?\n");
	} else {
		if (options->ct == 0) {
			fprintf(stderr, "Failed to get_rows_by_ts(). No entries match the query.\n");
		} else {
			return set_new_current(options->row[0]->file, options->row[0]->tags);
		}
		free_rows(options);
	}
	return false;
}*/
bool handle_set_fav(const arg_list_t * const al) {
	// Choose a favourite file.
	// 	set_wallpaper "$(grep -E '[Ff]avourite\s*$' < "$LogFile" | cut -d\  -f2 | sort -Ru | head -1)"
	// 	(TODO) Prefer not:
	// 		A wallpaper set the present day.
	// 		If all favourites have been set on the present day, cycle from last to current.
	// 			If only the current wallpaper is a favourite, or there are no favourites, do nothing.
	// 	Otherwise random.
	tags_t p_criteria = encode_tag(TAG_FAVOURITE);
	tags_t n_criteria = encode_tag(TAG_CURRENT);
	n_criteria |= encode_tag(TAG_HISTORIC);	// Do not bias toward wallpapers set more often.
	rows_t *favs = get_rows_by_tag(data_file_path, &p_criteria, &n_criteria, NULL);
	// For now, the database entries are in descending chronological order.

	if (favs == NULL) {
		fprintf(stderr, "No favourites were found.\n");
		return false;
	}
	//switch (sizeof(favs)/sizeof(favs[0])) {
	switch (favs->ct) {
		case 0:
			fprintf(stderr, "No favourites were found (and res != NULL).\n");
			return false;
			break;
		case 1:
			fprintf(stderr, "Current wallpaper is the only favourite. Aborting.\n");
			return false;
			break;
	}

	// Select a random favourite. Not ideal, but simple.
	srand(time(NULL));
	int which = rand() % favs->ct;

	bool ret = set_new_current(favs->row[which]->file, favs->row[which]->tags | n_criteria);
	free_rows(favs);
	return ret;
}
/*bool handle_set_prev(param_arg_ct argcnt, argument *arg) {
	// Detect the previous wallpaper?
	// For this to be useful...
	// 	The database's "current" should not be modified.
	// 	The database must use a cursor tag to track the initial "current".
	// 	handle_set_next() should be relative to the cursor, not "current".
	// 	Any other function that sets a wallpaper should either...
	// 		Clear the cursor, such that invoking handle_set_prev again would set the previous "current".
	// 		Set a timestamp, which could be used to timeout the cursor after some period (maybe an hour or day).
	// Send it to set_new_current().
	return true;
}
bool handle_set_next(param_arg_ct argcnt, argument *arg) {
	// Detect the next wallpaper?
	// Send it to set_new_current().
	return true;
}*/

bool handle_fav_current(const arg_list_t * const al) {
	tags_t criteria = encode_tag(TAG_CURRENT);
	tags_t tags_to_add = encode_tag(TAG_FAVOURITE);
	if (add_tag_by_tag(data_file_path, &criteria, &tags_to_add) == 0) {
		fprintf(stderr, "Failed to mark current as favourite.\n");
		return false;
	}
	return true;
}
bool handle_delete_current(const arg_list_t * const al) {
	tags_t p_criteria = encode_tag(TAG_CURRENT);
	tags_t n_criteria = encode_tag(TAG_FAVOURITE);	// Do not delete current if it's a favourite.
	rows_t rows;
	bool ret = del_entry_by_tag(&rows, data_file_path, &p_criteria, &n_criteria);
	if (rows.ct > 0) {
		printf("Entries deleted from database: %lu\nDeleting files...\n", rows.ct);
		for (num_rows i = 0; i < rows.ct; i++) {	// Delete the files.
			if (unlink(rows.row[i]->file)) fprintf(stderr, "Failed to delete file: \"%s\"\n", rows.row[i]->file);
		}
		printf("Changing the current wallpaper...\n");
		handle_set_new(NULL);
		return true;
	} else {
		if (ret) fprintf(stderr, "Nothing was deleted. Is the current wallpaper a favourite?\n");
		free_rows(&rows);
	}
	return false;
}

/*bool handle_sanity_check(param_arg_ct argcnt, argument *arg) {
	// Report and abort in case of pre-existing temp file.
	// Create temp file.
	// Copy sane entries to the temp file.
	// Back up the working file.
	// Replace working file with temp file.

	// Sanity check for:
	// 	Non-existant files.
	// 	Integrity of existing files?
	// get_current() checks for duplicate entries tagged as current.
	// get_tag_mask() checks for duplicate tags on a single entry.
	
	did_something = true;
	return sanity_check(data_file_path);
}*/
bool handle_print(const arg_list_t * const al) {
	assert(al == NULL || al->args[0]);
	if (verbosity == 0) return false;

	rows_t *currents = get_current(data_file_path, al ? al->args[0] : NULL);
	if (currents == NULL) {
		fprintf(stderr, "Error getting current wallpaper.\n");
		return false;
	}
	assert(currents->ct > 0);

	// Used to align colons.
	static const uint_fast8_t LengthOfLongestAttribute = sizeof("Height");

	for (num_rows i = 0; i < currents->ct; i++) {
		printf(
			"Monitor: \"%s\"\n"
				"\t%*s: \"%s\"\n"
			, currents->row[i]->monitor_name
			, LengthOfLongestAttribute, "File" , currents->row[i]->file
		);

		if (currents->row[i]->tags ^= encode_tag(TAG_CURRENT)) {	// Excluding "current", since it would be redundant.
			char tag_string[Max_Tag_String_Len];
			gen_tag_string(tag_string, currents->row[i]->tags);
			// (Implicitly all) "Tags" is a bit misleading but "other tags" looks terrible. Fix later.
			//printf("\t  Tags: %s\n", tag_string);	// Indented to align colons.
			printf(
				"\t%*s: %s\n"
				, LengthOfLongestAttribute, "Tags"
				, tag_string
			);
		}

		image_t *img;
		if (! (img = get_image_size(currents->row[i]->file))) {
			fprintf(stderr, "Error scanning image file.\n");
			continue;
		}
		printf(
			"\t%*s: %*u\n"
			"\t%*s: %*u\n"
			, LengthOfLongestAttribute, "Width", 4, img->width
			, LengthOfLongestAttribute, "Height", 4, img->height
		);

		free(img);
	}
	free_rows(currents);
	return true;
}
bool handle_list_monitors(const arg_list_t * const al) {
	assert(!al);
	assert(s_monitors.ct > 0);
	for (uint_fast16_t i = 0; i < s_monitors.ct; i++) {
		printf(
			"%-*s\n"	// I don't know of a format specifier for unsigned char*.
				"\t   Width: %*u\n"
				"\t  Height: %*u\n"
				"\tx_offset: %*u\n"
				"\ty_offset: %*u\n"
			, 10, (char*)(s_monitors.monitor[i].name)
			, 4, s_monitors.monitor[i].width
			, 4, s_monitors.monitor[i].height
			, 4, s_monitors.monitor[i].offset_x
			, 4, s_monitors.monitor[i].offset_y
		);
	}
	return true;
}

/*/
 * Above are run modes.
 * Below are for initialising runtime parameters.
/*/

bool handle_database_path(const arg_list_t * const al) {
	assert(al->ct == 1);
	assert(al->args[0]);
	if (access(al->args[0], F_OK) != 0) {	// Check whether exists and is readable.
		fprintf(stderr, "Failed to access database at path: \"%s\"\n", al->args[0]);
		return false;
	}
	const size_t len = strlen(al->args[0]);
	if (!len) {
		fprintf(stderr, "Database path length is 0.\n");
		return false;
	}
	data_file_path = (file_path_t)malloc(len+1);
	//strcpy(data_file_path, *al->args[0]);
	memcpy(data_file_path, al->args[0], len);
	data_file_path[len] = '\0';
	if (verbosity > 1) printf("Using database path: \"%s\"\n", data_file_path);
	return true;
}
bool handle_wallpaper_path(const arg_list_t * const al) {
	assert(al->ct == 1);
	assert(al->args[0]);
	if (access(al->args[0], F_OK) != 0) {	// Check whether exists and is readable.
		fprintf(stderr, "Failed to access wallpaper at path: \"%s\"\n", al->args[0]);
		return false;
	}
	const size_t len = strlen(al->args[0]);
	if (!len) {
		fprintf(stderr, "Wallpaper path length is 0.\n");
		return false;
	}
	wallpaper_path = (file_path_t)malloc(len+1);
	//strcpy(wallpaper_path, al->args[0]);
	memcpy(wallpaper_path, al->args[0], len);
	wallpaper_path[len] = '\0';
	if (verbosity > 1) printf("Using specified wallpaper path: \"%s\"\n", wallpaper_path);
	return true;
}
bool handle_follow_symlinks_beyond_specified_directory(const arg_list_t * const al) {
	assert(al == NULL);
	follow_symlinks_beyond_specified_directory = true;
	if (verbosity > 1) printf("User set: follow_symlinks_beyond_specified_directory\n", wallpaper_path);
	return true;
}
bool handle_scale_for_wm(const arg_list_t * const al) {
	assert(al);
	assert(al->ct == 1);
	assert(al->args[0]);
	if (is_text_true(al->args[0])) scale_for_wm = true;
	else scale_for_wm = false;
	if (verbosity > 1) printf("User set: scale_for_wm = %s\n", scale_for_wm ? "true" : "false");
	return true;
}
bool handle_target_monitor(const arg_list_t * const al) {
	assert(al);
	assert(al->ct == 1);
	assert(al->args[0]);
	s_target_monitor_name = al->args[0];
	return true;
}

/* Main: */

static inline bool load_application_component(const enum AppComponent component) {
	switch (component) {
		case COMPONENT_NONE :
			break;
		case COMPONENT_X11 :
			atexit(graphics_clean_up);	// Close connection to display server on return.
			s_monitors = get_monitor_info();
			if (!s_monitors.ct) {
				fprintf(stderr, "Failed to get_monitor_info.\n");
				return false;
			}
			assert(s_monitors.monitor);

			//
			// Make sure we know which monitor to operate on:
			//
			/*if (s_target_monitor_id) {
				// Validate user specified monitor I.D.
				if (!(s_target_monitor_name = get_monitor_name_from_id(s_target_monitor_id))) {
					fprintf(stderr, "Invalid monitor/output I.D.: %d\n", s_target_monitor_id);
					return false;
				}
			} else*/
			if (s_target_monitor_name) {
				if (!(s_target_monitor_id = get_monitor_id_from_name(s_target_monitor_name))) {
					fprintf(stderr, "Specified monitor name does not exist: \"%s\"\n", s_target_monitor_name);
					return false;
				}
			} else if (s_monitors.ct == 1) {
				s_target_monitor_id = s_monitors.monitor[0].id;
				if (verbosity >= 3) printf("Defaulting to the only monitor detected: \"%s\"\n", s_monitors.monitor[0].name);
			/*} else {	// Can't centralise failure here because it would preclude `--list-monitors`.
				// To do: use active monitor as default.
				fprintf(stderr, "No target monitor. Aborting.\n");
				return false;*/
			}

			break;
		case COMPONENT_DB :
			if (!data_file_path) {	// Fall back on default database path.
				if (!data_directory) data_directory = get_xdg_data_home();	// Currently always true.
				//size_t len = data_directory ? strlen(data_directory) : 0;
				size_t len;
				if (!data_directory || (len = strlen(data_directory)) == 0) {
					fprintf(stderr, "Failed to get X.D.G. data_directory. Aborting.\n");
					//if (data_directory) free(data_directory);
					clean_up();
					return false;
				}
				// Should probably check to confirm that adding to len won't exceed max.
				len += 1+sizeof(DEFAULT_DATA_FILE_NAME);	// +1 for '/'.
				data_file_path = (file_path_t)malloc(len+1);
				snprintf(data_file_path, len, "%s%s\0", data_directory, "/" DEFAULT_DATA_FILE_NAME);
				//free(data_directory);
			}
			break;
		default:
			fprintf(stderr, "Unknown AppComponent: %d\n", component);
	}
	return true;
}
int main(int argc, char** argv) {
	atexit(clean_up);


	// Note: init functions do not currently have access to data_file_path.
	if (! init(argc, argv)) return 2;	// Parse C.L.I. and config file(s).

	if (run_mode_params.ct == 0) {	// Has the user specified something to do?
		fprintf(stderr, "No action specified.\n");
		return 0;
	}


	// Application components are loaded after init, so parameters can affect their load process.
	// 	This means Init type parameter handlers can not reference their results (e.g. monitor info).
	if (app_components) {
		for (app_components_t mask_it = 1; app_components >= mask_it; mask_it <<= 1) {
			if (!load_application_component(app_components & mask_it)) {
				fprintf(stderr, "Failed to load application component: %d\n", app_components & mask_it);
				return 3;
			}
		}
	}


	bool status = true;
	for (param_ct i = 0; i < run_mode_params.ct; i++) {	// Execute requested functions.
		const handler_set_t *hs = run_mode_params.hs[i];
		if (! (status = hs->fn(hs->arg_list))) break;	// Break on error (status=false).
	}

	return status ? 0 : 1;
};
