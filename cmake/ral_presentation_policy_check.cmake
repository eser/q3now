# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/renderer/ral/ral_presentation_policy.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_presentation_policy.c" CORE)
file(READ "${ROOT}/code/renderervk/vk.c" VULKAN)
file(READ "${ROOT}/code/renderer/ral_metal/ral_metal_module.mm" METAL)
file(READ "${ROOT}/tests/ral_presentation_policy_test.c" TEST)
file(READ "${ROOT}/tests/fps-perf-analyze.py" ANALYZER)
file(READ "${ROOT}/CMakeLists.txt" BUILD)

function(require_text TEXT NEEDLE LABEL)
	string(FIND "${TEXT}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "RAL presentation policy lost ${LABEL}: ${NEEDLE}")
	endif()
endfunction()

foreach(NEEDLE IN ITEMS
	"RAL_PRESENTATION_POLICY_SCHEMA_VERSION"
	"ralPresentationPolicyRequest_t"
	"ralPresentationPolicyReceipt_t"
	"maxFramesInFlight"
	"allowTearingWhenLate"
	"preferVrr")
	require_text("${HEADER}" "${NEEDLE}" "native-free contract")
endforeach()
foreach(NEEDLE IN ITEMS
	"policy_intent"
	"frames_in_flight"
	"vrr_preferred")
	require_text("${ANALYZER}" "${NEEDLE}" "pacing evidence parser")
endforeach()
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${TEST}")
	foreach(FORBIDDEN IN ITEMS "VkPresentMode" "MTLDevice" "WGPUPresentMode")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "RAL presentation policy leaked backend type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS
	"Ral_PresentationPolicyRequestFromLegacySwapInterval"
	"Ral_ResolvePresentationPolicy"
	"Ral_FinalizePresentationPolicy"
	"presentationReceipt.presentationGeneration")
	require_text("${VULKAN}" "${NEEDLE}" "Vulkan consumer/measurement join")
endforeach()
require_text("${METAL}" "Ral_ResolvePresentationPolicy" "Metal shared resolver")
foreach(NEEDLE IN ITEMS
	"RAL_PRESENTATION_INTENT_UNLOCKED"
	"RAL_PRESENTATION_INTENT_SYNCHRONIZED"
	"RAL_PRESENTATION_INTENT_LOW_LATENCY_SYNCHRONIZED"
	"WebGPU-shaped safe default"
	"Ral_PresentationPolicyReceiptExact")
	require_text("${TEST}" "${NEEDLE}" "headless fixture")
endforeach()
foreach(NEEDLE IN ITEMS
	"ADD_EXECUTABLE(ral_presentation_policy_test"
	"code/renderer/ral/ral_presentation_policy.c")
	require_text("${BUILD}" "${NEEDLE}" "build authority")
endforeach()

message(STATUS "RAL presentation policy: PASS")
