/*
===========================================================================
Copyright (C) 2026 q3now contributors.

This file is part of q3now and is distributed under the terms of the
GNU General Public License version 2 or (at your option) any later version.

sv_ref.c — Dedicated server headless renderer (FEAT_HEADLESS_RENDERER)

Provides a refexport_t implementation for dedicated-server builds that
never open a window. 2D drawing, scene submission, and GPU-backed calls
become inert sinks because a dedicated server has no display surface.
The functions that server-side code (bot AI, physics, game VM) genuinely
needs — LoadWorld, RegisterModel, ModelBounds, LerpTag — do real work:

  * LoadWorld   : forwards to CM_LoadMap so collision data is available
                  for traces and movement queries.
  * RegisterModel: parses MD3 / IQM headers out of the file system and
                  caches bounds + tag tables in a server-local model
                  table. No GPU upload, no shader lookup, no images.
  * ModelBounds : returns the cached frame[0] AABB from the registered
                  model.
  * LerpTag     : interpolates MD3 tags between frames (matching the
                  renderer-side R_LerpTag math) so GVM code that walks
                  model attachments returns valid orientations.

q3now's dedicated build omits the client library and all renderer DLLs.
This file is compiled into the dedicated binary so server code that
calls refexport_t entry points gets a deterministic, correct response
instead of a crash or a silent dead-end.

Activation:
  * FEAT_HEADLESS_RENDERER enables this implementation.
  * Integration point: a dedicated build can call GetRefAPI_Headless()
    and populate its shared `re` export with the returned vtable.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#if FEAT_HEADLESS_RENDERER

#include "../renderercommon/tr_public.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/cm_public.h"

/* ---------- Server-side model table ---------- */

typedef enum {
	SVR_MOD_BAD,
	SVR_MOD_MD3,
	SVR_MOD_IQM
} svrModelType_t;

typedef struct svrModel_s {
	char			name[MAX_QPATH];
	svrModelType_t	type;

	/* Frame[0] bounding box for quick ModelBounds responses. */
	vec3_t			mins;
	vec3_t			maxs;

	/* Raw data used by LerpTag. The backing buffer is owned by this
	 * table and lives until the server shuts down or the slot is
	 * overwritten (we never GC mid-match — MAX_MODELS = 256). */
	void			*data;
	int				dataSize;

	/* Cached header pointers so LerpTag avoids re-parsing per call. */
	int				numFrames;
	int				numTags;
	const byte		*tagBase;		/* pointer into data for MD3 tag array */
} svrModel_t;

static svrModel_t	svrModels[MAX_MODELS];
static int			svrNumModels;

/*
====================
SVR_ClearModels

Drop every cached model and free its backing buffer. Called during
shutdown so we do not leak across server restarts.
====================
*/
static void SVR_ClearModels( void ) {
	int i;
	for ( i = 0; i < svrNumModels; i++ ) {
		if ( svrModels[i].data ) {
			Z_Free( svrModels[i].data );
			svrModels[i].data = NULL;
		}
	}
	Com_Memset( svrModels, 0, sizeof( svrModels ) );
	svrNumModels = 1; /* handle 0 reserved as "bad" */
}

/*
====================
SVR_FindModel

Linear lookup by qualified file name. Returns NULL when the model has
not been registered yet.
====================
*/
static svrModel_t *SVR_FindModel( const char *name ) {
	int i;
	for ( i = 1; i < svrNumModels; i++ ) {
		if ( !Q_stricmp( svrModels[i].name, name ) ) {
			return &svrModels[i];
		}
	}
	return NULL;
}

/*
====================
SVR_AllocModel

Reserve a fresh slot in the model table. Returns NULL when the table
is full.
====================
*/
static svrModel_t *SVR_AllocModel( const char *name, qhandle_t *outHandle ) {
	svrModel_t *mod;

	if ( svrNumModels >= MAX_MODELS ) {
		return NULL;
	}
	if ( svrNumModels == 0 ) {
		svrNumModels = 1; /* reserve handle 0 */
	}

	mod = &svrModels[svrNumModels];
	Com_Memset( mod, 0, sizeof( *mod ) );
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );
	*outHandle = svrNumModels;
	svrNumModels++;
	return mod;
}


/* ---------- MD3 loader ---------- */

#define LL(x) x=LittleLong(x)

