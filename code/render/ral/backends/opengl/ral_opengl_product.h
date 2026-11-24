// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_PRODUCT_H
#define WIRED_RAL_OPENGL_PRODUCT_H

#include "ral_opengl_world.h"
#include "ral_exposure.h"
#include "ral_display_visibility.h"
#include "render_material_package.h"
#include "render_antialiasing.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_OPENGL_PRODUCT_SCHEMA_VERSION 12u

typedef struct ralOpenGlProduct_s ralOpenGlProduct_t;

qboolean RalOpenGl_ProductSetExposure( ralOpenGlProduct_t *product,
	const ralExposureSettings_t *settings, uint32_t tonemapMode, float tonemapExposure );

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t productGeneration;
	uint64_t frameGeneration;
	uint32_t postprocessDrawCount;
	uint32_t exposureDispatchCount;
	uint32_t aaDispatchCount;   /* TAA: motion + resolve (or bootstrap) = 2, else 0 */
	uint32_t aaDrawCount;       /* SMAA: edge + blend + resolve = 3, else 0 */
	qboolean exposureHistoryReset;
	uint32_t vertexShaderByteCount;
	uint32_t fragmentShaderByteCount;
	uint64_t vertexShaderDigestLane0;
	uint64_t vertexShaderDigestLane1;
	uint64_t fragmentShaderDigestLane0;
	uint64_t fragmentShaderDigestLane1;
	ralOpenGlFrontendPlanReceipt_t plan;
	renderEffectDecalPoolReceipt_t effectDecalPool;
	renderEffectBeamPoolReceipt_t effectBeamPool;
	ralDisplayVisibilityPlan_t displayVisibility;
	ralOpenGlWorldReceipt_t native;
	ralAtmospherePlanReceipt_t atmosphere;
	ralSkyGpuTelemetryReceipt_t skyGpuTelemetry;
	uint32_t atmosphereDispatchCount;
	uint32_t atmosphereNativeDispatchCount; /* Includes the light-tile pass. */
	uint32_t atmosphereFroxelCount;
	uint32_t atmosphereCompositeCount;
	qboolean atmosphereCloudsActive;
	ralAtmosphereWeatherReceipt_t weather;
	uint32_t weatherDispatchCount;
	uint32_t weatherDrawCount;
	uint32_t lensSourceCount;
	uint32_t lensDispatchCount;
	uint32_t lensDeferredCount;
	uint32_t lensCompletedCount;
	uint32_t unresolvedCount;
	uint32_t fallbackCount;
	uint32_t fatalCount;
	qboolean ready;
} ralOpenGlProductFrameReceipt_t;

qboolean RalOpenGl_ProductCreate( ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *coreReceipt, uint64_t generation,
	ralOpenGlProduct_t **outProduct );
void RalOpenGl_ProductDestroy( ralOpenGlProduct_t *product );
qboolean RalOpenGl_ProductSetOutputExtent( ralOpenGlProduct_t *product,
	uint32_t width, uint32_t height );
qboolean RalOpenGl_ProductSetDirectionalLighting( ralOpenGlProduct_t *product,
	const ralOpenGlLightingReceipt_t *lighting );
qboolean RalOpenGl_ProductSetDisplayVisibility( ralOpenGlProduct_t *product,
	const ralDisplayVisibilityPlan_t *visibility );
qboolean RalOpenGl_ProductSetAerialHaze( ralOpenGlProduct_t *product,
	float aerialHaze );
/* The anti-aliasing plan the module resolved from r_aa (render_antialiasing.h). Takes effect on the next frame; switching away from
 * TAA drops its history, so switching back starts from the bootstrap frame. */
qboolean RalOpenGl_ProductSetAntiAliasing( ralOpenGlProduct_t *product, const renderAntialiasingPlan_t *plan );
/* Frames completed per anti-aliasing path since the product was created (r_aa diagnostics). */
void RalOpenGl_ProductAntiAliasingCounts( const ralOpenGlProduct_t *product, uint32_t *taaResolved, uint32_t *taaBootstrap,
	uint32_t *smaa );
/* r_lightmapBoost: world-lightmap overbright in linear space (1..24, default 4.6), as on every RAL backend. */
qboolean RalOpenGl_ProductSetLightmapBoost( ralOpenGlProduct_t *product,
	float lightmapBoost );
qboolean RalOpenGl_ProductResetLevelResources( ralOpenGlProduct_t *product );
/* Registration boundary, after all worlds unload; retains device/programs. */
qboolean RalOpenGl_ProductResetRegistrationAssets( ralOpenGlProduct_t *product,
	renderSubmissionState_t *frontend );
void RalOpenGl_ProductResetLensVisibility( ralOpenGlProduct_t *product );
void RalOpenGl_ProductUnloadHeightgrid( ralOpenGlProduct_t *product, int worldIndex );
qboolean RalOpenGl_ProductLensVisibility( const ralOpenGlProduct_t *product, int id, float *visibility );
qboolean RalOpenGl_ProductUnloadSkyRadianceResources(
	ralOpenGlProduct_t *product );
qboolean RalOpenGl_ProductReadbackRgb( ralOpenGlProduct_t *product,
	byte *outPixels, uint32_t byteCount );
qboolean RalOpenGl_ProductPrepareMaterialResources(
	ralOpenGlProduct_t *product, const renderSubmissionState_t *frontend );
qboolean RalOpenGl_ProductResolveMaterialRuntimeResource( void *product,
	uint64_t cacheKeyHash, ralMaterialRuntimeResource_t *outResource );
qboolean RalOpenGl_ProductSetMaterialRuntimeCatalog(
	ralOpenGlProduct_t *product,
	const renderMaterialRuntimeCatalog_t *catalog,
	renderSubmissionState_t *frontend );
qboolean RalOpenGl_ProductRender( ralOpenGlProduct_t *product,
	renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission, uint64_t frameGeneration,
	ralOpenGlProductFrameReceipt_t *outReceipt );
qboolean RalOpenGl_ProductFrameReceiptExact(
	const ralOpenGlProductFrameReceipt_t *a,
	const ralOpenGlProductFrameReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
