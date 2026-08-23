# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

# Modern video-mode defaults without restricting explicit user choices.
# Harnesses use a modern 16:9 extent for reproducible evidence, while r_mode
# and r_mode -1 remain free to select any positive width and height.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FUNCTION(REQUIRE_TEXT _body _needle _description)
	STRING(FIND "${${_body}}" "${_needle}" _offset)
	IF(_offset EQUAL -1)
		MESSAGE(FATAL_ERROR "video-mode policy: missing ${_description}")
	ENDIF()
ENDFUNCTION()

FUNCTION(FORBID_TEXT _body _needle _description)
	STRING(FIND "${${_body}}" "${_needle}" _offset)
	IF(NOT _offset EQUAL -1)
		MESSAGE(FATAL_ERROR "video-mode policy: forbidden ${_description}")
	ENDIF()
ENDFUNCTION()

FILE(READ "${SOURCE_ROOT}/code/client/cl_main.c" CLIENT)
FILE(READ "${SOURCE_ROOT}/code/client/cl_display_catalog.c" CATALOG)
FILE(READ "${SOURCE_ROOT}/code/sdl/sdl_glimp.c" SDL)
FILE(READ "${SOURCE_ROOT}/tests/ral_sdl_presentation_host_test.c" SDL_HOST)
FILE(READ "${SOURCE_ROOT}/tests/wired_metal_smoke.mm" METAL_SMOKE)
FILE(READ "${SOURCE_ROOT}/tests/ral_backend_conformance_test.mm" CONFORMANCE)
FILE(READ "${SOURCE_ROOT}/tests/ral_metal_module_test.mm" METAL_MODULE)

# Preserve established numeric modes and add current desktop/laptop presets.
REQUIRE_TEXT(CATALOG "Mode  3: 640x480" "legacy 640x480 user preset")
REQUIRE_TEXT(CATALOG "Mode 13: 1280x720" "720p preset")
REQUIRE_TEXT(CATALOG "Mode 19: 1920x1080" "1080p preset")
REQUIRE_TEXT(CATALOG "Mode 22: 3440x1440 (21:9)" "ultrawide preset")
REQUIRE_TEXT(CATALOG "Mode 23: 3840x2160" "4K preset")
REQUIRE_TEXT(CATALOG "Mode 25: 2560x1440" "QHD preset")
REQUIRE_TEXT(CATALOG "Mode 26: 2560x1600 (16:10)" "modern 16:10 preset")
REQUIRE_TEXT(CATALOG "Mode 27: 3840x1600" "ultrawide 1600p preset")
REQUIRE_TEXT(CATALOG "Mode 28: 5120x1440" "dual-QHD preset")
REQUIRE_TEXT(CATALOG "Mode 29: 5120x2160" "5K ultrawide preset")
REQUIRE_TEXT(CATALOG "Mode 30: 5120x2880" "5K preset")
REQUIRE_TEXT(CATALOG "Mode 31: 7680x4320" "8K preset")

# The only generic extent validation is positivity; aspect ratio is user-owned.
REQUIRE_TEXT(CLIENT "WiredDisplay_ResolveLegacyMode" "catalog-backed compatibility resolution")
REQUIRE_TEXT(CATALOG "customWidth == 0u || customHeight == 0u" "positive custom extent validation")
FORBID_TEXT(CLIENT "(int64_t)*width * 9 != (int64_t)*height * 16" "aspect-ratio rejection in CL_GetModeInfo")
FORBID_TEXT(CATALOG "height * 16" "aspect-ratio rejection in display catalog")
FORBID_TEXT(SDL "window mode %dx%d is not exact 16:9; refusing it" "pre-create aspect-ratio rejection")
FORBID_TEXT(SDL "Window became non-16:9" "post-create aspect-ratio rejection")
FORBID_TEXT(SDL "Automated window extent fell below 1280x720" "minimum-size rejection")
FORBID_TEXT(SDL "Automated window request was not explicit 16:9" "automated mode override")
FORBID_TEXT(SDL "WIRED_CLIENT_WINDOW_GUARD" "binary policy marker")
REQUIRE_TEXT(SDL "window-extent schema=3 requested=%dx%d logical=%dx%d pixels=%dx%d publish-ready=1" "aspect-neutral extent receipt")

# Window-opening harnesses choose a modern deterministic default themselves.
REQUIRE_TEXT(SDL_HOST "1280u, 720u" "1280x720 SDL presentation harness default")
REQUIRE_TEXT(METAL_SMOKE "desiredWidth = 1280u" "1280-wide Metal smoke default")
REQUIRE_TEXT(METAL_SMOKE "desiredHeight = 720u" "720-high Metal smoke default")
REQUIRE_TEXT(CONFORMANCE "1280, 720" "1280x720 conformance harness default")
REQUIRE_TEXT(METAL_MODULE "1280u, 720u" "1280x720 Metal module harness default")
FOREACH(_harness SDL_HOST METAL_SMOKE CONFORMANCE METAL_MODULE)
	FORBID_TEXT(${_harness} "WIRED_NATIVE_WINDOW_GUARD" "native binary marker")
ENDFOREACH()

MESSAGE(STATUS "video-mode policy: modern presets present; explicit user extents remain unrestricted")
