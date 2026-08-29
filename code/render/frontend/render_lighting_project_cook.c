// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_lighting_project_cook.h"

#include "render_lighting_sidecar.h"
#include "lighting_cook_fs.h"
#include "maps/map_format_registry.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROJECT_MAX_LINKS RAL_LIGHTING_BAKE_MAX_LINKS
#define PROJECT_MAX_SAMPLES RAL_IRRADIANCE_PRODUCT_MAX_SAMPLES

typedef struct {
	const ralLightingCookProducerRequest_t *request;
	const ralLightingCookProducerWorkspace_t *workspace;
	ralLightingCookProducerReceipt_t receipt;
} projectProducerContext_t;

typedef struct {
	void *bytes;
	uint64_t capacity;
	ralLightingArtifactReceipt_t receipt;
} projectVolumeArtifact_t;

qboolean RenderLightingProjectCook_DefaultRequest( const mapFile_t *map,
	const char *derivedRoot, renderLightingProjectCookRequest_t *outRequest,
	char outWorldStem[RENDER_LIGHTING_PROJECT_WORLD_STEM_CAPACITY] )
{
	const char *base, *cursor, *end;
	size_t length;
	uint64_t generation;
	if ( !map || !map->name[0] || !derivedRoot || !derivedRoot[0] ||
		!outRequest || !outWorldStem ) return qfalse;
	base = map->name;
	for ( cursor = map->name; *cursor; ++cursor )
		if ( *cursor == '/' || *cursor == '\\' ) base = cursor + 1;
	end = base + strlen( base );
	if ( end - base > 4 && end[-4] == '.' &&
		( end[-3] == 'b' || end[-3] == 'B' ) &&
		( end[-2] == 's' || end[-2] == 'S' ) &&
		( end[-1] == 'p' || end[-1] == 'P' ) ) end -= 4;
	length = (size_t)( end - base );
	if ( !length || length >= RENDER_LIGHTING_PROJECT_WORLD_STEM_CAPACITY )
		return qfalse;
	for ( cursor = base; cursor < end; ++cursor )
		if ( !( ( *cursor >= 'a' && *cursor <= 'z' ) ||
			( *cursor >= 'A' && *cursor <= 'Z' ) ||
			( *cursor >= '0' && *cursor <= '9' ) || *cursor == '-' ||
			*cursor == '_' ) ) return qfalse;
	memcpy( outWorldStem, base, length );
	outWorldStem[length] = '\0';
	generation = (uint32_t)map->checksum;
	if ( !generation ) generation = 1u;
	memset( outRequest, 0, sizeof( *outRequest ) );
	outRequest->schemaVersion = RENDER_LIGHTING_PROJECT_COOK_SCHEMA_VERSION;
	outRequest->cookGeneration = generation;
	outRequest->producerVersion = 1u;
	/* Canonical v1 bake settings: 2 bounces, bounded visibility/sample fanout. */
	outRequest->settingsHash = UINT64_C( 0x77697265646c6974 );
	outRequest->visibilityAuthorityHash = UINT64_C( 0x636d747261636531 );
	outRequest->bounceCount = 2u;
	outRequest->maximumLinksPerPatch = 16u;
	outRequest->maximumSamplesPerProbe = 8u;
	outRequest->workerRegionBudget = 256u;
	outRequest->energyClampQ16 = RAL_LIGHT_Q16_ONE * 16;
	outRequest->derivedRoot = derivedRoot;
	outRequest->worldStem = outWorldStem;
	return qtrue;
}

static qboolean ProjectProduceBatch( void *opaque,
	const ralLightingCookReceipt_t *batch, qboolean finalBatch,
	ralLightingCookArtifact_t *outArtifacts, uint32_t artifactCapacity,
	uint32_t *outArtifactCount )
{
	projectProducerContext_t *context = (projectProducerContext_t *)opaque;
	if ( !context || !Ral_LightingCookReceiptValid( batch ) || !outArtifacts ||
		artifactCapacity < 2u || !outArtifactCount ) return qfalse;
	*outArtifactCount = 0u;
	if ( !finalBatch ) return qtrue;
	if ( !Ral_LightingCookProducerExecute( context->request,
		context->workspace, &context->receipt ) ) return qfalse;
	memcpy( outArtifacts, context->receipt.artifacts,
		context->receipt.artifactCount * sizeof( outArtifacts[0] ) );
	*outArtifactCount = context->receipt.artifactCount;
	return qtrue;
}

