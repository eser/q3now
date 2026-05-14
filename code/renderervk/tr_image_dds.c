/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
              2015 James Canete

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================

Phase 6.5: Ported from code/renderer2/tr_image_dds.c. The file-format
parser and FourCC / DXGI tables are unchanged from the renderer2
implementation. The format dispatch swaps GL_COMPRESSED_* enums for the
matching VK_FORMAT_BC* values, and the loader hands its raw mip data
buffer back to the caller (tr_image.c R_FindImageFile detects the .dds
extension and routes here directly). The Vulkan upload path is not in
this file — see vk_upload_image_data_compressed in vk.c.
*/

#include "tr_local.h"

typedef unsigned int   ui32_t;

typedef struct ddsHeader_s
{
	ui32_t headerSize;
	ui32_t flags;
	ui32_t height;
	ui32_t width;
	ui32_t pitchOrFirstMipSize;
	ui32_t volumeDepth;
	ui32_t numMips;
	ui32_t reserved1[11];
	ui32_t always_0x00000020;
	ui32_t pixelFormatFlags;
	ui32_t fourCC;
	ui32_t rgbBitCount;
	ui32_t rBitMask;
	ui32_t gBitMask;
	ui32_t bBitMask;
	ui32_t aBitMask;
	ui32_t caps;
	ui32_t caps2;
	ui32_t caps3;
	ui32_t caps4;
	ui32_t reserved2;
}
ddsHeader_t;

// flags:
#define _DDSFLAGS_REQUIRED     0x001007
#define _DDSFLAGS_PITCH        0x8
#define _DDSFLAGS_MIPMAPCOUNT  0x20000
#define _DDSFLAGS_FIRSTMIPSIZE 0x80000
#define _DDSFLAGS_VOLUMEDEPTH  0x800000

// pixelFormatFlags:
#define DDSPF_ALPHAPIXELS 0x1
#define DDSPF_ALPHA       0x2
#define DDSPF_FOURCC      0x4
#define DDSPF_RGB         0x40
#define DDSPF_YUV         0x200
#define DDSPF_LUMINANCE   0x20000

// caps:
#define DDSCAPS_COMPLEX  0x8
#define DDSCAPS_MIPMAP   0x400000
#define DDSCAPS_REQUIRED 0x1000

// caps2:
#define DDSCAPS2_CUBEMAP            0x200
#define DDSCAPS2_CUBEMAP_POSITIVEX  0x400
#define DDSCAPS2_CUBEMAP_NEGATIVEX  0x800
#define DDSCAPS2_CUBEMAP_POSITIVEY  0x1000
#define DDSCAPS2_CUBEMAP_NEGATIVEY  0x2000
#define DDSCAPS2_CUBEMAP_POSITIVEZ  0x4000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ  0x8000
#define DDSCAPS2_CUBEMAP_ALLFACES \
	( DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX \
	| DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY \
	| DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ )
#define DDSCAPS2_VOLUME            0x200000

typedef struct ddsHeaderDxt10_s
{
	ui32_t dxgiFormat;
	ui32_t resourceDimension;   // D3D10_RESOURCE_DIMENSION: 2=1D, 3=2D, 4=3D
	ui32_t miscFlags;           // D3D10_RESOURCE_MISC_*: 0x4 = TEXTURECUBE
	ui32_t arraySize;           // array element count (per-face for cube arrays)
	ui32_t miscFlags2;
}
ddsHeaderDxt10_t;

// D3D10_RESOURCE_DIMENSION
#define DDS_DIMENSION_TEXTURE1D  2
#define DDS_DIMENSION_TEXTURE2D  3
#define DDS_DIMENSION_TEXTURE3D  4
// D3D10_RESOURCE_MISC_TEXTURECUBE
#define DDS_RESOURCE_MISC_TEXTURECUBE  0x4

