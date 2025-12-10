#pragma once

#include <stdbool.h>
#include <assert.h>
#include "util.h"	// For stdint.h and file_path_t.

#ifndef MAX_INODE_TESTS
#	define MAX_INODE_TESTS sizeof(uint_fast8_t)*8
#endif

typedef struct inode_test_set_t {
	uint_fast8_t ct;
	short ret;
	// Array of functions to be executed sequentially. See run_test_set below.
	short (*test[MAX_INODE_TESTS])(const file_path_t filepath);     // Hard-coded array length is not ideal.
} inode_test_set_t;


static inline void append_test_to_set(struct inode_test_set_t *tests, short(*test)(const file_path_t filepath)) {
	assert(tests->ct < MAX_INODE_TESTS);
	tests->test[tests->ct++] = test;
}

/*static inline void clear_test_set(struct inode_test_set_t *tests) {
	assert(tests->ct >= 0);
	for (uint_fast8_t i = 0; i < tests->ct; i++) tests->test[i] = NULL;
	tests->ct = 0;
}*/

static inline short run_test_set(struct inode_test_set_t *tests, const file_path_t filepath) {
	assert(tests->ct >= 0);
	assert(tests->ct <= MAX_INODE_TESTS);
	for (uint_fast8_t i = 0; i < tests->ct; i++)
		// Return values:
		// 	 1: Proceed -- file matches criteria.
		// 	 0: Return (to continue search).
		// 	-1: Return (to abort).
		if ((tests->ret = tests->test[i](filepath)) != 1) break;
	return tests->ret;
}

static inline bool is_test_in_set(short(*test)(const file_path_t filepath), struct inode_test_set_t *tests) {
	assert(test);
	static_assert(sizeof(uint_fast8_t)*8 >= MAX_INODE_TESTS, "Data type must be large enough.");
	for (uint_fast8_t i = 0; i < tests->ct; i++) {
		if (tests->test[i] == test) return true;
	}
	return false;
}

