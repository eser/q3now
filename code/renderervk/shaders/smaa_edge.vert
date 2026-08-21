// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2013 Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro, Diego Gutierrez
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// SMAA Pass 1: Edge Detection — Vertex Shader

// rtMetrics rides in a per-frame UBO at set 3 of the shared SMAA pipeline layout,
// not a VS|FS push. Anonymous block keeps the read sites.
layout(set = 3, binding = 0, std140) uniform RtMetrics {
	vec4 rtMetrics; // { 1/w, 1/h, w, h }
};

layout(location = 0) out vec2 texcoord;
layout(location = 1) out vec4 offset0;
layout(location = 2) out vec4 offset1;
layout(location = 3) out vec4 offset2;

out gl_PerVertex { vec4 gl_Position; };

void main() {
	// fullscreen triangle: 3 vertices cover entire screen
	vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
	texcoord = pos;

	// neighbor offsets for edge detection
	offset0 = texcoord.xyxy + rtMetrics.xyxy * vec4(-1.0, 0.0, 0.0, -1.0);
	offset1 = texcoord.xyxy + rtMetrics.xyxy * vec4( 1.0, 0.0, 0.0,  1.0);
	offset2 = texcoord.xyxy + rtMetrics.xyxy * vec4(-2.0, 0.0, 0.0, -2.0);
}
