#pragma once

#include "util.h"
#include "argument.h"
#include "parameter.h"


bool parse_line(char *line);
bool parse_file(const file_path_t file_path);	// Make file_path const.

