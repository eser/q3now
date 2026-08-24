cmake_minimum_required(VERSION 3.16)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_resolve.h" H)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_resolve.c" C)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/temporal_resolve.comp" SHADER)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/shaders.manifest.mjs" MANIFEST)
file(READ "${ROOT}/tests/vk_temporal_resolve_test.c" TEST)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_history_store.c" STORE)
file(READ "${ROOT}/tests/vk_temporal_history_store_test.c" STORE_TEST)

function(require_text haystack needle why)
	string(FIND "${${haystack}}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "${why}: missing `${needle}`")
	endif()
endfunction()
function(require_count haystack regex expected why)
	string(REGEX MATCHALL "${regex}" hits "${${haystack}}")
	list(LENGTH hits count)
	if(NOT count EQUAL expected)
		message(FATAL_ERROR "${why}: expected ${expected}, got ${count} for `${regex}`")
	endif()
endfunction()
function(require_order haystack first second why)
	string(FIND "${${haystack}}" "${first}" first_pos)
	string(FIND "${${haystack}}" "${second}" second_pos)
	if(first_pos EQUAL -1 OR second_pos EQUAL -1 OR NOT first_pos LESS second_pos)
		message(FATAL_ERROR "${why}: `${first}` must precede `${second}`")
	endif()
endfunction()
function(forbid_text haystack needle why)
	string(FIND "${${haystack}}" "${needle}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "${why}: forbidden `${needle}`")
	endif()
endfunction()

require_text(C "CandidateAliasesProtected( &candidate, owner, key )"
	"candidate publication must reject borrowed/live aliases")
require_text(C "owner->ready && KeyEqual( &owner->key, key )"
	"same-key reuse must precede replacement")
require_text(C "owner->allocationGeneration == UINT32_MAX"
	"allocation generation saturation must fail closed")
require_text(C "VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE"
	"H3 content must carry an explicit producer identity")
require_text(C "ticket.committedWriteExpected.colorView"
	"ticket must bind the exact physical history write slot")
require_text(C "!ticket->committedWriteExpected.valid"
	"submit must reject a forged invalid expected history receipt")
require_text(C "ticket->committedWriteExpected.historyIndex !=\n\t\t\t\tticket->authority.historyWriteIndex"
	"expected history receipt must self-join its authority")
require_text(C "count += KeyPointers( &live->key, out + count );"
	"replacement cleanup must protect the old borrowed key cohort")
require_text(C "targetReceipt->targetView != ticket->content.targetView"
	"submit promotion must revalidate current H2 target identity")
require_text(C "VK_TemporalMainActivationReceiptExact(\n\t\t\t&authority->activation, activation )"
	"accepted generic+IQM activation must be revalidated exactly")
require_text(C "Ral_CmdTransitionTexture( commandBuffer, owner->key.velocity"
	"velocity must acquire compute visibility")
require_text(C "Ral_CmdTransitionTexture( commandBuffer, owner->key.validity"
	"validity must acquire compute visibility")
require_text(C "VK_IMAGE_LAYOUT_GENERAL"
	"resolved target must enter storage-write layout")
require_text(C "owner->groups[readIndex]"
	"the exact committed history read slot selects the descriptor group")
require_text(C "!KeyValid( &owner->key ) || !OwnerHandlesDistinct( owner )"
	"Record must revalidate its live owner before emitting commands")
require_text(C "return owner->pipeline && owner->groups[r]"
	"Record must require the selected pipeline and history-slot group")
require_text(C "RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,\n\t\tRAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,\n\t\tVK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL"
	"motion auxiliaries must be restored for later readback authority")

require_text(SHADER "layout(set = 0, binding = 6) uniform sampler nearestSampler;"
	"nearest sampler ABI missing")
require_text(SHADER "layout(rgba16f, set = 0, binding = 7) uniform writeonly image2D resolvedColor;"
	"resolved output ABI missing")
require_text(SHADER "precise vec2 motionTexel = velocity * vec2(pc.extent);"
	"previous texel must consume motion velocity without division")
require_text(SHADER "precise vec2 unjitteredPreviousTexel = vec2(pixel) - motionTexel;"
	"previous texel subtraction must be explicit")
require_text(SHADER "pc.previousEffectiveJitterUv - pc.currentEffectiveJitterUv"
	"previous UV must remove current and add prior effective jitter")
require_text(SHADER "float samplePreviousDepthLinear(vec2 texel)"
	"manual bilinear previous-depth helper missing")
require_text(SHADER "vec4 samplePreviousColorLinear(vec2 texel)"
	"manual bilinear previous-color helper missing")
require_text(SHADER "texelFetch(sampler2D(previousLinearDepth, nearestSampler)"
	"previous depth bilinear must be built from nearest texel fetches")
require_text(SHADER "texelFetch(sampler2D(previousColor, nearestSampler)"
	"previous color bilinear must be built from nearest texel fetches")
require_text(SHADER "sampler2D(motionVelocity, nearestSampler)"
	"motion velocity must use nearest sampling")
require_text(SHADER "sampler2D(motionValidity, nearestSampler), pixel, 0).r;"
	"motion validity must use integer texel fetch")
require_text(SHADER "sampler2D(currentDeviceDepth, nearestSampler), pixel, 0).r;"
	"current depth must use integer texel fetch")
require_text(SHADER "max(pc.depthThresholdAbsolute,"
	"depth rejection must use max absolute/relative threshold")
require_text(SHADER "for (int y = -1; y <= 1; ++y)"
	"3x3 current-neighborhood clamp missing")
require_text(SHADER "if (!finite4(result)) result = vec4(0.0);"
	"non-finite output must be canonicalized")
require_text(SHADER "valid > 0.5"
	"motion validity must gate history consumption")
require_text(SHADER "finite2(previousTexel) && inside"
	"non-finite and out-of-bounds reprojection must reject history")
require_text(SHADER "currentLinear > 0.0 && priorLinear > 0.0"
	"both depths must be finite positive before history consumption")
require_text(SHADER "abs(currentLinear - priorLinear) <= depthLimit"
	"depth discontinuity must reject history")
require_text(SHADER "prior.rgb, neighborhoodMin, neighborhoodMax"
	"history color must be clamped to the current 3x3 neighborhood")
require_text(SHADER "mixPrecise(current.rgb, clampedPrior"
	"accepted history must feed the resolved RGB")
require_text(SHADER "current.a);"
	"resolve must preserve current alpha")
require_text(SHADER "precise vec2 f = texel - baseFloat;"
	"previous texel fraction must have an explicit round point")
require_text(SHADER "float mixPrecise(float x, float y, float a)"
	"depth bilinear no-contract helper missing")
require_text(SHADER "vec4 mixPrecise(vec4 x, vec4 y, float a)"
	"color bilinear no-contract helper missing")
require_text(SHADER "vec3 mixPrecise(vec3 x, vec3 y, float a)"
	"final blend no-contract helper missing")
require_count(SHADER "previousTexel[ \t]*=" 1
	"previous texel must have one canonical authoring site")
require_count(SHADER "previousTexel[ \t]*(\\+|-|\\*|/)?=" 1
	"previous texel must not be reassigned after canonical authoring")
require_count(SHADER "previousEffectiveJitterUv - pc.currentEffectiveJitterUv" 1
	"effective jitter correction must occur exactly once")
require_count(SHADER "previousTexel\\.y[ \t]*=" 0
	"no hidden reprojection Y flip is admitted")
require_count(SHADER "previousTexel\\." 0
	"no post-declaration reprojection component or swizzle mutation is admitted")
require_count(SHADER "currentUv" 0
	"color path must not normalize current coordinates")
require_count(SHADER "textureLod" 0
	"resolve inputs must use integer texel fetches")
require_count(SHADER "velocity\\." 0
	"no component-wise velocity transform is admitted")
require_count(SHADER "velocity[ \t]*(\\+|-|\\*|/)?=" 1
	"velocity must not be reassigned after canonical sampling")
require_count(SHADER "imageStore\\(resolvedColor" 1
	"shader must write exactly one resolved output")
require_count(SHADER "imageStore\\(previous" 0
	"adjacent-raw contract must never write previous history")
require_text(MANIFEST "source: 'temporal_resolve.comp', output: 'temporal_resolve_comp_spv'"
	"shader manifest ownership missing")

require_text(TEST "capturedValues[0][6].sampler == owner.nearestSampler"
	"host must pin nearest sampler binding")
require_text(TEST "capturedValues[0][7].textureView == f.key.resolvedTargetView"
	"host must pin resolved storage binding")
require_text(TEST "VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE"
	"host must pin producer identity")
require_text(TEST "borrowed[17]"
	"host must execute the borrowed alias matrix")
require_text(TEST "for ( j = 0; j < 7; ++j )"
	"host must exercise every candidate role against protected identities")
require_text(TEST "ticket.committedWriteExpected"
	"host must exercise durable history-write promotion")
require_text(TEST "commands[baseCommands + 12]"
"host must pin the exact 13-command resolve and auxiliary-restore trace")
require_text(TEST "bad.allocationGeneration = 0;"
	"host must reject a forged zero-generation live owner")
require_text(TEST "bad.groups[f.authority.historyReadIndex] = NULL;"
	"host must reject a missing selected history group before commands")
require_text(C "sizeof( vkTemporalResolvePush_t ) == 48u"
	"host ABI must pin the exact reflected push size")
require_text(C "historyReadIndex ) == 44u"
	"host ABI must pin the final reflected push member offset")

file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VKC)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_backend.c" BACKEND)
require_count(VKC "VK_TemporalResolveInit\\(" 1 "sole H3 Init")
require_count(VKC "VK_TemporalResolveNeedsIdle\\(" 1 "sole H3 idle query")
require_count(VKC "VK_TemporalResolveEnsureAfterFence\\(" 1 "sole H3 Ensure")
require_count(VKC "VK_TemporalResolveGetReceipt\\(" 2 "H3 prepare+submit receipts")
require_count(VKC "VK_TemporalResolveBuildAuthority\\(" 1 "sole H3 authority join")
require_count(VKC "VK_TemporalResolveRecord\\(" 1 "sole H3 dispatch")
require_count(VKC "VK_TemporalResolveResolveSubmit\\(" 1 "sole H3 promotion")
require_count(VKC "VK_TemporalResolveHasLive\\(" 4 "H3 lifecycle probes")
require_count(VKC "VK_TemporalResolveReleaseAfterIdle\\(" 1 "sole H3 release")
require_count(VKC "vk_temporal_resolve_prepare_authority\\(" 2
	"vk.c may define and call the prepare wrapper exactly once")
require_count(VKC "vk_temporal_resolve_record_or_copy\\(" 2
	"vk.c may define and call the producer wrapper exactly once")
require_count(BACKEND "vk_temporal_recursive_record\\(" 1
	"tr_backend UI path has the sole shared H4 transaction call")
require_count(VKC "vk_temporal_recursive_record\\(" 2
	"vk.c may define and call the shared H4 transaction exactly once")
require_text(VKC "vk_temporal_history_store_prepare()"
	"H4 must Prepare visibility before producer commands")
require_text(VKC "recursiveProduced = vk_temporal_resolve_record_or_copy();"
	"H4 must choose H3 or H2 copy exactly once")
require_text(VKC "VK_TemporalHistoryStoreRecord("
	"H4 must Store the chosen H2 target after producer")
require_text(STORE "static qboolean OwnerValidExact("
	"Store must validate one combined borrowed/owned role cohort")
require_text(STORE "n = AppendKeyBorrowed( &owner->key, roles, 0 );"
	"Store owner validity must begin with all 13 borrowed key roles")
require_text(STORE "roles[n++] = owner->groups[1]; roles[n++] = owner->pipeline;"
	"Store owner validity must append all six owned roles")
require_text(STORE "DestroyOwnedProtected(owner,&owner->key,key)"
	"Store replacement must not destroy old/new borrowed aliases")
require_text(STORE "DestroyOwnedProtected(owner,&owner->key,NULL)"
	"Store release must not destroy forged borrowed aliases")
require_text(STORE_TEST "for(role=0;role<6;++role) for(j=0;j<13;++j)"
	"Store host must pin every owned role against every borrowed role")
require_text(VKC "vk_temporal_resolved_hdr_record_copy();"
	"H2 copy remains the no-H3 fallback")
require_text(VKC "recursive ? qtrue : qfalse"
	"staged feedback must atomically carry previous-slot consumption")
require_text(VKC "VK_TemporalResolvedHdrGetReceipt("
	"H3 prepromotion must refetch the live H2 target owner receipt")
require_text(VKC "vk_temporal_resolve_release_after_idle( \"history-store-release\" );"
	"H3 must release before raw history parents")
require_text(VKC "vk_temporal_resolve_release_after_idle( reason );"
	"H3 must release before H2/motion borrowed parents")

# Function-scoped command-graph authority.  These slices begin at the actual
# owning function/branch, so earlier definitions or comments cannot satisfy
# the positional checks.
require_order(BACKEND "(void)vk_temporal_motion_seal_primary();"
	"vk_temporal_recursive_record();"
	"UI path must seal before shared H4 transaction")
require_order(BACKEND "vk_temporal_recursive_record();"
	"if ( r_bloom->integer )"
	"UI path must complete H4 producer/Store before bloom")

string(FIND "${VKC}" "void vk_temporal_recursive_record( void )" recursive_start)
string(FIND "${VKC}" "// SHADER_MODULE_BL macro retired." recursive_end)
math(EXPR recursive_length "${recursive_end}-${recursive_start}")
string(SUBSTRING "${VKC}" ${recursive_start} ${recursive_length} RECURSIVE)
require_order(RECURSIVE "vk_temporal_resolve_prepare_authority();"
	"vk_temporal_history_store_prepare()" "H4 authority before Prepare")
require_order(RECURSIVE "vk_temporal_history_store_prepare()"
	"vk_temporal_resolve_record_or_copy();" "H4 Prepare before Producer")
require_order(RECURSIVE "vk_temporal_resolve_record_or_copy();"
	"vk_temporal_history_store_feedback(" "H4 Producer before Store")

string(FIND "${VKC}" "void vk_end_frame( void )" end_frame_start)
if(end_frame_start EQUAL -1)
	message(FATAL_ERROR "cannot isolate vk_end_frame")
endif()
string(SUBSTRING "${VKC}" ${end_frame_start} -1 END_FRAME)
require_order(END_FRAME "vk_temporal_recursive_record();"
	"vk_tonemap();"
	"no-2D path must complete H4 transaction before tonemap")
require_order(END_FRAME "backendQuery = R_TemporalBackendQuerySubmitAuthority("
	"submitResult = Ral_Submit("
	"exact temporal decision authority must be snapshotted before submit")
require_order(END_FRAME "backendQuery == TEMPORAL_BACKEND_SUBMIT_INVALID"
	"submitResult = Ral_Submit("
	"invalid or missing expected temporal authority must fail before submit")
require_text(END_FRAME "backendQuery == TEMPORAL_BACKEND_SUBMIT_NONE\n\t\t\t\t\t&& ( havePendingWrite\n\t\t\t\t\t\t|| vk_temporal_submit_has_command_local_residue("
	"ordinary submit is legal only with no temporal command-local residue")
require_order(END_FRAME "submitResult = Ral_Submit("
	"R_TemporalBackendSubmitted( &backendAuthority,"
	"history commit authority must follow the actual submit")
string(FIND "${END_FRAME}" "submitResult = Ral_Submit(" submit_start)
string(SUBSTRING "${END_FRAME}" ${submit_start} -1 SUBMIT_TAIL)
require_order(SUBMIT_TAIL "vk_temporal_history_prevalidate_submit("
	"R_TemporalBackendSubmitted( &backendAuthority,"
	"producer prepromotion must precede backend commit")
require_order(SUBMIT_TAIL "R_TemporalBackendSubmitted( &backendAuthority,"
	"activationApplied = VK_TemporalMainActivationResolveSubmit("
	"history commit must precede durable activation publication")
require_text(SUBMIT_TAIL "if ( backendQuery == TEMPORAL_BACKEND_SUBMIT_EXACT\n\t\t\t\t&& vk_temporal_main_activation.submissionPending )"
	"activation publication must be gated by exact temporal authority")
require_text(SUBMIT_TAIL "if ( backendQuery == TEMPORAL_BACKEND_SUBMIT_EXACT ) {\n\t\t\ttemporalHistoryCommittedReceipt_t committedHistory[2];"
	"H1 publication must be gated by exact temporal authority")
require_text(SUBMIT_TAIL "if ( backendQuery == TEMPORAL_BACKEND_SUBMIT_EXACT ) {\n\t\t\tvkTemporalMainActivationReceipt_t resolvedActivation;"
	"motion readback publication must be gated by exact temporal authority")
require_text(SUBMIT_TAIL "if ( backendQuery == TEMPORAL_BACKEND_SUBMIT_EXACT\n\t\t\t\t&& ( vk_temporal_resolved_hdr_frame.prepared"
	"H3/H2 publication must be gated by exact temporal authority")
require_order(SUBMIT_TAIL "VK_TemporalMainActivationResolveSubmit("
	"vk_temporal_resolved_hdr_resolve_submit("
	"activation promotion must precede H3/H2 promotion")
require_order(END_FRAME "R_TemporalCancelQueuedFrames();"
	"qfalse, qfalse, NULL );"
	"discard must cancel the recorded H3/H2 producer")

string(FIND "${VKC}" "static void vk_temporal_resolve_release_after_idle" resolve_release_start)
string(FIND "${VKC}" "static void vk_temporal_entmat_release_after_idle" resolve_release_end)
math(EXPR resolve_release_length "${resolve_release_end}-${resolve_release_start}")
string(SUBSTRING "${VKC}" ${resolve_release_start} ${resolve_release_length} RESOLVE_RELEASE)
forbid_text(RESOLVE_RELEASE "Ral_WaitIdleAndDrainDeferred"
	"low-level H3 child release must enqueue without a nested drain")
require_text(VKC "|| ( vk_temporal_history_store_owner.initialized"
	"aggregate live query must include Store-only ownership")
require_text(VKC "|| ( vk_temporal_history_consume.initialized"
	"aggregate live query must include H1-only ownership")

string(FIND "${VKC}" "if ( VK_TemporalMotionMaterializationNeedsIdle(" a2b_start)
if(a2b_start EQUAL -1)
	message(FATAL_ERROR "cannot isolate A2b replacement")
endif()
string(SUBSTRING "${VKC}" ${a2b_start} 7000 A2B_REPLACE)
require_order(A2B_REPLACE
	"vk_temporal_resolve_release_after_idle(\n\t\t\t\t\t\t\"A2b-resource-replacement\" );"
	"VK_TemporalMotionMaterializationEnsureAfterFence("
	"resolve child must release before motion parent replacement")
require_order(A2B_REPLACE
	"vk_temporal_pipeline_table_release_after_idle(\n\t\t\t\t\t\t\"A2b-resource-replacement\" );"
	"Ral_WaitIdleAndDrainDeferred("
	"A2b siblings must enqueue before the sole child drain")
require_order(A2B_REPLACE "Ral_WaitIdleAndDrainDeferred("
	"VK_TemporalMotionMaterializationEnsureAfterFence("
	"A2b child drain must precede parent replacement")
require_order(A2B_REPLACE
	"vk_temporal_resolved_hdr_release_after_idle(\n\t\t\t\t\t\t\t\t\t\"H2-resource-replacement\" );"
	"VK_TemporalResolvedHdrEnsureAfterFence("
	"resolve child must release before H2 target replacement")

file(GLOB PRODUCT_C "${ROOT}/code/render/ral/backends/vulkan/renderer/*.c")
foreach(path IN LISTS PRODUCT_C)
	if(path STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_resolve.c"
			OR path STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_resolve_authority.c"
			OR path STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c")
		continue()
	endif()
	file(READ "${path}" body)
	if(NOT path STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_backend.c")
		foreach(wrapper IN ITEMS
			"vk_temporal_resolve_prepare_authority"
			"vk_temporal_resolve_record_or_copy")
			string(FIND "${body}" "${wrapper}" wrapper_pos)
			if(NOT wrapper_pos EQUAL -1)
				message(FATAL_ERROR
					"lowercase H3 wrapper authority escaped into ${path}: ${wrapper}")
			endif()
		endforeach()
	endif()
	foreach(token IN ITEMS
		"VK_TemporalResolveInit" "VK_TemporalResolveNeedsIdle"
		"VK_TemporalResolveEnsureAfterFence" "VK_TemporalResolveGetReceipt"
		"VK_TemporalResolveBuildAuthority" "VK_TemporalResolveRecord"
		"VK_TemporalResolveResolveSubmit" "VK_TemporalResolveHasLive"
		"VK_TemporalResolveReleaseAfterIdle")
		string(FIND "${body}" "${token}" pos)
		if(NOT pos EQUAL -1)
			message(FATAL_ERROR "H3 product authority escaped vk.c into ${path}: ${token}")
		endif()
	endforeach()
endforeach()

file(GLOB PRODUCT_H "${ROOT}/code/render/ral/backends/vulkan/renderer/*.h")
foreach(path IN LISTS PRODUCT_H)
	if(path STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.h")
		continue()
	endif()
	file(READ "${path}" body)
	foreach(wrapper IN ITEMS
		"vk_temporal_resolve_prepare_authority"
		"vk_temporal_resolve_record_or_copy")
		string(FIND "${body}" "${wrapper}" wrapper_pos)
		if(NOT wrapper_pos EQUAL -1)
			message(FATAL_ERROR
				"lowercase H3 wrapper declaration escaped into ${path}: ${wrapper}")
		endif()
	endforeach()
endforeach()

message(STATUS "vk temporal resolve source policy: PASS")
