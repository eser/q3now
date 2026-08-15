// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_IQM_PAYLOAD_AUTHORING_H
#define WIRED_VK_TEMPORAL_IQM_PAYLOAD_AUTHORING_H

#include "vk_temporal_iqm_payload.h"

typedef struct {
	temporalIqmSequence_t sequence;
	vkTemporalIqmPayloadReceipt_t payload;
	temporalCameraPoseReceipt_t camera;
	float jitteredProjection[16];
	uint64_t contentDigest;
	uint32_t nextRecordIndex;
	qboolean initialized;
	qboolean active;
	qboolean poisoned;
	qboolean sealed;
} vkTemporalIqmPayloadAuthor_t;

typedef struct {
	temporalIqmSequenceAuthority_t authority;
	vkTemporalIqmPayloadReceipt_t payload;
	temporalCameraPoseReceipt_t camera;
	float jitteredProjection[16];
	uint64_t sequenceDigest;
	uint64_t contentDigest;
	uint32_t recordCount;
	qboolean ready;
} vkTemporalIqmPayloadContentReceipt_t;

void VK_TemporalIqmPayloadAuthorInit( vkTemporalIqmPayloadAuthor_t *author );
qboolean VK_TemporalIqmPayloadAuthorBegin(
	vkTemporalIqmPayloadAuthor_t *author,
	const temporalIqmSequence_t *sequence,
	const vkTemporalIqmPayloadReceipt_t *payload,
	const temporalCameraPoseReceipt_t *camera,
	const float jitteredProjection[16] );
qboolean VK_TemporalIqmPayloadAuthorWrite(
	vkTemporalIqmPayloadAuthor_t *author, uint32_t recordIndex,
	const temporalIqmModelView_t *model );
qboolean VK_TemporalIqmPayloadAuthorSeal(
	vkTemporalIqmPayloadAuthor_t *author,
	vkTemporalIqmPayloadContentReceipt_t *outReceipt );
qboolean VK_TemporalIqmPayloadContentReceiptExact(
	const vkTemporalIqmPayloadContentReceipt_t *a,
	const vkTemporalIqmPayloadContentReceipt_t *b );
qboolean VK_TemporalIqmPayloadContentRevalidate(
	const vkTemporalIqmPayloadContentReceipt_t *receipt,
	const vkTemporalIqmPayloadOwner_t *payloadOwner );
qboolean VK_TemporalIqmPayloadAuthorCancel(
	vkTemporalIqmPayloadAuthor_t *author );

#endif