/*
====================
SVR_LoadMD3

Parse an MD3 file into a server model slot. Copies the full buffer so
LerpTag can walk the tag array later without re-reading from disk.
====================
*/
static qboolean SVR_LoadMD3( svrModel_t *mod, const byte *buffer, int fileSize, const char *name ) {
	const md3Header_t	*pin;
	md3Header_t			*hdr;
	md3Frame_t			*frame;
	md3Tag_t			*tag;
	int					i, j;
	uint32_t			version;
	uint32_t			size;

	if ( fileSize < (int)sizeof( md3Header_t ) ) {
		Com_DPrintf( "SVR_LoadMD3: %s truncated header\n", name );
		return qfalse;
	}

	pin = (const md3Header_t *)buffer;
	version = LittleLong( pin->version );
	if ( version != MD3_VERSION ) {
		Com_DPrintf( "SVR_LoadMD3: %s wrong version (%u should be %i)\n",
			name, version, MD3_VERSION );
		return qfalse;
	}

	size = LittleLong( pin->ofsEnd );
	if ( size == 0 || size > (uint32_t)fileSize ) {
		Com_DPrintf( "SVR_LoadMD3: %s corrupted header\n", name );
		return qfalse;
	}

	/* Own a copy of the buffer so we can byteswap in place and keep the
	 * tag array around for LerpTag. Z_Malloc is the right allocator
	 * because this persists past the current frame but should be freed
	 * on server shutdown. */
	hdr = Z_Malloc( size );
	Com_Memcpy( hdr, buffer, size );

	LL( hdr->ident );
	LL( hdr->version );
	LL( hdr->numFrames );
	LL( hdr->numTags );
	LL( hdr->numSurfaces );
	LL( hdr->numSkins );
	LL( hdr->ofsFrames );
	LL( hdr->ofsTags );
	LL( hdr->ofsSurfaces );
	LL( hdr->ofsEnd );

	if ( hdr->numFrames < 1 ) {
		Com_DPrintf( "SVR_LoadMD3: %s has no frames\n", name );
		Z_Free( hdr );
		return qfalse;
	}

	if ( hdr->ofsFrames > size || hdr->ofsTags > size ) {
		Com_DPrintf( "SVR_LoadMD3: %s corrupted offsets\n", name );
		Z_Free( hdr );
		return qfalse;
	}

	if ( hdr->numFrames > (int)((size - hdr->ofsFrames) / sizeof( md3Frame_t )) ) {
		Com_DPrintf( "SVR_LoadMD3: %s corrupted frame count\n", name );
		Z_Free( hdr );
		return qfalse;
	}
	if ( hdr->numTags > 0 && hdr->numFrames > 0 ) {
		if ( (size_t)hdr->numTags * (size_t)hdr->numFrames > (size - hdr->ofsTags) / sizeof( md3Tag_t ) ) {
			Com_DPrintf( "SVR_LoadMD3: %s corrupted tag count\n", name );
			Z_Free( hdr );
			return qfalse;
		}
	}

	/* Byteswap frames and capture frame[0] bounds. */
	frame = (md3Frame_t *)( (byte *)hdr + hdr->ofsFrames );
	for ( i = 0; i < hdr->numFrames; i++, frame++ ) {
		frame->radius = LittleFloat( frame->radius );
		for ( j = 0; j < 3; j++ ) {
			frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
			frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
			frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
		}
	}

	/* Byteswap tags so LerpTag can consume them as-is. */
	tag = (md3Tag_t *)( (byte *)hdr + hdr->ofsTags );
	for ( i = 0; i < hdr->numTags * hdr->numFrames; i++, tag++ ) {
		tag->name[sizeof( tag->name ) - 1] = '\0';
		for ( j = 0; j < 3; j++ ) {
			tag->origin[j] = LittleFloat( tag->origin[j] );
			tag->axis[0][j] = LittleFloat( tag->axis[0][j] );
			tag->axis[1][j] = LittleFloat( tag->axis[1][j] );
			tag->axis[2][j] = LittleFloat( tag->axis[2][j] );
		}
	}

	frame = (md3Frame_t *)( (byte *)hdr + hdr->ofsFrames );
	VectorCopy( frame->bounds[0], mod->mins );
	VectorCopy( frame->bounds[1], mod->maxs );

	mod->type = SVR_MOD_MD3;
	mod->data = hdr;
	mod->dataSize = size;
	mod->numFrames = hdr->numFrames;
	mod->numTags = hdr->numTags;
	mod->tagBase = (const byte *)hdr + hdr->ofsTags;

	return qtrue;
}


