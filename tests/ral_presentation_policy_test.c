// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_presentation_policy.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
	return 1; \
} } while ( 0 )

int main( void ) {
	ralPresentationPolicyRequest_t request, requestBefore;
	ralPresentationPolicy_t policy, policyBefore;
	ralPresentationPolicyReceipt_t receipt, receiptBefore, exact;
	ralSwapchainInfo_t swapchain;

	memset( &request, 0x5a, sizeof( request ) ); requestBefore = request;
	CHECK( !Ral_PresentationPolicyRequestFromLegacySwapInterval( 0u, 0u, &request ) );
	CHECK( !memcmp( &request, &requestBefore, sizeof( request ) ) );
	CHECK( Ral_PresentationPolicyRequestFromLegacySwapInterval( 0, 2u, &request ) );
	CHECK( request.intent == RAL_PRESENTATION_INTENT_UNLOCKED
		&& request.allowTearingWhenLate == qtrue
		&& request.preferVrr == qfalse );
	CHECK( Ral_ResolvePresentationPolicy( &request, &policy ) );
	CHECK( Ral_PresentationPolicyValid( &policy ) && policy.preferenceCount == 5u );
	CHECK( policy.preferences[0].mode == RAL_PRESENT_IMMEDIATE
		&& policy.preferences[0].desiredImageCount == 2u );
	CHECK( policy.preferences[1].mode == RAL_PRESENT_MAILBOX
		&& policy.preferences[1].desiredImageCount == 3u );
	CHECK( policy.preferences[4].mode == RAL_PRESENT_FIFO
		&& policy.preferences[4].unboundedImageCount == 4u );

	CHECK( Ral_PresentationPolicyRequestFromLegacySwapInterval( 1, 2u, &request ) );
	CHECK( request.intent == RAL_PRESENTATION_INTENT_SYNCHRONIZED
		&& request.allowTearingWhenLate == qtrue && request.preferVrr == qtrue );
	CHECK( Ral_ResolvePresentationPolicy( &request, &policy ) );
	CHECK( policy.preferenceCount == 2u
		&& policy.preferences[0].mode == RAL_PRESENT_FIFO_RELAXED
		&& policy.preferences[1].mode == RAL_PRESENT_FIFO );

	CHECK( Ral_PresentationPolicyRequestFromLegacySwapInterval( 2, 2u, &request ) );
	CHECK( request.intent == RAL_PRESENTATION_INTENT_LOW_LATENCY_SYNCHRONIZED
		&& request.allowTearingWhenLate == qfalse && request.preferVrr == qtrue );
	CHECK( Ral_ResolvePresentationPolicy( &request, &policy ) );
	CHECK( policy.preferenceCount == 3u
		&& policy.preferences[0].mode == RAL_PRESENT_MAILBOX
		&& policy.preferences[1].mode == RAL_PRESENT_FIFO_LATEST_READY
		&& policy.preferences[2].mode == RAL_PRESENT_FIFO );

	/* WebGPU-shaped safe default: one mandatory FIFO preference, no invented
	 * tearing or mailbox capability. The backend can consume this unchanged. */
	request.intent = RAL_PRESENTATION_INTENT_SYNCHRONIZED;
	request.allowTearingWhenLate = qfalse;
	request.preferVrr = qfalse;
	CHECK( Ral_ResolvePresentationPolicy( &request, &policy ) );
	CHECK( policy.preferenceCount == 1u
		&& policy.preferences[0].mode == RAL_PRESENT_FIFO );

	policyBefore = policy;
	request.schemaVersion++;
	memset( &policy, 0x5a, sizeof( policy ) ); policyBefore = policy;
	CHECK( !Ral_ResolvePresentationPolicy( &request, &policy ) );
	CHECK( !memcmp( &policy, &policyBefore, sizeof( policy ) ) );
	request.schemaVersion = RAL_PRESENTATION_POLICY_SCHEMA_VERSION;
	request.maxFramesInFlight = 2u;
	request.intent = RAL_PRESENTATION_INTENT_LOW_LATENCY_SYNCHRONIZED;
	request.preferVrr = qtrue;
	CHECK( Ral_ResolvePresentationPolicy( &request, &policy ) );

	memset( &swapchain, 0, sizeof( swapchain ) );
	swapchain.generation = 19u;
	swapchain.presentMode = RAL_PRESENT_MAILBOX;
	swapchain.requestedImageCount = 3u;
	swapchain.imageCount = 4u;
	CHECK( Ral_FinalizePresentationPolicy( &policy, &swapchain, &receipt ) );
	CHECK( receipt.presentationGeneration == 19u
		&& receipt.selectedMode == RAL_PRESENT_MAILBOX
		&& receipt.maxFramesInFlight == 2u
		&& receipt.actualImageCount == 4u
		&& receipt.preferVrr == qtrue );
	exact = receipt;
	CHECK( Ral_PresentationPolicyReceiptExact( &receipt, &exact ) );
#define MUTATE(field) do { exact = receipt; exact.field++; \
	CHECK( !Ral_PresentationPolicyReceiptExact( &receipt, &exact ) ); } while ( 0 )
	MUTATE( presentationGeneration ); MUTATE( intent ); MUTATE( selectedMode );
	MUTATE( maxFramesInFlight ); MUTATE( requestedImageCount );
	MUTATE( actualImageCount ); MUTATE( allowTearingWhenLate ); MUTATE( preferVrr );
#undef MUTATE

	memset( &receipt, 0x5a, sizeof( receipt ) ); receiptBefore = receipt;
	swapchain.presentMode = RAL_PRESENT_IMMEDIATE;
	CHECK( !Ral_FinalizePresentationPolicy( &policy, &swapchain, &receipt ) );
	CHECK( !memcmp( &receipt, &receiptBefore, sizeof( receipt ) ) );
	swapchain.presentMode = RAL_PRESENT_FIFO;
	swapchain.imageCount = 1u;
	CHECK( Ral_FinalizePresentationPolicy( &policy, &swapchain, &receipt ) );
	CHECK( receipt.maxFramesInFlight == 1u );

	puts( "RAL presentation policy: PASS" );
	return 0;
}
