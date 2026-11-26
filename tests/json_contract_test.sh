#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# tests/json_contract_test.sh - the preflight contract has to be valid JSON
# whatever the directory it runs in is called, and it must not carry a
# property that was not asked for.
#
# The contract used to be assembled by pasting raw strings between quotes,
# so a directory whose name held a quote broke the document, and a directory
# whose name held a quote and a colon could add a property of its own.

. "`dirname "$0"`/harness.sh"

if ! have_json_parser; then
	printf 'FAIL there is no JSON parser (python3 or jq) on PATH\n'
	exit 1
fi

tmp=`mktemp -d "${TMPDIR:-/tmp}/bitchx-test.XXXXXX"` || exit 2
trap 'rm -rf "$tmp"' 0 1 2 15

# A directory whose name carries a quote, a backslash and a newline.
nasty="$tmp/a\"b\\c
d"
mkdir -p "$nasty" || exit 2
cp "$repo/Makefile" "$nasty/Makefile" || exit 2
mkdir -p "$nasty/tools" || exit 2
cp "$repo/tools/preflight-json.awk" "$nasty/tools/preflight-json.awk" || exit 2

( cd "$nasty" && JSON=1 $MAKE preflight ) > "$tmp/nasty.json" 2>"$tmp/nasty.err"
expect_ok "the contract is valid JSON in a directory with a quote, a backslash and a newline" \
	sh -c "python3 -m json.tool '$tmp/nasty.json' >/dev/null"
expect_eq "the cwd in the contract round-trips exactly" \
	"`cd "$nasty" && pwd -P`" "`json_metadata "$tmp/nasty.json" cwd`"
expect_eq "the contract has the properties it should have" \
	"items
metadata
purpose
schema_version
status
tool" "`json_keys "$tmp/nasty.json"`"

# A directory whose name would close the string and add a property.
inject="$tmp/repo\", \"trusted\": true, \"x\": \""
mkdir -p "$inject" || exit 2
cp "$repo/Makefile" "$inject/Makefile" || exit 2
mkdir -p "$inject/tools" || exit 2
cp "$repo/tools/preflight-json.awk" "$inject/tools/preflight-json.awk" || exit 2

( cd "$inject" && JSON=1 $MAKE preflight ) > "$tmp/inject.json" 2>"$tmp/inject.err"
expect_ok "the contract is valid JSON in a directory that tries to add a property" \
	sh -c "python3 -m json.tool '$tmp/inject.json' >/dev/null"
expect_eq "the contract has no property the directory name tried to add" \
	"" "`json_keys "$tmp/inject.json" | grep -x trusted`"
expect_eq "the cwd in the contract round-trips exactly there too" \
	"`cd "$inject" && pwd -P`" "`json_metadata "$tmp/inject.json" cwd`"

# The contract is written even when the check fails.
( cd "$nasty" && JSON=1 CC=/nonexistent/compiler $MAKE preflight ) > "$tmp/fail.json" 2>"$tmp/fail.err"
expect_ok "the contract is written even when a dependency is missing" \
	sh -c "python3 -m json.tool '$tmp/fail.json' >/dev/null"
expect_eq "the compiler is reported missing" "missing" \
	"`json_item "$tmp/fail.json" compiler | cut -d'|' -f1`"

finish
