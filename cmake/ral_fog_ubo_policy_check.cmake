# SPDX-License-Identifier: GPL-3.0-or-later
# Enhanced-fog portability policy: bounded per-draw UBO, no renderer push range.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.h" VKH)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VKC)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_shade.c" SHADE)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_spearmint.c" SPEARMINT)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_backend.c" BACKEND)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_init.c" VK_INIT)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_shader.c" VK_SHADER)
FILE(READ "${SOURCE_ROOT}/code/render/frontend/wired_fog_runtime_policy.h" FOG_RUNTIME_POLICY)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_pipeline_factory.c" TEMPORAL_FACTORY)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_map.c" VK_MAP)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_shader_cohort.c" TEMPORAL_COHORT)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/gen_frag.tmpl" GEN_FRAG)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/light_frag.tmpl" LIGHT_FRAG)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/forwardplus_lit.frag" FORWARDPLUS_FRAG)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/advanced_fog.glsl" ADVANCED_FOG)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/frag_tx0_bindless.msl" GEN_MSL)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/frag_tx0_bindless.wgsl" GEN_WGSL)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/frag_light_bindless.msl" LIGHT_MSL)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/frag_light_bindless.wgsl" LIGHT_WGSL)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/forwardplus_lit_frag_spv.msl" FORWARDPLUS_MSL)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/forwardplus_lit_frag_spv.wgsl" FORWARDPLUS_WGSL)
FILE(READ "${SOURCE_ROOT}/code/qcommon/q_feats.h" FEATURES)
FILE(READ "${SOURCE_ROOT}/CMakeLists.txt" BUILD)
FILE(READ "${SOURCE_ROOT}/Makefile" MAKEFILE)
FILE(READ "${SOURCE_ROOT}/tests/advanced-fog-runtime-check.sh" FOG_RUNTIME_HARNESS)

FUNCTION(REQUIRE_TEXT BODY NEEDLE MESSAGE_TEXT)
	STRING(FIND "${BODY}" "${NEEDLE}" POS)
	IF(POS EQUAL -1)
		MESSAGE(FATAL_ERROR "${MESSAGE_TEXT}: ${NEEDLE}")
	ENDIF()
ENDFUNCTION()

FUNCTION(FORBID_TEXT BODY NEEDLE MESSAGE_TEXT)
	STRING(FIND "${BODY}" "${NEEDLE}" POS)
	IF(NOT POS EQUAL -1)
		MESSAGE(FATAL_ERROR "${MESSAGE_TEXT}: ${NEEDLE}")
	ENDIF()
ENDFUNCTION()

FOREACH(NEEDLE IN ITEMS
	"#define WIRED_ADVANCED_FOG_UBO_OFFSET 608u"
	"#define WIRED_ADVANCED_FOG_UBO_SIZE    32u"
	"vec4_t advancedFogColorDensity;"
	"vec4_t advancedFogTypeFarEnabled;"
	"sizeof( vkUniform_t ) == 640"
	"void vk_update_fog_uniform(")
	REQUIRE_TEXT("${VKH}" "${NEEDLE}" "enhanced-fog host UBO ABI missing")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"OPTION(USE_FOG_SYSTEM \"Compile the enhanced linear/exp/exp2 fog path (default OFF until TASK-85 activation)\" OFF)"
	"IF(USE_FOG_SYSTEM)"
	"ADD_COMPILE_DEFINITIONS(FEAT_FOG_SYSTEM=1)"
	"ADD_COMPILE_DEFINITIONS(FEAT_FOG_SYSTEM=0)")
	REQUIRE_TEXT("${BUILD}" "${NEEDLE}" "enhanced-fog build permutation missing")
ENDFOREACH()
REQUIRE_TEXT("${FEATURES}" "#ifndef FEAT_FOG_SYSTEM\n#define FEAT_FOG_SYSTEM                   0"
	"enhanced-fog header default is not override-safe")
