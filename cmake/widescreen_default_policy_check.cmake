# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
#
# W-103 widescreen policy: every DEFAULT resolution in the tree is 16:9.
#
# Rationale is evidence correctness, not aesthetics. A screenshot captured at
# 4:3 does not match the player's framing -- HUD placement and FOV differ -- so
# "the HUD looks right" read off a 4:3 capture is false evidence.
#
# Scope is deliberately narrow. This gate inspects exactly three things:
#
#   1. resolution cvar defaults  (r_renderWidth/Height, r_customWidth/Height)
#   2. the SDL desktop-probe/fallback extents in code/sdl/sdl_glimp.c, which
#      are the resolution the headless/CI evidence path actually runs at
#   3. harness swapchain-extent expectations in tests/*.sh
#
# It must NOT flag:
#   * Quake 3's VIRTUAL 640x480 UI coordinate space (a unit of scale, not a
#     resolution) in tr_backend.c / cg_*.c / cl_scrn.c / *.wui
#   * the user-selectable cl_vidModes[] list in code/client/cl_main.c, which
#     may legitimately still offer 320x240 .. 1024x768 to players
#
# Canonical 16:9 table: 1280x720 default and visual gate, 960x540 cheap
# headless, 1920x1080 / 2560x1440 high-res.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

SET(_violations)

# ---------------------------------------------------------------------------
# Shared predicate: is WxH an exact 16:9 pair?
# ---------------------------------------------------------------------------
FUNCTION(WIRED_IS_16X9 _w _h _out)
	MATH(EXPR _lhs "${_w} * 9")
	MATH(EXPR _rhs "${_h} * 16")
	IF(_lhs EQUAL _rhs)
		SET(${_out} TRUE PARENT_SCOPE)
	ELSE()
		SET(${_out} FALSE PARENT_SCOPE)
	ENDIF()
ENDFUNCTION()

# ---------------------------------------------------------------------------
# 1. Resolution cvar defaults.
#
# Both spellings appear in the tree: the renderers use
#   ri.Cvar_Get( "r_renderWidth", "1280", ... )
# and the client cvar table uses
#   CVAR_INT( "r_customWidth", "1280", ... )
# Width and height are declared on separate lines, so collect each cvar's
# default first and pair them afterwards.
# ---------------------------------------------------------------------------
SET(_cvar_files
	code/renderer/tr_init.c
	code/renderer2/tr_init.c
	code/renderervk/tr_init.c
	code/client/cl_main.c
)
SET(_cvar_pairs
	"r_renderWidth:r_renderHeight"
	"r_customWidth:r_customHeight"
)

FOREACH(_relative IN LISTS _cvar_files)
	SET(_path "${SOURCE_ROOT}/${_relative}")
	IF(NOT EXISTS "${_path}")
		MESSAGE(FATAL_ERROR "W-103 policy: expected file is missing: ${_relative}")
	ENDIF()
	FILE(READ "${_path}" _body)
	FOREACH(_pair IN LISTS _cvar_pairs)
		STRING(REPLACE ":" ";" _names "${_pair}")
		LIST(GET _names 0 _width_name)
		LIST(GET _names 1 _height_name)
		SET(_width_value "")
		SET(_height_value "")
		FOREACH(_name ${_width_name} ${_height_name})
			# Case-insensitive: the client table spells r_customWidth, the
			# renderer help text spells r_customwidth.
			STRING(REGEX MATCH "\"(${_name})\"[ \t]*,[ \t]*\"([0-9]+)\""
				_hit "${_body}")
			SET(_found "${CMAKE_MATCH_2}")
			IF(_hit)
				IF(_name STREQUAL _width_name)
					SET(_width_value "${_found}")
				ELSE()
					SET(_height_value "${_found}")
				ENDIF()
			ENDIF()
		ENDFOREACH()
		IF(_width_value AND _height_value)
			WIRED_IS_16X9("${_width_value}" "${_height_value}" _ok)
			IF(NOT _ok)
				LIST(APPEND _violations
					"${_relative}: ${_width_name}/${_height_name} default ${_width_value}x${_height_value} is not 16:9")
			ENDIF()
		ENDIF()
	ENDFOREACH()
ENDFOREACH()

# ---------------------------------------------------------------------------
# 2. SDL desktop probe + fallback extents.
#
# These fire when the desktop query fails -- exactly the headless/CI path -- so
# they are the resolution the visual evidence is captured at.
#   FindNearestDisplay( &x, &y, 1280, 720 )
#   glw_state.desktop_width  = 1280;
#   glw_state.desktop_height = 720;
# ---------------------------------------------------------------------------
SET(_glimp_relative code/sdl/sdl_glimp.c)
SET(_glimp "${SOURCE_ROOT}/${_glimp_relative}")
IF(NOT EXISTS "${_glimp}")
	MESSAGE(FATAL_ERROR "W-103 policy: expected file is missing: ${_glimp_relative}")
ENDIF()
FILE(READ "${_glimp}" _glimp_body)

STRING(REGEX MATCHALL "FindNearestDisplay\\([^)]*,[ \t]*([0-9]+)[ \t]*,[ \t]*([0-9]+)[ \t]*\\)"
	_probes "${_glimp_body}")
