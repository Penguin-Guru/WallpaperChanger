#pragma once

#include <stdint.h>
#include "parameter.h"


static uint_fast8_t terms_pending = 0;

static parameter_t * match_short_param(const short_flag_t arg);
static parameter_t * match_long_param(const long_flag_t arg);

bool parse_params(int argc, char** argv);

