#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# Phase 6.5.1 — synthesize the tiny DDS smoke assets for the cubemap /
# volume loader:
#
#   gfx/env/test_cube.dds   4x4 BC1 (DXT1) cubemap, 6 faces, 1 mip,
#                           one solid colour per face (+X red, -X cyan,
#                           +Y green, -Y magenta, +Z blue, -Z yellow).
#   gfx/test_volume.dds     4x4x4 uncompressed R8G8B8A8 volume, 1 mip,
#                           one solid colour per z-slice (red/green/blue/white).
#   gfx/test_normal_bc5.dds 4x4 BC5 UNORM normal map (Phase 6.5.2), DXT10
#                           header (DXGI_FORMAT_BC5_UNORM=83), 1 mip, every
#                           texel R=128 G=128 → tangent normal (≈0,≈0,1)
#                           after the shader unpacks RG and reconstructs Z.
#
# The cube/volume assets validate the Phase 6.5.1 DDS header
# classification + cube/3D VkImage + view + upload (via the `testdds`
# debug command). The BC5 normal asset validates Phase 6.5.2's loader
# path → image->internalFormat == VK_FORMAT_BC5_UNORM_BLOCK → the
# `normal_format` spec constant. None is referenced by a shipped
# shader yet (samplerCube/sampler3D rendering lands with IBL; a normal-
# mapped test surface is a content task).
#
# Usage: python3 tools/gen_test_dds.py [outdir1 outdir2 ...]
#   Writes gfx/env/test_cube.dds, gfx/test_volume.dds and
#   gfx/test_normal_bc5.dds under each given dir (default: modfiles/).

import os
import struct
import sys

# ---- DDS constants -------------------------------------------------------
DDSD_CAPS        = 0x1
DDSD_HEIGHT      = 0x2
DDSD_WIDTH       = 0x4
DDSD_PIXELFORMAT = 0x1000
DDSD_LINEARSIZE  = 0x80000
DDSD_DEPTH       = 0x800000

DDPF_ALPHAPIXELS = 0x1
DDPF_FOURCC      = 0x4
DDPF_RGB         = 0x40

DDSCAPS_COMPLEX  = 0x8
DDSCAPS_TEXTURE  = 0x1000

DDSCAPS2_CUBEMAP           = 0x200
DDSCAPS2_CUBEMAP_ALLFACES  = 0xFC00          # +X -X +Y -Y +Z -Z bits
DDSCAPS2_VOLUME            = 0x200000


def dds_header(width, height, depth, pf_flags, fourcc, rgb_bits,
               masks, caps, caps2, flags):
    # 4-byte magic + 124-byte DDS_HEADER
    magic = b"DDS "
    rmask, gmask, bmask, amask = masks
    fourcc_u32 = struct.unpack("<I", fourcc.ljust(4, b"\0")[:4])[0] if fourcc else 0
    pixelformat = struct.pack("<II I IIII",
                              32,            # dwSize
                              pf_flags,      # dwFlags
                              fourcc_u32,    # dwFourCC
                              rgb_bits,      # dwRGBBitCount
                              rmask, gmask, bmask)  # masks (R G B)
    pixelformat += struct.pack("<I", amask)  # dwABitMask
    assert len(pixelformat) == 32
    header = struct.pack("<I I I I I I I",
                         124,               # dwSize
                         flags,             # dwFlags
                         height,            # dwHeight
                         width,             # dwWidth
                         0,                 # dwPitchOrLinearSize (ignored by loader)
                         depth,             # dwDepth
                         0)                 # dwMipMapCount (0 -> loader uses 1)
    header += b"\0" * (11 * 4)              # dwReserved1[11]
    header += pixelformat
    header += struct.pack("<I I I I I",
                          caps,             # dwCaps
                          caps2,            # dwCaps2
                          0,                # dwCaps3
                          0,                # dwCaps4
                          0)                # dwReserved2
    assert len(header) == 124
    return magic + header


# DXGI_FORMAT subset (Microsoft enum values, == DDS spec)
DXGI_FORMAT_BC5_UNORM = 83
# D3D10_RESOURCE_DIMENSION
DDS_DIMENSION_TEXTURE2D = 3


def dds_header_dxt10(width, height, depth, dxgi_format, dimension,
                     caps, caps2, flags, pitch_or_linear_size=0,
                     misc_flags=0, array_size=1):
    # 4-byte magic + 124-byte DDS_HEADER (FourCC "DX10") + 20-byte DDS_HEADER_DXT10
    magic = b"DDS "
    pixelformat = struct.pack("<II I I IIII",
                              32,                # dwSize
                              DDPF_FOURCC,       # dwFlags
                              struct.unpack("<I", b"DX10")[0],  # dwFourCC = "DX10"
                              0, 0, 0, 0, 0)     # bitcount + 4 masks (unused)
    assert len(pixelformat) == 32
    header = struct.pack("<I I I I I I I",
                         124, flags, height, width,
                         pitch_or_linear_size, depth, 0)
    header += b"\0" * (11 * 4)
    header += pixelformat
    header += struct.pack("<I I I I I", caps, caps2, 0, 0, 0)
    assert len(header) == 124
    dxt10 = struct.pack("<I I I I I",
                        dxgi_format,   # dxgiFormat
                        dimension,     # resourceDimension
                        misc_flags,    # miscFlag
                        array_size,    # arraySize
                        0)             # miscFlags2
    assert len(dxt10) == 20
    return magic + header + dxt10


