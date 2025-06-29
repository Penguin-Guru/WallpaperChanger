
#define _GNU_SOURCE	// For strchrnul.

#ifdef __has_include
#	if ! __has_include(<sys/stat.h>)
#		error "System does not appear to have a necessary library: \"<sys/stat.h>\""
#	endif	// <sys/stat.h>
#endif	// __has_include

#include <sys/stat.h>
#include <wordexp.h>	// For wordexp and wordfree.
#include <string.h>
#include <stdlib.h>	// For free.
#include <stdio.h>	// For fprintf.
#include <assert.h>
#include <stdint.h>
#include <math.h>	// For powf.
#include "config.h"
#include "init.h"
#include "verbosity.h"
#include "argument.h"
#include "parameter.h"

#define SPACE_AND_TAB " \t"
#define WHITESPACE_CHARACTERS SPACE_AND_TAB "\r\n\v\f"
#define KEY_VALUE_DELIMS "=:" WHITESPACE_CHARACTERS
#define VALUE_DELIMS ",;" WHITESPACE_CHARACTERS


bool parse_line(const char * const line) {
	char *op;
	if ((op = strpbrk(line, KEY_VALUE_DELIMS))) {
		uint_fast16_t len = op - line;	// Data type limits max row length.
		char label[len];
		memcpy(label, line, len);
		label[len] = '\0';

		// Identify the operation/parameter (label).
		parameter_t *param = NULL;
		for (param_ct i = 0; i < num_params_known; i++) {
			if (!strcmp(label, params_known[i].flag_pair.long_flag)) {
				param = &params_known[i];
				break;
			}
		}
		if (!param) return false;

		// Now we know how many terms/arguments the operation/parameter expects.
		param_arg_ct etc = param->arg_params.max;
		argument args[etc];

		// Parse the terms/arguments.
		char *buff = NULL, *saveptr;
		param_arg_ct arg_ct = 0;
		wordexp_t shell_expanded;
		for(
			buff = strtok_r(op+1, VALUE_DELIMS, &saveptr)
			;buff != NULL && arg_ct <= etc;
			buff = strtok_r(NULL, VALUE_DELIMS, &saveptr)
		) {
			// Skip key/value delims (that aren't whitespace) in case they were not separated by whitespace.
			static_assert(
				((unsigned short)floorf(powf(2,(sizeof(uint_fast8_t)*8)))) >= MAX_COLUMN_LENGTH,
				"skip data type must be sized to fit potential column length"
			);
			uint_fast8_t skip = strspn(buff, KEY_VALUE_DELIMS);
			if (strlen(buff) == skip) continue;
			//args[arg_ct++] = buff + skip;
			int wordexp_error;
			if ((wordexp_error = wordexp(buff + skip, &shell_expanded, 0))) {
				char *wordexp_error_string;
				switch (wordexp_error) {
					case WRDE_BADCHAR :
						wordexp_error_string = "Illegal occurrence of newline or one of |, &, ;, <, >, (, ), {, }.";
						break;
					case WRDE_SYNTAX :
						wordexp_error_string = "Shell syntax error, such as unbalanced parentheses or unmatched quotes.";
						break;
					/*case WRDE_CMDSUB :
						wordexp_error_string = "Command substitution requested, but the WRDE_NOCMD flag told us to consider this an error.";
						break;*/
					/*case WRDE_BADVAL :
						wordexp_error_string = "An undefined shell variable was referenced, and the WRDE_UNDEF flag told us to consider this an error.";
						break*/
					case WRDE_NOSPACE :
						wordexp_error_string = "Out of memory.";
						break;
					default :
						wordexp_error_string = "Unknown.";
				}
				fprintf(stderr,
					"Value in config file failed shell expansion. Aborting execution.\n"
					"\tValue was: \"%s\"\n"
					"\t    Error: %s\n"
					, buff + skip
					, wordexp_error_string
				);
			}
			args[arg_ct++] = shell_expanded.we_wordv[0];
			// wordfree(&shell_expanded) would supposedly only free the value string, which we need. Free later.
		}
		if (arg_ct < param->arg_params.min || arg_ct > etc) {
			fprintf(stderr,
				"Config: Invalid number of arguments for parameter \"%s\" (\"%s\").\n"
					"\tFound: %hu\n"
					"\tExpected: %hu\n"
				,
				param->flag_pair.long_flag, param->handler_set.name,
				arg_ct,
				etc
			);
			return false;
		}
		if (arg_ct < etc) {
			for (param_arg_ct i = arg_ct; i < etc; i++) args[i] = NULL;
		}

		// Register the operation/parameter for execution.
		arg_list_t al = {
			.ct=arg_ct,
			.args=args
		};
		return register_param(param, &al, CONFIG_FILE);
	}
	// Unary operations/parameters are not currently supported.
	return false;
}

char* strrcpbrk(const char * const start, const char *end, const char * const reject) {
	// Seek backwards for first non-matching char.
	// This is similar to strpbrk.
	assert(end >= start);
	while (end != start) {
		const char *r = reject;
		while (*r != '\0') {
			if (*r++ == *end) return (char*)end;
		}
		--end;
	}
	return NULL;
}
size_t clip_trailing_chars(char *start, char *end, const char *chars) {
	assert(end > start);	// Hopefully catch overflows.
	if (*end != '\0') *end = '\0';
	if ((end = strrcpbrk(start, end, chars))) {
		assert(end >= start);
		*end = '\0';	// Clip string at position.
		return end - start;
	}
	return 0;
}

bool parse_file(const file_path_t const file_path) {
	// Not currently offering to create the file.

	struct stat info;
	if (stat(file_path, &info)) {
		fprintf(stderr, "Failed to stat config file at path: \"%s\"\n", file_path);
		return false;
	}
	if (! (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode) )) {
		fprintf(stderr, "Supposed config file is not a regular file or symlink: \"%s\"\n", file_path);
		return false;
	}

	/*if (!access(file_path, R_OK)) {
		fprintf(stderr, "Denied permission to read config file: \"%s\"\n", file_path);
		return false;
	}*/


	FILE *f = fopen(file_path, "r");
	if (!f){
		fprintf(stderr, "Failed to open config file: \"%s\"\n", file_path);
		return false;
	}
	fseek(f,0,SEEK_END); long len = ftell(f); fseek(f,0,SEEK_SET);
	if (len == -1) {
		fclose(f);
		return false;
	}
	
	if (verbosity > 1) printf("Loading config file: \"%s\"\n", file_path);

	char *line = NULL;
	size_t size = 0;
	while (getline(&line, &size, f) > 0) {
		char *parse_start = line + strspn(line, WHITESPACE_CHARACTERS);		// Ignore conventional indentation.
		char * const parse_end = strchrnul(parse_start, '#');			// Check for comment delimiter.
		if (parse_end - parse_start == 0) continue;				// The whole line is a comment. Ignore it.
		clip_trailing_chars(parse_start, parse_end, WHITESPACE_CHARACTERS);	// Trim trailing whitespace. (unnecessary)
		// Finished pre-processing. Proceed to main parsing of line.
		if (!parse_line(parse_start)) {
			fprintf(stderr,
				"Invalid line in config file: \"%s\"\n"
					"\tInvalid line is: \"%s\"\n"
					"\tDiscontinuing config parse.\n"
				,
				file_path,
				parse_start
			);
			fclose(f);
			return false;
		}
	}
	free(line);

	fclose(f);
	return true;
}
