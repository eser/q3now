# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_color_output.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_color_output.c" CORE)
file(READ "${ROOT}/code/renderervk/vk.c" VULKAN)
file(READ "${ROOT}/code/renderervk/shaders/gamma.frag" GAMMA)
file(READ "${ROOT}/code/renderer/ral_metal/ral_metal_module.mm" METAL)
file(READ "${ROOT}/tests/ral_color_output_test.c" TEST)
file(READ "${ROOT}/tests/ral-readback-runtime-check.sh" VISUAL)
file(READ "${ROOT}/CMakeLists.txt" BUILD)

function(require_text TEXT NEEDLE LABEL)
	string(FIND "${TEXT}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "RAL color output lost ${LABEL}: ${NEEDLE}")
	endif()
endfunction()

foreach(NEEDLE IN ITEMS "RAL_COLOR_OUTPUT_SCHEMA_VERSION" "sceneFormat"
	"uiReferenceWhiteNits" "presentationTransfer" "screenshotTransfer"
	"readbackTransfer" "toneMapOperator" "lutEnabled" "hdrFallback")
	require_text("${HEADER}" "${NEEDLE}" "native-free receipt")
endforeach()
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${TEST}")
	foreach(FORBIDDEN IN ITEMS "VkColorSpace" "MTLColor" "WGPUTextureFormat")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "RAL color output leaked backend type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "vk_color_output_receipt" "Ral_ResolveColorOutput"
	"colorRequest.presentationGeneration = actual.generation"
	"colorRequest.selectedOutput.format = actual.format"
	"capture_hdr_to_sdr"
	"program_index != 3")
	require_text("${VULKAN}" "${NEEDLE}" "Vulkan color/presentation join")
endforeach()
foreach(NEEDLE IN ITEMS "Stable display-referred screenshot/readback target"
	"RAL_TEXTURE_USAGE_COLOR_ATTACHMENT"
	"RAL_TEXTURE_USAGE_TRANSFER_SRC"
	"source = vk.capture.ral_image;")
	require_text("${VULKAN}" "${NEEDLE}" "stable display-referred capture")
endforeach()
foreach(NEEDLE IN ITEMS "constant_id = 14" "capture_hdr_to_sdr"
	"tonemapPBRNeutralSDR")
	require_text("${GAMMA}" "${NEEDLE}" "HDR-to-sRGB capture conversion")
endforeach()
foreach(NEEDLE IN ITEMS "BuildColorOutputReceipt" "Ral_ResolveColorOutput"
	"s_module.colorOutputReceipt = colorOutput")
	require_text("${METAL}" "${NEEDLE}" "Metal color/presentation join")
endforeach()
foreach(NEEDLE IN ITEMS "RAL_TONEMAP_PBR_NEUTRAL" "RAL_TONEMAP_AGX"
	"RAL_HDR_FALLBACK_OUTPUT_UNAVAILABLE" "RAL_COLOR_TRANSFER_PQ"
	"Ral_ColorOutputReceiptExact" "!memcmp( &receipt, &before")
	require_text("${TEST}" "${NEEDLE}" "headless fixture")
endforeach()
foreach(NEEDLE IN ITEMS "WIRED_HDR_DISPLAY_REQUEST" "WIRED_TONEMAP_OPERATOR"
	"+set r_hdrDisplay \"$HDR_REQUEST\"" "+set r_tonemap \"$TONEMAP_OPERATOR\""
	"log renderer.init debug" "log renderer.hdr debug"
	"exact color-output receipt" "mixed HDR/SDR color state"
	"screenshot=sRGB" "readback=sRGB" "r_customwidth 1280"
	"r_customheight 720")
	require_text("${VISUAL}" "${NEEDLE}" "bounded native color/readback evidence")
endforeach()
require_text("${BUILD}" "ADD_EXECUTABLE(ral_color_output_test" "build authority")

message(STATUS "RAL color output: PASS")
