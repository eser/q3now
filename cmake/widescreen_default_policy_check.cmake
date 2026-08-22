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
#   2. the SDL desktop-probe/fallback extents and failed-mode recovery, which
#      are the resolution the headless/CI evidence path actually runs at
#   3. harness window-mode arguments, direct SDL test windows, and
#      swapchain-extent expectations
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

# Broader repository invariant for every window-opening harness, including
# nested visual/tool scripts: 4:3 and narrower are forbidden.  The canonical
# evidence paths above remain stricter (exact 16:9); this predicate permits a
# deliberately artboard-native 16:10 harness while still making 640x480
# impossible.
FUNCTION(WIRED_IS_WIDESCREEN _w _h _out)
	MATH(EXPR _lhs "${_w} * 3")
	MATH(EXPR _rhs "${_h} * 4")
	IF(_lhs GREATER _rhs)
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

# A failed custom/window request used to retry mode 3 (640x480), so a harness
# with correct 1280x720 arguments could still open a forbidden 4:3 window.
# Recovery must use the canonical 1280x720 table entry (mode 13) in both GL and
# Vulkan paths. The unknown-desktop fallback in CL_GetModeInfo must agree.
STRING(REGEX MATCHALL "GLimp_StartDriverAndSetMode[(][ \t]*13[ \t]*," _widescreen_retries "${_glimp_body}")
LIST(LENGTH _widescreen_retries _widescreen_retry_count)
IF(NOT _widescreen_retry_count EQUAL 2)
	LIST(APPEND _violations
		"${_glimp_relative}: expected exactly two mode-13 (1280x720) failed-mode retries, got ${_widescreen_retry_count}")
ENDIF()
IF(_glimp_body MATCHES "GLimp_StartDriverAndSetMode[(][ \t]*3[ \t]*,")
	LIST(APPEND _violations
		"${_glimp_relative}: legacy mode-3 (640x480) failed-mode retry is forbidden")
ENDIF()

# Source arguments are not sufficient: archived cvars used to override a
# malformed harness back to 640x480 while the static script gate stayed green.
# Every runtime path, automated or interactive, must reject a requested or
# platform-coerced non-16:9 extent before publishing the hidden SDL window.
FOREACH(_needle IN ITEMS
	"mode != -1"
	"(int64_t)r_customwidth->integer * 9"
	"mode = 13;"
	"flags |= SDL_WINDOW_HIDDEN;"
	"window mode %dx%d is not exact 16:9; refusing it"
	"(int64_t)config->vidWidth * 9"
	"Window became non-16:9"
	"Automated window extent fell below 1280x720"
	"r_customwidth->integer < 1280"
	"r_customheight->integer < 720"
	"(int64_t)logicalWidth * 9 != (int64_t)logicalHeight * 16"
	"(int64_t)pixelWidth * 9 != (int64_t)pixelHeight * 16"
	"window-extent schema=2 requested=%dx%d logical=%dx%d pixels=%dx%d exact16x9=1 publish-ready=1"
	"if ( !SDL_ShowWindow( SDL_window ) )"
	"SDL_DestroyWindow( SDL_window )")
	STRING(FIND "${_glimp_body}" "${_needle}" _found)
	IF(_found EQUAL -1)
		LIST(APPEND _violations
			"${_glimp_relative}: automated runtime widescreen guard lost: ${_needle}")
	ENDIF()
ENDFOREACH()
STRING(FIND "${_glimp_body}"
	"window mode %dx%d is not exact 16:9; refusing it" _requested_guard)
STRING(FIND "${_glimp_body}" "SDL_CreateWindow( cl_title" _window_create)
STRING(FIND "${_glimp_body}" "Window became non-16:9" _actual_guard)
STRING(FIND "${_glimp_body}" "window-extent schema=2" _actual_receipt)
STRING(FIND "${_glimp_body}" "if ( !SDL_ShowWindow( SDL_window ) )" _window_show)
IF(_requested_guard EQUAL -1 OR _window_create EQUAL -1
	OR _actual_guard EQUAL -1 OR _actual_receipt EQUAL -1 OR _window_show EQUAL -1
	OR NOT _requested_guard LESS _window_create
	OR NOT _window_create LESS _actual_guard
	OR NOT _actual_guard LESS _actual_receipt
	OR NOT _actual_receipt LESS _window_show)
	LIST(APPEND _violations
		"${_glimp_relative}: exact-16:9 request/create/actual/show order is not fail-closed")
