cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()

file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph.h" HEADER)
file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph.c" CORE)
file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph_execution.h" EXEC_HEADER)
file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph_execution.c" EXEC_CORE)
file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph_record.h" RECORD_HEADER)
file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph_record.c" RECORD_CORE)
file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph_record_ral.c" RAL_RECORD)
file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph_submit.h" SUBMIT_HEADER)
file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph_submit.c" SUBMIT_CORE)
file(READ "${ROOT}/code/renderer/ral_frame_graph/ral_frame_graph_submit_ral.c" RAL_SUBMIT)
file(READ "${ROOT}/tests/ral_frame_graph_test.c" TEST)
file(READ "${ROOT}/tests/ral_frame_graph_execution_test.c" EXEC_TEST)
file(READ "${ROOT}/tests/ral_frame_graph_record_test.c" RECORD_TEST)
file(READ "${ROOT}/tests/ral_frame_graph_submit_test.c" SUBMIT_TEST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)

foreach(forbidden IN ITEMS "Vk[A-Z]" "VK_" "MTL" "WGPU" "SDL_")
  if(HEADER MATCHES "${forbidden}" OR CORE MATCHES "${forbidden}"
      OR EXEC_HEADER MATCHES "${forbidden}" OR EXEC_CORE MATCHES "${forbidden}")
    message(FATAL_ERROR "above-RAL frame graph leaked native API: ${forbidden}")
  endif()
endforeach()

foreach(forbidden IN ITEMS "Vk[A-Z]" "VK_" "MTL" "WGPU" "SDL_")
  if(SUBMIT_HEADER MATCHES "${forbidden}" OR SUBMIT_CORE MATCHES "${forbidden}"
      OR RAL_SUBMIT MATCHES "${forbidden}")
    message(FATAL_ERROR "above-RAL frame-graph submit leaked native API: ${forbidden}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "RAL_FRAME_GRAPH_SUBMIT_SCHEMA_VERSION 1u"
  "RAL_FRAME_GRAPH_SUBMIT_COMPLETE"
  "RAL_FRAME_GRAPH_SUBMIT_FAILED"
  "ralFrameGraphSubmissionReceipt_t"
  "ralFrameGraphSubmissionFailureReceipt_t"
  "RalFrameGraphSubmit_Submit"
  "RalFrameGraphSubmit_ReceiptExact"
  "RalFrameGraphSubmit_FailureReceiptExact"
  "RalFrameGraphSubmit_RalOps")
  string(FIND "${SUBMIT_HEADER}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph submit contract missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "PreflightCurrent(&frozen,&ops)"
  "ops.endCommand"
  "ExecutableMatchesRecording"
  "submit.waitSemaphores=waits"
  "submit.signalSemaphores=signals"
  "ops.submitExact"
  "SubmissionMatchesExecutable"
  "CancelRange(&frozen,&ops,i"
  "PublishFailure(&frozen,RAL_FRAME_GRAPH_SUBMIT_FAILURE_QUEUE"
  "candidate.timelineFinalValues"
  "SuccessReceiptValid(&candidate)")
  string(FIND "${SUBMIT_CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph submit seam missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "Ral_GetTimelineValue(semaphore)"
  "Ral_EndCommandBufferExact"
  "Ral_CancelCommandBuffer"
  "Ral_SubmitExact")
  string(FIND "${RAL_SUBMIT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "real RAL submit adapter missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "bad.batchCommands[0].recordingReceipt.state=RAL_COMMAND_EXECUTABLE"
  "bad.queueTimelines[1]=bad.queueTimelines[0]"
  "capture.timelineValues[0]++"
  "capture.events[5].waitCount==2u"
  "receipt.timelineFinalValues[2]==8u"
  "capture.failEndBatch=1u"
  "failure.submittedBatchCount==0u"
  "capture.failSubmitBatch=1u"
  "failure.submittedBatchCount==1u"
  "failure.cancellationFailureCount==1u"
  "RAL_BACKEND_WEBGPU,qfalse"
  "capture.events[1].signal==NULL")
  string(FIND "${SUBMIT_TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph submit mutation evidence missing: ${needle}")
  endif()
endforeach()