static qboolean SizeMultiply( uint64_t a, uint64_t b, uint64_t *out )
{
	if ( !out || ( a && b > UINT64_MAX / a ) ) return qfalse;
	*out = a * b;
	return qtrue;
}

static void *AllocateArray( uint64_t count, uint64_t size )
{
	uint64_t bytes;
	if ( !SizeMultiply( count, size, &bytes ) || !bytes || bytes > SIZE_MAX )
		return NULL;
	return calloc( 1u, (size_t)bytes );
}

static qboolean SourceWorkspaceAllocate( const mapFile_t *map,
	const renderLightingProjectCookRequest_t *request,
	renderLightingProjectSourceWorkspace_t *workspace,
	uint32_t *outTexelCapacity )
{
	uint64_t pixels, texels, candidateCapacity;
	if ( !map || !request || !workspace || !outTexelCapacity ||
		map->numLightmapPages <= 0 || map->lightmapPageSize <= 0 ||
		map->lightmapPageSize % 3 ) return qfalse;
	pixels = (uint32_t)map->lightmapPageSize / 3u;
	if ( !SizeMultiply( pixels, (uint32_t)map->numLightmapPages, &texels ) ||
		!texels || texels > UINT32_MAX ) return qfalse;
	candidateCapacity = (uint64_t)RAL_LIGHTING_BAKE_MAX_PATCHES *
		request->maximumLinksPerPatch;
	if ( candidateCapacity > PROJECT_MAX_LINKS ) candidateCapacity = PROJECT_MAX_LINKS;
	memset( workspace, 0, sizeof( *workspace ) );
	workspace->triangleCapacity = RAL_LIGHTING_BAKE_MAX_PATCHES;
	workspace->candidateCapacity = (uint32_t)candidateCapacity;
	workspace->visibilityCapacity = (uint32_t)candidateCapacity;
	workspace->patchCapacity = RAL_LIGHTING_BAKE_MAX_PATCHES;
	workspace->linkCapacity = PROJECT_MAX_LINKS;
	workspace->dirtyPatchCapacity = RAL_LIGHTING_BAKE_MAX_PATCHES;
	workspace->texelCapacity = (uint32_t)texels;
	workspace->volumeCapacity = RENDER_LIGHTING_PROJECT_MAX_VOLUMES;
	workspace->sampleCapacity = PROJECT_MAX_SAMPLES;
	workspace->triangles = AllocateArray( workspace->triangleCapacity,
		sizeof( workspace->triangles[0] ) );
	workspace->candidates = AllocateArray( workspace->candidateCapacity,
		sizeof( workspace->candidates[0] ) );
	workspace->visibilityScratch = AllocateArray( workspace->visibilityCapacity,
		sizeof( workspace->visibilityScratch[0] ) );
	workspace->visibility = AllocateArray( workspace->visibilityCapacity,
		sizeof( workspace->visibility[0] ) );
	workspace->patches = AllocateArray( workspace->patchCapacity,
		sizeof( workspace->patches[0] ) );
	workspace->links = AllocateArray( workspace->linkCapacity,
		sizeof( workspace->links[0] ) );
	workspace->dirtyPatches = AllocateArray( workspace->dirtyPatchCapacity,
		sizeof( workspace->dirtyPatches[0] ) );
	workspace->texelPatchIndices = AllocateArray( workspace->texelCapacity,
		sizeof( workspace->texelPatchIndices[0] ) );
	workspace->volumes = AllocateArray( workspace->volumeCapacity,
		sizeof( workspace->volumes[0] ) );
	workspace->samples = AllocateArray( workspace->sampleCapacity,
		sizeof( workspace->samples[0] ) );
	if ( !workspace->triangles || !workspace->candidates ||
		!workspace->visibilityScratch || !workspace->visibility ||
		!workspace->patches || !workspace->links || !workspace->dirtyPatches ||
		!workspace->texelPatchIndices || !workspace->volumes || !workspace->samples )
		return qfalse;
	*outTexelCapacity = (uint32_t)texels;
	return qtrue;
}

