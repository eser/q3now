// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_presentation_policy.h"

#include <limits.h>
#include <string.h>

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue ? qtrue : qfalse;
}

static qboolean IntentValid( ralPresentationIntent_t intent ) {
	return intent >= RAL_PRESENTATION_INTENT_UNLOCKED
		&& intent <= RAL_PRESENTATION_INTENT_LOW_LATENCY_SYNCHRONIZED
		? qtrue : qfalse;
}

static qboolean ModeValid( ralPresentMode_t mode ) {
	return mode >= RAL_PRESENT_FIFO
		&& mode <= RAL_PRESENT_FIFO_LATEST_READY ? qtrue : qfalse;
}

static void AddPreference( ralPresentationPolicy_t *policy,
		ralPresentMode_t mode, uint32_t desiredImages,
		uint32_t unboundedImages ) {
	ralPresentPreference_t *preference =
		&policy->preferences[policy->preferenceCount++];
	preference->mode = mode;
	preference->desiredImageCount = desiredImages;
	preference->unboundedImageCount = unboundedImages;
}

qboolean Ral_PresentationPolicyRequestFromLegacySwapInterval(
		int swapInterval, uint32_t maxFramesInFlight,
		ralPresentationPolicyRequest_t *outRequest ) {
	ralPresentationPolicyRequest_t request;
	if ( !outRequest || maxFramesInFlight == 0u || maxFramesInFlight > 4u )
		return qfalse;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_PRESENTATION_POLICY_SCHEMA_VERSION;
	request.maxFramesInFlight = maxFramesInFlight;
	if ( swapInterval == 0 ) {
		request.intent = RAL_PRESENTATION_INTENT_UNLOCKED;
		request.allowTearingWhenLate = qtrue;
		request.preferVrr = qfalse;
	} else if ( swapInterval == 2 ) {
		request.intent = RAL_PRESENTATION_INTENT_LOW_LATENCY_SYNCHRONIZED;
		request.allowTearingWhenLate = qfalse;
		request.preferVrr = qtrue;
	} else {
		request.intent = RAL_PRESENTATION_INTENT_SYNCHRONIZED;
		/* Preserve Wired's established FIFO_RELAXED -> FIFO policy. */
		request.allowTearingWhenLate = qtrue;
		request.preferVrr = qtrue;
	}
	*outRequest = request;
	return qtrue;
}

qboolean Ral_ResolvePresentationPolicy(
		const ralPresentationPolicyRequest_t *request,
		ralPresentationPolicy_t *outPolicy ) {
	ralPresentationPolicy_t policy;
	uint32_t directImages, queuedImages;
	if ( !request || !outPolicy
			|| request->schemaVersion != RAL_PRESENTATION_POLICY_SCHEMA_VERSION
			|| !IntentValid( request->intent )
			|| request->maxFramesInFlight == 0u
			|| request->maxFramesInFlight > 4u
			|| !BoolValid( request->allowTearingWhenLate )
			|| !BoolValid( request->preferVrr ) ) return qfalse;

	directImages = request->maxFramesInFlight < 2u
		? 2u : request->maxFramesInFlight;
	queuedImages = request->maxFramesInFlight + 1u;
	memset( &policy, 0, sizeof( policy ) );
	policy.schemaVersion = RAL_PRESENTATION_POLICY_SCHEMA_VERSION;
	policy.intent = request->intent;
	policy.maxFramesInFlight = request->maxFramesInFlight;
	policy.allowTearingWhenLate = request->allowTearingWhenLate;
	policy.preferVrr = request->preferVrr;

	switch ( request->intent ) {
	case RAL_PRESENTATION_INTENT_UNLOCKED:
		if ( request->allowTearingWhenLate )
			AddPreference( &policy, RAL_PRESENT_IMMEDIATE, directImages, 0u );
		AddPreference( &policy, RAL_PRESENT_MAILBOX, queuedImages, 0u );
		AddPreference( &policy, RAL_PRESENT_FIFO_LATEST_READY, queuedImages, 0u );
		if ( request->allowTearingWhenLate )
			AddPreference( &policy, RAL_PRESENT_FIFO_RELAXED, queuedImages, 0u );
		AddPreference( &policy, RAL_PRESENT_FIFO, queuedImages,
			queuedImages + 1u );
		break;
	case RAL_PRESENTATION_INTENT_LOW_LATENCY_SYNCHRONIZED:
		AddPreference( &policy, RAL_PRESENT_MAILBOX, queuedImages, 0u );
		AddPreference( &policy, RAL_PRESENT_FIFO_LATEST_READY, queuedImages, 0u );
		if ( request->allowTearingWhenLate )
			AddPreference( &policy, RAL_PRESENT_FIFO_RELAXED, queuedImages, 0u );
		AddPreference( &policy, RAL_PRESENT_FIFO, queuedImages, 0u );
		break;
	case RAL_PRESENTATION_INTENT_SYNCHRONIZED:
		if ( request->allowTearingWhenLate )
			AddPreference( &policy, RAL_PRESENT_FIFO_RELAXED, queuedImages, 0u );
		AddPreference( &policy, RAL_PRESENT_FIFO, queuedImages, 0u );
		break;
	default:
		return qfalse;
	}
	if ( !Ral_PresentationPolicyValid( &policy ) ) return qfalse;
	*outPolicy = policy;
	return qtrue;
}

