// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_resource.h"

#include <float.h>

static qboolean Ral_SamplerFinite( float value ) {
	return value == value && value >= -FLT_MAX && value <= FLT_MAX;
}

qboolean Ral_SamplerCreateInfoValid( const ralSamplerCreateInfo_t *ci ) {
	if ( !ci
			|| ci->minFilter < RAL_FILTER_NEAREST
			|| ci->minFilter > RAL_FILTER_LINEAR
			|| ci->magFilter < RAL_FILTER_NEAREST
			|| ci->magFilter > RAL_FILTER_LINEAR
			|| ci->mipmapMode < RAL_MIPMAP_NEAREST
			|| ci->mipmapMode > RAL_MIPMAP_LINEAR
			|| ci->addressU < RAL_ADDRESS_REPEAT
			|| ci->addressU > RAL_ADDRESS_CLAMP_TO_BORDER
			|| ci->addressV < RAL_ADDRESS_REPEAT
			|| ci->addressV > RAL_ADDRESS_CLAMP_TO_BORDER
			|| ci->addressW < RAL_ADDRESS_REPEAT
			|| ci->addressW > RAL_ADDRESS_CLAMP_TO_BORDER
			|| !Ral_SamplerFinite( ci->maxAnisotropy )
			|| ci->maxAnisotropy < 0.0f
			|| ( ci->compareEnable != qfalse && ci->compareEnable != qtrue )
			|| ci->compareOp < RAL_COMPARE_NEVER
			|| ci->compareOp > RAL_COMPARE_ALWAYS
			|| !Ral_SamplerFinite( ci->minLod ) || ci->minLod < 0.0f
			|| !Ral_SamplerFinite( ci->maxLod ) || ci->maxLod < 0.0f
			|| ( ci->maxLod > 0.0f && ci->maxLod < ci->minLod )
			|| ci->borderColor < RAL_BORDER_TRANSPARENT_BLACK
			|| ci->borderColor > RAL_BORDER_OPAQUE_WHITE ) {
		return qfalse;
	}
	return qtrue;
}
