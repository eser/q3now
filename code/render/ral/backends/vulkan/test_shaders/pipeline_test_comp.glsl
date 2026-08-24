// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Wired RAL pipeline-test compute shader. Writes idx*3+7 into ssbo.data[idx]
// for idx < count. Lets the \ral_dump pipeline test verify Ral_CmdDispatch +
// Ral_CmdBindBindGroup + push constants end-to-end via an SSBO readback.
//
// glslangValidator -V pipeline_test_comp.glsl -S comp -o pipeline_test_comp.spv

#version 450

layout(local_size_x = 64) in;

layout(set = 0, binding = 0) buffer SSBO { uint data[]; } ssbo;
layout(push_constant) uniform PC { uint count; };

void main() {
	uint idx = gl_GlobalInvocationID.x;
	if ( idx < count ) {
		ssbo.data[idx] = idx * 3u + 7u;
	}
}
