cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_transient_texture.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_transient_texture.c" CORE)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_transient.c" VULKAN)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_internal.h" VK_INTERNAL)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" VK_RESOURCE)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_backend.c" VK_BACKEND)
file(READ "${ROOT}/tests/ral_transient_texture_cohort_test.c" TEST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)
foreach(forbidden "Vk[A-Z]" "VK_" "WGPU" "MTL")
  if(HEADER MATCHES "${forbidden}" OR CORE MATCHES "${forbidden}")
    message(FATAL_ERROR "transient texture owner leaked native API: ${forbidden}")
  endif()
endforeach()
foreach(needle
    "RAL_TRANSIENT_TEXTURE_SCHEMA_VERSION 1u"
    "ops->createUnbound"
    "ops->candidateAllowed"
    "ops->getAllocationReceipt"
    "Ral_TransientPlanBuild"
    "ops->allocateSlot"
    "ops->bindComplete"
    "CleanupCandidates(candidate,textureCount,allocationCount)"
    "candidate->receipt.cohortIdentity=(uintptr_t)candidate"
    "Ral_AllocationReceiptExact(&slot->allocation,&slot->allocation)"
    "binding->resourceGeneration!=assignment->request.resourceGeneration"
    "slot->allocation.ownerGeneration!=slot->allocation.allocationGeneration"
    "Ral_TransientBatchReceiptExact(&cohort->lifecycle.active,terminalBatch)"
    "cohort->ops.retireTexture"
    "cohort->ops.retireAllocation")
  string(FIND "${HEADER}\n${CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transient texture owner missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "VK_IMAGE_CREATE_ALIAS_BIT"
    "GetImageMemoryRequirements"
    "ralVk_TransientKeyExact"
    "requirements.size = slot->committedSize"
    "RAL_ALLOCATION_TRANSIENT, RAL_ALLOCATION_RESIDENCY_TRANSIENT"
    "BindImageMemory"
    "CreateImageView"
    "Ral_AllocationReceiptExact( &allocation->receipt, &allocation->receipt )"
    "RAL_RES_IMAGE_AND_VIEW"
    "RAL_RES_ALLOCATION_ONLY"
    "ci->policy.explicitAliasing != qtrue")
  string(FIND "${VULKAN}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan transient alias adapter missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "qboolean            transientCohortOwned"
    "qboolean               transientAlias"
    "RAL_RES_ALLOCATION_ONLY")
  string(FIND "${VK_INTERNAL}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan transient ownership state missing: ${needle}")
  endif()
endforeach()
string(FIND "${VK_RESOURCE}" "if ( tex->transientCohortOwned ) return;" guard_pos)
if(guard_pos EQUAL -1)
  message(FATAL_ERROR "ordinary texture destroy can retire a cohort child")
endif()
string(FIND "${VK_BACKEND}" "case RAL_RES_ALLOCATION_ONLY: break;" allocation_destroy_pos)
if(allocation_destroy_pos EQUAL -1)
  message(FATAL_ERROR "allocation-only deferred teardown missing")
endif()
foreach(needle
    "context.failCreate=fail"
    "context.failAllocate=1u"
    "context.failBind=fail"
    "context.rejectCandidate=qtrue"
    "context.duplicateTexture=qtrue"
    "context.crossRoleAllocation=qtrue"
    "context.failReceipt=qtrue"
    "ci.policy.budgetBytes=512u"
    "context.duplicateAllocation=qtrue"
    "context.incompatibleTexture=qtrue"
    "exact.textures[1].resourceGeneration++"
    "exact.allocations[0].allocation.allocationGeneration++"
    "CHECK(cohort==NULL)"
    "!memcmp(context.retireOrder,\"TTTA\",4u)"
    "Ral_TransientTextureCohortCancel")
  string(FIND "${TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transient texture mutation missing: ${needle}")
  endif()
endforeach()
foreach(needle "ral_transient_texture_cohort_test" "ral_transient_texture.c"
    "ral_vulkan_transient.c"
    "ral_transient_texture_source_policy_contract")
  string(FIND "${CMAKE_TEXT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transient texture test ownership missing: ${needle}")
  endif()
endforeach()
message(STATUS "ral transient texture source policy: PASS")
