#!/bin/bash

project_root=$(dirname -- "${BASH_SOURCE[0]}")/../..

declare target_file
if [[ "$1" ]]; then target_file="$1"
else target_file="$project_root/redo.sh"
fi
if [[ ! -f "$target_file" ]]; then
	echo "Target file does not exist: \"$target_file\"" 1>&2
	return
fi

function _bash_completion_for_wallpaperchanger_build_script() {
	local -a known_parameters=(
		"again"
		"debug"
		"gdb"
		"analyser"
		"analyzer"
		"print"
		"verbose"
		"production"
		"-D"
	)

	if [ -n "$2" ]; then
		COMPREPLY=($(compgen -W "${known_parameters[*]}" "$2"))
		#COMPREPLY=($(compgen -W "${known_parameters[*]}" "${COMP_WORDS[COMP_CWORD]}"))
	else
		COMPREPLY=("${known_parameters[@]}")
	fi
}

complete -F _bash_completion_for_wallpaperchanger_build_script "$(basename "$target_file")"

