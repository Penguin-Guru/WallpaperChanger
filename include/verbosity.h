#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "argument.h"

extern uint_fast8_t verbosity;

bool handle_verbosity(const arg_list_t * const al);