# ---- BC1 (DXT1) ----------------------------------------------------------
def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def bc1_solid_block(r, g, b):
    # color0 == color1 -> 3-colour mode; all indices 0 -> color0 everywhere,
    # full alpha (index 3 would be transparent black, which we never use).
    c = rgb565(r, g, b)
    return struct.pack("<HH I", c, c, 0)   # 8 bytes


def gen_cube():
    # DDS cube face order is +X, -X, +Y, -Y, +Z, -Z (== Vulkan cube layers 0..5)
    faces = [
        (255,   0,   0),   # +X red
        (  0, 255, 255),   # -X cyan
        (  0, 255,   0),   # +Y green
        (255,   0, 255),   # -Y magenta
        (  0,   0, 255),   # +Z blue
        (255, 255,   0),   # -Z yellow
    ]
    hdr = dds_header(4, 4, 0,
                     DDPF_FOURCC, b"DXT1", 0, (0, 0, 0, 0),
                     DDSCAPS_TEXTURE | DDSCAPS_COMPLEX,
                     DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALLFACES,
                     DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE)
    body = b"".join(bc1_solid_block(*c) for c in faces)   # 6 faces * 1 block * 8 = 48 bytes
    return hdr + body


def gen_volume():
    # 4x4x4 R8G8B8A8, one solid colour per z-slice. Byte order R,G,B,A to
    # match the loader's DDPF_RGB|DDPF_ALPHAPIXELS 32-bit recognition and
    # VK_FORMAT_R8G8B8A8_UNORM.
    slices = [
        (255,   0,   0, 255),   # z0 red
        (  0, 255,   0, 255),   # z1 green
        (  0,   0, 255, 255),   # z2 blue
        (255, 255, 255, 255),   # z3 white
    ]
    hdr = dds_header(4, 4, 4,
                     DDPF_RGB | DDPF_ALPHAPIXELS, b"", 32,
                     (0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000),
                     DDSCAPS_TEXTURE | DDSCAPS_COMPLEX,
                     DDSCAPS2_VOLUME,
                     DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_DEPTH)
    body = b""
    for (r, g, b, a) in slices:
        body += bytes([r, g, b, a]) * (4 * 4)    # 16 px/slice -> 64 bytes; 4 slices -> 256 bytes
    return hdr + body


# ---- BC4 / BC5 -----------------------------------------------------------
def bc4_solid_block(v):
    # BC4: red0, red1 (8-bit endpoints) + 16 x 3-bit indices (6 bytes).
    # red0 == red1 -> the "6-interpolated" mode; index 0..5 all give the
    # endpoint value, indices 6/7 give 0.0/1.0. All-zero indices -> v.
    return bytes([v & 0xFF, v & 0xFF, 0, 0, 0, 0, 0, 0])   # 8 bytes


def bc5_solid_block(r, g):
    # BC5 == two BC4 blocks: block 0 = R channel, block 1 = G channel
    # (D3D BC5_UNORM convention, == VK_FORMAT_BC5_UNORM_BLOCK).
    return bc4_solid_block(r) + bc4_solid_block(g)          # 16 bytes


def gen_bc5_normal():
    # 4x4 BC5 UNORM, every texel R=128 G=128. The shader unpacks RG to
    # ~(0,0) in tangent space and reconstructs Z = sqrt(1-x²-y²) ≈ 1, i.e.
    # the neutral "flat surface" tangent normal (0, 0, 1). DXT10 header so
    # the format is unambiguous (DXGI_FORMAT_BC5_UNORM -> VK_FORMAT_BC5_UNORM_BLOCK).
    hdr = dds_header_dxt10(4, 4, 0,
                           DXGI_FORMAT_BC5_UNORM, DDS_DIMENSION_TEXTURE2D,
                           DDSCAPS_TEXTURE, 0,
                           DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE,
                           pitch_or_linear_size=16)
    body = bc5_solid_block(128, 128)                        # 4x4 -> 1 block -> 16 bytes
    return hdr + body


def write(outdir, relpath, data):
    full = os.path.join(outdir, relpath)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "wb") as f:
        f.write(data)
    print(f"  wrote {full} ({len(data)} bytes)")


def main():
    outdirs = sys.argv[1:] or ["modfiles"]
    cube       = gen_cube()
    vol        = gen_volume()
    bc5_normal = gen_bc5_normal()
    for d in outdirs:
        print(f"[{d}]")
        write(d, "gfx/env/test_cube.dds", cube)
        write(d, "gfx/test_volume.dds", vol)
        write(d, "gfx/test_normal_bc5.dds", bc5_normal)


if __name__ == "__main__":
    main()
