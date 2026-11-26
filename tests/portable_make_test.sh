#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# tests/portable_make_test.sh - the Makefile has to work under every make
# that is on the machine, with the same target names.
#
# The Makefile is a fork wrapper: it is meant to be used on FreeBSD, where
# make is bmake, and on Linux, where make is GNU make, so it may not use a
# construct that only one of them understands.

. "`dirname "$0"`/harness.sh"

tmp=`mktemp -d "${TMPDIR:-/tmp}/bitchx-test.XXXXXX"` || exit 2
trap 'rm -rf "$tmp"' 0 1 2 15

src="$tmp/src"
mkdir -p "$src" || exit 2
cp "$repo/Makefile" "$src/Makefile" || exit 2
mkdir -p "$src/tools" || exit 2
cp "$repo/tools/preflight-json.awk" "$src/tools/preflight-json.awk" || exit 2
printf 'AC_INIT\n' > "$src/configure.in"

seen=""
for m in gmake bmake make; do
	path=`command -v "$m" 2>/dev/null` || continue
	case "$seen" in
		*" $path "*) continue ;;
	esac
	seen="$seen $path "
	expect_eq "$m prints the purpose first" \
		"purpose: fork" "`cd "$src" && $m help 2>&1 | grep -v 'Entering directory' | sed -n '1p'`"
	expect_eq "$m prints the purpose on its own" \
		"fork" "`cd "$src" && $m purpose 2>&1 | grep -v 'Entering directory' | sed -n '1p'`"
	expect_ok "$m runs the preflight" sh -c "cd '$src' && $m preflight"
	expect_ok "$m writes the JSON contract" sh -c "cd '$src' && JSON=1 $m preflight"
	expect_fail "$m refuses a build directory that is this tree" \
		sh -c "cd '$src' && BUILD_DIR=. $m build"
done

finish
