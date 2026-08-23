# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_texture_asset.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_texture_asset.c" CORE)
file(READ "${ROOT}/tests/ral_texture_asset_test.c" TEST)
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${TEST}")
	foreach(FORBIDDEN IN ITEMS "VkFormat" "MTLPixelFormat" "WGPUTextureFormat")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "texture asset policy leaked backend type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "provenanceHash" "channelSemantic" "mipPolicy"
	"residency" "RAL_TEXTURE_COMPRESSION_BC" "RAL_TEXTURE_COMPRESSION_ASTC"
	"RAL_TEXTURE_COMPRESSION_ETC2" "deterministicFallback"
	"RAL_KTX2_SCHEMA_VERSION" "RAL_KTX2_MAX_LEVELS"
	"RAL_KTX2_PAYLOAD_BASIS_ETC1S" "RAL_KTX2_PAYLOAD_BASIS_UASTC"
	"ralKtx2LevelReceipt_t" "Ral_ParseKtx2")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture asset policy lost contract field: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "Ktx2Identifier" "ReadLE32" "ReadLE64"
	"RangeValid" "RangesOverlap" "colorModel == 163u" "colorModel == 166u"
	"scheme != 0u && scheme != 2u" "Ral_Ktx2ReceiptValid")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture asset policy lost KTX2 admission: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_FORMAT_BC7_SRGB" "RAL_FORMAT_BC5_UNORM"
	"RAL_FORMAT_BC6H_UFLOAT" "RAL_FORMAT_ASTC_4x4_SRGB"
	"RAL_FORMAT_ETC2_R8G8B8A8_SRGB" "RAL_FORMAT_R8G8B8A8_SRGB"
	"!memcmp( &receipt, &before")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture asset fixture lost selection/mutation: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "MakeKtx2" "RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ"
	"RAL_TEXTURE_ASSET_CUBE" "RAL_TEXTURE_ASSET_2D_ARRAY"
	"RAL_TEXTURE_ASSET_3D" "Put64( corrupt + 80, UINT64_MAX )"
	"!memcmp( &ktxReceipt, &ktxBefore")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture asset fixture lost KTX2 mutation: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL texture asset policy: PASS")
