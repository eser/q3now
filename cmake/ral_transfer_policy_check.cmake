cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_transfer.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_transfer.c" CORE)
file(READ "${ROOT}/tests/ral_transfer_lifecycle_test.c" HOST)
file(READ "${ROOT}/tests/ral_webgpu_transfer_lifecycle_test.c" WEBGPU)
file(READ "${ROOT}/tests/ral_vulkan_buffer_write_test.c" BUFFER_WRITE_HOST)
file(READ "${ROOT}/code/renderer/ral/ral_resource.h" RESOURCE_HEADER)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" VULKAN)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_pipeline.c" PIPELINE)
file(READ "${ROOT}/code/renderervk/vk_ral_textures.c" PRODUCT)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)
function(require_call_count body regex expected label)
  string(REGEX MATCHALL "${regex}" hits "${body}")
  list(LENGTH hits count)
  if(NOT count EQUAL expected)
    message(FATAL_ERROR "${label}: expected ${expected}, found ${count}")
  endif()
endfunction()
foreach(forbidden "Vk[A-Z]" "VK_" "WGPU" "MTL")
  if(HEADER MATCHES "${forbidden}" OR CORE MATCHES "${forbidden}")
    message(FATAL_ERROR "transfer contract leaked native API: ${forbidden}")
  endif()
endforeach()
string(FIND "${VULKAN}" "qboolean Ral_BufferWriteImmediate" write_begin)
if(write_begin EQUAL -1)
  message(FATAL_ERROR "Vulkan immediate buffer write function missing")
endif()
string(LENGTH "${VULKAN}" vulkan_length)
math(EXPR write_tail_length "${vulkan_length} - ${write_begin}")
string(SUBSTRING "${VULKAN}" ${write_begin} ${write_tail_length} WRITE_TAIL)
string(FIND "${WRITE_TAIL}" "static qboolean ralVk_BufferUploadTicketValid" write_end)
if(write_end EQUAL -1)
  message(FATAL_ERROR "Vulkan immediate buffer write end marker missing")
endif()
string(SUBSTRING "${WRITE_TAIL}" 0 ${write_end} BUFFER_WRITE)
foreach(needle
    "ralTransferReceipt_t transfer"
    "ralBufferUploadTicket_t Ral_BufferUploadBegin"
    "Ral_BufferUploadTicketComplete"
    "Ral_BufferUploadTicketGetReceipt"
    "Ral_BufferAcquireBatchToGraphics"
    "Ral_BufferWriteImmediate"
    "Ral_TextureUploadTicketComplete"
    "Ral_TextureUploadTicketGetReceipt")
  string(FIND "${RESOURCE_HEADER}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "upload ticket receipt surface missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "qboolean Ral_BufferWriteImmediate"
    "!( buffer->usage & RAL_BUFFER_TRANSFER_DST )"
    "backend->nextTransferGeneration >= UINT64_MAX - 1u"
    "RAL_TRANSFER_OUTCOME_SYNCHRONOUS"
    "Ral_BufferUploadReceiptBuild( &completed, generation"
    "memcpy( (unsigned char *)mapped + offset, data, (size_t)size )"
    "buffer->portableState.usage = RAL_RESOURCE_USAGE_HOST_WRITE"
    "backend->nextTransferGeneration = generation"
    "*outReceipt = candidate")
  string(FIND "${BUFFER_WRITE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan immediate buffer write missing: ${needle}")
  endif()
endforeach()
string(FIND "${BUFFER_WRITE}" "Ral_BufferUploadReceiptBuild( &completed, generation" write_candidate)
string(FIND "${BUFFER_WRITE}" "mapped = ralVk_Map( buffer->alloc );" write_map)
string(FIND "${BUFFER_WRITE}" "backend->nextTransferGeneration = generation;" write_generation)
string(FIND "${BUFFER_WRITE}" "*outReceipt = candidate;" write_publish)
if(write_candidate EQUAL -1 OR write_map EQUAL -1 OR write_generation EQUAL -1
    OR write_publish EQUAL -1 OR NOT write_candidate LESS write_map
    OR NOT write_map LESS write_generation OR NOT write_generation LESS write_publish)
  message(FATAL_ERROR "immediate buffer write is not candidate-first/output-atomic")