FOREACH(NEEDLE IN ITEMS
	"USE_FOG_SYSTEM ?= 0"
	"CMAKE_FOG_FLAG := -DUSE_FOG_SYSTEM=ON"
	"CMAKE_FOG_FLAG := -DUSE_FOG_SYSTEM=OFF"
	"$(CMAKE_WASM_FLAG) $(CMAKE_FOG_FLAG)")
	REQUIRE_TEXT("${MAKEFILE}" "${NEEDLE}" "canonical build fog permutation missing")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"->advancedFogColorDensity,"
	"vk_world.advancedFogColorDensity"
	"->advancedFogTypeFarEnabled,"
	"vk_world.advancedFogTypeFarEnabled"
	"vk_tess_publish_shadow_range( offset, vk.uniform_item_size )")
	REQUIRE_TEXT("${SHADE}" "${NEEDLE}" "bounded per-draw fog snapshot missing")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"vk_world.advancedFogColorDensity[0] = R_SRGBToLinear( color[0] );"
	"vk_world.advancedFogColorDensity[1] = R_SRGBToLinear( color[1] );"
	"vk_world.advancedFogColorDensity[2] = R_SRGBToLinear( color[2] );"
	"vk_world.advancedFogColorDensity[3] = density;"
	"vk_world.advancedFogTypeFarEnabled[0] = (float)fogType;"
	"vk_world.advancedFogTypeFarEnabled[1] = farClip;"
	"vk_world.advancedFogTypeFarEnabled[2] = enabled ? 1.0f : 0.0f;")
	REQUIRE_TEXT("${VKC}" "${NEEDLE}" "enhanced-fog stash publication missing")
ENDFOREACH()

REQUIRE_TEXT("${VKC}" "desc.pushConstantRangeCount = 0;"
	"ordinary main layout must be push-free")
REQUIRE_TEXT("${VKC}" "desc.pPushConstantRanges    = NULL;"
	"ordinary main layout must publish no push ranges")
REQUIRE_TEXT("${VKC}" "WIRED_ENTITY_MAT_SET + 1,\n\t\t\t0u, 0u,"
	"RAL-adopted ordinary layout must be push-free")
REQUIRE_TEXT("${TEMPORAL_FACTORY}" "Fog state is part of the ordinary set-0 UBO; temporal layouts are push-free."
	"temporal layout push-free authority missing")
REQUIRE_TEXT("${TEMPORAL_COHORT}" "Generic temporal shaders share the ordinary push-free set-0 UBO ABI."
	"temporal recipe push-free authority missing")

FOREACH(NEEDLE IN ITEMS
	"vec4 advancedFogColorDensity;"
	"vec4 advancedFogTypeFarEnabled;"
	"#include \"advanced_fog.glsl\"")
	REQUIRE_TEXT("${GEN_FRAG}" "${NEEDLE}" "portable generic shader UBO tail missing")
ENDFOREACH()

FOREACH(SHADER IN ITEMS LIGHT_FRAG FORWARDPLUS_FRAG)
	FOREACH(NEEDLE IN ITEMS
		"vec4 advancedFogColorDensity;"
		"vec4 advancedFogTypeFarEnabled;"
		"#include \"advanced_fog.glsl\"")
		REQUIRE_TEXT("${${SHADER}}" "${NEEDLE}"
			"portable additive shader UBO tail/model missing")
	ENDFOREACH()
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"bool wired_advanced_fog_enabled()"
	"float wired_advanced_fog_amount()"
	"1.0 / max( gl_FragCoord.w, 0.000001 )"
	"viewDepth / advancedFogTypeFarEnabled.y"
	"1.0 - exp( -opticalDepth )"
	"1.0 - exp( -( opticalDepth * opticalDepth ) )")
	REQUIRE_TEXT("${ADVANCED_FOG}" "${NEEDLE}"
		"shared portable advanced-fog distance model missing")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"if ( wired_advanced_fog_enabled() )"
	"base.rgb = mix( base.rgb, advancedFogColorDensity.rgb, fogAmount );"
	"else {"
	"base = mix( base, fog * fogColor, fog.a );")
	REQUIRE_TEXT("${GEN_FRAG}" "${NEEDLE}"
		"generic advanced/classic single-path composition missing")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"float fogAmount = wired_advanced_fog_amount();"
	"if ( !wired_advanced_fog_enabled() )"
	"fogAmount = fog.a;"
	"base.xyz *= 1.0 - fogAmount;")
	REQUIRE_TEXT("${LIGHT_FRAG}" "${NEEDLE}"
		"PMLIGHT additive fog attenuation missing")
ENDFOREACH()
REQUIRE_TEXT("${FORWARDPLUS_FRAG}"
	"lit *= 1.0 - wired_advanced_fog_amount();"
	"Forward+ additive fog attenuation missing")

