# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/core/ral_texture_asset.h" HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_texture_asset.c" CORE)
file(READ "${ROOT}/code/render/ral/core/ral_texture_transcode.h" TRANSCODE_HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_texture_transcode.c" TRANSCODE_CORE)
file(READ "${ROOT}/tests/ral_texture_asset_test.c" TEST)
file(READ "${ROOT}/tests/ral_texture_transcode_test.c" TRANSCODE_TEST)
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${TRANSCODE_HEADER}" "${TEST}")
	foreach(FORBIDDEN IN ITEMS "VkFormat" "MTLPixelFormat" "WGPUTextureFormat")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "texture asset policy leaked backend type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
string(FIND "${TRANSCODE_HEADER}" "ktx.h" KTX_PUBLIC_POS)
if(NOT KTX_PUBLIC_POS EQUAL -1)
	message(FATAL_ERROR "texture transcode public surface leaked libktx")
endif()
foreach(NEEDLE IN ITEMS "#include <ktx.h>" "ktxTexture2_CreateFromMemory"
	"ktxTexture2_TranscodeBasis" "Ral_Ktx2ReceiptExact"
	"Ral_EncodeTextureArtifact" "KTX_TTF_RGBA32")
	string(FIND "${TRANSCODE_CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture transcode adapter lost seam: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RalEtc1sFixtureBase64" "MakeUastcFixture"
	"RAL_KTX2_PAYLOAD_BASIS_ETC1S" "RAL_KTX2_PAYLOAD_BASIS_UASTC"
	"RAL_FORMAT_BC7_SRGB" "RAL_FORMAT_R8G8B8A8_SRGB"
	"!memcmp( artifactA, artifactB" "!memcmp( output, before")
	string(FIND "${TRANSCODE_TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture transcode fixture lost contract: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "provenanceHash" "channelSemantic" "mipPolicy"
	"residency" "RAL_TEXTURE_COMPRESSION_BC" "RAL_TEXTURE_COMPRESSION_ASTC"
	"RAL_TEXTURE_COMPRESSION_ETC2" "deterministicFallback"
	"RAL_KTX2_SCHEMA_VERSION" "RAL_KTX2_MAX_LEVELS"
	"RAL_KTX2_PAYLOAD_BASIS_ETC1S" "RAL_KTX2_PAYLOAD_BASIS_UASTC"
	"ralKtx2LevelReceipt_t" "Ral_ParseKtx2"
	"RAL_TEXTURE_ARTIFACT_SCHEMA_VERSION" "ralTextureArtifactReceipt_t"
	"Ral_EncodeTextureArtifact" "Ral_DecodeTextureArtifact")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture asset policy lost contract field: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "TextureArtifactMagic" "TextureArtifactHash"
	"RAL_TEXTURE_ARTIFACT_FIXED_HEADER" "WriteLE32" "WriteLE64"
	"Ral_TextureAssetReceiptExact( &v.asset, expectedAsset )"
	"Ral_TextureArtifactReceiptValid")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture asset policy lost cache artifact seam: ${NEEDLE}")
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
foreach(NEEDLE IN ITEMS "artifactLevels[3]" "Ral_EncodeTextureArtifact"
	"Ral_DecodeTextureArtifact" "Ral_TextureArtifactReceiptExact"
	"ResealArtifact" "artifactBytes - 1u" "staleAsset.targetFormat"
	"!memcmp( transported, artifactBefore" "!memcmp( &decodedArtifact")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture asset fixture lost artifact round-trip/mutation: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL texture asset policy: PASS")
