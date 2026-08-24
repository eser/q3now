// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_iqm_command.h"

#include <stdint.h>

static qboolean OpsValid( const vkTemporalIqmCommandOps_t *ops ) {
	return ops && ops->preflight && ops->endOrdinary && ops->barrier
		&& ops->beginExact && ops->setDynamicState && ops->bindPipeline
		&& ops->bindGroup && ops->push && ops->bindVertex && ops->bindIndex
		&& ops->drawIndexed && ops->endExact && ops->beginResume
		&& ops->resetOrdinaryState ? qtrue : qfalse;
}

static qboolean PlanResourcesValid(
		const vkTemporalMainIqmActivationPlan_t *plan,
		const vkTemporalIqmCommandResources_t *resources ) {
	const temporalIqmDrawFacts_t *facts;
	uint64_t indexEnd;
	if ( !plan || !resources || !resources->commandBuffer
			|| !plan->authority.token || !plan->authority.frameId
			|| !plan->planSerial || !plan->sequenceDigest
			|| !plan->payloadGroup || !plan->pipeline
			|| !plan->endOrdinary || !plan->beginExact3
			|| !plan->endExact3 || !plan->resumeOrdinary
			|| ( plan->kind != VK_TEMPORAL_SHADER_WRITE
				&& plan->kind != VK_TEMPORAL_SHADER_INVALIDATE )
			|| !R_TemporalIqmDrawFactsValid( &plan->entry.facts )
			|| plan->recordIndex != plan->entry.recordIndex
			|| plan->recordIndex >= TEMPORAL_IQM_MAX_RECORDS
			|| ( plan->clearAuxiliary != qfalse
				&& plan->clearAuxiliary != qtrue )
			|| plan->pipeline != ( plan->kind == VK_TEMPORAL_SHADER_WRITE
				? plan->factory.writePipeline
				: plan->factory.invalidatePipeline )
			|| !VK_TemporalIqmExact3FactoryReceiptExact(
				&plan->factory, &resources->currentFactory )
			|| resources->ordinaryOpen != qtrue
			|| !resources->scene || !resources->velocity
			|| !resources->validity || !resources->depth
			|| resources->scene == resources->velocity
			|| resources->scene == resources->validity
			|| resources->velocity == resources->validity
			|| resources->scene == resources->depth
			|| resources->velocity == resources->depth
			|| resources->validity == resources->depth
			|| resources->payloadGroup != plan->payloadGroup
			|| resources->bindlessGroup !=
				(ralBindGroup_t *)plan->factory.bindless.setIdentity
			|| resources->payloadGroup == resources->bindlessGroup
			|| !resources->vertexBuffer || !resources->indexBuffer
			|| resources->vertexBuffer == resources->indexBuffer
			|| resources->width != plan->authority.width
			|| resources->height != plan->authority.height
			|| ( resources->hasStencil != qfalse
				&& resources->hasStencil != qtrue ) ) return qfalse;
	facts = &plan->entry.facts;
	if ( plan->ordinal != facts->ordinal
			|| resources->vertexBuffer != (ralBuffer_t *)facts->ralVertexBuffer
			|| resources->indexBuffer != (ralBuffer_t *)facts->ralIndexBuffer
			|| !facts->rawVertexBuffer || !facts->rawIndexBuffer
			|| !facts->vertexBufferBytes || !facts->indexBufferBytes
			|| facts->vertexBufferBytes % TEMPORAL_IQM_VERTEX_STRIDE
			|| facts->indexBufferBytes % sizeof( uint32_t )
			|| !facts->indexCount
			|| facts->textureSlot >= TEMPORAL_IQM_TEXTURE_SLOTS
			|| facts->samplerSlot >= TEMPORAL_IQM_SAMPLER_SLOTS ) return qfalse;
	indexEnd = (uint64_t)facts->firstIndex + facts->indexCount;
	return indexEnd <= facts->indexBufferBytes / sizeof( uint32_t )
		? qtrue : qfalse;
}

qboolean VK_TemporalIqmCommandExecute(
		const vkTemporalMainIqmActivationPlan_t *plan,
		const vkTemporalIqmCommandResources_t *resources,
		void *context, const vkTemporalIqmCommandOps_t *ops ) {
	ralRenderingInfo_t exactInfo, resumeInfo;
	vkTemporalIqmExact3Push_t push;
	if ( !OpsValid( ops ) || !PlanResourcesValid( plan, resources )
			|| !VK_TemporalMainRenderingBuildExact3( resources->scene,
				resources->velocity, resources->validity, resources->depth,
				resources->width, resources->height, resources->hasStencil,
				plan->clearAuxiliary, &exactInfo )
			|| !VK_TemporalMainRenderingBuildResume( resources->scene,
				resources->depth, resources->width, resources->height,
				resources->hasStencil, &resumeInfo )
			|| !ops->preflight( context, plan, resources ) ) return qfalse;
	push.imageSlot = plan->entry.facts.textureSlot;
	push.samplerSlot = plan->entry.facts.samplerSlot;

	ops->endOrdinary( context );
	ops->barrier( context );
	ops->beginExact( context, &exactInfo );
	ops->setDynamicState( context );
	ops->bindPipeline( context, plan->pipeline );
	ops->bindGroup( context, 0u, resources->payloadGroup );
	ops->bindGroup( context, 1u, resources->bindlessGroup );
	ops->push( context, &push );
	ops->bindVertex( context, resources->vertexBuffer );
	ops->bindIndex( context, resources->indexBuffer );
	ops->drawIndexed( context, plan->entry.facts.indexCount,
		plan->entry.facts.firstIndex, plan->recordIndex );
	ops->endExact( context );
	ops->barrier( context );
	ops->beginResume( context, &resumeInfo );
	ops->setDynamicState( context );
	ops->resetOrdinaryState( context );
	return qtrue;
}
