#include <string.h>
#include <assert.h>
#include "verbosity.h"

uint_fast8_t verbosity = 1;

bool init::handle_verbosity(arg_list_t *al) {
	assert(al != nullptr);
	assert(al->ct == 1);
	assert(al->args);
	assert(al->args[0]);
	// Could assign to an unsigned long for comparison against USHRT_MAX.
	verbosity = (unsigned short)strtoul(al->args[0], NULL, 0);
	if (verbosity >= 2) printf("Set verbosity: %hu\n", verbosity);
	return true;
}

