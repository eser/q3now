cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderervk/vk.c" VKC)
file(READ "${ROOT}/code/renderervk/vk.h" VKH)
file(READ "${ROOT}/code/renderervk/vk_temporal_pipeline_factory.c" FACTORY)
file(READ "${ROOT}/code/renderervk/vk_temporal_pipeline_factory.h" HEADER)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)
foreach(needle
    "vk_ral_create_pipeline_from_gpinfo_exact_core"
    "const vkTemporalSpirvOverrides_t *overrides"
    "vk_ral_create_pipeline_from_gpinfo_exact_core( ci_vk, layout, colorFormats,"
    "numColorFormats, depthFormat, NULL, debugName )"
    "VK_TemporalPipelineLayoutAcquire( layoutOwner )"
    "owner->retiringLease"
    "ops->drain( owner->retiringLayout->backend )"
    "VK_GenericTemporalSpecializationValidate"
    "BlobEqual(iqmFs,blobs->iqmOrdinaryFragment)"
    "owner->iqmLayoutGeneration == input->iqmLayoutGeneration"
    "owner->genericBaseFingerprint == genericFingerprint"
    "candidate[3] = ops->create"
    "colorBlend.attachmentCount = 3"
    "vkBlends[1].colorWriteMask = recipe.colorBlends[1].writeMask"
    "vkBlends[2].colorWriteMask = recipe.colorBlends[2].writeMask")
  string(FIND "${VKC}\n${FACTORY}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "factory authority missing: ${needle}")
  endif()
endforeach()
string(REGEX MATCHALL "owner->allocationGeneration==UINT32_MAX" allocation_overflow_guards "${FACTORY}")
list(LENGTH allocation_overflow_guards allocation_overflow_guard_count)
if(NOT allocation_overflow_guard_count EQUAL 2)
  message(FATAL_ERROR "generic and IQM factories must each reject allocation-generation overflow")
endif()
foreach(needle
    "VK_TemporalGenericCatalogSelect(&input->key,&catalog)"
    "for(i=0;i<3u;++i)"
    "candidate=ops->create(&exact,layout,recipe.colorFormats,3u"
    "owner->pendingDrain"
    "if(!ops->drain(owner->backend))return qfalse"
	"owner->allocationGeneration==UINT32_MAX"
    "ri.fog=layoutOwner->fog")
  string(FIND "${FACTORY}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "split generic/IQM factory authority missing: ${needle}")
  endif()
endforeach()
string(FIND "${FACTORY}" "input->key.shaderFog && !layoutOwner->fog" shader_fog_push_coupling)
if(NOT shader_fog_push_coupling EQUAL -1)
	message(FATAL_ERROR "temporal pipeline factory policy: shader UBO fog must not require build push-fog layout")
endif()
if(NOT HEADER MATCHES "WIRED_TEMPORAL_REPRESENTATIVE_FACTORY_TEST_ONLY" OR
   NOT CMAKE_TEXT MATCHES "WIRED_TEMPORAL_REPRESENTATIVE_FACTORY_TEST_ONLY=1")
  message(FATAL_ERROR "legacy combined representative factory must be test-only")
endif()
string(REGEX MATCHALL "WIRED_TEMPORAL_REPRESENTATIVE_FACTORY_TEST_ONLY=1" legacy_test_def "${CMAKE_TEXT}")
list(LENGTH legacy_test_def legacy_test_def_count)
if(NOT legacy_test_def_count EQUAL 1)
  message(FATAL_ERROR "legacy combined factory compile authority must belong to exactly one host test")
endif()
foreach(needle
    "ci_vk->stageCount != 2"
    "ci_vk->pStages[0].stage != VK_SHADER_STAGE_VERTEX_BIT"
    "ci_vk->pStages[1].stage != VK_SHADER_STAGE_FRAGMENT_BIT"
    "vk_ral_exact_temporal_base_valid( ci_vk )"
    "memcpy( &value, (const uint8_t *)si->pData + e->offset, sizeof(value) )")
  string(FIND "${VKC}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "direct translator validation missing: ${needle}")
  endif()
endforeach()
string(REGEX MATCHALL "vk_ral_create_pipeline_from_gpinfo_exact_spirv[ \t\r\n]*\\(" direct_defs "${VKC}")
list(LENGTH direct_defs direct_count)
if(NOT direct_count EQUAL 1)
  message(FATAL_ERROR "direct SPIR-V translator must have exactly one definition and zero product calls")
endif()
string(FIND "${VKC}" "ralPipeline_t *vk_ral_create_pipeline_from_gpinfo_exact_spirv(" direct_start)
string(FIND "${VKC}" "qboolean vk_temporal_pipeline_lookup_blob(" direct_end)
if(direct_start EQUAL -1 OR direct_end EQUAL -1 OR direct_end LESS direct_start)
  message(FATAL_ERROR "cannot isolate direct SPIR-V override seam")
endif()
math(EXPR direct_len "${direct_end}-${direct_start}")
string(SUBSTRING "${VKC}" ${direct_start} ${direct_len} DIRECT_SEAM)
foreach(forbidden "vkCreateShaderModule" "vk_shader_blob_record" "vk_shader_blob_table" "vk_shader_blob_reset")
  if(DIRECT_SEAM MATCHES "${forbidden}" OR FACTORY MATCHES "${forbidden}")
    message(FATAL_ERROR "direct/factory seam gained module cache or registry authority: ${forbidden}")
  endif()
endforeach()
foreach(forbidden "vkDestroyPipeline" "qvkDestroyPipeline" "memcmp\\(&base" "memcmp\\( &owner->blobs")
  if(FACTORY MATCHES "${forbidden}")
    message(FATAL_ERROR "factory lifecycle/key bypass: ${forbidden}")
  endif()
endforeach()
if(NOT VKH MATCHES "sizeof\\( vkUniform_t \\) == 608")
  message(FATAL_ERROR "ordinary vkUniform_t 608-byte ABI drift")
endif()
foreach(forbidden "Cvar" "Ral_Cmd" "BeginRendering" "vkCmd" "R_TemporalMotionPayloadEnsure" "r_temporal")
  if(FACTORY MATCHES "${forbidden}")
    message(FATAL_ERROR "definition-only factory gained runtime authority: ${forbidden}")
  endif()
endforeach()
file(GLOB PRODUCT_TUS "${ROOT}/code/renderervk/*.c")
foreach(tu IN LISTS PRODUCT_TUS)
  if(tu STREQUAL "${ROOT}/code/renderervk/vk_temporal_pipeline_factory.c" OR
     tu STREQUAL "${ROOT}/code/renderervk/vk.c")
    continue()
  endif()
  file(READ "${tu}" text)
  if(tu STREQUAL "${ROOT}/code/renderervk/vk_temporal_generic_pipeline_table.c")
    if(NOT text MATCHES "VK_TemporalGenericPipelineFactoryEnsure" OR
       text MATCHES "VK_TemporalPipelineFactoryEnsure|VK_TemporalIqmPipelineFactoryEnsure|vk_ral_create_pipeline_from_gpinfo_exact_spirv")
      message(FATAL_ERROR "A2c2b table may own only the split generic exact3 factory")
    endif()
    continue()
  endif()
  if(tu STREQUAL "${ROOT}/code/renderervk/vk_temporal_motion_materialization.c")
    if(NOT text MATCHES "VK_TemporalPipelineLayoutEnsure" OR
       text MATCHES "VK_TemporalPipelineFactoryEnsure|VK_TemporalGenericPipelineFactoryEnsure|VK_TemporalIqmPipelineFactoryEnsure|vk_ral_create_pipeline_from_gpinfo_exact_spirv")
      message(FATAL_ERROR "A2b may materialize only the pipeline layout owner")
    endif()
    continue()
  endif()
  foreach(token VK_TemporalPipelineLayoutEnsure VK_TemporalPipelineFactoryEnsure
      VK_TemporalGenericPipelineFactoryEnsure VK_TemporalIqmPipelineFactoryEnsure
      vk_ral_create_pipeline_from_gpinfo_exact_spirv)
    if(text MATCHES "${token}")
      message(FATAL_ERROR "temporal factory caller leaked into ${tu}: ${token}")
    endif()
  endforeach()
endforeach()
foreach(token VK_TemporalPipelineLayoutEnsure VK_TemporalPipelineFactoryEnsure
    VK_TemporalGenericPipelineFactoryEnsure VK_TemporalIqmPipelineFactoryEnsure)
  if(VKC MATCHES "${token}")
    message(FATAL_ERROR "vk.c must expose only the direct-SPIR-V translator, not call the factory")
  endif()
endforeach()
if(NOT CMAKE_TEXT MATCHES "vk_temporal_pipeline_factory_test" OR
   NOT CMAKE_TEXT MATCHES "vk_temporal_pipeline_factory.c" OR
   NOT CMAKE_TEXT MATCHES "AUX_SOURCE_DIRECTORY\\(code/renderervk RENDERER_VK_SRCS\\)")
  message(FATAL_ERROR "factory product/test ownership missing")
endif()
message(STATUS "vk temporal pipeline factory source policy: PASS")
