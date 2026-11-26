#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# tests/run.sh - run the tests of this wrapper and report the counts.
#
#   sh tests/run.sh            run them all
#   sh tests/run.sh json       run only the tests whose name holds "json"
#
# The exit status is 0 when every test script passed.

set -u
here=`dirname "$0"`
cd "$here/.." || exit 2

filter=${1:-}
passed=0
failed=0
failed_names=

for t in "$here"/*_test.sh; do
	[ -f "$t" ] || continue
	name=`basename "$t"`
	case "$name" in
		*"$filter"*) ;;
		*) continue ;;
	esac
	out=`sh "$t" 2>&1`
	rc=$?
	if [ $rc -eq 0 ]; then
		passed=`expr $passed + 1`
		printf 'PASS %s\n' "$name"
	else
		failed=`expr $failed + 1`
		failed_names="$failed_names $name"
		printf 'FAIL %s (exit %s)\n' "$name" "$rc"
	fi
	printf '%s\n' "$out" | sed 's/^/    /'
done

printf '%s test script(s): %s passed, %s failed\n' "`expr $passed + $failed`" "$passed" "$failed"
if [ $failed -ne 0 ]; then
	printf 'failed:%s\n' "$failed_names"
	exit 1
fi
exit 0