FOREACH(PORTABLE IN ITEMS GEN_MSL GEN_WGSL LIGHT_MSL LIGHT_WGSL FORWARDPLUS_MSL FORWARDPLUS_WGSL)
	FOREACH(NEEDLE IN ITEMS
		"advancedFogColorDensity"
		"advancedFogTypeFarEnabled"
		"wired_advanced_fog_amount"
		"exp(")
		REQUIRE_TEXT("${${PORTABLE}}" "${NEEDLE}"
			"committed MSL/WGSL advanced-fog artifact is stale")
	ENDFOREACH()
ENDFOREACH()
REQUIRE_TEXT("${GEN_MSL}" "advancedFogColorDensity.xyz"
	"MSL generic fog-colour composition missing")

REQUIRE_TEXT("${SPEARMINT}" "vk_update_fog_uniform("
	"enhanced-fog lifecycle does not publish to UBO stash")

# TASK-85.4: advanced fog is opt-in map state. The master gate suppresses the
# advanced path; explicit map types win; legacy fog stays on fogCollapse unless
# the user deliberately selects a -1/linear/exp/exp2 fallback. Global fog is
# never synthesized here: only a valid producer-published tr.globalFogType can
# become authoritative.
FOREACH(INIT_SOURCE IN ITEMS VK_INIT)
	FOREACH(NEEDLE IN ITEMS
		"ri.Cvar_Get( \"r_useGlFog\", \"1\""
		"ri.Cvar_Get( \"r_defaultFogParmsType\", \"-1\""
		"-1=classic fogCollapse, 0=linear, 1=exp, 2=exp2."
		"ri.Cvar_CheckRange( r_defaultFogParmsType, \"-1\", \"2\", CV_INTEGER )")
		REQUIRE_TEXT("${${INIT_SOURCE}}" "${NEEDLE}"
			"renderer fog authority cvar contract missing")
	ENDFOREACH()
ENDFOREACH()
FOREACH(RUNTIME_SOURCE IN ITEMS SPEARMINT)
	FOREACH(NEEDLE IN ITEMS
		"static qboolean R_AdvancedFogRuntimeEnabled( void )"
		"r_useGlFog && r_useGlFog->integer"
		"wired_fog_runtime_resolve_volume_type("
		"r_defaultFogParmsType ? r_defaultFogParmsType->integer : -1"
		"static fogType_t R_ResolveGlobalFogType( void )"
		"wired_fog_runtime_resolve_global_type(")
		REQUIRE_TEXT("${${RUNTIME_SOURCE}}" "${NEEDLE}"
			"renderer advanced-fog runtime authority missing")
	ENDFOREACH()
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"WIRED_FOG_RUNTIME_NONE = 0"
	"WIRED_FOG_RUNTIME_LINEAR = 1"
	"WIRED_FOG_RUNTIME_EXP = 2"
	"WIRED_FOG_RUNTIME_EXP2 = 3"
	"if ( !enabled )"
	"authoredType >= WIRED_FOG_RUNTIME_LINEAR"
	"if ( fallbackType < 0 || fallbackType > 2 )"
	"return WIRED_FOG_RUNTIME_LINEAR + fallbackType;"
	"wired_fog_runtime_resolve_global_type(")
	REQUIRE_TEXT("${FOG_RUNTIME_POLICY}" "${NEEDLE}"
		"backend-neutral advanced-fog runtime policy missing")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"ADD_EXECUTABLE(wired_fog_runtime_policy_test"
	"ADD_TEST(NAME wired_fog_runtime_policy_contract"
	"backend.vulkan;backend.metal;backend.webgpu;feature.fog")
	REQUIRE_TEXT("${BUILD}" "${NEEDLE}"
		"advanced-fog runtime policy test registration missing")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"shader.fogParms.type = FT_LINEAR;"
	"shader.fogParms.type = FT_EXP;"
	"shader.fogParms.type = FT_EXP2;")
	REQUIRE_TEXT("${VK_SHADER}" "${NEEDLE}"
		"map-authored advanced-fog type parser missing")
ENDFOREACH()

