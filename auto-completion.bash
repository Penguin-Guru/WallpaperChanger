#!/bin/bash

declare -a known_parameters=(
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
function _completions() {
	if [ -n "$2" ]; then
		COMPREPLY=($(compgen -W "${known_parameters[*]}" "$2"))
	else
		COMPREPLY=("${known_parameters[@]}")
	fi
}

WD=$(dirname -- "${BASH_SOURCE[0]}")
complete -F _completions "$WD/redo.sh"

