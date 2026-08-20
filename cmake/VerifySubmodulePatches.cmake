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

FOREACH(_patch IN LISTS PATCHES)
	GET_FILENAME_COMPONENT(_patch_name "${_patch}" NAME)

	# Same test the applier uses: --check --reverse exits 0 only when the patch
	# is already cleanly applied. --ignore-whitespace matches the apply side so
	# CRLF/LF drift in a working tree is not reported as a missing patch.
	IF(DIR_FLAG STREQUAL "")
		EXECUTE_PROCESS(
			COMMAND "${GIT_EXE}" apply --check --reverse --ignore-whitespace "${_patch}"
			WORKING_DIRECTORY "${APPLY_CWD}"
			RESULT_VARIABLE _rc
			OUTPUT_QUIET ERROR_QUIET)
	ELSE()
		EXECUTE_PROCESS(
			COMMAND "${GIT_EXE}" apply ${DIR_FLAG} --check --reverse --ignore-whitespace "${_patch}"
			WORKING_DIRECTORY "${APPLY_CWD}"
			RESULT_VARIABLE _rc
			OUTPUT_QUIET ERROR_QUIET)
	ENDIF()

	IF(NOT _rc EQUAL 0)
		MESSAGE(FATAL_ERROR
			"${LABEL}: patch '${_patch_name}' is NOT applied to the submodule.\n"
			"\n"
			"The sources would compile without it and the resulting binary would "
			"be missing whatever the patch fixes, with no other symptom. That is "
			"why this stops the build.\n"
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
ENDFOREACH()
