// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

uniform sampler2D u_DiffuseMap;
uniform vec4      u_Color;

varying vec2      var_Tex1;


void main()
{
	gl_FragColor = texture2D(u_DiffuseMap, var_Tex1) * u_Color;
}
