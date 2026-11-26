#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# tests/out_of_tree_test.sh - the build has to happen outside this tree, and
# it has to refuse a build directory that is this tree or a directory in it.
#
# The build used to write the generated configure script and Autoconf's cache
# into the source tree, and it took a build directory as it was given: a
# build directory that was the source tree, or a link to it, let the
# generated Makefile land on this tree's own Makefile.

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

# The tree before and after the build.
( cd "$src" && find . -type f | sort ) > "$tmp/before"
( cd "$src" && PATH="$tmp/bin:$PATH" BUILD_DIR="$build" $MAKE build ) > "$tmp/out" 2>&1
( cd "$src" && find . -type f | sort ) > "$tmp/after"
expect_eq "the build leaves this tree alone" "`cat "$tmp/before"`" "`cat "$tmp/after"`"
expect_eq "the build wrote its configure script into the build directory" \
	"yes" "`[ -f "$build/srcdir/configure" ] && echo yes || echo no`"

# The build works when this tree is read-only.
chmod -R a-w "$src" || exit 2
rm -rf "$build"
expect_ok "the build works when this tree is read-only" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='$build' $MAKE build"
chmod -R u+w "$src" || exit 2

# A build directory that is this tree is refused, and this tree's Makefile
# is left alone.
rm -rf "$build"
expect_fail "a build directory that is this tree is refused" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='.' $MAKE build"
expect_eq "this tree's Makefile is untouched" "`cat "$repo/Makefile"`" "`cat "$src/Makefile"`"

# A build directory inside this tree is refused.
expect_fail "a build directory inside this tree is refused" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='sub' $MAKE build"

# A build directory that is a link to this tree is refused.
ln -s "$src" "$tmp/link"
expect_fail "a build directory that links to this tree is refused" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='$tmp/link' $MAKE build"

# A build directory beside the tree works.
rm -rf "$build"
expect_ok "a build directory beside the tree works" \
	sh -c "cd '$src' && PATH='$tmp/bin:$PATH' BUILD_DIR='$build' $MAKE build"
expect_eq "the build ran in the build directory" \
	"`cd "$build" && pwd -P`" "`cat "$build/configure.cwd" 2>/dev/null`"

finish
