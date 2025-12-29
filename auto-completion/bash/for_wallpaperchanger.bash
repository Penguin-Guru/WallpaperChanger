#!/bin/bash

project_root=$(dirname -- "${BASH_SOURCE[0]}")/../..

declare target_file
if [[ "$1" ]]; then target_file="$1"
else target_file="$project_root/wallpaperchanger"
fi
if [[ ! -f "$target_file" ]]; then
	echo "Target file does not exist: \"$target_file\"" 1>&2
	return
fi

declare -a known_parameters=()
declare parameter_file="$project_root/src/parameters.c"
readarray -t known_parameters < <(sed -nE 's/^\s*.name\s*=\s*"([^"]+)".*$/\1/p' "$parameter_file")
if (( ${#known_parameters[@]} <= 0 )); then
	echo "Failed to parse known parameters from file: \"$parameter_file\"" 1>&2
	return
fi

function _bash_completion_for_wallpaperchanger() {
	if [ -n "$2" ]; then
		COMPREPLY=($(compgen -W "${known_parameters[*]}" "$2"))
		#COMPREPLY=($(compgen -W "${known_parameters[*]}" "${COMP_WORDS[COMP_CWORD]}"))
	else
		COMPREPLY=("${known_parameters[@]}")
	fi
}
#eval "function _bash_completion_for_wallpaperchanger() {
#	#local -a known_parameters=(\"${known_parameters[@]}\")
#	local -a known_parameters=()
#	for param in \"${known_parameters[*]}\"; do
#		known_parameters+=(\"\$param\")
#	done
#	if [ -n \"\$2\" ]; then
#		#COMPREPLY=(\$(compgen -W \"${known_parameters[*]}\" \"\$2\"))
#		COMPREPLY=(\$(compgen -W \"\${known_parameters[*]}\" \"\$2\"))
#	else
#		#COMPREPLY=(\"${known_parameters[@]}\")
#		COMPREPLY=(\"\${known_parameters[@]}\")
#	fi
#}"

#complete -F _bash_completion_for_wallpaperchanger "$(basename "$target_file")"
#complete -C "$target_file --print-parameters" "$(basename "$target_file")"
#complete -C "$target_file --print-parameters" -F _bash_completion_for_wallpaperchanger "$(basename "$target_file")"

known_parameters=()
#alias _do_bash_completion_for_wallpaperchanger="_bash_completion_for_wallpaperchanger PROGRAM_NAME \$COMP_WORDS \$COMP_CWORD $known_parameters"
#complete -F _do_bash_completion_for_wallpaperchanger "$(basename "$target_file")"

#complete -C "$target_file --print-parameters" -F _bash_completion_for_wallpaperchanger "$(basename "$target_file")"
complete -C "$target_file --print-raw-parameters" "$(basename "$target_file")"
#complete -C "$project_root/test.sh" "$(basename "$target_file")"

