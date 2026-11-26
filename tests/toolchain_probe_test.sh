#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# tests/toolchain_probe_test.sh - the preflight has to probe the toolchain
# the build would use: the compiler in CC, the include directories in
# CPPFLAGS, and the ncurses library behind the ncurses header.
#
# The preflight used to look for a command called cc or gcc on PATH and for
# a header in four fixed directories, so it reported a toolchain that
# configure would have used as missing, and a header without a library
# behind it as complete.

. "`dirname "$0"`/harness.sh"

tmp=`mktemp -d "${TMPDIR:-/tmp}/bitchx-test.XXXXXX"` || exit 2
trap 'rm -rf "$tmp"' 0 1 2 15

src="$tmp/src"
mkdir -p "$src" || exit 2
cp "$repo/Makefile" "$src/Makefile" || exit 2
mkdir -p "$src/tools" || exit 2
cp "$repo/tools/preflight-json.awk" "$src/tools/preflight-json.awk" || exit 2
printf 'AC_INIT\n' > "$src/configure.in"

inc="$tmp/include"
mkdir -p "$inc" || exit 2
printf '#ifndef NCURSES_H\n#define NCURSES_H\nextern int initscr(void);\n#endif\n' > "$inc/ncurses.h"

stub_compiler "$tmp/withinc" with-include "$inc" || exit 2
stub_compiler "$tmp/always" always || exit 2
stub_compiler "$tmp/nolink" no-link || exit 2
stub_compiler "$tmp/broken" broken || exit 2

# The compiler and the header are found when the toolchain is given the
# include directory it needs.
( cd "$src" && CC="$tmp/withinc/cc" CPPFLAGS="-I$inc" JSON=1 $MAKE preflight ) > "$tmp/withinc.json" 2>/dev/null
expect_eq "a compiler that needs the include directory it was given is found" \
	"ok" "`json_item "$tmp/withinc.json" compiler | cut -d'|' -f1`"
expect_eq "the ncurses header in that include directory is found" \
	"ok" "`json_item "$tmp/withinc.json" ncurses | cut -d'|' -f1`"

# The same toolchain without the include directory: the compiler cannot work.
( cd "$src" && CC="$tmp/withinc/cc" JSON=1 $MAKE preflight ) > "$tmp/without.json" 2>/dev/null
expect_eq "a compiler that cannot work without its include directory is missing" \
	"missing" "`json_item "$tmp/without.json" compiler | cut -d'|' -f1`"

# A compiler that fails at everything is missing, not present.
( cd "$src" && CC="$tmp/broken/cc" JSON=1 $MAKE preflight ) > "$tmp/broken.json" 2>/dev/null
expect_eq "a compiler that fails at everything is reported missing" \
	"missing" "`json_item "$tmp/broken.json" compiler | cut -d'|' -f1`"
expect_contains "the reason says the compiler could not compile" \
	"`json_item "$tmp/broken.json" compiler`" "cannot compile"

# A header without a library behind it is not a complete ncurses.
( cd "$src" && CC="$tmp/nolink/cc" JSON=1 $MAKE preflight ) > "$tmp/nolink.json" 2>/dev/null
expect_eq "a ncurses header without a library to link is reported missing" \
	"missing" "`json_item "$tmp/nolink.json" ncurses | cut -d'|' -f1`"
expect_contains "the reason says there is no library to link" \
	"`json_item "$tmp/nolink.json" ncurses`" "no ncurses library"

# Without CC the compiler is looked for on PATH.
( cd "$src" && PATH="$tmp/always:/usr/bin:/bin" JSON=1 $MAKE preflight ) > "$tmp/path.json" 2>/dev/null
expect_eq "a compiler on PATH is found" \
	"ok" "`json_item "$tmp/path.json" compiler | cut -d'|' -f1`"

finish
