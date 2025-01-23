#pragma once

#include <stdint.h>
#include "argument.h"

extern uint_fast8_t verbosity;	// Consider using bitmask to encode categories.

namespace init {
	bool handle_verbosity(arg_list_t *al);
}

