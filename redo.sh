#!/bin/bash

## https://stackoverflow.com/questions/59895/how-do-i-get-the-directory-where-a-bash-script-is-located-from-within-the-script
#WD=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
WD=$(dirname -- "${BASH_SOURCE[0]}")
declare SourcePath="$WD/src"
declare IncludePath="$WD/include"
declare Exe="$WD/wallpaperchanger"
declare BuildDir="$WD/build"
#declare ConfigBak=""
declare -a system_libs=(
	xcb
	xcb-image
	xcb-randr
	xcb-util
	xcb-errors
	freeimage
	#ftw
	magic
)
declare -a project_libs=(
	parameters
	verbosity
	cli
	libxdgbasedir
	config
	init
	db
	flatfiledb
	graphics
	image
	argument
)
declare prog=' gcc '
declare debug_string=' -g3 -ggdb -O0 -fno-omit-frame-pointer '
	## -Wanalyzer-symbol-too-complex -Wanalyzer-too-complex
	## -Wall -Wpedantic
	## "-Wl,--build-id" is required for `perf buildid-cache`.
	## "-pg" for static probes (prof/gprof).
	## -fno-builtin
declare -i max_errors=3
	## Apparently not honoured by linker?
	#-Wfatal-errors	## Stop on first error.



#declare -i clean_install=0
declare -i param_again=0
declare -i param_debug=0
declare -i param_print_command=0
declare param_verbose=
declare preprocessor_macros=
declare -a passthrough_params=()

for param in "$@"; do
	case $param in
		#[Cc]lean) clean_install=1 ;;
		[Aa]gain)	param_again=1		;;
		[Dd]ebug)	param_debug=1		;;
		[Gg][Dd][Bb])	param_debug=1		;;
		[Aa]naly[sz]er)	debug_string+=' -fanalyzer -fanalyzer-transitivity -fanalyzer-fine-grained '	;;
		[Pp]rint*)	param_print_command=1	;;
		[Vv]erbose)	param_verbose='true'	;;
		[Pp]rod*)	debug_string=' -O3 '	;;
		-D*)
			if [[ $param == '-D' ]]; then
				echo 'Preprocessor macros must be specified immediately after "-D" (with no space in between).'
				exit 1
			fi
			preprocessor_macros+=" $param "
			;;
		*)
			echo "Assuming intended pass-through parameter: \"$param\""
			passthrough_params+=("$param")
			;;
	esac
done


if ((! param_again)); then
	if ((! param_print_command)); then
		if [[ -d "$BuildDir" ]]; then
			rm -f "$BuildDir/"*
		else
			if [[ -e "$BuildDir" ]]; then
				echo "Failed to make build directory. Conflicting file name: \"$BuildDir\""
				exit 1
			fi
			mkdir "$BuildDir"
		fi
	fi

	declare error_handling_string=" ${param_verbose:+-v} ${max_errors:+-fmax-errors=$max_errors} ${preprocessor_macros} "
		## -Wall -Werror -Wpedantic
	declare include_string=" -I $IncludePath "

	declare libs_string=
	## System libs:
	for lib in "${system_libs[@]}"; do
		libs_string+=" -l $lib "
	done
	## Project libs:
	libs_string+=' -L. '	## Add working directory to library path?
	for lib in "${project_libs[@]}" ; do
		declare -a cmd=($prog $debug_string $error_handling_string $include_string -c "$SourcePath/$lib.c"* -o "$BuildDir/$lib.o")
		if ((param_print_command)); then
			echo "${cmd[*]}"
		else
			if ! ${cmd[*]} ; then
				echo 'Script aborting due to compilation error.'
				exit 1
			fi
			#if ! ar rcs "$BuildDir/$lib.a" "$lib.o" ; then
			#	echo 'Script aborting due to error creating archive file (ar rcs).'
			#	exit 1
			#fi
		fi
		libs_string+=" -l:$BuildDir/$lib.o "
	done

	declare src_files=" $SourcePath/${Exe##*/}.c"*' '
	#declare all_param_string=" -std=c23 $debug_string $error_handling_string $include_string $libs_string -o ${Exe##*/}"
	declare all_param_string=" -std=gnu23 $debug_string $error_handling_string $include_string $libs_string -o ${Exe##*/}"
	declare -a cmd=($prog $src_files $all_param_string)
	if [[ -f "$Exe" ]]; then rm "$Exe"; fi
	if ((param_print_command)); then
		echo "${cmd[*]}"
		exit 0
	else 
		if ! ${cmd[*]} ; then
			echo 'Compilation failed.'
			exit 1
		fi
		echo 'Compilation successful...'

		if ! rm -r "$BuildDir" ; then
			echo "Error removing build directory: \"$BuildDir\""
		fi
	fi
fi

if ((param_debug)); then
	gdb -ex=r --args "$Exe"	${passthrough_params[@]}
else
	while true; do
		declare ans=
		if ((param_print_command)); then
			read -r -p "Which to print? (None|Gdb|Valgrind): " ans
		else
			read -r -p "Run? (Run|No|Gdb|Valgrind): " ans
		fi
		case $ans in
			[Rr]*)
				if ((param_print_command)); then
					echo "\"$Exe\" ${passthrough_params[@]}"
					exit 0
				fi
				echo 'Running program...'
				"$Exe" ${passthrough_params[@]}
				exit $?
				;;
			[Gg]*)
				if ((param_print_command)); then
					echo 'Printing debug commands is not currently supported.'
					exit 1
				fi
				echo "passthrough_params: ${passthrough_params[@]}"
				gdb -ex=r --args "$Exe" ${passthrough_params[@]}
				#gdb -ex=r --args "$Exe"
				exit $?
				;;
			[Vv]*)
				declare ValgrindLog='valgrind.log'
				declare ValgrindCmd="valgrind --log-file=$ValgrindLog --leak-check=full --show-leak-kinds=all --track-origins=yes --read-var-info=yes $Exe ${passthrough_params[@]}"
				if ((param_print_command)); then
					echo "$ValgrindCmd"
					break;
				fi
				$ValgrindCmd
				declare -i ret=$?
				echo "See output in log file: \"$ValgrindLog\""
				exit $ret
				;;
			[Nn]*) exit 0 ;;
		esac
	done
fi