ENDIF()
IF(_glimp_body MATCHES
	"if [(] com_automated && com_automated->integer [)][ \t\r\n]*flags [|]= SDL_WINDOW_HIDDEN")
	LIST(APPEND _violations
		"${_glimp_relative}: hidden-until-validated is automated-only; all windows must obey it")
ENDIF()
FOREACH(_needle IN ITEMS
	"config->vidHeight = ( config->vidHeight / 9 ) * 9;"
	"config->vidWidth = ( config->vidHeight / 9 ) * 16;")
	STRING(FIND "${_glimp_body}" "${_needle}" _found)
	IF(_found EQUAL -1)
		LIST(APPEND _violations
			"${_glimp_relative}: decorated-window fit no longer preserves exact 16:9: ${_needle}")
	ENDIF()
ENDFOREACH()

SET(_client_relative code/client/cl_main.c)
FILE(READ "${SOURCE_ROOT}/${_client_relative}" _client_body)
STRING(FIND "${_client_body}"
	"(int64_t)*width * 9 != (int64_t)*height * 16" _mode_extent_guard)
IF(_mode_extent_guard EQUAL -1)
	LIST(APPEND _violations
		"${_client_relative}: common mode authority can publish a non-16:9 extent")
ENDIF()
STRING(FIND "${_client_body}"
	"if ( mode == -2 && (dw == 0 || dh == 0) )\n\t\tmode = 13;"
	_unknown_desktop_fallback)
IF(_unknown_desktop_fallback EQUAL -1)
	LIST(APPEND _violations
		"${_client_relative}: unknown-desktop fallback is not canonical mode 13 (1280x720)")
