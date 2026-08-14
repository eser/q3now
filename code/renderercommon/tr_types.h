// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
#ifndef __TR_TYPES_H
#define __TR_TYPES_H

#define MAX_VIDEO_HANDLES	16

#define	MAX_DLIGHTS			32			// can't be increased, because bit flags are used on surfaces

// renderfx flags
#define	RF_MINLIGHT			0x0001		// always have some light (viewmodel, some items)
#define	RF_THIRD_PERSON		0x0002		// don't draw through eyes, only mirrors (player bodies, chat sprites)
#define	RF_FIRST_PERSON		0x0004		// only draw through eyes (view weapon, damage blood blob)
#define	RF_DEPTHHACK		0x0008		// for view weapon Z crunching

#define RF_CROSSHAIR		0x0010		// This item is a cross hair and will draw over everything similar to
										// DEPTHHACK in stereo rendering mode, with the difference that the
										// projection matrix won't be hacked to reduce the stereo separation as
										// is done for the gun.

#define RF_FORCE_ENT_ALPHA	0x0020		// force entity shaderRGBA[3] as alpha, enable blending

#define	RF_NOSHADOW			0x0040		// don't add stencil shadows

#define RF_LIGHTING_ORIGIN	0x0080		// use refEntity->lightingOrigin instead of refEntity->origin
										// for lighting.  This allows entities to sink into the floor
										// with their origin going solid, and allows all parts of a
										// player to get the same lighting

// 0x0100 retired (was RF_SHADOW_PLANE — legacy projection-shadow plane, removed
// with refEntity_t.shadowPlane). The bit is left unused so sibling RF_ values
// keep their numbers and the renderfx ABI bit layout stays stable.
#define	RF_WRAP_FRAMES		0x0200		// mod the model frames by the maxframes to allow continuous
										// animation without needing to know the frame count

// ── Character skin dispatch (shared between engine, cgame, and renderers) ──
// Handles are resolved at character-load time; renderer looks up via ri.GetCharacterSkin.
// singlePath=1: fallbackShader applied to every surface.
// singlePath=0: overrides[] matched by surface name; defaultShader used for unlisted surfaces.

#define CM_SKIN_NAME_LEN          32
#define CM_SURFACE_NAME_LEN       32
#define CM_MAX_SURFACE_OVERRIDES   8

