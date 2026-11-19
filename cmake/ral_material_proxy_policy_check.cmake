# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/core/ral_material_proxy.h" HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_material_proxy.c" CORE)
file(READ "${ROOT}/code/render/ral/core/ral_material_proxy_lower.h" LOWER_HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_material_proxy_lower.c" LOWER)
file(READ "${ROOT}/tests/ral_material_proxy_test.c" TEST)
file(READ "${ROOT}/tests/ral_material_proxy_lower_test.c" LOWER_TEST)
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${LOWER_HEADER}" "${LOWER}"
		"${TEST}" "${LOWER_TEST}")
	foreach(FORBIDDEN IN ITEMS "Vk" "WGPU" "MTL" "SDL_" "sin(" "malloc(" "calloc(" "realloc(")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "material proxy leaked native/nondeterministic API: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_MATERIAL_PROXY_Q16_ONE" "RAL_MATERIAL_PROXY_SINE"
	"ralMaterialProxyEvaluationReceipt_t" "RAL_MATERIAL_PROXY_DOMAIN_PREVIEW"
	"RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE_CONSTANT"
	"RAL_MATERIAL_EXPRESSION_TABLE" "RAL_MATERIAL_EXPRESSION_OPCODE_COUNT"
	"ralMaterialExpressionInstructionMustRemainEightBytes"
	"ralMaterialExpressionField_t")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material proxy contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "sineQ16" "hasWriter" "ProgramHash" "capacity<RAL_MATERIAL_PROXY_VALUE_COUNT"
	"ExpressionProgramValid" "EvaluateExpression" "ExpressionTableLane")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material proxy invariant lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_MATERIAL_PROXY_DOMAIN_RUNTIME" "RAL_MATERIAL_PROXY_DOMAIN_PREVIEW"
	"RAL_MATERIAL_PROXY_VALUE_COUNT-1u" "stale.artifactHash++" "INT32_MAX"
	"RAL_MATERIAL_EXPRESSION_OPCODE_COUNT" "RAL_MATERIAL_PROXY_LEGACY_SCHEMA_VERSION"
	"expressionFields[2].name")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material proxy hostile fixture lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "Ral_MaterialProxyLowerSource" "Q30_SIN_STEP"
	"RAL_MATERIAL_TCMOD_SCROLL" "RAL_MATERIAL_TCMOD_ROTATE"
	"RAL_MATERIAL_TCMOD_STRETCH" "RAL_MATERIAL_TCMOD_TRANSFORM"
	"RAL_MATERIAL_DEFORM_WAVE" "RAL_MATERIAL_DEFORM_MOVE"
	"RAL_MATERIAL_WAVE_NOISE")
	string(FIND "${LOWER_HEADER}${LOWER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material proxy authored lowering lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "stage0.rgbWave" "stage0.tc0" "stage0.tc3.tail"
	"deform0.wave" "RAL_MATERIAL_PROXY_LOWER_INVALID_MATERIAL")
	string(FIND "${LOWER_TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material proxy lowering fixture lost: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL material proxy policy: PASS")
