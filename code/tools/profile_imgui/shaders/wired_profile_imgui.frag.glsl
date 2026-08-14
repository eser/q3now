// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#version 450

layout(set = 0, binding = 0) uniform sampler2D fontAtlas;

layout(location = 0) in vec2 fragmentUv;
layout(location = 1) in vec4 fragmentColor;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = fragmentColor * texture( fontAtlas, fragmentUv );
}
