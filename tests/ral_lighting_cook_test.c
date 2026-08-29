// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "ral_lighting_cook.h"
#include "ral_lighting_cook_driver.h"
#include "ral_lighting_cook_pipeline.h"
#include <stdio.h>
#include <string.h>
#define C(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)

typedef struct {
	char keys[RAL_LIGHTING_COOK_MAX_ARTIFACTS][RAL_LIGHTING_CACHE_KEY_TEXT_CAPACITY];
	unsigned char bytes[RAL_LIGHTING_COOK_MAX_ARTIFACTS][512];
	uint64_t lengths[RAL_LIGHTING_COOK_MAX_ARTIFACTS];
	uint32_t count, writes, reads;
	qboolean corruptRead;
} pipelineStore_t;

typedef struct {
	uint32_t cancelAt, cancelCalls, progressCalls;
} pipelineCallbacks_t;

typedef struct {
	uint32_t calls, regions;
} driverProducer_t;

static qboolean DriverProduce( void *context,
	const ralLightingCookReceipt_t *batch, qboolean finalBatch,
	ralLightingCookArtifact_t *artifacts, uint32_t artifactCapacity,
	uint32_t *outArtifactCount )
{
	driverProducer_t *producer = (driverProducer_t *)context;
	if ( !producer || !Ral_LightingCookReceiptValid( batch ) ||
		batch->deferredRegionCount || !artifacts || artifactCapacity < 2u ||
		!outArtifactCount ) return qfalse;
	producer->calls++;
	producer->regions += batch->scheduledRegionCount;
	*outArtifactCount = finalBatch ? 2u : 0u;
	return qtrue;
}

static qboolean PipelineWrite( void *context, const char *key, const void *bytes,
	uint64_t length )
{
	pipelineStore_t *store = (pipelineStore_t *)context;
	uint32_t index;
	if ( !store || !key || !bytes || !length || length > sizeof( store->bytes[0] ) )
		return qfalse;
	for ( index = 0u; index < store->count; ++index )
		if ( !strcmp( store->keys[index], key ) ) break;
	if ( index == store->count ) {
		if ( store->count >= RAL_LIGHTING_COOK_MAX_ARTIFACTS ) return qfalse;
		store->count++;
	}
	strcpy( store->keys[index], key );
	memcpy( store->bytes[index], bytes, (size_t)length );
	store->lengths[index] = length;
	store->writes++;
	return qtrue;
}

static qboolean PipelineRead( void *context, const char *key, void *bytes,
	uint64_t capacity, uint64_t *outLength )
{
	pipelineStore_t *store = (pipelineStore_t *)context;
	uint32_t index;
	if ( !store || !key || !bytes || !outLength ) return qfalse;
	for ( index = 0u; index < store->count; ++index )
		if ( !strcmp( store->keys[index], key ) ) break;
	if ( index == store->count || store->lengths[index] > capacity ) return qfalse;
	memcpy( bytes, store->bytes[index], (size_t)store->lengths[index] );
	if ( store->corruptRead ) ( (unsigned char *)bytes )[store->lengths[index] - 1u] ^= 1u;
	*outLength = store->lengths[index];
	store->reads++;
	return qtrue;
}

static qboolean PipelineCancel( void *context )
{
	pipelineCallbacks_t *callbacks = (pipelineCallbacks_t *)context;
	callbacks->cancelCalls++;
	return callbacks->cancelAt && callbacks->cancelCalls >= callbacks->cancelAt;
}

static void PipelineProgress( void *context, const ralLightingCookReceipt_t *receipt,
	uint32_t completedProducts, uint32_t artifactIndex, uint32_t artifactCount )
{
	pipelineCallbacks_t *callbacks = (pipelineCallbacks_t *)context;
	(void)completedProducts; (void)artifactIndex; (void)artifactCount;
	if ( receipt && Ral_LightingCookReceiptValid( receipt ) ) callbacks->progressCalls++;
}

