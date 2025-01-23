// Initially based on: https://github.com/emcrisostomo/libxdgbasedir/tree/master
#pragma once

#define XDG_DATA_HOME_SUFFIX "/.local/share"
#define XDG_CONFIG_HOME_SUFFIX "/.config"
#define XDG_CACHE_HOME_SUFFIX "/.cache"
#define XDG_DATA_DIRS_DEFAULT "/usr/local/share/:/usr/share/"
#define XDG_CONFIG_DIRS_DEFAULT "/etc/xdg"

// Below are standard environmental variable names. Above are not.

#define HOME "HOME"
#define XDG_DATA_HOME "XDG_DATA_HOME"
#define XDG_CONFIG_HOME "XDG_CONFIG_HOME"


char* get_xdg_config_home();
char* get_xdg_data_home();

