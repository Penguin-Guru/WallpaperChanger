#define _GNU_SOURCE     // For strchrnul.

#ifdef __has_include
#	if ! __has_include(<ftw.h>)
#		error "System does not appear to have a necessary library: \"<ftw.h>\""
#	endif   // <ftw.h>
#	if ! __has_include(<magic.h>)
#		error "System does not appear to have a necessary library: \"<magic.h>\""
#	endif   // <magic.h>
#	if ! __has_include(<unistd.h>)
#		error "System does not appear to have a necessary library: \"<unistd.h>\""
#	endif   // <unistd.h>
#endif  // __has_include

#include <string.h>
#include <ftw.h>        // Used for finding new wallpapers.
#include <magic.h>      // Used for detecting MIME content types.
#include <unistd.h>     // Used for reading symlinks.
#include <libgen.h>     // Used for dirname.
#include <time.h>
#include <errno.h>
#include <stdlib.h>     // For free.
#include <stdio.h>      // For (f)printf.
#include "wallpaperchanger.h"
#include "init.h"
#include "image.h"
#include "flatfiledb.h"
#define MAX_INODE_TESTS 2	// Currently equal to the number of tests defined.
#include "inode_test_set.h"


extern app_components_t app_components;


/* Utility functions: */

void clean_up() {
	if (s_old_wallpaper_cache.ct) { // Depends on run mode.
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
	if (data_file_path) free(data_file_path);       // Should always be true.
	if (data_directory) free(data_directory);       // Almost always true.
	if (wallpaper_path) free(wallpaper_path);       // Depends on run mode.
}

bool is_text_true(const char * const string) {
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
				|| mode_flags[i] == 'X'
				|| mode_flags[i] == 'x'
			);
			switch (mode_flags[i]) {
				case 'R' :
				case 'r' :
					mode |= R_OK;   // Check read permissions.
					break;
				case 'W' :
				case 'w' :
					mode |= W_OK;   // Check write permissions.
					break;
				case 'X' :
				case 'x' :
					mode |= X_OK;   // Check execute/search permissions.
					break;
				default :
					fprintf(stderr, "Unknown mode flag supplied to is_file_accessible().\n");
					return false;
			}
		}
	} else mode = F_OK;     // Check whether exists (not read/write permissions).

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
static inline int is_directory(const file_path_t const path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0) return 0;     // Not ideal.
   return S_ISDIR(statbuf.st_mode);
}
static inline file_path_t validate_directory_failed(char *msg, file_path_t path, const char * const name, const bool do_reset, const bool do_free) {
	assert(path);

	assert(msg);
	assert(name);
	fprintf(stderr, "%s %s directory: \"%s\"\n", msg, name, path);

	if (do_free) free(path);
	if (do_reset) path = NULL;
	return NULL;
}
static inline file_path_t validate_directory(file_path_t path, const char * const name, const bool do_reset, const bool do_free) {
	assert(path);
	assert(name);
	if (!(path[0] && is_directory(path))) {
		return validate_directory_failed("Invalid", path, name, do_reset, do_free);
	}
	if (!is_file_accessible(path, "X")) {
		return validate_directory_failed("Denied permission to search", path, name, do_reset, do_free);
	}
	return path;
}

