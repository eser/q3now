// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_PRODUCT_H
#define WIRED_RAL_WEBGPU_PRODUCT_H

#include "ral_webgpu_frontend.h"
#include "ral_webgpu_weather.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_PRODUCT_SCHEMA_VERSION 5u

typedef struct ralWebGpuProduct_s ralWebGpuProduct_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t backendGeneration;
	uint64_t productGeneration;
	uint64_t frameGeneration;
	uintptr_t targetIdentity;
	ralWebGpuFrontendPlanReceipt_t plan;
	ralWebGpuResourceReceipt_t worldVertexBuffer;
	ralWebGpuResourceReceipt_t worldIndexBuffer;
	ralWebGpuWriteReceipt_t worldVertexWrite;
	ralWebGpuWriteReceipt_t worldIndexWrite;
	ralWebGpuResourceReceipt_t worldMaterialBuffer;
	ralWebGpuWriteReceipt_t worldMaterialWrite;
	ralWebGpuResourceReceipt_t entityVertexBuffer;
	ralWebGpuResourceReceipt_t entityIndexBuffer;
	ralWebGpuResourceReceipt_t entityLightingBuffer;
	ralWebGpuWriteReceipt_t entityVertexWrite;
	ralWebGpuWriteReceipt_t entityIndexWrite;
	ralWebGpuWriteReceipt_t entityLightingWrite;
	ralWebGpuResourceReceipt_t miscVertexBuffer;
	ralWebGpuResourceReceipt_t miscIndexBuffer;
	ralWebGpuWriteReceipt_t miscVertexWrite;
	ralWebGpuWriteReceipt_t miscIndexWrite;
	ralWebGpuCommandReceipt_t command;
	ralWebGpuSubmissionReceipt_t submission;
	uint32_t uploadedMaterialCount;
	uint32_t reusedMaterialCount;
	uint32_t worldDrawCount;
	uint32_t worldEmissiveDrawCount;
	uint32_t modelEntityCount;
	uint32_t primitiveEntityCount;
	uint32_t temporalEntityCount;
	uint32_t entityDrawCount;
	uint32_t localIrradianceEntityCount;
	uint32_t localIrradianceDrawCount;
	uint32_t polygonDrawCount;
	uint32_t lightDrawCount;
	uint32_t uiDrawCount;
	uint32_t atmosphereBoundDrawCount;
	ralAtmosphereWeatherReceipt_t weather;
	uint32_t weatherDrawCount;
	uint32_t deferredNonWorldDrawCount;
	uint32_t unresolvedCount;
	uint32_t fallbackCount;
	uint32_t fatalCount;
	qboolean ready;
} ralWebGpuProductFrameReceipt_t;

qboolean RalWebGpu_ProductCreate( ralWebGpuRuntime_t *runtime,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuPipelineReceipt_t *worldPipeline, uint64_t generation,
	ralWebGpuProduct_t **outProduct );
qboolean RalWebGpu_ProductSetEntityPipeline( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuPipelineReceipt_t *entityPipeline );
qboolean RalWebGpu_ProductGetWorldMaterialBuffer(
	const ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t *outIdentity, uint64_t *outByteSize );
qboolean RalWebGpu_ProductSetWorldMaterialBindGroup(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t bindGroupIdentity );
qboolean RalWebGpu_ProductGetEntityLightingBuffer(
	const ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t *outIdentity, uint64_t *outByteSize );
qboolean RalWebGpu_ProductSetEntityLightingBindGroup(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t bindGroupIdentity );
qboolean RalWebGpu_ProductSetEffectPipeline( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuPipelineReceipt_t *effectPipeline );
qboolean RalWebGpu_ProductSetUiPipelines( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuPipelineReceipt_t *uiPipeline,
	const ralWebGpuPipelineReceipt_t *msdfPipeline );
qboolean RalWebGpu_ProductSetAtmosphereBindGroups(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t worldBindGroup, uintptr_t entityBindGroup,
	uintptr_t effectBindGroup );
qboolean RalWebGpu_ProductSetViewport( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uint32_t width, uint32_t height );
qboolean RalWebGpu_ProductSetWeather( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralAtmosphereWeatherReceipt_t *plan,
	const ralWebGpuWeatherReceipt_t *execution );
void RalWebGpu_ProductDestroy( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt );
qboolean RalWebGpu_ProductPlan( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission,
	ralWebGpuFrontendPlanReceipt_t *outPlan );
qboolean RalWebGpu_ProductRenderPlan( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission,
	const ralWebGpuFrontendPlanReceipt_t *plan, uintptr_t targetIdentity,
	uint64_t frameGeneration, ralWebGpuProductFrameReceipt_t *outReceipt );
qboolean RalWebGpu_ProductRender( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission, uintptr_t targetIdentity,
	uint64_t frameGeneration, ralWebGpuProductFrameReceipt_t *outReceipt );
ralWebGpuAsyncStatus_t RalWebGpu_ProductPoll( ralWebGpuProduct_t *product,
	const ralWebGpuProductFrameReceipt_t *receipt );
qboolean RalWebGpu_ProductFrameReceiptExact(
	const ralWebGpuProductFrameReceipt_t *a,
	const ralWebGpuProductFrameReceipt_t *b );
uint32_t RalWebGpu_ProductLastFailureStage( void );

#ifdef __cplusplus
}
#endif

#endif
