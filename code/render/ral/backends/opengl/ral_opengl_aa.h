// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_OPENGL_AA_H
#define WIRED_RAL_OPENGL_AA_H
#include "ral_opengl_core.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Anti-aliasing for the OpenGL 4.6 product (r_aa 1 TAA, r_aa 2 SMAA), run on the HDR scene before exposure and tone mapping as on
 * the Vulkan, Metal and WebGPU renderers. The TAA resolve and bootstrap and the three SMAA passes are the shared catalog modules
 * (lowered to GLSL 4.60); the product passes their sources in, so the catalog stays in one translation unit. Textures, programs and
 * sampler objects are owned here; the product sees GL texture names only. */
typedef struct ralOpenGlAa_s ralOpenGlAa_t;

typedef struct {
	const unsigned char *source;
	uint32_t byteCount;
} ralOpenGlAaSource_t;

typedef struct {
	ralOpenGlAaSource_t temporalBootstrap, temporalResolve;
	ralOpenGlAaSource_t smaaEdgeVertex, smaaEdgeFragment, smaaBlendVertex, smaaBlendFragment;
	ralOpenGlAaSource_t smaaResolveVertex, smaaResolveFragment;
} ralOpenGlAaSources_t;

/* The product's world projection (ral_opengl_product.vert): clip = (-dot(d, left) * projectionX, dot(d, up) * projectionY,
 * dot(d, forward) - near, dot(d, forward)) with d = position - origin and near = RAL_OPENGL_AA_NEAR. */
#define RAL_OPENGL_AA_NEAR 4.0f
typedef struct {
	float origin[3], forward[3], left[3], up[3];
	float projectionX, projectionY;
} ralOpenGlAaCamera_t;

typedef struct {
	uint32_t width, height;
	ralOpenGlAaCamera_t current, previous;
	float jitterNdc[2];                       /* applied to the world geometry this frame */
	float currentJitterUv[2], previousJitterUv[2];
	qboolean historyValid;
	uint32_t readIndex, writeIndex;
} ralOpenGlAaTaaFrame_t;

qboolean RalOpenGl_AaCreate( const ralOpenGlCore_t *core, const ralOpenGlAaSources_t *sources, ralOpenGlAa_t **out );
void RalOpenGl_AaDestroy( ralOpenGlAa_t *aa );
/* The R8 coverage target the scene pass writes as colour attachment 1 while TAA runs (sized to the scene); 0 on failure. */
uint32_t RalOpenGl_AaCoverageTexture( ralOpenGlAa_t *aa, uint32_t width, uint32_t height );
/* The object motion program (vertex: current and previous world position at locations 0 and 1, ProductDraw block at binding 0;
 * fragment: effect-mask gba at colour location 1). Built on first use; 0 when it does not compile. */
uint32_t RalOpenGl_AaObjectMotionProgram( ralOpenGlAa_t *aa );
/* Attaches the coverage target to `framebuffer` as colour attachment 1 and draws to both attachments, or (coverage 0) detaches it
 * and draws to attachment 0 only, leaving the scene pass exactly as without anti-aliasing. */
qboolean RalOpenGl_AaAttachCoverage( ralOpenGlAa_t *aa, uint32_t framebuffer, uint32_t coverage );
/* Motion (camera reprojection of the scene depth; covered pixels invalid), then the resolve (or the bootstrap without history).
 * Two compute dispatches; the anti-aliased HDR colour is returned. */
qboolean RalOpenGl_AaTaa( ralOpenGlAa_t *aa, uint32_t sceneColor, uint32_t sceneDepth, const ralOpenGlAaTaaFrame_t *frame,
	uint32_t *outColor );
/* SMAA edge detection, blend weights and neighbourhood resolve: three draws. `quality` 1 low .. 4 ultra (0 selects 3). */
qboolean RalOpenGl_AaSmaa( ralOpenGlAa_t *aa, uint32_t sceneColor, uint32_t width, uint32_t height, int quality,
	uint32_t *outColor );
/* The same, with the neighbourhood resolve drawn into `framebuffer` (0 is the default framebuffer) instead of returned: r_aa 2
 * resolves the tone-mapped scene straight into the output attachment (render_antialiasing.h). */
qboolean RalOpenGl_AaSmaaInto( ralOpenGlAa_t *aa, uint32_t sceneColor, uint32_t width, uint32_t height, int quality,
	uint32_t framebuffer );

#ifdef __cplusplus
}
#endif
#endif
