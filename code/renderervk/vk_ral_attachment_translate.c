// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_ral_attachment_translate.h"

#include <string.h>

static ralBlendFactor_t TranslateBlendFactor( VkBlendFactor f ) {
	switch ( f ) {
	case VK_BLEND_FACTOR_ONE:                 return RAL_BLEND_ONE;
	case VK_BLEND_FACTOR_SRC_COLOR:           return RAL_BLEND_SRC_COLOR;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return RAL_BLEND_ONE_MINUS_SRC_COLOR;
	case VK_BLEND_FACTOR_DST_COLOR:           return RAL_BLEND_DST_COLOR;
	case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return RAL_BLEND_ONE_MINUS_DST_COLOR;
	case VK_BLEND_FACTOR_SRC_ALPHA:           return RAL_BLEND_SRC_ALPHA;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return RAL_BLEND_ONE_MINUS_SRC_ALPHA;
	case VK_BLEND_FACTOR_DST_ALPHA:           return RAL_BLEND_DST_ALPHA;
	case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return RAL_BLEND_ONE_MINUS_DST_ALPHA;
	case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:  return RAL_BLEND_SRC_ALPHA_SATURATE;
	case VK_BLEND_FACTOR_ZERO:
	default:                                  return RAL_BLEND_ZERO;
	}
}

static ralBlendOp_t TranslateBlendOp( VkBlendOp o ) {
	switch ( o ) {
	case VK_BLEND_OP_SUBTRACT:         return RAL_BLEND_OP_SUBTRACT;
	case VK_BLEND_OP_REVERSE_SUBTRACT: return RAL_BLEND_OP_REVERSE_SUBTRACT;
	case VK_BLEND_OP_MIN:              return RAL_BLEND_OP_MIN;
	case VK_BLEND_OP_MAX:              return RAL_BLEND_OP_MAX;
	case VK_BLEND_OP_ADD:
	default:                           return RAL_BLEND_OP_ADD;
	}
}

static qboolean FormatsValid( const vkRalAttachmentContract_t *c ) {
	uint32_t i;
	if ( !c || c->numColorAttachments > RAL_MAX_COLOR_ATTACHMENTS ) return qfalse;
	for ( i = 0; i < c->numColorAttachments; ++i ) {
		if ( c->colorFormats[i] <= RAL_FORMAT_UNDEFINED ||
		     c->colorFormats[i] >= RAL_FORMAT_COUNT ) return qfalse;
		// Exact authored mode must never let an ambiguous zero mask fall back to
		// the RAL legacy WRITE_ALL default. Callers must state their intent.
		if ( c->colorBlends[i].writeMask == 0 &&
		     !c->colorBlends[i].writeMaskExplicit ) return qfalse;
	}
	if ( c->depthFormat < RAL_FORMAT_UNDEFINED || c->depthFormat >= RAL_FORMAT_COUNT ) return qfalse;
	return qtrue;
}

qboolean VK_RalAttachmentContractFromVk(
	const ralFormat_t *colorFormats, uint32_t numColorFormats,
	ralFormat_t depthFormat,
	const VkPipelineColorBlendStateCreateInfo *vkBlend,
	vkRalAttachmentContract_t *out ) {
	vkRalAttachmentContract_t next;
	uint32_t i;

	if ( !out || numColorFormats > RAL_MAX_COLOR_ATTACHMENTS ) return qfalse;
	if ( numColorFormats > 0 && ( !colorFormats || !vkBlend ||
	     vkBlend->attachmentCount != numColorFormats || !vkBlend->pAttachments ) ) return qfalse;
	if ( numColorFormats == 0 && vkBlend && vkBlend->attachmentCount != 0 ) return qfalse;
	memset( &next, 0, sizeof( next ) );
	next.numColorAttachments = numColorFormats;
	next.depthFormat = depthFormat;
	for ( i = 0; i < numColorFormats; ++i ) {
		const VkPipelineColorBlendAttachmentState *src = &vkBlend->pAttachments[i];
		ralColorBlendAttachment_t *dst = &next.colorBlends[i];
		next.colorFormats[i] = colorFormats[i];
		dst->blendEnable = src->blendEnable ? qtrue : qfalse;
		dst->srcColor = TranslateBlendFactor( src->srcColorBlendFactor );
		dst->dstColor = TranslateBlendFactor( src->dstColorBlendFactor );
		dst->colorOp  = TranslateBlendOp( src->colorBlendOp );
		dst->srcAlpha = TranslateBlendFactor( src->srcAlphaBlendFactor );
		dst->dstAlpha = TranslateBlendFactor( src->dstAlphaBlendFactor );
		dst->alphaOp  = TranslateBlendOp( src->alphaBlendOp );
		dst->writeMask = (uint32_t)src->colorWriteMask;
		dst->writeMaskExplicit = qtrue;
	}
	if ( !FormatsValid( &next ) ) return qfalse;
	*out = next;
	return qtrue;
}

qboolean VK_RalAttachmentContractCopy(
	const vkRalAttachmentContract_t *in,
	vkRalAttachmentContract_t *out ) {
	if ( !out || !FormatsValid( in ) ) return qfalse;
	*out = *in;
	return qtrue;
}
