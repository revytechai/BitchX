#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# tests/preflight_build_agreement_test.sh - preflight and build have to
# agree about whether autoconf is needed.
#
# The build reused a configure script that was already there, while the
# preflight always asked for autoconf, so a tree that had been built once
# was reported as missing a dependency it did not need.

. "`dirname "$0"`/harness.sh"

tmp=`mktemp -d "${TMPDIR:-/tmp}/bitchx-test.XXXXXX"` || exit 2
trap 'rm -rf "$tmp"' 0 1 2 15

src="$tmp/src"
build="$tmp/build"
mkdir -p "$src" || exit 2
cp "$repo/Makefile" "$src/Makefile" || exit 2
mkdir -p "$src/tools" || exit 2
cp "$repo/tools/preflight-json.awk" "$src/tools/preflight-json.awk" || exit 2
printf 'AC_INIT\n' > "$src/configure.in"
stub_autoconf "$tmp/bin" || exit 2

# A tree that has never been configured needs autoconf.
( cd "$src" && PATH="/usr/bin:/bin" JSON=1 $MAKE preflight ) > "$tmp/fresh.json" 2>/dev/null
expect_eq "a tree without a configure script says autoconf is missing" \
	"missing" "`json_item "$tmp/fresh.json" autoconf | cut -d'|' -f1`"

# After a build the configure script is there, and autoconf is not needed.
( cd "$src" && PATH="$tmp/bin:$PATH" BUILD_DIR="$build" $MAKE build ) > "$tmp/out" 2>&1
( cd "$src" && PATH="/usr/bin:/bin" JSON=1 $MAKE preflight ) > "$tmp/after.json" 2>/dev/null
expect_eq "after a build autoconf is not needed" \
	"ok" "`json_item "$tmp/after.json" autoconf | cut -d'|' -f1`"
expect_ok "the preflight passes once the configure script is there" \
	sh -c "cd '$src' && PATH='/usr/bin:/bin' $MAKE preflight"

# And the build reuses the script when autoconf is not there at all.
expect_ok "the build reuses the configure script without autoconf" \
	sh -c "cd '$src' && PATH='/usr/bin:/bin' BUILD_DIR='$build' $MAKE build"
expect_eq "the configure script was not generated again" \
	"1" "`wc -l < "$build/srcdir/autoconf.runs" 2>/dev/null | tr -d ' '`"

finish
