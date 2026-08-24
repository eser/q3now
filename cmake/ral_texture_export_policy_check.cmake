# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/core/ral_texture_export.h" HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_texture_export.c" CORE)
file(READ "${ROOT}/tests/ral_texture_export_test.c" TEST)
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${TEST}")
	foreach(FORBIDDEN IN ITEMS "Vk" "WGPU" "MTL" "SDL_")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "texture export leaked native type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_TEXTURE_EXPORT_SCHEMA_VERSION"
	"ralTextureExportReceipt_t" "ralTextureReadbackPlan_t"
	"ralTextureReadbackResult_t" "Ral_TextureReadbackRegionFromPlan")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture export contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_TEXTURE_USAGE_TRANSFER_SRC" "offsetZ"
	"textureAspects" "stagingBytesPerRow" "FNV64_PRIME")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture export/readback join lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "sizeof( mapped ) - 1u" "wrongReadback.transfer.request.mipLevel++"
	"resource.resourceGeneration++" "RAL_TEXTURE_ASPECT_STENCIL"
	"volumePlan.transfer.offsetZ == 2u")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture export hostile fixture lost: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL texture export/readback policy: PASS")
