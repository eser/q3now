# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_resolve_authority.h" H)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_resolve_authority.c" C)
file(READ "${ROOT}/tests/vk_temporal_resolve_authority_test.c" TEST)

function(require_text var needle why)
	string(FIND "${${var}}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "H3 authority policy missing ${why}: ${needle}")
	endif()
endfunction()
function(forbid_text var needle why)
	string(FIND "${${var}}" "${needle}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "H3 authority policy forbidden ${why}: ${needle}")
	endif()
endfunction()

foreach(field IN ITEMS batchToken frameId previousFrameId worldIndex width height
		topologyEpoch planGeneration commandSlot frameCount
		historyAllocationGeneration historyReadIndex historyWriteIndex
		motionTargetAllocationGeneration motionPipelineLayoutAllocationGeneration
		motionMaterializationGeneration pipelineTableGeneration
		sceneColorAttachmentGeneration resolvedTargetAllocationGeneration
		temporalSegments written invalidated drawSequence activation ready)
	require_text(H "${field}" "pointer-free receipt field ${field}")
endforeach()
foreach(field IN ITEMS currentEffectiveJitterUv previousEffectiveJitterUv zNear zFar)
	require_text(H "${field}" "exact resolve numeric field ${field}")
endforeach()
foreach(field IN ITEMS currentColor currentDepth previousColor previousColorView
		previousDepth previousDepthView velocity velocityView validity validityView resolvedTarget
		resolvedTargetView)
	require_text(H "${field}" "borrowed product field ${field}")
endforeach()
require_text(C "activation->temporalSegments !=\n\t\t\t\tactivation->written + activation->invalidated"
	"activation arithmetic")
require_text(C "activation->prepared != activation->drawSequence.count"
	"ordered draw coverage")
require_text(C "activation->taggedSequence.count !="
	"combined tagged draw coverage")
require_text(C "VK_TemporalMainActivationReceiptExact( activation, activation )"
	"full generic+IQM activation self-validation")
require_text(C "receipt.activation = *activation"
	"full activation authority publication")
require_text(C "committed->frameId + 1u != a->frameId"
	"adjacent committed history")
require_text(C "plan->historyReadIndex == plan->historyWriteIndex"
	"physical ping-pong separation")
require_text(C "motion->allocationGeneration != a->materializationGeneration"
	"activation/materialization generation join")
require_text(C "resolved->sceneColorAttachmentGeneration !="
	"scene/target generation join")
require_text(C "DistinctPointers( roles, 12u )"
	"combined texture/view cohort alias rejection")
require_text(C "-plan->sceneJitterPixels[1] / (float)a->height"
	"Vulkan current jitter Y convention")
require_text(C "-plan->previousJitterPixels[1] / (float)a->height"
	"Vulkan previous jitter Y convention")
require_text(C "plan->sceneJitterUv[0] !="
	"planned jitter pixel/UV provenance match")
require_text(C "*outReceipt = receipt;\n\t*outProducts = products;"
	"output-atomic dual publication")
foreach(token IN ITEMS Ral_Cmd Ral_Create Ral_Destroy Ral_BeginRendering
		Ral_EndRendering Cvar CmdBind Dispatch CopyImage qvk vkCmd)
	forbid_text(C "${token}" "GPU/lifecycle/cvar authority ${token}")
endforeach()
require_text(TEST "REJECT( f.activation.authority.pipelineTableGeneration = 0 )"
	"activation mutation matrix")
require_text(TEST "REJECT( f.history.committed.frameId-- )"
	"history mutation matrix")
require_text(TEST "REJECT( f.resolved.target = f.motionProducts.velocity )"
	"cross-owner texture alias mutation")
require_text(TEST "REJECT( f.plan.sceneJitterPixels[0] = NAN )"
	"current jitter mutation")
require_text(TEST "REJECT( f.plan.previousJitterPixels[1] = -INFINITY )"
	"previous jitter mutation")
require_text(TEST "REJECT( f.plan.sceneJitterUv[1] += 0.001f )"
	"jitter pixel/UV mismatch mutation")
require_text(TEST "f.resolved.targetView = (ralTextureView_t *)f.input.currentColor"
	"texture/view cross-role alias mutation")
require_text(TEST "RejectsAtomically" "failure output atomicity")

message(STATUS "vk temporal resolve authority policy: PASS")