static file_path_t get_data_directory() {
	if (data_directory) {
		assert(*data_directory);
		return data_directory;
	}
	// Initialise.
	if (!(data_directory = get_xdg_data_home())) {
		fprintf(stderr, "Failed to get X.D.G. data_directory.\n");
		return NULL;
	}
	return validate_directory(data_directory, "data", true, false);
}
static file_path_t get_wallpaper_path() {
	if (wallpaper_path) {
		assert(wallpaper_path[0]);
		return wallpaper_path;
	}

	file_path_t dd;
	if (!(dd = get_data_directory())) return NULL;
	size_t len;
	if ((len = strlen(dd)) == 0) return NULL;

	assert(DEFAULT_WALLPAPER_DIR_NAME[sizeof(DEFAULT_WALLPAPER_DIR_NAME)-1] == '\0');
	wallpaper_path = (file_path_t)malloc(len+1+sizeof(DEFAULT_WALLPAPER_DIR_NAME));
	memcpy(wallpaper_path, dd, len);
	// Add slash only if not already present.
	if (dd[len-1] == '/') { // -1 to offset array's zero indexing.
		memcpy(wallpaper_path+len, DEFAULT_WALLPAPER_DIR_NAME, sizeof(DEFAULT_WALLPAPER_DIR_NAME));
	} else {
		fprintf(stderr, "Data directory string does not end with a slash. This is unexpected.\n");
		memcpy(wallpaper_path+len, "/" DEFAULT_WALLPAPER_DIR_NAME, 1+sizeof(DEFAULT_WALLPAPER_DIR_NAME));
	}

	return validate_directory(wallpaper_path, "wallpaper", true, true);
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
	if (start_of_relative_path - full_path < 0) return NULL;        // Quick bounds check.
	return start_of_relative_path;
}

short populate_wallpaper_cache();        // Forward declaration.
// Functions used with inode_test_set:   // Count should match MAX_INODE_TESTS (defined above).
short wallpaper_is_new(const file_path_t wallpaper_file_path) {
	if (wallpaper_file_path == NULL || *wallpaper_file_path == '\0') {
		fprintf(stderr, "wallpaper_is_new was provided an empty string.\n");
		return -1;      // Abort.
	}
	if (s_old_wallpaper_cache.ct == 0) {    // Populate the cache if not already done from previous call.
		short ret;
		if ((ret = populate_wallpaper_cache()) != 0) return ret;
	}
	const file_path_t start_of_relative_path = get_start_of_relative_path(wallpaper_file_path);
	if (!start_of_relative_path) return -1; // Abort (without reporting error for each such file).
	if (s_current_wallpaper.path && !strcmp(s_current_wallpaper.path, start_of_relative_path)) return 0;    // Not new (continue search).
	for (unsigned int i = 0; i < s_old_wallpaper_cache.ct; i++) {
		if (!strcmp(s_old_wallpaper_cache.wallpapers[i].path, start_of_relative_path)) return 0;        // Not new (continue search).
	}
	return 1;       // Seems new.
}
short check_mime_type(const file_path_t filepath) {
	struct magic_set *magic = magic_open(MAGIC_MIME_TYPE|MAGIC_CHECK);
	magic_load(magic, NULL);
	const char *mime_type = magic_file(magic, filepath); // Freed by magic_close.
	if (mime_type == NULL || *mime_type == '\0') {
		fprintf(stderr, "Failed to detect mime type for file: \"%s\"\n", filepath);
		magic_close(magic);
		return 0;       // Continue search.
	}
	const char *end_of_type = strchrnul(mime_type, '/');    // Is '/' always the delimiter?
	unsigned short primary_type_len = end_of_type - mime_type;
	if (strncmp(mime_type, "image", primary_type_len)) {
		magic_close(magic);
		return 0;       // Continue search.
	}
	magic_close(magic);
	return 1;       // Mime type matches ("image").
}

