#pragma once

#include <string>
#include <vector>

namespace init {
	typedef char* argument;
	typedef argument* arg_list;
	typedef unsigned short param_arg_ct;	// Limits number of arguments/terms a parameter may accept.
	typedef struct {
		param_arg_ct ct = 0;
		argument *args = nullptr;	// Heap.
	} arg_list_t;

	void free_args(arg_list_t * const al);
	void reset_args_buffer(arg_list_t * const al);
}

