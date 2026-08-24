// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_PRESENTATION_POLICY_H
#define WIRED_RAL_PRESENTATION_POLICY_H

#include "ral_swapchain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_PRESENTATION_POLICY_SCHEMA_VERSION 1u
#define RAL_PRESENTATION_POLICY_MAX_PREFERENCES 5u

typedef enum {
	RAL_PRESENTATION_INTENT_UNLOCKED = 0,
	RAL_PRESENTATION_INTENT_SYNCHRONIZED,
	RAL_PRESENTATION_INTENT_LOW_LATENCY_SYNCHRONIZED
} ralPresentationIntent_t;

typedef struct {
	uint32_t schemaVersion;
	ralPresentationIntent_t intent;
	uint32_t maxFramesInFlight;
	qboolean allowTearingWhenLate;
	qboolean preferVrr;
} ralPresentationPolicyRequest_t;

typedef struct {
	uint32_t schemaVersion;
	ralPresentationIntent_t intent;
	uint32_t maxFramesInFlight;
	qboolean allowTearingWhenLate;
	qboolean preferVrr;
	uint32_t preferenceCount;
	ralPresentPreference_t preferences[RAL_PRESENTATION_POLICY_MAX_PREFERENCES];
} ralPresentationPolicy_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t presentationGeneration;
	ralPresentationIntent_t intent;
	ralPresentMode_t selectedMode;
	uint32_t maxFramesInFlight;
	uint32_t requestedImageCount;
	uint32_t actualImageCount;
	qboolean allowTearingWhenLate;
	qboolean preferVrr;
} ralPresentationPolicyReceipt_t;

qboolean Ral_PresentationPolicyRequestFromLegacySwapInterval(
	int swapInterval, uint32_t maxFramesInFlight,
	ralPresentationPolicyRequest_t *outRequest );
qboolean Ral_ResolvePresentationPolicy(
	const ralPresentationPolicyRequest_t *request,
	ralPresentationPolicy_t *outPolicy );
qboolean Ral_PresentationPolicyValid( const ralPresentationPolicy_t *policy );
qboolean Ral_FinalizePresentationPolicy(
	const ralPresentationPolicy_t *policy,
	const ralSwapchainInfo_t *swapchain,
	ralPresentationPolicyReceipt_t *outReceipt );
qboolean Ral_PresentationPolicyReceiptValid(
	const ralPresentationPolicyReceipt_t *receipt );
qboolean Ral_PresentationPolicyReceiptExact(
	const ralPresentationPolicyReceipt_t *a,
	const ralPresentationPolicyReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
