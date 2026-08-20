#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# Every test script must PARSE under the oldest bash it will meet.
#
# WHY THIS EXISTS
# macOS ships bash 3.2 (frozen in 2007 for licence reasons) as /bin/bash, and
# the macOS CI runner uses it. A developer machine with Homebrew has bash 5.x,
# so a script can parse cleanly all day locally and fail on the runner.
#
# The failure that prompted this was maximally unhelpful:
#
#   headless-map-transition-zonecheck.sh: line 889: unexpected EOF while
#   looking for matching `"'
#
# Line 889 was the end of the file. The actual cause was line 273 — a lone
# apostrophe inside a PYTHON COMMENT, in a heredoc, inside a "$(...)"
# substitution:
#
#   # 6. the artefact's self-report: ...
#
# bash 3.2 tracks quotes through a heredoc body nested in a double-quoted
# command substitution; bash 5 does not. One unpaired ' sent it hunting for a
# closing quote to the end of the file. Paired apostrophes (like 'foo' in an
# f-string) are fine — it is the odd one that breaks it.
#
# No amount of care prevents that being written again, so this checks instead.
# Parse-only (-n): nothing is executed, so it is fast and has no side effects.

set -u

BASH32="${BASH32:-/bin/bash}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -x "$BASH32" ]; then
	echo "SKIP: no $BASH32 to check against"
	exit 0
fi

ver="$( "$BASH32" --version | head -1 )"
echo "==> parsing every tests/*.sh with: $ver"

fail=0
checked=0
for f in "$SCRIPT_DIR"/*.sh; do
	[ -e "$f" ] || continue
	checked=$(( checked + 1 ))
	if ! err="$( "$BASH32" -n "$f" 2>&1 )"; then
		echo "  FAIL $( basename "$f" )"
		printf '%s\n' "$err" | sed 's/^/        /'
		# The reported line is usually the END of the file, not the cause.
		# Unpaired apostrophes inside heredoc bodies are the usual culprit, so
		# point at them directly rather than leaving the reader to bisect.
		awk '/<<'"'"'[A-Z]+'"'"'/ { inblk=1; next }
		     /^[A-Z]+$/           { inblk=0 }
		     inblk {
		         n=gsub(/'"'"'/,"&")
		         if (n % 2 == 1) printf "        suspect line %d: %s\n", NR, $0
		     }' "$f"
		fail=1
	fi
done

echo "    $checked script(s) checked"
if [ "$fail" -ne 0 ]; then
	echo "==> SHELL PORTABILITY FAIL"
	exit 1
fi
echo "==> SHELL PORTABILITY PASS"
