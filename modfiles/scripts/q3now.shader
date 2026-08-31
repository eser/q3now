// q3now custom shaders

// ── loading screen radial glow ──────────────────────────────────────
// White-to-transparent radial falloff texture (alpha encodes falloff).
// GL_SRC_ALPHA GL_ONE = alpha-modulated additive blend:
//   result = src_color * src_alpha + dst_color
// The alpha channel provides the radial shape, additive ensures the
// glow only brightens the background, never darkens it.
gfx/ui/glow_radial
{
	sort additive
	nopicmip
	nomipmaps
	{
		map gfx/ui/glow_radial.png
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
		alphaGen vertex
	}
}

// Compact projectile-bound hot core used by the layered rocket trail. It
// deliberately reuses the shipped soft radial image while giving the particle
// registry a world-effect material name and an explicit additive contract.
rocketExhaustGlow
{
	sort additive
	nopicmip
	{
		clampmap gfx/ui/glow_radial.png
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
		alphaGen vertex
	}
}

// Layered rocket-explosion materials. These deliberately separate the flash,
// irregular fire cards and pressure ring so the effect does not become a pile
// of identical radial glows. rlboom_4 is used as one static, unanimated card;
// the ROCKET_FX_EXPLOSION path never advances the legacy flipbook sequence.
rocketExplosionFlash
{
	sort additive
	nopicmip
	{
		clampmap gfx/misc/flare_modern.png
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
		alphaGen vertex
	}
}

rocketExplosionFireCard
{
	sort additive
	nopicmip
	{
		clampmap models/weaphits/rlboom/rlboom_4.png
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
		alphaGen vertex
	}
}

rocketExplosionRing
{
	sort additive
	nopicmip
	{
		clampmap sprites/bfglfring.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
		alphaGen vertex
	}
}

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

// Bright Arena Lens Flares
bfgLFStar
{
	sort nearest
	{
		map sprites/bfglfstar.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
	}
}

bfgLFGlare
{
	sort nearest
	{
		map sprites/bfglfglare.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
	}
}

bfgLFDisc
{
	sort nearest
	{
		map sprites/bfglfdisc.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
	}
}

bfgLFRing
{
	sort nearest
	{
		map sprites/bfglfring.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
	}
}

bfgLFLine
{
	nomipmaps
	nopicmip
	sort nearest
	{
		clampmap sprites/bfglfline.tga
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
		tcmod transform 1 0 0 64 0 -31.5
	}
}

// Modern surface/dlight flare sprite (the tr.flareShader target). The 256x256
// RGBA sprite carries all structure in its alpha channel: a defined hot core, an
// inverse-square halo falloff, and a subtle horizontal anamorphic streak. The
// alpha-modulated additive blend shapes a crisp flare while rgbGen vertex tints
// it with the light's colour; replaces the classic 64x64 mushy round gradient.
flareModern
{
	sort nearest
	nopicmip
	{
		map gfx/misc/flare_modern.png
		blendfunc gl_src_alpha gl_one
		alphagen vertex
		rgbgen vertex
	}
}

// ── end q3now lens flare shaders ────────────────────────────────────

powerups/spawnProtect
{
	deformVertexes wave 100 sin 0.5 0.5 0 0.5
	{
		map textures/effects/envmap.jpg
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

// Required pax01 sky textures:
//   inteldimclouds.jpg, intelredclouds.jpg, killsky_1.jpg, killsky_2.jpg

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
		map textures/skies/inteldimclouds.jpg
		blendfunc gl_one gl_one
		rgbgen const ( 0.35 0.35 0.55 )
		tcmod scale 1.5 1.5
		tcmod scroll 0.01 0.003
	}
	{
		// layer 2 — warm clouds, counter-drift left, parallax depth
		map textures/skies/intelredclouds.jpg
		blendfunc gl_one gl_one
		rgbgen const ( 0.40 0.28 0.45 )
		tcmod scale 2.5 2.0
		tcmod scroll -0.02 0.006
	}
	{
		// layer 3 — high wisps, diagonal drift
		map textures/skies/killsky_1.jpg
		blendfunc gl_one gl_one
		rgbgen const ( 0.20 0.20 0.30 )
		tcmod scale 4 3
		tcmod scroll 0.005 -0.012
	}
}

