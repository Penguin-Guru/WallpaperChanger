#include <assert.h>
#include <stdlib.h>	// For free.
#include "argument.h"

void free_args(arg_list_t * const al) {
	for (param_arg_ct i = 0; i < al->ct; i++) {
		assert(al->args[i] != NULL);
		free(al->args[i]);
	}
	al->ct = 0;
}

void reset_args_buffer(arg_list_t * const al) {	// Used for hand-off without freeing memory.
	assert(al->ct > 0 && al->args != NULL);
	al->ct = 0;
	al->args = NULL;
}

