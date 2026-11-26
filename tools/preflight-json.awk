# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 REVYTECH, Inc.
#
# preflight-json.awk -- write the machine-readable preflight contract.
#
# The Makefile's preflight target collects what it found and hands it over
# through the environment; this program writes the JSON document. Every value
# that is not a literal of this file goes through jesc(), so a directory name
# that carries a quote, a backslash or a control character cannot break the
# document or add a property to it.
#
# The environment it reads:
#
#   PREFLIGHT_ITEMS        the item names, separated by spaces
#   PREFLIGHT_<NAME>_STATE "ok" or "missing", for each item
#   PREFLIGHT_<NAME>_INFO  the path or the reason, for each item
#   PREFLIGHT_OS           the operating system
#   PREFLIGHT_ARCH         the machine architecture
#   PREFLIGHT_CWD          the directory the check ran in
#
# The purpose of the repository comes on the command line:
#
#   awk -f preflight-json.awk --purpose fork
#
# It is run with LC_ALL=C, so one character is one byte.

# The byte value of every character, so the control characters can be told
# apart from the printable ones.
function build_table(    i) {
	for (i = 1; i < 256; i++)
		ord[sprintf("%c", i)] = i
}

# One JSON string, without the quotes around it.
function jesc(s,    i, c, code, out) {
	out = ""
	for (i = 1; i <= length(s); i++) {
		c = substr(s, i, 1)
		code = ord[c]
		if (c == "\"")
			out = out "\\\""
		else if (c == "\\")
			out = out "\\\\"
		else if (code == 9)
			out = out "\\t"
		else if (code == 10)
			out = out "\\n"
		else if (code == 13)
			out = out "\\r"
		else if (code < 32 || code == 127)
			out = out sprintf("\\u%04x", code)
		else
			out = out c
	}
	return out
}

BEGIN {
	build_table()

	# The purpose comes on the command line: --purpose <value>
	purpose = ""
	for (i = 1; i < ARGC; i++) {
		if (ARGV[i] == "--purpose" && i + 1 < ARGC) {
			purpose = ARGV[i + 1]
			ARGV[i] = ""
			ARGV[i + 1] = ""
			i++
		}
	}

	n = split(ENVIRON["PREFLIGHT_ITEMS"], names, " ")
	if (n < 1)
		n = 0

	printf "{\n"
	printf "  \"schema_version\": \"1.0\",\n"
	printf "  \"tool\": \"preflight\",\n"
	printf "  \"purpose\": \"%s\",\n", jesc(purpose)
	printf "  \"metadata\": {\n"
	printf "    \"os\": \"%s\",\n", jesc(ENVIRON["PREFLIGHT_OS"])
	printf "    \"arch\": \"%s\",\n", jesc(ENVIRON["PREFLIGHT_ARCH"])
	printf "    \"cwd\": \"%s\",\n", jesc(ENVIRON["PREFLIGHT_CWD"])
	printf "    \"checked\": %d\n", n
	printf "  },\n"
	printf "  \"status\": [\n"
	for (i = 1; i <= n; i++) {
		name = names[i]
		state = ENVIRON["PREFLIGHT_" name "_STATE"]
		info = ENVIRON["PREFLIGHT_" name "_INFO"]
		printf "    {\"index\": %d, \"name\": \"%s\", \"state\": \"%s\", \"available\": %s",
			i - 1, jesc(name), jesc(state), (state == "ok" ? "true" : "false")
		if (state != "ok")
			printf ", \"reason\": \"%s\"", jesc(info)
		printf "}%s\n", (i < n ? "," : "")
	}
	printf "  ],\n"
	printf "  \"items\": []\n"
	printf "}\n"
}