foreach(forbidden IN ITEMS "Vk[A-Z]" "VK_" "MTL" "WGPU" "SDL_")
  if(RECORD_HEADER MATCHES "${forbidden}" OR RECORD_CORE MATCHES "${forbidden}"
      OR RAL_RECORD MATCHES "${forbidden}")
    message(FATAL_ERROR "above-RAL frame-graph recorder leaked native API: ${forbidden}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "RAL_FRAME_GRAPH_EXECUTION_SCHEMA_VERSION 1u"
  "ralFrameGraphResourceBinding_t"
  "physicalQueueForLogical"
  "timelineBaseValues"
  "ralFrameGraphExecutionPass_t"
  "ralFrameGraphSubmissionBatch_t"
  "RalFrameGraphExecution_Compile"
  "RalFrameGraphExecution_PlanExact")
  string(FIND "${EXEC_HEADER}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph execution contract missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "RAL_FRAME_GRAPH_RECORD_SCHEMA_VERSION 1u"
  "ralFrameGraphBatchCommand_t"
  "ralFrameGraphRecordOps_t"
  "ralFrameGraphPassCallbacks_t"
  "RalFrameGraphRecord_Record"
  "RalFrameGraphRecord_ReceiptExact"
  "RalFrameGraphRecord_RalOps")
  string(FIND "${RECORD_HEADER}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph recorder contract missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "Complete preflight precedes the first resource command or pass callback"
  "CommandAuthorityCurrent(&frozen,&ops,i)"
  "pass->acquireBufferTransitionCount"
  "ops.transitionResources"
  "frozen.passCallbacks.record"
  "pass->releaseBufferTransitionCount"
  "CancelPending(&frozen,&ops,pending,pendingCount)"
  "releaseCount != acquireCount"
  "RalFrameGraphExecution_PlanExact(frozen.executionPlan"
  "ReceiptValid(&candidate)")
  string(FIND "${RECORD_CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph recorder seam missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "Ral_GetCommandBufferReceipt(command,outReceipt)"
  "Ral_CmdTransitionResources(command,batch)"
  "Ral_CmdReleaseBufferOwnership"
  "Ral_CmdAcquireBufferOwnership"
  "Ral_CancelBufferOwnershipTransfer"
  "Ral_CmdReleaseTextureOwnership"
  "Ral_CmdAcquireTextureOwnership"
  "Ral_CancelTextureOwnershipTransfer")
  string(FIND "${RAL_RECORD}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "real RAL recorder adapter missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "expectedKinds[]={'T','P','R','A','T','P','R','r','A','a','P'}"
  "capture.preflightCount==3u"
  "bad.batchCommands[0].recordingReceipt.state=RAL_COMMAND_EXECUTABLE"
  "badOps.transitionResources=NULL"
  "capture.failOperationAttempt=3u"
  "capture.cancelCount==1u"
  "capture.corruptReleaseReceipt=qtrue"
  "expectedKinds[]={'T','P','T','P','T','P'}"
  "receipt.ownershipReleaseCount==0u"
  "memcmp(&output,&before,sizeof(output))==0")
  string(FIND "${RECORD_TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph recorder mutation evidence missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "RalFrameGraph_PlanExact(description->graphPlan,description->graphPlan)"
  "description->bindingCount != graph->description.resourceCount"
  "BindingValid(&graph->description.resources[i]"
  "Ral_BufferTransitionValid"
  "Ral_TextureTransitionValid"
  "FindHandoffForTransition(graph,fact) < 0"
  "AppendOwnershipTransition"
  "sourceBatch->physicalQueue == destinationBatch->physicalQueue"
  "destinationBatch->waits[j].value = sourceBatch->signalValue"
  "memcmp(a,&rebuiltA,sizeof(*a)) == 0")
  string(FIND "${EXEC_CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph execution seam missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "plan.submissionBatchCount==4u"
  "plan.acquireBufferTransitionCount==2u"
  "WaitHas(&plan.submissionBatches[2],RAL_QUEUE_COMPUTE,21u)"
  "badDescription.bindings[0].resourceGeneration++"
  "badDescription.bindings[2].buffer=badDescription.bindings[0].buffer"
  "badDescription.timelineBaseValues[0]=UINT64_MAX"
  "plan.submissionBatchCount==1u"
  "plan.preBufferTransitionCount==4u"
  "ImportedDedicatedFirstUseRejects"
  "memcmp(&badPlan,&beforePlan,sizeof(badPlan))==0")
  string(FIND "${EXEC_TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph execution mutation evidence missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "RAL_FRAME_GRAPH_SCHEMA_VERSION 1u"
  "RAL_FRAME_GRAPH_MAX_RESOURCES 32u"
  "RAL_FRAME_GRAPH_HAZARD_RAW"
  "RAL_FRAME_GRAPH_HAZARD_WAR"
  "RAL_FRAME_GRAPH_HAZARD_WAW"
  "RAL_FRAME_GRAPH_HAZARD_STATE"
  "RAL_FRAME_GRAPH_HAZARD_QUEUE_HANDOFF"
  "RalFrameGraph_Compile"
  "RalFrameGraph_PlanExact")
  string(FIND "${HEADER}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph public contract missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "scratch->useByPassResource[passIndex][resourceIndex] >= 0"
  "RAL_FRAME_GRAPH_HAZARD_RAW"
  "RAL_FRAME_GRAPH_HAZARD_WAW"
  "RAL_FRAME_GRAPH_HAZARD_WAR"
  "RAL_FRAME_GRAPH_HAZARD_STATE"
  "RAL_FRAME_GRAPH_HAZARD_QUEUE_HANDOFF"
  "selected == UINT32_MAX"
  "currentQueue != pass->queue"
  "request->firstPass = firstPosition"
  "request->lastPass = lastPosition"
  "Ral_TransientPlanBuild"
  "description->transientPolicy.backendType != description->backendType"
  "transientCount == 0u"
  "memcmp(&a->transientPlan,&emptyTransientPlan"
  "CompileInternal( &a->description, &rebuiltA )")
  string(FIND "${CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph compiler seam missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "RAL_FRAME_GRAPH_HAZARD_RAW,1u<<0"
  "RAL_FRAME_GRAPH_HAZARD_WAR,1u<<0"
  "RAL_FRAME_GRAPH_HAZARD_WAW,1u<<0"
  "plan.queueHandoffCount == 4u"
  "RAL_TRANSIENT_PLAN_EXPLICIT_ALIAS"
  "badDescription.explicitDependencies[0]"
  "explicitDependencies[0].sourcePassId=400u"
  "explicitDependencies[0].destinationPassId=100u"
  "badDescription.transientPolicy.generation++"
  "RAL_FRAME_GRAPH_MAX_RESOURCES+1u"
  "RAL_FRAME_GRAPH_ACCESS_READ_WRITE"
  "RAL_FRAME_GRAPH_RESOURCE_EXTERNAL"
  "plan.transientRequestCount==0u"
  "RAL_BACKEND_WEBGPU"
  "RAL_TRANSIENT_PLAN_MANAGED_DISJOINT"
  "memcmp(&badPlan,&beforePlan,sizeof(badPlan))==0")
  string(FIND "${TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph mutation evidence missing: ${needle}")
  endif()