static void SourceWorkspaceFree( renderLightingProjectSourceWorkspace_t *workspace )
{
	if ( !workspace ) return;
	free( workspace->samples );
	free( workspace->volumes );
	free( workspace->texelPatchIndices );
	free( workspace->dirtyPatches );
	free( workspace->links );
	free( workspace->patches );
	free( workspace->visibility );
	free( workspace->visibilityScratch );
	free( workspace->candidates );
	free( workspace->triangles );
	memset( workspace, 0, sizeof( *workspace ) );
}

static qboolean ProducerWorkspaceAllocate(
	const renderLightingProjectSourceReceipt_t *source,
	const renderLightingProjectVolume_t *volume,
	ralLightingCookProducerWorkspace_t *workspace )
{
	uint64_t probeCount, bytes;
	if ( !source || !volume || !workspace ) return qfalse;
	probeCount = (uint64_t)volume->placement.dimensions[0] *
		volume->placement.dimensions[1] * volume->placement.dimensions[2];
	if ( !probeCount || probeCount > UINT32_MAX ) return qfalse;
	memset( workspace, 0, sizeof( *workspace ) );
	workspace->patchCapacity = source->patchCount;
	workspace->texelCapacity = source->texelCount;
	workspace->incoming = AllocateArray( source->patchCount,
		sizeof( workspace->incoming[0] ) );
	workspace->outgoing = AllocateArray( source->patchCount,
		sizeof( workspace->outgoing[0] ) );
	workspace->directionAccum = AllocateArray( source->patchCount,
		sizeof( workspace->directionAccum[0] ) );
	workspace->patchRadiance = AllocateArray( source->patchCount,
		sizeof( workspace->patchRadiance[0] ) );
	workspace->patchDirection = AllocateArray( source->patchCount,
		sizeof( workspace->patchDirection[0] ) );
	workspace->texelRadiance = AllocateArray( source->texelCount,
		sizeof( workspace->texelRadiance[0] ) );
	workspace->texelDirection = AllocateArray( source->texelCount,
		sizeof( workspace->texelDirection[0] ) );
	if ( !SizeMultiply( source->texelCount, 4u, &bytes ) ) return qfalse;
	workspace->directionalRadianceCapacity = bytes;
	workspace->directionalRadianceBytes = AllocateArray( bytes, 1u );
	if ( !SizeMultiply( source->texelCount, 2u, &bytes ) ) return qfalse;
	workspace->directionalDirectionCapacity = bytes;
	workspace->directionalDirectionBytes = AllocateArray( bytes, 1u );
	if ( !SizeMultiply( probeCount, 24u, &bytes ) ) return qfalse;
	workspace->irradianceCoefficientCapacity = bytes;
	workspace->irradianceCoefficientBytes = AllocateArray( bytes, 1u );
	workspace->irradianceValidityCapacity = (uint32_t)probeCount;
	workspace->irradianceValidityBytes = AllocateArray( probeCount, 1u );
	workspace->directionalArtifactCapacity =
		RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES +
		workspace->directionalRadianceCapacity +
		workspace->directionalDirectionCapacity;
	workspace->directionalArtifactBytes = AllocateArray(
		workspace->directionalArtifactCapacity, 1u );
	workspace->irradianceArtifactCapacity =
		RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES +
		workspace->irradianceCoefficientCapacity + probeCount;
	workspace->irradianceArtifactBytes = AllocateArray(
		workspace->irradianceArtifactCapacity, 1u );
	return workspace->incoming && workspace->outgoing &&
		workspace->directionAccum && workspace->patchRadiance &&
		workspace->patchDirection && workspace->texelRadiance &&
		workspace->texelDirection && workspace->directionalRadianceBytes &&
		workspace->directionalDirectionBytes &&
		workspace->irradianceCoefficientBytes &&
		workspace->irradianceValidityBytes &&
		workspace->directionalArtifactBytes &&
		workspace->irradianceArtifactBytes ? qtrue : qfalse;
}

