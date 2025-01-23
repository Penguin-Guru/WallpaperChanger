#include <assert.h>
#include "argument.h"

void init::free_args(arg_list_t * const al) {
	for (param_arg_ct i = 0; i < al->ct; i++) {
		assert(al->args[i] != nullptr);
		free(al->args[i]);
	}
	al->ct = 0;
}

void init::reset_args_buffer(arg_list_t * const al) {	// Used for hand-off without freeing memory.
	assert(al->ct > 0 && al->args != nullptr);
	al->ct = 0;
	al->args = nullptr;
}