static int Pipeline( void )
{
	static const unsigned char radiance[4] = { 1u, 2u, 3u, 4u };
	static const unsigned char direction[2] = { 5u, 6u };
	static const unsigned char visibility[1] = { 255u };
	static const unsigned char coefficients[24] = { 7u };
	static const unsigned char validity[1] = { 255u };
	unsigned char directional[512], irradiance[512], scratch[512];
	ralLightingArtifactDefinition_t definition;
	ralLightingPayloadView_t payloads[3];
	ralLightingArtifactReceipt_t artifact;
	ralLightingCookArtifact_t products[2];
	ralLightingCookPipelineRequest_t request;
	ralLightingCookPipelineReceipt_t receipt, exact, before;
	ralLightingCookDriverRequest_t driverRequest;
	ralLightingCookDriverReceipt_t driverReceipt;
	ralLightingCacheOps_t ops = { PipelineWrite, PipelineRead };
	pipelineStore_t store;
	pipelineCallbacks_t callbacks;
	driverProducer_t producer;
	memset( &definition, 0, sizeof( definition ) );
	definition.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	definition.kind = RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP;
	definition.artifactGeneration = 101u; definition.cacheKey = 102u;
	definition.producerVersion = 103u;
	definition.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	definition.flags = RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY |
		RAL_LIGHTING_ARTIFACT_HAS_STATIONARY_VISIBILITY;
	definition.dimensions[0] = definition.dimensions[1] = definition.dimensions[2] = 1u;
	definition.payloadCount = 3u;
	payloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_RADIANCE,
		radiance, sizeof( radiance ) };
	payloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_DIRECTION,
		direction, sizeof( direction ) };
	payloads[2] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY,
		visibility, sizeof( visibility ) };
	C( Ral_LightingArtifactWrite( &definition, payloads, directional,
		sizeof( directional ), &artifact ) );
	products[0] = (ralLightingCookArtifact_t){
		RAL_LIGHTING_COOK_DIRECTIONAL_LIGHTMAP | RAL_LIGHTING_COOK_STATIONARY_VISIBILITY,
		definition.kind, definition.artifactGeneration, definition.cacheKey,
		directional, artifact.byteLength };
	definition.kind = RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME;
	definition.artifactGeneration = 201u; definition.cacheKey = 202u;
	definition.encoding = RAL_IRRADIANCE_SH_L1_RGB16F; definition.flags = 0u;
	definition.payloadCount = 2u;
	payloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS,
		coefficients, sizeof( coefficients ) };
	payloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY,
		validity, sizeof( validity ) };
	C( Ral_LightingArtifactWrite( &definition, payloads, irradiance,
		sizeof( irradiance ), &artifact ) );
	products[1] = (ralLightingCookArtifact_t){ RAL_LIGHTING_COOK_IRRADIANCE_PROBES,
		definition.kind, definition.artifactGeneration, definition.cacheKey,
		irradiance, artifact.byteLength };
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_LIGHTING_COOK_PIPELINE_SCHEMA_VERSION;
	request.cook.schemaVersion = RAL_LIGHTING_COOK_SCHEMA_VERSION;
	request.cook.cookGeneration = 301u; request.cook.sourceRevision = 302u;
	request.cook.desiredKey = (ralLightingCacheKey_t){
		RAL_LIGHTING_CACHE_KEY_SCHEMA_VERSION, 1u, 2u, 3u, qtrue };
	request.cook.requestedProducts = RAL_LIGHTING_COOK_ALL;
	request.cook.workerRegionBudget = 2u; request.cook.dirtyRegionCount = 2u;
	request.cook.dirtyRegionIds[0] = 11u; request.cook.dirtyRegionIds[1] = 22u;
	request.artifacts = products; request.artifactCount = 2u;
	request.cacheOps = &ops; request.cacheContext = &store;
	request.verifyScratch = scratch; request.verifyScratchCapacity = sizeof( scratch );
	request.shouldCancel = PipelineCancel; request.progress = PipelineProgress;
	request.callbackContext = &callbacks;
	memset( &store, 0, sizeof( store ) ); memset( &callbacks, 0, sizeof( callbacks ) );
	C( Ral_LightingCookPipelineExecute( &request, &receipt ) &&
		Ral_LightingCookPipelineReceiptValid( &receipt ) &&
		receipt.cook.state == RAL_LIGHTING_COOK_PUBLISHED &&
		receipt.storedProducts == RAL_LIGHTING_COOK_ALL &&
		receipt.verifiedProducts == RAL_LIGHTING_COOK_ALL &&
		receipt.artifactCount == 2u && store.writes == 2u && store.reads == 2u &&
		callbacks.progressCalls == receipt.progressEventCount );
	exact = receipt; memset( &callbacks, 0, sizeof( callbacks ) );
	C( Ral_LightingCookPipelineExecute( &request, &receipt ) &&
		receipt.artifactManifestHash == exact.artifactManifestHash &&
		receipt.cook.planHash == exact.cook.planHash );
	memset( &store, 0, sizeof( store ) ); memset( &callbacks, 0, sizeof( callbacks ) );
	callbacks.cancelAt = 1u;
	C( Ral_LightingCookPipelineExecute( &request, &receipt ) &&
		receipt.cook.state == RAL_LIGHTING_COOK_CANCELLED && !store.writes );
	memset( &store, 0, sizeof( store ) ); memset( &callbacks, 0, sizeof( callbacks ) );
	callbacks.cancelAt = 3u;
	C( Ral_LightingCookPipelineExecute( &request, &receipt ) &&
		receipt.cook.state == RAL_LIGHTING_COOK_CANCELLED &&
		receipt.storedProducts && !receipt.verifiedProducts && store.writes == 1u );
	memset( &store, 0, sizeof( store ) ); memset( &callbacks, 0, sizeof( callbacks ) );
	store.corruptRead = qtrue; memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	C( !Ral_LightingCookPipelineExecute( &request, &receipt ) &&
		!memcmp( &receipt, &before, sizeof( receipt ) ) );
	request.cook.workerRegionBudget = 1u;
	C( !Ral_LightingCookPipelineExecute( &request, &receipt ) );
	memset( &driverRequest, 0, sizeof( driverRequest ) );
	driverRequest.schemaVersion = RAL_LIGHTING_COOK_DRIVER_SCHEMA_VERSION;
	driverRequest.pipeline = request;
	driverRequest.pipeline.cook.dirtyRegionCount = 3u;
	driverRequest.pipeline.cook.dirtyRegionIds[0] = 11u;
	driverRequest.pipeline.cook.dirtyRegionIds[1] = 22u;
	driverRequest.pipeline.cook.dirtyRegionIds[2] = 33u;
	driverRequest.produceBatch = DriverProduce;
	driverRequest.producerContext = &producer;
	driverRequest.artifactStorage = products;
	driverRequest.artifactCapacity = 2u;
	memset( &store, 0, sizeof( store ) ); memset( &callbacks, 0, sizeof( callbacks ) );
	memset( &producer, 0, sizeof( producer ) );
	C( Ral_LightingCookDriverExecute( &driverRequest, &driverReceipt ) &&
		Ral_LightingCookDriverReceiptValid( &driverReceipt ) &&
		driverReceipt.batchCount == 3u && driverReceipt.completedRegionCount == 3u &&
		driverReceipt.producedArtifactCount == 2u && producer.calls == 3u &&
		producer.regions == 3u && store.writes == 2u && store.reads == 2u );
	memset( &store, 0, sizeof( store ) ); memset( &callbacks, 0, sizeof( callbacks ) );
	memset( &producer, 0, sizeof( producer ) ); callbacks.cancelAt = 2u;
	C( Ral_LightingCookDriverExecute( &driverRequest, &driverReceipt ) &&
		Ral_LightingCookDriverReceiptValid( &driverReceipt ) &&
		driverReceipt.cancelled && driverReceipt.batchCount == 1u &&
		driverReceipt.completedRegionCount == 1u && producer.calls == 1u &&
		!store.writes );
	return 0;
}
static int Selection(void){
	static const unsigned char radiance[4]={1,2,3,4},direction[2]={5,6};
	ralLightingArtifactDefinition_t d;ralLightingPayloadView_t payloads[2];
	ralLightingArtifactReceipt_t artifact;ralLightingCacheSelectionRequest_t q;
	ralLightingCacheSelectionReceipt_t r,before;unsigned char bytes[256];
	memset(&d,0,sizeof(d));d.schemaVersion=RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	d.kind=RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP;d.artifactGeneration=8;d.cacheKey=9;
	d.producerVersion=10;d.encoding=RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	d.flags=RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY;d.dimensions[0]=1;
	d.dimensions[1]=1;d.dimensions[2]=1;d.payloadCount=2;
	payloads[0]=(ralLightingPayloadView_t){RAL_LIGHTING_PAYLOAD_RADIANCE,radiance,sizeof(radiance)};
	payloads[1]=(ralLightingPayloadView_t){RAL_LIGHTING_PAYLOAD_DIRECTION,direction,sizeof(direction)};
	C(Ral_LightingArtifactWrite(&d,payloads,bytes,sizeof(bytes),&artifact));
	memset(&q,0,sizeof(q));q.schemaVersion=RAL_LIGHTING_CACHE_SELECTION_SCHEMA_VERSION;
	q.expectedArtifactGeneration=8;q.expectedCacheKey=9;q.expectedKind=d.kind;q.artifact=&artifact;
	C(Ral_LightingCacheSelect(&q,&r)&&r.mode==RAL_LIGHTING_CACHE_MODERN_FINAL&&r.finalQuality);
	q.expectedCacheKey=11;q.requireModernProduct=qtrue;q.allowPreview=qtrue;
	C(Ral_LightingCacheSelect(&q,&r)&&r.mode==RAL_LIGHTING_CACHE_PREVIEW&&!r.finalQuality&&r.requiresRegeneration);
	q.allowPreview=qfalse;C(Ral_LightingCacheSelect(&q,&r)&&r.mode==RAL_LIGHTING_CACHE_BLOCKED&&!r.finalQuality);
	q.requireModernProduct=qfalse;q.legacyCompatibilityAvailable=qtrue;
	C(Ral_LightingCacheSelect(&q,&r)&&r.mode==RAL_LIGHTING_CACHE_LEGACY_COMPATIBILITY&&r.finalQuality&&r.requiresRegeneration);
	before=r;q.expectedCacheKey=0;C(!Ral_LightingCacheSelect(&q,&r)&&!memcmp(&r,&before,sizeof(r)));return 0;}
