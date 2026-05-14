// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

uniform sampler2D u_TextureMap;

uniform vec2      u_InvTexRes;
varying vec2      var_TexCoords;

void main()
{
	vec4 color;
	vec2 tc;
	
	tc = var_TexCoords + u_InvTexRes * vec2(-1.5, -1.5);  color  = texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2(-0.5, -1.5);  color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2( 0.5, -1.5);  color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2( 1.5, -1.5);  color += texture2D(u_TextureMap, tc);

	tc = var_TexCoords + u_InvTexRes * vec2(-1.5, -0.5); color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2(-0.5, -0.5); color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2( 0.5, -0.5); color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2( 1.5, -0.5); color += texture2D(u_TextureMap, tc);

	tc = var_TexCoords + u_InvTexRes * vec2(-1.5,  0.5); color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2(-0.5,  0.5); color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2( 0.5,  0.5); color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2( 1.5,  0.5); color += texture2D(u_TextureMap, tc);

	tc = var_TexCoords + u_InvTexRes * vec2(-1.5,  1.5);  color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2(-0.5,  1.5);  color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2( 0.5,  1.5);  color += texture2D(u_TextureMap, tc);
	tc = var_TexCoords + u_InvTexRes * vec2( 1.5,  1.5);  color += texture2D(u_TextureMap, tc);
	
	color *= 0.0625;
	
	gl_FragColor = color;
}
