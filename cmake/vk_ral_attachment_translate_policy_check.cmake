FILE(READ "${ROOT}/code/renderervk/vk.c" VKC)
FILE(READ "${ROOT}/code/renderervk/vk_ral_textures.c" TEXTURES)

FOREACH(needle
    "VK_RalAttachmentContractFromVk( colorFormats, numColorFormats,"
    "ci.colorBlends    = attachments.numColorAttachments ? attachments.colorBlends : NULL;"
    "ci.numColorFormats = attachments.numColorAttachments;"
    "if ( p->numColorAttachments > 1 ) return NULL;"
    "if ( !VK_RalAttachmentContractCopy( p->exactAttachments, &attachments ) ) return NULL;"
    "VK_FORMAT_R16G16_SFLOAT:             return RAL_FORMAT_R16G16_SFLOAT;"
    "VK_FORMAT_R8_UNORM:                  return RAL_FORMAT_R8_UNORM;")
    STRING(FIND "${VKC}" "${needle}" pos)
    IF(pos EQUAL -1)
        MESSAGE(FATAL_ERROR "renderervk exact attachment seam missing: ${needle}")
    ENDIF()
ENDFOREACH()

STRING(REGEX MATCHALL "vk_ral_create_pipeline_from_gpinfo_exact\\(" exact_calls "${VKC}")
LIST(LENGTH exact_calls exact_call_count)
IF(NOT exact_call_count EQUAL 4)
    MESSAGE(FATAL_ERROR "generic scalar wrapper must be the only caller of the exact core (expected declaration+definition+2 branches)")
ENDIF()

STRING(REGEX MATCHALL "VK_RalAttachmentContractCopy\\(" special_copy_calls "${VKC}")
LIST(LENGTH special_copy_calls special_copy_call_count)
IF(NOT special_copy_call_count EQUAL 2)
    MESSAGE(FATAL_ERROR "special translator requires exactly two contract-copy branches")
ENDIF()
STRING(FIND "${VKC}" "if ( p->exactAttachments ) {" special_start)
IF(special_start EQUAL -1)
    MESSAGE(FATAL_ERROR "special exact attachment branch missing")
ENDIF()
STRING(SUBSTRING "${VKC}" ${special_start} 5000 special_body)
STRING(FIND "${special_body}" "ci.colorBlends = attachments.numColorAttachments" special_publish_blends)
STRING(FIND "${special_body}" "ci.numColorFormats = attachments.numColorAttachments" special_publish_formats)
IF(special_publish_blends EQUAL -1 OR special_publish_formats EQUAL -1)
    MESSAGE(FATAL_ERROR "special exact/scalar validation must precede attachment publication")
ENDIF()
FOREACH(stale IN ITEMS
    "ci.colorBlends = &blendAtt"
    "ci.colorFormats[0] = p->colorFormat"
    "ci.numColorFormats = ( p->numColorAttachments")
    STRING(FIND "${special_body}" "${stale}" stale_pos)
    IF(NOT stale_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "manual special attachment publication remains: ${stale}")
    ENDIF()
ENDFOREACH()
STRING(FIND "${VKC}" "blendAtt.writeMask" stale_generic_mask)
IF(NOT stale_generic_mask EQUAL -1)
    MESSAGE(FATAL_ERROR "stale single-attachment gpInfo translation remains")
ENDIF()

STRING(REGEX REPLACE "[ \t\r\n]" "" TEXTURES_COMPACT "${TEXTURES}")
STRING(FIND "${TEXTURES_COMPACT}"
    "bci.requestFeatures.wantIndependentBlend=qtrue;" request_pos)
STRING(FIND "${TEXTURES_COMPACT}" "Ral_CreateBackend(&bci)" create_pos)
IF(request_pos EQUAL -1 OR create_pos EQUAL -1 OR NOT request_pos LESS create_pos)
    MESSAGE(FATAL_ERROR "production RAL backend must request independentBlend before creation")
ENDIF()
