#!/bin/bash

# These commands were used to run the test case
# Make sure that the $RISCV variable is set to the RISC-V installation path


function run-test {
	local directory="$1"
	local name="$2"

	if [[ -z "$directory" ]]; then
		echo "No directory specified!"
		return 1
	fi

	if [[ -z "$name" ]]; then
		echo "No test case name specified!"
		return 1
	fi

	if [[ ! -d "$directory" ]]; then
		echo "'$directory' is not a directory!"
		return 1
	fi

	cd "$directory"

	if [[ ! -d "tags" ]]; then
		mkdir "tags"
	fi

	if [[ ! -d "no-tags" ]]; then
		mkdir "no-tags"
	fi

	# Compile the test case
	"$RISCV/bin/riscv64-unknown-elf-gcc" "../$name.c" -o "$name"

	# Generate tag and policy files
	tag-parser "$name" "$name.tags" "../$name.policy"

	# Run without tag propagation
	spike-tag -d --debug-cmd=../debug-cmds.txt \
		pk "$name" \
		>no-tags/out.txt 2>no-tags/execution.txt
	spike-tag -d --debug-cmd=../debug-cmds-silent.txt \
		pk "$name" \
		>no-tags/out-silent.txt 2>no-tags/execution-silent.txt

	# Run with tag propagation
	spike-tag --tag-files=policy.d2sc,tags.d2sc -d --debug-cmd=../debug-cmds.txt \
		pk "$name" \
		>tags/out.txt 2>tags/execution.txt
	spike-tag --tag-files=policy.d2sc,tags.d2sc -d --debug-cmd=../debug-cmds-silent.txt \
		pk "$name" \
		>tags/out-silent.txt 2>tags/execution-silent.txt

	cd ..
}

run-test "case-one" "separate-policies"
run-test "case-two" "separate-policies"
run-test "case-three" "separate-policies"