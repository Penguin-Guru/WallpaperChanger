#pragma once

#include <fstream>
#include "util.h"
#include "argument.h"
#include "parameter.h"


namespace init::config {
	bool parse_line(char *line);
	bool parse_file(const_file_path_t file_path);	// Make file_path const.
};
