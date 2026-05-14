// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_sync.c — Vulkan backend: fences (CPU↔GPU) + binary & timeline
// semaphores (GPU↔GPU + host signal/wait). Phase 7.3.
//
// A ralFence_t is normally backed by a real VkFence (Ral_CreateFence); the
// synchronous-upload paths in 7.2 returned a "pre-signaled" token form which
// is still honoured. Destroys go through the deferred-destroy queue.

#include "ral_vulkan_internal.h"

// ── fences ──────────────────────────────────────────────────────────────
ralFence_t *Ral_CreateFence( ralBackend_t *b ) {
	VkFenceCreateInfo  fi;
	ralFence_t        *f;
	if ( !b ) return NULL;
	f = (ralFence_t *)malloc( sizeof( *f ) );
	if ( !f ) return NULL;
	RAL_ZERO( *f );
	f->backend     = b;
	f->preSignaled = qfalse;
	f->ownsFence   = qtrue;   // Phase 7.4c-submit-BC-C-min: native RAL allocation owns the VkFence; Ral_AdoptFence flips this to qfalse for adopted handles.
	RAL_ZERO( fi );
	fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if ( b->vk.CreateFence( b->device, &fi, NULL, &f->fence ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateFence: vkCreateFence failed\n" );
		free( f ); return NULL;
	}
	return f;
}

void Ral_DestroyFence( ralFence_t *f ) {
	if ( !f ) return;
	// Phase 7.4c-submit-BC-C-min: ownsFence=qfalse on Ral_AdoptFence-created
	// wrappers (the renderer's existing qvkCreateFence/qvkDestroyFence pair
	// retains lifetime ownership). Free only the wrapper struct.
	if ( f->ownsFence && f->fence != VK_NULL_HANDLE && f->backend )
		ralVk_DeferDestroy( f->backend, RAL_RES_FENCE, RAL_VK_H2U( f->fence ), 0, NULL );
	free( f );
}

// Phase 7.4c-submit-BC-C-min — fence adoption helper. Wraps an existing
// VkFence in a ralFence_t with ownsFence=qfalse. See ral_sync.h for the
// parallel-paths lifetime contract.
ralFence_t *Ral_AdoptFence( ralBackend_t *b, void *externalFence, const char *debugName ) {
	ralFence_t *f;
	(void)debugName;   // no SetObjectName call — VkFence inherits the caller's debug name
	if ( !b || !externalFence ) return NULL;
	f = (ralFence_t *)malloc( sizeof( *f ) );
	if ( !f ) return NULL;
	RAL_ZERO( *f );
	f->backend     = b;
	f->fence       = (VkFence)externalFence;
	f->preSignaled = qfalse;
	f->ownsFence   = qfalse;
	return f;
}

void *Ral_GetFenceHandle( const ralFence_t *fen ) {
	if ( !fen || fen->preSignaled ) return NULL;
	return (void *)fen->fence;
}

void Ral_WaitFence( ralFence_t *f, uint64_t timeoutNs ) {
	if ( !f || f->preSignaled || f->fence == VK_NULL_HANDLE ) return;   // already done
	f->backend->vk.WaitForFences( f->backend->device, 1, &f->fence, VK_TRUE, timeoutNs );
}

void Ral_ResetFence( ralFence_t *f ) {
	if ( !f || f->preSignaled || f->fence == VK_NULL_HANDLE ) return;
	f->backend->vk.ResetFences( f->backend->device, 1, &f->fence );
}

qboolean Ral_FenceSignaled( ralFence_t *f ) {
	if ( !f ) return qfalse;
	if ( f->preSignaled || f->fence == VK_NULL_HANDLE ) return qtrue;
	return ( f->backend->vk.GetFenceStatus( f->backend->device, f->fence ) == VK_SUCCESS ) ? qtrue : qfalse;
}

// ── semaphores (binary + timeline) ──────────────────────────────────────
ralSemaphore_t *Ral_CreateSemaphore( ralBackend_t *b, ralSemaphoreType_t type ) {
	VkSemaphoreCreateInfo     sci;
	VkSemaphoreTypeCreateInfo tci;
	ralSemaphore_t           *s;
	if ( !b ) return NULL;
	if ( type == RAL_SEMAPHORE_TIMELINE && !b->caps.timelineSemaphores ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateSemaphore: timeline semaphores requested but caps.timelineSemaphores is false\n" );
		return NULL;
	}
	s = (ralSemaphore_t *)malloc( sizeof( *s ) );
	if ( !s ) return NULL;
	RAL_ZERO( *s );
	s->backend = b; s->type = type;
	s->ownsSemaphore = qtrue;   // Phase 7.4c-submit-BC-C-min: native RAL allocation owns the VkSemaphore; Ral_AdoptSemaphore flips this to qfalse for adopted handles.
	RAL_ZERO( sci ); sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if ( type == RAL_SEMAPHORE_TIMELINE ) {
		RAL_ZERO( tci );
		tci.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		tci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		tci.initialValue  = 0;
		sci.pNext = &tci;
	}
	if ( b->vk.CreateSemaphore( b->device, &sci, NULL, &s->sem ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateSemaphore: vkCreateSemaphore failed\n" );
		free( s ); return NULL;
	}
	return s;
}

void Ral_DestroySemaphore( ralSemaphore_t *s ) {
	if ( !s ) return;
	// Phase 7.4c-submit-BC-C-min: ownsSemaphore=qfalse on Ral_AdoptSemaphore-
	// created wrappers (the renderer's existing qvkCreateSemaphore/Destroy
	// pair retains lifetime ownership). Free only the wrapper struct.
	if ( s->ownsSemaphore && s->sem != VK_NULL_HANDLE && s->backend )
		ralVk_DeferDestroy( s->backend, RAL_RES_SEMAPHORE, RAL_VK_H2U( s->sem ), 0, NULL );
	free( s );
}

// Phase 7.4c-submit-BC-C-min — semaphore adoption helper. Wraps an existing
// VkSemaphore in a ralSemaphore_t with ownsSemaphore=qfalse. See ral_sync.h
// for the parallel-paths lifetime contract.
ralSemaphore_t *Ral_AdoptSemaphore( ralBackend_t *b, void *externalSemaphore, ralSemaphoreType_t type, const char *debugName ) {
	ralSemaphore_t *s;
	(void)debugName;
	if ( !b || !externalSemaphore ) return NULL;
	s = (ralSemaphore_t *)malloc( sizeof( *s ) );
	if ( !s ) return NULL;
	RAL_ZERO( *s );
	s->backend       = b;
	s->sem           = (VkSemaphore)externalSemaphore;
	s->type          = type;
	s->ownsSemaphore = qfalse;
	return s;
}

void *Ral_GetSemaphoreHandle( const ralSemaphore_t *sem ) {
	return sem ? (void *)sem->sem : NULL;
}

uint64_t Ral_GetTimelineValue( ralSemaphore_t *s ) {
	uint64_t v = 0;
	if ( !s || s->type != RAL_SEMAPHORE_TIMELINE ) return 0;
	s->backend->vk.GetSemaphoreCounterValue( s->backend->device, s->sem, &v );
	return v;
}

void Ral_SignalTimeline( ralSemaphore_t *s, uint64_t value ) {
	VkSemaphoreSignalInfo si;
	if ( !s || s->type != RAL_SEMAPHORE_TIMELINE ) return;
	RAL_ZERO( si );
	si.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
	si.semaphore = s->sem;
	si.value     = value;
	s->backend->vk.SignalSemaphore( s->backend->device, &si );
}

void Ral_WaitTimeline( ralSemaphore_t *s, uint64_t value, uint64_t timeoutNs ) {
	VkSemaphoreWaitInfo wi;
	if ( !s || s->type != RAL_SEMAPHORE_TIMELINE ) return;
	RAL_ZERO( wi );
	wi.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	wi.semaphoreCount = 1;
	wi.pSemaphores    = &s->sem;
	wi.pValues        = &value;
	s->backend->vk.WaitSemaphores( s->backend->device, &wi, timeoutNs );
}
