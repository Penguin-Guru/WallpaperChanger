#include <string.h>
#include <assert.h>
#include <stdlib.h>     // For strtoul.
#include <stdio.h>      // For printf.
#include "verbosity.h"

uint_fast8_t verbosity = 1;

bool handle_verbosity(const arg_list_t * const al) {
	assert(al != NULL);
	assert(al->ct == 1);
	assert(al->args);
	assert(al->args[0]);
	// Could assign to an unsigned long for comparison against USHRT_MAX.
	verbosity = (unsigned short)strtoul(al->args[0], NULL, 0);
	if (verbosity >= 2) printf("Set verbosity: %hu\n", verbosity);
	return true;
}