static void ProducerWorkspaceFree( ralLightingCookProducerWorkspace_t *workspace )
{
	if ( !workspace ) return;
	free( workspace->irradianceArtifactBytes );
	free( workspace->directionalArtifactBytes );
	free( workspace->irradianceValidityBytes );
	free( workspace->irradianceCoefficientBytes );
	free( workspace->directionalDirectionBytes );
	free( workspace->directionalRadianceBytes );
	free( workspace->texelDirection );
	free( workspace->texelRadiance );
	free( workspace->patchDirection );
	free( workspace->patchRadiance );
	free( workspace->directionAccum );
	free( workspace->outgoing );
	free( workspace->incoming );
	memset( workspace, 0, sizeof( *workspace ) );
}

static qboolean BuildCacheKey( const renderSubmissionState_t *submission,
	const renderLightingProjectSourceReceipt_t *source,
	const renderLightingProjectCookRequest_t *request,
	ralLightingCacheKey_t *outKey )
{
	const ralLightDescription_t *lights = NULL;
	const ralEmissiveRouteReceipt_t *routes;
	const renderEmissiveRoutingReceipt_t *routing;
	ralLightingCacheInputs_t inputs;
	ralLightCatalogReceipt_t catalog;
	uint32_t lightCount = 0u, routeCount;
	if ( !submission || !source || !request || !outKey ) return qfalse;
	if ( !RenderSubmission_EmissiveAuthoritySnapshot( submission, &routes,
		&routeCount, &lights, &lightCount, &routing ) ) {
		routes = NULL; routing = NULL; routeCount = 0u; lightCount = 0u;
	}
	(void)routes; (void)routing; (void)routeCount;
	if ( !Ral_LightCatalogBuild( lights, lightCount, request->cookGeneration,
		&catalog ) ) return qfalse;
	memset( &inputs, 0, sizeof( inputs ) );
	inputs.schemaVersion = RAL_LIGHTING_CACHE_SCHEMA_VERSION;
	inputs.geometryHash = source->geometryHash;
	inputs.materialHash = source->materialHash;
	inputs.probeLayoutHash = source->probeLayoutHash;
	inputs.producerVersion = request->producerVersion;
	inputs.settingsHash = request->settingsHash;
	inputs.lightCatalog = catalog;
	return Ral_LightingCacheKeyBuild( &inputs, outKey );
}

