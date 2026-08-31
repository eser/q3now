// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "tr_types.h"
#include "r_profile_telemetry.h"
#include "../ral/core/ral_presentation_host.h"
#include "../../qcommon/asset_load_log.h"
#include "../../qcommon/wired/render/primitives.h"
#include "../../qcommon/wired/render/particle_class.h"

typedef struct mapFile_s mapFile_t;

/* Forward-declared so refimport_t can carry arena allocator function
 * pointers without pulling arena.h into renderer headers. The full
 * definition lives in code/qcommon/arena.h and is linked into the engine
 * (wired.x64). The renderer DLL only sees the opaque pointer. */
typedef struct arena_s arena_t;

#define	REF_API_VERSION		31	/* explicit per-world level allocation owner */

#define REF_UI_TRANSFORM_SCHEMA_VERSION 1u
typedef struct {
	uint32_t schemaVersion;
	float x;
	float y;
	float width;
	float height;
	/* Signed normalized edge recession: negative = left, positive = right. */
	float perspective;
} refUiTransform_t;

#define REF_PRESENTATION_CHANGE_SCHEMA_VERSION 1u
enum {
	REF_PRESENTATION_CHANGE_TOPOLOGY = 1u << 0,
	REF_PRESENTATION_CHANGE_ACTIVE_OUTPUT = 1u << 1,
	REF_PRESENTATION_CHANGE_SCALE = 1u << 2,
	REF_PRESENTATION_CHANGE_COLOR = 1u << 3,
	REF_PRESENTATION_CHANGE_FULLSCREEN = 1u << 4,
	REF_PRESENTATION_CHANGE_EXTENT = 1u << 5
};
typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	uint64_t catalogGeneration;
	uint32_t changeFlags;
	uint32_t logicalWidth;
	uint32_t logicalHeight;
	uint32_t presentationWidth;
	uint32_t presentationHeight;
	uint32_t renderWidth;
	uint32_t renderHeight;
	uint32_t uiWidth;
	uint32_t uiHeight;
} refPresentationChange_t;

// Number of concurrent world slots the renderer holds — one per local client
// app. Must be >= the engine's MAX_LOCAL_CGAME_VMS (the app-instance count); the
// world index passed to LoadWorld/RenderScene is bounds-checked against this.
// Only slot 0 is populated when a single app is connected.
#define MAX_RENDER_WORLDS	4

// Lightmap index constants for RegisterShaderLightMap and friends.
// Must match the values in each renderer's tr_common.h.
#define LIGHTMAP_NONE       -1
#define LIGHTMAP_WHITEIMAGE -2
#define LIGHTMAP_BY_VERTEX  -3
#define LIGHTMAP_2D         -4

#if !defined(REF_FOG_TYPE_DEFINED)
#define REF_FOG_TYPE_DEFINED
// Fog type enum — shared between renderer and engine.
// Must match fogType_t in each renderer's tr_local.h.
typedef enum {
	REF_FT_NONE,
	REF_FT_LINEAR,
	REF_FT_EXP,
	REF_FT_EXP2
} refFogType_t;
#endif

//
// these are the functions exported by the refresh module
//
typedef enum {
	REF_LEVEL_ONLY,    // tear down map-scoped state only;
	                   // renderer + RAL context persists
	                   // (RAL owns the Vulkan context for the
	                   //  lifetime of the process)
	REF_KEEP_WINDOW,   // destroy device, keep window
	REF_DESTROY_WINDOW,
	REF_UNLOAD_DLL
} refShutdownCode_t;

