#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# tests/injection_test.sh - the switches are data, not shell syntax.
#
# Make expands a variable into the shell source before the shell parses it,
# so a value that holds a semicolon or a quote could run a command. The
# switches are read from the environment now, so they stay data all the way
# to the program that receives them.

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

sentinel="$src/pwn"

# CONFIGURE_OPTIONS with a command in it.
rm -f "$sentinel"
expect_fail "a CONFIGURE_OPTIONS with a command in it fails the build" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='$build' CONFIGURE_OPTIONS='--with-plugins; touch pwn' $MAKE build"
expect_eq "the command in CONFIGURE_OPTIONS did not run" \
	"no" "`[ -e "$sentinel" ] && echo yes || echo no`"

# BUILD_DIR with a command in it.
rm -f "$sentinel"
expect_fail "a BUILD_DIR with a command in it fails the build" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='x\"; touch pwn; mkdir -p x; : \"' $MAKE build"
expect_eq "the command in BUILD_DIR did not run" \
	"no" "`[ -e "$sentinel" ] && echo yes || echo no`"

# AUTOCONF with a command in it. The build directory is empty, so autoconf
# has to run.
rm -f "$sentinel"
rm -rf "$build"
expect_fail "an AUTOCONF with a command in it fails the build" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='$build' AUTOCONF='autoconf; touch pwn; : ' $MAKE build"
expect_eq "the command in AUTOCONF did not run" \
	"no" "`[ -e "$sentinel" ] && echo yes || echo no`"

# PREFIX with a command in it. A prefix is a path, so a strange one is
# passed on as data: what matters is that it is not run as a command.
rm -f "$sentinel"
expect_ok "a PREFIX with a command in it is passed on as data" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='$build' PREFIX='/opt/bitchx\"; touch pwn; : \"' $MAKE build"
expect_eq "the command in PREFIX did not run" \
	"no" "`[ -e "$sentinel" ] && echo yes || echo no`"
expect_contains "the prefix was passed on as one argument" \
	"`cat "$build/configure.args" 2>/dev/null`" "--prefix=/opt/bitchx\"; touch pwn; : \""

# A legitimate build directory and prefix, with spaces in them, are passed on
# exactly as they were given.
rm -rf "$build"
spacey="$tmp/with space"
expect_ok "a build directory with a space in it works" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='$spacey' PREFIX='/opt/bitch x' $MAKE build"
expect_eq "the prefix and the options were passed on exactly" \
	"--srcdir=$spacey/srcdir --prefix=/opt/bitch x --with-plugins" \
	"`cat "$spacey/configure.args" 2>/dev/null`"
expect_eq "the build ran in the build directory" \
	"`cd "$spacey" && pwd -P`" "`cat "$spacey/configure.cwd" 2>/dev/null`"

finish
