# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# BitchX - root Makefile for the revytechai/BitchX fork.
#
# This is a thin fork wrapper: it follows upstream's build and adds nothing of
# its own. Upstream (BitchX/BitchX1.3) builds with autoconf/automake, so
# UPSTREAM_BUILD is autoconf and every target below only drives upstream's own
# build system.
#
# Portability (GNU make and FreeBSD/OpenBSD bmake):
#   - recipe lines start with a real tab; no .RECIPEPREFIX;
#   - no .ONESHELL: one shell per recipe line, chained with && or a backslash;
#   - no ifeq/.if: the OS is detected inside the recipe with uname and a case;
#   - plain = assignments, $(VAR), $$ for the shell;
#   - no include/.include, no GNU functions, no bmake modifiers, no $^ or $>;
#   - help is the first target (.DEFAULT_GOAL is GNU-only).
# See skill project-makefile-contract.
#
# The switches are read from the ENVIRONMENT, never spliced into the shell by
# make, so a value that carries a quote, a semicolon or a newline stays data
# all the way to the program that receives it:
#
#   BUILD_DIR=/somewhere make build      (or: make build BUILD_DIR=/somewhere)
#   PREFIX=/opt/bitchx make build
#   CONFIGURE_OPTIONS="--with-plugins" make build
#   AUTOCONF=/opt/tools/bin/autoconf make build
#   JSON=1 make preflight
#
# The values make itself has to splice (REPO_PURPOSE, the two constants that
# describe upstream and the one rule below) are written inside single quotes,
# so a quote or a semicolon in them cannot become shell syntax.

REPO_PURPOSE = fork

# A fork follows upstream's build: upstream BitchX 1.3 uses autoconf.
UPSTREAM_BUILD = autoconf
UPSTREAM_URL = https://github.com/BitchX/BitchX1.3.git

# BitchX 1.3 ships no test suite of its own. The tests of this wrapper live
# in tests/ and run with "make test"; the recipe runs that path literally, so
# that no value is ever spliced into the shell.
TEST = tests/run.sh

# The one rule that decides whether the configure script can be reused. Both
# preflight and build use it, so the two cannot drift apart: the script is
# current when it exists and is not older than the template it came from.
# INTERNAL: not a switch; do not override it.
CONFIGURE_REUSABLE = test -f "$${CONFIGURE_SCRIPT}" && test "$${CONFIGURE_SCRIPT}" -nt "$${CONFIGURE_TEMPLATE}"

# help is the first target, so a bare "make" prints usage on both makes.
help:
	@printf '%s\n' \
		'purpose: $(REPO_PURPOSE)' \
		"" \
		"BitchX - thin fork Makefile; it follows upstream's build." \
		"  upstream build system: '$(UPSTREAM_BUILD)'  ('$(UPSTREAM_URL)')" \
		"" \
		"Targets (same names on every OS):" \
		"  make help          this message (default)" \
		"  make purpose       print REPO_PURPOSE (for machines)" \
		"  make preflight     report the missing build dependencies (JSON=1 for the --json contract)" \
		"  make build         generate configure, configure and build out of tree, in BUILD_DIR" \
		"  make test          run the tests of this wrapper (tests/run.sh)" \
		"" \
		"Switches (read from the environment; make never splices them into the shell):" \
		"  BUILD_DIR=$${BUILD_DIR:-../build}   out-of-tree build directory; it may not be this tree or a directory in it" \
		"  PREFIX=$${PREFIX:-/usr/local}        prefix configure is asked for" \
		"  CONFIGURE_OPTIONS=$${CONFIGURE_OPTIONS:---with-plugins}  options configure is asked for" \
		"  AUTOCONF=$${AUTOCONF:-autoconf}       the autoconf program (a path, not a command line)" \
		"  JSON=$${JSON:-0}                      1 makes preflight write the --json contract" \
		"" \
		"Note: the build runs in BUILD_DIR and stages the Autotools inputs in a mirror" \
		"of this tree there, so this tree is never written to and may be read-only." \
		"Note: upstream BitchX 1.3 has no test suite of its own; the tests here cover this wrapper." \
		"Run make from the directory that holds this Makefile."

.PHONY: help purpose preflight build test

purpose:
	@printf '%s\n' '$(REPO_PURPOSE)'

