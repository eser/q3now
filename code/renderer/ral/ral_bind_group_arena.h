// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Native-free ownership epoch for resettable bind-group cohorts. Vulkan lowers
// reset to vkResetDescriptorPool; WebGPU drops the old bind groups and starts a
// new logical arena generation.

#ifndef WIRED_RAL_BIND_GROUP_ARENA_H
#define WIRED_RAL_BIND_GROUP_ARENA_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	const ralBackend_t *backendIdentity;
	const ralBindGroupArena_t *arenaIdentity;
	uint64_t generation;
	uint32_t maxGroups;
	qboolean ready;
} ralBindGroupArenaReceipt_t;

typedef struct {
	const ralBackend_t *backendIdentity;
	const ralBindGroupArena_t *arenaIdentity;
	uint64_t generation;
	uint32_t maxGroups;
	qboolean ready;
} ralBindGroupArenaLifecycle_t;

qboolean Ral_BindGroupArenaReceiptValid(
	const ralBindGroupArenaReceipt_t *receipt );
qboolean Ral_BindGroupArenaReceiptExact(
	const ralBindGroupArenaReceipt_t *a,
	const ralBindGroupArenaReceipt_t *b );
void Ral_BindGroupArenaLifecycleInit(
	ralBindGroupArenaLifecycle_t *lifecycle,
	const ralBackend_t *backend,
	const ralBindGroupArena_t *arena,
	uint32_t maxGroups );
ralResult_t Ral_BindGroupArenaLifecyclePublishCreate(
	ralBindGroupArenaLifecycle_t *lifecycle,
	ralBindGroupArenaReceipt_t *outReceipt );
ralResult_t Ral_BindGroupArenaLifecycleGetReceipt(
	const ralBindGroupArenaLifecycle_t *lifecycle,
	ralBindGroupArenaReceipt_t *outReceipt );
ralResult_t Ral_BindGroupArenaLifecyclePublishReset(
	ralBindGroupArenaLifecycle_t *lifecycle,
	const ralBindGroupArenaReceipt_t *current,
	ralBindGroupArenaReceipt_t *outNext );

#ifdef __cplusplus
}
#endif

#endif
