
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>	// For free.
#include <stdio.h>	// For fprintf.
#include "config.h"
#include "init.h"

//#ifdef INCLUDE_VERBOSITY
#include "verbosity.h"
//#endif


bool parse_line(char *line) {
//bool parse_line(char *line) {
	char *op;
	if (op = strpbrk(line, "=:")) {	// Move these later.
		//char label[] = strlcpy(label, line, op - line);
		char label[op - line];
		strlcpy(label, line, op - line);

		// Identify the operation/parameter (label).
		const parameter_t *param = NULL;
		for (param_ct i = 0; i < num_params_known; i++) {
			if (!strcmp(label, params_known[i].flag_pair.long_flag)) {
				param = &params_known[i];
				break;
			}
		}
		if (!param) return false;

		// Now we know how many terms/arguments the operation/parameter expects.
		param_arg_ct etc = param->handler_set.arg_list->ct;
		//argument args[etc] = {0};
		//char* args[etc] = {0};
		//argument args[etc] = {0};
		argument args[etc] = {};

		// Parse the terms/arguments.
		char *buff = NULL, *saveptr;
		const char Arg_Delims[] = ",; \t\r\n\v\f";	// Move these later.
		param_arg_ct arg_ct = 0;
		for(
			buff = strtok_r(op+1, Arg_Delims, &saveptr)
			;buff != NULL && arg_ct <= etc;
			buff = strtok_r(NULL, Arg_Delims, &saveptr)
		) {
			//strcpy(args[arg_ct++], buff);
			args[arg_ct++] = buff;
		}
		if (arg_ct != etc) {
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

		// Register the operation/parameter for execution.
		/*switch (param->type) {
			case ParamType::RUN :
				run_mode_params.push_back(param_execution_pair{&param->handler_set, args});
				break;
			case ParamType::INIT :
				init_type_params.push_back(param_execution_pair{&param->handler_set, args});
				break;
			default:
				fprintf(stderr, "Invalid parameter type for \"%s\".\n", param->flag_pair.long_flag);
				return false;
		}*/
		//return register_param(param, arg_ct, args);
		arg_list_t al = {
			.ct=arg_ct,
			.args=args
		};
		return register_param(param, &al);
	}
	// Unary operations/parameters are not currently supported.
	return false;
}

bool parse_file(const file_path_t file_path) {
	// Not currently offering to create the file.

	/*if (!access(file_path, F_OK)) {
		fprintf(stderr, "Config file not accessible: \"%s\"\n", file_path);
		return false;
	}*/

	struct stat info;
	if (stat(file_path, &info)) {
		fprintf(stderr, "Failed to read config file: \"%s\"\n", file_path);
		return false;
	}
	if (! (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode) )) {
		fprintf(stderr, "Supposed config file is not a regular file or symlink: \"%s\"\n", file_path);
		return false;
	}
	// Consider seeking default file name if non-default directory is specified without a file name.
	//free(stat);	// ?


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
		// Ignore commented lines.
		if (line[0] == '#') continue;
		if (!parse_line(line)) {
			fprintf(stderr, "Discontinuing the config parse.\n");
			fclose(f);
			return false;
		}
	}
	free(line);

	fclose(f);
	//num_config_files_loaded++;
	return true;
}
