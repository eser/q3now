if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/renderercommon/tr_types.h" TYPES)
file(READ "${ROOT}/code/cgame/cg_draw.c" CG_DRAW)
file(READ "${ROOT}/code/renderervk/tr_temporal_input.c" INPUT)
file(READ "${ROOT}/code/renderervk/tr_backend.c" BACKEND)
file(READ "${ROOT}/code/renderervk/tr_model_iqm.c" IQM)
file(READ "${ROOT}/code/renderervk/tr_scene.c" SCENE)
file(READ "${ROOT}/code/renderervk/vk.c" VK)
file(READ "${ROOT}/code/renderervk/tr_cmds.c" COMMANDS)
file(READ "${ROOT}/code/renderervk/tr_temporal_submit.c" SUBMIT)
file(READ "${ROOT}/tests/tr_temporal_submit_test.c" SUBMIT_TEST)
file(READ "${ROOT}/code/renderer2/tr_extratypes.h" RENDERER2_TYPES)
file(READ "${ROOT}/code/client/cl_cgame.c" CLIENT)

function(require_text haystack needle label)
	string(FIND "${${haystack}}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "missing ${label}: ${needle}")
	endif()
endfunction()

require_text(TYPES "RDF_TEMPORAL_PRIMARY\t0x0040" "non-colliding primary-scene flag")
require_text(CG_DRAW "stereoView == STEREO_CENTER" "center-eye producer gate")
require_text(CG_DRAW "stereoView == STEREO_CENTER && !cl_splitScreen.integer" "single-view producer gate")
require_text(CG_DRAW "cg.refdef.rdflags |= RDF_TEMPORAL_PRIMARY" "primary mark")
require_text(CG_DRAW "cg.refdef.rdflags &= ~RDF_TEMPORAL_PRIMARY" "primary clear")
require_text(INPUT "!( tr.refdef.rdflags & RDF_TEMPORAL_PRIMARY )" "primary consumer gate")
require_text(INPUT "view->portalView != PV_NONE" "portal rejection")
require_text(INPUT "view->stereoFrame != STEREO_CENTER" "stereo rejection")
require_text(INPUT "view->viewportWidth != glConfig.vidWidth" "full-target width gate")
require_text(INPUT "view->viewportHeight != glConfig.vidHeight" "full-target height gate")
require_text(SCENE "fd->rdflags & RDF_HYPERSPACE" "worldless-visible camera cut observation")
require_text(SCENE "R_TemporalMarkCameraCut( worldIndex )" "sticky camera cut mark")
require_text(INPUT "s_temporalCameraCutPending[worldIndex]" "sticky camera cut consumption")
require_text(INPUT "R_TemporalEntityCacheStageCamera" "unjittered camera staging")
require_text(INPUT "view->temporalCameraReceipt = diagnostic.cameraReceipt" "backend camera receipt")
require_text(INPUT "camera-previous=%d" "camera receipt observability")
require_text(INPUT "temporal-continuity schema=1" "raw continuity receipt")
require_text(INPUT "camera-previous-frame=%llu" "camera predecessor identity")
require_text(INPUT "entity-previous-frame=%llu" "entity predecessor identity")
require_text(BACKEND "RB_RecordTemporalEntityReceipts( cmd )" "backend draw receipt adapter")
require_text(BACKEND "R_TemporalEntityCacheRecord" "production cache record")
require_text(BACKEND "R_TemporalBackendEntityReceiptsBegin()" "single receipt scan per replay cohort")
require_text(BACKEND "temporalDelivery = R_TemporalHistoryClassifyBatchDelivery(" "typed DRAW_BUFFER delivery classification")
require_text(BACKEND "temporalDelivery == TEMPORAL_BACKEND_SUBMIT_EXACT ) {\n\t\tR_TemporalBackendRequestDelivered( temporalRequest );\n\t\tvk_begin_frame( temporalRequest );" "exact DRAW_BUFFER token delivery before frame begin")
require_text(BACKEND "temporalDelivery == TEMPORAL_BACKEND_SUBMIT_INVALID ) {\n\t\tR_TemporalBackendRequestDelivered( NULL );\n\t\tvk_begin_frame( NULL );" "invalid DRAW_BUFFER delivery poisoning")
require_text(BACKEND "R_GetModelByHandle" "renderer model topology stamp")
require_text(BACKEND "data->num_frames" "IQM frame topology")
# IQM joint topology validation. The guarantee: a skeletal palette is checked
# for structural sanity before the GPU is asked to use it.
#
# The code MOVED — from tr_backend.c to tr_model_iqm.c
# (IQM_TemporalPaletteValid) — and grew while it moved: it now bounds the joint
# count against TEMPORAL_IQM_MAX_JOINTS, requires num_poses == num_joints,
# guards the frame multiply against overflow, and checks the bind matrices are
# finite. Pinning the file the code used to live in fails on a strictly better
# implementation, which is what happened here.
require_text(IQM "data->num_joints > (int)TEMPORAL_IQM_MAX_JOINTS"
	"IQM joint topology bounded")
require_text(IQM "data->num_poses != data->num_joints"
	"IQM pose/joint consistency")
# Same move as the joint check above: the IQM topology fields are read in
# tr_model_iqm.c now, not tr_backend.c. The guarantee is unchanged — the
# renderer inspects the model's declared topology rather than trusting it —
# only the file it lives in changed.
require_text(IQM "data->num_poses" "IQM pose topology")
require_text(IQM "data->num_surfaces" "IQM surface topology")
require_text(IQM "data->num_vertexes" "IQM vertex topology")
require_text(IQM "data->num_triangles" "IQM triangle topology")
require_text(RENDERER2_TYPES "RDF_NOFOG\t\t0x0008" "renderer2 no-fog bit")
require_text(RENDERER2_TYPES "RDF_EXTRA\t\t0x0010" "renderer2 extended-refdef bit")
require_text(RENDERER2_TYPES "RDF_SUNLIGHT    0x0020" "renderer2 sunlight bit")
math(EXPR RDF_OVERLAP "0x0040 & (0x0008 | 0x0010 | 0x0020)")
if(NOT RDF_OVERLAP EQUAL 0)
	message(FATAL_ERROR "RDF_TEMPORAL_PRIMARY overlaps renderer2 rdflags")
endif()
string(FIND "${INPUT}" "s_temporalStates[worldIndex].pending\n\t\t\t\t&& s_temporalDiagnostics[worldIndex].queued" stale_cancel_gate)
if(NOT stale_cancel_gate EQUAL -1)
	message(FATAL_ERROR "temporal cancellation must not depend on diagnostic queued state")
endif()
set(H4_SUBMIT_DECISION "backendApplied = backendQuery == TEMPORAL_BACKEND_SUBMIT_EXACT\n\t\t\t? R_TemporalBackendSubmitted( &backendAuthority,\n\t\t\t\tsubmitResult == ralSuccess,\n\t\t\t\thavePendingWrite && producerAccepted ? &pendingWrite : NULL,\n\t\t\t\t&historyCommitted, &committedWrite ) : qtrue;")
require_text(VK "${H4_SUBMIT_DECISION}" "submit-bound H4 commit/cancel decision")
require_text(VK "if ( !backendApplied )\n\t\t\tri.Terminate( TERM_UNRECOVERABLE," "all-or-fatal backend decision")
require_text(VK "backendQuery = R_TemporalBackendQuerySubmitAuthority( &backendAuthority );" "tri-state submit authority query")
require_text(VK "backendQuery == TEMPORAL_BACKEND_SUBMIT_NONE\n\t\t\t\t\t&& ( havePendingWrite" "startup submit residue rejection")
require_text(INPUT "s_temporalBackendExpectation = TEMPORAL_BACKEND_SUBMIT_INVALID;" "sticky recorded expectation poisoning")
require_text(INPUT "s_temporalDeliveredExpectation = TEMPORAL_BACKEND_SUBMIT_INVALID;" "sticky delivered request poisoning")
require_text(INPUT "if ( s_temporalStates[worldIndex].pending ) pendingCount++;" "authoritative pending-state scan")
require_text(INPUT "R_TemporalBackendSubmitAuthorityMatchesRequest(\n\t\t\tauthority, &s_temporalDeliveredRequest )" "recorded/request exact tuple join")
require_text(INPUT "disabledDeliveredOnly = !authority->enabled\n\t\t&& s_temporalBackendExpectation == TEMPORAL_BACKEND_SUBMIT_NONE\n\t\t&& s_temporalBackendWorld < 0 && !s_temporalBackendFrame"
	"disabled delivered-only submit authority")
require_text(INPUT "if ( authority->recorded != recordedExact ) return qfalse;"
	"recorded receipt bit revalidation")
require_text(INPUT "disabledDeliveredOnly && ( diagnostic->historyRecorded"
	"disabled delivered-only history residue rejection")
require_text(INPUT "diagnostic->committed && authority->recorded ? qtrue : qfalse"
	"disabled delivered-only submit must not publish unstaged entity/cache receipts")
require_text(VK "vk.ral_color_image, vk.sceneDepth.ral_image, &view"
	"motion readback exact scene-depth product cohort")
require_text(VK "if ( vk_temporal_diagnostic_arm_ready( qfalse )\n\t\t\t&& VK_TemporalMotionReadbackArm("
	"motion arm requires exact live ring/materialization cohort")
require_text(VK "if ( vk_temporal_diagnostic_arm_ready( qtrue )\n\t\t\t&& VK_TemporalResolveReadbackArm("
	"H3c arm requires exact live ring/materialization/H2 cohort")
require_text(VK "vk_temporal_entmat_ring.buffer[i] != slot->entMatBuf"
	"ring buffer identity revalidation")
require_text(VK "vk_temporal_entmat_ring.descriptor[i] != slot->entMatDesc"
	"ring descriptor identity revalidation")
require_text(VK "if ( repair[i] )\n\t\t\tif ( !vk_entmat_materialize_slot_after_idle(\n\t\t\t\t\t&vk.tess[i], bytes ) ) return qfalse;"
	"only repaired command slots are mutated and failure stays unpublished")
require_text(VK "|| !slot->entMatDesc || !slot->ral_entMatDesc"
	"ring requires native mirror and direct RAL group")
require_text(VK "Ral_GetBindGroupHandle( slot->ral_entMatDesc )\n\t\t\t\t\t!= (void *)slot->entMatDesc"
	"ring revalidates exact RAL/native group identity")
require_text(VK "backendQuery == TEMPORAL_BACKEND_SUBMIT_EXACT\n\t\t\t\t\t&& !backendAuthority.recorded\n\t\t\t\t\t&& ( havePendingWrite"
	"disabled delivered-only product residue rejection")
require_text(SUBMIT "pendingCount == 1u && backendPresent && authorityExact" "tri-state exact singleton classifier")
require_text(SUBMIT "!requestPresent || requestState == TEMPORAL_BATCH_REQUEST_NONE" "ordinary NONE batch delivery classification")
require_text(SUBMIT "requestState == TEMPORAL_BATCH_REQUEST_EXACT && tokenExact" "exact token-bound batch delivery classification")
require_text(SUBMIT_TEST "mutated=request; mutated.token++;" "token mismatch host mutation")
require_text(SUBMIT_TEST "2u, qtrue, qtrue) == TEMPORAL_BACKEND_SUBMIT_INVALID" "multi-pending host mutation")
require_text(SUBMIT_TEST "0u, qfalse, qfalse);\n\tCHECK(query == TEMPORAL_BACKEND_SUBMIT_NONE);" "exact-then-ordinary NONE regression")
require_text(SUBMIT_TEST "authority.enabled=0u;" "disabled delivered-only host case")
require_text(SUBMIT_TEST "authority.enabled=1u;" "enabled delivered-without-recorded rejection case")
require_text(SUBMIT_TEST "qtrue, qfalse, TEMPORAL_BATCH_REQUEST_NONE" "startup NONE batch host case")
require_text(SUBMIT_TEST "qtrue, qfalse, TEMPORAL_BATCH_REQUEST_EXACT" "exact request token mismatch host case")
require_text(SUBMIT_TEST "qtrue, qtrue, TEMPORAL_BATCH_REQUEST_CONFLICT" "conflicting request delivery host case")
require_text(COMMANDS "R_TemporalCancelQueuedFrames();" "command allocation/dispatch cancellation")
require_text(CLIENT "temporal-entity-capability glconfig-generation=%u key=trap_R_AddRefEntityToSceneTemporal expected=232 discovered=232 route=native-syscall export=1" "slot232 capability receipt")
require_text(CLIENT "temporal-entity-slot glconfig-generation=%u trap=232 owner=%u entity-generation=%u role=%u accepted=1 export=1" "validated slot232 ingress receipt")

string(FIND "${BACKEND}" "RB_RecordTemporalEntityReceipts( cmd )" record_pos)
string(FIND "${BACKEND}" "vk_forwardplus_capture_dlights" draw_pos)
if(record_pos GREATER draw_pos)
	message(FATAL_ERROR "temporal receipt scan must precede renderer draw setup")
endif()

string(FIND "${VK}" "${H4_SUBMIT_DECISION}" callback_pos)
string(FIND "${VK}" "ri.Terminate( TERM_UNRECOVERABLE, \"Ral_Submit failed\" )" terminate_pos)
if(callback_pos GREATER terminate_pos)
	message(FATAL_ERROR "submit result must reach temporal authority before termination")
endif()

message(STATUS "RAL temporal entity source policy contract: ok")
