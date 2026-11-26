#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# tests/harness.sh - the tiny harness the tests of this wrapper share.
#
# It is sourced, not executed; the shebang is here because the shell checker
# asks every shell file for one. Every check prints one line, and the script
# that sourced it ends with finish, which exits non-zero when a check failed.
#
#   ok "what was checked"
#   bad "what was checked"
#   expect_eq "what was checked" "expected" "actual"
#   expect_contains "what was checked" "haystack" "needle"
#   expect_ok "what was checked" command...
#   expect_fail "what was checked" command...
#   finish

checks=0
failed=0

ok() {
	checks=`expr $checks + 1`
	printf 'ok   %s\n' "$1"
}

bad() {
	checks=`expr $checks + 1`
	failed=`expr $failed + 1`
	printf 'FAIL %s\n' "$1"
}

# expect_eq description expected actual
expect_eq() {
	if [ "$2" = "$3" ]; then
		ok "$1"
	else
		bad "$1 (expected [$2], got [$3])"
	fi
}

# expect_contains description haystack needle
expect_contains() {
	case "$2" in
		*"$3"*) ok "$1" ;;
		*) bad "$1 ([$3] is not in [$2])" ;;
	esac
}

# expect_ok description command...
expect_ok() {
	ex_desc=$1
	shift
	if "$@" >/dev/null 2>&1; then
		ok "$ex_desc"
	else
		bad "$ex_desc (the command failed: $*)"
	fi
}

# expect_fail description command...
expect_fail() {
	ex_desc=$1
	shift
	if "$@" >/dev/null 2>&1; then
		bad "$ex_desc (the command was expected to fail)"
	else
		ok "$ex_desc"
	fi
}

# The repository these tests belong to.
here=`dirname "$0"`
repo=`cd "$here/.." && pwd -P`

# The make these tests drive. The same target names work under both makes.
MAKE=${MAKE:-make}

# A scratch fixture: a copy of this repository's Makefile and of the tool it
# needs, in a directory of its own, so a test can run make there.
fixture() {
	fx_dir=`mktemp -d "${TMPDIR:-/tmp}/bitchx-test.XXXXXX"` || return 1
	cp "$repo/Makefile" "$fx_dir/Makefile" || return 1
	mkdir -p "$fx_dir/tools" || return 1
	cp "$repo/tools/preflight-json.awk" "$fx_dir/tools/preflight-json.awk" || return 1
	printf '%s\n' "$fx_dir"
}

# A stub autoconf: it records the template it was given and the file it was
# told to write, and it writes a stub configure that records how it was
# called. It stands in for the real autoconf so that the tests do not need
# one and stay fast.
stub_autoconf() {
	sa_dir=$1
	mkdir -p "$sa_dir" || return 1
	cat > "$sa_dir/autoconf" <<'STUB'
#!/bin/sh
# A stub autoconf: record the template and the output, write a configure.
out=""
in=""
next=0
for a in "$@"; do
	if [ "$next" = 1 ]; then
		out=$a
		next=0
		continue
	fi
	case $a in
	-o) next=1 ;;
	*) in=$a ;;
	esac
done
printf '%s\n' "$in" > autoconf.template
printf '%s\n' "$out" > autoconf.output
printf 'run\n' >> autoconf.runs
cat > "$out" <<'CONFIGURE'
#!/bin/sh
# A stub configure: record the arguments and the working directory, and leave
# a Makefile behind so that the build has something to run.
printf '%s\n' "$*" > configure.args
pwd -P > configure.cwd
printf 'all:\n\t@echo stub build\n' > Makefile
exit 0
CONFIGURE
chmod +x "$out"
exit 0
STUB
	chmod +x "$sa_dir/autoconf"
}

# A stub compiler. The first form only compiles when the given include
# directory is on the include path; the second compiles but never links a
# program that asks for a library; the third fails at everything.
stub_compiler() {
	sc_dir=$1
	sc_kind=$2
	sc_inc=$3
	mkdir -p "$sc_dir" || return 1
	case $sc_kind in
	with-include)
		printf '%s\n' "$sc_inc" > "$sc_dir/include"
		cat > "$sc_dir/cc" <<'STUB'
#!/bin/sh
# A stub compiler: it only works when the include directory it was built for
# is on the include path.
inc=$(cat "$(dirname "$0")/include" 2>/dev/null)
case "$*" in
	*-I$inc*) exit 0 ;;
esac
exit 1
STUB
		;;
	always)
		cat > "$sc_dir/cc" <<'STUB'
#!/bin/sh
# A stub compiler: it works at everything.
exit 0
STUB
		;;
	no-link)
		cat > "$sc_dir/cc" <<'STUB'
#!/bin/sh
# A stub compiler: it compiles, but it never links a program that asks for a
# library.
for a in "$@"; do
	case $a in
	-c) exit 0 ;;
	*ncurses*) exit 1 ;;
	esac
done
exit 0
STUB
		;;
	*)
		cat > "$sc_dir/cc" <<'STUB'
#!/bin/sh
# A stub compiler: it fails at everything.
exit 1
STUB
		;;
	esac
	chmod +x "$sc_dir/cc"
}

# The state of one item of the preflight contract, read out of the JSON.
# json_item file name  ->  "state|reason"
json_item() {
	python3 - "$1" "$2" <<'PY' 2>/dev/null
import json, sys
doc = json.load(open(sys.argv[1]))
for item in doc["status"]:
    if item["name"] == sys.argv[2]:
        print("%s|%s" % (item["state"], item.get("reason", "")))
PY
}

# The metadata of the preflight contract.
json_metadata() {
	python3 - "$1" "$2" <<'PY' 2>/dev/null
import json, sys
print(json.load(open(sys.argv[1]))["metadata"][sys.argv[2]])
PY
}

# The property names of the preflight contract, one per line.
json_keys() {
	python3 - "$1" <<'PY' 2>/dev/null
import json, sys
doc = json.load(open(sys.argv[1]))
for key in sorted(doc.keys()):
    print(key)
PY
}

# Has a JSON parser? The contract has to be parsed to be checked.
have_json_parser() {
	command -v python3 >/dev/null 2>&1 || command -v jq >/dev/null 2>&1
}

finish() {
	printf '%s check(s), %s failed\n' "$checks" "$failed"
	[ "$failed" -eq 0 ] || exit 1
	exit 0
}
