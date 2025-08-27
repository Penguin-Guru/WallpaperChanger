#pragma once

#include <stdbool.h>
#include "util.h"


bool parse_config_line(const char * const line);
bool parse_config_file(const file_path_t const file_path, const bool is_default_config_path);

