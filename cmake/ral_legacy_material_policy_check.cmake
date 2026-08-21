cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_legacy_material.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_legacy_material.c" CORE)
file(READ "${ROOT}/code/renderervk/vk_generic_specialization_contract.c" VK_BRIDGE)
file(READ "${ROOT}/tests/ral_legacy_material_test.c" TEST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)

foreach(forbidden "Vk[A-Z]" "VK_" "WGPU" "wgpu" "MTL" "Metal")
  if(HEADER MATCHES "${forbidden}" OR CORE MATCHES "${forbidden}")
    message(FATAL_ERROR "legacy material contract leaked native API: ${forbidden}")
  endif()
endforeach()
foreach(needle
    "textureCoordinateAnimation"
    "vertexDeform"
    "RAL_LEGACY_MATERIAL_DIRECT"
    "RAL_LEGACY_MATERIAL_FALLBACK"
    "RAL_LEGACY_FALLBACK_ALPHA_TEST"
    "facts->textureCount <= 2u"
    "variant->textureDomainMask & ~textureDomainMask"
    "variant->lightmapSlot > facts->textureCount + 1u"
    "candidate.fallbackReason = FallbackReason(variant)"
    "candidate.outcome = candidate.fallbackReason == RAL_LEGACY_FALLBACK_NONE")
  string(FIND "${HEADER}\n${CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "legacy material contract missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "Ral_LegacyMaterialResolve(&materialFacts,&materialVariant,&materialReceipt)"
    "materialVariant.alphaTest = (ralLegacyAlphaTest_t)candidate.fragmentWords[0]"
    "materialVariant.textureDomainMask = candidate.fragmentWords[4]"
    "materialVariant.combine = (ralLegacyCombine_t)candidate.fragmentWords[6]"
    "materialVariant.lightmapSlot = candidate.fragmentWords[14]"
    "materialReceipt.outcome != RAL_LEGACY_MATERIAL_DIRECT")
  string(FIND "${VK_BRIDGE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan bridge bypassed semantic RAL contract: ${needle}")
  endif()
endforeach()
foreach(needle
    "\"opaque\""
    "\"lightmap\""
    "\"additive\""
    "\"alpha-blend\""
    "\"fixed-color\""
    "\"fog\""
    "\"environment\""
    "\"tcmod\""
    "\"vertex-deform\""
    "\"atest\""
    "count == 15u"
    "Ral_LegacyMaterialReceiptExact")
  string(FIND "${TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "legacy material corpus missing: ${needle}")
  endif()
endforeach()
foreach(needle "ral_legacy_material_test" "ral_legacy_material.c" "ral_legacy_material_source_policy_contract")
  string(FIND "${CMAKE_TEXT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "legacy material ownership missing: ${needle}")
  endif()
endforeach()
message(STATUS "ral legacy material source policy: PASS")
