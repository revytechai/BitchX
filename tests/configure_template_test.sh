#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# tests/configure_template_test.sh - the build has to pick the Autoconf
# template it is given and use that same file for the freshness check.
#
# A tree that ships configure.ac and no configure.in has to be configured
# from configure.ac. The build used to run "autoconf configure.in" whatever
# the tree held, so it failed on a tree that only had configure.ac, and its
# freshness check looked at the wrong file.

. "`dirname "$0"`/harness.sh"

tmp=`mktemp -d "${TMPDIR:-/tmp}/bitchx-test.XXXXXX"` || exit 2
trap 'rm -rf "$tmp"' 0 1 2 15

src="$tmp/src"
build="$tmp/build"
mkdir -p "$src" || exit 2
cp "$repo/Makefile" "$src/Makefile" || exit 2
mkdir -p "$src/tools" || exit 2
cp "$repo/tools/preflight-json.awk" "$src/tools/preflight-json.awk" || exit 2
stub_autoconf "$tmp/bin" || exit 2

# A tree with configure.ac only.
printf 'AC_INIT\n' > "$src/configure.ac"
( cd "$src" && PATH="$tmp/bin:$PATH" BUILD_DIR="$build" $MAKE build ) > "$tmp/out" 2>&1
expect_eq "a tree with only configure.ac is configured from configure.ac" \
	"configure.ac" "`cat "$build/srcdir/autoconf.template" 2>/dev/null`"

# A tree with configure.in only.
rm -rf "$build"
rm -f "$src/configure.ac"
printf 'AC_INIT\n' > "$src/configure.in"
( cd "$src" && PATH="$tmp/bin:$PATH" BUILD_DIR="$build" $MAKE build ) > "$tmp/out" 2>&1
expect_eq "a tree with only configure.in is configured from configure.in" \
	"configure.in" "`cat "$build/srcdir/autoconf.template" 2>/dev/null`"

# A tree with both: configure.ac is the one to use.
rm -rf "$build"
printf 'AC_INIT\n' > "$src/configure.ac"
( cd "$src" && PATH="$tmp/bin:$PATH" BUILD_DIR="$build" $MAKE build ) > "$tmp/out" 2>&1
expect_eq "a tree with both templates is configured from configure.ac" \
	"configure.ac" "`cat "$build/srcdir/autoconf.template" 2>/dev/null`"

# The freshness check has to use the same file as the generation. Touching
# configure.in alone leaves the configure script current, because
# configure.ac is the template; touching configure.ac makes it stale.
rm -rf "$build"
( cd "$src" && PATH="$tmp/bin:$PATH" BUILD_DIR="$build" $MAKE build ) > "$tmp/out" 2>&1
sleep 1
touch "$src/configure.in"
( cd "$src" && PATH="$tmp/bin:$PATH" BUILD_DIR="$build" $MAKE build ) > "$tmp/out" 2>&1
expect_eq "a configure.in that is newer than the script does not make it stale" \
	"1" "`wc -l < "$build/srcdir/autoconf.runs" 2>/dev/null | tr -d ' '`"

sleep 1
touch "$src/configure.ac"
( cd "$src" && PATH="$tmp/bin:$PATH" BUILD_DIR="$build" $MAKE build ) > "$tmp/out" 2>&1
expect_eq "a configure.ac that is newer than the script makes it stale" \
	"2" "`wc -l < "$build/srcdir/autoconf.runs" 2>/dev/null | tr -d ' '`"

finish
