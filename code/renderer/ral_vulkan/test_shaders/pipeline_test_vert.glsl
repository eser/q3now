// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Wired RAL pipeline-test vertex shader (Phase 7.3c \ral_dump pipeline).
// Push-constant MVP × per-vertex (pos, color) → frag_color. Tiny so the
// resulting SPIR-V blob is small enough to embed verbatim in the test code.
//
// glslangValidator -V pipeline_test_vert.glsl -S vert -o pipeline_test_vert.spv

#version 450

layout(push_constant) uniform PC { mat4 mvp; };

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 frag_color;

void main() {
	gl_Position = mvp * vec4(in_pos, 1.0);
	frag_color = in_color;
}
