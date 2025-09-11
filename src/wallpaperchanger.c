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
#include <magic.h>	// Used for detecting MIME content types.
#include <unistd.h>	// Used for reading symlinks.
#include <libgen.h>	// Used for dirname.
#include <time.h>
#include <errno.h>
#include <stdlib.h>	// For free.
#include <stdio.h>	// For (f)printf.
#include "wallpaperchanger.h"
#include "init.h"
#include "image.h"
#include "flatfiledb.h"


extern app_components_t app_components;


/* Utility functions: */

void clean_up() {
	if (s_old_wallpaper_cache.ct) {	// Depends on run mode.
		for (size_t i = 0; i < s_old_wallpaper_cache.ct; i++) {
			free(s_old_wallpaper_cache.wallpapers[i].path);
		}
		free(s_old_wallpaper_cache.wallpapers);
	}
	if (s_current_wallpaper.path) free(s_current_wallpaper.path);
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
		if (strcoll((char*)(s_monitors.monitor[i].name), name)) continue;
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

#define IFA_MAX_MODE_FLAGS 2
static inline bool is_file_accessible(const file_path_t file, const char mode_flags[IFA_MAX_MODE_FLAGS]) {
	int mode;
	if (mode_flags) {
		mode = 0;
		for (uint_fast8_t i = 0; i < IFA_MAX_MODE_FLAGS && mode_flags[i]; i++) {
			assert(
				   mode_flags[i] == 'R'
				|| mode_flags[i] == 'r'
				|| mode_flags[i] == 'W'
				|| mode_flags[i] == 'w'
			);
			switch (mode_flags[i]) {
				case 'R' :
				case 'r' :
					mode |= R_OK;	// Check read permissions.
					break;
				case 'W' :
				case 'w' :
					mode |= W_OK;	// Check write permissions.
					break;
				default :
					fprintf(stderr, "Unknown mode flag supplied to is_file_accessible().\n");
					return false;
			}
		}
	} else mode = F_OK;	// Check whether exists (not read/write permissions).

	int status;
	if ((status = access(file, mode)) == 0) return true;
	fprintf(stderr,
		"File is not accessible: \"%s\"\n"
		, file
	);
	char *status_str;
	switch (status) {
		case EACCES :
			status_str = "EACCES: may be caused by:\n"
				"\t\tFile/path not existing.\n"
				"\t\tParent directory not granting search permission to your user account."
			;
			break;
		case EBADF :
			status_str = "EBADF: see manual (2) page for \"access\".";
			break;
		case EFAULT :
			status_str = "EFAULT: see manual (2) page for \"access\".";
			break;
		case EINVAL :
			status_str = "EINVAL: see manual (2) page for \"access\".";
			break;
		case EIO :
			status_str = "EIO: see manual (2) page for \"access\".";
			break;
		case ELOOP :
			status_str = "ELOOP: too many symbolic links.";
			break;
		case ENAMETOOLONG :
			status_str = "ENAMETOOLONG: path is too long.";
			break;
		case ENOENT :
			status_str = "ENOENT: a component of path does not exist or is a dangling symbolic link.";
			break;
		case ENOMEM :
			status_str = "ENOMEM: insufficient kernel memory.";
			break;
		case ENOTDIR :
			status_str = "ENOTDIR: path references a directory that is not a directory.";
			break;
		case EPERM :
			status_str = "EPERM: can not write. File has immutable flag set.";
			break;
		case EROFS :
			status_str = "EROFS: can not write. File is on a read-only filesystem.";
			break;
		case ETXTBSY :
			status_str = "ETXTBSY: will not write. File is an executable which is being executed.";
			break;
		default :
			status_str = "Unknown error (%d): see manual (2) page for \"access\".";
	}
	fprintf(stderr, "\t%s\n", status_str);
	return false;
}


static file_path_t get_wallpaper_path() {
	if (wallpaper_path) {
		assert(wallpaper_path[0]);
		return wallpaper_path;
	}

	// Initialise default.
	if (!data_directory) {
		if (!(data_directory = get_xdg_data_home())) {
			fprintf(stderr, "Failed to get X.D.G. data_directory. Aborting.\n");
			clean_up();
			return NULL;
		}
	}
	assert(data_directory[0]);
	const size_t len = strlen(data_directory);
	assert(DEFAULT_WALLPAPER_DIR_NAME[sizeof(DEFAULT_WALLPAPER_DIR_NAME)-1] == '\0');
	wallpaper_path = (file_path_t)malloc(len+1+sizeof(DEFAULT_WALLPAPER_DIR_NAME));
	memcpy(wallpaper_path, data_directory, len);
	/*assert(data_directory[len-1] == '/');	// -1 to offset array's zero indexing.
	memcpy(wallpaper_path+len, DEFAULT_WALLPAPER_DIR_NAME, sizeof(DEFAULT_WALLPAPER_DIR_NAME));*/
	// Add slash only if not already present.
	if (data_directory[len-1] == '/') {	// -1 to offset array's zero indexing.
		memcpy(wallpaper_path+len, DEFAULT_WALLPAPER_DIR_NAME, sizeof(DEFAULT_WALLPAPER_DIR_NAME));
	} else {
		fprintf(stderr, "Data directory string does not end with a slash. This is unexpected.\n");
		memcpy(wallpaper_path+len, "/" DEFAULT_WALLPAPER_DIR_NAME, 1+sizeof(DEFAULT_WALLPAPER_DIR_NAME));
	}

	assert(wallpaper_path && wallpaper_path[0]);
	return wallpaper_path;
}


/* Intermediary functions: */

bool set_new_current(const file_path_t wallpaper_file_path, tags_t tags) {
	if (verbosity) printf("Setting wallpaper: \"%s\"\n", wallpaper_file_path);

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

	// Validate strings for database format here, so we can abort before setting the wallpaper.
	if (!validate_string_value(target_monitor->name)) {
		fprintf(stderr,
			"Monitor name contains invalid character(s). Aborting.\n"
				"\tName: \"%s\"\n"
				"\tMust not contain any of these characters: \"" COLUMN_DELIMS "\"\n"
			, target_monitor->name
		);
		return false;
	}
	format_path(wallpaper_file_path);
	if (!validate_string_value(wallpaper_file_path)) {
		fprintf(stderr,
			"Wallpaper file name/path contains invalid character(s). Aborting.\n"
				"\tFile: \"%s\"\n"
				"\tMust not contain any of these characters: \"" COLUMN_DELIMS "\"\n"
			, target_monitor->name
		);
		return false;
	}

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
	// Relative paths are used to preserve functionality in the event that a user moves their root wallpaper directory.
	assert(full_path);
	assert(full_path[0]);

	// Consolidate logic with is_path_within_path().
	char *start_of_relative_path = strstr(full_path, get_wallpaper_path());
	if (!start_of_relative_path) return NULL;

	const size_t wpp_len = strlen(get_wallpaper_path());

	// Relative path should start with a slash.
	if (*(start_of_relative_path + wpp_len) != '/') return NULL;

	//start_of_relative_path += sizeof(DEFAULT_WALLPAPER_DIR_NAME);
	start_of_relative_path += wpp_len;
	if (start_of_relative_path - full_path < 0) return NULL;	// Quick bounds check.
	return start_of_relative_path;
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
short populate_wallpaper_cache() {
	if (s_old_wallpaper_cache.ct || s_old_wallpaper_cache.wallpapers) {
		fprintf(stderr,
			"Attempted to re-populate previously initialised cache of old wallpapers. This should not happen.\n"
				"\tCache count: %zu\n"
				"\tPointer initialised: %s\n"
			, s_old_wallpaper_cache.ct
			, (s_old_wallpaper_cache.wallpapers ? "yes" : "no")
		);
		return -1;	// Abort.
	}
	num_rows skipped_ct = 0;	// Used for sanity check.
	rows_t *rows = get_rows_by_tag(data_file_path, NULL, NULL, NULL);
	if (!rows || rows->ct <= 0) {
		if (rows) free_rows(rows);
		return 1;	// Seems new.
	}
	// Allocating memory for all rows is inefficient. Fix later.
	s_old_wallpaper_cache.wallpapers = (wallpaper_info*)malloc(sizeof(wallpaper_info)*rows->ct);
	for (unsigned int i = 0; i < rows->ct; i++, s_old_wallpaper_cache.ct++) {
		const row_t *row = rows->row[i];

		// TAG_HISTORIC wallpapers are not cached, so as to prevent bias toward more frequently set wallpapers.
		// They are skipped here, rather than as a negative tag criteria,
		// 	because we need to cache TAG_CURRENT wallpapers regardless of whether they are historic.
		// 	This is currently not supported by the database implementation.
		if (
			     row->tags & encode_tag(TAG_HISTORIC)
			&& !(row->tags & encode_tag(TAG_CURRENT))
		) {
			skipped_ct++;
			s_old_wallpaper_cache.ct--;
			continue;
		}

		const file_path_t fp = row->file;
		// Comparing file names may be insufficiently unique.
		// Path is only cached relative to wallpaper_path. This is to reduce memory usage.
		const file_path_t start_of_relative_path = get_start_of_relative_path(fp);
		if (!start_of_relative_path) {
			fprintf(stderr, "Skipping file outside of wallpaper directory: \"%s\"\n", fp);
			skipped_ct++;
			s_old_wallpaper_cache.ct--;	// Eliminate some day.
			//decrement_static_wallpapers();
			continue;
		}

		// The currently set wallpaper is cached separately.
		wallpaper_info *cache;
		if (row->tags & encode_tag(TAG_CURRENT)) {
			if (s_current_wallpaper.path || s_current_wallpaper.tags) {
				fprintf(stderr, "Encountered multiple entries for current wallpaper while building cache. Ignoring previous.\n");
				free(s_current_wallpaper.path);
			}
			cache = &s_current_wallpaper;
			s_old_wallpaper_cache.ct--;	// Eliminate some day.
			// Not considered skipped. Not incrementing skipped_ct.
		} else {
			cache = &s_old_wallpaper_cache.wallpapers[s_old_wallpaper_cache.ct];
		}

		const size_t len = strlen(start_of_relative_path);
		if (!len) {
			fprintf(stderr, "Invalid path length. Entry will not be cached.\n");
			skipped_ct++;
			s_old_wallpaper_cache.ct--;	// Eliminate some day.
			//decrement_static_wallpapers();
			continue;
		}
		if (! (cache->path = (file_path_t)malloc(len+1))) {	// +1 for terminating null.
			fprintf(stderr, "Failed to allocate memory. Aborting.\n");
			free_rows(rows);
			return -1;	// Abort.
		}
		assert(start_of_relative_path[len] == '\0');
		memcpy(cache->path, start_of_relative_path, len+1);

		cache->tags = row->tags;
	}
	assert(
		   s_old_wallpaper_cache.ct +1 == rows->ct - skipped_ct	// Account for potential TAG_CURRENT.
		|| s_old_wallpaper_cache.ct == rows->ct - skipped_ct
	);
	if (skipped_ct || s_current_wallpaper.path) {
		if (!reallocarray(s_old_wallpaper_cache.wallpapers, s_old_wallpaper_cache.ct, sizeof(wallpaper_info))) {
			fprintf(stderr, "Failed to resize old wallpaper cache.\n");
			free_rows(rows);
			return -1;
		}
	}
	free_rows(rows);
	return 0;
}
short wallpaper_is_new(const file_path_t wallpaper_file_path) {
	if (wallpaper_file_path == NULL || *wallpaper_file_path == '\0') {
		fprintf(stderr, "wallpaper_is_new was provided an empty string.\n");
		return -1;	// Abort.
	}
	if (s_old_wallpaper_cache.ct == 0) {	// Populate the cache if not already done from previous call.
		short ret;
		if ((ret = populate_wallpaper_cache()) != 0) return ret;
	}
	const file_path_t start_of_relative_path = get_start_of_relative_path(wallpaper_file_path);
	if (!start_of_relative_path) return -1;	// Abort (without reporting error for each such file).
	if (s_current_wallpaper.path && !strcmp(s_current_wallpaper.path, start_of_relative_path)) return 0;	// Not new (continue search).
	for (unsigned int i = 0; i < s_old_wallpaper_cache.ct; i++) {
		if (!strcmp(s_old_wallpaper_cache.wallpapers[i].path, start_of_relative_path)) return 0;	// Not new (continue search).
	}
	return 1;	// Seems new.
}

short check_mime_type(const file_path_t filepath) {
	struct magic_set *magic = magic_open(MAGIC_MIME_TYPE|MAGIC_CHECK);
	magic_load(magic, NULL);
	const char *mime_type = magic_file(magic, filepath); // Freed by magic_close.
	if (mime_type == NULL || *mime_type == '\0') {
		fprintf(stderr, "Failed to detect mime type for file: \"%s\"\n", filepath);
		magic_close(magic);
		return 0;	// Continue search.
	}
	const char *end_of_type = strchrnul(mime_type, '/');	// Is '/' always the delimiter?
	unsigned short primary_type_len = end_of_type - mime_type;
	if (strncmp(mime_type, "image", primary_type_len)) {
		magic_close(magic);
		return 0;	// Continue search.
	}
	magic_close(magic);
	return 1;	// Mime type matches ("image").
}
bool is_path_within_path(const file_path_t a, const file_path_t b) {
	// Tests whether path A is path B or is contained within directory at path B.

	// Files are always within the path of their parent directory.
	size_t len = strlen(a);
	char buff_a[len];
	memcpy(buff_a, a, len+1);	// +1 for terminating null.
	const char *needle = dirname(buff_a);
	if (needle[1] == '\0' && needle[0] == '/') {
		len = strlen(b);
		char buff_b[len];
		memcpy(buff_b, b, len+1);	// +1 for terminating null.
		const char *haystack = dirname(buff_b);
		if (!(haystack[1] == '\0' && haystack[0] == '/')) return false;
	}

	char *start_of_subpath = strstr(b, needle);
	if (!start_of_subpath) return false;
	if (start_of_subpath - a > 0) return false;
	return true;
}
#define MAX_INODE_TESTS 2
typedef struct test_set {
	uint_fast8_t ct;
	short ret;
	// Array of functions to be executed sequentially. See run_test_set below.
	short (*test[MAX_INODE_TESTS])(const file_path_t filepath);	// Hard-coded array length is not ideal.
} test_set;
void append_test_to_set(struct test_set *tests, short(*test)(const file_path_t filepath)) {
	assert(tests->ct < MAX_INODE_TESTS);
	tests->test[tests->ct++] = test;
}
/*void clear_test_set(struct test_set *tests) {
	assert(tests->ct >= 0);
	for (uint_fast8_t i = 0; i < tests->ct; i++) tests->test[i] = NULL;
	tests->ct = 0;
}*/
short run_test_set(struct test_set *tests, const file_path_t filepath) {
	assert(tests->ct >= 0);
	assert(tests->ct <= MAX_INODE_TESTS);
	for (uint_fast8_t i = 0; i < tests->ct; i++)
		// Return values:
		// 	 1: Proceed -- file matches criteria.
		// 	 0: Return (to continue search).
		// 	-1: Return (to abort).
		if ((tests->ret = tests->test[i](filepath)) != 1) break;
	return tests->ret;
}
struct test_set tests;	// Global struct for nftw functions. Eliminate later.
int search_for_wallpaper(
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
			break;	// Continue search.
		case FTW_F: {
			short ret;
			if ((ret = run_test_set(&tests, (const file_path_t)filepath)) == 1)
				set_new_current((const file_path_t)filepath, 0);
			return ret;
		}
		case FTW_SL: {
			char *target;
			char target_buff[PATH_MAX];
			if (!(target = realpath(filepath, target_buff))) {
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
					ret = nftw(target, search_for_wallpaper, --s_directory_depth_remaining, FTW_PHYS);
					s_directory_depth_remaining++;	// This is ugly.
					assert(0 <= s_directory_depth_remaining <= MAX_DIRECTORY_DEPTH);
					break;
				}
				case S_IFREG:
					if (verbosity > 1) printf("%s -> %s\n", filepath, target);
					ret = run_test_set(&tests, (const file_path_t)filepath);
					if ((ret = run_test_set(&tests, (const file_path_t)filepath)) == 1)
						set_new_current((const file_path_t)filepath, 0);
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
			break;	// Continue search.
		case FTW_DNR:
			fprintf(stderr, "Unreadable directory: \"%s\"\n", filepath);
			break;	// Continue search.
		default:
			fprintf(stderr, "Unknown type of thing: \"%s\"\n", filepath);
			break;	// Continue search.
	}

	return 0;	// Continue search.
}

/* Parameter handlers: */

bool handle_set(const arg_list_t * const al) {	// It would be nice if this weren't necessary.
	assert(al == NULL || (al->ct == 1 && al->args[0]));
	const file_path_t const target_path = al ? al->args[0] : get_wallpaper_path();

	if (!is_path_within_path(get_wallpaper_path(), target_path)) {
		fprintf(stderr,
			"Target path is not within configured wallpaper directory.\n"
				"\tTarget path: \"%s\"\n"
				"\tWallpaper directory: \"%s\"\n"
			, target_path
			, get_wallpaper_path()
		);
		return false;
	}

	if (verbosity >= 3) printf("Target path for set wallpaper: \"%s\"\n", target_path);

	// Check whether specified path refers to a regular file or directory.
	struct stat statbuff;
	if (lstat(target_path, &statbuff) == -1) {
		fprintf(stderr, "Error status (-1) returned by lstat.\n");
		return false;
	}
	switch (statbuff.st_mode & S_IFMT) {
		case S_IFREG:
			// Path refers to regular file. Attempt to use it.

			// Apply tags if file has been set before.
			enum Tag tags = TAG_CURRENT;
			const file_path_t sorp = get_start_of_relative_path(target_path);
			assert(sorp && sorp[0]);	// Already confirmed that target_path is within wallpaper_path.
			if (s_old_wallpaper_cache.ct == 0) populate_wallpaper_cache();
			for (size_t i = 0; i < s_old_wallpaper_cache.ct; i++) {
				if (!strcmp(s_old_wallpaper_cache.wallpapers[i].path, sorp)) {
					tags |= s_old_wallpaper_cache.wallpapers[i].tags | encode_tag(TAG_HISTORIC);
					break;
				}
			}

			//target_wallpaper = target_path;
			//break;
			set_new_current(target_path, tags);
			return true;
		case S_IFDIR: {
			// Path refers to a directory.
			// Select a wallpaper from within it (recursively).

			// Prefer new wallpapers if available:
			append_test_to_set(&tests, check_mime_type);
			append_test_to_set(&tests, wallpaper_is_new);
			s_directory_depth_remaining = MAX_DIRECTORY_DEPTH;
			switch (nftw(target_path, search_for_wallpaper, MAX_DIRECTORY_DEPTH, FTW_PHYS)) {
				case 1:
					// Wallpaper should have been set successfully.
					return true;
				case 0:
					// No need to report this. We will use a different approach for file selection.
					break;
				case -1:
					fprintf(stderr, "nftw returned an error status.\n");
					return false;
				default:
					fprintf(stderr, "Unexpected return status from nftw.\n");
					return false;
			}

			// No new wallpaper was available.
			// Use cache to select a random wallpaper with the specified path prefix:
			if (s_old_wallpaper_cache.ct == 0) {
				switch (populate_wallpaper_cache()) {
					case  0: break;
					case  1:
						printf("Failed to find a suitable wallpaper in the specified path.\n");
						return false;
					case -1: return false;
					default:
						 fprintf(stderr, "Unknown return value from populate_wallpaper_cache().\n");
						 return false;
				}
				if (s_old_wallpaper_cache.ct == 0) {
					if (verbosity) printf("Failed to find a suitable wallpaper in the specified path.\n");
					return false;
				}
			}
			assert(s_old_wallpaper_cache.ct > 0);
			assert(s_old_wallpaper_cache.wallpapers);
			srand(time(NULL));
			int start, i;
			if ((i = start = rand() % s_old_wallpaper_cache.ct) < 0) {
				fprintf(stderr, "Integer overflow while attempting to generate a random start position within wallpaper file cache.\n");
				return false;
			}
			do {
				assert(s_old_wallpaper_cache.wallpapers[i].path);
				if (
					// Make sure it is in the specified directory.
					is_path_within_path(s_old_wallpaper_cache.wallpapers[i].path, target_path)
					// Make sure it is not an old entry for the currently set wallpaper.
					&& (
						!s_current_wallpaper.path
						|| strcmp(s_old_wallpaper_cache.wallpapers[i].path, s_current_wallpaper.path)
					)
				) {
					const size_t upper_path_len = strlen(get_wallpaper_path());
					const size_t lower_path_len = strlen(s_old_wallpaper_cache.wallpapers[i].path);
					char buff[upper_path_len + lower_path_len + 1];	// +1 for terminating null.
					memcpy(buff, get_wallpaper_path(), upper_path_len);
					memcpy(buff + upper_path_len, s_old_wallpaper_cache.wallpapers[i].path, lower_path_len);
					buff[upper_path_len + lower_path_len] = '\0';
					set_new_current(buff, s_old_wallpaper_cache.wallpapers[i].tags | encode_tag(TAG_HISTORIC));
					return true;
				}
				if (++i == s_old_wallpaper_cache.ct) i = 0;
			} while (i != start);

			if (verbosity) printf("There do not seem to be any valid wallpapers in the specified directory.\n");
			break;	// Fail.
		}
		case S_IFLNK: {
			// Pass through symlink.
			char *resolved_target_path;
			char resolved_target_path_buff[PATH_MAX];
			if (!(resolved_target_path = realpath(target_path, resolved_target_path_buff))) {
				fprintf(stderr, "Failed to parse realpath for symlink: \"%s\"\n", target_path);
				return false;
			}

			const arg_list_t silly = {
				.ct = 1,
				.args = &resolved_target_path
			};
			bool ret = handle_set(&silly);

			return ret;
		}
		default:
			fprintf(stderr, "Specified path refers to unsupported inode file type: \"%s\"\n", target_path);
			break;	// Fail.
	}

	return false;
}

bool handle_set_new(const arg_list_t * const al) {
	if (!get_wallpaper_path()) return false;

	append_test_to_set(&tests, check_mime_type);
	append_test_to_set(&tests, wallpaper_is_new);
	s_directory_depth_remaining = MAX_DIRECTORY_DEPTH;
	switch (nftw(wallpaper_path, search_for_wallpaper, MAX_DIRECTORY_DEPTH, FTW_PHYS)) {
		case 1:
			// Wallpaper should have been set successfully.
			return true;
		case 0:
			fprintf(stderr, "Failed to find a new wallpaper.\n");
			break;
		case -1:
			fprintf(stderr, "nftw returned an error status.\n");
			break;
		default:
			fprintf(stderr, "Unexpected return status from nftw.\n");
			break;
	}

	return false;
}

bool handle_set_fav(const arg_list_t * const al) {
	// Set an automatically selected favourite wallpaper.
	// 	(TODO) Prefer not:
	// 		A wallpaper set the present day.
	// 		If all favourites have been set on the present day, cycle from last to current.
	// 			If only the current wallpaper is a favourite, or there are no favourites, do nothing.
	// 	Otherwise random.
	tags_t p_criteria = encode_tag(TAG_FAVOURITE);
	tags_t n_criteria = encode_tag(TAG_CURRENT);
	n_criteria |= encode_tag(TAG_HISTORIC);	// Do not bias toward wallpapers set more often.
	rows_t *favs = get_rows_by_tag(data_file_path, &p_criteria, &n_criteria, NULL);
	// At least for now, the database entries are in descending chronological order.

	if (favs == NULL) {
		fprintf(stderr, "No favourites were found.\n");
		return false;
	}
	switch (favs->ct) {
		case 0:
			fprintf(stderr, "No favourites were found (and res != NULL).\n");
			// Purposefully flows into next case.
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
bool handle_restore_recent(const arg_list_t * const al) {
	rows_t *currents = get_current(data_file_path, al ? al->args[0] : NULL);
	if (currents == NULL) {
		fprintf(stderr, "Error getting current wallpaper.\n");
		return false;
	}
	assert(currents->ct > 0);

	monitor_info *target_monitor;
	monitor_id mid;
	row_t *row;
	for (num_rows i = 0; i < currents->ct; i++) {
		row = currents->row[i];

		if (!( mid = get_monitor_id_from_name(row->monitor_name))) {
			if (verbosity) printf(
				"Skipping monitor with unknown I.D. (assuming detached): \"%s\"\n",
				row->monitor_name
			);
			continue;
		}

		if (!is_file_accessible(row->file, "R")) return false;

		// No need to write entry to log, since we are only restoring the most recent entry.
		if (!set_wallpaper(row->file, get_monitor_by_id(mid))) {
			fprintf(stderr, "Failed to set new wallpaper.\n");
			return false;
		}
	}

	return true;
}

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
	assert(al == NULL || (al->ct == 1 && al->args[0] && al->args[0][0]));
	const monitor_name_t target_monitor_name = (al ? al->args[0] : NULL);
	// The specified monitor name may not be connected to the system, so no validation is attempted.

	tags_t p_criteria = encode_tag(TAG_CURRENT);
	tags_t n_criteria = encode_tag(TAG_FAVOURITE);	// Do not delete current if it's a favourite.
	// Preceeding entries marking the same file as a favourite do not prevent deletion.
	// 	This should not matter, since the favourite tag should be inherited when those entries are referenced.
	db_entries_operated_t res = {.rows.ct = 0};
	if (!del_entries(&res, data_file_path, &p_criteria, &n_criteria, target_monitor_name)) {
		return false;
	}
	if (res.rows.ct <= 0) {
		if (verbosity) {
			fprintf(stderr, "Nothing was deleted.\n");
			if (verbosity >= 2) {
				fprintf(stderr, "\tIs the current wallpaper a favourite?\n");
				if (target_monitor_name) fprintf(stderr, "\tDid you specify the monitor name you intended?\n");
			}
		}
		return false;
	}
	printf(
		"Entries deleted from database: %lu\n"
		"Deleting %lu %s...\n"
		, res.ct
		, res.rows.ct
		, (res.rows.ct == 1 ? "file" : "files")
	);
	for (num_rows i = 0; i < res.rows.ct; i++) {	// Delete the files.
		// Note: non-existant files are silently ignored.
		if (unlink(res.rows.row[i]->file) == -1) fprintf(stderr, "\tFailed to delete file: \"%s\"\n", res.rows.row[i]->file);
	}
	free_rows_contents(&res.rows);

	if (verbosity) printf("Changing the current wallpaper...\n");
	handle_set(NULL);
	return true;
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
			// (Implicitly all) "Tags" is a bit misleading but "other tags" looks terrible.
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
 * Above are primary application functions (Run type parameter handlers).
 * Below are for initialising runtime parameters (Init type parameter handlers).
/*/

bool handle_database_path(const arg_list_t * const al) {
	assert(al->ct == 1);
	assert(al->args[0]);
	if (!is_file_accessible(al->args[0], "RW")) return false;
	const size_t len = strlen(al->args[0]);
	if (!len) {
		fprintf(stderr, "Database path length is 0.\n");
		return false;
	}
	assert(al->args[0][len] == '\0');
	data_file_path = (file_path_t)malloc(len+1);
	memcpy(data_file_path, al->args[0], len);
	data_file_path[len] = '\0';
	if (verbosity > 1) printf("Using database path: \"%s\"\n", data_file_path);
	return true;
}
bool handle_wallpaper_path(const arg_list_t * const al) {
	assert(al->ct == 1);
	assert(al->args[0]);
	if (!is_file_accessible(al->args[0], "R")) return false;
	const size_t len = strlen(al->args[0]);
	if (!len) {
		fprintf(stderr, "Wallpaper path length is 0.\n");
		return false;
	}
	assert(al->args[0][len] == '\0');
	wallpaper_path = (file_path_t)malloc(len+1);
	memcpy(wallpaper_path, al->args[0], len);
	wallpaper_path[len] = '\0';
	if (verbosity > 1) printf("Using specified wallpaper path: \"%s\"\n", wallpaper_path);
	return true;
}
bool handle_follow_symlinks_beyond_specified_directory(const arg_list_t * const al) {
	assert(al == NULL);
	follow_symlinks_beyond_specified_directory = true;
	if (verbosity > 1) printf("User set: follow_symlinks_beyond_specified_directory\n");
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
			/*} else {
				// To do: use active monitor as default.

				// Can't centralise failure here because it would preclude `--list-monitors`.
				//fprintf(stderr, "No target monitor. Aborting.\n");
				//return false;*/
			}

			break;
		case COMPONENT_DB :
			if (!data_file_path) {	// Fall back on default database path.
				if (!data_directory) data_directory = get_xdg_data_home();	// Currently always true.
				//size_t len = data_directory ? strlen(data_directory) : 0;
				size_t len;
				if (!data_directory || (len = strlen(data_directory)) == 0) {
					fprintf(stderr, "Failed to get X.D.G. data_directory. Aborting.\n");
					return false;
				}
				// Should probably check to confirm that adding to len won't exceed max.
				len += 1+sizeof(DEFAULT_DATA_FILE_NAME);	// +1 for '/'.
				data_file_path = (file_path_t)malloc(len);
				snprintf(data_file_path, len, "%s%s", data_directory, "/" DEFAULT_DATA_FILE_NAME);
				// data_directory is cached in case it's useful later.
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

	// Initialise cache.
	s_old_wallpaper_cache.ct = 0;
	s_old_wallpaper_cache.wallpapers = NULL;

	bool status = true;
	for (param_ct i = 0; i < run_mode_params.ct; i++) {	// Execute requested functions.
		const handler_set_t *hs = run_mode_params.hs[i];
		if (! (status = hs->fn(hs->arg_list))) break;	// Break on error (status=false).
	}

	return status ? 0 : 1;
};