endforeach()

foreach(needle IN ITEMS
  "ral_frame_graph_test"
  "ral_frame_graph_execution_test"
  "ral_frame_graph_record_test"
  "ral_frame_graph_submit_test"
  "code/renderer/ral_frame_graph/ral_frame_graph.c"
  "code/renderer/ral_frame_graph/ral_frame_graph_execution.c"
  "code/renderer/ral_frame_graph/ral_frame_graph_record.c"
  "code/renderer/ral_frame_graph/ral_frame_graph_record_ral.c"
  "code/renderer/ral_frame_graph/ral_frame_graph_submit.c"
  "code/renderer/ral_frame_graph/ral_frame_graph_submit_ral.c"
  "ral_frame_graph_contract"
  "ral_frame_graph_execution_contract"
  "ral_frame_graph_record_contract"
  "ral_frame_graph_submit_contract"
  "ral_frame_graph_source_policy_contract")
  string(FIND "${CMAKE_TEXT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "frame-graph build/test ownership missing: ${needle}")
  endif()
endforeach()

file(GLOB_RECURSE PRODUCT_SOURCES
  "${ROOT}/code/client/*.c" "${ROOT}/code/client/*.h"
  "${ROOT}/code/renderer/*.c" "${ROOT}/code/renderer/*.h"
  "${ROOT}/code/renderer2/*.c" "${ROOT}/code/renderer2/*.h"
  "${ROOT}/code/renderervk/*.c" "${ROOT}/code/renderervk/*.h"
  "${ROOT}/code/renderercommon/*.c" "${ROOT}/code/renderercommon/*.h"
  "${ROOT}/code/renderercommon/*.cpp" "${ROOT}/code/renderercommon/*.mm")
foreach(path IN LISTS PRODUCT_SOURCES)
  if(path MATCHES "/code/renderer/ral_frame_graph/")
    continue()
  endif()
  file(READ "${path}" text)
  if(text MATCHES "RalFrameGraph_(Compile|PlanExact)[(]"
      OR text MATCHES "RalFrameGraphExecution_(Compile|PlanExact)[(]"
      OR text MATCHES "RalFrameGraphRecord_(Record|ReceiptExact|RalOps)[(]"
      OR text MATCHES "RalFrameGraphSubmit_(Submit|ReceiptExact|FailureReceiptExact|RalOps)[(]")
    message(FATAL_ERROR "pure frame-graph compiler gained product execution authority: ${path}")
  endif()
endforeach()

message(STATUS "ral frame graph source policy: PASS")