static inline void populate_wallpaper_cache_entry_skipper(num_rows * const skipped_ct) {
	// Memory will be resized later, with a single call.
	(*skipped_ct)++;
	s_old_wallpaper_cache.ct--;     // Eliminate some day.
}
short populate_wallpaper_cache() {
	assert(!s_old_wallpaper_cache.ct);
	assert(!s_old_wallpaper_cache.wallpapers);

	num_rows skipped_ct = 0;        // Used for sanity check and realloc.
	rows_t *rows = get_rows_by_tag(data_file_path, NULL, NULL, NULL);
	if (!rows || rows->ct <= 0) {
		if (rows) free_rows(rows);
		return 1;       // Seems new.
	}
	// Allocating memory for all rows is inefficient. Fix later.
	s_old_wallpaper_cache.wallpapers = (wallpaper_info*)malloc(sizeof(wallpaper_info)*rows->ct);
	for (unsigned int i = 0; i < rows->ct; i++) {
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
			continue;
		}

		const file_path_t fp = row->file;
		// Comparing file names may be insufficiently unique.
		// Path is only cached relative to wallpaper_path. This is to reduce memory usage.
		const file_path_t start_of_relative_path = get_start_of_relative_path(fp);
		if (!start_of_relative_path) {
			fprintf(stderr, "Skipping file outside of wallpaper directory: \"%s\"\n", fp);
			skipped_ct++;
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
			skipped_ct++;
		} else {
			cache = &s_old_wallpaper_cache.wallpapers[s_old_wallpaper_cache.ct++];
		}

		const size_t len = strlen(start_of_relative_path);
		if (!len) {
			fprintf(stderr, "Invalid path length. Entry will not be cached.\n");
			populate_wallpaper_cache_entry_skipper(&skipped_ct);
			continue;
		}
		if (! (cache->path = (file_path_t)malloc(len+1))) {     // +1 for terminating null.
			fprintf(stderr, "Failed to allocate memory. Aborting.\n");
			free_rows(rows);
			return -1;      // Abort.
		}
		memcpy(cache->path, start_of_relative_path, len+1);     // +1 for terminating null.

		cache->tags = row->tags;
	}
	assert(s_old_wallpaper_cache.ct == rows->ct - skipped_ct);
	if (skipped_ct) {
		if (!reallocarray(s_old_wallpaper_cache.wallpapers, s_old_wallpaper_cache.ct, sizeof(wallpaper_info))) {
			fprintf(stderr, "Failed to resize old wallpaper cache.\n");
			free_rows(rows);
			return -1;      // Abort.
		}
	}
	free_rows(rows);
	return 0;
}
static inline bool is_path_within_path_helper(const file_path_t const a, const file_path_t const b) {
	const size_t b_len = strlen(b);
	if (strlen(a) < b_len) return false;    // Path can not exist within a longer path.
	// ... A is longer than B.

	const char * const start_of_subpath = strstr(a, b);
	if (!(start_of_subpath && start_of_subpath - a == 0)) return false;     // B does not exist at beginning of A.
	// ... B exists at the beginning of A.

	// Check whether B was specified with a trailing slash.
	const char *checker = *(b + b_len - 1) == '/' ? a + b_len - 1 : a + b_len;
	switch (*checker) {
		case  '/' :     // There is a slash separating B from the rest of A.
		case '\0' :     // There is no need for a slash, because A ends immediately after B.
			return true;
	}
	// ... There is no slash after the assumed directory prefix B that begins A.
	return false;
}
bool is_path_within_path(const file_path_t const a, const file_path_t const b) {
	// Tests whether path A is (either):
	// 	     The same as path B.
	// 	(OR) Contained within B, assuming...
	// 	 	      A and B both exist.
	// 	 	(AND) B is a directory.
	// A does not need to end with a slash, but the corresponding substring within B must.
	// Empty (not null) B is considered to also represent the (relative) root directory.
	// 	This allows for truncation of a path prefix by positioning its pointer to its terminating null.
	// Relative paths are only crudely supported.
	// 	For this function to return true...
	// 	 	      B must begin with a slash unless (either):
	// 	 		     B is empty (interpreted as root).
	// 	 		(OR) A and B are pointers to the same path address.
	// 	 	(AND) A must begin with a slash unless (any):
	// 	 		     B is root (or empty).
	// 	 		(OR) A and B are pointers to the same path address.
	// 	 		(OR) B does not begin with a slash.
	// 	Periods, tildes, and such are interpreted literally.

	assert(a);
	assert(b);
	// ... Neither A nor B should be null pointers.

	if (a[0] == '\0') return false;         // A is empty.
	// ... A is not empty.

	if (
		   a == b                       //    Pointers to same path address.
		|| b[0] == '\0'                 // OR B is empty.
	) return true;
	// ... A and B do not point to the same path address.
	// ... B is not empty.

	if (b[0] != '/') {      // B is not an absolute path.
		if (a[0] == '/') return false;  // A is an absolute path.
		// ... Both A and B appear to be relative paths (not starting with '/').

		return is_path_within_path_helper(a, b);
	}
	// ... B appears to be an absolute path (starts with '/').

	if (b[1] == '\0') return true;  // B is (relative) root-- contains everything.
	// ... B is not the (relative) root.

	if (a[0] != '/') return false;  // A is not an absolute path.
	// ... A appears to be an absolute path (starts with '/').

	return is_path_within_path_helper(a, b);
}
static inline tags_t get_historic_tags_by_path(const file_path_t fp) {
	if (s_old_wallpaper_cache.ct == 0) {
		if (populate_wallpaper_cache() < 0) return 0;	// 0=Null.
	}

	const file_path_t sorp = get_start_of_relative_path(fp);
	assert(sorp && sorp[0]);

	for (size_t i = 0; i < s_old_wallpaper_cache.ct; i++) {
		if (!strcmp(s_old_wallpaper_cache.wallpapers[i].path, sorp)) {
			return s_old_wallpaper_cache.wallpapers[i].tags | encode_tag(TAG_HISTORIC);
		}
	}
	return 0;	// 0=Null.
}


