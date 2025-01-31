// https://wiki.gentoo.org/wiki/XDG_directories
// https://github.com/emcrisostomo/libxdgbasedir/tree/master

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "libxdgbasedir.h"

static bool is_absolute_path(const char* path);
bool is_absolute_path(const char* path) {
	return (path[0] == '\0' || path[0] == '/');
}

static char* get_env(const char *name, char *default_value) {
	char *value = getenv(name);
	if (value) {
		//const size_t len = strlen(value) + 2;	// +2 for terminating null and slash.
		const size_t len = strlen(value);
		//char *ret = (char*)malloc(len+2);	// +2 for terminating null and slash.
		char *ret = (char*)malloc(len+1);	// +1 for terminating null.
		char *p = (char*)mempcpy(ret, value, len);
		//if (*(p-1) != '/') *p++ = '/';	// Append slash to streamline conditional concatonation. Presuming directory.
		*p = '\0';
		return ret;
	}
	if (default_value) {
		/*const size_t len = strlen(default_value) + 2;	// +2 for terminating null and slash.
		char *ret = (char*)malloc(len);*/
		const size_t len = strlen(default_value);
		//char *ret = (char*)malloc(len+2);	// +2 for terminating null and slash.
		char *ret = (char*)malloc(len+1);	// +1 for terminating null.
		char *p = (char*)mempcpy(ret, value, len);
		//if (*(p-1) != '/') *p++ = '/';	// Append slash to streamline conditional concatonation. Presuming directory.
		*p = '\0';
		return ret;
	}
	return default_value;
};


char* get_xdg_config_home() {
	char *path = get_env(XDG_CONFIG_HOME, NULL);
	if (!path || !is_absolute_path(path)) {
		char *home = get_env(HOME, NULL);
		if (!home) {
			free(path);
			path = (nullptr_t){0};	// Not sure if useful.
		}
		const size_t home_len = strlen(home);
		const size_t total_len = home_len + sizeof(XDG_CONFIG_HOME_SUFFIX);
		//path = (char*)realloc(path, total_len+2);	// +2 for terminating null and slash.
		path = (char*)realloc(path, total_len+1);	// +1 for terminating null.
		char *pos = (char*)mempcpy(mempcpy(path, home, home_len), XDG_CONFIG_HOME_SUFFIX, sizeof(XDG_CONFIG_HOME_SUFFIX));
		free(home);
		//path[total_len] = '\0';
		//*pos++ = '/';	// Append slash to streamline conditional concatonation. Presumably a directory...
		*pos++ = '\0';
		if (!is_absolute_path(path)) {
			free(path);
			path = (nullptr_t){0};	// Not sure if useful.
		}
	}
	return path;
};

char* get_xdg_data_home() {
	char *path = get_env(XDG_DATA_HOME, NULL);
	if (!path || !is_absolute_path(path)) {
		char *home = get_env(HOME, NULL);
		if (!home) {
			free(path);
			path = (nullptr_t){0};	// Not sure if useful.
		}
		const size_t home_len = strlen(home);
		const size_t total_len = home_len + sizeof(XDG_DATA_HOME_SUFFIX);
		path = (char*)realloc(path, total_len+2);	// +2 for terminating null and slash.
		char *pos = (char*)mempcpy(mempcpy(path, home, home_len), XDG_DATA_HOME_SUFFIX, sizeof(XDG_DATA_HOME_SUFFIX));
		free(home);
		//path[total_len] = '\0';
		*pos++ = '/';	// Append slash to streamline conditional concatonation. Presumably a directory...
		*pos++ = '\0';
		if (!is_absolute_path(path)) {
			free(path);
			path = (nullptr_t){0};	// Not sure if useful.
		}
	}
	return path;
};

