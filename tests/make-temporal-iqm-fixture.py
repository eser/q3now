#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
"""Write the deterministic loose IQM character used by the H5a runtime proof."""

import argparse
import hashlib
import math
import pathlib
import struct

MAGIC = b"INTERQUAKEMODEL\0"
ANIMS = (
    "BOTH_DEATH1", "BOTH_DEAD1", "BOTH_DEATH2", "BOTH_DEAD2",
    "BOTH_DEATH3", "BOTH_DEAD3", "TORSO_GESTURE", "TORSO_ATTACK",
    "TORSO_ATTACK2", "TORSO_DROP", "TORSO_RAISE", "TORSO_STAND",
    "TORSO_STAND2", "LEGS_WALKCR", "LEGS_WALK", "LEGS_RUN",
    "LEGS_BACK", "LEGS_SWIM", "LEGS_JUMP", "LEGS_LAND",
    "LEGS_JUMPB", "LEGS_LANDB", "LEGS_IDLE", "LEGS_IDLECR",
    "LEGS_TURN", "TORSO_GETFLAG", "TORSO_GUARDBASE", "TORSO_PATROL",
    "TORSO_FOLLOWME", "TORSO_AFFIRMATIVE", "TORSO_NEGATIVE",
    "LEGS_BACKCR", "LEGS_BACKWALK", "FLAG_RUN", "FLAG_STAND",
    "FLAG_STAND2RUN",
)


def align4(blob: bytearray) -> None:
    blob.extend(b"\0" * ((-len(blob)) & 3))


def build_iqm() -> bytes:
    names = ("fixture", "textures/h5a/fixture", "root") + ANIMS
    text = bytearray()
    offsets = {}
    for name in names:
        offsets[name] = len(text)
        text.extend(name.encode("ascii") + b"\0")

    header_size = 16 + 27 * 4
    blob = bytearray(b"\0" * header_size)

    ofs_text = len(blob)
    blob.extend(text)
    align4(blob)

    ofs_meshes = len(blob)
    blob.extend(struct.pack("<6I", offsets["fixture"],
                            offsets["textures/h5a/fixture"], 0, 3, 0, 2))

    ofs_vertexarrays = len(blob)
    vertex_array_patch = len(blob)
    blob.extend(b"\0" * (5 * 20))

    ofs_triangles = len(blob)
    blob.extend(struct.pack("<6I", 0, 1, 2, 2, 1, 0))

    ofs_joints = len(blob)
    blob.extend(struct.pack("<Ii10f", offsets["root"], -1,
                            0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 1.0,
                            1.0, 1.0, 1.0))

    ofs_poses = len(blob)
    channel_offset = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,
                      1.0, 1.0, 1.0)
    # Animate only the root's lateral Y translation.  The camera and world stay
    # fixed in the runtime contract, so nonzero screen velocity is attributable
    # to this exact two-pose palette chain rather than to camera input.
    channel_scale = (0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                     0.0, 0.0, 0.0)
    blob.extend(struct.pack("<iI20f", -1, 1 << 1,
                            *(channel_offset + channel_scale)))

    ofs_anims = len(blob)
    for name in ANIMS:
        blob.extend(struct.pack("<3IfI", offsets[name], 0, 2, 1000.0, 1))

    ofs_frames = len(blob)
    blob.extend(struct.pack("<2H", 0, 8))
    align4(blob)

    ofs_bounds = len(blob)
    bound = (-128.0, -128.0, -64.0, 128.0, 128.0, 128.0,
             math.sqrt(2.0 * 128.0 * 128.0),
             math.sqrt(3.0 * 128.0 * 128.0))
    blob.extend(struct.pack("<8f", *bound) * 2)

    # A camera-facing Y/Z triangle in the local-player plane.  Both winding
    # orders are indexed above so the fixture is visible from either side.
    positions = (0.0, -64.0, -32.0,
                 0.0, 64.0, -32.0,
                 0.0, 0.0, 96.0)
    texcoords = (0.0, 0.0, 1.0, 0.0, 0.5, 1.0)
    normals = (-1.0, 0.0, 0.0) * 3
    ofs_positions = len(blob)
    blob.extend(struct.pack("<9f", *positions))
    ofs_texcoords = len(blob)
    blob.extend(struct.pack("<6f", *texcoords))
    ofs_normals = len(blob)
    blob.extend(struct.pack("<9f", *normals))
    ofs_indexes = len(blob)
    blob.extend(bytes((0, 0, 0, 0)) * 3)
    ofs_weights = len(blob)
    blob.extend(bytes((255, 0, 0, 0)) * 3)

    arrays = (
        (0, 0, 7, 3, ofs_positions),
        (1, 0, 7, 2, ofs_texcoords),
        (2, 0, 7, 3, ofs_normals),
        (4, 0, 1, 4, ofs_indexes),
        (5, 0, 1, 4, ofs_weights),
    )
    for i, row in enumerate(arrays):
        struct.pack_into("<5I", blob, vertex_array_patch + i * 20, *row)

    header = (
        2, len(blob), 0,
        len(text), ofs_text,
        1, ofs_meshes,
        len(arrays), 3, ofs_vertexarrays,
        2, ofs_triangles, 0,
        1, ofs_joints,
        1, ofs_poses,
        len(ANIMS), ofs_anims,
        2, 1, ofs_frames, ofs_bounds,
        0, 0, 0, 0,
    )
    blob[:16] = MAGIC
    struct.pack_into("<27I", blob, 16, *header)
    return bytes(blob)


def write_fixture(base: pathlib.Path) -> None:
    iqm = build_iqm()
    model = base / "characters/h5a_fixture/models/body.iqm"
    manifest = base / "characters/h5a_fixture/main.lua"
    texture = base / "textures/h5a/fixture.tga"
    model.parent.mkdir(parents=True, exist_ok=True)
    texture.parent.mkdir(parents=True, exist_ok=True)
    model.write_bytes(iqm)
    manifest.write_text(
        "return {\n"
        "  archetype = \"mechanized_male\",\n"
        "  name = \"h5a_fixture\",\n"
        "  display_name = \"h5a_fixture\",\n"
        "  selectable = false,\n"
        "  model = { parts = { \"body\" }, skins = {} },\n"
        "}\n", encoding="ascii")
    # Uncompressed 2x2 BGRA, bottom-left origin, opaque pure green.
    texture.write_bytes(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0,
                                    0, 0, 2, 2, 32, 8)
                        + bytes((0, 255, 0, 255)) * 4)
    digest = hashlib.sha256(iqm).hexdigest()
    print(f"h5a-iqm-fixture bytes={len(iqm)} sha256={digest}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("base", type=pathlib.Path)
    args = parser.parse_args()
    write_fixture(args.base)


if __name__ == "__main__":
    main()