// DXGI_FORMAT subset — only the entries the loader maps. The values
// match the Microsoft enum (from MSDN bb173059), kept stable as the
// DDS file format spec.
enum {
	DXGI_FORMAT_R16G16B16A16_FLOAT       = 10,
	DXGI_FORMAT_R8G8B8A8_UNORM           = 28,
	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB      = 29,
	DXGI_FORMAT_R8G8B8A8_SNORM           = 31,
	DXGI_FORMAT_BC1_TYPELESS             = 70,
	DXGI_FORMAT_BC1_UNORM                = 71,
	DXGI_FORMAT_BC1_UNORM_SRGB           = 72,
	DXGI_FORMAT_BC2_TYPELESS             = 73,
	DXGI_FORMAT_BC2_UNORM                = 74,
	DXGI_FORMAT_BC2_UNORM_SRGB           = 75,
	DXGI_FORMAT_BC3_TYPELESS             = 76,
	DXGI_FORMAT_BC3_UNORM                = 77,
	DXGI_FORMAT_BC3_UNORM_SRGB           = 78,
	DXGI_FORMAT_BC4_TYPELESS             = 79,
	DXGI_FORMAT_BC4_UNORM                = 80,
	DXGI_FORMAT_BC4_SNORM                = 81,
	DXGI_FORMAT_BC5_TYPELESS             = 82,
	DXGI_FORMAT_BC5_UNORM                = 83,
	DXGI_FORMAT_BC5_SNORM                = 84,
	DXGI_FORMAT_BC6H_TYPELESS            = 94,
	DXGI_FORMAT_BC6H_UF16                = 95,
	DXGI_FORMAT_BC6H_SF16                = 96,
	DXGI_FORMAT_BC7_TYPELESS             = 97,
	DXGI_FORMAT_BC7_UNORM                = 98,
	DXGI_FORMAT_BC7_UNORM_SRGB           = 99
};

#define EncodeFourCC(x) ((((ui32_t)((x)[0]))      ) | \
                         (((ui32_t)((x)[1])) << 8 ) | \
                         (((ui32_t)((x)[2])) << 16) | \
                         (((ui32_t)((x)[3])) << 24) )

