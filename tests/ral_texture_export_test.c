// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_texture_export.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static ralTextureResourceReceipt_t Resource( ralTextureType_t type,
		ralFormat_t format, ralBackendType_t backend, uint64_t generation ) {
	ralTextureResourceReceipt_t r;
	memset( &r, 0, sizeof( r ) ); r.schemaVersion = RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	r.backendType = backend; r.textureIdentity = (uintptr_t)0x1200u;
	r.resourceGeneration = generation; r.type = type; r.format = format;
	r.usage = (ralTextureUsage_t)( RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
		| RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_SRC );
	r.width = 16u; r.height = 8u; r.mipLevels = 3u;
	r.arrayLayers = type == RAL_TEXTURE_3D ? 1u : 2u; r.ready = qtrue;
	return r;
}

static int CompletedReadback( const ralTextureReadbackPlan_t *plan,
		ralReadbackReceipt_t *out ) {
	ralTransferReceipt_t prepared, submitted, completed;
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	memset( &request, 0, sizeof( request ) ); memset( &facts, 0, sizeof( facts ) );
	request.memoryClass = RAL_ALLOCATION_READBACK;
	request.residency = RAL_ALLOCATION_RESIDENCY_STREAMED;
	request.size = plan->stagingByteLength; request.alignment = 256u;
	request.ownerIdentity = (uintptr_t)0x3000u; request.ownerGeneration = 7u;
	facts.backendType = plan->transfer.backendType;
	facts.placement = RAL_ALLOCATION_PLACEMENT_DEDICATED;
	facts.committedSize = plan->stagingByteLength; facts.actualAlignment = 256u;
	facts.allocationGeneration = 8u; facts.hostVisible = qtrue;
	memset( out, 0, sizeof( *out ) ); out->schemaVersion = RAL_READBACK_SCHEMA_VERSION;
	if ( !Ral_TransferPrepare( &plan->transfer, plan->request.readbackGeneration, &prepared )
			|| !Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_NATIVE_ASYNC,
				42u, &submitted )
			|| !Ral_TransferComplete( &submitted, 43u, qtrue, &completed )
			|| !Ral_AllocationReceiptBuild( &request, &facts, &out->stagingAllocation ) )
		return 0;
	out->transfer = completed; out->stagingIdentity = request.ownerIdentity;
	out->submissionIdentity = (uintptr_t)0x4000u; out->ready = qtrue;
	return Ral_ReadbackReceiptExact( out, out );
}