static qboolean BuildAdditionalVolumeArtifact(
	const renderLightingProjectCookRequest_t *request,
	const renderLightingProjectSourceReceipt_t *source,
	const renderLightingProjectVolume_t *volume,
	const ralLightingBakeReceipt_t *bake,
	const ralLightVec3Q16_t *patchRadiance,
	const ralIrradianceProductSample_t *samples,
	projectVolumeArtifact_t *artifact )
{
	ralIrradianceProductRequest_t product;
	uint64_t probes, coefficientBytes;
	void *coefficients = NULL;
	uint8_t *validity = NULL;
	qboolean ok = qfalse;
	if ( !request || !source || !volume || !bake || !patchRadiance ||
		!artifact ) return qfalse;
	probes = (uint64_t)volume->placement.dimensions[0] *
		volume->placement.dimensions[1] * volume->placement.dimensions[2];
	if ( !probes || probes > UINT32_MAX ||
		!SizeMultiply( probes, 24u, &coefficientBytes ) ) return qfalse;
	artifact->capacity = RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES +
		coefficientBytes + probes;
	if ( artifact->capacity > SIZE_MAX ) return qfalse;
	artifact->bytes = AllocateArray( artifact->capacity, 1u );
	coefficients = AllocateArray( coefficientBytes, 1u );
	validity = AllocateArray( probes, 1u );
	if ( !artifact->bytes || !coefficients || !validity ) goto done;
	memset( &product, 0, sizeof( product ) );
	product.schemaVersion = RAL_IRRADIANCE_PRODUCT_SCHEMA_VERSION;
	product.artifactGeneration = volume->placement.sourceGeneration;
	product.producerVersion = request->producerVersion;
	product.geometryHash = source->geometryHash;
	product.materialHash = source->materialHash;
	product.layoutHash = volume->layoutHash;
	product.settingsHash = request->settingsHash;
	product.bake = *bake;
	memcpy( product.dimensions, volume->placement.dimensions,
		sizeof( product.dimensions ) );
	product.encoding = RAL_IRRADIANCE_SH_L1_RGB16F;
	product.patchRadiance = patchRadiance;
	product.patchCount = source->patchCount;
	product.samples = samples + volume->firstSample;
	product.sampleCount = volume->sampleCount;
	ok = Ral_IrradianceProductWrite( &product, coefficients, coefficientBytes,
		validity, (uint32_t)probes, artifact->bytes, artifact->capacity,
		&artifact->receipt );
done:
	free( validity );
	free( coefficients );
	if ( !ok ) {
		free( artifact->bytes );
		memset( artifact, 0, sizeof( *artifact ) );
	}
	return ok;
}

qboolean RenderLightingProjectCook_ReceiptValid(
	const renderLightingProjectCookReceipt_t *receipt )
{
	return receipt &&
		receipt->schemaVersion == RENDER_LIGHTING_PROJECT_COOK_RECEIPT_SCHEMA_VERSION &&
		receipt->cookGeneration && receipt->sourceRevision &&
		receipt->directionalManifestHash && receipt->irradianceManifestHash &&
		receipt->cacheManifestHash && receipt->directionalByteLength &&
		receipt->irradianceByteLength && receipt->patchCount && receipt->linkCount &&
		receipt->mappedTexelCount && receipt->volumeCount && receipt->batchCount &&
		receipt->completedRegionCount && receipt->ready == qtrue;
}

