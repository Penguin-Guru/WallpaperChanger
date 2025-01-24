#include <assert.h>
#include <stdlib.h>	// For free.
#include "argument.h"

inline void free_args(arg_list_t * const al) {
	if (al == NULL) return;
	for (param_arg_ct i = 0; i < al->ct; i++) {
		assert(al->args[i] != NULL);
		free(al->args[i]);
	}
	al->ct = 0;
}

inline void reset_args_buffer(arg_list_t * const al) {	// Used for hand-off without freeing memory.
	if (al == NULL) return;
	al->ct = 0;
	al->args = NULL;
}