ENDIF()

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
# 3. Harness window modes and swapchain-extent expectations.
#
# Test harnesses that force a window size must use custom mode (-1), because a
# non-negative r_mode is an opaque lookup into the legacy mode table and may
# silently select a 4:3 window (r_mode 3 is 640x480). Literal custom extents are
# checked as exact 16:9 pairs. Variable-driven extents remain covered by their
# owning harness defaults and the observed swapchain receipt.
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

	# A windowed native harness must state its complete resolution authority.
	# Otherwise a fresh test home may fall back to a legacy mode while a reused
	# home may inherit an archived one, making both the window and its evidence
	# nondeterministic. Join shell continuation lines before checking each
	# logical invocation independently.
	STRING(REGEX REPLACE "\\\\[ \t]*\r?\n" " " _logical_body "${_body}")
	# CMake lists split at semicolons; shell command lines commonly contain
	# controller bookkeeping after the launch. Protect those separators so one
	# logical invocation remains one policy item.
	STRING(REPLACE ";" "__WIRED_SHELL_SEMICOLON__" _logical_body "${_logical_body}")
	STRING(REGEX MATCHALL
		"[^\r\n]*\\+set[ \t]+r_fullscreen[ \t]+0[^\r\n]*"
		_windowed_invocations "${_logical_body}")
	FOREACH(_invocation IN LISTS _windowed_invocations)
		IF(NOT _invocation MATCHES "\\+set[ \t]+r_mode[ \t]+-1([ \t]|$)"
			OR NOT _invocation MATCHES "\\+set[ \t]+r_customwidth[ \t]+[^ \t\r\n]+"
			OR NOT _invocation MATCHES "\\+set[ \t]+r_customheight[ \t]+[^ \t\r\n]+")
			LIST(APPEND _violations
				"${_rel}: windowed harness invocation omits r_mode -1 and an explicit custom width/height")
		ENDIF()
		# Startup scripts can execute a map before later command-line setters are
		# consumed.  Resolution authority therefore has to precede every +exec or
		# +map that may initialize the renderer; presence alone is insufficient.
		STRING(FIND "${_invocation}" "+set r_fullscreen 0" _fullscreen_pos)
		STRING(FIND "${_invocation}" "+set r_mode -1" _mode_pos)
		STRING(FIND "${_invocation}" "+set r_customwidth " _width_pos)
		STRING(FIND "${_invocation}" "+set r_customheight " _height_pos)
		STRING(FIND "${_invocation}" "+exec " _exec_pos)
		STRING(FIND "${_invocation}" "+map " _map_pos)
		FOREACH(_renderer_start_pos IN ITEMS "${_exec_pos}" "${_map_pos}")
			IF(NOT _renderer_start_pos EQUAL -1
				AND (NOT _fullscreen_pos LESS _renderer_start_pos
					OR NOT _mode_pos LESS _renderer_start_pos
					OR NOT _width_pos LESS _renderer_start_pos
					OR NOT _height_pos LESS _renderer_start_pos))
				LIST(APPEND _violations
					"${_rel}: 16:9 window authority must precede +exec/+map renderer startup")
			ENDIF()
		ENDFOREACH()
	ENDFOREACH()

	# An explicit built-in mode is never acceptable in an evidence harness.
	# Requiring custom mode makes the width/height authority visible in source.
	STRING(REGEX MATCHALL "\\+set[ \t]+r_mode[ \t]+(-?[0-9]+)" _mode_args "${_body}")
	FOREACH(_mode_arg IN LISTS _mode_args)
		STRING(REGEX MATCH "r_mode[ \t]+(-?[0-9]+)" _m "${_mode_arg}")
		IF(NOT CMAKE_MATCH_1 STREQUAL "-1")
			LIST(APPEND _violations
				"${_rel}: harness forces opaque legacy ${_mode_arg}; use r_mode -1 with an explicit 16:9 custom extent")
		ENDIF()
	ENDFOREACH()

	# Check adjacent literal custom width/height arguments. The whitespace class
	# includes shell line continuations so both one-line and formatted commands
	# are covered without treating Quake's virtual 640x480 UI space as a window.
	STRING(REGEX MATCHALL
		"\\+set[ \t]+r_customwidth[ \t]+([0-9]+)[ \t\r\n\\\\]+\\+set[ \t]+r_customheight[ \t]+([0-9]+)"
		_custom_pairs "${_body}")
	FOREACH(_custom_pair IN LISTS _custom_pairs)
		STRING(REGEX MATCH
			"r_customwidth[ \t]+([0-9]+)[ \t\r\n\\\\]+\\+set[ \t]+r_customheight[ \t]+([0-9]+)"
			_m "${_custom_pair}")
		SET(_custom_w "${CMAKE_MATCH_1}")
		SET(_custom_h "${CMAKE_MATCH_2}")
		WIRED_IS_16X9("${_custom_w}" "${_custom_h}" _ok)
		IF(NOT _ok)
			LIST(APPEND _violations
				"${_rel}: harness launches a ${_custom_w}x${_custom_h} window, which is not 16:9")
		ENDIF()
	ENDFOREACH()

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

# Host executables can create SDL windows without going through r_mode. Scan
# their literal SDL_CreateWindow extents too; hidden tests are still windows
# and follow the same widescreen-only rule.
FILE(GLOB _native_harnesses
	"${SOURCE_ROOT}/tests/*.c"
	"${SOURCE_ROOT}/tests/*.cc"
	"${SOURCE_ROOT}/tests/*.cpp"
	"${SOURCE_ROOT}/tests/*.m"
	"${SOURCE_ROOT}/tests/*.mm")