/* ---------- IQM header parsing (bounds + joint enumeration) ---------- */

#if FEAT_IQM

#define SVR_IQM_MAGIC "INTERQUAKEMODEL"

/* Local mirror of the on-disk IQM header — enough to extract per-frame
 * bounds. We do not need mesh / vertex / pose data on the server. */
typedef struct {
	char		magic[16];
	uint32_t	version;
	uint32_t	filesize;
	uint32_t	flags;
	uint32_t	num_text, ofs_text;
	uint32_t	num_meshes, ofs_meshes;
	uint32_t	num_vertexarrays, num_vertexes, ofs_vertexarrays;
	uint32_t	num_triangles, ofs_triangles, ofs_adjacency;
	uint32_t	num_joints, ofs_joints;
	uint32_t	num_poses, ofs_poses;
	uint32_t	num_anims, ofs_anims;
	uint32_t	num_frames, num_framechannels, ofs_frames, ofs_bounds;
	uint32_t	num_comment, ofs_comment;
	uint32_t	num_extensions, ofs_extensions;
} svrIqmHeader_t;

typedef struct {
	float		bbmin[3];
	float		bbmax[3];
	float		xyradius;
	float		radius;
} svrIqmBounds_t;

/*
====================
SVR_LoadIQM

Parse an IQM file header into a server model slot. We only need the
per-frame bounding boxes; any vertex / skin / mesh data is discarded.
====================
*/
static qboolean SVR_LoadIQM( svrModel_t *mod, const byte *buffer, int fileSize, const char *name ) {
	svrIqmHeader_t		header;
	const svrIqmBounds_t *bounds;
	uint32_t			ofsBounds;

	if ( fileSize < (int)sizeof( svrIqmHeader_t ) ) {
		Com_DPrintf( "SVR_LoadIQM: %s truncated header\n", name );
		return qfalse;
	}

	Com_Memcpy( &header, buffer, sizeof( header ) );
	if ( Q_strncmp( header.magic, SVR_IQM_MAGIC, sizeof( header.magic ) ) ) {
		Com_DPrintf( "SVR_LoadIQM: %s wrong magic\n", name );
		return qfalse;
	}

	header.version = LittleLong( header.version );
	header.filesize = LittleLong( header.filesize );
	header.num_frames = LittleLong( header.num_frames );
	header.ofs_bounds = LittleLong( header.ofs_bounds );

	if ( header.filesize > (uint32_t)fileSize ) {
		Com_DPrintf( "SVR_LoadIQM: %s filesize mismatch\n", name );
		return qfalse;
	}

	/* IQM bounds are optional; fall back to zero AABB. */
	VectorClear( mod->mins );
	VectorClear( mod->maxs );

	ofsBounds = header.ofs_bounds;
	if ( ofsBounds && header.num_frames > 0
		&& ofsBounds + sizeof( svrIqmBounds_t ) <= header.filesize ) {
		svrIqmBounds_t first;
		Com_Memcpy( &first, buffer + ofsBounds, sizeof( first ) );
		first.bbmin[0] = LittleFloat( first.bbmin[0] );
		first.bbmin[1] = LittleFloat( first.bbmin[1] );
		first.bbmin[2] = LittleFloat( first.bbmin[2] );
		first.bbmax[0] = LittleFloat( first.bbmax[0] );
		first.bbmax[1] = LittleFloat( first.bbmax[1] );
		first.bbmax[2] = LittleFloat( first.bbmax[2] );
		VectorCopy( first.bbmin, mod->mins );
		VectorCopy( first.bbmax, mod->maxs );
	}

	/* We do not cache tag data for IQM — the renderer-side loader
	 * reconstructs tags from joint poses, which requires the full pose
	 * pipeline. Headless LerpTag on an IQM handle returns identity. */
	bounds = NULL;
	(void)bounds;

	mod->type = SVR_MOD_IQM;
	mod->data = NULL;
	mod->dataSize = 0;
	mod->numFrames = header.num_frames;
	mod->numTags = 0;
	mod->tagBase = NULL;
	return qtrue;
}

#endif /* FEAT_IQM */


/* ---------- MD3 tag lookup + interpolation ---------- */

