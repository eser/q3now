IF(NOT DEFINED SOURCE_ROOT)
	MESSAGE(FATAL_ERROR "SOURCE_ROOT is required")
ENDIF()

SET(_vk_path "${SOURCE_ROOT}/code/renderervk/vk.c")
SET(_vk_h_path "${SOURCE_ROOT}/code/renderervk/vk.h")
SET(_ral_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_dynamic_bind.c")
SET(_command_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_command.c")
SET(_bridge_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_bridge.h")
SET(_test_path "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c")
SET(_adopt_path "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c")
SET(_scene_path "${SOURCE_ROOT}/code/renderervk/tr_scene.c")
SET(_init_path "${SOURCE_ROOT}/code/renderervk/tr_init.c")
SET(_smoke_path "${SOURCE_ROOT}/tests/ral-effects-dynamic-bind-smoke.sh")
FOREACH(_path IN ITEMS "${_vk_path}" "${_vk_h_path}" "${_ral_path}"
		"${_command_path}" "${_bridge_path}" "${_test_path}" "${_adopt_path}"
		"${_scene_path}" "${_init_path}" "${_smoke_path}")
	IF(NOT EXISTS "${_path}")
		MESSAGE(FATAL_ERROR "missing RAL effects dynamic-bind source: ${_path}")
	ENDIF()
ENDFOREACH()
FILE(READ "${_vk_path}" _vk)
FILE(READ "${_vk_h_path}" _vk_h)
FILE(READ "${_ral_path}" _ral)
FILE(READ "${_command_path}" _command)
FILE(READ "${_bridge_path}" _bridge)
FILE(READ "${_test_path}" _test)
FILE(READ "${_adopt_path}" _adopt)
FILE(READ "${_scene_path}" _scene)
FILE(READ "${_init_path}" _init)
FILE(READ "${_smoke_path}" _smoke)

FUNCTION(slice_between out body begin_marker end_marker)
	STRING(FIND "${body}" "${begin_marker}" _begin)
	STRING(FIND "${body}" "${end_marker}" _end)
	IF(_begin EQUAL -1 OR _end EQUAL -1 OR NOT _begin LESS _end)
		MESSAGE(FATAL_ERROR "cannot isolate product span: ${begin_marker} -> ${end_marker}")
	ENDIF()
	MATH(EXPR _length "${_end} - ${_begin}")
	STRING(SUBSTRING "${body}" ${_begin} ${_length} _slice)
	SET(${out} "${_slice}" PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(require_text body needle label)
	STRING(FIND "${body}" "${needle}" _hit)
	IF(_hit EQUAL -1)
		MESSAGE(FATAL_ERROR "${label}: missing '${needle}'")
	ENDIF()
ENDFUNCTION()

FUNCTION(forbid_text body needle label)
	STRING(FIND "${body}" "${needle}" _hit)
	IF(NOT _hit EQUAL -1)
		MESSAGE(FATAL_ERROR "${label}: forbidden '${needle}'")
	ENDIF()
ENDFUNCTION()

FUNCTION(require_call_count body regex expected label)
	STRING(REGEX MATCHALL "${regex}" _hits "${body}")
	LIST(LENGTH _hits _count)
	IF(NOT _count EQUAL expected)
		MESSAGE(FATAL_ERROR "${label}: expected ${expected} matches of '${regex}', found ${_count}")
	ENDIF()
ENDFUNCTION()

slice_between(_bind "${_vk}"
	"static qboolean vk_effects_bind_ral("
	"static void vk_effects_set_viewport_scissor( void )")
slice_between(_viewport "${_vk}"
	"static void vk_effects_set_viewport_scissor( void )"
	"void RB_DrawRibbons( void )")
slice_between(_ribbon "${_vk}" "void RB_DrawRibbons( void )" "void RB_DrawRailRibbons( void )")
slice_between(_rail "${_vk}" "void RB_DrawRailRibbons( void )" "void vk_init_beam( void )")
slice_between(_beam "${_vk}" "void RB_DrawBeams( void )" "void vk_init_sprite( void )")
slice_between(_sprite "${_vk}" "void RB_DrawSprites( void )" "void vk_init_particle( void )")
slice_between(_effects_adopt "${_adopt}"
	"// Effects set 1 is a real dynamic UBO binding."
	"RETAIN_ADOPT( vk.particle.ral_compute_descriptor[i]")

# One pipeline-authoritative portable bind transaction: set0 has no offsets;
# set1 has exactly one uint32 dynamic offset. Both binds must be checked.
require_call_count("${_bind}" "Ral_CmdBindPipeline[(]" 1 "effects bind helper")
require_call_count("${_bind}" "Ral_CmdBindBindGroupDynamic[(]" 2 "effects bind helper")
require_text("${_bind}" "Ral_CmdBindBindGroupDynamic( cmd, 0, effectGroup, NULL, 0 )" "effects set0 bind")
require_text("${_bind}" "Ral_CmdBindBindGroupDynamic( cmd, 1, uboGroup, &dynamicOffset, 1 )" "effects set1 bind")
require_text("${_bind}" "if ( !cmd || !pipeline || !effectGroup || !uboGroup ) return qfalse;" "effects fail-closed preflight")

require_call_count("${_viewport}" "Ral_CmdSetViewport[(]" 1 "effects viewport helper")
require_call_count("${_viewport}" "Ral_CmdSetScissor[(]" 1 "effects viewport helper")
forbid_text("${_viewport}" "qvkCmd" "effects viewport helper")

SET(_draw_spans _ribbon _rail _beam _sprite)
FOREACH(_span_name IN LISTS _draw_spans)
	SET(_span "${${_span_name}}")
	forbid_text("${_span}" "qvkCmd" "${_span_name} product draw")
	require_text("${_span}" "vk_effects_set_viewport_scissor();" "${_span_name} viewport")
	require_text("${_span}" "vk_effects_bind_ral(" "${_span_name} bind")
	require_text("${_span}" "Ral_CmdDraw(" "${_span_name} draw")
ENDFOREACH()
require_call_count("${_ribbon}" "vk_effects_bind_ral[(]" 1 "ribbon bind inventory")
require_call_count("${_rail}" "vk_effects_bind_ral[(]" 1 "rail bind inventory")
require_call_count("${_beam}" "vk_effects_bind_ral[(]" 1 "beam bind inventory")
require_call_count("${_sprite}" "vk_effects_bind_ral[(]" 2 "sprite blend bind inventory")
require_call_count("${_ribbon}" "Ral_CmdDraw[(]" 1 "ribbon draw inventory")
require_call_count("${_rail}" "Ral_CmdDraw[(]" 1 "rail draw inventory")
require_call_count("${_beam}" "Ral_CmdDraw[(]" 1 "beam draw inventory")
require_call_count("${_sprite}" "Ral_CmdDraw[(]" 2 "sprite blend draw inventory")

# Native verification is default-off and submits the procedural families plus
# the retained particle/decal/atmospheric pools once per map. Evidence is
# emitted from each backend owner only after a RAL draw call.
require_text("${_init}" "ri.Cvar_Get( \"r_ralEffectsSmoke\", \"0\", CVAR_CHEAT )" "effects smoke default-off gate")
require_text("${_scene}" "static void R_InjectRalEffectsSmoke( const refdef_t *fd )" "effects smoke injector")
require_text("${_scene}" "!vk.ribbon.available || !vk.railRibbon.available" "effects smoke all-owner preflight")
FOREACH(_needle IN ITEMS "RE_AddRibbonToScene( &ribbon )" "RE_AddRailRibbonToScene( &rail )" "RE_AddBeamToScene( &beam )" "RE_AddSpriteToScene( &sprite )")
	require_text("${_scene}" "${_needle}" "effects smoke submission")
ENDFOREACH()
require_call_count("${_vk}" "vk_effects_smoke_receipt[(]" 8 "effects post-draw receipt inventory")
FOREACH(_span_name IN LISTS _draw_spans)
	require_text("${${_span_name}}" "vk_effects_smoke_receipt(" "${_span_name} post-draw receipt")
ENDFOREACH()
FOREACH(_needle IN ITEMS "arena1" "arena17" "ribbon" "rail-ribbon" "beam" "sprite" "particle" "decal" "atmospheric" "draws=([0-9]+)" "dynamic-offsets=" "expected_dynamic=1")
	require_text("${_smoke}" "${_needle}" "effects native smoke analyzer")
ENDFOREACH()

# Product ownership must wrap the real renderer UBO and raw descriptor set,
# then register one exact item range. Parallel/shadow buffers are forbidden.
require_text("${_vk_h}" "struct ralBindGroupLayout_s *ral_bgl_effects_ubo;" "effects layout owner")
require_text("${_vk_h}" "struct ralBuffer_s    *ral_buffer[NUM_COMMAND_BUFFERS];" "effects buffer wrappers")
require_text("${_vk_h}" "struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];" "effects group wrappers")
require_text("${_vk}" "e.dynamicOffset = qtrue;" "effects dynamic layout metadata")
require_text("${_vk}" "vk.set_layout_effects_ubo, 1, &e," "effects exact adopted layout")
require_text("${_effects_adopt}" "vk.effectsUbo.ral_buffer[i] = Ral_AdoptBuffer(" "effects raw-buffer adoption")
require_text("${_effects_adopt}" "vk.effectsUbo.ral_descriptor[i] = Ral_AdoptBindGroup(" "effects descriptor adoption")
require_call_count("${_effects_adopt}" "Ral_RegisterAdoptedBindGroupDynamicBuffer[(]" 1 "effects range registration")
require_text("${_effects_adopt}" "vk.effectsUbo.ral_descriptor[i], 0," "effects binding identity")
require_text("${_effects_adopt}" "vk.effectsUbo.ral_buffer[i], 0, item" "effects base/range identity")

# Backend rejects every portable authority mutation before emission and only
# publishes boundBindGroups after the Vulkan callback.
FOREACH(_needle IN ITEMS
	"dynamicOffsetCount != group->dynamicOffsetCount"
	"dynamicOffsetCount != group->layout->dynamicOffsetCount"
	"offset % alignment != 0"
	"dynamic->range > dynamic->buffer->size - dynamic->baseOffset - offset"
	"!cb->currentPipeline"
	"!ralVk_BindGroupBuffersGpuUseAllowed( group )"
	"cb->boundBindGroups[setIndex] = group;")
	require_text("${_ral}" "${_needle}" "Vulkan dynamic-bind authority")
ENDFOREACH()
FOREACH(_needle IN ITEMS
	"offsets[0] = 128u"
	"offsets[1] = 17u"
	"registered = qfalse"
	"range = 1800u"
	"group.backend = &otherBackend"
	"uniformBuffer.legacyMapped = qtrue"
	"command.currentPipeline = NULL"
	"bindCalls == 0u && command.boundBindGroups[3] == &sentinel")
	require_text("${_test}" "${_needle}" "dynamic-bind mutation host")
ENDFOREACH()

# Persistent and adopted commands share one portable lifecycle. There is no
# native/external bypass: exact begin must clear every weak binding before the
# next recording can observe it.
FOREACH(_needle IN ITEMS
	"cb->state != RAL_VK_CMD_RECORDING"
	"cb->lifecycle.state != RAL_COMMAND_RECORDING"
	"cb->currentPipeline = NULL;"
	"cb->currentLayout = VK_NULL_HANDLE;"
	"memset( cb->boundVertexBuffers, 0, sizeof( cb->boundVertexBuffers ) );"
	"cb->boundIndexBuffer = NULL;"
	"memset( cb->boundBindGroups, 0, sizeof( cb->boundBindGroups ) );"
	"cb->renderingActive = qfalse;")
	require_text("${_command}" "${_needle}" "exact command lifecycle authority")
ENDFOREACH()
STRING(FIND "${_vk}" "vk_ral_begin_command_exact( vk.cmd->ral_cmd, \"vk_begin_frame\" );" _exact_begin)
STRING(FIND "${_vk}" "Ral_ResetDebugLabelStats( vk.cmd->ral_cmd );" _label_reset)
IF(_exact_begin EQUAL -1 OR _label_reset EQUAL -1 OR NOT _exact_begin LESS _label_reset)
	MESSAGE(FATAL_ERROR "owned RAL command lifecycle must begin exactly before frame debug state")
ENDIF()
FOREACH(_surface IN ITEMS Ral_ResetExternalCommandState Ral_SetCommandBufferExternalLifecycle externalLifecycle)
	STRING(FIND "${_bridge}${_command}${_ral}${_vk}" "${_surface}" _retired_surface)
	IF(NOT _retired_surface EQUAL -1)
		MESSAGE(FATAL_ERROR "retired external command-lifecycle surface returned: ${_surface}")
	ENDIF()
ENDFOREACH()
FOREACH(_needle IN ITEMS
	"command.boundBindGroups[0] = &group;"
	"command.renderingActive = qtrue;"
	"Ral_BeginCommandBufferExact( &command, &commandRecording ) == ralSuccess"
	"command.boundVertexBuffers[0] == NULL"
	"command.boundIndexBuffer == NULL"
	"command.boundBindGroups[0] == NULL")
	require_text("${_test}" "${_needle}" "exact command begin reset host")
ENDFOREACH()

MESSAGE(STATUS "RAL procedural effects use portable dynamic bind offsets and RAL-only draw commands")
