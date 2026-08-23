# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_texture_upload.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_texture_upload.c" CORE)
file(READ "${ROOT}/tests/ral_texture_upload_test.c" TEST)
file(READ "${ROOT}/tests/ral_vulkan_texture_copy_test.c" VULKAN_TEST)
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${TEST}")
	foreach(FORBIDDEN IN ITEMS "VkBufferImageCopy" "VkFormat" "WGPUImageCopyTexture")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "texture upload plan leaked backend type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_TEXTURE_UPLOAD_ALIGNMENT" "ralTextureUploadPlan_t"
	"ralBufferTextureCopy_t" "Ral_BuildTextureArtifactUploadPlan"
	"Ral_PackTextureArtifactUpload")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture upload public contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "FormatBlock" "TextureFacts" "BuildLevel"
	"Ral_DecodeTextureArtifact" "RAL_TEXTURE_CUBE_ARRAY"
	"region.bytesPerRow" "concurrentGraphicsTransfer")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture upload implementation lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_TEXTURE_ASSET_2D_ARRAY" "RAL_TEXTURE_ASSET_CUBE"
	"RAL_TEXTURE_ASSET_3D" "RAL_TEXTURE_COMPRESSION_BC"
	"RAL_TEXTURE_COMPRESSION_ASTC" "plan.stagingByteLength - 1u"
	"!memcmp( staging, before")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture upload fixture lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "Ral_BuildTextureArtifactUploadPlan"
	"Ral_PackTextureArtifactUpload" "plan.levels[0].region"
	"capturedUpload[1].bufferOffset == plan.levels[1].stagingOffset")
	string(FIND "${VULKAN_TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture upload Vulkan lowering lost: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL texture upload policy: PASS")
