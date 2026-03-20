// q3now custom shaders

// ── q3now lens flare shaders ────────────────────────────────────────
// Alternatives inspired by JJ Abrams / cinematic lens flare references.
// All use sort nearest + additive alpha blend for proper compositing.

// Warm soft glow — gaussian falloff, core of map/missile flares
lfWarmGlow
{
	sort nearest
	nopicmip
	{
		map sprites/bfglfglare.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
	}
}

// Cool blue glow — JJ Abrams map flare tint
lfCoolGlow
{
	sort nearest
	nopicmip
	{
		map sprites/bfglfglare.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen const ( 0.6 0.75 1.0 )
	}
}

// Anamorphic horizontal streak — film camera signature
lfAnamorphic
{
	sort nearest
	nopicmip
	nomipmaps
	{
		clampmap sprites/bfglfline.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
		tcmod transform 1 0 0 48 0 -23.5
	}
}

// Blue anamorphic streak — JJ Abrams signature
lfBlueStreak
{
	sort nearest
	nopicmip
	nomipmaps
	{
		clampmap sprites/bfglfline.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen const ( 0.5 0.65 1.0 )
		tcmod transform 1 0 0 48 0 -23.5
	}
}

// Star spike — multi-ray pattern
lfStar
{
	sort nearest
	nopicmip
	{
		map sprites/bfglfstar.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
	}
}

// Ghost ring — translucent circular reflection (secondary element)
lfGhostRing
{
	sort nearest
	nopicmip
	{
		map sprites/bfglfring.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
	}
}

// Ghost disc — small translucent reflection blob
lfGhostDisc
{
	sort nearest
	nopicmip
	{
		map sprites/bfglfdisc.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
	}
}

// Powerup glow — pulsing, used for quad/haste/regen pickup items
lfPowerupGlow
{
	sort nearest
	nopicmip
	{
		map sprites/bfglfglare.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
		tcmod turb 0 0.05 0 0.3
	}
}

// ── end q3now lens flare shaders ────────────────────────────────────

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

// ── atmospheric effects ────────────────────────────────────────────

// Rain drop — semi-transparent white, additive blend
gfx/misc/raindrop
{
	nopicmip
	cull none
	{
		map sprites/spot.tga
		blendfunc gl_src_alpha gl_one
		rgbgen vertex
		alphagen vertex
	}
}

// Snow flake — soft white dot, alpha blend
gfx/misc/snow
{
	nopicmip
	cull none
	{
		map sprites/spot.tga
		blendfunc gl_src_alpha gl_one_minus_src_alpha
		rgbgen vertex
		alphagen vertex
	}
}
