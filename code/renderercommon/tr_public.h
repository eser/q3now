// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "tr_types.h"
#include "vulkan/vulkan.h"
#include "../qcommon/asset_load_log.h"
#include "../qcommon/wired/render/primitives.h"
#include "../qcommon/wired/render/particle_class.h"

typedef struct bspFile_s bspFile_t;

#define	REF_API_VERSION		10

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
	REF_KEEP_CONTEXT, // don't destroy window and context
	REF_KEEP_WINDOW,  // destroy context, keep window
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
	void	(*LoadWorld)( const bspFile_t *bsp );

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

	void	(*RenderScene)( const refdef_t *fd );

	void	(*SetColor)( const float *rgba );	// NULL = 1,1,1,1
	void	(*SetMSDFOutline)( float outlineWidth, const float *outlineColor,
		float glowWidth, const float *glowColor );
	void	(*SetMSDFShadow)( float offsetX, float offsetY, const float *color );
	void	(*SetClipRegion)( const float *region );	// NULL = clear clip region; non-NULL = {x,y,w,h}
	void	(*DrawStretchPic) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader );	// 0 = white
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

	void	(*VertexLighting)( qboolean allowed );
	void	(*SyncRender)( void );

#if FEAT_FOG_SYSTEM
	// Query global fog parameters (fog volume 0 or explicit global fog).
	// type receives REF_FT_NONE if no global fog is set.
	void	(*GetGlobalFog)( refFogType_t *type, vec3_t color, float *depthForOpaque, float *density );

	// Query the fog affecting a given view origin. useColorArray is set to qtrue
	// if the engine should use vertex color arrays for fog, qfalse for fixed-function fog.
	void	(*GetViewFog)( const vec3_t origin, refFogType_t *type, vec3_t color,
		float *depthForOpaque, float *density, qboolean *useColorArray );
#endif

#if FEAT_CORONA
	// Add a corona (lens-flare-style glow) to the current scene. Rendered with
	// depth-buffer occlusion testing.
	void	(*AddCoronaToScene)( const vec3_t org, float r, float g, float b,
		float scale, int id, qboolean visible );
#endif

#if FEAT_IQM
	// Query embedded IQM animation data from a model.
	// Returns number of animations found (0 if not IQM or no anims).
	int		(*GetIQMAnimations)( qhandle_t model, iqmAnimInfo_t *anims, int maxAnims );
#endif // FEAT_IQM

	// Set lightstyle pattern string at runtime (Phase 1+).
	// style in [0,63]; pattern is a NUL-terminated string up to LIGHTSTYLE_PATTERN_MAX chars.
	// Stores the pattern and derives a float value for backward-compat with the float path.
	void	(*SetLightstylePattern)( int style, const char *pattern );

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

	// BSP loading — used by R_RegisterBSP for standalone prop BSPs
	qboolean (*BSP_Load)( const char *name, bspFile_t **bspFile, unsigned flags );
	void     (*BSP_Free)( bspFile_t *bspFile );

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
	qboolean(*Sys_LowPhysicalMemory)( void );

	int		(*Com_RealTime)( qtime_t *qtime );

	// platform-dependent functions
	void(*GLimp_InitGamma)(glconfig_t *config);
	void(*GLimp_SetGamma)(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);

	// OpenGL
	void	(*GLimp_Init)( glconfig_t *config );
	void	(*GLimp_Shutdown)( qboolean unloadDLL );
	void	(*GLimp_EndFrame)( void );
	void*	(*GL_GetProcAddress)( const char *name );

	// Vulkan
	void	(*VKimp_Init)( glconfig_t *config );
	void	(*VKimp_Shutdown)( qboolean unloadDLL );
	void*	(*VK_GetInstanceProcAddr)( VkInstance instance, const char *name );
	qboolean (*VK_CreateSurface)( VkInstance instance, VkSurfaceKHR *pSurface );

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