qboolean Ral_PresentationPolicyValid( const ralPresentationPolicy_t *policy ) {
	uint32_t i, j;
	if ( !policy
			|| policy->schemaVersion != RAL_PRESENTATION_POLICY_SCHEMA_VERSION
			|| !IntentValid( policy->intent )
			|| policy->maxFramesInFlight == 0u
			|| policy->maxFramesInFlight > 4u
			|| !BoolValid( policy->allowTearingWhenLate )
			|| !BoolValid( policy->preferVrr )
			|| policy->preferenceCount == 0u
			|| policy->preferenceCount > RAL_PRESENTATION_POLICY_MAX_PREFERENCES
			|| policy->preferences[policy->preferenceCount - 1u].mode
				!= RAL_PRESENT_FIFO ) return qfalse;
	for ( i = 0u; i < policy->preferenceCount; ++i ) {
		const ralPresentPreference_t *preference = &policy->preferences[i];
		if ( !ModeValid( preference->mode )
				|| preference->desiredImageCount == 0u
				|| preference->desiredImageCount > RAL_SWAPCHAIN_MAX_REQUESTED_IMAGES
				|| preference->unboundedImageCount > RAL_SWAPCHAIN_MAX_REQUESTED_IMAGES )
			return qfalse;
		for ( j = 0u; j < i; ++j )
			if ( policy->preferences[j].mode == preference->mode ) return qfalse;
	}
	return qtrue;
}

qboolean Ral_FinalizePresentationPolicy(
		const ralPresentationPolicy_t *policy,
		const ralSwapchainInfo_t *swapchain,
		ralPresentationPolicyReceipt_t *outReceipt ) {
	ralPresentationPolicyReceipt_t receipt;
	uint32_t i;
	qboolean selected = qfalse;
	if ( !Ral_PresentationPolicyValid( policy ) || !swapchain || !outReceipt
			|| swapchain->generation == 0u
			|| swapchain->generation == UINT64_MAX
			|| !ModeValid( swapchain->presentMode )
			|| swapchain->requestedImageCount == 0u
			|| swapchain->imageCount == 0u ) return qfalse;
	for ( i = 0u; i < policy->preferenceCount; ++i )
		if ( policy->preferences[i].mode == swapchain->presentMode ) {
			selected = qtrue; break;
		}
	if ( !selected ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_PRESENTATION_POLICY_SCHEMA_VERSION;
	receipt.presentationGeneration = swapchain->generation;
	receipt.intent = policy->intent;
	receipt.selectedMode = swapchain->presentMode;
	receipt.maxFramesInFlight = policy->maxFramesInFlight < swapchain->imageCount
		? policy->maxFramesInFlight : swapchain->imageCount;
	receipt.requestedImageCount = swapchain->requestedImageCount;
	receipt.actualImageCount = swapchain->imageCount;
	receipt.allowTearingWhenLate = policy->allowTearingWhenLate;
	receipt.preferVrr = policy->preferVrr;
	if ( !Ral_PresentationPolicyReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_PresentationPolicyReceiptValid(
		const ralPresentationPolicyReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_PRESENTATION_POLICY_SCHEMA_VERSION
		&& receipt->presentationGeneration != 0u
		&& receipt->presentationGeneration != UINT64_MAX
		&& IntentValid( receipt->intent )
		&& ModeValid( receipt->selectedMode )
		&& receipt->maxFramesInFlight != 0u
		&& receipt->maxFramesInFlight <= 4u
		&& receipt->requestedImageCount != 0u
		&& receipt->actualImageCount != 0u
		&& receipt->maxFramesInFlight <= receipt->actualImageCount
		&& BoolValid( receipt->allowTearingWhenLate )
		&& BoolValid( receipt->preferVrr ) ? qtrue : qfalse;
}

qboolean Ral_PresentationPolicyReceiptExact(
		const ralPresentationPolicyReceipt_t *a,
		const ralPresentationPolicyReceipt_t *b ) {
	return Ral_PresentationPolicyReceiptValid( a )
		&& Ral_PresentationPolicyReceiptValid( b )
		&& a->schemaVersion == b->schemaVersion
		&& a->presentationGeneration == b->presentationGeneration
		&& a->intent == b->intent
		&& a->selectedMode == b->selectedMode
		&& a->maxFramesInFlight == b->maxFramesInFlight
		&& a->requestedImageCount == b->requestedImageCount
		&& a->actualImageCount == b->actualImageCount
		&& a->allowTearingWhenLate == b->allowTearingWhenLate
		&& a->preferVrr == b->preferVrr ? qtrue : qfalse;
}
