// We want POSIX.1-2008 + XSI, i.e. SuSv4, features.
#define _XOPEN_SOURCE 700

/* If the C library can support 64-bit file sizes
   and offsets, using the standard names,
   these defines tell the C library to do so. */
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64 

//#define _GNU_SOURCE

#include <string.h>
#include <ftw.h>	// Used for finding new wallpapers.
//#include <dirent.h>	// Used for finding new wallpapers.
#include <magic.h>	// Used for detecting MIME content types.
#include <unistd.h>	// Used for reading symlinks.
#include <ctime>	// ?
#include <errno.h>
#include "wallpaperchanger.h"
#include "init.h"
#include "image.h"
#include "flatfiledb.h"
#include "graphics.h"


void clean_up() {
	if (s_wallpapers) {	// Depends on run mode.
		for (size_t i = 0; i < s_wallpapers_ct; i++) free(s_wallpapers[i]);
		free(s_wallpapers);
	}
	if (data_file_path) free(data_file_path);	// Should always be true.
	if (data_directory) free(data_directory);	// Almost always true.
	if (wallpaper_path) free(wallpaper_path);	// Depends on run mode.
}


/* Misc. intermediary functions: */

bool set_new_current(const file_path_t wallpaper_file_path, tags_t tags) {
	if (!set_wallpaper(wallpaper_file_path)) {
		fprintf(stderr, "Failed to set new wallpaper.\n");
		return false;
	}

	db::row_t new_entry;
	const size_t len = strlen(wallpaper_file_path);
	if (!len) {
		fprintf(stderr, "Invalid length for file path.\n");
		return false;
	}
	new_entry.file = wallpaper_file_path;
	new_entry.tags = tags | db::encode_tag(db::Tag::CURRENT);	// Make sure it's tagged as current.
	if (!db::append_new_current(data_file_path, &new_entry)) {
		fprintf(stderr, "Failed to append new current wallpaper to database.\n");
		return false;
	}
	return true;
}

