// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_resource.h"
#include "render_texture_sampling.h"

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
	const char *modes[] = { "GL_NEAREST", "GL_LINEAR", "GL_NEAREST_MIPMAP_NEAREST",
		"GL_LINEAR_MIPMAP_NEAREST", "GL_NEAREST_MIPMAP_LINEAR", "GL_LINEAR_MIPMAP_LINEAR" };
	for ( unsigned i = 0; i < 6; ++i ) {
		CHECK( wired_texture_sampling_policy( modes[i], qtrue, 8.0f, &ci ) );
		CHECK( Ral_SamplerCreateInfoValid( &ci ) );
		CHECK( ci.minFilter == ( ( i & 1 ) ? RAL_FILTER_LINEAR : RAL_FILTER_NEAREST ) );
		CHECK( ci.magFilter == ci.minFilter );
		CHECK( ci.mipmapMode == ( i >= 4 ? RAL_MIPMAP_LINEAR : RAL_MIPMAP_NEAREST ) );
		CHECK( ci.maxLod == ( i < 2 ? 0.25f : 0.0f ) );
		CHECK( ci.maxAnisotropy == ( i == 5 ? 8.0f : 1.0f ) );
	}
	CHECK( wired_texture_sampling_policy( "gl_linear_mipmap_linear", qfalse, 8.0f, &ci ) );
	CHECK( ci.maxAnisotropy == 1.0f );
	ralSamplerCreateInfo_t before = ci;
	CHECK( !wired_texture_sampling_policy( "GL_LINEAR_INVALID", qtrue, 8.0f, &ci ) );
	CHECK( !wired_texture_sampling_policy( NULL, qtrue, 8.0f, &ci ) );
	CHECK( !wired_texture_sampling_policy( modes[5], qtrue, NAN, &ci ) );
	CHECK( !wired_texture_sampling_policy( modes[5], qtrue, 0.0f, &ci ) );
	CHECK( !memcmp( &before, &ci, sizeof( ci ) ) );
	ci = ValidInfo();
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
