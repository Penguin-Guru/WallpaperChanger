#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "argument.h"

/*/
 * Terminology (based on a few web references):
 * 	One source's example: git checkout -b "newBranch"
 * 		"git" is a command.
 * 		"checkout" is a subcommand.
 * 		"-b" is an option/flag.
 * 		"newBranch" is an argument.
 * 	Options seem generally understood to be hard-coded, with the purpose of modifying the command's behaviour.
 * 		Some say options must be optional to the command's use.
 * 			I suppose this means that the program must be capable of doing *something* useful without any.
 * 	Some say everything other than or including the initial command are arguments.
 * 	Some say parameter is the term for arguments within the context of a function.
 * 	Some say a parameter is either a subcommand or an option/flag.
 * 	Some say parameters are data provided to either the command or an option-- typically not hard-coded.
 * 	Some say flags start with at least one hyphen while some presumably allow other symbol prefixes.
 * 	Some say flags are options (whether optional or not) that do not accept or require a proceeding argument.
 * 	Some say flags are options (whether optional or not) that demarcate/identify ("flag") a proceeding argument.
 * 	Some say flags always affect a boolean state.
 * 		Some say the default of such values (when no flag is provided) must always be false.
 * 			This implies that such flags always convey a true value to the command.
 * 	Some say options tell the command *how* to act (e.g. "--verbose") while arguments tell it *what* to act on/from (e.g. "file1.txt" or "hostname").
 *
 * 	https://unix.stackexchange.com/questions/285575/whats-the-difference-between-a-flag-an-option-and-an-argument
 * 	https://stackoverflow.com/questions/36495669/difference-between-terms-option-argument-and-parameter
 * 	https://en.wikipedia.org/wiki/Parameter#Computer_programming
 * 	https://en.wikipedia.org/wiki/Parameter_(computer_programming)#Parameters_and_arguments
/*/

typedef const char * param_name ;
typedef const char * param_description;
typedef bool(*param_handler_t)(arg_list_t *al);
typedef struct {
	const param_name name;
		// Name is stored here to allow for verbose debug messages from handler.
		// It also improves readability for pairing of hard-coded values with flag_pair_t.
	const param_description description;
		// Description is used exclusively for printing help text.
		// Storing it here improves readability for pairing of hard-coded values with flag_pair_t.
	const param_handler_t fn;
	arg_list_t *arg_list;	// Heap.
} handler_set_t;

typedef char* const flag_t;
// It may be more clear for flag_t to be renamed flag_value_t.
typedef flag_t long_flag_t;	// Support multi-character short flags-- for now.
typedef flag_t short_flag_t;
typedef struct {
	const long_flag_t long_flag;
	const short_flag_t short_flag;
} flag_pair_t;

enum ParamType {
	INIT,
	RUN
};

typedef struct {
	handler_set_t handler_set;
	flag_pair_t flag_pair;
	enum ParamType type;
} parameter_t;	// Parameter definition. param_def_t

typedef uint_fast8_t param_ct;	// Limits the number of parameters the application may accept.
typedef struct {
	const handler_set_t **hs;	// Heap.
	// Is that double pointer necessary?
	param_ct ct;
} handler_set_list_t;

