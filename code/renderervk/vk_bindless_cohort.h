// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_VK_BINDLESS_COHORT_H
#define WIRED_VK_BINDLESS_COHORT_H

#include "vk_bindless_publication.h"

struct ralBackend_s;
struct ralBindGroupLayout_s;
struct ralBindGroup_s;

typedef struct {
	struct ralBackend_s *backend;
	struct ralBindGroupLayout_s *layout;
	struct ralBindGroup_s *set;
	void *rawLayout;
	void *rawSet;
	uint64_t setGeneration;
	qboolean ready;
} vkRalBindlessCohortReceipt_t;

qboolean VK_BindlessCohortBuild(
	struct ralBackend_s *backend,
	struct ralBindGroupLayout_s *layout,
	struct ralBindGroup_s *set,
	void *rawLayout, void *rawSet,
	qboolean publicationInitialized,
	const vkBindlessPublicationLedger_t *publication,
	vkRalBindlessCohortReceipt_t *outReceipt );
qboolean VK_BindlessCohortReceiptExact(
	const vkRalBindlessCohortReceipt_t *a,
	const vkRalBindlessCohortReceipt_t *b );

#endif
