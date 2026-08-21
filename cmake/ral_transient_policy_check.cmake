cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderer/ral/ral_transient.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_transient.c" CORE)
file(READ "${ROOT}/tests/ral_transient_plan_test.c" TEST)
file(READ "${ROOT}/tests/ral_webgpu_transient_plan_test.c" WEBGPU_TEST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)
foreach(forbidden "Vk[A-Z]" "VK_" "WGPU" "MTL")
  if(HEADER MATCHES "${forbidden}" OR CORE MATCHES "${forbidden}")
    message(FATAL_ERROR "transient contract leaked native API: ${forbidden}")
  endif()
endforeach()
foreach(needle
    "RAL_TRANSIENT_SCHEMA_VERSION 1u"
    "RAL_TRANSIENT_MAX_REQUESTS 64u"
    "RAL_TRANSIENT_PLAN_EXPLICIT_ALIAS"
    "RAL_TRANSIENT_PLAN_MANAGED_DISJOINT"
	"policy->backendType == RAL_BACKEND_WEBGPU"
	"policy->backendType == RAL_BACKEND_WEBGL2"
    "slot->lastPass < request->firstPass"
    "slot->compatibilityKey==request->compatibilityKey"
    "delta>policy->budgetBytes-candidate->peakBytes"
    "lifecycle->nextBatchGeneration>=UINT64_MAX-1u"
	"authority->planIdentity==plan"
	"Ral_TransientPlanExact(&lifecycle->boundPlan,plan)"
    "fenceCompleted!=qtrue")
  string(FIND "${HEADER}\n${CORE}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transient contract missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "requests[3].firstPass=1u"
    "requests[2].resourceIdentity=requests[0].resourceIdentity"
    "requests[0].alignment=3u"
    "policy.budgetBytes=2048u"
    "Ral_TransientBatchCancel"
    "Ral_TransientBatchRetire(&lifecycle,&submitted,&plan,qfalse"
    "lifecycle.nextBatchGeneration=UINT64_MAX-1u")
  string(FIND "${TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transient mutation missing: ${needle}")
  endif()
endforeach()
foreach(needle
    "RAL_BACKEND_WEBGPU"
    "explicitAliasing=qfalse"
    "RAL_TRANSIENT_PLAN_MANAGED_DISJOINT"
    "plan.slotCount==3u&&plan.aliasCount==0u"
	"!Ral_TransientPlanBuild(&policy,requests,3u,&plan)")
  string(FIND "${WEBGPU_TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "WebGPU transient fixture missing: ${needle}")
  endif()
endforeach()
foreach(needle "ral_transient_plan_test" "ral_webgpu_transient_plan_test"
    "ral_transient.c" "ral_transient_source_policy_contract")
  string(FIND "${CMAKE_TEXT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "transient test ownership missing: ${needle}")
  endif()
endforeach()
message(STATUS "ral transient source policy: PASS")