# TASK-85.5: the current Vulkan RAL-tier renderer proves the real-map path.
# Deprecated GL1/GL2 trees are intentionally excluded; future RAL backends
# reuse the same portable shader artifacts and fixture via conformance gates.
FOREACH(NEEDLE IN ITEMS
	"if ( out->parms.type != FT_NONE )"
	"Advanced fog volume: shader=%s type=%d density=%g farClip=%g depthForOpaque=%g")
	REQUIRE_TEXT("${VK_MAP}" "${NEEDLE}"
		"real-map advanced-fog runtime receipt missing")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"fogparms ( .75 .38 0 ) 800 exp2 0.006 1000"
	"'set r_customwidth 1280' 'set r_customheight 720'"
	"'set r_useGlFog 1' 'set r_defaultFogParmsType -1'"
	"Advanced fog volume: shader=textures/sfx/fog_intel type=3 density=0.006 farClip=1000 depthForOpaque=800"
	"run_renderer vulkan"
	"run_headless")
	REQUIRE_TEXT("${FOG_RUNTIME_HARNESS}" "${NEEDLE}"
		"real-map advanced-fog runtime harness contract missing")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"test-advanced-fog-runtime: copy-all $(PNG2RAW_BIN)"
	"DEV=1 USE_FOG_SYSTEM=1 make test-advanced-fog-runtime")
	REQUIRE_TEXT("${MAKEFILE}" "${NEEDLE}"
		"canonical advanced-fog runtime target missing")
ENDFOREACH()
# TASK-85.3: publication belongs to draw-batch transitions, never the same-sort
# fast path. Each relevant list flushes the previous batch, publishes fogNum,
# begins the new batch, then disables fog only after its final flush.
STRING(REGEX MATCHALL "RB_Fog[(] fogNum [)]" BACKEND_FOG_PUBLICATIONS "${BACKEND}")
LIST(LENGTH BACKEND_FOG_PUBLICATIONS BACKEND_FOG_PUBLICATION_COUNT)
IF(NOT BACKEND_FOG_PUBLICATION_COUNT EQUAL 3)
	MESSAGE(FATAL_ERROR
		"generic/PMLIGHT/Forward+ must own exactly three fogNum publications")
ENDIF()
STRING(REGEX MATCHALL "R_FogOff[(][)]" BACKEND_FOG_RESETS "${BACKEND}")
LIST(LENGTH BACKEND_FOG_RESETS BACKEND_FOG_RESET_COUNT)
IF(NOT BACKEND_FOG_RESET_COUNT EQUAL 3)
	MESSAGE(FATAL_ERROR
		"generic/PMLIGHT/Forward+ must own exactly three list-end fog resets")
ENDIF()
FOREACH(NEEDLE IN ITEMS
	"RB_EndSurface();\n\t\t\t}\n#if FEAT_FOG_SYSTEM\n\t\t\tRB_Fog( fogNum );\n#endif\n\t\t\tRB_BeginSurface( shader, fogNum );"
	"if ( oldShader != NULL )\n\t\t\t\tRB_EndSurface();\n#if FEAT_FOG_SYSTEM\n\t\t\tRB_Fog( fogNum );\n#endif\n\t\t\tRB_BeginSurface( shader, fogNum );")
	REQUIRE_TEXT("${BACKEND}" "${NEEDLE}"
		"lit/Forward+ fog publication is not ordered after the old batch flush")
ENDFOREACH()
REQUIRE_TEXT("${BACKEND}"
	"same-sort fast path deliberately keeps it unchanged.\n\t\t\tRB_Fog( fogNum );"
	"generic fog publication/fast-path authority missing")
FOREACH(NEEDLE IN ITEMS
	"if ( oldShader != NULL ) {\n\t\tRB_EndSurface();\n\t}\n#if FEAT_FOG_SYSTEM\n\tR_FogOff();\n#endif"
	"if ( oldShader != NULL )\n\t\tRB_EndSurface();\n#if FEAT_FOG_SYSTEM\n\tR_FogOff();\n#endif")
	REQUIRE_TEXT("${BACKEND}" "${NEEDLE}"
		"fog reset must occur after the draw list's final batch flush")
ENDFOREACH()

SET(RENDERER_FOG "${VKH}\n${VKC}\n${SHADE}\n${SPEARMINT}\n${TEMPORAL_FACTORY}\n${TEMPORAL_COHORT}")
FOREACH(FORBIDDEN IN ITEMS
	"WIRED_FOG_PUSH"
	"VK_TEMPORAL_FOG_PUSH"
	"vk_update_fog_push"
	"Ral_CmdPushConstantsLayoutExact")
	FORBID_TEXT("${RENDERER_FOG}" "${FORBIDDEN}"
		"enhanced fog regained renderer push-constant ownership")
ENDFOREACH()

MESSAGE(STATUS "RAL enhanced-fog UBO policy: bounded 32-byte tail; ordinary/temporal layouts push-free")