int main( void ) {
	unsigned char mapped[512], paddedDifferently[512];
	ralTextureExportCreateInfo_t info;
	ralTextureExportReceipt_t exported, beforeExport;
	ralTextureReadbackRequest_t request;
	ralTextureReadbackPlan_t plan, badPlan, beforePlan, volumePlan;
	ralTextureReadbackRegion_t region;
	ralReadbackReceipt_t readback, wrongReadback;
	ralTextureReadbackResult_t result, sameResult, beforeResult;
	uint32_t row;
	memset( &info, 0, sizeof( info ) );
	info.resource = Resource( RAL_TEXTURE_2D_ARRAY, RAL_FORMAT_R8G8B8A8_SRGB,
		RAL_BACKEND_VULKAN, 3u );
	info.exportGeneration = 9u; info.volumeDepth = 1u;
	info.aspect = RAL_TEXTURE_ASPECT_COLOR; info.mipLevelCount = 3u;
	info.arrayLayerCount = 2u; info.depthSliceCount = 1u;
	info.encoding = RAL_TEXTURE_EXPORT_SRGB;
	CHECK( Ral_TextureExportBuild( &info, &exported )
		&& Ral_TextureExportReceiptExact( &exported, &exported ) );
	beforeExport = exported; info.encoding = RAL_TEXTURE_EXPORT_LINEAR;
	CHECK( !Ral_TextureExportBuild( &info, &exported )
		&& !memcmp( &exported, &beforeExport, sizeof( exported ) ) );
	info.encoding = RAL_TEXTURE_EXPORT_SRGB;

	memset( &request, 0, sizeof( request ) ); request.exportReceipt = beforeExport;
	request.readbackGeneration = 41u; request.mipLevel = 1u; request.arrayLayer = 1u;
	request.x = request.y = 1u; request.width = 3u; request.height = 2u;
	CHECK( Ral_TextureReadbackPlanBuild( &request, &plan )
		&& Ral_TextureReadbackPlanValid( &plan ) && plan.tightBytesPerRow == 12u
		&& plan.stagingBytesPerRow == 256u && plan.tightByteLength == 24u
		&& plan.stagingByteLength == sizeof( mapped )
		&& plan.transfer.textureAspects == RAL_TEXTURE_ASPECT_COLOR );
	CHECK( Ral_TextureReadbackRegionFromPlan( &plan, &region )
		&& region.mipLevel == 1u && region.arrayLayer == 1u
		&& region.aspects == RAL_TEXTURE_ASPECT_COLOR );
	CHECK( CompletedReadback( &plan, &readback ) );
	memset( mapped, 0xA5, sizeof( mapped ) );
	for ( row = 0u; row < plan.tightBytesPerRow; row++ ) {
		mapped[row] = (unsigned char)( row + 1u );
		mapped[256u + row] = (unsigned char)( row + 31u );
	}
	CHECK( Ral_TextureReadbackResultPublish( &plan, &readback, mapped,
		sizeof( mapped ), &result ) );
	memcpy( paddedDifferently, mapped, sizeof( mapped ) );
	memset( paddedDifferently + 12u, 0x5C, 244u );
	memset( paddedDifferently + 268u, 0x6D, 244u );
	CHECK( Ral_TextureReadbackResultPublish( &plan, &readback, paddedDifferently,
		sizeof( paddedDifferently ), &sameResult )
		&& Ral_TextureReadbackResultExact( &result, &sameResult ) );

	memset( &beforeResult, 0xCC, sizeof( beforeResult ) ); sameResult = beforeResult;
	CHECK( !Ral_TextureReadbackResultPublish( &plan, &readback, mapped,
		sizeof( mapped ) - 1u, &sameResult )
		&& !memcmp( &sameResult, &beforeResult, sizeof( sameResult ) ) );
	wrongReadback = readback; wrongReadback.transfer.request.mipLevel++;
	CHECK( !Ral_TextureReadbackResultPublish( &plan, &wrongReadback, mapped,
		sizeof( mapped ), &sameResult ) );
	badPlan = plan; badPlan.request.exportReceipt.resource.resourceGeneration++;
	CHECK( !Ral_TextureReadbackPlanValid( &badPlan ) );
	beforePlan = plan; request.arrayLayer = 2u;
	CHECK( !Ral_TextureReadbackPlanBuild( &request, &plan )
		&& !memcmp( &plan, &beforePlan, sizeof( plan ) ) );

	/* One exact 3D slice is represented by offsetZ, never by an invented array layer. */
	memset( &info, 0, sizeof( info ) );
	info.resource = Resource( RAL_TEXTURE_3D, RAL_FORMAT_R16G16B16A16_SFLOAT,
		RAL_BACKEND_WEBGPU, 4u );
	info.exportGeneration = 10u; info.volumeDepth = 8u;
	info.aspect = RAL_TEXTURE_ASPECT_COLOR; info.baseMipLevel = 1u;
	info.mipLevelCount = 1u; info.arrayLayerCount = 1u;
	info.baseDepthSlice = 1u; info.depthSliceCount = 2u;
	CHECK( Ral_TextureExportBuild( &info, &exported ) );
	memset( &request, 0, sizeof( request ) ); request.exportReceipt = exported;
	request.readbackGeneration = 50u; request.mipLevel = 1u;
	request.depthSlice = 2u; request.width = request.height = 1u;
	CHECK( Ral_TextureReadbackPlanBuild( &request, &volumePlan )
		&& volumePlan.transfer.offsetZ == 2u && volumePlan.transfer.arrayLayer == 0u
		&& volumePlan.transfer.backendType == RAL_BACKEND_WEBGPU );
	info.resource.format = RAL_FORMAT_D24_UNORM_S8_UINT;
	info.resource.usage = (ralTextureUsage_t)( RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
		| RAL_TEXTURE_USAGE_TRANSFER_SRC ); info.aspect = RAL_TEXTURE_ASPECT_STENCIL;
	CHECK( !Ral_TextureExportBuild( &info, &exported ) );
	puts( "ral_texture_export_test: ok" );
	return 0;
}
