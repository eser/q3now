# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/server/sv_init.c" SOURCE)
string(FIND "${SOURCE}" "Nav_UnloadMap();" UNLOAD_POS)
string(FIND "${SOURCE}" "Hunk_ClearLevel();" HUNK_POS)
string(FIND "${SOURCE}" "CM_ClearMap();" COLLISION_POS)

if(UNLOAD_POS EQUAL -1)
	message(FATAL_ERROR "map teardown must join/unload nav before invalidating level memory")
endif()
if(HUNK_POS EQUAL -1 OR COLLISION_POS EQUAL -1)
	message(FATAL_ERROR "map teardown invalidation anchors are missing")
endif()
if(NOT UNLOAD_POS LESS HUNK_POS OR NOT HUNK_POS LESS COLLISION_POS)
	message(FATAL_ERROR
		"unsafe map teardown order: expected Nav_UnloadMap < Hunk_ClearLevel < CM_ClearMap")
endif()

message(STATUS "nav map teardown source policy: PASS")