/*
====================
SVR_GetTag

Walk the MD3 tag table and return the tag matching tagName in the
requested frame, or NULL if not found. Out-of-range frames clamp to
the last frame (matching R_GetTag behaviour in tr_model.c).
====================
*/
static const md3Tag_t *SVR_GetTag( const svrModel_t *mod, int frame, const char *tagName ) {
	const md3Tag_t *tag;
	int i;

	if ( !mod->tagBase || mod->numTags == 0 ) {
		return NULL;
	}
	if ( frame >= mod->numFrames ) {
		frame = mod->numFrames - 1;
	}
	if ( frame < 0 ) {
		frame = 0;
	}

	tag = (const md3Tag_t *)mod->tagBase + frame * mod->numTags;
	for ( i = 0; i < mod->numTags; i++, tag++ ) {
		if ( !strcmp( tag->name, tagName ) ) {
			return tag;
		}
	}
	return NULL;
}


/* ---------- refexport_t entry points ---------- */

static void SVR_Shutdown( refShutdownCode_t code ) {
	(void)code;
	SVR_ClearModels();
}

static void SVR_BeginRegistration( glconfig_t *config ) {
	/* A dedicated server has no display, but the engine still expects
	 * a populated glconfig so shared code that inspects vidWidth /
	 * windowAspect stays sane. */
	if ( config ) {
		Com_Memset( config, 0, sizeof( *config ) );
		config->vidWidth = 640;
		config->vidHeight = 480;
		config->windowAspect = 4.0f / 3.0f;
	}
	SVR_ClearModels();
}

/*
====================
SVR_RegisterModel

Load an MD3 or IQM file from the VFS, parse its header, and cache the
bounds and (for MD3) tag table in a server-local model slot. Returns a
handle that SVR_ModelBounds and SVR_LerpTag understand. Handle 0 is
reserved for failure so the classic refexport_t convention holds.
====================
*/
static qhandle_t SVR_RegisterModel( const char *name ) {
	svrModel_t	*mod;
	qhandle_t	handle;
	union {
		const byte	*b;
		void		*v;
	} buf;
	int			fileSize;
	uint32_t	ident;
	qboolean	loaded = qfalse;

	if ( !name || !name[0] ) {
		return 0;
	}
	if ( strlen( name ) >= MAX_QPATH ) {
		Com_DPrintf( "SVR_RegisterModel: name too long (%s)\n", name );
		return 0;
	}

	mod = SVR_FindModel( name );
	if ( mod ) {
		return (qhandle_t)( mod - svrModels );
	}

	fileSize = FS_ReadFile( name, &buf.v );
	if ( !buf.v || fileSize <= 0 ) {
		return 0;
	}

	mod = SVR_AllocModel( name, &handle );
	if ( !mod ) {
		FS_FreeFile( buf.v );
		return 0;
	}

	if ( fileSize >= (int)sizeof( uint32_t ) ) {
		ident = LittleLong( *(const uint32_t *)buf.v );
		if ( ident == MD3_IDENT ) {
			loaded = SVR_LoadMD3( mod, buf.b, fileSize, name );
		}
#if FEAT_IQM
		else {
			/* IQM identifies by ASCII magic, not a numeric ident. */
			if ( fileSize >= 16 && !Q_strncmp( (const char *)buf.v, SVR_IQM_MAGIC, 16 ) ) {
				loaded = SVR_LoadIQM( mod, buf.b, fileSize, name );
			}
		}
#endif
	}

	FS_FreeFile( buf.v );

	if ( !loaded ) {
		/* Roll back the allocation so later registrations can reuse
		 * the slot. */
		Com_Memset( mod, 0, sizeof( *mod ) );
		svrNumModels--;
		return 0;
	}

	return handle;
}

/*
====================
SVR_RegisterSkin / SVR_RegisterShader*

These would live in the client renderer. On a dedicated server there
is no texture upload path and no shader database — return 0 so callers
know the handle is inactive.
====================
*/
static qhandle_t SVR_RegisterSkin( const char *name ) { (void)name; return 0; }
static qhandle_t SVR_RegisterShader( const char *name ) { (void)name; return 0; }
static qhandle_t SVR_RegisterShaderNoMip( const char *name ) { (void)name; return 0; }
static qhandle_t SVR_RegisterMSDFShader( const char *name, float distanceRange, int atlasWidth, int atlasHeight ) {
	(void)name; (void)distanceRange; (void)atlasWidth; (void)atlasHeight;
	return 0;
}

