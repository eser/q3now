// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_resource.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x); return 1; } } while (0)

static ralSamplerCreateInfo_t ValidInfo( void ) {
	ralSamplerCreateInfo_t ci;
	memset( &ci, 0, sizeof( ci ) );
	ci.minFilter = RAL_FILTER_LINEAR;
	ci.magFilter = RAL_FILTER_LINEAR;
	ci.mipmapMode = RAL_MIPMAP_LINEAR;
	ci.addressU = RAL_ADDRESS_CLAMP_TO_BORDER;
	ci.addressV = RAL_ADDRESS_CLAMP_TO_BORDER;
	ci.addressW = RAL_ADDRESS_CLAMP_TO_BORDER;
	ci.maxAnisotropy = 16.0f;
	ci.compareEnable = qtrue;
	ci.compareOp = RAL_COMPARE_LESS_EQUAL;
	ci.minLod = 1.0f;
	ci.maxLod = 8.0f;
	ci.borderColor = RAL_BORDER_OPAQUE_WHITE;
	return ci;
}

int main( void ) {
	ralSamplerCreateInfo_t ci = ValidInfo();
	CHECK( !Ral_SamplerCreateInfoValid( NULL ) );
	CHECK( Ral_SamplerCreateInfoValid( &ci ) );
	ci.maxLod = 0.0f;
	CHECK( Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.minFilter = (ralFilter_t)2;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.magFilter = (ralFilter_t)-1;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.mipmapMode = (ralMipmapMode_t)2;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.addressU = (ralAddressMode_t)4;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.addressV = (ralAddressMode_t)-1;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.addressW = (ralAddressMode_t)4;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.maxAnisotropy = -1.0f;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.maxAnisotropy = FLT_MAX; ci.maxAnisotropy *= 2.0f;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.compareEnable = (qboolean)2;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.compareOp = (ralCompareOp_t)8;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.minLod = -1.0f;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.maxLod = 0.5f;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.borderColor = (ralBorderColor_t)3;
	CHECK( !Ral_SamplerCreateInfoValid( &ci ) );
	ci = ValidInfo(); ci.borderColor = RAL_BORDER_TRANSPARENT_BLACK;
	CHECK( Ral_SamplerCreateInfoValid( &ci ) );
	ci.borderColor = RAL_BORDER_OPAQUE_BLACK;
	CHECK( Ral_SamplerCreateInfoValid( &ci ) );
	puts( "RAL sampler policy: PASS" );
	return 0;
}