models/powerups/health/red
{

	{
		map textures/effects/envmapred.jpg
        tcGen environment
	}
}

models/powerups/health/red_sphere
{
	{
		map textures/effects/tinfx2b.png
		tcGen environment
		blendfunc GL_ONE GL_ONE
	}
}

// ── Lightning Gun Chain Arc beam shader ─────────────────────────────
// Visually distinct from primary: bluer tint, more flicker, thinner appearance.
// Uses existing gfx/misc/lightning1.jpg — no new texture needed.
// Dual-pass additive with offset scroll creates crackling interference pattern.
lightningArc
{
	cull none
	{
		map gfx/misc/lightning1.jpg
		blendFunc GL_ONE GL_ONE
		rgbGen wave inverseSawtooth 0 1 0 10
		tcMod scroll 2.0 0
		tcMod scale 1.5 1
	}
	{
		map gfx/misc/lightning1.jpg
		blendFunc GL_ONE GL_ONE
		rgbGen wave sawtooth 0.3 0.7 0 7
		tcMod scroll -3.0 0
		tcMod scale 1.0 1
	}
}

lightningBolt
{
	cull none
	{
		map gfx/misc/lightningbolt.jpg
		blendfunc add
		tcMod scale 1.5 1
		tcMod scroll -1.8 0
	}
	{
		map gfx/misc/lightningbolt.jpg
		blendfunc add
		tcMod scale -1.5 -1
		tcMod scroll -4 0
	}
}


lightningBoltNPM
{
	nopicmip
	cull none
	{
		map gfx/misc/lightningbolt.jpg
		blendfunc add
		tcMod scale 1.5 1
		tcMod scroll -1.8 0
	}
	{
		map gfx/misc/lightningbolt.jpg
		blendfunc add
		tcMod scale -1.5 -1
		tcMod scroll -4 0
	}
}

// Push-displacement trail beam (Phase 5R; jumppads are the first
// consumer, but the effect generalizes to any "applied velocity"
// hook — knockback, launchers, Q-style movement assists). Subtle
// invismap-based shimmer, two-stage cross-scroll for "ghost trail
// invisible-until-motion" aesthetic. The cgame-side render
// enforces an alpha ceiling (≤0.25) so this stays a barely-there
// distortion rather than a bright energy beam.
//
// rgbGen/alphaGen vertex pulls per-vertex tint and fade alpha from
// the cgame-supplied bd.startColor / bd.endColor.
pushTrail
{
	cull none
	{
		map textures/effects/invismap.png
		blendfunc add
		rgbGen vertex
		alphaGen vertex
		tcMod scale 1 1
		tcMod scroll -1.5 0.3
	}
	{
		map textures/effects/invismap.png
		blendfunc add
		rgbGen vertex
		alphaGen vertex
		tcMod scale -1 -1
		tcMod scroll -2.0 -0.4
	}
}

// Additive grayscale particle, rgbGen vertex
// so per-entity shaderRGBA controls the tint.
gfx/misc/particle
{
	nopicmip
	cull none
	{
		map gfx/misc/particle.png
		blendfunc gl_one gl_one
		rgbGen vertex
	}
}

// ── Glass Cloaking ──────────────────────────────────────────────────
powerups/glassCloaking
{
	sort portal
	{
		map $whiteimage
		blendfunc gl_zero gl_one
		depthwrite
	}
}

powerups/glassCloakingSpecular
{
	{
		map textures/effects/invismap.png
		blendfunc gl_src_alpha gl_one
		alphagen entity
		tcmod turb 0 0.15 0 0.25
		tcgen environment
	}
}

// ── Q1 teleporter pad liquid surface ────────────────────────────────
// Substitutes textures/sfx/portal_sfx.tga (Q3 pak) — uses local
// gfx/misc/teleportEffect2.jpg so the shader works without pak lookup.
textures/liquids/teleporter_q1
{
	qer_editorimage gfx/misc/teleportEffect2.jpg
	q3map_globaltexture
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm noimpact
	surfaceparm nolightmap
	q3map_surfacelight 400
	cull disable

	deformVertexes wave 64 sin 0.5 0.5 0 0.5

	{
		map gfx/misc/teleportEffect2.jpg
		blendFunc GL_ONE GL_ONE
		tcMod scale 1 1
		tcMod scroll 0.05 0.05
		rgbGen identity
	}
	{
		map gfx/misc/teleportEffect2.jpg
		blendFunc GL_ONE GL_ONE
		tcMod turb 0 0.2 0 0.1
		tcMod scroll -0.03 0.02
		rgbGen wave sin 0.5 0.5 0 0.3
	}
}