qboolean RenderLightingProjectCook_Execute(
	const renderSubmissionState_t *submission, const mapFile_t *map,
	const refimport_t *imports, const renderLightingProjectCookRequest_t *request,
	renderLightingProjectCookReceipt_t *outReceipt )
{
	renderLightingProjectCookReceipt_t result;
	renderLightingProjectSourceRequest_t sourceRequest;
	renderLightingProjectSourceWorkspace_t sourceWorkspace;
	renderLightingProjectSourceReceipt_t source;
	ralLightingCookProducerRequest_t producerRequest;
	ralLightingCookProducerWorkspace_t producerWorkspace;
	projectProducerContext_t producerContext;
	ralLightingCookDriverRequest_t driverRequest;
	ralLightingCookDriverReceipt_t driverReceipt;
	ralLightingCookArtifact_t artifacts[2];
	ralLightingCacheKey_t desiredKey;
	wiredLightingCookFilesystem_t filesystem;
	projectVolumeArtifact_t *volumeArtifacts = NULL;
	renderIrradianceVolumeSource_t *sidecarSources = NULL;
	void *verifyScratch = NULL, *irradianceSidecar = NULL;
	uint64_t verifyCapacity, irradianceCapacity, irradianceLength = 0u,
		irradianceManifest = 0u;
	uint32_t texelCapacity = 0u, volumeIndex;
	qboolean ok = qfalse;
	if ( !submission || !map || !imports || !request || !outReceipt ||
		request->schemaVersion != RENDER_LIGHTING_PROJECT_COOK_SCHEMA_VERSION ||
		!request->cookGeneration || !request->producerVersion ||
		!request->settingsHash || !request->visibilityAuthorityHash ||
		!request->bounceCount || request->bounceCount > RAL_LIGHTING_BAKE_MAX_BOUNCES ||
		!request->maximumLinksPerPatch || request->maximumLinksPerPatch > 64u ||
		!request->maximumSamplesPerProbe || request->maximumSamplesPerProbe > 64u ||
		!request->workerRegionBudget ||
		request->workerRegionBudget > RAL_LIGHTING_COOK_MAX_DIRTY_REGIONS ||
		request->energyClampQ16 <= 0 || !request->derivedRoot ||
		!request->worldStem ) return qfalse;
	memset( &result, 0, sizeof( result ) );
	memset( &source, 0, sizeof( source ) );
	memset( &sourceWorkspace, 0, sizeof( sourceWorkspace ) );
	memset( &producerWorkspace, 0, sizeof( producerWorkspace ) );
	if ( !SourceWorkspaceAllocate( map, request, &sourceWorkspace,
		&texelCapacity ) ) goto done;
	(void)texelCapacity;
	memset( &sourceRequest, 0, sizeof( sourceRequest ) );
	sourceRequest.schemaVersion = RENDER_LIGHTING_PROJECT_SOURCE_SCHEMA_VERSION;
	sourceRequest.sourceGeneration = request->cookGeneration;
	sourceRequest.visibilityAuthorityHash = request->visibilityAuthorityHash;
	sourceRequest.maximumLinksPerPatch = request->maximumLinksPerPatch;
	sourceRequest.maximumSamplesPerProbe = request->maximumSamplesPerProbe;
	if ( !RenderLightingProjectSource_Build( submission, map, imports,
		&sourceRequest, &sourceWorkspace, &source ) ||
		!BuildCacheKey( submission, &source, request, &desiredKey ) ||
		!ProducerWorkspaceAllocate( &source, &sourceWorkspace.volumes[0],
			&producerWorkspace ) ||
		!WiredLightingCookFilesystem_Init( &filesystem, request->derivedRoot ) )
		goto done;
	memset( &producerRequest, 0, sizeof( producerRequest ) );
	producerRequest.schemaVersion = RAL_LIGHTING_COOK_PRODUCER_SCHEMA_VERSION;
	producerRequest.bake.schemaVersion = RAL_LIGHTING_BAKE_SCHEMA_VERSION;
	producerRequest.bake.bakeGeneration = request->cookGeneration;
	producerRequest.bake.staticIndirectKey = desiredKey.staticIndirectKey;
	producerRequest.bake.staticBakeHash = source.emissiveHash;
	producerRequest.bake.producerVersion = request->producerVersion;
	producerRequest.bake.settingsHash = request->settingsHash;
	producerRequest.bake.patches = sourceWorkspace.patches;
	producerRequest.bake.patchCount = source.patchCount;
	producerRequest.bake.links = sourceWorkspace.links;
	producerRequest.bake.linkCount = source.linkCount;
	producerRequest.bake.bounceCount = request->bounceCount;
	producerRequest.bake.energyClampQ16 = request->energyClampQ16;
	producerRequest.directionalArtifactGeneration = request->cookGeneration;
	producerRequest.directionalEncoding =
		RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	producerRequest.pageWidth = source.pageWidth;
	producerRequest.pageHeight = source.pageHeight;
	producerRequest.pageCount = source.pageCount;
	producerRequest.texelPatchIndices = sourceWorkspace.texelPatchIndices;
	producerRequest.texelCount = source.texelCount;
	producerRequest.irradianceArtifactGeneration =
		sourceWorkspace.volumes[0].placement.sourceGeneration;
	producerRequest.geometryHash = source.geometryHash;
	producerRequest.materialHash = source.materialHash;
	producerRequest.layoutHash = sourceWorkspace.volumes[0].layoutHash;
	memcpy( producerRequest.irradianceDimensions,
		sourceWorkspace.volumes[0].placement.dimensions,
		sizeof( producerRequest.irradianceDimensions ) );
	producerRequest.irradianceSamples = sourceWorkspace.samples +
		sourceWorkspace.volumes[0].firstSample;
	producerRequest.irradianceSampleCount = sourceWorkspace.volumes[0].sampleCount;
	verifyCapacity = producerWorkspace.directionalArtifactCapacity >
		producerWorkspace.irradianceArtifactCapacity ?
		producerWorkspace.directionalArtifactCapacity :
		producerWorkspace.irradianceArtifactCapacity;
	verifyScratch = AllocateArray( verifyCapacity, 1u );
	if ( !verifyScratch ) goto done;
	memset( &driverRequest, 0, sizeof( driverRequest ) );
	driverRequest.schemaVersion = RAL_LIGHTING_COOK_DRIVER_SCHEMA_VERSION;
	driverRequest.pipeline.schemaVersion = RAL_LIGHTING_COOK_PIPELINE_SCHEMA_VERSION;
	driverRequest.pipeline.cook.schemaVersion = RAL_LIGHTING_COOK_SCHEMA_VERSION;
	driverRequest.pipeline.cook.cookGeneration = request->cookGeneration;
	driverRequest.pipeline.cook.sourceRevision = source.sourceRevision;
	driverRequest.pipeline.cook.desiredKey = desiredKey;
	driverRequest.pipeline.cook.requestedProducts =
		RAL_LIGHTING_COOK_DIRECTIONAL_LIGHTMAP |
		RAL_LIGHTING_COOK_IRRADIANCE_PROBES;
	driverRequest.pipeline.cook.workerRegionBudget = request->workerRegionBudget;
	driverRequest.pipeline.cook.dirtyRegionCount = source.dirtyRegionCount;
	memcpy( driverRequest.pipeline.cook.dirtyRegionIds, source.dirtyRegionIds,
		source.dirtyRegionCount * sizeof( source.dirtyRegionIds[0] ) );
	driverRequest.pipeline.cacheOps = WiredLightingCookFilesystem_Ops();
	driverRequest.pipeline.cacheContext = &filesystem;
	driverRequest.pipeline.verifyScratch = verifyScratch;
	driverRequest.pipeline.verifyScratchCapacity = verifyCapacity;
	memset( &producerContext, 0, sizeof( producerContext ) );
	producerContext.request = &producerRequest;
	producerContext.workspace = &producerWorkspace;
	driverRequest.produceBatch = ProjectProduceBatch;
	driverRequest.producerContext = &producerContext;
	driverRequest.artifactStorage = artifacts;
	driverRequest.artifactCapacity = 2u;
	if ( !Ral_LightingCookDriverExecute( &driverRequest, &driverReceipt ) ||
		!Ral_LightingCookDriverReceiptValid( &driverReceipt ) ||
		driverReceipt.cancelled ) goto done;
	volumeArtifacts = AllocateArray( source.volumeCount,
		sizeof( volumeArtifacts[0] ) );
	sidecarSources = AllocateArray( source.volumeCount,
		sizeof( sidecarSources[0] ) );
	if ( !volumeArtifacts || !sidecarSources ) goto done;
	volumeArtifacts[0].bytes = producerWorkspace.irradianceArtifactBytes;
	volumeArtifacts[0].capacity = producerWorkspace.irradianceArtifactCapacity;
	volumeArtifacts[0].receipt = producerContext.receipt.irradiance;
	for ( volumeIndex = 1u; volumeIndex < source.volumeCount; ++volumeIndex ) {
		ralLightingArtifactReceipt_t stored, loaded;
		uint64_t loadedLength = 0u;
		if ( !BuildAdditionalVolumeArtifact( request, &source,
			&sourceWorkspace.volumes[volumeIndex], &producerContext.receipt.bake,
			producerWorkspace.patchRadiance, sourceWorkspace.samples,
			&volumeArtifacts[volumeIndex] ) ) goto done;
		if ( volumeArtifacts[volumeIndex].capacity > SIZE_MAX ) goto done;
		if ( volumeArtifacts[volumeIndex].capacity > verifyCapacity ) {
			void *larger = realloc( verifyScratch,
				(size_t)volumeArtifacts[volumeIndex].capacity );
			if ( !larger ) goto done;
			verifyScratch = larger;
			verifyCapacity = volumeArtifacts[volumeIndex].capacity;
		}
		if ( !Ral_LightingArtifactCacheStore(
			WiredLightingCookFilesystem_Ops(), &filesystem,
			volumeArtifacts[volumeIndex].bytes,
			volumeArtifacts[volumeIndex].receipt.byteLength,
			RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME,
			volumeArtifacts[volumeIndex].receipt.artifactGeneration,
			volumeArtifacts[volumeIndex].receipt.cacheKey, &stored ) ||
			!Ral_LightingArtifactCacheLoad( WiredLightingCookFilesystem_Ops(),
				&filesystem, RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME,
				stored.artifactGeneration, stored.cacheKey, verifyScratch,
				verifyCapacity, &loadedLength, &loaded ) ||
			loadedLength != stored.byteLength ||
			!Ral_LightingArtifactReceiptExact( &stored, &loaded ) ) goto done;
	}
	irradianceCapacity = RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES +
		(uint64_t)source.volumeCount * RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES;
	for ( volumeIndex = 0u; volumeIndex < source.volumeCount; ++volumeIndex ) {
		sidecarSources[volumeIndex].placement =
			sourceWorkspace.volumes[volumeIndex].placement;
		sidecarSources[volumeIndex].artifactBytes =
			volumeArtifacts[volumeIndex].bytes;
		sidecarSources[volumeIndex].artifactByteLength =
			volumeArtifacts[volumeIndex].receipt.byteLength;
		if ( UINT64_MAX - irradianceCapacity <
			sidecarSources[volumeIndex].artifactByteLength ) goto done;
		irradianceCapacity += sidecarSources[volumeIndex].artifactByteLength;
	}
	irradianceSidecar = AllocateArray( irradianceCapacity, 1u );
	if ( !irradianceSidecar || !RenderLightingSidecar_PackIrradiance(
		sidecarSources, source.volumeCount, irradianceSidecar, irradianceCapacity,
		&irradianceLength, &irradianceManifest ) ) goto done;
	/* Publish the aggregate probe bundle first and the directional product last;
	 * each replacement is independently atomic and source content is untouched. */
	if ( !WiredLightingCookFilesystem_PublishSidecar( &filesystem,
		request->worldStem, RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME,
		irradianceSidecar, irradianceLength ) ||
		!WiredLightingCookFilesystem_PublishSidecar( &filesystem,
			request->worldStem, RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP,
			producerWorkspace.directionalArtifactBytes,
			producerContext.receipt.directional.byteLength ) ) goto done;
	memset( &result, 0, sizeof( result ) );
	result.schemaVersion = RENDER_LIGHTING_PROJECT_COOK_RECEIPT_SCHEMA_VERSION;
	result.cookGeneration = request->cookGeneration;
	result.sourceRevision = source.sourceRevision;
	result.directionalManifestHash =
		producerContext.receipt.directional.manifestHash;
	result.irradianceManifestHash = irradianceManifest;
	result.cacheManifestHash = driverReceipt.publication.artifactManifestHash;
	result.directionalByteLength =
		producerContext.receipt.directional.byteLength;
	result.irradianceByteLength = irradianceLength;
	result.patchCount = source.patchCount;
	result.linkCount = source.linkCount;
	result.mappedTexelCount = source.mappedTexelCount;
	result.volumeCount = source.volumeCount;
	result.batchCount = driverReceipt.batchCount;
	result.completedRegionCount = driverReceipt.completedRegionCount;
	result.ready = qtrue;
	if ( !RenderLightingProjectCook_ReceiptValid( &result ) ) goto done;
	*outReceipt = result;
	ok = qtrue;
done:
	free( irradianceSidecar );
	if ( volumeArtifacts )
		for ( volumeIndex = 1u; volumeIndex < source.volumeCount; ++volumeIndex )
			free( volumeArtifacts[volumeIndex].bytes );
	free( sidecarSources );
	free( volumeArtifacts );
	free( verifyScratch );
	ProducerWorkspaceFree( &producerWorkspace );
	SourceWorkspaceFree( &sourceWorkspace );
	return ok;
}