FOREACH(_path IN LISTS _native_harnesses)
	FILE(RELATIVE_PATH _rel "${SOURCE_ROOT}" "${_path}")
	FILE(READ "${_path}" _body)
	STRING(REGEX MATCHALL
		"SDL_CreateWindow[(][^,]+,[ \t\r\n]*([0-9]+)[ \t]*,[ \t\r\n]*([0-9]+)[ \t]*,"
		_direct_windows "${_body}")
	FOREACH(_window IN LISTS _direct_windows)
		STRING(REGEX MATCH
			",[ \t\r\n]*([0-9]+)[ \t]*,[ \t\r\n]*([0-9]+)[ \t]*,$"
			_m "${_window}")
		SET(_window_w "${CMAKE_MATCH_1}")
		SET(_window_h "${CMAKE_MATCH_2}")
		WIRED_IS_16X9("${_window_w}" "${_window_h}" _ok)
		IF(NOT _ok)
			LIST(APPEND _violations
				"${_rel}: direct SDL test window ${_window_w}x${_window_h} is not 16:9")
		ENDIF()
	ENDFOREACH()
ENDFOREACH()

# SDL adapters can pass dimensions indirectly into SDL_CreateWindow. Their
# public create/resize boundaries and post-SDL receipts use the same pure RAL
# extent predicate. Native hosts never pass a non-widescreen extent to a
# window-owning API, even as a negative test: 640x480 is tested only through
# the pure predicate so a regression cannot flash or publish a 4:3 window.
SET(_indirect_window_contracts
	"code/renderer/ral/ral_presentation_host.c:(uint64_t)width * 9u == (uint64_t)height * 16u"
	"code/sdl/sdl_metal_ral.mm:!Ral_PresentationExtentValid( createInfo->desiredWidth"
	"code/sdl/sdl_metal_ral.mm:!Ral_PresentationExtentValid( logicalWidth, logicalHeight )"
	"code/sdl/sdl_metal_ral.mm:!Ral_PresentationExtentValid( (uint32_t)pixelWidth"
	"code/sdl/sdl_ral_presentation.mm:!Ral_PresentationExtentValid( logicalWidth, logicalHeight )"
	"code/sdl/sdl_ral_presentation.mm:!Ral_PresentationExtentValid( (uint32_t)pixelWidth"
	"tests/ral_presentation_host_test.c:!Ral_PresentationExtentValid( 640u, 480u )")
FOREACH(_contract IN LISTS _indirect_window_contracts)
	STRING(REPLACE ":" ";" _parts "${_contract}")
	LIST(GET _parts 0 _relative)
	LIST(GET _parts 1 _needle)
	FILE(READ "${SOURCE_ROOT}/${_relative}" _body)
	STRING(FIND "${_body}" "${_needle}" _found)
	IF(_found EQUAL -1)
		LIST(APPEND _violations
			"${_relative}: indirect SDL widescreen contract lost: ${_needle}")
	ENDIF()
ENDFOREACH()

FOREACH(_contract IN ITEMS
	"tests/wired_metal_smoke.mm:WiredMetalSdl_Create( core, &coreReceipt, &createInfo"
	"tests/ral_sdl_presentation_host_test.c:WiredSdlRalPresentationHost_Create( 640u, 480u"
	"tests/ral_sdl_presentation_host_test.c:WiredSdlRalPresentationHost_RequestResize( host, 640u, 480u )")
	STRING(REPLACE ":" ";" _parts "${_contract}")
	LIST(GET _parts 0 _relative)
	LIST(GET _parts 1 _needle)
	IF(_relative STREQUAL "tests/wired_metal_smoke.mm")
		# The smoke still creates a window, but its create info is pinned above to
		# 1280x720; only the literal forbidden 640x480 assignment matters here.
		SET(_needle "createInfo.desiredHeight = 480u")
	ENDIF()
	FILE(READ "${SOURCE_ROOT}/${_relative}" _body)
	STRING(FIND "${_body}" "${_needle}" _found)
	IF(NOT _found EQUAL -1)
		LIST(APPEND _violations
			"${_relative}: native harness passes forbidden 640x480 to a window-owning API")
	ENDIF()
ENDFOREACH()