/*
====================
SVR_LoadWorld

Forward to the collision manager so world traces, PVS queries, and bot
navigation work against the correct map data. sv_init.c also calls
CM_LoadMap directly during SV_SpawnServer — doing so here is safe
because CM_LoadMap is idempotent for identical names on the server
side and guarantees the collision database is loaded after the
refexport_t entry point returns.
====================
*/
static void SVR_LoadWorld( const bspFile_t *bsp ) {
	const char *name;
	int checksum = 0;

	if ( !bsp ) {
		return;
	}

	name = bsp->name;
	if ( !name || !name[0] ) {
		return;
	}
	CM_LoadMap( name, qfalse, &checksum );
}

static void SVR_SetWorldVisData( const byte *vis ) { (void)vis; }
static void SVR_EndRegistration( void ) {}

/* ----- Scene / draw entry points: inert on a dedicated server ----- */

static void SVR_ClearScene( void ) {}
static void SVR_AddRefEntityToScene( const refEntity_t *re, qboolean intShaderTime ) {
	(void)re; (void)intShaderTime;
}
static void SVR_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int num ) {
	(void)hShader; (void)numVerts; (void)verts; (void)num;
}
static int SVR_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir ) {
	(void)point;
	if ( ambientLight ) VectorClear( ambientLight );
	if ( directedLight ) VectorClear( directedLight );
	if ( lightDir ) { lightDir[0] = 0; lightDir[1] = 0; lightDir[2] = 1; }
	return 0;
}
static void SVR_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	(void)org; (void)intensity; (void)r; (void)g; (void)b;
}
static void SVR_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	(void)org; (void)intensity; (void)r; (void)g; (void)b;
}
static void SVR_AddLinearLightToScene( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b ) {
	(void)start; (void)end; (void)intensity; (void)r; (void)g; (void)b;
}
static void SVR_AddRailTrailParams( const railTrailParams_t *params ) { (void)params; }
static void SVR_RenderScene( const refdef_t *fd ) { (void)fd; }

static void SVR_SetColor( const float *rgba ) { (void)rgba; }
static void SVR_SetClipRegion( const float *region ) { (void)region; }
static void SVR_SetMSDFOutline( float outlineWidth, const float *outlineColor, float glowWidth, const float *glowColor ) {
	(void)outlineWidth; (void)outlineColor; (void)glowWidth; (void)glowColor;
}
static void SVR_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	(void)x; (void)y; (void)w; (void)h; (void)s1; (void)t1; (void)s2; (void)t2; (void)hShader;
}
static void SVR_DrawRotatedPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, float angle, qhandle_t hShader ) {
	(void)x; (void)y; (void)w; (void)h; (void)s1; (void)t1; (void)s2; (void)t2; (void)angle; (void)hShader;
}
static void SVR_DrawLine( float x1, float y1, float x2, float y2, float width, qhandle_t hShader ) {
	(void)x1; (void)y1; (void)x2; (void)y2; (void)width; (void)hShader;
}
static void SVR_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty ) {
	(void)x; (void)y; (void)w; (void)h; (void)cols; (void)rows; (void)data; (void)client; (void)dirty;
}
static void SVR_UploadCinematic( int w, int h, int cols, int rows, byte *data, int client, qboolean dirty ) {
	(void)w; (void)h; (void)cols; (void)rows; (void)data; (void)client; (void)dirty;
}
static void SVR_BeginFrame( stereoFrame_t stereoFrame ) { (void)stereoFrame; }
static void SVR_EndFrame( int *frontEndMsec, int *backEndMsec ) {
	if ( frontEndMsec ) *frontEndMsec = 0;
	if ( backEndMsec ) *backEndMsec = 0;
}

static int SVR_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
	int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer )
{
	(void)numPoints; (void)points; (void)projection;
	(void)maxPoints; (void)pointBuffer; (void)maxFragments; (void)fragmentBuffer;
	return 0;
}

