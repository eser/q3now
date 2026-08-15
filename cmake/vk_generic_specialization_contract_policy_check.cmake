cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderervk/vk.c" VKC)
file(READ "${ROOT}/code/renderervk/vk_generic_specialization_contract.c" CONTRACT)
file(READ "${ROOT}/code/renderervk/vk_generic_specialization_contract.h" HEADER)
file(READ "${ROOT}/code/renderervk/vk_temporal_pipeline_factory.c" FACTORY)
file(READ "${ROOT}/tests/vk_generic_specialization_contract_test.c" TEST)
file(READ "${ROOT}/tests/vk_temporal_pipeline_factory_test.c" FACTORY_TEST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)

foreach(needle
    "VK_GENERIC_VERTEX_SPEC_COUNT = 1"
    "VK_GENERIC_FRAGMENT_SPEC_COUNT = 15"
    "VK_GENERIC_TOTAL_SPEC_COUNT = VK_GENERIC_VERTEX_SPEC_COUNT + VK_GENERIC_FRAGMENT_SPEC_COUNT"
    "VK_GENERIC_TRANSLATOR_SPEC_CAPACITY = 18"
    "0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 15u, 14u, 26u"
    "candidate.vertexMap[0].constantID = 16u"
    "candidate.fragmentMaps[i].offset = i * sizeof(uint32_t)"
    "candidate.fragmentInfo.dataSize = VK_GENERIC_FRAGMENT_SPEC_COUNT * sizeof(uint32_t)"
    "candidate.fragmentWords[0] != 0u"
    "candidate.fragmentWords[1] != 0u"
    "candidate.fragmentWords[2] != depthThresholdBits"
    "candidate.fragmentWords[3] != 0u"
    "candidate.fragmentWords[4] & ~texDomainMask"
    "candidate.fragmentWords[5] != 0u"
    "candidate.fragmentWords[6] > 7u"
    "facts->textureCount == 0u && candidate.fragmentWords[6] != 0u"
    "candidate.fragmentWords[7] != 0u"
    "!isfinite(fixedColor)"
    "!isfinite(fixedAlpha)"
    "!facts->shaderFog && candidate.fragmentWords[10] != 0u"
    "!isfinite(depthFade) || depthFade <= 0.0f"
    "candidate.fragmentWords[12] != 0u"
    "candidate.fragmentWords[13] != 0u"
    "candidate.fragmentWords[14] > facts->textureCount + 1u")
  string(FIND "${HEADER}\n${CONTRACT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "generic specialization contract missing: ${needle}")
  endif()
endforeach()
string(FIND "${VKC}" "ralSpecConstant_t                 specs[VK_GENERIC_TRANSLATOR_SPEC_CAPACITY]" translator_capacity)
if(translator_capacity EQUAL -1)
  message(FATAL_ERROR "exact gpInfo translator scratch must use the shared 18-entry capacity")
endif()

string(REGEX MATCHALL "VK_GenericSpecializationAuthor[ \t\r\n]*\\(" author_calls "${VKC}")
list(LENGTH author_calls author_count)
if(NOT author_count EQUAL 1)
  message(FATAL_ERROR "production create_pipeline must call the exact author once")
endif()
foreach(forbidden "vert_spec_entry.constantID" "spec_entries\\[1\\]\\.constantID" "frag_spec_info.mapEntryCount = 15")
  string(FIND "${VKC}" "VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index ) {" create_start)
  string(FIND "${VKC}" "static uint32_t vk_alloc_pipeline" create_end)
  if(create_start EQUAL -1 OR create_end EQUAL -1 OR create_end LESS create_start)
    message(FATAL_ERROR "cannot isolate production create_pipeline")
  endif()
  math(EXPR create_len "${create_end}-${create_start}")
  string(SUBSTRING "${VKC}" ${create_start} ${create_len} CREATE_PIPELINE)
  if(CREATE_PIPELINE MATCHES "${forbidden}")
    message(FATAL_ERROR "parallel manual specialization authoring survived: ${forbidden}")
  endif()
endforeach()
string(FIND "${FACTORY}" "VK_GenericTemporalSpecializationValidate(base,&specializationFacts,NULL)" split_validate)
string(FIND "${FACTORY}" "VK_TemporalGenericCatalogSelect(&input->key,&catalog)" split_catalog)
string(FIND "${FACTORY}" "BaseFingerprint(base,&fingerprint)" split_fingerprint)
string(FIND "${FACTORY}" "candidate[i]=ops->create" split_create)
if(split_validate EQUAL -1 OR split_catalog EQUAL -1 OR split_fingerprint EQUAL -1 OR
   split_create EQUAL -1 OR split_validate LESS split_catalog OR
   split_fingerprint LESS split_validate OR split_create LESS split_fingerprint)
  message(FATAL_ERROR "split factory must select facts, validate exact schema, fingerprint, then create")
endif()
foreach(needle
    "VkPipelineMultisampleStateCreateInfo multisample"
    "VkPipelineDepthStencilStateCreateInfo depthStencil"
    "for ( i = 0; i < VK_GENERIC_FRAGMENT_SPEC_COUNT; ++i )"
    "f.fragmentWords[1]=0x80000000u"
    "f.fragmentWords[6]=8u"
    "for(i=1u;i<=7u;++i){f.fragmentWords[6]=i;REJECT_CURRENT();}"
    "f.fragmentWords[10]=4u"
    "f.fragmentWords[11]=0x7f800000u"
    "SPLIT_REJECT_SLOT(14,3u)"
    "s_createPipeCount==3")
  string(FIND "${TEST}\n${FACTORY_TEST}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "fixture/mutation coverage missing: ${needle}")
  endif()
endforeach()
foreach(forbidden "Cvar" "Ral_" "BeginRendering" "vkCmd" "r_temporal")
  if(CONTRACT MATCHES "${forbidden}")
    message(FATAL_ERROR "pure specialization contract gained runtime authority: ${forbidden}")
  endif()
endforeach()
file(GLOB PRODUCT_TUS "${ROOT}/code/renderervk/*.c")
foreach(tu IN LISTS PRODUCT_TUS)
  file(READ "${tu}" text)
  if(NOT tu STREQUAL "${ROOT}/code/renderervk/vk.c" AND
     NOT tu STREQUAL "${ROOT}/code/renderervk/vk_temporal_generic_recipe_table.c" AND
     NOT tu STREQUAL "${ROOT}/code/renderervk/vk_generic_specialization_contract.c" AND
     text MATCHES "VK_GenericSpecializationAuthor")
    message(FATAL_ERROR "author authority leaked outside vk.c: ${tu}")
  endif()
  if(NOT tu STREQUAL "${ROOT}/code/renderervk/vk_temporal_pipeline_factory.c" AND
     NOT tu STREQUAL "${ROOT}/code/renderervk/vk_temporal_generic_recipe_table.c" AND
     NOT tu STREQUAL "${ROOT}/code/renderervk/vk_generic_specialization_contract.c" AND
     text MATCHES "VK_GenericTemporalSpecializationValidate")
	message(FATAL_ERROR "validator authority leaked outside factory/recipe/definition: ${tu}")
  endif()
endforeach()
if(NOT CMAKE_TEXT MATCHES "vk_generic_specialization_contract_test" OR
   NOT CMAKE_TEXT MATCHES "vk_generic_specialization_contract.c" OR
   NOT CMAKE_TEXT MATCHES "AUX_SOURCE_DIRECTORY\\(code/renderervk RENDERER_VK_SRCS\\)")
  message(FATAL_ERROR "specialization contract product/test ownership missing")
endif()
message(STATUS "vk generic specialization source policy: PASS")