/* Intermediary functions: */

bool set_new_current(const file_path_t wallpaper_file_path, tags_t tags) {
	if (verbosity) printf("Setting wallpaper: \"%s\"\n", wallpaper_file_path);

	// The provided value of tags is expected to be one of:
	// 	HISTORIC (and not CURRENT), indicating that all historically associated tags are included.
	// 	CURRENT (and not HISTORIC), indicating that the file (path) is known to be new.
	// 	Null (0)                  , indicating unknown historic/new status.
	assert( tags == 0 || tags ^ (encode_tag(TAG_CURRENT) | encode_tag(TAG_HISTORIC) ));
	// If file (path) is not known to be new or historic (with tags already loaded), get historic tags.
	if (!tags) tags = get_historic_tags_by_path(wallpaper_file_path) | encode_tag(TAG_CURRENT);
	// Set CURRENT flag if it is not already set.
	else if (!(tags & encode_tag(TAG_CURRENT))) tags |= encode_tag(TAG_CURRENT);

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
	new_entry.tags = tags;
	assert(data_file_path);
	if (!append_new_current(data_file_path, &new_entry)) {
		fprintf(stderr, "Failed to append new current wallpaper to database.\n");
		return false;
	}
	return true;
}

static inline short attempt_set_wallpaper(const file_path_t fp, tags_t tags) {
	if (set_new_current(fp, tags)) return 1;
	// Failed to set the wallpaper.
	if (num_file_skips_remaining) {    // Negative also true.
		if (num_file_skips_remaining > 0) num_file_skips_remaining--;
		return 0;        // Continue search.
	}
	return -1;       // Fail.
}
static struct inode_test_set_t tests;  // Global struct loaded before calling and then referenced within ftw function(s). Eliminate later.
int search_for_wallpaper(
	const char *filepath,
	const struct stat *info,
	const int typeflag,
	struct FTW *pathinfo
) {
	assert(pathinfo->level >= 0);
	if (info->st_size == 0) return 0;       // Continue search.

	switch (typeflag) {
		case FTW_D:
		case FTW_DP:
			break;  // Continue search.
		case FTW_F: {
			short ret;
			if ((ret = run_test_set(&tests, (const file_path_t)filepath)) == 1) {
				// Set CURRENT flag to indicate when file (path) is known to be new.
				tags_t tags = (is_test_in_set(wallpaper_is_new, &tests) ? encode_tag(TAG_CURRENT) : 0);
				ret = attempt_set_wallpaper((const file_path_t)filepath, tags);
			}
			return ret;
		}
		case FTW_SL: {
			char *target;
			char target_buff[PATH_MAX];
			if (!(target = realpath(filepath, target_buff))) {
				fprintf(stderr, "Failed to parse realpath for symlink: \"%s\"\n", filepath);
				return -1;      // Fail loudly.
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
				return -1;      // Fail loudly.
			}
			assert(ret == 0);
			switch (statbuff.st_mode & S_IFMT) {
				case S_IFDIR: {
					if (!is_path_within_path(target, wallpaper_path)) {
							if (!follow_symlinks_beyond_specified_directory) {
								if (verbosity) printf("Ignoring symlink to directory outside wallpaper directory: \"%s\"\n", filepath);
								break;  // Continue search.
							}
							if (verbosity > 3) printf("Following symlink beyond wallpaper directory, as requested.\n");
					}
					if (s_directory_depth_remaining <= 0) {
						fprintf(stderr, "Reached maximum directory depth. Skipping directory: \"%s\"\n", target);
						break;  // Continue search.
					}
					ret = nftw(target, search_for_wallpaper, --s_directory_depth_remaining, FTW_PHYS);
					s_directory_depth_remaining++;  // This is ugly.
					assert(0 <= s_directory_depth_remaining <= MAX_DIRECTORY_DEPTH);
					break;
				}
				case S_IFREG:
					if (verbosity > 1) printf("%s -> %s\n", filepath, target);
					ret = run_test_set(&tests, (const file_path_t)filepath);
					if ((ret = run_test_set(&tests, (const file_path_t)filepath)) == 1) {
						// Set CURRENT flag to indicate when file (path) is known to be new.
						tags_t tags = (is_test_in_set(wallpaper_is_new, &tests) ? encode_tag(TAG_CURRENT) : 0);
						ret = attempt_set_wallpaper((const file_path_t)filepath, tags);
					}
					break;
				default:
					fprintf(stderr, "Detected symlink to unsupported inode file type: \"%s\"\n", filepath);
					break;  // Continue search.
			}

			free(target);
			return ret;
		}
		case FTW_SLN:
			fprintf(stderr, "Dangling symlink: \"%s\"\n", filepath);
			break;  // Continue search.
		case FTW_DNR:
			fprintf(stderr, "Unreadable directory: \"%s\"\n", filepath);
			break;  // Continue search.
		default:
			fprintf(stderr, "Unknown type of thing: \"%s\"\n", filepath);
			break;  // Continue search.
	}

	return 0;       // Continue search.
}