/*
====================
SVR_LerpTag

Interpolate the named tag between startFrame and endFrame, matching
the renderer-side R_LerpTag math (linear blend of origin + axis,
followed by re-normalisation of the axis basis). Currently supports
MD3 attachments, which covers every stock Q3 model. IQM handles fall
back to identity because reconstructing tags from skeletal poses
requires the full pose machinery that the dedicated server skips.
====================
*/
static int SVR_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame,
	float frac, const char *tagName )
{
	const svrModel_t *mod;
	const md3Tag_t *start;
	const md3Tag_t *end;
	float frontLerp, backLerp;
	int i;

	if ( !tag ) {
		return 0;
	}

	AxisClear( tag->axis );
	VectorClear( tag->origin );

	if ( handle <= 0 || handle >= svrNumModels ) {
		return 0;
	}
	mod = &svrModels[handle];
	if ( mod->type != SVR_MOD_MD3 ) {
		return 0;
	}
	if ( !tagName || !tagName[0] ) {
		return 0;
	}

	start = SVR_GetTag( mod, startFrame, tagName );
	end = SVR_GetTag( mod, endFrame, tagName );
	if ( !start || !end ) {
		return 0;
	}

	frontLerp = frac;
	backLerp = 1.0f - frac;

	for ( i = 0; i < 3; i++ ) {
		tag->origin[i]  = start->origin[i]  * backLerp + end->origin[i]  * frontLerp;
		tag->axis[0][i] = start->axis[0][i] * backLerp + end->axis[0][i] * frontLerp;
		tag->axis[1][i] = start->axis[1][i] * backLerp + end->axis[1][i] * frontLerp;
		tag->axis[2][i] = start->axis[2][i] * backLerp + end->axis[2][i] * frontLerp;
	}
	VectorNormalize( tag->axis[0] );
	VectorNormalize( tag->axis[1] );
	VectorNormalize( tag->axis[2] );
	return 1;
}

/*
====================
SVR_ModelBounds

Return the frame[0] bounding box cached by SVR_RegisterModel. Invalid
handles zero out both vectors so callers can still use the result for
VectorAdd-style arithmetic without a special case.
====================
*/
static void SVR_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs ) {
	const svrModel_t *mod;

	if ( mins ) VectorClear( mins );
	if ( maxs ) VectorClear( maxs );

	if ( handle <= 0 || handle >= svrNumModels ) {
		return;
	}
	mod = &svrModels[handle];
	if ( mod->type == SVR_MOD_BAD ) {
		return;
	}
	if ( mins ) VectorCopy( mod->mins, mins );
	if ( maxs ) VectorCopy( mod->maxs, maxs );
}

static void SVR_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font ) {
	(void)fontName; (void)pointSize;
	if ( font ) Com_Memset( font, 0, sizeof( *font ) );
}

static void SVR_RemapShader( const char *oldShader, const char *newShader, const char *offsetTime ) {
	(void)oldShader; (void)newShader; (void)offsetTime;
}

static qboolean SVR_GetEntityToken( char *buffer, int size ) {
	(void)buffer; (void)size;
	return qfalse;
}

static qboolean SVR_inPVS( const vec3_t p1, const vec3_t p2 ) {
	(void)p1; (void)p2;
	return qtrue;
}

static void SVR_TakeVideoFrame( int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg ) {
	(void)h; (void)w; (void)captureBuffer; (void)encodeBuffer; (void)motionJpeg;
}

static void SVR_ThrottleBackend( void ) {}
static void SVR_FinishBloom( void ) {}
static void SVR_SetColorMappings( void ) {}
static qboolean SVR_CanMinimize( void ) { return qfalse; }

static glconfig_t svr_glconfig;
static const glconfig_t *SVR_GetConfig( void ) { return &svr_glconfig; }

static void SVR_VertexLighting( qboolean allowed ) { (void)allowed; }
static void SVR_SyncRender( void ) {}

#if FEAT_FOG_SYSTEM
static void SVR_GetGlobalFog( refFogType_t *type, vec3_t color, float *depthForOpaque, float *density ) {
	if ( type ) *type = REF_FT_NONE;
	if ( color ) VectorClear( color );
	if ( depthForOpaque ) *depthForOpaque = 0.0f;
	if ( density ) *density = 0.0f;
}

static void SVR_GetViewFog( const vec3_t origin, refFogType_t *type, vec3_t color,
	float *depthForOpaque, float *density, qboolean *useColorArray )
{
	(void)origin;
	if ( type ) *type = REF_FT_NONE;
	if ( color ) VectorClear( color );
	if ( depthForOpaque ) *depthForOpaque = 0.0f;
	if ( density ) *density = 0.0f;
	if ( useColorArray ) *useColorArray = qfalse;
}
#endif

