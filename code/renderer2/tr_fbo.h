// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2010 James Canete (use.less01@gmail.com)
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// tr_fbo.h

#ifndef __TR_FBO_H__
#define __TR_FBO_H__

struct image_s;
struct shaderProgram_s;

typedef struct FBO_s
{
	char            name[MAX_QPATH];

	int             index;

	uint32_t        frameBuffer;

	uint32_t        colorBuffers[16];
	int             colorFormat;
	struct image_s  *colorImage[16];

	uint32_t        depthBuffer;
	int             depthFormat;

	uint32_t        stencilBuffer;
	int             stencilFormat;

	uint32_t        packedDepthStencilBuffer;
	int             packedDepthStencilFormat;

	int             width;
	int             height;
} FBO_t;

void FBO_AttachImage(FBO_t *fbo, image_t *image, GLenum attachment, GLuint cubemapside);
void FBO_Bind(FBO_t *fbo);
void FBO_Init(void);
void FBO_Shutdown(void);

void FBO_BlitFromTexture(struct image_s *src, vec4_t inSrcTexCorners, vec2_t inSrcTexScale, FBO_t *dst, ivec4_t inDstBox, struct shaderProgram_s *shaderProgram, const vec4_t inColor, int blend);
void FBO_Blit(FBO_t *src, ivec4_t srcBox, vec2_t srcTexScale, FBO_t *dst, ivec4_t dstBox, struct shaderProgram_s *shaderProgram, const vec4_t color, int blend);
void FBO_FastBlit(FBO_t *src, ivec4_t srcBox, FBO_t *dst, ivec4_t dstBox, int buffers, int filter);


#endif