/* Parameter handlers: */

bool handle_set(const arg_list_t * const al) {  // It would be nice if this weren't necessary.
	assert(al == NULL || (al->ct == 1 && al->args[0]));
	const file_path_t const target_path = al ? al->args[0] : get_wallpaper_path();

	if (!is_path_within_path(target_path, get_wallpaper_path())) {
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
			// Path refers to regular file.
			// Attempt to set it as a new current wallpaper.
			// Not using attempt_set_wallpaper because files specified should never be skipped.
			set_new_current(target_path, 0);
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
			const file_path_t const relative_target = target_path + strlen(get_wallpaper_path());
			do {
				assert(s_old_wallpaper_cache.wallpapers[i].path);
				if (
					// Make sure it is in the specified directory.
					is_path_within_path(s_old_wallpaper_cache.wallpapers[i].path, relative_target)
					// Make sure it is not an old entry for the currently set wallpaper.
					&& (
						!s_current_wallpaper.path
						|| strcmp(s_old_wallpaper_cache.wallpapers[i].path, s_current_wallpaper.path)
					)
				) {
					const size_t upper_path_len = strlen(get_wallpaper_path());
					const size_t lower_path_len = strlen(s_old_wallpaper_cache.wallpapers[i].path);
					char buff[upper_path_len + lower_path_len + 1]; // +1 for terminating null.
					memcpy(buff, get_wallpaper_path(), upper_path_len);
					memcpy(buff + upper_path_len, s_old_wallpaper_cache.wallpapers[i].path, lower_path_len);
					buff[upper_path_len + lower_path_len] = '\0';
					switch (attempt_set_wallpaper(buff, encode_tag(TAG_HISTORIC) | s_old_wallpaper_cache.wallpapers[i].tags)) {
						case 0 : continue;
						case 1 : return true;
						default: break;  // Fail.
					}
				}
				if (++i == s_old_wallpaper_cache.ct) i = 0;
			} while (i != start);

			if (verbosity) printf("There do not seem to be any valid wallpapers in the specified directory.\n");
			break;  // Fail.
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
			break;  // Fail.
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
	tags_t p_criteria = encode_tag(TAG_FAVOURITE);
	tags_t n_criteria = encode_tag(TAG_CURRENT);
	n_criteria |= encode_tag(TAG_HISTORIC); // Do not bias toward wallpapers set more often.
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

	bool ret = false;

	// Select a random favourite. Not ideal, but simple.
	srand(time(NULL));
	int start, i;
	if ((i = start = rand() % favs->ct) < 0) {
		fprintf(stderr, "Integer overflow while attempting to generate a random start position within query results.\n");
		return false;
	}
	do {
		assert(favs->row[i]->file);
		switch (attempt_set_wallpaper(favs->row[i]->file, encode_tag(TAG_HISTORIC) | favs->row[i]->tags)) {
			case 0 :
				assert(ret == false);
				if (++i == favs->ct) i = 0;
				continue;
			case 1 :
				ret = true;
				i = start;      // Exit loop.
				break;
			default:        // Fail.
				assert(ret == false);
				i = start;      // Exit loop.
				break;
		}
	} while (i != start);

	free_rows(favs);
	return ret;
}

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
	tags_t n_criteria = encode_tag(TAG_FAVOURITE);  // Do not delete current if it's a favourite.
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
	for (num_rows i = 0; i < res.rows.ct; i++) {    // Delete the files.
		// Note: non-existant files are silently ignored.
		if (unlink(res.rows.row[i]->file) == -1) fprintf(stderr, "\tFailed to delete file: \"%s\"\n", res.rows.row[i]->file);
	}
	free_rows_contents(&res.rows);

	if (verbosity) printf("Changing the current wallpaper...\n");
	handle_set(NULL);
	return true;
}

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

		if (currents->row[i]->tags ^= encode_tag(TAG_CURRENT)) {        // Excluding "current", since it would be redundant.
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
			"%-*s\n"        // I don't know of a format specifier for unsigned char*.
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

	data_file_path = al->args[0];
	if (verbosity > 1) printf("Using database path: \"%s\"\n", data_file_path);
	return true;
}
bool handle_wallpaper_path(const arg_list_t * const al) {
	assert(al->ct == 1);
	assert(al->args[0]);

	if (!(wallpaper_path = validate_directory(al->args[0], "wallpaper", false, false))) return false;

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
bool handle_max_file_skips(const arg_list_t * const al) {
	assert(al);
	assert(al->ct == 1);
	assert(al->args[0]);

	const char * val = al->args[0];
	while (*val == '#' && *val != '\0') val++;      // Skip any leading '#' symbols.
	if (*val == '\0') return false;                 // Do not process empty strings.

	assert(errno == 0);
	errno = 0;
	char *endptr;
	num_file_skips_remaining = strtol(val, &endptr, 10);
	if (errno == ERANGE) {
		fprintf(stderr, "Failed to parse specified value for max-file-skips.\n");
		perror("strtol");
		return false;
	}
	if (endptr == val) {
		fprintf(stderr, "Failed to parse specified value for max-file-skips.\n");
		return false;
	}
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
			atexit(graphics_clean_up);      // Close connection to display server on return.
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
			if (!data_file_path) {  // Fall back on default database path.
				file_path_t dd;
				if (!(dd = get_data_directory())) return false;
				size_t len;
				if ((len = strlen(dd)) == 0) return false;

				// Should probably check to confirm that adding to len won't exceed max.
				len += 1+sizeof(DEFAULT_DATA_FILE_NAME);        // +1 for '/'.
				data_file_path = (file_path_t)malloc(len);
				snprintf(data_file_path, len, "%s%s", dd, "/" DEFAULT_DATA_FILE_NAME);
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
	if (! init(argc, argv)) return 2;       // Parse C.L.I. and config file(s).

	if (run_mode_params.ct == 0) {  // Has the user specified something to do?
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
	for (param_ct i = 0; i < run_mode_params.ct; i++) {     // Execute requested functions.
		const handler_set_t *hs = run_mode_params.hs[i];
		if (! (status = hs->fn(hs->arg_list))) break;   // Break on error (status=false).
	}

	return status ? 0 : 1;
};
