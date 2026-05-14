// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Wired RAL pipeline-test fragment shader. Passes per-vertex color through.
//
// glslangValidator -V pipeline_test_frag.glsl -S frag -o pipeline_test_frag.spv

#version 450

layout(location = 0) in vec3 frag_color;
layout(location = 0) out vec4 out_color;

void main() {
	out_color = vec4(frag_color, 1.0);
}