const file_path_t get_start_of_relative_path(const file_path_t full_path) {
	// Relative paths are used to preserve functionality in the event that a user moves their root wallpaper directory.
	//const char *start_of_relative_path = strstr(full_path, "/" DEFAULT_WALLPAPER_DIR_NAME "/");
	char *start_of_relative_path = strstr(full_path, "/" DEFAULT_WALLPAPER_DIR_NAME "/");
	if (!start_of_relative_path) {
		fprintf(stderr,
			"Failed to parse wallpaper directory (\"%s\") from path: \"%s\"\n",
			"/" DEFAULT_WALLPAPER_DIR_NAME "/", full_path
		);
		return nullptr;
	}
	start_of_relative_path += sizeof(DEFAULT_WALLPAPER_DIR_NAME);
	//start_of_relative_path += sizeof(DEFAULT_WALLPAPER_DIR_NAME) + 1;
	if (start_of_relative_path - full_path < 0) return nullptr;	// Quick bounds check.
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
	if (wallpaper_file_path == nullptr || *wallpaper_file_path == '\0') {
		fprintf(stderr, "wallpaper_is_new was provided an empty string.\n");
		return -1;
	}
	if (s_wallpapers == nullptr) {	// Populate the cache if not already done from previous call.
		tags_t n_criteria = encode_tag(db::Tag::HISTORIC);	// Do not bias toward wallpapers set more often.
		db::rows_t *rows = db::get_rows_by_tag(data_file_path, nullptr, &n_criteria);
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
	if (mime_type == nullptr || *mime_type == '\0') {
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
			set_new_current((const file_path_t)filepath);
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
int handle_inode(
//FTW_ACTIONRETVAL handle_inode(
	const char *filepath,
	const struct stat *info,
	const int typeflag,
	struct FTW *pathinfo
) {
	assert(pathinfo->level >= 0);
	if (info->st_size == 0) {
		fprintf(stderr, "Detected inode entry for object with zero size: \"%s\"\n", filepath);
		return -1;
	}

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
					ret = nftw(target, handle_inode, --s_directory_depth_remaining, FTW_PHYS);
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

bool handle_set(init::arg_list_t *al) {	// It would be nice if this weren't necessary.
	//return set_new_current((const file_path_t)arg[0]);
	assert(al->ct == 1);
	assert(al->args[0]);
	return set_new_current(al->args[0]);
	//return did_something = true;
}
/*
// This implementation looks for database entried with the "new" tag.
// It's commented out because I don't need it and get_row_if_match() would need to support missing timestamp fields.
bool handle_set_new(init::param_arg_ct argcnt, init::argument *arg) {
	bool ret;
	// Choose a recent file.
	// 	Prefer wallpapers not previously viewed.
	// 	Prefer newer wallpapers.
	// 		Consider adding a tag, "new", for wallpapers that have never been set.
	// 		Avoid looping only the most recent wallpapers.
	tags_t p_criteria = encode_tag(db::Tag::NEW);
	tags_t n_criteria = encode_tag(db::Tag::HISTORIC);	// Do not bias toward wallpapers set more often.
	db::rows *options = db::get_rows_by_tag(data_file_path, &p_criteria, &n_criteria);
	if (options == nullptr) {
		fprintf(stderr, "get_rows_by_tag() returned nullptr. Is this ok?\n");
		ret = false;
	} else {
		if (options->ct) {
			// TODO: make set_new_current remove the "new" tag from the initial entry. Perhaps simply replace it with "current".
			ret = set_new_current(options->row[0]->file, options->row[0]->tags);
		}
		db::free_rows(options);
	}
	return ret;
}*/
bool handle_set_new(init::arg_list_t *al) {
	assert(al->ct == 0);
	//int result;

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
		if (!data_directory) data_directory = xdg::data::home();
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
	switch (nftw(wallpaper_path, handle_inode, MAX_DIRECTORY_DEPTH, FTW_PHYS)) {
	//switch (nftw(wallpaper_path, handle_inode, MAX_DIRECTORY_DEPTH, FTW_PHYS | FTW_ACTIONRETVAL)) {
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

/*bool handle_set_recent(init::param_arg_ct argcnt, init::argument *arg) {
	tags_t n_criteria = encode_tag(db::Tag::HISTORIC);	// Do not bias toward wallpapers set more often.
	db::ts_query query = {
		.order = db::Order::DESCENDING,
		.limit = 1,
		//.n_criteria = encode_tag(db::Tag::HISTORIC)
		.n_criteria = n_criteria	// HISTORIC
	};
	options = db::get_rows_by_ts(data_file_path, query);
	if (options == nullptr) {
		fprintf(stderr, "get_rows_by_ts() returned nullptr. Is this ok?\n");
	} else {
		if (options->ct == 0) {
			fprintf(stderr, "Failed to get_rows_by_ts(). No entries match the query.\n");
		} else {
			return set_new_current(options->row[0]->file, options->row[0]->tags);
		}
		db::free_rows(options);
	}
	return false;
}*/
bool handle_set_fav(init::arg_list_t *al) {
	assert(al->ct == 0);
	// Choose a favourite file.
	// 	set_wallpaper "$(grep -E '[Ff]avourite\s*$' < "$LogFile" | cut -d\  -f2 | sort -Ru | head -1)"
	// 	(TODO) Prefer not:
	// 		A wallpaper set the present day.
	// 		If all favourites have been set on the present day, cycle from last to current.
	// 			If only the current wallpaper is a favourite, or there are no favourites, do nothing.
	// 	Otherwise random.
	tags_t p_criteria = encode_tag(db::Tag::FAVOURITE);
	tags_t n_criteria = encode_tag(db::Tag::CURRENT);
	n_criteria |= encode_tag(db::Tag::HISTORIC);	// Do not bias toward wallpapers set more often.
	db::rows_t *favs = db::get_rows_by_tag(data_file_path, &p_criteria, &n_criteria);
	// For now, the database entries are in descending chronological order.

	if (favs == nullptr) {
		fprintf(stderr, "No favourites were found.\n");
		return false;
	}
	//switch (sizeof(favs)/sizeof(favs[0])) {
	switch (favs->ct) {
		case 0:
			fprintf(stderr, "No favourites were found (and res != nullptr).\n");
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
	db::free_rows(favs);
	return ret;
}
/*bool handle_set_prev(init::param_arg_ct argcnt, init::argument *arg) {
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
bool handle_set_next(init::param_arg_ct argcnt, init::argument *arg) {
	// Detect the next wallpaper?
	// Send it to set_new_current().
	return true;
}*/

bool handle_fav_current(init::arg_list_t *al) {
	assert(al->ct == 0);
	tags_t criteria = encode_tag(db::Tag::CURRENT);
	tags_t tags_to_add = encode_tag(db::Tag::FAVOURITE);
	if (db::add_tag_by_tag(data_file_path, &criteria, &tags_to_add) == 0) {
		fprintf(stderr, "Failed to mark current as favourite.\n");
		return false;
	}
	return true;
}
bool handle_delete_current(init::arg_list_t *al) {
	assert(al->ct == 0);
	tags_t p_criteria = encode_tag(db::Tag::CURRENT);
	tags_t n_criteria = encode_tag(db::Tag::FAVOURITE);	// Do not delete current if it's a favourite.
	db::rows_t rows;
	bool ret = db::del_entry_by_tag(&rows, data_file_path, &p_criteria, &n_criteria);
	if (rows.ct > 0) {
		printf("Entries deleted from database: %lu\nDeleting files...\n", rows.ct);
		for (num_rows i = 0; i < rows.ct; i++) {	// Delete the files.
			if (unlink(rows.row[i]->file)) fprintf(stderr, "Failed to delete file: \"%s\"\n", rows.row[i]->file);
		}
		printf("Changing the current wallpaper...\n");
		handle_set_new(nullptr);
		return true;
	} else {
		if (ret) fprintf(stderr, "Nothing was deleted. Is the current wallpaper a favourite?\n");
		free_rows(&rows);
	}
	return false;
}

/*bool handle_sanity_check(init::param_arg_ct argcnt, init::argument *arg) {
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
	return db::sanity_check(data_file_path);
}*/
bool handle_print(init::arg_list_t *al) {
	assert(al->ct == 0);
	if (verbosity == 0) return false;

	db::row_t *current = db::get_current(data_file_path);
	if (current == nullptr) {
		//std::cerr << "Error getting current wallpaper." << std::endl;
		fprintf(stderr, "Error getting current wallpaper.\n");
		return false;
	}

	//std::cout << "Current wallpaper: \"" << current->file << '"' << std::endl;
	printf("Current wallpaper: \"%s\"\n", current->file);

	//image_t img = {.file = current->file};
	//free(current);	// I'm not sure why this isn't necessary.
	//if (! get_image_size(&img)) {
	image_t *img;
	if (! (img = get_image_size(current->file))) {
		//std::cerr << "Error scanning image file." << std::endl;
		fprintf(stderr, "Error scanning image file.\n");
		free_row(current);
		return false;
	}
	free_row(current);
	/*std::cout
		<< "\tWidth: " << img.width
		<< "\n\tHeight: " << img.height
	<< std::endl;*/
	printf("\tWidth: %u\n\tHeight: %u\n", img->width, img->height);

	free(img);
	return true;
}

/*/
 * Above are run modes.
 * Below are for initialising runtime parameters.
/*/

bool handle_database_path(init::arg_list_t *al) {
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
	*((char*)mempcpy(data_file_path, al->args[0], len)) = '\0';
	if (verbosity > 1) printf("Using database path: \"%s\"\n", data_file_path);
	return true;
}
bool handle_wallpaper_path(init::arg_list_t *al) {
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
	*((char*)mempcpy(wallpaper_path, al->args[0], len)) = '\0';
	if (verbosity > 1) printf("Using specified wallpaper path: \"%s\"\n", wallpaper_path);
	return true;
}
bool handle_follow_symlinks_beyond_specified_directory(init::arg_list_t *al) {
	assert(al == nullptr);
	follow_symlinks_beyond_specified_directory = true;
	if (verbosity > 1) printf("User set: follow_symlinks_beyond_specified_directory\n", wallpaper_path);
	return true;
}
/*bool handle_verbosity(init::param_arg_ct argcnt, init::argument *arg) {
	if (argcnt != 1) {
		std::cerr << "Function handle_wallpaper_path expects exactly one argument. Aborting." << std::endl;
		return false;
	}
	// Could assign to an unsigned long for comparison against USHRT_MAX.
	verbosity = (unsigned short)strtoul(arg[0].c_str(), NULL, 0);
	if (verbosity >= 2) printf("Set verbosity: %hu\n", verbosity);
	return true;
}*/

/* Main: */

int main(int argc, char** argv) {
	atexit(clean_up);

	// Note: init functions do not currently have access to data_file_path.
	if (! init::init(argc, argv)) return 1;	// Parse C.L.I. and config file(s).

	if (init::run_mode_params.ct == 0) {	// Has the user specified something to do?
		fprintf(stderr, "No action specified.\n");
		clean_up();
		return 1;
	}


	if (!data_file_path) {	// Fall back on default database path.
		if (!data_directory) data_directory = xdg::data::home();	// Currently always true.
		//size_t len = data_directory ? strlen(data_directory) : 0;
		size_t len;
		if (!data_directory || (len = strlen(data_directory)) == 0) {
			fprintf(stderr, "Failed to get X.D.G. data_directory. Aborting.\n");
			//if (data_directory) free(data_directory);
			clean_up();
			return 1;
		}
		// Should probably check to confirm that adding to len won't exceed max.
		len += 1+sizeof(DEFAULT_DATA_FILE_NAME);	// +1 for '/'.
		data_file_path = (file_path_t)malloc(len+1);
		snprintf(data_file_path, len, "%s%s\0", data_directory, "/" DEFAULT_DATA_FILE_NAME);
		//free(data_directory);
		/*if (len) {
			len += 1+sizeof(DEFAULT_DATA_FILE_NAME);	// +1 for '/'.
			data_file_path = malloc(len);
			snprintf(data_file_path, len, "%s%s\0", data_directory.c_str(), '/' DEFAULT_DATA_FILE_NAME);
		} else {	// This shouldn't happen.
			fprintf(stderr, "Warning: failed to get X.D.G. data_directory.\n");
			// Default to default name in working directory?
			len = sizeof(DEFAULT_DATA_FILE_NAME);
			data_file_path= malloc(len);
			strncpy(data_file_path, DEFAULT_DATA_FILE_NAME, len);
		}*/
	}


	bool status = true;
	for (init::param_ct i = 0; i < init::run_mode_params.ct; i++) {	// Execute requested functions.
		const init::handler_set_t *hs = init::run_mode_params.hs[i];
		if (! (status = hs->fn(hs->arg_list))) break;	// Break on error (status=false).
	}

	return status ? 1 : 0;
};