// ── Q1 water surface ─────────────────────────────────────────────────
textures/liquids/clear_calm1
{
	qer_editorimage textures/liquids/pool3d_3e.jpg
	surfaceparm nolightmap
	surfaceparm water
	cull disable
	{
		map textures/liquids/pool3d_3e.jpg
		tcMod turb 0 0.15 0 0.08
		tcMod scroll 0.03 0.02
		rgbGen identity
	}
}

// ── Q1 slime surface ─────────────────────────────────────────────────
textures/liquids/slime2
{
	qer_editorimage textures/liquids/slime7e.jpg
	surfaceparm nolightmap
	surfaceparm slime
	cull disable
	{
		map textures/liquids/slime7e.jpg
		tcMod turb 0 0.1 0 0.05
		tcMod scroll 0.015 0.01
		rgbGen identity
	}
}

// ── Q1 lava surface ──────────────────────────────────────────────────
// No dedicated lava texture in tree; pool3d_5e with warm tint stands in.
textures/liquids/lavacrust
{
	qer_editorimage textures/liquids/pool3d_5e.jpg
	surfaceparm nolightmap
	surfaceparm lava
	q3map_surfacelight 800
	cull disable
	{
		map textures/liquids/pool3d_5e.jpg
		tcMod turb 0 0.2 0 0.05
		tcMod scroll 0.01 0.008
		rgbGen wave sin 0.9 0.1 0 0.4
	}
}

// ── Armors ───────────────────────────────────────────────────────────
models/powerups/armor/newgreen
{
    {
		map textures/sfx/specular.png
		tcGen environment
		rgbGen identity
	}
	{
		map models/powerups/armor/newgreen.png
		blendFunc blend
		rgbGen entity
	}
}

models/powerups/armor/energy_gre1
{
    cull none
    nopicmip
    nomipmaps

	{
		map models/powerups/armor/energy_base1.jpg
		blendFunc add
		rgbGen entity
		tcMod scroll 7.4 1.3
	}
}

models/powerups/armor/newbase
{
    {
		map textures/sfx/specular.png
		tcGen environment
		rgbGen identity
	}
	{
		map models/powerups/armor/newbase.png
		blendFunc blend
		rgbGen entity
	}
}

models/powerups/armor/energy_base1
{
    cull none
    nopicmip
    nomipmaps

	{
		map models/powerups/armor/energy_base1.jpg
		blendFunc add
		rgbGen entity
		tcMod scroll 7.4 1.3
	}
}

models/powerups/health/green
{
	{
		map textures/effects/envmapgreen.jpg
		tcGen environment
	}
}

models/powerups/armor/energy_grn1
{
	deformVertexes wave 100 sin 2 0 0 0
	{
		map textures/effects/envmapgreen.jpg
		blendFunc GL_ONE GL_ONE
		tcGen environment
		tcmod rotate 30
		tcMod scroll 1 1
		rgbGen wave triangle -.3 1.3 0 .3
	}
}

models/weapons2/grenadel/grenadel
{
	{
		map models/weapons2/grenadel/grenadel.tga
		rgbGen lightingDiffuse
	}
	{
		map models/weapons2/grenadel/grenadel.tga
		blendfunc gl_dst_color gl_dst_alpha
		rgbGen lightingDiffuse
		alphaGen lightingSpecular
	}
}

// PBR test material — ORM-packed (R=AO, G=roughness ramp, B=metalness: left half
// metallic, right half dielectric). Parallax-free on purpose: a pbrMap surface that
// also carries a normalMap gets its PBR pipeline overwritten by the parallax swap,
// so this stays normalMap-free. Renders via Cook-Torrance GGX under r_pbr 1 when the
// surface enters the dynamic-light (PMLIGHT) pass; legacy Blinn-Phong under r_pbr 0.
textures/pbr_test/pbr_test
{
	{
		map textures/pbr_test/albedo.png
		rgbGen lightingDiffuse
		pbrMap textures/pbr_test/orm.png
	}
}
