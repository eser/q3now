if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/renderer/ral/ral_backend_conformance.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_backend_conformance.c" SOURCE)
file(READ "${ROOT}/code/renderer/ral/ral_capability.c" CAPABILITY_SOURCE)
file(READ "${ROOT}/tests/ral_backend_conformance_test.mm" HOST)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_bridge.h" VULKAN_BRIDGE)

foreach(forbidden IN ITEMS "Vk" "MTL" "SDL_" "WGPU")
  if(HEADER MATCHES "${forbidden}" OR SOURCE MATCHES "${forbidden}")
    message(FATAL_ERROR "portable conformance contract leaked native API: ${forbidden}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "backendType == RAL_BACKEND_VULKAN && caps->maxPushConstantSize>0u"
  "backendType == RAL_BACKEND_VULKAN ? caps->maxPushConstantSize : 0u")
  string(FIND "${CAPABILITY_SOURCE}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "inline-data native/emulated divergence lost: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "RAL_BACKEND_CONFORMANCE_SCHEMA_VERSION 1u"
  "ralCapabilityProfile_t capabilities"
  "ralAllocationReceipt_t uploadAllocation"
  "ralAllocationReceipt_t readbackAllocation"
  "ralAllocationReceipt_t textureAllocation"
  "ralSubmissionReceipt_t submission"
  "ralTransferReceipt_t transfer"
  "Ral_BackendConformanceBuild"
  "Ral_BackendConformanceReceiptExact"
  "Ral_BackendConformanceCompatible"
  "Ral_BackendRecreateBuild"
  "Ral_BackendRecreateReceiptExact")
  string(FIND "${HEADER}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "conformance header lost required surface: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "Ral_CapabilityProfileExact( &receipt->capabilities"
  "receipt->capabilities.generation != receipt->backendGeneration"
  "Ral_AllocationReceiptExact( &receipt->uploadAllocation"
  "receipt->uploadAllocation.ownerIdentity"
  "== receipt->readbackAllocation.ownerIdentity"
  "(uintptr_t)receipt->submission.backendIdentity"
  "!= receipt->backendIdentity"
  "receipt->transfer.request.resourceIdentity"
  "!= receipt->readbackAllocation.ownerIdentity"
  "receipt->transfer.request.resourceGeneration"
  "!= receipt->readbackAllocation.allocationGeneration"
  "receipt->transfer.state != RAL_TRANSFER_COMPLETED"
  "receipt->loss.event.cause == RAL_MEMORY_FAILURE_DEVICE_LOST"
  "receipt->loss.action == RAL_MEMORY_RECOVERY_RECREATE_BACKEND"
  "receipt->replacement.backendGeneration"
  "> receipt->previous.backendGeneration")
  string(FIND "${SOURCE}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "conformance validator lost exact join: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "SDL_CreateWindow( \"Wired RAL backend conformance\""
  "SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN"
  "Ral_CreateBackend( &createInfo )"
  "RalMetal_CoreCreate"
  "Ral_CmdCopyBuffer( command, upload, readback, &copy )"
  "Ral_SubmitExact"
  "RalMetal_OffscreenConformance( core, 4096u"
  "CheckReceiptMutations( &vulkanFirst )"
  "CheckReceiptMutations( &metalFirst )"
  "CheckRecreate( &vulkanFirst"
  "CheckRecreate( &metalFirst"
  "Ral_BackendConformanceCompatible( &vulkanFirst, &metalFirst )"
  "RAL_CAP_INLINE_DATA].outcome"
  "== RAL_CAP_OUTCOME_EMULATED")
  string(FIND "${HOST}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "dual-backend host lost live/mutation evidence: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "Vulkan migration bridge."
  "deliberately NOT part of code/renderer/ral/'s portable public surface"
  "Only the Vulkan backend and renderervk integration may include this header.")
  string(FIND "${VULKAN_BRIDGE}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Vulkan migration bridge lost its bounded ownership contract: ${needle}")
  endif()
endforeach()

file(GLOB PORTABLE_FRONTEND
  "${ROOT}/code/client/*.c" "${ROOT}/code/client/*.h"
  "${ROOT}/code/renderer/*.c" "${ROOT}/code/renderer/*.h"
  "${ROOT}/code/renderer2/*.c" "${ROOT}/code/renderer2/*.h"
  "${ROOT}/code/renderercommon/*.c" "${ROOT}/code/renderercommon/*.h")
foreach(path IN LISTS PORTABLE_FRONTEND)
  file(READ "${path}" text)
  if(text MATCHES "ral_vulkan_bridge[.]h|ral_metal_internal[.]h")
    message(FATAL_ERROR "portable renderer/client included backend bridge: ${path}")
  endif()
  if(text MATCHES "RAL_BACKEND_(VULKAN|METAL|GL43|WEBGPU|WEBGL2)")
    message(FATAL_ERROR "portable renderer/client branched on backend enum: ${path}")
  endif()
endforeach()

message(STATUS "RAL live backend conformance policy: PASS")
