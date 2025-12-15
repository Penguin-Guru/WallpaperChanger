
#define _GNU_SOURCE     // For strchrnul.

#ifdef __has_include
#	if ! __has_include(<sys/stat.h>)
#		error "System does not appear to have a necessary library: \"<sys/stat.h>\""
#	endif   // <sys/stat.h>
#endif  // __has_include

#include <wordexp.h>    // For wordexp and wordfree.
#include <string.h>
#include <stdlib.h>     // For free.
#include <stdio.h>      // For fprintf.
#include <assert.h>
#include <stdint.h>
#include <math.h>       // For powf.
#include <errno.h>
#include <libgen.h>     // For dirname.
#include <unistd.h>     // For access.
#include "config.h"
#include "init.h"
#include "verbosity.h"
#include "argument.h"
#include "parameter.h"

#define SPACE_AND_TAB " \t"
#define WHITESPACE_CHARACTERS SPACE_AND_TAB "\r\n\v\f"
#define KEY_VALUE_DELIMS "=:" WHITESPACE_CHARACTERS
#define VALUE_DELIMS ",;" WHITESPACE_CHARACTERS

// This is sized to contain the max value for data type of "skip" in parse_config_line().
// 255 = floorf(powf(2, sizeof(uint_fast8_t)*8))-1      // -1 to exclude 0 from count.
#define MAX_CONFIG_COLUMN_LENGTH 255


bool parse_config_line(const char * const line) {
	char *op;
	if ((op = strpbrk(line, KEY_VALUE_DELIMS))) {
		const uint_fast16_t len = op - line;  // Data type limits max row length.
		char label[len];
		memcpy(label, line, len);
		label[len] = '\0';

		// Identify the operation/parameter (label).
		parameter_t *param;
		if (!(param = match_long_param(label))) return false;


		// Now we know how many terms/arguments the operation/parameter expects.
		param_arg_ct max_accepted_terms = param->arg_params.max;
		argument args[max_accepted_terms];

		// Parse the terms/arguments.
		char *buff = NULL, *saveptr;
		param_arg_ct arg_ct = 0;
		wordexp_t shell_expanded;
		for(
			buff = strtok_r(op+1, VALUE_DELIMS, &saveptr)
			;buff != NULL && arg_ct <= max_accepted_terms;
			buff = strtok_r(NULL, VALUE_DELIMS, &saveptr)
		) {
			const size_t buff_len = strlen(buff);
			if (buff_len >= MAX_CONFIG_COLUMN_LENGTH) {
				fprintf(stderr,
					"Column in config file exceeds maximum supported length. Aborting execution.\n"
					"\t         Column text: \"%s\"\n"
					"\t       Column length: %zd\n"
					"\tMax supported length: %hu\n"
					, buff
					, strlen(buff)
					, MAX_CONFIG_COLUMN_LENGTH
				);
				return false;
			}
			// Skip key/value delims (that aren't whitespace) in case they were not separated by whitespace.
			static_assert(
				((unsigned short)floorf(powf(2,(sizeof(uint_fast8_t)*8)))) >= MAX_CONFIG_COLUMN_LENGTH,
				"Data type of \"skip\" must be sized to fit potential column length"
			);
			const uint_fast8_t skip = strspn(buff, KEY_VALUE_DELIMS);
			if (skip == buff_len) continue;

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
				wordfree(&shell_expanded);
				return false;
			}
			args[arg_ct++] = shell_expanded.we_wordv[0];
			// wordfree(&shell_expanded) would supposedly only free the value string, which we need. Free later.
		}
		if (arg_ct < param->arg_params.min || arg_ct > max_accepted_terms) {
			print_arg_mismatch(
				param->handler_set.name,
				param->arg_params,
				arg_ct
			);
			return false;
		}
		if (arg_ct < max_accepted_terms) {
			for (param_arg_ct i = arg_ct; i < max_accepted_terms; i++) args[i] = NULL;
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
	assert(end > start);    // Hopefully catch overflows.
	if (*end != '\0') *end = '\0';
	if ((end = strrcpbrk(start, end, chars))) {
		assert(end >= start);
		*end = '\0';    // Clip string at position.
		return end - start;
	}
	return 0;
}

bool parse_config_file(const file_path_t const file_path, const bool is_default_config_path) {
	FILE *f;
	if (!(f = fopen(file_path, "r"))) {
		// A missing config file is not necessarily considered an error here.
		int err = errno;
		const char *description = NULL;
		if (is_default_config_path) {
			switch (err) {
				case ENOENT :   // Flows down.
				case EACCES :
					// This case catches and accepts non-existance of the default config file.
					// Such cases are still considered an error if a parent directory either...
					// 	Was somehow deleted.
					// 	Became a dangling symbolic link.
					size_t dir_len = strlen(file_path);
					char dir[dir_len+1];
					strcpy(dir, file_path);
					dirname(dir);
					if (access(dir, X_OK) == -1) description = "Failed to access default config directory.";
					else {
						// Default config directory exists and user has access to search within it.
						if (access(file_path, F_OK) == -1) {
							// Only the default config file is missing-- this is not an error.
							if (verbosity >= 2) printf("No config file to read.\n");
							return false;
						}
						// File does exist-- presumably the user did not have permission to read it.
						description = "Config file exists but could not be read-- check file permissions.";
					}
			}
		}
		// description may be null if !is_default_config_path or no switch cases apply.
		if (!description) description = "Failed to open config file.";
		fprintf(stderr, "%s\n", description);
		errno = err;
		perror("\tperror says");
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
		char *parse_start = line + strspn(line, WHITESPACE_CHARACTERS);         // Ignore conventional indentation.
		char * const parse_end = strchrnul(parse_start, '#');                   // Check for comment delimiter.
		if (parse_end - parse_start == 0) continue;                             // The whole line is a comment. Ignore it.
		clip_trailing_chars(parse_start, parse_end, WHITESPACE_CHARACTERS);     // Trim trailing whitespace. (unnecessary)
		// Finished pre-processing. Proceed to main parsing of line.
		if (!parse_config_line(parse_start)) {
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