# make run-game wrappers do not name the client binary directly, so the
# invocation scan above cannot infer that they open a window. They still must
# carry the same explicit custom-mode authority.
FOREACH(_path IN LISTS _harnesses)
	FILE(RELATIVE_PATH _rel "${SOURCE_ROOT}" "${_path}")
	FILE(READ "${_path}" _body)
	STRING(REGEX REPLACE "\\\\[ \t]*\r?\n" " " _logical_body "${_body}")
	STRING(REGEX MATCHALL "[^\r\n]*make[ \t]+run-game[^\r\n]*" _make_runs "${_logical_body}")
	FOREACH(_invocation IN LISTS _make_runs)
		IF(NOT _invocation MATCHES "r_fullscreen[ \t]+0"
			OR NOT _invocation MATCHES "r_mode[ \t]+-1"
			OR NOT _invocation MATCHES "r_customwidth[ \t]+[^ \t\r\n]+"
			OR NOT _invocation MATCHES "r_customheight[ \t]+[^ \t\r\n]+")
			LIST(APPEND _violations
				"${_rel}: make run-game harness omits explicit widescreen custom-mode authority")
		ENDIF()
	ENDFOREACH()
ENDFOREACH()

# The exact-16:9 evidence inventory above intentionally owns the top-level
# gates.  This second, recursive inventory closes the wider authority boundary:
# helper scripts under tests/visual/scripts (and future nested harnesses) may
# not evade the no-4:3 rule merely because they are not in tests/*.sh.
FILE(GLOB_RECURSE _all_shell_harnesses "${SOURCE_ROOT}/tests/*.sh")
FOREACH(_path IN LISTS _all_shell_harnesses)
	FILE(RELATIVE_PATH _rel "${SOURCE_ROOT}" "${_path}")
	FILE(READ "${_path}" _body)

	# Both command-line setters (`+set`) and generated cfg setters (`set`) are
	# window authority.  Any script that asks for a windowed renderer must carry
	# explicit custom-mode width and height in the same source file.
	IF(_body MATCHES "set[ \t]+r_fullscreen[ \t]+0([ \t\r\n]|$)")
		FOREACH(_needle IN ITEMS "r_mode" "r_customwidth" "r_customheight")
			IF(NOT _body MATCHES "set[ \t]+${_needle}[ \t]+")
				LIST(APPEND _violations
					"${_rel}: nested/windowed harness omits explicit ${_needle} authority")
			ENDIF()
		ENDFOREACH()
		IF(NOT _body MATCHES "set[ \t]+r_mode[ \t]+-1([ \t\r\n]|$)")
			LIST(APPEND _violations
				"${_rel}: nested/windowed harness must use custom r_mode -1")
		ENDIF()
	ENDIF()

	STRING(REGEX MATCHALL
		"set[ \t]+r_customwidth[ \t]+([0-9]+)[ \t\r\n\\\\]+[+]?set[ \t]+r_customheight[ \t]+([0-9]+)"
		_literal_window_pairs "${_body}")
	FOREACH(_pair IN LISTS _literal_window_pairs)
		STRING(REGEX MATCH
			"r_customwidth[ \t]+([0-9]+)[ \t\r\n\\\\]+[+]?set[ \t]+r_customheight[ \t]+([0-9]+)"
			_match "${_pair}")
		SET(_window_w "${CMAKE_MATCH_1}")
		SET(_window_h "${CMAKE_MATCH_2}")
		WIRED_IS_WIDESCREEN("${_window_w}" "${_window_h}" _wide)
		IF(NOT _wide)
			LIST(APPEND _violations
				"${_rel}: nested harness requests forbidden non-widescreen ${_window_w}x${_window_h}")
		ENDIF()
	ENDFOREACH()
ENDFOREACH()

# The retained temporal native run accepts an externally supplied client
# artifact.  Command-line 1280x720 is not enough when that artifact predates
# the hidden-until-validated runtime guard: its legacy failed-mode recovery can
# still publish 640x480.  Require a binary capability preflight before the
# timeout runner can start the process.
SET(_temporal_harness_relative tests/ral-temporal-projection-check.sh)
FILE(READ "${SOURCE_ROOT}/${_temporal_harness_relative}" _temporal_harness_body)
STRING(FIND "${_temporal_harness_body}"
	"grep -aFq \"Automated window extent fell below 1280x720\" \"$WIRED\""
	_temporal_binary_guard)