FOREACH(_probe IN LISTS _probes)
	STRING(REGEX MATCH ",[ \t]*([0-9]+)[ \t]*,[ \t]*([0-9]+)[ \t]*\\)$" _m "${_probe}")
	IF(_m)
		# Latch the captures before calling out: a nested command resets
		# CMAKE_MATCH_* and the pair would be read back mangled.
		SET(_probe_w "${CMAKE_MATCH_1}")
		SET(_probe_h "${CMAKE_MATCH_2}")
		WIRED_IS_16X9("${_probe_w}" "${_probe_h}" _ok)
		IF(NOT _ok)
			LIST(APPEND _violations
				"${_glimp_relative}: FindNearestDisplay probe extent ${_probe_w}x${_probe_h} is not 16:9")
		ENDIF()
	ENDIF()
ENDFOREACH()

# desktop_width/desktop_height literal assignments, paired in source order.
# Read line-wise rather than MATCHALL over the whole body: these statements end
# in ';', and a ';' inside a MATCHALL result silently splits the CMake list.
FILE(STRINGS "${_glimp}" _desktop_lines
	REGEX "desktop_(width|height)[ \t]*=[ \t]*[0-9]+[ \t]*;")
SET(_pending_width "")
FOREACH(_assign IN LISTS _desktop_lines)
	STRING(REGEX MATCH "desktop_(width|height)[ \t]*=[ \t]*([0-9]+)" _m "${_assign}")
	IF(NOT _m)
		CONTINUE()
	ENDIF()
	# Latch before any nested call: CMAKE_MATCH_* does not survive one.
	SET(_which "${CMAKE_MATCH_1}")
	SET(_value "${CMAKE_MATCH_2}")
	IF(_which STREQUAL "width")
		SET(_pending_width "${_value}")
	ELSEIF(NOT _pending_width STREQUAL "")
		WIRED_IS_16X9("${_pending_width}" "${_value}" _ok)
		IF(NOT _ok)
			LIST(APPEND _violations
				"${_glimp_relative}: desktop fallback extent ${_pending_width}x${_value} is not 16:9")
		ENDIF()
		SET(_pending_width "")
	ENDIF()
ENDFOREACH()

# ---------------------------------------------------------------------------
# 3. Harness swapchain-extent expectations.
#
# Runtime-check harnesses embed the extent they expect to observe, e.g.
#   "RAL swapchain ready: recreate=1 extent=1280x720 images=3 ..."
# A 4:3 value here means the harness is asserting 4:3 evidence is correct.
#
# Two deliberate exclusions:
#   * replace("extent=AxB","extent=CxD") -- the harnesses' own negative-control
#     mutations, which inject a WRONG extent on purpose (e.g. 1279x720) to
#     prove the drift check fires. Only the replacement ARGUMENTS are dropped,
#     matched by that exact shape; a bare .replace( anywhere on the line is
#     NOT enough to excuse a real expectation on the same line.
#   * tests/visual/, whose 1440x900 (16:10) artboard-native baselines are an
#     open owner decision, not a defect this gate may pre-empt. GLOB is
#     non-recursive so tests/visual/scripts/*.sh is already out of scope; the
#     explicit skip keeps that intent from silently changing.
#
# Read whole-file, not FILE(STRINGS): these harnesses embed their fixtures in
# very long ';'-bearing python lines, and FILE(STRINGS) splits on ';' into
# list elements that no longer correspond to source lines.
# ---------------------------------------------------------------------------
FILE(GLOB _harnesses "${SOURCE_ROOT}/tests/*.sh")
FOREACH(_path IN LISTS _harnesses)
	FILE(RELATIVE_PATH _rel "${SOURCE_ROOT}" "${_path}")
	IF(_rel MATCHES "^tests/visual/")
		CONTINUE()
	ENDIF()
	FILE(READ "${_path}" _body)
	# Drop only the mutation replacement pairs, then scan what remains.
	STRING(REGEX REPLACE
		"replace\\([ \t]*\"extent=[0-9]+x[0-9]+\"[ \t]*,[ \t]*\"extent=[0-9]+x[0-9]+\"[ \t]*\\)"
		"" _scanned "${_body}")
	STRING(REGEX MATCHALL "extent=([0-9]+)x([0-9]+)" _extents "${_scanned}")
	LIST(REMOVE_DUPLICATES _extents)
	FOREACH(_extent IN LISTS _extents)
		STRING(REGEX MATCH "extent=([0-9]+)x([0-9]+)" _m "${_extent}")
		SET(_ext_w "${CMAKE_MATCH_1}")
		SET(_ext_h "${CMAKE_MATCH_2}")
		WIRED_IS_16X9("${_ext_w}" "${_ext_h}" _ok)
		IF(NOT _ok)
			LIST(APPEND _violations
				"${_rel}: harness expects extent ${_ext_w}x${_ext_h}, which is not 16:9")
		ENDIF()
	ENDFOREACH()
ENDFOREACH()

# ---------------------------------------------------------------------------
# Verdict.
# ---------------------------------------------------------------------------
IF(_violations)
	LIST(JOIN _violations "\n  " _report)
	MESSAGE(FATAL_ERROR
		"W-103 widescreen policy: non-16:9 default(s) entered the tree.\n"
		"  ${_report}\n"
		"Canonical table: 1280x720 default/visual gate, 960x540 cheap headless,\n"
		"1920x1080 and 2560x1440 high-res. The user-selectable cl_vidModes[]\n"
		"list and the virtual 640x480 UI coordinate space are out of scope.")
ENDIF()

MESSAGE(STATUS "W-103 widescreen policy: all resolution defaults and harness extents are 16:9")
