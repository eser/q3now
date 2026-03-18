// q3now custom shaders

powerups/spawnProtect
{
	deformVertexes wave 100 sin 0.5 0.5 0 0.5
	{
		map textures/effects/envmap.tga
		tcGen environment
		rgbGen const ( 1.0 1.0 1.0 )
		blendFunc GL_ONE GL_ONE
	}
}