typedef struct {
	// called before the library is unloaded
	// if the system is just reconfiguring, pass destroyWindow = qfalse,
	// which will keep the screen from flashing to the desktop.
	void	(*Shutdown)( refShutdownCode_t code );

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void	(*BeginRegistration)( glconfig_t *config );
	qhandle_t (*RegisterModel)( const char *name );
	qhandle_t (*RegisterSkin)( const char *name );
	qhandle_t (*RegisterShader)( const char *name );
	qhandle_t (*RegisterShaderNoMip)( const char *name );
	qhandle_t (*RegisterShaderLightMap)( const char *name, int lightmapIndex );
	qhandle_t (*RegisterMSDFShader)( const char *name, float distanceRange, int atlasWidth, int atlasHeight );
	// Like RegisterShader, but additionally writes the resolved shader's
	// stages[0]→bundle[0]→image[0] into the wired primitive shader image
	// registry (vk_primitive_shader_images[]) so that ribbon / beam /
	// other primitive pipelines can sample the texture by handle. Returns
	// the same qhandle_t RegisterShader would; use this for shader
	// handles that will be passed to trap_R_AddRibbonToScene or
	// trap_R_AddBeamToScene.
	qhandle_t (*RegisterPrimitiveShader)( const char *name );
	// Phase 7.15.4-a: stamp IMGFLAG_PINNED onto every image of the resolved
	// shader's active stages so the texture-LRU (7.15.4-c) never evicts it.
	// The CALLER invokes this after registering a UI / font / console / persistent
	// atlas shader whose image_t*/bindless slot it caches without re-checking
	// residency. Renderer-internal classification only — no render-output effect
	// (the flag has no reader until the victim-scan lands).
	void	(*PinShaderImages)( qhandle_t hShader );
	void	(*LoadWorld)( const mapFile_t *bsp, int worldIndex );

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void	(*SetWorldVisData)( const byte *vis );

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void	(*EndRegistration)( void );

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void	(*ClearScene)( void );
	void	(*AddRefEntityToScene)( const refEntity_t *re, qboolean intShaderTime );
	void	(*AddPolyToScene)( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num );
	int		(*LightForPoint)( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
	void	(*AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddAdditiveLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddLinearLightToScene)( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b );

	// ── primitive submission (wired/render) — generic, effect-agnostic ──
	void	(*AddRibbonToScene)  ( const ribbonDesc_t  *desc );
	void	(*AddBeamToScene)    ( const beamDesc_t    *desc );
	void	(*AddSpriteToScene)  ( const spriteDesc_t  *desc );
	void	(*EmitParticles)     ( const emitterDesc_t *desc );
	void	(*AddDecalToScene)   ( const decalDesc_t   *desc );
	void	(*RegisterParticleClass)( particleClassHandle_t handle,
	                                  const particleClass_t *cls );
	void	(*SetAtmosphere)         ( const atmosphericDesc_t *desc );
	void	(*SetAtmosphereHeightgrid)( const float *grid, int count );

	// Lens-source occlusion oracle (lens-glow unification). The game registers a
	// light source each frame; the renderer's depth-sampling oracle reports its
	// visibility (0 = occluded by geometry, 1 = clear). GetLensVisibility returns
	// qfalse when no GPU oracle is available (GL backends, r_lens off) so the game
	// falls back to its own occlusion test — zero feature loss.
	void	(*AddLensSourceToScene)( const lensSourceDesc_t *desc );
	qboolean (*GetLensVisibility)( int id, float *outVis );

	void	(*RenderScene)( const refdef_t *fd, int worldIndex );

	void	(*SetColor)( const float *rgba );	// NULL = 1,1,1,1
	void	(*SetMSDFOutline)( float outlineWidth, const float *outlineColor,
		float glowWidth, const float *glowColor );
	void	(*SetMSDFShadow)( float offsetX, float offsetY, const float *color );
	void	(*SetClipRegion)( const float *region );	// NULL = clear clip region; non-NULL = {x,y,w,h}
	/* Paint-only subtree transform. NULL restores identity. The client keeps
	 * layout and hit testing axis-aligned; every subsequent 2D primitive is
	 * projected until the transform is cleared/replaced. */
	void	(*SetUiTransform)( const refUiTransform_t *transform );
	void	(*DrawStretchPic) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader );	// 0 = white
	// WiredUI SCENE procedural backdrop: draws a blended full-viewport constellation-
	// over-warm-dusk scene into the current 2D UI pass. time is continuous wallclock
	// seconds; mouseX/Y are normalized cursor [-1..1]; transition is the eased menu-
	// change nudge. Drawn as the backmost UI layer; menu content composites on top.
	void	(*DrawMenuBackdrop)( float x, float y, float w, float h,
		float time, float mouseX, float mouseY, float transition );
	void	(*DrawStretchPicOverlay) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader );	// post-gamma display-space (composite overlay)
	void	(*DrawRotatedPic)( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, float angle, qhandle_t hShader );
	void	(*DrawLine)( float x1, float y1, float x2, float y2, float width, qhandle_t hShader );

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty );
	void	(*UploadCinematic)( int w, int h, int cols, int rows, byte *data, int client, qboolean dirty );

	void	(*BeginFrame)( stereoFrame_t stereoFrame );

	// if the pointers are not NULL, timing info will be returned
	void	(*EndFrame)( int *frontEndMsec, int *backEndMsec );


	int		(*MarkFragments)( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

	int		(*LerpTag)( orientation_t *tag,  qhandle_t model, int startFrame, int endFrame,
					 float frac, const char *tagName );
	void	(*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );

#ifdef __USEA3D
	void    (*A3D_RenderGeometry) (void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
#endif
	void	(*RegisterFont)(const char *fontName, int pointSize, fontInfo_t *font);
	void	(*RemapShader)(const char *oldShader, const char *newShader, const char *offsetTime);
	qboolean (*GetEntityToken)( char *buffer, int size );
	qboolean (*inPVS)( const vec3_t p1, const vec3_t p2 );

	void	(*TakeVideoFrame)( int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg );

	void	(*ThrottleBackend)( void );
	void	(*FinishBloom)( void );

	void	(*SetColorMappings)( void );

	qboolean (*CanMinimize)( void ); // == fbo enabled

	const glconfig_t *(*GetConfig)( void );

	// GPU memory budget for the /meminfo GPU section. Out-params are plain bytes
	// (no RAL struct across the ABI). Returns qtrue if the numbers are real
	// (the backend exposes a budget extension) or qfalse if they are estimates /
	// unavailable (still fills the RAL-tracked footprint into *used). A NULL
	// pointer is allowed for any out-param the caller doesn't need.
	qboolean (*GetMemoryBudget)( uint64_t *deviceLocalUsed, uint64_t *deviceLocalBudget,
	                             uint64_t *hostVisibleUsed,  uint64_t *hostVisibleBudget,
	                             int *pressureLevel /* 0 normal, 1 warning, 2 critical */ );

	void	(*VertexLighting)( qboolean allowed );
	void	(*SyncRender)( void );

	// Query global fog parameters (fog volume 0 or explicit global fog).
	// type receives REF_FT_NONE if no global fog is set.
	void	(*GetGlobalFog)( refFogType_t *type, vec3_t color, float *depthForOpaque, float *density );

	// Query the fog affecting a given view origin. useColorArray is set to qtrue
	// if the engine should use vertex color arrays for fog, qfalse for fixed-function fog.
	void	(*GetViewFog)( const vec3_t origin, refFogType_t *type, vec3_t color,
		float *depthForOpaque, float *density, qboolean *useColorArray );

#if FEAT_HALO
	// Add a halo (lens-flare-style glow) to the current scene. Rendered with
	// depth-buffer occlusion testing.
	void	(*AddHaloToScene)( const vec3_t org, float r, float g, float b,
		float scale, int id, qboolean visible );
#endif

#if FEAT_IQM
	// Query embedded IQM animation data from a model.
	// Returns number of animations found (0 if not IQM or no anims).
	int		(*GetIQMAnimations)( qhandle_t model, iqmAnimInfo_t *anims, int maxAnims );
#endif // FEAT_IQM

	// Query Q1-.mdl-derived animation ranges (prefix-grouped frame names) from a
	// model. Returns the number of ranges found (0 if not a mesh / no named frames).
	int		(*GetMDLAnimations)( qhandle_t model, mdlAnimRange_t *anims, int maxAnims );

	// Set lightstyle pattern string at runtime.
	// style in [0,63]; pattern is a NUL-terminated string up to LIGHTSTYLE_PATTERN_MAX chars.
	// Stores the pattern and derives a float value for backward-compat with the float path.
	void	(*SetLightstylePattern)( int style, const char *pattern );

	// Recoverable init-failure signal. Set by the renderer's BeginRegistration
	// path when a runtime check (e.g. GPU caps) makes the renderer non-viable
	// but NOT a hard process error — cl_main reads this immediately after the
	// BeginRegistration call and advances `cl_renderer` to the next entry in
	// the renderer fallback list. Default (zero-initialised) = qfalse.
	qboolean	initFailed;

	// GPU-resident parametric helix ribbon: cgame submits the spawn-fixed
	// spiral params once at fire; the renderer's persistent pool regenerates
	// the evolving geometry each frame until the duration expires. Appended at
	// the end of refexport_t so existing pointer offsets stay stable (ABI-
	// additive; full rebuild after adding). NULL on renderers without the pool.
	void	(*AddRailRibbonToScene)( const railRibbonDesc_t *desc );

	// Optional pointer-free pull of the newest completed GPU timestamp sample.
	// The engine calls this only after EndFrame while the renderer DLL remains
	// loaded. NULL on renderers without semantic GPU profiling.
	qboolean (*GetGpuProfileSample)( refGpuProfileSample_t *out );

	// Optional atomic entity + temporal identity submission. The renderer owns
	// a copy of both PODs on return. Invalid motion metadata rejects the whole
	// submission; clients lacking discovery support keep using AddRefEntityToScene.
	void (*AddRefEntityToSceneTemporal)( const refEntity_t *re,
		const refEntityMotion_t *motion );

	/* One callback represents one coalesced platform event burst. */
	void (*PresentationChanged)( const refPresentationChange_t *change );

	/* App-owned semantic atmosphere intent; particle instances remain GPU-owned. */
	void (*AddAtmosphereEmitter)( const atmosphereEmitter_t *emitter );
	/* Immutable bounded multi-stage graph; resolved once at registration. */
	void (*RegisterAtmosphereEffectProfile)( uint32_t handle,
		const atmosphereEffectProfile_t *profile );
	/* Frame-local footprint/impact/traversal intent; tiles are renderer-owned. */
	void (*AddAtmosphereSurfaceEvent)( const atmosphereSurfaceEvent_t *event );
	/* Frame-local local-media intent; froxels/composition are renderer-owned. */
	void (*AddAtmosphereMediaVolume)( const atmosphereMediaVolume_t *volume );

	/* Explicit authoring cook. Writes derived .wlight/.wprobe sidecars only. */
	qboolean (*CookLightingProject)( const char *derivedRoot );

	/* Serialized per-app world selection. Selection changes no shared asset
	 * handles; unload retires only the addressed app's map residency. */
	qboolean (*SelectWorld)( int worldIndex );
	qboolean (*UnloadWorld)( int worldIndex );
	int (*ResidentWorldCount)( void );

} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct {
	// log a message at a given severity
	void	FORMAT_PRINTF(2, 3) (QDECL *Log)( log_severity_t severity, const char *fmt, ... );

	// terminate the engine (abort / disconnect)
	void	NORETURN_PTR FORMAT_PRINTF(2, 3)(QDECL *Terminate)( terminationReason_t level, const char *fmt, ... );

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	int		(*Milliseconds)( void );

	int64_t	(*Microseconds)( void );

	// stack based memory allocation for per-level things that
	// won't be freed
#ifdef HUNK_DEBUG
	void	*(*Hunk_AllocDebug)( size_t size, ha_pref pref, const char *label, const char *file, int line );
#else
	void	*(*Hunk_Alloc)( size_t size, ha_pref pref );
#endif
	void	*(*Hunk_AllocateTempMemory)( size_t size );
	void	(*Hunk_FreeTempMemory)( void *block );

	// dynamic memory allocator for things that need to be freed
	void	*(*Malloc)( size_t bytes );
	void	(*Free)( void *buf );
	void	(*FreeAll)( void );

	// Named arena allocators (engine-side qcommon/arena.c).
	// Used by the renderer to register process-lifetime allocations in
	// /meminfo. Bumps the renderer ABI to REF_API_VERSION 10+.
	arena_t *(*Arena_Create)( const char *name, size_t size );
	void     (*Arena_Destroy)( arena_t *arena );
	void    *(*Arena_Alloc)( arena_t *arena, size_t size, size_t alignment );

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags );
	void	(*Cvar_Set)( const char *name, const char *value );
	void	(*Cvar_SetValue) (const char *name, float value);
	void	(*Cvar_CheckRange)( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type );
	void	(*Cvar_SetDescription)( cvar_t *cv, const char *description );

	void	(*Cvar_SetGroup)( cvar_t *var, cvarGroup_t group );
	int		(*Cvar_CheckGroup)( cvarGroup_t group );
	void	(*Cvar_ResetGroup)( cvarGroup_t group, qboolean resetModifiedFlags );

	void	(*Cvar_VariableStringBuffer)( const char *var_name, char *buffer, int bufsize );
	const char *(*Cvar_VariableString)( const char *var_name );
	int		(*Cvar_VariableIntegerValue)( const char *var_name );

	void	(*Cmd_AddCommand)( const char *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand)( const char *name );

	int		(*Cmd_Argc) (void);
	const char	*(*Cmd_Argv) (int i);

	void	(*Cmd_ExecuteText)( cbufExec_t exec_when, const char *text );

	byte	*(*CM_ClusterPVS)(int cluster);
	int		(*CM_PointContents)( const vec3_t p, clipHandle_t model );
	// Engine collision ray-cast bridge. Wraps the engine
	// CM_BoxTrace (cm_trace.c); the engine cm.tracer vtable dispatches q1/q3
	// internally, so the renderer stays format-blind. Used by the load-time
	// sun-mask compute to test per-texel sun visibility against world BSP
	// geometry. brushmask 0 + zero mins/maxs = a point ray through hull-0.
	void	(*CM_BoxTrace)( trace_t *results, const vec3_t start, const vec3_t end,
							const vec3_t mins, const vec3_t maxs,
							clipHandle_t model, int brushmask, qboolean capsule );
	int		(*CM_NumBrushes)( void );
	void	(*CM_GetBrushData)( int idx, int *contents, int *shaderNum, const char **shaderName,
								float mins[3], float maxs[3], int *numsides );
	void	(*CM_GetBrushSideData)( int brushIdx, int sideIdx, int *planeNum, float normal[3],
									float *dist, int *shaderNum, const char **shaderName );

	// visualization for debugging collision detection
	void	(*CM_DrawDebugSurface)( void (*drawPoly)(int color, int numPoints, float *points) );

	// a qfalse return means the file does not exist
	// NULL can be passed for buf to just determine existence
	//int		(*FS_FileIsInPAK)( const char *name, int *pCheckSum );
	int		(*FS_ReadFile)( const char *name, void **buf );
	void	(*FS_FreeFile)( void *buf );
	char **	(*FS_ListFiles)( const char *name, const char *extension, int *numfilesfound );
	void	(*FS_FreeFileList)( char **filelist );
	void	(*FS_WriteFile)( const char *qpath, const void *buffer, int size );
	qboolean (*FS_FileExists)( const char *file );
	/* Resolve exact VFS aliases and search-path precedence without opening a
	 * mutable file cursor. File-backed renderer registries must hash the
	 * returned canonical path/source id, not the caller's compatibility name. */
	qboolean (*FS_ResolveResource)( const char *name, char *canonicalPath,
		size_t canonicalPathSize, uint64_t *sourceId, uint64_t *byteSize,
		unsigned *fsGeneration );

	// BSP loading — used by R_RegisterBSP for standalone prop BSPs
	qboolean (*Map_Load)( const char *name, mapFile_t **bspFile, unsigned flags );
	void     (*Map_Free)( mapFile_t *bspFile );

	// cinematic stuff
	void	(*CIN_UploadCinematic)( int handle );
	int		(*CIN_PlayCinematic)( const char *arg0, int xpos, int ypos, int width, int height, int bits );
	e_status (*CIN_RunCinematic)( int handle );

	void	(*CL_WriteAVIVideoFrame)( const byte *buffer, int size );

	size_t	(*CL_SaveJPGToBuffer)( byte *buffer, size_t bufSize, int quality, int image_width, int image_height, byte *image_buffer, int padding );
	void	(*CL_SaveJPG)( const char *filename, int quality, int image_width, int image_height, byte *image_buffer, int padding );
	void	(*CL_LoadJPG)( const char *filename, unsigned char **pic, int *width, int *height );

	qboolean (*CL_IsMinimized)( void );
	void	(*CL_SetScaling)( float factor, int captureWidth, int captureHeight );

	void	(*Sys_SetClipboardBitmap)( const byte *bitmap, int size );
	// cross-platform image clipboard (PNG payload).
	void	(*Sys_SetClipboardImagePNG)( const byte *png, int length );
	qboolean(*Sys_LowPhysicalMemory)( void );

	int		(*Com_RealTime)( qtime_t *qtime );

	// platform-dependent functions
	void(*GLimp_InitGamma)(glconfig_t *config);
	void(*GLimp_SetGamma)(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);

	// OpenGL
	void	(*GLimp_Init)( glconfig_t *config );
	void	(*GLimp_InitOpenGL46)( glconfig_t *config );
	void	(*GLimp_Shutdown)( qboolean unloadDLL );
	void	(*GLimp_EndFrame)( void );
	void*	(*GL_GetProcAddress)( const char *name );

	// Vulkan
	void	(*VKimp_Init)( glconfig_t *config );
	void	(*VKimp_Shutdown)( qboolean unloadDLL );
	void*	(*VK_GetInstanceProcAddr)( void *nativeInstance, const char *name );
	const char *const *(*VK_GetInstanceExtensions)( uint32_t *count );
	qboolean (*VK_CreateSurface)( void *nativeInstance, uint64_t *outNativeSurface );

	const cmSkin_t *(*GetCharacterSkin)( qhandle_t handle );

	void	(*AssetLog_Event)( const char *subsystem, const char *full_path,
	                           const char *extensions_tried, const char *shader_context,
	                           assetLogSeverity_t severity );

	// q3now meta-remap: renderer DLL → engine callback for the active map's
	// typed asset substitution set. `kind` selects the sub-table
	// (shaders / textures / sounds / music; values match the engine-side
	// remap_kind_t enum 0..3). Returns the substitute path on hit, NULL
	// otherwise. May itself be NULL in early-init paths; callers must
	// null-check. Engine-side state lives in code/qcommon/maps/meta_remap.c.
	const char *(*MetaRemap_Lookup)( int kind, const char *name );

	// rilog-channel-mechanism Turn A — channel-aware logging.
	// `GetLogChannel(name)` resolves (lazy-registers) a dot-hierarchical
	// channel and returns its integer id; the renderer's R_LOG macro caches
	// the id per-TU so the engine-side Log_GetChannel runs once per channel
	// per TU. `LogCh(channel, sev, fmt, ...)` is the channel-aware sink — a
	// thin wrapper over Com_Logv. The existing `Log` entry (which routes to
	// the `renderer` root channel only) stays parallel for the ~600 legacy
	// ri.Log call sites; the channelled path bypasses it. Bumps the renderer
	// ABI to REF_API_VERSION 11.
	int  (*GetLogChannel)( const char *name );
	void FORMAT_PRINTF(3, 4) (QDECL *LogCh)( int channel, log_severity_t severity, const char *fmt, ... );

	// Backend-neutral main-surface ownership. The engine/platform layer owns
	// the window or canvas; renderer modules borrow an opaque native surface
	// only through this versioned cohort. No SDL, Vulkan, Metal or WebGPU type
	// crosses the renderer ABI.
	ralPresentationHostImports_t PresentationHost;

	/* Explicit renderer-world → Level arena bridge.  Map-owned renderer data
	 * must name its owner; it may not discover one through the process-global
	 * active-app cursor.  The client resets this arena only after UnloadWorld
	 * has retired the matching renderer slot. */
	void *(*WorldLevelAlloc)( int worldIndex, size_t size, size_t alignment );
	size_t (*WorldLevelUsed)( int worldIndex );

} refimport_t;

extern	refimport_t	ri;

// this is the only function actually exported at the linker level
// If the module can't init to a valid rendering state, NULL will be
// returned.
#ifdef USE_RENDERER_DLOPEN
typedef	refexport_t* (QDECL *GetRefAPI_t) (int apiVersion, refimport_t * rimp);
#else
refexport_t*GetRefAPI( int apiVersion, refimport_t *rimp );
#endif

#endif	// __TR_PUBLIC_H
