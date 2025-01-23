#pragma once

#include <stdint.h>
#include "parameter.h"


static uint_fast8_t terms_pending = 0;

static bool match_short_param(short_flag_t arg);
static bool match_long_param(long_flag_t arg);

bool parse_params(int argc, char** argv);

