# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# Build-time check that vendored-submodule patches are STILL applied.
#
# Run in script mode (cmake -P) from a custom target, so it executes on every
# build rather than only at configure time. See the rationale where
# WIRED_APPLY_SUBMODULE_PATCHES creates the target: patching is a configure-time
# side effect that a build never revisits, so a `git checkout` inside a
# submodule silently produces an unpatched binary that compiles and runs.
#
# Expected -D arguments: GIT_EXE, LABEL, APPLY_CWD, DIR_FLAG, PATCHES.

# Ask the narrow question: has the tree been RESET, losing the patches entirely?
#
# The obvious check — reverse-apply each patch and require every one to succeed
# — is wrong, and picoquic proves it. Patches apply in order and later ones
# rewrite lines earlier ones used as context: 06 sits next to
# `#include <WinSock2.h>`, and 07 lower-cases that include for MinGW. On a
# correctly patched tree 06 is present (the `#include <errno.h>` it adds is
# right there) yet reverse-applying it alone FAILS, because its context no
# longer exists. Reverse-applying the whole series together fails for the same
# reason. So "can this be reversed" and "was this applied" are different
# questions, and only the second one matters here.
#
# What is reliable is the FIRST patch of the series: nothing else has rewritten
# its context yet, so if it reverse-applies, the series was applied; if it does
# not, either the tree was reset or the series drifted. Both cases want the same
# response — re-run configure, which detects drift and reapplies from scratch.
#
# This deliberately does NOT try to detect a partially-applied series. Configure
# already handles that ("patch-series drift — resetting the submodule tree and
# reapplying"), and a build-time check that duplicates it would fail on trees
# configure is about to fix.
LIST(GET PATCHES 0 _first_patch)
GET_FILENAME_COMPONENT(_first_name "${_first_patch}" NAME)

IF(DIR_FLAG STREQUAL "")
	EXECUTE_PROCESS(
		COMMAND "${GIT_EXE}" apply --check --reverse --ignore-whitespace "${_first_patch}"
		WORKING_DIRECTORY "${APPLY_CWD}"
		RESULT_VARIABLE _rc
		OUTPUT_QUIET ERROR_QUIET)
ELSE()
	EXECUTE_PROCESS(
		COMMAND "${GIT_EXE}" apply ${DIR_FLAG} --check --reverse --ignore-whitespace "${_first_patch}"
		WORKING_DIRECTORY "${APPLY_CWD}"
		RESULT_VARIABLE _rc
		OUTPUT_QUIET ERROR_QUIET)
ENDIF()

IF(NOT _rc EQUAL 0)
	MESSAGE(FATAL_ERROR
		"${LABEL}: the vendored patches are NOT applied to the submodule "
		"('${_first_name}' is absent).\n"
		"\n"
		"The sources would compile without them and the resulting binary would "
		"be missing whatever they fix, with no other symptom. That is why this "
		"stops the build.\n"
		"\n"
		"Most likely you ran `git checkout`/`reset` inside the submodule, or "
		"`git submodule update` moved it. Patches are applied at CONFIGURE "
		"time, so a build alone does not restore them.\n"
		"\n"
		"To recover:\n"
		"  cmake -S . -B <build-dir>    # re-applies, prints 'applied ...'\n"
		"or, if the submodule has conflicting local edits:\n"
		"  git -C src/libs/<submodule> reset --hard\n"
		"  cmake -S . -B <build-dir>")
ENDIF()
