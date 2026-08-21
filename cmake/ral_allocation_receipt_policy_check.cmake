cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_allocation.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_allocation.c" CORE)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_memory.c" VK_MEMORY)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" VK_RESOURCE)
file(READ "${ROOT}/tests/ral_allocation_receipt_test.c" TEST)
file(READ "${ROOT}/tests/ral_webgpu_allocation_receipt_test.c" WEBGPU_TEST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)

foreach(forbidden "Vk[A-Z]" "VK_" "WGPU" "MTL")
  if(HEADER MATCHES "${forbidden}" OR CORE MATCHES "${forbidden}")
    message(FATAL_ERROR "portable allocation contract leaked native API: ${forbidden}")
  endif()
endforeach()
foreach(needle
    "RAL_ALLOCATION_SCHEMA_VERSION 1u"
    "RAL_ALLOCATION_DEVICE_LOCAL = 1"
    "RAL_ALLOCATION_UPLOAD"
    "RAL_ALLOCATION_READBACK"
    "RAL_ALLOCATION_TRANSIENT"
    "RAL_ALLOCATION_RESIDENCY_STREAMED"
    "RAL_ALLOCATION_PLACEMENT_MANAGED"
    "RAL_ALLOCATION_PRESSURE_UNKNOWN"
    "request->size > UINT64_MAX - (request->alignment - 1u)"
    "facts->committedSize > facts->budgetBytes - facts->usedBytesBefore"
    "emulated && !request->allowFallback"
    "Ral_AllocationReceiptExact")
  string(FIND "${HEADER}\n${CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "allocation contract missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "b->nextAllocationGeneration >= UINT64_MAX - 1u"
    "facts.placement = a->block ? RAL_ALLOCATION_PLACEMENT_SUBALLOCATED"
    ": RAL_ALLOCATION_PLACEMENT_DEDICATED"
    "facts.usedBytesBefore = ralVk_TrackedHeapBytes"
    "Ral_AllocationReceiptBuild(&request,&facts,&a->receipt)"
    "Ral_BufferGetAllocationReceipt"
    "Ral_TextureGetAllocationReceipt")
  string(FIND "${VK_MEMORY}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan allocation adapter missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "RAL_ALLOCATION_READBACK"
    "RAL_BUFFER_MAP_READ"
    "RAL_ALLOCATION_RESIDENCY_TRANSIENT"
    "(uintptr_t)buf"
    "(uintptr_t)tex")
  string(FIND "${VK_RESOURCE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan resource allocation ownership missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "request.alignment=3u"
    "request.size=UINT64_MAX"
    "facts.usedBytesBefore=15000u"
    "request.allowFallback=(qboolean)2"
    "facts.hostVisible=(qboolean)2")
  string(FIND "${TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "allocation mutation missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "RAL_BACKEND_WEBGPU"
    "RAL_ALLOCATION_PLACEMENT_MANAGED"
    "RAL_ALLOCATION_PRESSURE_UNKNOWN"
    "RAL_ALLOCATION_READBACK")
  string(FIND "${WEBGPU_TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "WebGPU allocation fixture missing: ${needle}")
  endif()
endforeach()
foreach(needle "ral_allocation_receipt_test" "ral_webgpu_allocation_receipt_test"
    "ral_allocation.c" "ral_allocation_receipt_source_policy_contract")
  string(FIND "${CMAKE_TEXT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "allocation test ownership missing: ${needle}")
  endif()
endforeach()
message(STATUS "ral allocation receipt source policy: PASS")
