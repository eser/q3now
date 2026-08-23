# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_texture_cache.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_texture_cache.c" CORE)
file(READ "${ROOT}/tests/ral_texture_cache_test.c" TEST)
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${TEST}")
	foreach(FORBIDDEN IN ITEMS "FILE *" "Vk" "WGPU" "MTL")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "texture cache leaked platform/backend type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_TEXTURE_CACHE_KEY_SCHEMA_VERSION"
	"ralTextureCacheOps_t" "writeAtomic" "Ral_TextureArtifactCacheStore"
	"Ral_TextureArtifactCacheLoad")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture cache public contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "a->schemaVersion" "a->assetGeneration"
	"a->provenanceHash" "a->dimension" "a->width" "a->height" "a->depth"
	"a->layers" "a->sourceMipLevels" "a->targetMipLevels" "a->colorEncoding"
	"a->channelSemantic" "a->sourceEncoding" "a->mipPolicy" "a->residency"
	"a->hasAlpha" "a->selectedCompression" "a->targetFormat"
	"a->transcodeRequired" "a->deterministicFallback" "wrtex-v1-"
	"Ral_DecodeTextureArtifact" "malloc" "free")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture cache key/atomic load lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "failWrite" "truncateRead" "corruptRead"
	"stale.provenanceHash++" "artifact.containerByteLength - 1u"
	"!memcmp( loaded, before")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "texture cache hostile fixture lost: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL texture cache policy: PASS")
