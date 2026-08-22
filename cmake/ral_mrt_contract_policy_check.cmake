FILE(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_backend.c" BACKEND)
FILE(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_caps.c" CAPS)
FILE(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_pipeline.c" PIPELINE)
FILE(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" RESOURCE)
FILE(READ "${ROOT}/code/renderer/ral/ral_backend.h" RAL_BACKEND_H)
FILE(READ "${ROOT}/code/renderer/ral/ral_resource.h" RAL_RESOURCE_H)
FILE(READ "${ROOT}/code/renderer/ral/ral_types.h" RAL_TYPES_H)
FILE(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_mrt_policy.c" MRT_POLICY)
FILE(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_translate.c" TRANSLATE)
FILE(READ "${ROOT}/code/renderervk/vk.c" VK_PRODUCT)
FILE(READ "${ROOT}/tests/ral_mrt_contract_test.c" MRT_TEST)

FOREACH(needle
    "ci->requestFeatures.wantIndependentBlend"
    "f2enable.features.independentBlend = VK_TRUE"
    "b->haveIndependentBlend = qfalse")
    STRING(FIND "${BACKEND}" "${needle}" pos)
    IF(pos EQUAL -1)
        MESSAGE(FATAL_ERROR "RAL MRT contract missing backend seam: ${needle}")
    ENDIF()
ENDFOREACH()

# There are exactly two RAL-owned device-creation branches: the production
# owned-instance/device path uses f2support, and the legacy standalone path
# uses f2query. Pin the full operands so qtrue->qfalse, request substitution,
# or an unrelated feature-support bit cannot false-green this policy.
STRING(REGEX REPLACE "[ \t\r\n]" "" BACKEND_COMPACT "${BACKEND}")
STRING(REGEX MATCHALL "ralVk_IndependentBlendEnabled\\(" owned_helper_calls "${BACKEND_COMPACT}")
LIST(LENGTH owned_helper_calls owned_helper_call_count)
IF(NOT owned_helper_call_count EQUAL 2)
    MESSAGE(FATAL_ERROR "RAL MRT requires exactly two owned independentBlend helper calls")
ENDIF()
SET(expected_owned_primary
    "ralVk_IndependentBlendEnabled(qtrue,ci->requestFeatures.wantIndependentBlend,f2support.features.independentBlend)")
SET(expected_owned_standalone
    "ralVk_IndependentBlendEnabled(qtrue,ci->requestFeatures.wantIndependentBlend,f2query.features.independentBlend)")
FOREACH(expected IN ITEMS "${expected_owned_primary}" "${expected_owned_standalone}")
    STRING(FIND "${BACKEND_COMPACT}" "${expected}" expected_pos)
    IF(expected_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "RAL MRT owned feature gate operands changed: ${expected}")
    ENDIF()
ENDFOREACH()

STRING(FIND "${CAPS}" "c->independentBlend         = b->haveIndependentBlend" caps_pos)
IF(caps_pos EQUAL -1)
    MESSAGE(FATAL_ERROR "RAL MRT contract does not mirror enabled independentBlend into caps")
ENDIF()
STRING(REGEX MATCHALL "c->independentBlend[ \t]*=" caps_writes "${CAPS}")
LIST(LENGTH caps_writes caps_write_count)
IF(NOT caps_write_count EQUAL 1)
    MESSAGE(FATAL_ERROR "RAL MRT independentBlend cap must have one authoritative assignment")
ENDIF()
STRING(REGEX MATCHALL "b->caps\.independentBlend[ \t]*=" backend_caps_writes "${BACKEND}")
LIST(LENGTH backend_caps_writes backend_caps_write_count)
IF(NOT backend_caps_write_count EQUAL 0)
    MESSAGE(FATAL_ERROR "RAL MRT backend must not overwrite the FillCaps authority")
ENDIF()

STRING(REGEX MATCHALL "uint64_t[ \t]+maxStorageBufferRange" storage_tail_fields "${RAL_BACKEND_H}")
LIST(LENGTH storage_tail_fields storage_tail_field_count)
IF(NOT storage_tail_field_count EQUAL 1)
    MESSAGE(FATAL_ERROR "RAL caps must expose one uint64_t maxStorageBufferRange field")
ENDIF()
STRING(FIND "${RAL_BACKEND_H}"
    "qboolean independentBlend;          // append-only: per-colour-attachment blend/write-mask state enabled\n\tuint64_t maxStorageBufferRange;"
    storage_tail_pos)
IF(storage_tail_pos EQUAL -1)
    MESSAGE(FATAL_ERROR "maxStorageBufferRange must remain the terminal append-only cap after independentBlend")
ENDIF()
STRING(REGEX MATCHALL "c->maxStorageBufferRange[ \t]*=" storage_cap_writes "${CAPS}")
LIST(LENGTH storage_cap_writes storage_cap_write_count)
IF(NOT storage_cap_write_count EQUAL 1)
    MESSAGE(FATAL_ERROR "maxStorageBufferRange needs one FillCaps authority")
ENDIF()
STRING(FIND "${CAPS}"
    "c->maxStorageBufferRange     = (uint64_t)L->maxStorageBufferRange;"
    storage_assignment_pos)
IF(storage_assignment_pos EQUAL -1)
    MESSAGE(FATAL_ERROR "FillCaps maxStorageBufferRange assignment drifted")
ENDIF()
STRING(FIND "${BACKEND}" "ralVk_FillCaps( b );" fill_caps_pos)
IF(fill_caps_pos EQUAL -1)
	MESSAGE(FATAL_ERROR "usable Vulkan backend must converge through one FillCaps call")
ENDIF()
STRING(LENGTH "${BACKEND}" backend_length)
MATH(EXPR fill_caps_tail_pos "${fill_caps_pos}+21")
MATH(EXPR fill_caps_tail_length "${backend_length}-${fill_caps_tail_pos}")
STRING(SUBSTRING "${BACKEND}" ${fill_caps_tail_pos} ${fill_caps_tail_length} fill_caps_tail)
STRING(FIND "${fill_caps_tail}" "ralVk_FillCaps( b );" second_fill_caps_pos)
IF(NOT second_fill_caps_pos EQUAL -1)
	MESSAGE(FATAL_ERROR "usable Vulkan backend has multiple FillCaps authorities")
ENDIF()
STRING(FIND "${BACKEND}"
    "maxStorageBufferRange   : %llu bytes\\n\", (unsigned long long)c->maxStorageBufferRange"
    storage_diag_pos)
IF(storage_diag_pos EQUAL -1)
    MESSAGE(FATAL_ERROR "maxStorageBufferRange diagnostic format/cast drifted")
ENDIF()
STRING(FIND "${PIPELINE}" "ralVk_ColorWriteMask( src )" mask_pos)
IF(mask_pos EQUAL -1)
    MESSAGE(FATAL_ERROR "RAL pipeline does not use the exact write-mask policy")
ENDIF()
STRING(FIND "${PIPELINE}"
    "if ( !ralVk_ColorBlendStatesSupported( ci, b->caps.independentBlend ) ) {" blend_gate_pos)
STRING(FIND "${PIPELINE}" "// ── pipeline layout" layout_pos)
STRING(FIND "${PIPELINE}" "modVert = ralVk_MakeShaderModule" shader_pos)
IF(blend_gate_pos EQUAL -1 OR layout_pos EQUAL -1 OR shader_pos EQUAL -1)
    MESSAGE(FATAL_ERROR "RAL MRT pipeline fail-closed gate/order anchors are missing")
ENDIF()
IF(NOT blend_gate_pos LESS layout_pos OR NOT blend_gate_pos LESS shader_pos)
    MESSAGE(FATAL_ERROR "RAL MRT blend gate must run before layout and shader creation")
ENDIF()
STRING(SUBSTRING "${PIPELINE}" ${blend_gate_pos} 500 gate_body)
STRING(FIND "${gate_body}" "return NULL;" gate_return_pos)
IF(gate_return_pos EQUAL -1)
    MESSAGE(FATAL_ERROR "RAL MRT incompatible blend gate does not immediately reject")
ENDIF()
STRING(FIND "${RESOURCE}" "ralVk_TextureUsage( ci->usage )" usage_pos)
IF(usage_pos EQUAL -1)
    MESSAGE(FATAL_ERROR "RAL texture creation no longer shares the audited usage expansion")
ENDIF()

# Per-format capabilities preserve the backend-neutral distinctions shared by
# Vulkan, Metal and WebGPU: sampled/filterable and renderable/blendable are not
# interchangeable claims.
FOREACH(feature IN ITEMS
    SAMPLED FILTER_LINEAR STORAGE COLOR_ATTACHMENT COLOR_ATTACHMENT_BLEND
    DEPTH_STENCIL_ATTACHMENT TRANSFER_SRC TRANSFER_DST)
    STRING(REGEX MATCHALL "RAL_TEXTURE_FORMAT_FEATURE_${feature}[ 	]*=" feature_defs "${RAL_RESOURCE_H}")
    LIST(LENGTH feature_defs feature_def_count)
    IF(NOT feature_def_count EQUAL 1)
        MESSAGE(FATAL_ERROR "RAL texture-format feature ${feature} needs one public definition")
    ENDIF()
ENDFOREACH()
FOREACH(needle IN ITEMS
    "features == 0u || ( features & ~RAL_TEXTURE_FORMAT_FEATURE_ALL ) != 0u"
    "required = ralVk_TextureFormatFeatures( features );"
    "props.optimalTilingFeatures & required"
    "return Ral_TextureFormatSupportsFeatures( b, format, required );")
    STRING(FIND "${MRT_POLICY}" "${needle}" format_policy_pos)
    IF(format_policy_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "RAL exact texture-format feature gate drifted: ${needle}")
    ENDIF()
ENDFOREACH()
FOREACH(mapping IN ITEMS
    "RAL_TEXTURE_FORMAT_FEATURE_SAMPLED:VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT"
    "RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR:VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT"
    "RAL_TEXTURE_FORMAT_FEATURE_STORAGE:VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT"
    "RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT:VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT"
    "RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND:VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT"
    "RAL_TEXTURE_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT:VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT"
    "RAL_TEXTURE_FORMAT_FEATURE_TRANSFER_SRC:VK_FORMAT_FEATURE_TRANSFER_SRC_BIT"
    "RAL_TEXTURE_FORMAT_FEATURE_TRANSFER_DST:VK_FORMAT_FEATURE_TRANSFER_DST_BIT")
    STRING(REPLACE ":" ";" mapping_parts "${mapping}")
    LIST(GET mapping_parts 0 ral_feature)
    LIST(GET mapping_parts 1 vk_feature)
    SET(mapping_body "if ( features & ${ral_feature} )\n\t\tv |= ${vk_feature};")
    STRING(FIND "${MRT_POLICY}" "${mapping_body}" mapping_pos)
    IF(mapping_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "Vulkan texture-format feature mapping drifted: ${mapping}")
    ENDIF()
ENDFOREACH()
FOREACH(hdr_seam IN ITEMS
    "const ralTextureFormatFeatures_t required ="
    "RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND"
    "RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR;"
    "Ral_TextureFormatSupportsFeatures( backend,"
    "RAL_FORMAT_R16G16B16A16_SFLOAT, required );")
    STRING(FIND "${VK_PRODUCT}" "${hdr_seam}" hdr_pos)
    IF(hdr_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "HDR product format-feature join drifted: ${hdr_seam}")
    ENDIF()
ENDFOREACH()
STRING(REGEX MATCHALL "qvkGetPhysicalDeviceFormatProperties[(][ 	]*physical_device,[ 	]*VK_FORMAT_R16G16B16A16_SFLOAT" raw_hdr_queries "${VK_PRODUCT}")
LIST(LENGTH raw_hdr_queries raw_hdr_query_count)
IF(NOT raw_hdr_query_count EQUAL 0)
    MESSAGE(FATAL_ERROR "HDR reacquired a renderer-local Vulkan format query")
ENDIF()
FOREACH(test_needle IN ITEMS
    "hdrRequired & ~hdrRequiredBits[j]"
    "RAL_FORMAT_R16G16B16A16_SFLOAT, 0u"
    "RAL_FORMAT_R16G16B16A16_SFLOAT, 1u << 31"
    "RAL_FORMAT_UNDEFINED, hdrFeatures")
    STRING(FIND "${MRT_TEST}" "${test_needle}" test_pos)
    IF(test_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "RAL format-feature mutation coverage drifted: ${test_needle}")
    ENDIF()
ENDFOREACH()

# DDS consumes fourteen distinct BC encodings. Extended enum values stay after
# the original ETC2 tail so existing public numeric identities do not move.
STRING(FIND "${RAL_TYPES_H}" "RAL_FORMAT_ETC2_R8G8B8A8_SRGB," old_format_tail)
STRING(FIND "${RAL_TYPES_H}" "RAL_FORMAT_BC1_RGB_UNORM," extended_bc_begin)
STRING(FIND "${RAL_TYPES_H}" "RAL_FORMAT_COUNT" format_count)
IF(old_format_tail EQUAL -1 OR extended_bc_begin EQUAL -1 OR format_count EQUAL -1
    OR NOT old_format_tail LESS extended_bc_begin OR NOT extended_bc_begin LESS format_count)
    MESSAGE(FATAL_ERROR "extended BC formats no longer preserve existing ralFormat numeric identities")
ENDIF()
FOREACH(mapping IN ITEMS
    "RAL_FORMAT_BC1_RGB_UNORM:VK_FORMAT_BC1_RGB_UNORM_BLOCK"
    "RAL_FORMAT_BC1_RGB_SRGB:VK_FORMAT_BC1_RGB_SRGB_BLOCK"
    "RAL_FORMAT_BC2_UNORM:VK_FORMAT_BC2_UNORM_BLOCK"
    "RAL_FORMAT_BC2_SRGB:VK_FORMAT_BC2_SRGB_BLOCK"
    "RAL_FORMAT_BC4_SNORM:VK_FORMAT_BC4_SNORM_BLOCK"
    "RAL_FORMAT_BC5_SNORM:VK_FORMAT_BC5_SNORM_BLOCK"
    "RAL_FORMAT_BC6H_SFLOAT:VK_FORMAT_BC6H_SFLOAT_BLOCK")
    STRING(REPLACE ":" ";" mapping_parts "${mapping}")
    LIST(GET mapping_parts 0 portable)
    LIST(GET mapping_parts 1 native)
    STRING(FIND "${TRANSLATE}" "case ${portable}: return ${native};" translate_pos)
    IF(translate_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "extended BC translation drifted: ${mapping}")
    ENDIF()
ENDFOREACH()
FOREACH(product_probe IN ITEMS
    "RAL_FORMAT_BC1_RGB_UNORM,  bc1_unorm"
    "RAL_FORMAT_BC1_RGB_SRGB,   bc1_srgb"
    "RAL_FORMAT_BC2_UNORM,      bc2_unorm"
    "RAL_FORMAT_BC2_SRGB,       bc2_srgb"
    "RAL_FORMAT_BC3_UNORM,      bc3_unorm"
    "RAL_FORMAT_BC3_SRGB,       bc3_srgb"
    "RAL_FORMAT_BC4_UNORM,      bc4_unorm"
    "RAL_FORMAT_BC4_SNORM,      bc4_snorm"
    "RAL_FORMAT_BC5_UNORM,      bc5_unorm"
    "RAL_FORMAT_BC5_SNORM,      bc5_snorm"
    "RAL_FORMAT_BC6H_UFLOAT,    bc6h_ufloat"
    "RAL_FORMAT_BC6H_SFLOAT,    bc6h_sfloat"
    "RAL_FORMAT_BC7_UNORM,      bc7_unorm"
    "RAL_FORMAT_BC7_SRGB,       bc7_srgb")
    STRING(FIND "${VK_PRODUCT}" "RAL_BC_PROBE( ${product_probe}" probe_pos)
    IF(probe_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "portable BC product probe drifted: ${product_probe}")
    ENDIF()
ENDFOREACH()
FOREACH(product_needle IN ITEMS
    "const ralTextureFormatFeatures_t bc_required ="
    "RAL_TEXTURE_FORMAT_FEATURE_SAMPLED |"
    "RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR;"
    "Ral_TextureFormatSupportsFeatures( backend, (ralfmt), bc_required )")
    STRING(FIND "${VK_PRODUCT}" "${product_needle}" probe_pos)
    IF(probe_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "portable BC feature authority drifted: ${product_needle}")
    ENDIF()
ENDFOREACH()
STRING(FIND "${VK_PRODUCT}" "VK_BC_PROBE" raw_bc_macro)
IF(NOT raw_bc_macro EQUAL -1)
    MESSAGE(FATAL_ERROR "renderer reacquired the raw Vulkan BC probe macro")
ENDIF()
FOREACH(test_needle IN ITEMS
    "static const ralFormat_t bcFormats[]"
    "static const VkFormat nativeBcFormats[]"
    "nativeBcFeatures & ~VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT"
    "nativeBcFeatures & ~VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT")
    STRING(FIND "${MRT_TEST}" "${test_needle}" test_pos)
    IF(test_pos EQUAL -1)
        MESSAGE(FATAL_ERROR "BC feature/mapping mutation coverage drifted: ${test_needle}")
    ENDIF()
ENDFOREACH()
