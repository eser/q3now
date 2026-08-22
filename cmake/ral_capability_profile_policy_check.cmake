cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_capability.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_capability.c" CORE)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_caps.c" VULKAN)
file(READ "${ROOT}/tests/ral_capability_profile_test.c" TEST)
file(READ "${ROOT}/tests/ral_webgpu_capability_profile_test.c" WEBGPU_TEST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)

foreach(forbidden "Vk[A-Z]" "VK_" "WGPU" "MTL")
  if(HEADER MATCHES "${forbidden}" OR CORE MATCHES "${forbidden}")
    message(FATAL_ERROR "capability profile leaked native API: ${forbidden}")
  endif()
endforeach()
foreach(needle
    "RAL_CAPABILITY_PROFILE_SCHEMA_VERSION 1u"
    "RAL_CAP_DYNAMIC_RENDERING = 0"
    "RAL_CAP_TEXTURE_COMPRESSION_BC"
    "RAL_CAP_TEXTURE_COMPRESSION_ASTC"
    "RAL_CAP_TEXTURE_COMPRESSION_ETC2"
    "RAL_CAP_OUTCOME_NATIVE"
    "RAL_CAP_OUTCOME_EMULATED"
    "RAL_CAP_OUTCOME_DISABLED"
    "factCount != RAL_CAP_COUNT"
    "fact->id != (ralCapabilityId_t)i"
	"fact->nativeLimit >= policy->minimum"
	"fact->emulationLimit >= policy->minimum"
	"entry->limit = fact->nativeLimit"
	"entry->limit = fact->emulationLimit"
    "policy->requirement == RAL_CAP_REQUIREMENT_OPTIONAL"
    "else return qfalse"
    "Ral_CapabilityProfileBuild(backendType,generation,facts,RAL_CAP_COUNT,out)")
  string(FIND "${HEADER}\n${CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "capability profile contract missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "VK_FORMAT_BC1_RGBA_UNORM_BLOCK"
    "VK_FORMAT_BC3_UNORM_BLOCK"
    "VK_FORMAT_BC5_UNORM_BLOCK"
    "VK_FORMAT_BC7_UNORM_BLOCK"
    "VK_FORMAT_ASTC_4x4_UNORM_BLOCK"
    "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK"
    "VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT")
  string(FIND "${VULKAN}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan capability adapter missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "facts[RAL_CAP_DYNAMIC_RENDERING].nativeSupport=qfalse"
	"facts[RAL_CAP_MAX_COLOR_ATTACHMENTS].nativeLimit=3u"
	"facts[RAL_CAP_INLINE_DATA].nativeLimit=64u"
	"facts[RAL_CAP_INLINE_DATA].emulationLimit=256u"
    "facts[RAL_CAP_BIND_GROUPS].nativeSupport=(qboolean)2"
    "Ral_CapabilityProfileFromCaps(RAL_BACKEND_VULKAN"
    "RAL_CAP_TEXTURE_COMPRESSION_ASTC].outcome==RAL_CAP_OUTCOME_EMULATED")
  string(FIND "${TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "capability mutation coverage missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "Ral_CapabilityProfileBuild(RAL_BACKEND_WEBGPU"
    "RAL_CAP_INLINE_DATA].outcome==RAL_CAP_OUTCOME_EMULATED"
    "RAL_CAP_ASYNC_COMPUTE].outcome==RAL_CAP_OUTCOME_EMULATED"
    "RAL_CAP_DRAW_INDIRECT_COUNT].outcome==RAL_CAP_OUTCOME_DISABLED"
    "RAL_CAP_TEXTURE_COMPRESSION_ETC2].outcome==RAL_CAP_OUTCOME_NATIVE"
    "const ralTextureFormatFeatures_t rgba16f ="
    "RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR"
    "RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND"
    "rgba16f & RAL_TEXTURE_FORMAT_FEATURE_STORAGE ) == 0u"
    "RAL_TEXTURE_FORMAT_FEATURE_ALL & ( 1u << 31 )")
  string(FIND "${WEBGPU_TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "WebGPU capability fixture missing: ${needle}")
  endif()
endforeach()
foreach(needle "ral_capability_profile_test" "ral_webgpu_capability_profile_test"
    "ral_capability.c" "ral_capability_profile_source_policy_contract")
  string(FIND "${CMAKE_TEXT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "capability profile ownership missing: ${needle}")
  endif()
endforeach()
message(STATUS "ral capability profile source policy: PASS")
