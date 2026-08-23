# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_texture_transcode.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_texture_transcode.c" CORE)
file(READ "${ROOT}/tests/ral_texture_transcode_test.c" TEST)
foreach(FORBIDDEN IN ITEMS "VkFormat" "MTLPixelFormat" "WGPUTextureFormat" "ktx.h")
	string(FIND "${HEADER}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "texture transcode public surface leaked: ${FORBIDDEN}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "#include <ktx.h>" "ktxTexture2_CreateFromMemory"
	"ktxTexture2_TranscodeBasis" "Ral_Ktx2ReceiptExact"
	"Ral_EncodeTextureArtifact" "KTX_TTF_RGBA32")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture transcode adapter lost seam: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RalEtc1sFixtureBase64" "MakeUastcFixture"
	"RAL_KTX2_PAYLOAD_BASIS_ETC1S" "RAL_KTX2_PAYLOAD_BASIS_UASTC"
	"RAL_FORMAT_BC7_SRGB" "RAL_FORMAT_R8G8B8A8_SRGB"
	"!memcmp( artifactA, artifactB" "!memcmp( output, before")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture transcode fixture lost contract: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL texture transcode policy: PASS")
