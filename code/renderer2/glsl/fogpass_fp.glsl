// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

uniform vec4  u_Color;

varying float var_Scale;

void main()
{
	gl_FragColor = u_Color;
	gl_FragColor.a = sqrt(clamp(var_Scale, 0.0, 1.0));
}
