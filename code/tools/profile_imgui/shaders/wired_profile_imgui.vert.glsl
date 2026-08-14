// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#version 450

layout(push_constant) uniform DrawTransform {
	vec2 scale;
	vec2 translate;
} transform;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 fragmentUv;
layout(location = 1) out vec4 fragmentColor;

void main() {
	fragmentUv = inUv;
	fragmentColor = inColor;
	gl_Position = vec4( inPosition * transform.scale + transform.translate, 0.0, 1.0 );
}