# preflight reports what the build needs: autoconf (only when the configure
# script has to be regenerated), a C compiler and the ncurses header and
# library the client needs. It probes the compiler it finds - CC first - with
# a real compile and a real link, so it reports what configure would find and
# not what a list of directories says.
#
# JSON=1 writes the --json contract: metadata, an indexed status array and
# the purpose. The report says what is missing; the contract says the same
# in a form a machine can read.
#
# The human report exits 0 when everything is present and 1 when something is
# missing. The contract always exits 0: a make that fails writes a message
# of its own into the same stream (bmake writes it to stdout), and a
# document that a machine cannot parse is no contract at all. The state of
# every dependency is in the document.
preflight:
	@set -f; \
	srcdir=`pwd -P`; \
	os=`uname -s`; \
	arch=`uname -m`; \
	builddir=$${BUILD_DIR:-../build}; \
	case "$$builddir" in /*) ;; *) builddir="$$srcdir/$$builddir";; esac; \
	template=configure.ac; \
	[ -f "$$srcdir/$$template" ] || template=configure.in; \
	script="$$builddir/srcdir/configure"; \
	CONFIGURE_SCRIPT="$$script"; CONFIGURE_TEMPLATE="$$srcdir/$$template"; \
	reusable=0; \
	if $(CONFIGURE_REUSABLE); then reusable=1; fi; \
	autoconf_state=missing; \
	autoconf_info=""; \
	if [ "$$reusable" -eq 1 ]; then \
		autoconf_state=ok; \
		autoconf_info="the configure script in $$script is current, so autoconf is not needed"; \
	elif command -v "$${AUTOCONF:-autoconf}" >/dev/null 2>&1; then \
		autoconf_state=ok; \
		autoconf_info=`command -v "$${AUTOCONF:-autoconf}"`; \
	else \
		autoconf_info="$${AUTOCONF:-autoconf} is not on PATH and there is no current configure script in $$script"; \
	fi; \
	cc=$${CC:-}; \
	if [ -z "$$cc" ]; then \
		for candidate in cc gcc clang; do \
			if command -v "$$candidate" >/dev/null 2>&1; then cc=`command -v "$$candidate"`; break; fi; \
		done; \
	fi; \
	compiler_state=missing; \
	compiler_info=""; \
	tmp=`mktemp -d "$${TMPDIR:-/tmp}/bitchx-preflight.XXXXXX" 2>/dev/null` || tmp=""; \
	if [ -n "$$tmp" ]; then printf '%s\n' 'int main(void) { return 0; }' > "$$tmp/probe.c"; fi; \
	if [ -n "$$cc" ] && [ -n "$$tmp" ]; then \
		if "$$cc" $$CPPFLAGS $$CFLAGS -o "$$tmp/probe" "$$tmp/probe.c" >/dev/null 2>&1; then \
			compiler_state=ok; \
			compiler_info="$$cc"; \
		else \
			compiler_info="$$cc cannot compile a test program"; \
		fi; \
	elif [ -n "$$cc" ]; then \
		compiler_info="$$cc was found, but no scratch directory is available to probe it"; \
	else \
		compiler_info="no C compiler (cc, gcc or clang) on PATH and CC is not set"; \
	fi; \
	ncurses_state=missing; \
	ncurses_info=""; \
	if [ -n "$$tmp" ]; then printf '%s\n' '#include <ncurses.h>' 'int main(void) { return initscr() == 0; }' > "$$tmp/ncurses.c"; fi; \
	if [ "$$compiler_state" = ok ] && [ -n "$$tmp" ]; then \
		if "$$cc" $$CPPFLAGS $$CFLAGS -c -o "$$tmp/ncurses.o" "$$tmp/ncurses.c" >/dev/null 2>&1; then \
			for lib in ncurses ncursesw curses tinfo; do \
				if "$$cc" $$CPPFLAGS $$CFLAGS -o "$$tmp/ncurses" "$$tmp/ncurses.c" $$LDFLAGS -l$$lib >/dev/null 2>&1; then \
					ncurses_state=ok; \
					ncurses_info="-l$$lib"; \
					break; \
				fi; \
			done; \
			if [ "$$ncurses_state" != ok ]; then \
				ncurses_info="ncurses.h was found, but there is no ncurses library to link (-lncurses, -lncursesw, -lcurses, -ltinfo)"; \
			fi; \
		else \
			ncurses_info="no ncurses.h was found by $$cc $$CPPFLAGS $$CFLAGS"; \
		fi; \
	else \
		ncurses_info="there is no usable C compiler to probe ncurses with"; \
	fi; \
	if [ -n "$$tmp" ]; then rm -rf "$$tmp"; fi; \
	missing=0; \
	[ "$$autoconf_state" = ok ] || missing=`expr $$missing + 1`; \
	[ "$$compiler_state" = ok ] || missing=`expr $$missing + 1`; \
	[ "$$ncurses_state" = ok ] || missing=`expr $$missing + 1`; \
	if [ "$${JSON}" = 1 ]; then \
		PREFLIGHT_ITEMS="autoconf compiler ncurses" \
		PREFLIGHT_autoconf_STATE="$$autoconf_state" \
		PREFLIGHT_autoconf_INFO="$$autoconf_info" \
		PREFLIGHT_compiler_STATE="$$compiler_state" \
		PREFLIGHT_compiler_INFO="$$compiler_info" \
		PREFLIGHT_ncurses_STATE="$$ncurses_state" \
		PREFLIGHT_ncurses_INFO="$$ncurses_info" \
		PREFLIGHT_OS="$$os" \
		PREFLIGHT_ARCH="$$arch" \
		PREFLIGHT_CWD="$$srcdir" \
		LC_ALL=C awk -f tools/preflight-json.awk -- --purpose '$(REPO_PURPOSE)' || exit 1; \
		exit 0; \
	else \
		printf 'preflight: purpose=%s os=%s arch=%s\n' '$(REPO_PURPOSE)' "$$os" "$$arch"; \
		if [ "$$autoconf_state" = ok ]; then printf 'ok    autoconf (%s)\n' "$$autoconf_info"; else printf 'miss  autoconf (%s)\n' "$$autoconf_info"; fi; \
		if [ "$$compiler_state" = ok ]; then printf 'ok    compiler (%s)\n' "$$compiler_info"; else printf 'miss  compiler (%s)\n' "$$compiler_info"; fi; \
		if [ "$$ncurses_state" = ok ]; then printf 'ok    ncurses (%s)\n' "$$ncurses_info"; else printf 'miss  ncurses (%s)\n' "$$ncurses_info"; fi; \
		if [ "$$missing" -eq 0 ]; then exit 0; fi; \
		exit 1; \
	fi

# build generates the configure script with autoconf, then configures and
# builds out of tree in BUILD_DIR.
#
# autoconf insists that the configure script sits in the directory it is given
# as --srcdir: its "build environment is sane" check compares the timestamps
# of $srcdir/configure and of a file it has just created, and it refuses to
# run when $srcdir/configure is not there. The Autoconf inputs are therefore
# staged in a mirror of this tree inside BUILD_DIR - every file in the mirror
# is a link back into this tree - and configure is run with --srcdir pointing
# at the mirror. This tree is then never written to, and it may be read-only.
#
# BUILD_DIR may not be this tree or a directory in it: it is resolved
# physically first, and the build stops before configure runs if it lands
# inside the source tree.
build:
	@set -f; \
	srcdir=`pwd -P`; \
	builddir=$${BUILD_DIR:-../build}; \
	case "$$builddir" in /*) ;; *) builddir="$$srcdir/$$builddir";; esac; \
	parent=`dirname "$$builddir"`; \
	[ -d "$$parent" ] || mkdir -p "$$parent" || exit 1; \
	parent=`cd "$$parent" && pwd -P`; \
	builddir="$$parent/`basename "$$builddir"`"; \
	case "$$builddir" in "$$srcdir"|"$$srcdir"/*) printf 'build: BUILD_DIR (%s) is this source tree or a directory in it\n' "$$builddir"; exit 1;; esac; \
	mkdir -p "$$builddir" || exit 1; \
	physical=`cd "$$builddir" && pwd -P`; \
	case "$$physical" in "$$srcdir"|"$$srcdir"/*) printf 'build: BUILD_DIR resolves to %s, which is this source tree or a directory in it\n' "$$physical"; exit 1;; esac; \
	template=configure.ac; \
	[ -f "$$srcdir/$$template" ] || template=configure.in; \
	if [ ! -f "$$srcdir/$$template" ]; then printf 'build: no configure.ac or configure.in in %s\n' "$$srcdir"; exit 1; fi; \
	mirror="$$builddir/srcdir"; \
	mkdir -p "$$mirror" || exit 1; \
	( cd "$$srcdir" && find . -type d ! -name .git -exec mkdir -p "$$mirror/{}" \; ) || exit 1; \
	( cd "$$srcdir" && find . -type f ! -name .git -exec sh -c 'm=$$1; s=$$2; shift 2; for f do ln -sf "$$s/$$f" "$$m/$$f"; done' x "$$mirror" "$$srcdir" {} + ) || exit 1; \
	script="$$mirror/configure"; \
	CONFIGURE_SCRIPT="$$script"; CONFIGURE_TEMPLATE="$$srcdir/$$template"; \
	if $(CONFIGURE_REUSABLE); then \
		printf 'build: reusing the configure script in %s\n' "$$script"; \
	else \
		printf 'build: generating configure with %s\n' "$${AUTOCONF:-autoconf}"; \
		( cd "$$mirror" && "$${AUTOCONF:-autoconf}" -o configure "$$template" ) || exit 1; \
	fi; \
	prefix=$${PREFIX:-/usr/local}; \
	opts=$${CONFIGURE_OPTIONS:---with-plugins}; \
	leftover=`printf '%s' "$$opts" | LC_ALL=C tr -d 'A-Za-z0-9_+=:.,/ -'`; \
	if [ -n "$$leftover" ]; then printf 'build: CONFIGURE_OPTIONS may only hold letters, digits, spaces and _+=:.,/-\n'; exit 1; fi; \
	set -- $$opts; \
	( cd "$$builddir" && "$$script" --srcdir="$$mirror" --prefix="$$prefix" "$$@" ) || exit 1; \
	( cd "$$builddir" && $(MAKE) )

# test runs the tests of this wrapper.
test:
	@sh tests/run.sh