// Case-insensitive FNV-1a hash for surface names. Used as a fast-reject
// signature so per-frame skin override / skin surface lookups in tr_mesh.c
// can skip the strcmp on hash mismatch.
static ID_INLINE unsigned int Q_HashSurfaceName( const char *s ) {
	unsigned int h = 2166136261u; // FNV-1a offset basis
	while ( *s ) {
		unsigned char c = (unsigned char)*s++;
		if ( c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
		h ^= c;
		h *= 16777619u;
	}
	return h;
}

typedef struct {
	char         surfaceName[CM_SURFACE_NAME_LEN]; // lowercase-normalized at registration
	unsigned int surfaceNameHash;                  // Q_HashSurfaceName( surfaceName )
	qhandle_t    shader;
} cmSkinOverride_t;

typedef struct {
	char              name[CM_SKIN_NAME_LEN];
	int               paintable;
	int               singlePath;
	qhandle_t         fallbackShader;   // singlePath=1: applied to every surface
	qhandle_t         defaultShader;    // singlePath=0: fallback for unlisted surfaces
	int               overrideCount;
	cmSkinOverride_t  overrides[CM_MAX_SURFACE_OVERRIDES];
} cmSkin_t;

// refdef flags
#define RDF_NOWORLDMODEL	0x0001		// used for player configuration screen
#define RDF_HYPERSPACE		0x0004		// teleportation effect
#define RDF_TEMPORAL_PRIMARY	0x0040		// authoritative full-window gameplay scene

typedef struct {
	vec3_t		xyz;
	float		st[2];
	color4ub_t	modulate;
} polyVert_t;

typedef struct poly_s {
	qhandle_t			hShader;
	int					numVerts;
	polyVert_t			*verts;
} poly_t;

typedef enum {
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_BEAM,
	RT_LIGHTNING,			// DEPRECATED: use trap_R_AddBeamToScene. Submission produces no draw.
	RT_PORTALSURFACE,		// doesn't draw anything, just info for portals
	RT_POLYSTRIP,			// continuous tri-strip (pairs of verts, proper strip indexing)

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

typedef struct {
	refEntityType_t	reType;
	int			renderfx;

	qhandle_t	hModel;				// opaque type outside refresh

	// most recent data
	vec3_t		lightingOrigin;		// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)

	vec3_t		axis[3];			// rotation vectors
	qboolean	nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	float		origin[3];			// also used as MODEL_BEAM's "from"
	int			frame;				// also used as MODEL_BEAM's diameter

	// previous data for frame interpolation
	float		oldorigin[3];		// also used as MODEL_BEAM's "to"
	int			oldframe;
	float		backlerp;			// 0.0 = current, 1.0 = old

	// texturing
	int			skinNum;			// inline skin index
	qhandle_t	customSkin;			// NULL for default skin
	qhandle_t	customShader;		// use one image for the entire thing (powerup overlays, etc.)
	qhandle_t	characterSkin;		// character skin handle; 0 = not a character entity

	// misc
	color4ub_t	shader;
	float		shaderTexCoord[2];	// texture coordinates used by tcMod entity modifiers

	// subtracted from refdef time to control effect start times
	floatint_t	shaderTime;			// -EC- set to union

	// extra sprite information
	float		radius;
	float		rotation;
} refEntity_t;

// Optional cross-frame identity submitted atomically with one refEntity_t.
// Fixed-width, pointer-free and intentionally separate from refEntity_t so old
// modules retain their exact ABI and ordinary submissions remain unchanged.
#define REF_ENTITY_MOTION_VERSION 1u
#define REF_ENTITY_MOTION_VALID_FLAGS 0u
// Append-only semantic sub-entity key.  Values are shared by producer and the
// future renderer history cache; never renumber or reuse an existing value.
typedef enum {
	REF_ENTITY_MOTION_ROLE_NONE = 0,
	REF_ENTITY_MOTION_ROLE_GENERAL = 1,
	REF_ENTITY_MOTION_ROLE_CREATURE_BODY = 2,
	REF_ENTITY_MOTION_ROLE_PLAYER_BODY = 3,
	REF_ENTITY_MOTION_ROLE_PLAYER_LEGS = 4,
	REF_ENTITY_MOTION_ROLE_PLAYER_TORSO = 5,
	REF_ENTITY_MOTION_ROLE_PLAYER_HEAD = 6,
	REF_ENTITY_MOTION_ROLE_ITEM_PRIMARY = 7,
	REF_ENTITY_MOTION_ROLE_ITEM_BARREL = 8,
	REF_ENTITY_MOTION_ROLE_ITEM_SECONDARY = 9,
	REF_ENTITY_MOTION_ROLE_MOVER_PRIMARY = 10,
	REF_ENTITY_MOTION_ROLE_MOVER_SECONDARY = 11,
	REF_ENTITY_MOTION_ROLE_GRAPPLE = 12,
	REF_ENTITY_MOTION_ROLE_COUNT
} refEntityMotionRole_t;
typedef struct {
	uint32_t structSize;
	uint32_t version;
	uint32_t ownerId;
	uint32_t generation;
	uint32_t role;
	uint32_t flags;
} refEntityMotion_t;

static ID_INLINE qboolean RefEntityMotion_IsValid( const refEntityMotion_t *motion ) {
	return motion && motion->structSize == sizeof( refEntityMotion_t )
		&& motion->version == REF_ENTITY_MOTION_VERSION
		&& ( motion->flags & ~REF_ENTITY_MOTION_VALID_FLAGS ) == 0u;
}

static ID_INLINE void RefEntityMotion_ClearOwned( refEntityMotion_t *owned,
		qboolean *hasTemporal ) {
	if ( owned ) memset( owned, 0, sizeof( *owned ) );
	if ( hasTemporal ) *hasTemporal = qfalse;
}

static ID_INLINE qboolean RefEntityMotion_CopyOwned( refEntityMotion_t *owned,
		qboolean *hasTemporal, const refEntityMotion_t *motion ) {
	if ( !owned || !hasTemporal || !RefEntityMotion_IsValid( motion ) ) return qfalse;
	*owned = *motion;
	*hasTemporal = qtrue;
	return qtrue;
}

static ID_INLINE qboolean RefEntityMotion_CanAppend( const refEntity_t *entity,
		const refEntityMotion_t *motion, uint32_t count, uint32_t capacity ) {
	return entity && count < capacity && RefEntityMotion_IsValid( motion );
}


#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

	// time in milliseconds for shader effects and other time dependent rendering issues
	int			time;

	int			rdflags;			// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte		areamask[MAX_MAP_AREA_BYTES];

	// text messages for deform text shaders
	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
} refdef_t;


typedef enum {
	STEREO_CENTER,
	STEREO_LEFT,
	STEREO_RIGHT
} stereoFrame_t;


/*
** glconfig_t
**
** Contains variables specific to the OpenGL configuration
** being run right now.  These are constant once the OpenGL
** subsystem is initialized.
*/
typedef enum {
	TC_NONE,
	TC_S3TC,  // this is for the GL_S3_s3tc extension.
	TC_S3TC_ARB  // this is for the GL_EXT_texture_compression_s3tc extension.
} textureCompression_t;

typedef enum {
	GLDRV_ICD,					// driver is integrated with window system
								// WARNING: there are tests that check for
								// > GLDRV_ICD for minidriverness, so this
								// should always be the lowest value in this
								// enum set
	GLDRV_STANDALONE,			// driver is a non-3Dfx standalone driver
	GLDRV_VOODOO				// driver is a 3Dfx standalone driver
} glDriverType_t;

typedef enum {
	GLHW_GENERIC,			// where everything works the way it should
	GLHW_3DFX_2D3D,			// Voodoo Banshee or Voodoo3, relevant since if this is
							// the hardware type then there can NOT exist a secondary
							// display adapter
	GLHW_RIVA128,			// where you can't interpolate alpha
	GLHW_RAGEPRO,			// where you can't modulate alpha on alpha textures
	GLHW_PERMEDIA2			// where you don't have src*dst
} glHardwareType_t;

typedef struct {
	char					renderer_string[MAX_STRING_CHARS];
	char					vendor_string[MAX_STRING_CHARS];
	char					version_string[MAX_STRING_CHARS];
	char					extensions_string[BIG_INFO_STRING];

	int						maxTextureSize;			// queried from GL
	int						numTextureUnits;		// multitexture ability

	int						colorBits, depthBits, stencilBits;

	glDriverType_t			driverType;
	glHardwareType_t		hardwareType;

	qboolean				deviceSupportsGamma;
	textureCompression_t	textureCompression;
	qboolean				textureEnvAddAvailable;

	int						vidWidth, vidHeight;
	// aspect is the screen's physical width / height, which may be different
	// than scrWidth / scrHeight if the pixels are non-square
	// normal screens should be 4/3, but wide aspect monitors may be 16/9
	float					windowAspect;

	int						displayFrequency;

	// synonymous with "does rendering consume the entire screen?", therefore
	// a Voodoo or Voodoo2 will have this set to TRUE, as will a Win32 ICD that
	// used CDS.
	qboolean				isFullscreen;
	qboolean				stereoEnabled;
	qboolean				smpActive;		// UNUSED, present for compatibility

	// Logical (DPI-independent) window size in points. On a HiDPI display the
	// OS reports a larger physical pixel size (vidWidth/vidHeight, filled from
	// SDL_GetWindowSizeInPixels) than the logical point size the window was
	// created with (SDL_GetWindowSize). dpiScale = vidWidth / vidWidthLogical
	// (>=1.0; 1.0 on a non-HiDPI display). The platform layer (code/sdl) fills
	// these next to vidWidth/vidHeight; the WiredUI compositor reads them to
	// scale font sizes. Appended at the end of the struct so the VM-facing
	// glconfig ABI for the existing fields above stays byte-stable. Zero when
	// the platform layer has not published them — consumers fall back to the
	// physical size (dpiScale 1.0).
	int						vidWidthLogical, vidHeightLogical;
} glconfig_t;

#define	myftol(x) ((int)(x))

#if defined(_WIN32)
#define OPENGL_DRIVER_NAME	"opengl32"
#elif defined(MACOS_X)
#define OPENGL_DRIVER_NAME	"/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#else
#define OPENGL_DRIVER_NAME	"libGL.so.1"
#endif

// ── IQM animation query (renderer -> cgame) ─────────────────────────
#include "../qcommon/q_feats.h"
#if FEAT_IQM
#define MAX_IQM_ANIMS	64
typedef struct {
	char		name[MAX_QPATH];	// animation name from IQM file
	int		first_frame;		// first frame index
	int		num_frames;		// number of frames
	float		framerate;		// frames per second
	int		flags;			// IQM_LOOP etc.
} iqmAnimInfo_t;
#endif // FEAT_IQM

// ── MDL (Q1) animation-range query (renderer -> cgame) ──────────────
// A Q1 .mdl has no explicit animation table — animations are implied by frame
// NAMES ("stand1".."stand8", "walk1".."walk8", ...). The MDL loader carries the
// per-frame name into md3Frame_t.name (tr_model_mdl.c), and the renderer derives
// contiguous same-label ranges by prefix-grouping (strip trailing digits). The
// query returns one entry per derived range, keyed by the raw Q1 label; cgame
// maps the label -> a monster anim code (the alias table, mirroring the IQM
// name->code path CG_ParseIQMAnimations uses). Always-available (not FEAT_IQM).
#define MAX_MDL_ANIMS	32
typedef struct {
	char	label[16];		// the Q1 frame-name prefix, digits stripped (e.g. "walk", "painb")
	int		first_frame;	// first MD3 frame index of the range
	int		num_frames;		// contiguous frame count in the range
} mdlAnimRange_t;

#endif	// __TR_TYPES_H
