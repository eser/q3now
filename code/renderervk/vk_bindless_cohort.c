// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_bindless_cohort.h"

#include <string.h>

static qboolean ReceiptValid( const vkRalBindlessCohortReceipt_t *receipt ) {
	return receipt && receipt->ready == qtrue && receipt->backend
		&& receipt->layout && receipt->set
		&& (const void *)receipt->backend != (const void *)receipt->layout
		&& (const void *)receipt->backend != (const void *)receipt->set
		&& (const void *)receipt->layout != (const void *)receipt->set
		&& receipt->rawLayout && receipt->rawSet
		&& receipt->setGeneration
		&& receipt->setGeneration != UINT64_MAX ? qtrue : qfalse;
}

qboolean VK_BindlessCohortBuild(
		struct ralBackend_s *backend,
		struct ralBindGroupLayout_s *layout,
		struct ralBindGroup_s *set,
		void *rawLayout, void *rawSet,
		qboolean publicationInitialized,
		const vkBindlessPublicationLedger_t *publication,
		vkRalBindlessCohortReceipt_t *outReceipt ) {
	vkRalBindlessCohortReceipt_t candidate;
	if ( !outReceipt || publicationInitialized != qtrue || !publication
			|| publication->setIdentity != (uintptr_t)set
			|| !publication->setGeneration
			|| publication->setGeneration == UINT64_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.backend = backend;
	candidate.layout = layout;
	candidate.set = set;
	candidate.rawLayout = rawLayout;
	candidate.rawSet = rawSet;
	candidate.setGeneration = publication->setGeneration;
	candidate.ready = qtrue;
	if ( !ReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_BindlessCohortReceiptExact(
		const vkRalBindlessCohortReceipt_t *a,
		const vkRalBindlessCohortReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& a->backend == b->backend && a->layout == b->layout
		&& a->set == b->set && a->rawLayout == b->rawLayout
		&& a->rawSet == b->rawSet
		&& a->setGeneration == b->setGeneration
		&& a->ready == b->ready ? qtrue : qfalse;
}
