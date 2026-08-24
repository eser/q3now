// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_legacy_material.h"

#include <math.h>
#include <string.h>

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static qboolean FactsValid( const ralLegacyMaterialFacts_t *facts ) {
	return facts && facts->textureCount <= 2u
		&& BoolValid(facts->shaderFog)
		&& BoolValid(facts->environmentMapping)
		&& BoolValid(facts->textureCoordinateAnimation)
		&& BoolValid(facts->vertexDeform);
}

static qboolean VariantValid( const ralLegacyMaterialFacts_t *facts,
		const ralLegacyMaterialVariant_t *variant ) {
	uint32_t textureDomainMask;
	if ( !variant || !BoolValid(variant->entityStorageTransform)
			|| !BoolValid(variant->alphaToCoverage)
			|| !BoolValid(variant->absoluteLight)
			|| !BoolValid(variant->iblEnabled)
			|| variant->alphaTest > RAL_LEGACY_ALPHA_TEST_KEEP_GREATER_EQUAL
			|| variant->combine > RAL_LEGACY_COMBINE_DST_COLOR_SOURCE_ALPHA
			|| !isfinite(variant->alphaTestValue)
			|| variant->alphaTestValue < 0.0f || variant->alphaTestValue > 1.0f
			|| !isfinite(variant->depthFragment)
			|| !isfinite(variant->fixedColor)
			|| variant->fixedColor < 0.0f || variant->fixedColor > 1.0f
			|| !isfinite(variant->fixedAlpha)
			|| variant->fixedAlpha < 0.0f || variant->fixedAlpha > 1.0f
			|| !isfinite(variant->depthFadeScale) || variant->depthFadeScale <= 0.0f
			|| variant->fogFactor > 3u || variant->discardMode > 2u
			|| variant->normalFormat > 2u ) return qfalse;
	textureDomainMask = (1u << (facts->textureCount + 1u)) - 1u;
	if ( (variant->textureDomainMask & ~textureDomainMask) != 0u
			|| (facts->textureCount == 0u
				&& variant->combine != RAL_LEGACY_COMBINE_MODULATE)
			|| (!facts->shaderFog && variant->fogFactor != 0u)
			|| variant->lightmapSlot > facts->textureCount + 1u ) return qfalse;
	return qtrue;
}

static ralLegacyFallbackReason_t FallbackReason(
		const ralLegacyMaterialVariant_t *variant ) {
	uint32_t alphaBits;
	memcpy( &alphaBits, &variant->alphaTestValue, sizeof(alphaBits) );
	if ( !variant->entityStorageTransform ) return RAL_LEGACY_FALLBACK_ENTITY_UNIFORM;
	if ( variant->alphaTest != RAL_LEGACY_ALPHA_TEST_NONE
			|| alphaBits != 0u ) return RAL_LEGACY_FALLBACK_ALPHA_TEST;
	if ( variant->alphaToCoverage ) return RAL_LEGACY_FALLBACK_ALPHA_TO_COVERAGE;
	if ( variant->absoluteLight ) return RAL_LEGACY_FALLBACK_ABSOLUTE_LIGHT;
	if ( variant->discardMode != 0u ) return RAL_LEGACY_FALLBACK_DISCARD;
	if ( variant->normalFormat != 0u ) return RAL_LEGACY_FALLBACK_NORMAL_MAP;
	if ( variant->iblEnabled ) return RAL_LEGACY_FALLBACK_IBL;
	return RAL_LEGACY_FALLBACK_NONE;
}

qboolean Ral_LegacyMaterialResolve( const ralLegacyMaterialFacts_t *facts,
		const ralLegacyMaterialVariant_t *variant,
		ralLegacyMaterialReceipt_t *out ) {
	ralLegacyMaterialReceipt_t candidate;
	uint32_t expectedDepthBits, actualDepthBits;
	const float expectedDepth = 0.85f;
	if ( !out || !FactsValid(facts) || !VariantValid(facts,variant) ) return qfalse;
	memcpy( &expectedDepthBits, &expectedDepth, sizeof(expectedDepthBits) );
	memcpy( &actualDepthBits, &variant->depthFragment, sizeof(actualDepthBits) );
	memset( &candidate, 0, sizeof(candidate) );
	candidate.facts = *facts;
	candidate.variant = *variant;
	candidate.fallbackReason = FallbackReason(variant);
	if ( actualDepthBits != expectedDepthBits
			&& candidate.fallbackReason == RAL_LEGACY_FALLBACK_NONE )
		candidate.fallbackReason = RAL_LEGACY_FALLBACK_DEPTH_FRAGMENT;
	candidate.outcome = candidate.fallbackReason == RAL_LEGACY_FALLBACK_NONE
		? RAL_LEGACY_MATERIAL_DIRECT : RAL_LEGACY_MATERIAL_FALLBACK;
	candidate.ready = qtrue;
	*out = candidate;
	return qtrue;
}

static qboolean FloatExact( float a, float b ) {
	uint32_t aBits, bBits;
	memcpy(&aBits,&a,sizeof(aBits));
	memcpy(&bBits,&b,sizeof(bBits));
	return aBits == bBits ? qtrue : qfalse;
}

static qboolean FactsExact( const ralLegacyMaterialFacts_t *a,
		const ralLegacyMaterialFacts_t *b ) {
	return a->textureCount == b->textureCount
		&& a->shaderFog == b->shaderFog
		&& a->environmentMapping == b->environmentMapping
		&& a->textureCoordinateAnimation == b->textureCoordinateAnimation
		&& a->vertexDeform == b->vertexDeform;
}

static qboolean VariantExact( const ralLegacyMaterialVariant_t *a,
		const ralLegacyMaterialVariant_t *b ) {
	return a->entityStorageTransform == b->entityStorageTransform
		&& a->alphaTest == b->alphaTest
		&& FloatExact(a->alphaTestValue,b->alphaTestValue)
		&& FloatExact(a->depthFragment,b->depthFragment)
		&& a->alphaToCoverage == b->alphaToCoverage
		&& a->textureDomainMask == b->textureDomainMask
		&& a->absoluteLight == b->absoluteLight
		&& a->combine == b->combine
		&& a->discardMode == b->discardMode
		&& FloatExact(a->fixedColor,b->fixedColor)
		&& FloatExact(a->fixedAlpha,b->fixedAlpha)
		&& a->fogFactor == b->fogFactor
		&& FloatExact(a->depthFadeScale,b->depthFadeScale)
		&& a->normalFormat == b->normalFormat
		&& a->iblEnabled == b->iblEnabled
		&& a->lightmapSlot == b->lightmapSlot;
}

qboolean Ral_LegacyMaterialReceiptExact( const ralLegacyMaterialReceipt_t *a,
		const ralLegacyMaterialReceipt_t *b ) {
	ralLegacyMaterialReceipt_t normalizedA, normalizedB;
	if ( !a || !b || !a->ready || !b->ready
			|| !Ral_LegacyMaterialResolve(&a->facts,&a->variant,&normalizedA)
			|| !Ral_LegacyMaterialResolve(&b->facts,&b->variant,&normalizedB)
			|| a->outcome != normalizedA.outcome
			|| a->fallbackReason != normalizedA.fallbackReason
			|| b->outcome != normalizedB.outcome
			|| b->fallbackReason != normalizedB.fallbackReason ) return qfalse;
	return a->outcome == b->outcome
		&& a->fallbackReason == b->fallbackReason
		&& FactsExact(&a->facts,&b->facts)
		&& VariantExact(&a->variant,&b->variant);
}
