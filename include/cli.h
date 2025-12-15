#pragma once

#include <stdint.h>
#include "parameter.h"


static uint_fast8_t terms_pending = 0;

bool parse_params(int argc, char** argv);

