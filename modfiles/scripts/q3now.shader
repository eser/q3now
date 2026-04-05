// q3now custom shaders

// ── loading screen radial glow ──────────────────────────────────────
// White-to-transparent radial falloff texture (alpha encodes falloff).
// GL_SRC_ALPHA GL_ONE = alpha-modulated additive blend:
//   result = src_color * src_alpha + dst_color
// The alpha channel provides the radial shape, additive ensures the
// glow only brightens the background, never darkens it.
gfx/loading/glow_radial
{
	sort additive
	nopicmip
	nomipmaps
	{
		map gfx/loading/glow_radial.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
		alphaGen vertex
	}
}

// ── UI radial glow ────────────────────────────────────────────────────
// Circular gradient from white center to transparent edges.
// Alpha-modulated additive blend: brightens the background, never darkens.
gfx/ui/radial_glow
{
	nopicmip
	nomipmaps
	{
		map gfx/ui/radial_glow.png
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
		alphaGen vertex
	}
}

// ── crosshair shaders (fix: rgbGen exactVertex enables SetColor tinting) ──
// Q3 vanilla uses rgbGen identity which ignores SetColor → crosshair always white.
// QL fixed this with rgbGen exactVertex. We override all 10 crosshairs here.
gfx/2d/crosshairMelee   { nopicmip { map gfx/2d/crosshairMelee.tga    blendfunc blend  rgbGen exactVertex } }
gfx/2d/crosshairBullet  { nopicmip { map gfx/2d/crosshairBullet.tga   blendfunc blend  rgbGen exactVertex } }
gfx/2d/crosshairBurst   { nopicmip { map gfx/2d/crosshairBurst.tga    blendfunc blend  rgbGen exactVertex } }
gfx/2d/crosshairMissile { nopicmip { map gfx/2d/crosshairMissile.tga  blendfunc blend  rgbGen exactVertex } }
gfx/2d/crosshairMisc    { nopicmip { map gfx/2d/crosshairMisc.tga     blendfunc blend  rgbGen exactVertex } }

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

// ── q3now rail trail shaders ──────────────────────────────────────
// Q2-spirit modernized rail trail: helix ribbon + debris particles.
// Uses additive blending for glow, tcMod scroll for energy flow.

q3now/railHelix
{
	cull none
	{
		map sprites/bfglfline.tga
		blendfunc gl_src_alpha gl_one
		rgbgen vertex
		alphagen vertex
		tcMod scroll 2.0 0
	}
	{
		map sprites/bfglfline.tga
		blendfunc gl_one gl_one
		rgbgen vertex
		alphagen vertex
		tcMod scroll -1.5 0
	}
}

q3now/railDebris
{
	cull none
	{
		map sprites/bfglfglare.tga
		blendfunc gl_one gl_one
		rgbgen vertex
		alphagen vertex
	}
	deformVertexes autosprite
}

// ── Wired UI menu background clouds ───────────────────────────────────
// Two-layer scrolling clouds with dark blue tint. Used as fullscreen
// background behind floating menu panels.

// Available sky textures:
//   bluedimclouds.jpg, dimclouds.jpg, inteldimclouds.jpg,
//   intelredclouds.jpg, killsky_1.jpg, killsky_2.jpg,
//   pjbasesky.jpg, topclouds.jpg

wiredui/clouds
{
	nopicmip
	nomipmaps
	{
		// base — dark blue fill
		map $whiteimage
		rgbgen const ( 0.06 0.06 0.12 )
	}
	{
		// layer 1 — blue clouds, slow drift right
		map textures/skies/bluedimclouds.jpg
		blendfunc gl_one gl_one
		rgbgen const ( 0.35 0.35 0.55 )
		tcmod scale 1.5 1.5
		tcmod scroll 0.01 0.003
	}
	{
		// layer 2 — warm clouds, counter-drift left, parallax depth
		map textures/skies/dimclouds.jpg
		blendfunc gl_one gl_one
		rgbgen const ( 0.40 0.28 0.45 )
		tcmod scale 2.5 2.0
		tcmod scroll -0.02 0.006
	}
	{
		// layer 3 — high wisps, diagonal drift
		map textures/skies/topclouds.jpg
		blendfunc gl_one gl_one
		rgbgen const ( 0.20 0.20 0.30 )
		tcmod scale 4 3
		tcmod scroll 0.005 -0.012
	}
}

models/powerups/health/red
{	
	
	{
		map textures/effects/envmapred.tga
        tcGen environment
	}
}

models/powerups/health/red_sphere
{
	{
		map textures/effects/tinfx2b.tga
		tcGen environment
		blendfunc GL_ONE GL_ONE
	}
}

// ── Lightning Gun Chain Arc beam shader ─────────────────────────────
// Visually distinct from primary: bluer tint, more flicker, thinner appearance.
// Uses existing gfx/misc/lightning1.tga — no new texture needed.
// Dual-pass additive with offset scroll creates crackling interference pattern.
lightningArc
{
	cull none
	{
		map gfx/misc/lightning1.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave inverseSawtooth 0 1 0 10
		tcMod scroll 2.0 0
		tcMod scale 1.5 1
	}
	{
		map gfx/misc/lightning1.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave sawtooth 0.3 0.7 0 7
		tcMod scroll -3.0 0
		tcMod scale 1.0 1
	}
}