endif()
foreach(needle
    "RAL_BUFFER_UPLOAD_RECEIPT_SCHEMA_VERSION 1u"
    "ralBufferUploadReceipt_t"
    "graphicsVisibilityGeneration"
    "Ral_BufferUploadReceiptBuild"
    "Ral_BufferUploadReceiptExact"
    "request->byteOffset > request->byteBudget - request->byteSize")
  string(FIND "${HEADER}\n${CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "buffer upload visibility receipt missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "RAL_BUFFER_UNIFORM | RAL_BUFFER_TRANSFER_DST"
    "Ral_BufferWriteImmediate( &buffer, 8u"
    "receipt.transfer.outcome == RAL_TRANSFER_OUTCOME_SYNCHRONOUS"
    "receipt.transfer.request.resourceGeneration == 3u"
    "receipt.transfer.request.byteBudget == sizeof( storage )"
    "backend.nextTransferGeneration == 2u"
    "buffer.usage = RAL_BUFFER_UNIFORM"
    "buffer.hostVisible = qfalse"
    "buffer.legacyMapped = qtrue"
    "buffer.portableStateKnown = qfalse"
    "backend.nextTransferGeneration = UINT64_MAX - 1u"
    "Ral_BufferUploadReceiptExact( &out, &sentinel )")
  string(FIND "${BUFFER_WRITE_HOST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "immediate buffer write host mutation missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "queue.writeBuffer"
    "upload.transfer.request.backendType==RAL_BACKEND_WEBGPU"
    "upload.transfer.request.resourceIdentity==(uintptr_t)0x400u"
    "upload.transfer.request.resourceGeneration==5u"
    "upload.transfer.request.byteOffset==64u"
    "upload.transfer.request.byteSize==192u"
    "upload.transfer.request.byteBudget==256u"
    "upload.transfer.outcome==RAL_TRANSFER_OUTCOME_MANAGED_ASYNC"
    "upload.graphicsVisibilityGeneration==12u"
    "badUpload.transfer.request.backendType=RAL_BACKEND_VULKAN"
    "badUpload.transfer.request.resourceGeneration=6u"
    "badUpload.transfer.request.byteOffset=63u"
    "badUpload.transfer.request.byteSize=191u"
    "badUpload.transfer.request.byteBudget=257u"
    "badUpload.transfer.outcome=RAL_TRANSFER_OUTCOME_NATIVE_ASYNC"
    "badUpload.transfer.transferGeneration=13u"
    "badUpload.graphicsVisibilityGeneration=13u")
  string(FIND "${WEBGPU}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "WebGPU queue-write receipt mutation missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "ralVk_BufferTransferPrepared"
    "Ral_BufferGetAllocationReceipt( buffer, &allocation )"
    "request.resourceIdentity = (uintptr_t)buffer"
    "request.resourceGeneration = allocation.allocationGeneration"
    "request.byteOffset = offset"
    "request.byteSize = size"
    "request.byteBudget = allocation.committedSize"
    "ralVk_BufferUploadNoWait"
    "ticket.graphicsAcquireRequired = queue == RAL_QUEUE_TRANSFER"
    "ticket.graphicsAcquired = queue == RAL_QUEUE_GRAPHICS"
    "ralVk_BufferUploadTicketValid"
    "ticket->transfer.state != RAL_TRANSFER_COMPLETED"
    "Ral_BufferUploadReceiptBuild"
    "ticket->transfer.request.queue != RAL_QUEUE_TRANSFER"
    "tickets[i].graphicsAcquired = qtrue")
  string(FIND "${VULKAN}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Vulkan buffer upload ticket missing: ${needle}")
  endif()
endforeach()
string(FIND "${VULKAN}" "if ( !ralVk_SubmitUploadCmdNoWait" buffer_submit)
string(FIND "${VULKAN}" "buffer->portableOwnerQueue = queue;" buffer_publish)
if(buffer_submit EQUAL -1 OR buffer_publish EQUAL -1 OR
    NOT buffer_submit LESS buffer_publish)
  message(FATAL_ERROR "buffer portable state published before successful submit")
endif()

# The shipping Vulkan diagnostic exercises the same two-buffer transaction
# before its first draw, so the runtime receipt cannot remain host-only.
string(FIND "${PIPELINE}" "void ralVk_RunPipelineTest( ralBackend_t *b )" pipeline_begin)
if(pipeline_begin EQUAL -1)
  message(FATAL_ERROR "Vulkan pipeline diagnostic is missing")
endif()
string(LENGTH "${PIPELINE}" pipeline_length)
math(EXPR pipeline_tail_length "${pipeline_length} - ${pipeline_begin}")
string(SUBSTRING "${PIPELINE}" ${pipeline_begin} ${pipeline_tail_length} PIPELINE_TEST)
require_call_count("${PIPELINE_TEST}" "Ral_BufferUploadBegin[(]" 2
  "pipeline diagnostic buffer upload begin")