STRING(FIND "${_temporal_harness_body}"
	"python3 \"$TIMEOUT_RUNNER\" --timeout 120"
	_temporal_native_launch)
IF(_temporal_binary_guard EQUAL -1 OR _temporal_native_launch EQUAL -1
	OR NOT _temporal_binary_guard LESS _temporal_native_launch)
	LIST(APPEND _violations
		"${_temporal_harness_relative}: stale client artifact is not rejected before native window launch")
ENDIF()

# Standalone native/tool hosts can bypass both r_mode and tests/. Scan every
# literal SDL window in tests plus Wired-owned tools and require the canonical
# test minimum as well as exact 16:9. This deliberately rejects even a small
# 640x360 host: no 640-based test/tool window may be mistaken for or coerced
# into the forbidden legacy 640x480 path. Variable extents remain guarded by
# the pure presentation contract.
FILE(GLOB_RECURSE _all_native_window_sources
	"${SOURCE_ROOT}/tests/*.c"
	"${SOURCE_ROOT}/tests/*.cc"
	"${SOURCE_ROOT}/tests/*.cpp"
	"${SOURCE_ROOT}/tests/*.m"
	"${SOURCE_ROOT}/tests/*.mm"
	"${SOURCE_ROOT}/code/tools/*.c"
	"${SOURCE_ROOT}/code/tools/*.cc"
	"${SOURCE_ROOT}/code/tools/*.cpp"
	"${SOURCE_ROOT}/code/tools/*.m"
	"${SOURCE_ROOT}/code/tools/*.mm")
FOREACH(_path IN LISTS _all_native_window_sources)
	FILE(RELATIVE_PATH _rel "${SOURCE_ROOT}" "${_path}")
	FILE(READ "${_path}" _body)
	STRING(REGEX MATCHALL
		"SDL_CreateWindow[(][^,]+,[ \t\r\n]*([0-9]+)[ \t]*,[ \t\r\n]*([0-9]+)[ \t]*,"
		_direct_windows "${_body}")
	FOREACH(_window IN LISTS _direct_windows)
		STRING(REGEX MATCH
			",[ \t\r\n]*([0-9]+)[ \t]*,[ \t\r\n]*([0-9]+)[ \t]*,$"
			_match "${_window}")
		SET(_window_w "${CMAKE_MATCH_1}")
		SET(_window_h "${CMAKE_MATCH_2}")
		WIRED_IS_16X9("${_window_w}" "${_window_h}" _wide)
		IF(NOT _wide OR _window_w LESS 1280 OR _window_h LESS 720)
			LIST(APPEND _violations
				"${_rel}: native host creates forbidden ${_window_w}x${_window_h}; test/tool windows must be exact 16:9 and at least 1280x720")
		ENDIF()
	ENDFOREACH()
ENDFOREACH()

# Indirect presentation-host tests do not spell SDL_CreateWindow at their call
# site. Keep their only window-owning successful create on the same canonical
# 1280x720 authority; invalid extents are tested through the pure predicate.
FILE(READ "${SOURCE_ROOT}/tests/ral_metal_module_test.mm" _metal_module_host)
STRING(FIND "${_metal_module_host}"
	"WiredSdlRalPresentationHost_Create( 1280u, 720u" _metal_module_extent)
IF(_metal_module_extent EQUAL -1)
	LIST(APPEND _violations
		"tests/ral_metal_module_test.mm: native presentation host is not canonical 1280x720")
ENDIF()

# The visible profile host bypasses the client cvars. Create it hidden, prove
# the requested logical size and physical 16:9 extent, publish a receipt, and
# only then show it. This prevents SDL/platform coercion or a stale 640x480
# default from flashing before the harness can reject it.
FILE(READ "${SOURCE_ROOT}/code/tools/profile_imgui/wired_profile_host_main.cpp"
	_profile_host_body)