#if FEAT_CORONA
static void SVR_AddCoronaToScene( const vec3_t org, float r, float g, float b,
	float scale, int id, qboolean visible )
{
	(void)org; (void)r; (void)g; (void)b; (void)scale; (void)id; (void)visible;
}
#endif

#if FEAT_IQM
static int SVR_GetIQMAnimations( qhandle_t model, iqmAnimInfo_t *anims, int maxAnims ) {
	(void)model; (void)anims; (void)maxAnims;
	return 0;
}
#endif


/*
====================
GetRefAPI_Headless

Populate a refexport_t with the headless server implementation. The
caller supplies the struct (usually a file-scope `re` in cl_main.c or
sv_init.c) so the vtable can be installed without dlopen'ing a
renderer DLL.
====================
*/
void GetRefAPI_Headless( refexport_t *re ) {
	if ( !re ) return;

	Com_Memset( re, 0, sizeof( *re ) );

	svr_glconfig.vidWidth = 640;
	svr_glconfig.vidHeight = 480;
	svr_glconfig.windowAspect = 4.0f / 3.0f;

	re->Shutdown = SVR_Shutdown;
	re->BeginRegistration = SVR_BeginRegistration;
	re->RegisterModel = SVR_RegisterModel;
	re->RegisterSkin = SVR_RegisterSkin;
	re->RegisterShader = SVR_RegisterShader;
	re->RegisterShaderNoMip = SVR_RegisterShaderNoMip;
	re->RegisterMSDFShader = SVR_RegisterMSDFShader;
	re->LoadWorld = SVR_LoadWorld;
	re->SetWorldVisData = SVR_SetWorldVisData;
	re->EndRegistration = SVR_EndRegistration;

	re->ClearScene = SVR_ClearScene;
	re->AddRefEntityToScene = SVR_AddRefEntityToScene;
	re->AddPolyToScene = SVR_AddPolyToScene;
	re->LightForPoint = SVR_LightForPoint;
	re->AddLightToScene = SVR_AddLightToScene;
	re->AddAdditiveLightToScene = SVR_AddAdditiveLightToScene;
	re->AddLinearLightToScene = SVR_AddLinearLightToScene;
	re->AddRailTrailParams = SVR_AddRailTrailParams;
	re->RenderScene = SVR_RenderScene;

	re->SetColor = SVR_SetColor;
	re->SetClipRegion = SVR_SetClipRegion;
	re->SetMSDFOutline = SVR_SetMSDFOutline;
	re->DrawStretchPic = SVR_DrawStretchPic;
	re->DrawRotatedPic = SVR_DrawRotatedPic;
	re->DrawLine = SVR_DrawLine;
	re->DrawStretchRaw = SVR_DrawStretchRaw;
	re->UploadCinematic = SVR_UploadCinematic;

	re->BeginFrame = SVR_BeginFrame;
	re->EndFrame = SVR_EndFrame;

	re->MarkFragments = SVR_MarkFragments;
	re->LerpTag = SVR_LerpTag;
	re->ModelBounds = SVR_ModelBounds;

	re->RegisterFont = SVR_RegisterFont;
	re->RemapShader = SVR_RemapShader;
	re->GetEntityToken = SVR_GetEntityToken;
	re->inPVS = SVR_inPVS;

	re->TakeVideoFrame = SVR_TakeVideoFrame;
	re->ThrottleBackend = SVR_ThrottleBackend;
	re->FinishBloom = SVR_FinishBloom;
	re->SetColorMappings = SVR_SetColorMappings;
	re->CanMinimize = SVR_CanMinimize;
	re->GetConfig = SVR_GetConfig;
	re->VertexLighting = SVR_VertexLighting;
	re->SyncRender = SVR_SyncRender;

#if FEAT_FOG_SYSTEM
	re->GetGlobalFog = SVR_GetGlobalFog;
	re->GetViewFog = SVR_GetViewFog;
#endif
#if FEAT_CORONA
	re->AddCoronaToScene = SVR_AddCoronaToScene;
#endif
#if FEAT_IQM
	re->GetIQMAnimations = SVR_GetIQMAnimations;
#endif
}

#else  /* FEAT_HEADLESS_RENDERER */

/* Feature disabled. ISO C forbids a strictly empty translation unit, so
 * emit a single typedef that the rest of the program never references.
 * This keeps the file compilable without introducing runtime state or
 * exported symbols. */
typedef int sv_ref_disabled_marker_t;

#endif /* FEAT_HEADLESS_RENDERER */