/*
===============
R_LoadDDS

Parse a .dds file, validate the magic and header, identify the
texel format, and return the raw mip-chain buffer.

The buffer hands back to the caller via *pic; ownership transfers
to the caller (free via ri.Free). *picFormat receives the matching
VkFormat (or VK_FORMAT_UNDEFINED on parse failure). *numMips
receives the mip-chain length declared in the header (>= 1).

Phase 6.5: BC1 / BC3 / BC5 / BC7 are the audit-priority formats and
get UNORM + sRGB variants; BC2 / BC4 / BC6H map per the renderer2
table for completeness. The caller (R_FindImageFile) checks
vk.bc_formats_supported and falls back to NULL when the GPU rejects
a format.

Phase 6.5.1: *info (may be NULL) receives the cubemap / volume
classification. A cubemap is recognised either via the legacy
DDSCAPS2_CUBEMAP caps2 bits (all six face bits must be present —
partial cubemaps are rejected, Vulkan needs all 6 layers) or the
DXT10 D3D10_RESOURCE_MISC_TEXTURECUBE flag; a volume via
DDSCAPS2_VOLUME / DDS_DIMENSION_TEXTURE3D (depth from dwDepth). For
a cubemap *numMips is the per-face mip count and *dataSize spans all
six faces' mip chains laid out face-major (+X,-X,+Y,-Y,+Z,-Z), the
order Vulkan uses for cube array layers. Cube arrays (DXT10
arraySize > 1) are not produced yet — treated as a plain cubemap on
the first element with a warning.
===============
*/
void R_LoadDDS( const char *filename, byte **pic, int *width, int *height, VkFormat *picFormat, int *numMips, int *dataSize, ddsImageInfo_t *info )
{
	union {
		byte *b;
		void *v;
	} buffer;
	ddsHeader_t *ddsHeader = NULL;
	ddsHeaderDxt10_t *ddsHeaderDxt10 = NULL;
	byte *data;
	int len;

	if ( !picFormat )
	{
		ri.Log( SEV_ERROR, "R_LoadDDS() called without picFormat parameter!" );
		return;
	}

	if ( width )
		*width = 0;
	if ( height )
		*height = 0;
	*picFormat = VK_FORMAT_UNDEFINED;
	if ( numMips )
		*numMips = 1;
	if ( dataSize )
		*dataSize = 0;
	if ( info )
	{
		info->texType = TEXTYPE_2D;
		info->layers  = 1;
		info->depth   = 1;
	}

	*pic = NULL;

	//
	// load the file
	//
	len = ri.FS_ReadFile( (char *)filename, &buffer.v );
	if ( !buffer.b || len < 0 ) {
		return;
	}

	//
	// reject files that are too small to hold even a header
	//
	if ( (size_t)len < 4 + sizeof( *ddsHeader ) )
	{
		ri.Log( SEV_INFO, "File %s is too small to be a DDS file.\n", filename );
		ri.FS_FreeFile( buffer.v );
		return;
	}

	//
	// reject files that don't start with "DDS "
	//
	if ( *((ui32_t *)(buffer.b)) != EncodeFourCC( "DDS " ) )
	{
		ri.Log( SEV_INFO, "File %s is not a DDS file.\n", filename );
		ri.FS_FreeFile( buffer.v );
		return;
	}

	//
	// parse header and dx10 header if available
	//
	ddsHeader = (ddsHeader_t *)(buffer.b + 4);
	if ( ( ddsHeader->pixelFormatFlags & DDSPF_FOURCC ) && ddsHeader->fourCC == EncodeFourCC( "DX10" ) )
	{
		if ( (size_t)len < 4 + sizeof( *ddsHeader ) + sizeof( *ddsHeaderDxt10 ) )
		{
			ri.Log( SEV_INFO, "File %s indicates a DX10 header it is too small to contain.\n", filename );
			ri.FS_FreeFile( buffer.v );
			return;
		}

		ddsHeaderDxt10 = (ddsHeaderDxt10_t *)(buffer.b + 4 + sizeof( ddsHeader_t ));
		data = buffer.b + 4 + sizeof( *ddsHeader ) + sizeof( *ddsHeaderDxt10 );
		len -= 4 + sizeof( *ddsHeader ) + sizeof( *ddsHeaderDxt10 );
	}
	else
	{
		data = buffer.b + 4 + sizeof( *ddsHeader );
		len -= 4 + sizeof( *ddsHeader );
	}

	if ( width )
		*width = (int)ddsHeader->width;
	if ( height )
		*height = (int)ddsHeader->height;

	if ( numMips )
	{
		if ( ddsHeader->flags & _DDSFLAGS_MIPMAPCOUNT )
			*numMips = (int)ddsHeader->numMips;
		else
			*numMips = 1;
	}

	//
	// Phase 6.5.1: cubemap / volume classification.
	//
	if ( info )
	{
		qboolean isCube   = qfalse;
		qboolean isVolume = qfalse;
		int      cubeArraySize = 1;

		if ( ddsHeaderDxt10 )
		{
			if ( ddsHeaderDxt10->resourceDimension == DDS_DIMENSION_TEXTURE3D )
				isVolume = qtrue;
			else if ( ( ddsHeaderDxt10->resourceDimension == DDS_DIMENSION_TEXTURE2D )
			       && ( ddsHeaderDxt10->miscFlags & DDS_RESOURCE_MISC_TEXTURECUBE ) )
			{
				isCube = qtrue;
				if ( ddsHeaderDxt10->arraySize > 1 )
					cubeArraySize = (int)ddsHeaderDxt10->arraySize;
			}
		}
		else
		{
			if ( ddsHeader->caps2 & DDSCAPS2_VOLUME )
				isVolume = qtrue;
			else if ( ddsHeader->caps2 & DDSCAPS2_CUBEMAP )
			{
				if ( ( ddsHeader->caps2 & DDSCAPS2_CUBEMAP_ALLFACES ) != DDSCAPS2_CUBEMAP_ALLFACES )
				{
					ri.Log( SEV_INFO, "DDS File %s is a partial cubemap (caps2 0x%x); a complete 6-face cubemap is required.\n",
						filename, (unsigned)ddsHeader->caps2 );
					ri.FS_FreeFile( buffer.v );
					return;
				}
				isCube = qtrue;
			}
		}

		if ( isCube )
		{
			if ( cubeArraySize > 1 )
				ri.Log( SEV_INFO, "DDS File %s is a cubemap array (arraySize %d); only the first cube is used (cube arrays unimplemented).\n",
					filename, cubeArraySize );
			info->texType = TEXTYPE_CUBE;
			info->layers  = 6;
			info->depth   = 1;
		}
		else if ( isVolume )
		{
			int d = (int)ddsHeader->volumeDepth;
			if ( !( ddsHeader->flags & _DDSFLAGS_VOLUMEDEPTH ) || d < 1 )
				d = 1;
			info->texType = TEXTYPE_3D;
			info->layers  = 1;
			info->depth   = d;
		}
	}

	//
	// Convert DXGI format / FourCC into VkFormat
	//
	if ( ddsHeaderDxt10 )
	{
		switch ( ddsHeaderDxt10->dxgiFormat )
		{
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
				*picFormat = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
				break;

			case DXGI_FORMAT_BC1_UNORM_SRGB:
				*picFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
				break;

			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
				*picFormat = VK_FORMAT_BC2_UNORM_BLOCK;
				break;

			case DXGI_FORMAT_BC2_UNORM_SRGB:
				*picFormat = VK_FORMAT_BC2_SRGB_BLOCK;
				break;

			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
				*picFormat = VK_FORMAT_BC3_UNORM_BLOCK;
				break;

			case DXGI_FORMAT_BC3_UNORM_SRGB:
				*picFormat = VK_FORMAT_BC3_SRGB_BLOCK;
				break;

			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
				*picFormat = VK_FORMAT_BC4_UNORM_BLOCK;
				break;

			case DXGI_FORMAT_BC4_SNORM:
				*picFormat = VK_FORMAT_BC4_SNORM_BLOCK;
				break;

			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
				*picFormat = VK_FORMAT_BC5_UNORM_BLOCK;
				break;

			case DXGI_FORMAT_BC5_SNORM:
				*picFormat = VK_FORMAT_BC5_SNORM_BLOCK;
				break;

			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
				*picFormat = VK_FORMAT_BC6H_UFLOAT_BLOCK;
				break;

			case DXGI_FORMAT_BC6H_SF16:
				*picFormat = VK_FORMAT_BC6H_SFLOAT_BLOCK;
				break;

			case DXGI_FORMAT_BC7_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
				*picFormat = VK_FORMAT_BC7_UNORM_BLOCK;
				break;

			case DXGI_FORMAT_BC7_UNORM_SRGB:
				*picFormat = VK_FORMAT_BC7_SRGB_BLOCK;
				break;

			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				*picFormat = VK_FORMAT_R8G8B8A8_SRGB;
				break;

			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
				*picFormat = VK_FORMAT_R8G8B8A8_UNORM;
				break;

			// Phase 6.5.1: 16-bit-float RGBA — the natural carrier for HDR
			// cubemaps that aren't BC6H-compressed (6C2 IBL probe captures).
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
				*picFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
				break;

			default:
				ri.Log( SEV_INFO, "DDS File %s has unsupported DXGI format %d.\n", filename, ddsHeaderDxt10->dxgiFormat );
				ri.FS_FreeFile( buffer.v );
				return;
		}
	}
	else
	{
		if ( ddsHeader->pixelFormatFlags & DDSPF_FOURCC )
		{
			if ( ddsHeader->fourCC == EncodeFourCC( "DXT1" ) )
				*picFormat = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "DXT2" ) )
				*picFormat = VK_FORMAT_BC2_UNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "DXT3" ) )
				*picFormat = VK_FORMAT_BC2_UNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "DXT4" ) )
				*picFormat = VK_FORMAT_BC3_UNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "DXT5" ) )
				*picFormat = VK_FORMAT_BC3_UNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "ATI1" ) )
				*picFormat = VK_FORMAT_BC4_UNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "BC4U" ) )
				*picFormat = VK_FORMAT_BC4_UNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "BC4S" ) )
				*picFormat = VK_FORMAT_BC4_SNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "ATI2" ) )
				*picFormat = VK_FORMAT_BC5_UNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "BC5U" ) )
				*picFormat = VK_FORMAT_BC5_UNORM_BLOCK;
			else if ( ddsHeader->fourCC == EncodeFourCC( "BC5S" ) )
				*picFormat = VK_FORMAT_BC5_SNORM_BLOCK;
			else
			{
				ri.Log( SEV_INFO, "DDS File %s has unsupported FourCC.\n", filename );
				ri.FS_FreeFile( buffer.v );
				return;
			}
		}
		else if ( ddsHeader->pixelFormatFlags == ( DDSPF_RGB | DDSPF_ALPHAPIXELS )
			&& ddsHeader->rgbBitCount == 32
			&& ddsHeader->rBitMask == 0x000000ff
			&& ddsHeader->gBitMask == 0x0000ff00
			&& ddsHeader->bBitMask == 0x00ff0000
			&& ddsHeader->aBitMask == 0xff000000 )
		{
			*picFormat = VK_FORMAT_R8G8B8A8_UNORM;
		}
		else
		{
			ri.Log( SEV_INFO, "DDS File %s has unsupported RGBA format.\n", filename );
			ri.FS_FreeFile( buffer.v );
			return;
		}
	}

	*pic = ri.Malloc( len );
	memcpy( *pic, data, len );
	if ( dataSize )
		*dataSize = len;

	ri.FS_FreeFile( buffer.v );
}