FOREACH(_needle IN ITEMS
	"SDL_WINDOW_HIDDEN"
	"logicalWidth != 1280 || logicalHeight != 720"
	"static_cast<std::int64_t>( pixelWidth ) * 9"
	"RAL_PROFILE_HOST window-extent logical=%dx%d pixels=%dx%d exact16x9=1 publish-ready=1"
	"SDL_ShowWindow( host.window )")
	STRING(FIND "${_profile_host_body}" "${_needle}" _found)
	IF(_found EQUAL -1)
		LIST(APPEND _violations
			"code/tools/profile_imgui/wired_profile_host_main.cpp: fail-closed widescreen publication seam lost: ${_needle}")
	ENDIF()
ENDFOREACH()
STRING(FIND "${_profile_host_body}" "SDL_CreateWindow( \"Wired RAL Profile Host\"" _profile_create)
STRING(FIND "${_profile_host_body}" "RAL_PROFILE_HOST window-extent" _profile_receipt)
STRING(FIND "${_profile_host_body}" "SDL_ShowWindow( host.window )" _profile_show)
IF(_profile_create EQUAL -1 OR _profile_receipt EQUAL -1 OR _profile_show EQUAL -1
	OR NOT _profile_create LESS _profile_receipt OR NOT _profile_receipt LESS _profile_show)
	LIST(APPEND _violations
		"code/tools/profile_imgui/wired_profile_host_main.cpp: hidden create/receipt/show order is not fail-closed")
ENDIF()

# ---------------------------------------------------------------------------
# 6. Central watchdog preflight.
#
# Static harness scans are not enough when an older temporary copy of a script
# is executed.  The process-tree runner itself must reject a Wired GUI command
# before spawn unless the command carries an explicit exact-16:9 custom mode.
# ---------------------------------------------------------------------------
SET(_timeout_runner "${SOURCE_ROOT}/tests/run-with-timeout.py")
IF(NOT EXISTS "${_timeout_runner}")
	LIST(APPEND _violations "tests/run-with-timeout.py: central watchdog is missing")
ELSE()
	FILE(READ "${_timeout_runner}" _timeout_body)
	FOREACH(_needle IN ITEMS
		"def _wired_client_widescreen_error(command: Sequence[str])"
		"def _wired_client_binary_guard_error("
		"WIRED_WIDESCREEN_BINARY_MARKERS = ("
		"WIRED_PROFILE_HOST_WIDESCREEN_BINARY_MARKER = ("
		"Automated window request was not explicit 16:9; using 1280x720"
		"Window became non-16:9 (logical=%dx%d pixels=%dx%d); refusing it"
		"Automated window extent fell below 1280x720"
		"is_automated_gui = cvars.get(\"com_automated\") == \"1\" and not is_headless"
		"is_profile_host = executable.startswith(\"wired_profile_host\")"
		"profile host lacks hidden-until-validated 16:9 publication guard"
		"cvars.get(name) != expected"
		"width * 9 != height * 16"
		"width < 1280 or height < 720"
		"widescreen_error = _wired_client_widescreen_error(command)"
		"binary_guard_error = _wired_client_binary_guard_error(command, cwd)"
		"GUI binary lacks the current hidden-until-validated 16:9 guard"
		"renamed_bad_gui[0] = \"/tmp/client-under-test\""
		"refusing Wired GUI harness before spawn"
		"bad_gui[-4] = \"640\""
		"bad_gui[-1] = \"480\""
		"small_widescreen_gui[-1] = \"360\""
		"stale GUI binary without pre-show 16:9 guards was accepted")
		STRING(FIND "${_timeout_body}" "${_needle}" _needle_pos)
		IF(_needle_pos EQUAL -1)
			LIST(APPEND _violations
				"tests/run-with-timeout.py: launch-before-spawn widescreen guard lost: ${_needle}")
		ENDIF()
	ENDFOREACH()
ENDIF()

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
		"list may retain legacy entries, but the SDL runtime must refuse to open them;
the virtual 640x480 UI coordinate space remains out of scope.")
ENDIF()

MESSAGE(STATUS "W-103 widescreen policy: all resolution defaults, harness windows, and expected extents are 16:9")
