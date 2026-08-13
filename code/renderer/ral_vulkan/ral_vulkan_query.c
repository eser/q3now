// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_query.c — Vulkan backend: GPU query pools (timestamps,
// occlusion, pipeline statistics).
//
// Timestamps pair with Ral_WriteTimestamp() (ral_vulkan_command.c) — convert
// raw tick deltas to nanoseconds with caps.timestampPeriodNs. The cmd-buffer-
// side reset (vkCmdResetQueryPool) is recorded by the consumer; Ral_ResetQueryPool
// here is the host-side reset (vkResetQueryPool, core 1.2).

#include "ral_vulkan_internal.h"

R_LOG_DECLARE_CHANNEL( rch_ral, "renderer.ral" );

static VkQueryType ralVk_QueryType( ralQueryType_t t ) {
	switch ( t ) {
	case RAL_QUERY_OCCLUSION:            return VK_QUERY_TYPE_OCCLUSION;
	case RAL_QUERY_PIPELINE_STATISTICS:  return VK_QUERY_TYPE_PIPELINE_STATISTICS;
	case RAL_QUERY_TIMESTAMP:
	default:                             return VK_QUERY_TYPE_TIMESTAMP;
	}
}

ralQueryPool_t *Ral_CreateQueryPool( ralBackend_t *b, const ralQueryPoolCreateInfo_t *ci ) {
	VkQueryPoolCreateInfo qpi;
	ralQueryPool_t       *qp;
	if ( !b || !ci || ci->count == 0 ) return NULL;
	qp = (ralQueryPool_t *)malloc( sizeof( *qp ) );
	if ( !qp ) return NULL;
	RAL_ZERO( *qp );
	qp->backend = b; qp->type = ci->type; qp->count = ci->count;
	RAL_ZERO( qpi );
	qpi.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	qpi.queryType  = ralVk_QueryType( ci->type );
	qpi.queryCount = ci->count;
	if ( ci->type == RAL_QUERY_PIPELINE_STATISTICS )
		qpi.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
		                       | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
		                       | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;
	if ( b->vk.CreateQueryPool( b->device, &qpi, NULL, &qp->pool ) != VK_SUCCESS ) {
		R_LOG( rch_ral, SEV_WARN, "Ral_CreateQueryPool: vkCreateQueryPool failed (type %d, count %u)\n", (int)ci->type, ci->count );
		free( qp ); return NULL;
	}
	ralVk_SetObjectName( b, RAL_VK_H2U( qp->pool ), VK_OBJECT_TYPE_QUERY_POOL, ci->debugName );
	// host-side reset so the pool starts in a known state (core 1.2, needs the
	// hostQueryReset feature — enabled at device creation when supported). If
	// unavailable, the consumer must record a vkCmdResetQueryPool before use.
	if ( b->haveHostQueryReset )
		b->vk.ResetQueryPool( b->device, qp->pool, 0, ci->count );
	return qp;
}

void Ral_DestroyQueryPool( ralQueryPool_t *pool ) {
	if ( !pool ) return;
	if ( pool->pool != VK_NULL_HANDLE && pool->backend )
		ralVk_DeferDestroy( pool->backend, RAL_RES_QUERY_POOL, RAL_VK_H2U( pool->pool ), 0, NULL );
	free( pool );
}

void Ral_ResetQueryPool( ralQueryPool_t *pool, uint32_t firstQuery, uint32_t queryCount ) {
	if ( !pool ) return;
	if ( firstQuery >= pool->count ) return;
	// Clamp without ever computing firstQuery+queryCount (which can wrap in
	// uint32_t and slip past the bound). firstQuery < pool->count is established
	// above, so (pool->count - firstQuery) is a safe non-negative remainder.
	if ( queryCount > pool->count - firstQuery ) queryCount = pool->count - firstQuery;
	if ( !pool->backend->haveHostQueryReset ) {
		RAL_NOTE_ONCE( "Ral_ResetQueryPool: hostQueryReset feature unavailable -- record a vkCmdResetQueryPool in a command buffer instead\n" );
		return;
	}
	pool->backend->vk.ResetQueryPool( pool->backend->device, pool->pool, firstQuery, queryCount );
}

qboolean Ral_GetQueryResults( ralQueryPool_t *pool, uint32_t firstQuery, uint32_t queryCount, uint64_t *out, qboolean wait ) {
	VkResult r;
	if ( !pool || !out || queryCount == 0 ) return qfalse;
	// Bounds-check without computing firstQuery+queryCount (which can wrap in
	// uint32_t and falsely pass). Reject an out-of-range start, then verify the
	// count fits in the remaining range via subtraction (the start is in-range).
	if ( firstQuery >= pool->count ) return qfalse;
	if ( queryCount > pool->count - firstQuery ) return qfalse;
	r = pool->backend->vk.GetQueryPoolResults( pool->backend->device, pool->pool, firstQuery, queryCount,
	                                           (size_t)queryCount * sizeof( uint64_t ), out, sizeof( uint64_t ),
	                                           VK_QUERY_RESULT_64_BIT | ( wait ? VK_QUERY_RESULT_WAIT_BIT : 0 ) );
	return ( r == VK_SUCCESS ) ? qtrue : qfalse;   // VK_NOT_READY (non-wait, results pending) → qfalse
}