require_call_count("${PIPELINE_TEST}" "Ral_BufferUploadTicketComplete[(]" 1
  "pipeline diagnostic buffer upload completion")
require_call_count("${PIPELINE_TEST}" "Ral_BufferAcquireBatchToGraphics[(]" 1
  "pipeline diagnostic graphics acquire")
require_call_count("${PIPELINE_TEST}" "Ral_BufferUploadTicketGetReceipt[(]" 1
  "pipeline diagnostic receipt publication")
require_call_count("${PIPELINE_TEST}" "Ral_BufferUploadReceiptExact[(]" 1
  "pipeline diagnostic exact receipt")
require_call_count("${PIPELINE_TEST}" "Ral_TextureUploadBegin[(]" 1
  "pipeline diagnostic residency upload begin")
require_call_count("${PIPELINE_TEST}" "Ral_TextureUploadTicketComplete[(]" 1
  "pipeline diagnostic residency upload completion")
require_call_count("${PIPELINE_TEST}" "Ral_TextureAcquireBatchToGraphics[(]" 1
  "pipeline diagnostic residency graphics acquire")
string(FIND "${PIPELINE_TEST}" "Ral_BufferUploadAsync" legacy_buffer_upload)
if(NOT legacy_buffer_upload EQUAL -1)
  message(FATAL_ERROR "pipeline diagnostic regressed to legacy buffer upload")
endif()
string(FIND "${PIPELINE_TEST}" "acquireTicket.readySemaphore =" forged_texture_ticket)
if(NOT forged_texture_ticket EQUAL -1)
  message(FATAL_ERROR "pipeline diagnostic forged a residency ticket")
endif()
foreach(needle
    "Ral_BufferUploadBegin( vb"
    "Ral_BufferUploadTicketComplete("
    "Ral_BufferAcquireBatchToGraphics( b,"
    "Ral_BufferUploadTicketGetReceipt("
    "buffer upload tickets: count=2 completed=%u graphics-visible=%u"
    "if ( uploadsReady ) {")
  string(FIND "${PIPELINE_TEST}" "${needle}" pipeline_needle_pos)
  if(pipeline_needle_pos EQUAL -1)
    message(FATAL_ERROR "pipeline diagnostic transaction missing: ${needle}")
  endif()
endforeach()
string(FIND "${PIPELINE_TEST}" "Ral_BufferUploadBegin( vb" pipeline_upload)
string(FIND "${PIPELINE_TEST}" "Ral_BufferUploadTicketComplete(" pipeline_complete)
string(FIND "${PIPELINE_TEST}" "Ral_BufferAcquireBatchToGraphics( b," pipeline_acquire)
string(FIND "${PIPELINE_TEST}" "Ral_BufferUploadTicketGetReceipt(" pipeline_receipt)
string(FIND "${PIPELINE_TEST}" "buffer upload tickets: count=2 completed=%u graphics-visible=%u" pipeline_log)
string(FIND "${PIPELINE_TEST}" "if ( uploadsReady ) {" pipeline_draw)
if(NOT pipeline_upload LESS pipeline_complete OR
    NOT pipeline_complete LESS pipeline_acquire OR
    NOT pipeline_acquire LESS pipeline_receipt OR
    NOT pipeline_receipt LESS pipeline_log OR
    NOT pipeline_log LESS pipeline_draw)
  message(FATAL_ERROR "pipeline diagnostic upload authority is not causal before draw")
endif()
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
    "RAL_TRANSFER_READBACK"
    "request.byteOffset=768u"
    "Ral_BufferUploadReceiptBuild(&completed,24u"
    "uploadBad.graphicsVisibilityGeneration++"
    "uploadBad.transfer.request.byteOffset++")
  string(FIND "${HOST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transfer host mutation missing: ${needle}")
  endif()
endforeach()
foreach(needle "RAL_BACKEND_WEBGPU" "RAL_TRANSFER_OUTCOME_MANAGED_ASYNC"
    "Ral_TransferComplete(&submitted,7u,qfalse"
    "request.byteOffset=64u"
    "Ral_BufferUploadReceiptBuild(&completed,12u"
    "badUpload.ready=qfalse")
  string(FIND "${WEBGPU}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "WebGPU transfer shape missing: ${needle}")
  endif()
endforeach()
foreach(needle "ral_transfer_lifecycle_test" "ral_webgpu_transfer_lifecycle_test"
    "ral_vulkan_buffer_write_test" "ral_transfer_source_policy_contract" "ral_transfer.c")
  string(FIND "${CMAKE_TEXT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transfer test ownership missing: ${needle}")
  endif()
endforeach()
message(STATUS "ral transfer source policy: PASS")
