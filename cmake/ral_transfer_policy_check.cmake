cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_transfer.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_transfer.c" CORE)
file(READ "${ROOT}/tests/ral_transfer_lifecycle_test.c" HOST)
file(READ "${ROOT}/tests/ral_webgpu_transfer_lifecycle_test.c" WEBGPU)
file(READ "${ROOT}/code/renderer/ral/ral_resource.h" RESOURCE_HEADER)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" VULKAN)
file(READ "${ROOT}/code/renderervk/vk_ral_textures.c" PRODUCT)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)
foreach(forbidden "Vk[A-Z]" "VK_" "WGPU" "MTL")
  if(HEADER MATCHES "${forbidden}" OR CORE MATCHES "${forbidden}")
    message(FATAL_ERROR "transfer contract leaked native API: ${forbidden}")
  endif()
endforeach()
foreach(needle
    "ralTransferReceipt_t transfer"
    "Ral_TextureUploadTicketComplete"
    "Ral_TextureUploadTicketGetReceipt")
  string(FIND "${RESOURCE_HEADER}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "upload ticket receipt surface missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "Ral_TextureGetAllocationReceipt( tex, &allocation )"
    "request.resourceIdentity = (uintptr_t)tex"
    "request.resourceGeneration = allocation.allocationGeneration"
    "request.byteBudget = allocation.committedSize"
    "region->mipLevel >= tex->mipLevels"
    "region->offsetX > UINT32_MAX - width"
    "b->nextTransferGeneration + 1u"
    "RAL_TRANSFER_OUTCOME_NATIVE_ASYNC"
    "RAL_TRANSFER_OUTCOME_SYNCHRONOUS"
    "ticket.transfer = published"
    "Ral_FenceSignaled( ticket->fence )"
    "ticket->transfer.state != RAL_TRANSFER_COMPLETED"
    "ticket->transfer.request.resourceGeneration != allocation.allocationGeneration")
  string(FIND "${VULKAN}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan transfer receipt adapter missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "if ( b->vk.EndCommandBuffer( cb ) != VK_SUCCESS ) goto fail"
    "if ( b->vk.CreateFence( b->device, &fi, NULL, &fence ) != VK_SUCCESS ) goto fail"
    "ralVk_QueueSubmit2( b, q, &si2, fence ) != ralSuccess"
    "b->vk.WaitForFences( b->device, 1, &fence, VK_TRUE, ~0ull ) != VK_SUCCESS"
    "if ( !ralVk_SubmitUploadCmdNoWait"
    "if ( !ralVk_SubmitUploadCmdAndWait")
  string(FIND "${VULKAN}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan transfer failure gate missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "vk_ral_upload_ticket_complete"
    "ticket->transfer.state == RAL_TRANSFER_SUBMITTED"
    "Ral_WaitFence( ticket->fence, RAL_TIMEOUT_INFINITE )"
    "Ral_TextureUploadTicketComplete( ticket )"
    "vk_ral_upload_ticket_complete( &p->ticket )"
    "vk_ral_upload_ticket_complete( &s_ral_mip_test_ticket )"
    "vk_ral_upload_ticket_complete( &s_ral_material_tickets[j] )")
  string(FIND "${PRODUCT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "product transfer completion join missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "RAL_TRANSFER_SCHEMA_VERSION 1u"
    "request->resourceGeneration == UINT64_MAX"
    "request->byteOffset > UINT64_MAX - request->byteSize"
    "receipt->state"
    "RAL_TRANSFER_OUTCOME_MANAGED_ASYNC"
    "prepared->state != RAL_TRANSFER_PREPARED"
    "submitted->state != RAL_TRANSFER_SUBMITTED"
    "fenceCompleted != qtrue"
    "candidate.state = RAL_TRANSFER_CANCELED"
    "Ral_TransferReceiptExact")
  string(FIND "${HEADER}\n${CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transfer lifecycle missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "MUTATE(request.resourceIdentity"
    "MUTATE(request.resourceGeneration"
    "MUTATE(request.queue"
    "RAL_TRANSFER_OUTCOME_NATIVE_ASYNC"
    "RAL_TRANSFER_OUTCOME_SYNCHRONOUS"
    "Ral_TransferCancel"
    "request.byteBudget=4095u"
    "RAL_TRANSFER_READBACK")
  string(FIND "${HOST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transfer host mutation missing: ${needle}")
  endif()
endforeach()
foreach(needle "RAL_BACKEND_WEBGPU" "RAL_TRANSFER_OUTCOME_MANAGED_ASYNC"
    "Ral_TransferComplete(&submitted,7u,qfalse")
  string(FIND "${WEBGPU}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "WebGPU transfer shape missing: ${needle}")
  endif()
endforeach()
foreach(needle "ral_transfer_lifecycle_test" "ral_webgpu_transfer_lifecycle_test"
    "ral_transfer_source_policy_contract" "ral_transfer.c")
  string(FIND "${CMAKE_TEXT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transfer test ownership missing: ${needle}")
  endif()
endforeach()
message(STATUS "ral transfer source policy: PASS")