int main(void){ralLightingCookRequest_t q;ralLightingCookReceipt_t p,r,before;memset(&q,0,sizeof(q));
	q.schemaVersion=RAL_LIGHTING_COOK_SCHEMA_VERSION;q.cookGeneration=1;q.sourceRevision=2;q.requestedProducts=RAL_LIGHTING_COOK_ALL;q.workerRegionBudget=2;q.dirtyRegionCount=3;
	q.dirtyRegionIds[0]=10;q.dirtyRegionIds[1]=20;q.dirtyRegionIds[2]=30;q.desiredKey=(ralLightingCacheKey_t){RAL_LIGHTING_CACHE_KEY_SCHEMA_VERSION,1,2,3,qtrue};
	C(Ral_LightingCookPlan(&q,&p)&&Ral_LightingCookReceiptValid(&p));C(p.invalidatedProducts==RAL_LIGHTING_COOK_ALL&&p.scheduledRegionCount==2&&p.deferredRegionCount==1);
	C(Ral_LightingCookTransition(&p,RAL_LIGHTING_COOK_RUNNING,0,0,&r));C(Ral_LightingCookTransition(&r,RAL_LIGHTING_COOK_STAGED,RAL_LIGHTING_COOK_ALL,99,&r));
	C(Ral_LightingCookTransition(&r,RAL_LIGHTING_COOK_VERIFIED,RAL_LIGHTING_COOK_ALL,99,&r));C(Ral_LightingCookTransition(&r,RAL_LIGHTING_COOK_PUBLISHED,RAL_LIGHTING_COOK_ALL,99,&r));
	before=r;C(!Ral_LightingCookTransition(&r,RAL_LIGHTING_COOK_RUNNING,0,0,&r)&&!memcmp(&r,&before,sizeof(r)));
	q.publishedKey=q.desiredKey;q.dirtyRegionCount=0;C(Ral_LightingCookPlan(&q,&p)&&p.invalidatedProducts==0);C(Ral_LightingCookTransition(&p,RAL_LIGHTING_COOK_RUNNING,0,0,&r));
	C(Ral_LightingCookTransition(&r,RAL_LIGHTING_COOK_STAGED,0,99,&r));
	q.cookGeneration++;C(Ral_LightingCookPlan(&q,&p));
	C(Ral_LightingCookTransition(&p,RAL_LIGHTING_COOK_CANCELLED,0,0,&r));
	C(r.state==RAL_LIGHTING_COOK_CANCELLED&&Ral_LightingCookReceiptValid(&r));C(Selection()==0);C(Pipeline()==0);
	puts("ral_lighting_cook_test: ok");return 0;}
