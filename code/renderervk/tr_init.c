// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// tr_init.c -- functions that are not called every frame

#include "tr_local.h"
#include "../renderercommon/r_log.h"  // rilog-channel-mechanism — R_LOG / R_LOG_DECLARE_CHANNEL
#include "../qcommon/wired/wired_build_stamp.h"  // WIRED_BUILD_ID / WIRED_BUILD_DATE (this DLL's own stamp)
#ifdef USE_VULKAN
#include "vk_ral_textures.h"   // \ral_textures dump cmd registration
#endif
#include <stdlib.h>

// rilog-channel-mechanism — TU-local handles for the renderer sub-channels
// used in this file. Turn A wired rch_init for GfxInfo + R_Init/RE_Shutdown
// banners; Turn B routes the remaining VarInfo / GL-extension probe / VkInfo
// dump banners through the same rch_init. The screenshot-command warnings
// belong to per-frame command surface and route through rch_cmd. Engine
// root `renderer` is floored to WARN; both sub-channels inherit.
R_LOG_DECLARE_CHANNEL( rch_init, "renderer.init" );
R_LOG_DECLARE_CHANNEL( rch_cmd,  "renderer.cmd"  );

static int s_r_device_mod = -1;

/* ---- Persistent backend storage (Step 3.6 — Renderer_Arena) -------------
   backEndData must survive Hunk_ClearLevel across async spawn phases.
   It is NOT allocated from the hunk — it lives in a named arena so map
   transitions cannot free it AND the allocation shows up in /meminfo
   alongside the other persistent arenas (Maps, WiredScript, Console,
   Audio, Font, WiredUI_HUD). The arena holds exactly one allocation —
   the backEndData_t header + poly pool — and is destroyed only at
   REF_KEEP_WINDOW / REF_DESTROY_WINDOW / REF_UNLOAD_DLL teardown.

   Arena migration: malloc/free → Arena_Create / Arena_Destroy.
   No size, layout, or lifetime change.
   -----------------------------------------------------------------------*/
static arena_t *s_backEndArena      = NULL;
static void    *s_backEndStorage    = NULL;
static size_t   s_backEndStorageSize = 0;

glconfig_t	glConfig;

qboolean	textureFilterAnisotropic;
int			maxAnisotropy;
int			gl_version;
int			gl_clamp_mode;	// GL_CLAMP or GL_CLAMP_TO_EGGE

glstate_t	glState;

glstatic_t	gls;

#ifdef USE_VULKAN
static void VkInfo_f( void );
#endif
static void GfxInfo( void );
static void VarInfo( void );
static void GL_SetDefaultState( void );

cvar_t	*r_flareSize;
cvar_t	*r_flareFade;
cvar_t	*r_flareCoeff;
cvar_t	*r_flareTarget;

/* c2-shadertime-pin — dev/C2-smoke-only override of the wall-clock-driven
 * shader animation time. When non-zero, RE_RenderScene clamps the inputs
 * to wave-shader / tcMod / deform evaluation (tr.refdef.time +
 * tr.refdef.floatTime) to this fixed value (seconds). Defaults to 0.0 →
 * no override → wall-clock-driven shader animation as in normal gameplay.
 * Set by the C2 smoke harness via +set; CVAR_CHEAT keeps it dev-only and
 * non-archive so it never persists to config.cfg. */
cvar_t	*r_pinShaderTime;

#if FEAT_FOG_SYSTEM
cvar_t	*r_useGlFog;
cvar_t	*r_defaultFogParmsType;
cvar_t	*r_globalLinearFogDrawSky;
#endif

cvar_t	*r_detailTextures;

cvar_t	*r_znear;
cvar_t	*r_zproj;
cvar_t	*r_stereoSeparation;

cvar_t	*r_skipBackEnd;

//cvar_t	*r_anaglyphMode;

cvar_t	*r_saturation;
cvar_t	*r_dither;
cvar_t	*r_chromaticAberration;
cvar_t	*r_presentBits;
#if FEAT_DEPTH_CLAMP
cvar_t	*r_depthClamp;
#endif

static cvar_t *r_ignorehwgamma;

cvar_t  *r_teleporterFlash;

cvar_t	*r_fastsky;
cvar_t	*r_neatsky;
cvar_t	*r_drawSky;
cvar_t	*r_drawSun;
cvar_t	*r_dynamiclight;
cvar_t  *r_mergeLightmaps;
cvar_t  *r_lightmapAtlas;
cvar_t  *r_loadingFpsCap;
#ifdef USE_PMLIGHT
cvar_t	*r_dlightScale;
cvar_t	*r_dlightIntensity;
#endif
cvar_t	*r_dlightSaturation;
#ifdef USE_VULKAN
cvar_t	*r_device;
#ifdef USE_VBO
cvar_t	*r_vbo;
#endif
cvar_t	*r_fbo;
cvar_t	*r_hdr;
// legacy-mainpath-retire: r_bindlessMainPath cvar retired (declaration
// in tr_local.h also gone). Bindless is the sole main path post-retire.
cvar_t	*r_hdrDisplay;        // HDR10 swapchain colorspace (BT.2020 + PQ)
cvar_t	*r_hdrPeakLuminance;  // HDR10 display peak (nits) — tonemap shoulder + metadata hint
cvar_t	*r_hdrMinLuminance;   // HDR10 display min (nits) — metadata hint only
cvar_t	*r_hdrAutoExposure;   // histogram auto-exposure master toggle
#ifndef NDEBUG
cvar_t	*r_hdrHistogramDebug; // developer histogram readback + log (debug build only)
cvar_t	*r_brdfLutDebug;      // developer BRDF LUT readback + log (debug build only)
cvar_t	*r_probeSourceDebug;  // developer IBL source-cube readback + log (debug build only)
cvar_t	*r_probeRadianceDebug;// developer IBL convolve readback + log (debug build only)
#endif
cvar_t	*r_hdrExposureKey;    // middle-grey target
cvar_t	*r_hdrExposurePctLow; // low histogram percentile clip
cvar_t	*r_hdrExposurePctHigh;// high histogram percentile clip
cvar_t	*r_hdrAdaptionRateUp; // adaptation rate dark->light
cvar_t	*r_hdrAdaptionRateDown;// adaptation rate light->dark
cvar_t	*r_hdrExposureMin;    // exposure clamp floor
cvar_t	*r_hdrExposureMax;    // exposure clamp ceiling
#if FEAT_PBR
cvar_t	*r_pbr;
#endif
cvar_t	*r_forwardPlus;
cvar_t	*r_unbakeStaticLights;
#if FEAT_SHADOW_MAPPING
cvar_t	*r_dlightShadows;
cvar_t	*r_dlightShadowK;
cvar_t	*r_dlightShadowTest;
cvar_t	*r_dlightShadowTestN;
cvar_t	*r_dlightShadowCount;
cvar_t	*r_dlightShadowProfile;
cvar_t	*r_shadowAtestTest;
#endif
cvar_t	*r_bloom;
cvar_t	*r_bloomPasses;
cvar_t	*r_vrs;
#ifdef __APPLE__
cvar_t	*r_vkApplePinkBarrier;
#endif
cvar_t	*r_bloomThreshold;
cvar_t	*r_bloomIntensity;
cvar_t	*r_bloomThresholdMode;
cvar_t	*r_renderWidth;
cvar_t	*r_renderHeight;
cvar_t	*r_renderScale;
cvar_t	*r_ext_supersample;
cvar_t	*r_depthFade;
cvar_t	*r_depthFadeScale;
#if FEAT_PARALLAX_MAPPING
cvar_t	*r_parallaxMapping;
#endif
#if FEAT_SSAO
cvar_t	*r_ssao;
cvar_t	*r_ssaoRadius;
cvar_t	*r_ssaoQuality;
cvar_t	*r_ssaoIntensity;
cvar_t	*r_showAO;
// SSCS (directional screen-space contact shadow) — folded into the GTAO pass,
// gated on r_shadows (cast) + a real map sun. Tuning only; the on/off is r_shadows.
cvar_t	*r_sscsRadius;
cvar_t	*r_sscsStrength;
#endif
cvar_t	*r_asyncCompute;   // async-compute: GTAO on the dedicated compute queue
cvar_t	*r_asyncTextureUpload;   // async-transfer: pipeline texture uploads on the dedicated transfer queue
#if FEAT_TONEMAP
cvar_t	*r_tonemap;
cvar_t	*r_tonemapExposure;
cvar_t	*r_lottes_contrast;
cvar_t	*r_lottes_shoulder;
cvar_t	*r_lottes_mid_in;
cvar_t	*r_lottes_mid_out;
cvar_t	*r_lottes_hdr_max;
#endif
#if FEAT_COLOR_GRADING
cvar_t	*r_colorGrading;
cvar_t	*r_grade_tint_r;
cvar_t	*r_grade_tint_g;
cvar_t	*r_grade_tint_b;
cvar_t	*r_grade_saturation;
cvar_t	*r_grade_contrast;
#endif
#if FEAT_SUNRAYS
cvar_t	*r_drawSunRays;
cvar_t	*r_sunRayIntensity;
cvar_t	*r_sunRayDecay;
#endif
cvar_t	*r_smaa;
cvar_t	*r_smaa_threshold;
cvar_t	*r_lerpLightstyles;
#endif // USE_VULKAN

cvar_t	*r_dlightBacks;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;

cvar_t	*r_norefresh;
cvar_t	*r_drawEntities;
cvar_t	*r_drawWorld;
cvar_t	*r_speeds;
cvar_t	*r_gpuSpeeds;
cvar_t	*r_profileMarkers;
cvar_t	*r_temporalInputTest;
cvar_t	*r_vkDebugTiming;
cvar_t	*r_frameSpikeUs;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_gpuBatchDecomp;   // host frame-current cull drives the VBO-eligible world draw
cvar_t	*r_facePlaneCull;
cvar_t	*r_showCluster;
cvar_t	*r_nocurves;

// File-scope refexport_t. Hoisted to module-top so R_Init can check
// s_re.initFailed (set by R_DeclineInit via vk_initialize's caps-decline path)
// without forward-reference juggling. GetRefAPI populates this struct and
// returns its address; cl_main keeps a pointer (s_re_dll) and polls
// initFailed after BeginRegistration.
static refexport_t s_re;

void R_DeclineInit( void ) {
	s_re.initFailed = qtrue;
}

cvar_t	*r_allowExtensions;

cvar_t	*r_ext_compressed_textures;
cvar_t	*r_ext_multitexture;
cvar_t	*r_ext_compiled_vertex_array;
cvar_t	*r_ext_texture_env_add;
cvar_t	*r_ext_texture_filter_anisotropic;
cvar_t	*r_ext_max_anisotropy;

cvar_t	*r_ignoreGLErrors;

//cvar_t	*r_stencilBits;
cvar_t	*r_textureBits;
// r_useRALTextures / r_useRALBuffers /
// r_useRALPipelines retired (RAL backend unconditional now).
cvar_t	*r_ext_alpha_to_coverage;

cvar_t	*r_drawBuffer;
cvar_t	*r_lightmap;
cvar_t	*r_entitySSBO;
cvar_t	*r_vertexLight;
cvar_t	*r_shadows;
cvar_t	*r_flares;
cvar_t	*r_lens;
cvar_t	*r_halos;
cvar_t	*r_nobind;
cvar_t	*r_singleShader;
cvar_t	*r_roundImagesDown;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_nomip;
cvar_t	*r_showTris;
cvar_t	*r_showSky;
cvar_t	*r_showNormals;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_textureMode;
cvar_t	*r_offsetFactor;
cvar_t	*r_offsetUnits;
cvar_t	*r_gamma;
cvar_t	*r_intensity;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;

cvar_t	*r_brightness;
cvar_t	*r_mapSaturation;
cvar_t	*r_lightmapSaturation;
cvar_t	*r_lightmapBoost;     // world-lightmap overbright (vanilla-x2 = 4.6 default; live)

cvar_t	*r_debugSurface;
cvar_t	*r_simpleMipMaps;

cvar_t	*r_showImages;
cvar_t	*r_defaultImage;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_debugLight;
cvar_t	*r_debugSort;
cvar_t	*r_printShaders;
cvar_t	*r_saveFontData;

cvar_t	*r_marksOnTriangleMeshes;

cvar_t	*r_gpuDecals;          // GPU decal projector pass (RB_DrawDecals) on/off
cvar_t	*r_atmosphericGPU;     // GPU-resident atmospheric weather: 1 = draw, 0 = skip (no weather)
cvar_t	*r_particles;          // GPU particle pass (RB_DrawParticles) on/off

cvar_t	*r_aviMotionJpegQuality;
cvar_t	*r_screenshotJpegQuality;

static cvar_t *r_maxpolys;
static cvar_t* r_maxpolyverts;
int		max_polys;
int		max_polyverts;

#ifdef USE_VULKAN

#include "vk.h"
Vk_Instance vk;
Vk_World	vk_world;

#else

static char gl_extensions[ 32768 ];

#define GLE( ret, name, ... ) ret ( APIENTRY * q##name )( __VA_ARGS__ );
	QGL_Core_PROCS;
	QGL_Ext_PROCS;
#undef GLE

typedef struct {
	void **symbol;
	const char *name;
} sym_t;

#define GLE( ret, name, ... ) { (void**)&q##name, XSTRING(name) },
static sym_t core_procs[] = { QGL_Core_PROCS };
static sym_t ext_procs[] = { QGL_Ext_PROCS };
#undef GLE


/*
==================
R_ResolveSymbols

returns NULL on success or last failed symbol name otherwise
==================
*/
static const char *R_ResolveSymbols( sym_t *syms, int count )
{
	for ( int i = 0; i < count; i++ )
	{
		*syms[ i ].symbol = ri.GL_GetProcAddress( syms[ i ].name );
		if ( *syms[ i ].symbol == NULL )
		{
			return syms[ i ].name;
		}
	}
	return NULL;
}


static void R_ClearSymbols( sym_t *syms, int count )
{
	for ( int i = 0; i < count; i++ )
	{
		*syms[ i ].symbol = NULL;
	}
}


static void R_ClearSymTables( void )
{
	R_ClearSymbols( core_procs, ARRAY_LEN( core_procs ) );
	R_ClearSymbols( ext_procs, ARRAY_LEN( ext_procs ) );
}

#endif


// for modular renderer
#ifdef USE_RENDERER_DLOPEN
void QDECL Com_Log_Impl( log_severity_t severity, int channel, const char *fmt, ... )
{
	char buf[ MAXPRINTMSG ];
	va_list	argptr;
	(void)channel;  // renderer DLL routes everything through ri.Log → "renderer"
	va_start( argptr, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );
	R_LOG( rch_init,severity, "%s", buf );
}

// Stub for q_shared.c's LOG_CH expansion. The renderer DLL has no access
// to the engine's channel registry; everything is routed through ri.Log to
// the top-level "renderer" channel anyway, so the returned id is unused.
int Log_GetChannel( const char *name )
{
	(void)name;
	return 0;
}

void NORETURN QDECL Com_Terminate( terminationReason_t reason, const char *fmt, ... )
{
	char buf[ MAXPRINTMSG ];
	va_list	argptr;
	va_start( argptr, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );
	ri.Terminate( reason, "%s", buf );
}
#endif


#ifndef USE_VULKAN
/*
** R_HaveExtension
*/
static qboolean R_HaveExtension( const char *ext )
{
	const char *ptr = Q_stristr( gl_extensions, ext );
	if (ptr == NULL)
		return qfalse;
	ptr += strlen(ext);
	return ((*ptr == ' ') || (*ptr == '\0'));  // verify its complete string.
}


/*
** R_InitExtensions
*/
static void R_InitExtensions( void )
{
	GLint max_texture_size = 0;
	float version;
	size_t len;

	if ( !qglGetString( GL_EXTENSIONS ) )
	{
		ri.Terminate( TERM_UNRECOVERABLE, "OpenGL installation is broken. Please fix video drivers and/or restart your system" );
	}

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (char *)qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (char *)qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	len = strlen( glConfig.renderer_string );
	if ( len && glConfig.renderer_string[ len - 1 ] == '\n' )
		glConfig.renderer_string[ len - 1 ] = '\0';
	Q_strncpyz( glConfig.version_string, (char *)qglGetString( GL_VERSION ), sizeof( glConfig.version_string ) );

	Q_strncpyz( gl_extensions, (char *)qglGetString( GL_EXTENSIONS ), sizeof( gl_extensions ) );
	Q_strncpyz( glConfig.extensions_string, gl_extensions, sizeof( glConfig.extensions_string ) );

	version = Q_atof( (const char *)qglGetString( GL_VERSION ) );
	gl_version = (int)(version * 10.001);

	glConfig.textureCompression = TC_NONE;

	glConfig.textureEnvAddAvailable = qfalse;

	textureFilterAnisotropic = qfalse;
	maxAnisotropy = 0;

	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;

	glConfig.numTextureUnits = 1;
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;

	gl_clamp_mode = GL_CLAMP; // by default

	// OpenGL driver constants
	qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &max_texture_size );
	glConfig.maxTextureSize = max_texture_size;

	// stubbed or broken drivers may have reported 0...
	if ( glConfig.maxTextureSize <= 0 )
		glConfig.maxTextureSize = 0;
	else if ( glConfig.maxTextureSize > MAX_TEXTURE_SIZE )
		glConfig.maxTextureSize = MAX_TEXTURE_SIZE; // ResampleTexture() relies on that maximum

	if ( !r_allowExtensions->integer )
	{
		R_LOG( rch_init, SEV_INFO, "*** IGNORING OPENGL EXTENSIONS ***\n" );
		return;
	}

	R_LOG( rch_init, SEV_INFO, "Initializing OpenGL extensions\n" );

	if ( R_HaveExtension( "GL_EXT_texture_edge_clamp" ) || R_HaveExtension( "GL_SGIS_texture_edge_clamp" ) ) {
		gl_clamp_mode = GL_CLAMP_TO_EDGE;
		R_LOG( rch_init, SEV_INFO, "...using GL_EXT_texture_edge_clamp\n" );
	} else {
		R_LOG( rch_init, SEV_INFO, "...GL_EXT_texture_edge_clamp not found\n" );
		R_LOG( rch_init, SEV_WARN, "...Degraded texture support likely!\n" );
	}

	// GL_EXT_texture_compression_s3tc
	if ( R_HaveExtension( "GL_ARB_texture_compression" ) &&
		 R_HaveExtension( "GL_EXT_texture_compression_s3tc" ) )
	{
		if ( r_ext_compressed_textures->integer ){
			glConfig.textureCompression = TC_S3TC_ARB;
			R_LOG( rch_init, SEV_INFO, "...using GL_EXT_texture_compression_s3tc\n" );
		} else {
			R_LOG( rch_init, SEV_INFO, "...ignoring GL_EXT_texture_compression_s3tc\n" );
		}
	} else {
		R_LOG( rch_init, SEV_INFO, "...GL_EXT_texture_compression_s3tc not found\n" );
	}

	// GL_S3_s3tc
	if ( glConfig.textureCompression == TC_NONE && r_ext_compressed_textures->integer ) {
		if ( R_HaveExtension( "GL_S3_s3tc" ) ) {
			if ( r_ext_compressed_textures->integer ) {
				glConfig.textureCompression = TC_S3TC;
				R_LOG( rch_init, SEV_INFO, "...using GL_S3_s3tc\n" );
			} else {
				glConfig.textureCompression = TC_NONE;
				R_LOG( rch_init, SEV_INFO, "...ignoring GL_S3_s3tc\n" );
			}
		} else {
			R_LOG( rch_init, SEV_INFO, "...GL_S3_s3tc not found\n" );
		}
	}

	// GL_EXT_texture_env_add
	if ( R_HaveExtension( "EXT_texture_env_add" ) ) {
		if ( r_ext_texture_env_add->integer ) {
			glConfig.textureEnvAddAvailable = qtrue;
			R_LOG( rch_init, SEV_INFO, "...using GL_EXT_texture_env_add\n" );
		} else {
			glConfig.textureEnvAddAvailable = qfalse;
			R_LOG( rch_init, SEV_INFO, "...ignoring GL_EXT_texture_env_add\n" );
		}
	} else {
		R_LOG( rch_init, SEV_INFO, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_ARB_multitexture
	if ( R_HaveExtension( "GL_ARB_multitexture" ) )
	{
		if ( r_ext_multitexture->integer )
		{
			qglMultiTexCoord2fARB = ri.GL_GetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ri.GL_GetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ri.GL_GetProcAddress( "glClientActiveTextureARB" );

			if ( qglActiveTextureARB && qglClientActiveTextureARB )
			{
				GLint textureUnits = 0;

				qglGetIntegerv( GL_MAX_ACTIVE_TEXTURES_ARB, &textureUnits );

				if ( textureUnits > 1 )
				{
					GLint max_shader_units = 0;
					GLint max_bind_units = 0;

					qglGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &max_shader_units );
					qglGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_bind_units );

					if ( max_bind_units > max_shader_units )
						max_bind_units = max_shader_units;
					if ( max_bind_units > MAX_TEXTURE_UNITS )
						max_bind_units = MAX_TEXTURE_UNITS;

					glConfig.numTextureUnits = MAX( textureUnits, max_bind_units );
					R_LOG( rch_init, SEV_INFO, "...using GL_ARB_multitexture\n" );
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					R_LOG( rch_init, SEV_INFO, "...not using GL_ARB_multitexture, < 2 texture units\n" );
				}
			}
		}
		else
		{
			R_LOG( rch_init, SEV_INFO, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		R_LOG( rch_init, SEV_INFO, "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	if ( R_HaveExtension( "GL_EXT_compiled_vertex_array" ) )
	{
		if ( r_ext_compiled_vertex_array->integer )
		{
			R_LOG( rch_init, SEV_INFO, "...using GL_EXT_compiled_vertex_array\n" );
			qglLockArraysEXT = ri.GL_GetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = ri.GL_GetProcAddress( "glUnlockArraysEXT" );
			if ( !qglLockArraysEXT || !qglUnlockArraysEXT ) {
				ri.Terminate( TERM_UNRECOVERABLE, "bad getprocaddress" );
			}
		}
		else
		{
			R_LOG( rch_init, SEV_INFO, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}
	else
	{
		R_LOG( rch_init, SEV_INFO, "...GL_EXT_compiled_vertex_array not found\n" );
	}

	if ( R_HaveExtension( "GL_EXT_texture_filter_anisotropic" ) )
	{
		if ( r_ext_texture_filter_anisotropic->integer ) {
			qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy );
			if ( maxAnisotropy <= 0 ) {
				R_LOG( rch_init, SEV_INFO, "...GL_EXT_texture_filter_anisotropic not properly supported!\n" );
				maxAnisotropy = 0;
			}
			else
			{
				R_LOG( rch_init, SEV_INFO, "...using GL_EXT_texture_filter_anisotropic (max: %i)\n", maxAnisotropy );
				textureFilterAnisotropic = qtrue;
				maxAnisotropy = MIN( r_ext_texture_filter_anisotropic->integer, maxAnisotropy );
			}
		}
		else
		{
			R_LOG( rch_init, SEV_INFO, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
		}
	}
	else
	{
		R_LOG( rch_init, SEV_INFO, "...GL_EXT_texture_filter_anisotropic not found\n" );
	}
}
#endif


/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL( void )
{
	//
	// initialize OS specific portions of the renderer
	//
	// GLimp_Init directly or indirectly references the following cvars:
	//		- r_fullscreen
	//		- r_mode
	//		- r_(color|depth|stencil)bits
	//		- r_ignorehwgamma
	//		- r_gamma
	//

	if ( glConfig.vidWidth == 0 )
	{
#ifdef USE_VULKAN
		if ( !ri.VKimp_Init )
		{
			ri.Terminate( TERM_UNRECOVERABLE, "Vulkan interface is not initialized" );
		}

		// This function is responsible for initializing a valid Vulkan subsystem.
		ri.VKimp_Init( &glConfig );

		gls.windowWidth = glConfig.vidWidth;
		gls.windowHeight = glConfig.vidHeight;

		gls.captureWidth = glConfig.vidWidth;
		gls.captureHeight = glConfig.vidHeight;

		ri.CL_SetScaling( 1.0, glConfig.vidWidth, glConfig.vidHeight );

		if ( r_fbo->integer )
		{
			if ( r_renderScale->integer )
			{
				glConfig.vidWidth = r_renderWidth->integer;
				glConfig.vidHeight = r_renderHeight->integer;
			}

			gls.captureWidth = glConfig.vidWidth;
			gls.captureHeight = glConfig.vidHeight;

			ri.CL_SetScaling( 1.0, gls.captureWidth, gls.captureHeight );

			if ( r_ext_supersample->integer )
			{
				glConfig.vidWidth *= 2;
				glConfig.vidHeight *= 2;
				ri.CL_SetScaling( 2.0, gls.captureWidth, gls.captureHeight );
			}
		}

		// vk_initialize returns qfalse on a RECOVERABLE init failure (bindless
		// caps decline). The renderer flags re.initFailed inside that path;
		// cl_main reads the flag and advances cl_renderer to the next renderer.
		// Bail out of InitOpenGL without touching the rest of the init chain.
		if ( !vk_initialize() ) {
			return;
		}
#else
		const char *err;

		ri.GLimp_Init( &glConfig );

		R_ClearSymTables();

		err = R_ResolveSymbols( core_procs, ARRAY_LEN( core_procs ) );
		if ( err )
			ri.Terminate( TERM_UNRECOVERABLE, "Error resolving core OpenGL function '%s'", err );

		R_InitExtensions();
#endif

		glConfig.deviceSupportsGamma = qfalse;

		ri.GLimp_InitGamma( &glConfig );

		gls.deviceSupportsGamma = glConfig.deviceSupportsGamma;

		if ( r_ignorehwgamma->integer )
			glConfig.deviceSupportsGamma = qfalse;

		// print info
		GfxInfo();

		gls.initTime = ri.Milliseconds();
	}

#ifdef USE_VULKAN
	if ( !vk.active ) {
		// might happen after REF_KEEP_WINDOW
		if ( !vk_initialize() ) {
			// Recoverable caps decline; re.initFailed is set, cl_main will
			// advance cl_renderer. Bail before vk_init_descriptors / the
			// `!vk.active` Terminate below.
			return;
		}
		gls.initTime = ri.Milliseconds();
	}
	if ( vk.active ) {
		vk_init_descriptors();
	} else {
		ri.Terminate( TERM_UNRECOVERABLE, "Recursive error during Vulkan initialization" );
	}
#endif

	// set default state
	GL_SetDefaultState();

	tr.inited = qtrue;
}


/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrors( void ) {
#ifdef USE_VULKAN
#else
	int		err;
    const char *s;
    char buf[32];

    err = qglGetError();
    if ( err == GL_NO_ERROR ) {
        return;
    }
    if ( r_ignoreGLErrors->integer ) {
        return;
    }
    switch( err ) {
        case GL_INVALID_ENUM:
            s = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            s = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            s = "GL_INVALID_OPERATION";
            break;
        case GL_STACK_OVERFLOW:
            s = "GL_STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            s = "GL_STACK_UNDERFLOW";
            break;
        case GL_OUT_OF_MEMORY:
            s = "GL_OUT_OF_MEMORY";
            break;
        default:
            Com_sprintf( buf, sizeof(buf), "%i", err);
            s = buf;
            break;
    }

    ri.Terminate( TERM_UNRECOVERABLE, "GL_CheckErrors: %s", s );
#endif
}


/*
==============================================================================

						SCREEN SHOTS

NOTE TTimo
some thoughts about the screenshots system:
screenshots get written in fs_homepath + fs_gamedir
q3now q3 .. base/screenshots/ *.png

one command: "screenshot" (see renderercommon/tr_screenshot.c for the
token grammar — png|jpg|bmp|tga|clipboard|silent|levelshot|<filename>)
we use statics to store a count and start writing the first screenshot/screenshot????.tga (.jpg) available
(with FS_FileExists / FS_FOpenFileWrite calls)
FIXME: the statics don't get a reinit between fs_game changes

==============================================================================
*/

/*
==================
RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with ri.Hunk_FreeTempMemory()
==================
*/
static byte *RB_ReadPixels(int x, int y, int width, int height, size_t *offset, int *padlen, int lineAlign )
{
#ifdef USE_VULKAN
	byte *buffer, *bufstart;
	int	bufAlign;
	int packAlign = 1;

	int linelen = width * 3;

	bufAlign = MAX( packAlign, 16 ); // for SIMD

	// Allocate a few more bytes so that we can choose an alignment we like
	//buffer = ri.Hunk_AllocateTempMemory(padwidth * height + *offset + bufAlign - 1);
	buffer = ri.Hunk_AllocateTempMemory(width * height * 4 + *offset + bufAlign - 1);
	bufstart = PADP((intptr_t) buffer + *offset, bufAlign);

	vk_read_pixels( bufstart, width, height );

	*offset = bufstart - buffer;
	*padlen = PAD(linelen, packAlign) - linelen;

	return buffer;
#else
	byte *buffer, *bufstart;
	int padwidth, linelen;
	int	bufAlign;
	GLint packAlign;

	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

	linelen = width * 3;

	if ( packAlign < lineAlign )
		padwidth = PAD(linelen, lineAlign);
	else
		padwidth = PAD(linelen, packAlign);

	bufAlign = MAX( packAlign, 16 ); // for SIMD

	// Allocate a few more bytes so that we can choose an alignment we like
	buffer = ri.Hunk_AllocateTempMemory(padwidth * height + *offset + bufAlign - 1);
	bufstart = PADP((intptr_t) buffer + *offset, bufAlign);

	qglReadPixels( x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, bufstart );

	*offset = bufstart - buffer;
	*padlen = PAD(linelen, packAlign) - linelen;

	return buffer;
#endif
}


/*
==================
RB_TakeScreenshot
==================
*/
void RB_TakeScreenshot( int x, int y, int width, int height, const char *fileName )
{
	const int header_size = 18;
	byte *allbuf, *buffer;
	byte *srcptr, *destptr;
	byte *endline, *endmem;
	byte temp;
	int linelen, padlen;
	size_t offset, memcount;

	offset = header_size;
	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen, 0 );
	buffer = allbuf + offset - header_size;

	memset( buffer, 0, header_size );
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr and remove padding from line endings
	linelen = width * 3;

	srcptr = destptr = allbuf + offset;
	endmem = srcptr + (linelen + padlen) * height;

	while(srcptr < endmem)
	{
		endline = srcptr + linelen;

		while(srcptr < endline)
		{
			temp = srcptr[0];
			*destptr++ = srcptr[2];
			*destptr++ = srcptr[1];
			*destptr++ = temp;

			srcptr += 3;
		}

		// Skip the pad
		srcptr += padlen;
	}

	memcount = linelen * height;

	// gamma correction
	R_GammaCorrect( allbuf + offset, memcount );

	ri.FS_WriteFile( fileName, buffer, memcount + header_size );

	ri.Hunk_FreeTempMemory( allbuf );
}


/*
==================
RB_TakeScreenshotJPEG
==================
*/
void RB_TakeScreenshotJPEG( int x, int y, int width, int height, const char *fileName )
{
	byte *buffer;
	size_t offset = 0, memcount;
	int padlen;

	buffer = RB_ReadPixels(x, y, width, height, &offset, &padlen, 0);
	memcount = (width * 3 + padlen) * height;

	// gamma correction
	R_GammaCorrect( buffer + offset, memcount );

	ri.CL_SaveJPG( fileName, r_screenshotJpegQuality->integer, width, height, buffer + offset, padlen );
	ri.Hunk_FreeTempMemory( buffer );
}


static void FillBMPHeader( byte *buffer, int width, int height, int memcount, int header_size )
{
	memset( buffer, 0, header_size );

	// bitmap file header
	buffer[0] = 'B';
	buffer[1] = 'M';
	int filesize = memcount + header_size;
	buffer[2] = (filesize >> 0) & 255;
	buffer[3] = (filesize >> 8) & 255;
	buffer[4] = (filesize >> 16) & 255;
	buffer[5] = (filesize >> 24) & 255;
	buffer[10] = header_size; // data offset

	// bitmap info header
	buffer[14] = 40; // size of this header
	buffer[18] = (width >> 0) & 255;
	buffer[19] = (width >> 8) & 255;
	buffer[20] = (width >> 16) & 255;
	buffer[21] = (width >> 24) & 255;

	buffer[22] = (height >> 0) & 255;
	buffer[23] = (height >> 8) & 255;
	buffer[24] = (height >> 16) & 255;
	buffer[25] = (height >> 24) & 255;
	buffer[26] = 1; // number of color planes
	buffer[28] = 24; // bpp

	buffer[34] = (memcount >> 0) & 255;
	buffer[35] = (memcount >> 8) & 255;
	buffer[36] = (memcount >> 16) & 255;
	buffer[37] = (memcount >> 24) & 255;
	buffer[38] = 0xC4; // horizontal dpi
	buffer[39] = 0x0E; // horizontal dpi
	buffer[42] = 0xC4; // vertical dpi
	buffer[43] = 0x0E; // vertical dpi
}


/*
==================
RB_TakeScreenshotBMP
==================
*/
void RB_TakeScreenshotBMP( int x, int y, int width, int height, const char *fileName, int clipboardOnly )
{
	byte *allbuf;
	byte *buffer; // destination buffer
	byte *srcptr, *srcline;
	byte *destptr, *dstline;
	byte *endmem;
	byte temp[4];
	size_t memcount, offset;
	const int header_size = 54; // bitmapfileheader(14) + bitmapinfoheader(40)
	int scanlen, padlen;
	int scanpad, len;

	offset = header_size;

	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen, 4 );
	buffer = allbuf + offset;

	// scanline length
	scanlen = PAD( width*3, 4 );
	scanpad = scanlen - width*3;
	memcount = scanlen * height;

	// swap rgb to bgr and add line padding
	if ( scanpad == 0 && padlen == 0 ) {
		// fastest case
		srcptr = destptr = allbuf + offset;
		endmem = srcptr + scanlen * height;
		while ( srcptr < endmem ) {
			temp[0] = srcptr[0];
			destptr[0] = srcptr[2];
			destptr[2] = temp[0];
			destptr += 3;
			srcptr += 3;
		}
	} else {
		// move destination buffer forward if source padding is greater than for BMP
		if ( padlen > scanpad )
			buffer += (width * 3 + padlen - scanlen ) * height;
		// point on last line
		srcptr = allbuf + offset + (height-1) * (width * 3 + padlen);
		destptr = buffer + (height-1) * scanlen;
		len = (width * 3 - 3);
		while ( destptr >= buffer ) {
			srcline = srcptr + len;
			dstline = destptr + len;
			while ( srcline >= srcptr ) {
				temp[2] = srcline[0];
				temp[1] = srcline[1];
				temp[0] = srcline[2];
				dstline[0] = temp[0];
				dstline[1] = temp[1];
				dstline[2] = temp[2];
				dstline-=3;
				srcline-=3;
			}
			srcptr -= (width * 3 + padlen);
			destptr -= scanlen;
		}
	}

	// fill this last to avoid data overwrite in case when we're moving destination buffer forward
	FillBMPHeader( buffer - header_size, width, height, memcount, header_size );

	// gamma correction
	R_GammaCorrect( buffer, memcount );

	if ( clipboardOnly ) {
		// copy starting from bitmapinfoheader
		ri.Sys_SetClipboardBitmap( buffer - 40, memcount + 40 );
	} else {
		ri.FS_WriteFile( fileName, buffer - header_size, memcount + header_size );
	}

	ri.Hunk_FreeTempMemory( allbuf );
}


/*
==================
RB_TakeScreenshotPNG

PNG encoder via tr_image_png_write.c. Inputs are the
RB_ReadPixels output convention (bottom-up RGB, no padding). When
clipboardOnly is non-zero, the encoded PNG bytes go to
ri.Sys_SetClipboardImagePNG instead of FS_WriteFile — the cross-platform
clipboard path that lifts the earlier Windows-only restriction.
==================
*/
void RB_TakeScreenshotPNG( int x, int y, int width, int height, const char *fileName, int clipboardOnly )
{
	byte	*allbuf;
	byte	*buffer;
	byte	*pngBytes = NULL;
	int		 pngLen = 0;
	size_t	 offset = 0;
	int		 padlen;

	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen, 0 );
	buffer = allbuf + offset;

	// vk_read_pixels produces tightly-packed RGB with no padding; if the
	// GL path ever introduces padding here, compact in-place first.
	if ( padlen != 0 ) {
		byte *src = buffer;
		byte *dst = buffer;
		int  i;
		for ( i = 0; i < height; i++ ) {
			if ( src != dst ) memmove( dst, src, width * 3 );
			src += width * 3 + padlen;
			dst += width * 3;
		}
	}

	// gamma correction (matches existing TGA/BMP path)
	R_GammaCorrect( buffer, (size_t)width * 3 * height );

	if ( !R_EncodePNG( buffer, width, height, &pngBytes, &pngLen ) ) {
		R_LOG( rch_cmd, SEV_WARN, "RB_TakeScreenshotPNG: encode failed\n" );
		ri.Hunk_FreeTempMemory( allbuf );
		return;
	}

	if ( clipboardOnly ) {
		ri.Sys_SetClipboardImagePNG( pngBytes, pngLen );
	} else {
		ri.FS_WriteFile( fileName, pngBytes, pngLen );
	}

	ri.Free( pngBytes );
	ri.Hunk_FreeTempMemory( allbuf );
}


/*
====================
R_LevelShot

levelshots are specialized 128*128 thumbnails for
the menu system, sampled down from full screen distorted images

Non-static — called by the shared screenshot grammar (renderercommon/
tr_screenshot.c) via the R_LevelShot hook.
====================
*/
void R_LevelShot( void ) {
	char		checkname[MAX_OSPATH];
	byte		*buffer;
	byte		*source, *allsource;
	byte		*src, *dst;
	size_t		offset = 0;
	int			padlen;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	Com_sprintf(checkname, sizeof(checkname), "levelshots/%s.tga", tr.world->baseName);

	allsource = RB_ReadPixels(0, 0, gls.captureWidth, gls.captureHeight, &offset, &padlen, 0 );
	source = allsource + offset;

	buffer = ri.Hunk_AllocateTempMemory(128 * 128*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = 128;
	buffer[14] = 128;
	buffer[16] = 24;	// pixel size

	// resample from source — drive the scale and row stride from the SAME
	// dimensions RB_ReadPixels filled (gls.captureWidth/Height), not
	// glConfig.vidWidth/Height: when supersampling / render-scale makes those
	// differ, the old stride read past the captured buffer.
	xScale = gls.captureWidth / 512.0f;
	yScale = gls.captureHeight / 384.0f;
	for ( y = 0 ; y < 128 ; y++ ) {
		for ( x = 0 ; x < 128 ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + (3 * gls.captureWidth + padlen) * (int)((y*3 + yy) * yScale) +
						3 * (int) ((x*4 + xx) * xScale);
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * 128 + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correction
	R_GammaCorrect( buffer + 18, 128 * 128 * 3 );

	ri.FS_WriteFile( checkname, buffer, 128 * 128*3 + 18 );

	ri.Hunk_FreeTempMemory(buffer);
	ri.Hunk_FreeTempMemory(allsource);

	R_LOG( rch_cmd, SEV_INFO, "Wrote %s\n", checkname );
}


/*
==================
RB_ScheduleScreenshot

Screenshot grammar hook (see renderercommon/tr_screenshot.h). The shared
grammar resolves (typeMask, fileName, silent); this schedules the capture
in the Vulkan backend via the screenshotMask model — the actual write
happens at end of RE_EndFrame. Returns qfalse when the screenshot cannot
be taken (minimized with no FBO) or is already scheduled for that format.
==================
*/
qboolean RB_ScheduleScreenshot( int typeMask, const char *fileName, qboolean silent ) {
	if ( ri.CL_IsMinimized() && !RE_CanMinimize() ) {
		R_LOG( rch_cmd, SEV_WARN, "WARNING: unable to take screenshot when minimized because FBO is not available/enabled.\n" );
		return qfalse;
	}

	// already-scheduled gate (per-format)
	if ( backEnd.screenshotMask & ( typeMask & ( SCREENSHOT_TGA | SCREENSHOT_JPG | SCREENSHOT_BMP | SCREENSHOT_PNG ) ) )
		return qfalse;

	backEnd.screenshotMask |= typeMask;
	if ( typeMask & SCREENSHOT_JPG ) {
		backEnd.screenShotJPGsilent = silent;
		Q_strncpyz( backEnd.screenshotJPG, fileName, sizeof( backEnd.screenshotJPG ) );
	} else if ( typeMask & SCREENSHOT_BMP ) {
		backEnd.screenShotBMPsilent = silent;
		Q_strncpyz( backEnd.screenshotBMP, fileName, sizeof( backEnd.screenshotBMP ) );
	} else if ( typeMask & SCREENSHOT_PNG ) {
		backEnd.screenShotPNGsilent = silent;
		Q_strncpyz( backEnd.screenshotPNG, fileName, sizeof( backEnd.screenshotPNG ) );
	} else {
		backEnd.screenShotTGAsilent = silent;
		Q_strncpyz( backEnd.screenshotTGA, fileName, sizeof( backEnd.screenshotTGA ) );
	}
	return qtrue;
}


//============================================================================

/*
==================
RB_TakeVideoFrameCmd
==================
*/
const void *RB_TakeVideoFrameCmd( const void *data )
{
	const videoFrameCommand_t *cmd;
	byte		*cBuf;
	size_t		memcount, linelen;
	int			padwidth, avipadwidth, padlen, avipadlen;
	int			packAlign;

	cmd = (const videoFrameCommand_t *)data;

#ifdef USE_VULKAN
	packAlign = 1;
#else
	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);
#endif

	linelen = cmd->width * 3;

	// Alignment stuff for glReadPixels
	padwidth = PAD(linelen, packAlign);
	padlen = padwidth - linelen;
	// AVI line padding
	avipadwidth = PAD(linelen, AVI_LINE_PADDING);
	avipadlen = avipadwidth - linelen;

	cBuf = PADP(cmd->captureBuffer, packAlign);

#ifdef USE_VULKAN
	vk_read_pixels(cBuf, cmd->width, cmd->height);
#else
	qglReadPixels(0, 0, cmd->width, cmd->height, GL_RGB, GL_UNSIGNED_BYTE, cBuf);
#endif

	memcount = padwidth * cmd->height;

	// gamma correction
	R_GammaCorrect( cBuf, memcount );

	if ( cmd->motionJpeg )
	{
		memcount = ri.CL_SaveJPGToBuffer( cmd->encodeBuffer, linelen * cmd->height,
			r_aviMotionJpegQuality->integer,
			cmd->width, cmd->height, cBuf, padlen );
		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, memcount);
	}
	else
	{
		byte *lineend, *memend;
		byte *srcptr, *destptr;

		srcptr = cBuf;
		destptr = cmd->encodeBuffer;
		memend = srcptr + memcount;

		// swap R and B and remove line paddings
		while(srcptr < memend)
		{
			lineend = srcptr + linelen;
			while(srcptr < lineend)
			{
				*destptr++ = srcptr[2];
				*destptr++ = srcptr[1];
				*destptr++ = srcptr[0];
				srcptr += 3;
			}

			memset(destptr, '\0', avipadlen);
			destptr += avipadlen;

			srcptr += padlen;
		}

		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, avipadwidth * cmd->height);
	}

	return (const void *)(cmd + 1);
}


//============================================================================

/*
** GL_SetDefaultState
*/
static void GL_SetDefaultState( void )
{
#ifdef USE_VULKAN
	GL_TextureMode( r_textureMode->string );

	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;
#else
	glState.currenttmu = 0;
	glState.currentArray = 0;

	for ( int i = 0; i < MAX_TEXTURE_UNITS; i++ )
	{
		glState.currenttextures[ i ] = 0;
		glState.glClientStateBits[ i ] = 0;
	}

	qglClearDepth( 1.0f );

	qglCullFace( GL_FRONT );
	glState.faceCulling = -1;

	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	// initialize downstream texture unit if we're running
	// in a multitexture environment
	if ( qglActiveTextureARB )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		GL_TextureMode( r_textureMode->string );
		GL_TexEnv( GL_MODULATE );
		qglDisable( GL_TEXTURE_2D );
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		qglActiveTextureARB( GL_TEXTURE0_ARB );
	}

	qglEnable( GL_TEXTURE_2D );
	GL_TextureMode( r_textureMode->string );
	GL_TexEnv( GL_MODULATE );

	qglShadeModel( GL_SMOOTH );
	qglDepthFunc( GL_LEQUAL );

	// the vertex array is always enabled, but the color and texture
	// arrays are enabled and disabled around the compiled vertex array call
	qglEnableClientState( GL_VERTEX_ARRAY );

	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglDisableClientState( GL_COLOR_ARRAY );
	qglDisableClientState( GL_NORMAL_ARRAY );

	//
	// make sure our GL state vector is set correctly
	//
	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;

	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglDepthMask( GL_TRUE );
	qglDisable( GL_DEPTH_TEST );
	qglEnable( GL_SCISSOR_TEST );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_BLEND );
#endif
}


/*
================
R_PrintLongString

Workaround for ri.Printf's 1024 characters buffer limit.
================
*/
static void R_PrintLongString(const char *string) {
	char buffer[1024];
	const char *p;
	int size = strlen(string);

	p = string;
	while(size > 0)
	{
		Q_strncpyz(buffer, p, sizeof (buffer) );
		R_LOG( rch_init, SEV_DEBUG, "%s", buffer );
		p += 1023;
		size -= 1023;
	}
}


/*
================
GfxInfo

Prints persistent rendering configuration
================
*/
static void GfxInfo( void )
{
	const char *fsstrings[] = { "windowed", "fullscreen" };
	const char *fs;
	int mode;
#ifdef USE_VULKAN
	R_LOG( rch_init, SEV_INFO, "\n" );
	R_LOG( rch_init, SEV_INFO, "VK_VENDOR: %s\n", glConfig.vendor_string );
	R_LOG( rch_init, SEV_INFO, "VK_RENDERER: %s\n", glConfig.renderer_string );
	R_LOG( rch_init, SEV_INFO, "VK_VERSION: %s\n", glConfig.version_string );

	if ( vk.driverNote[0] != '\0' )
	{
		R_LOG( rch_init, SEV_INFO, "%s", vk.driverNote );
	}

	R_LOG( rch_init, SEV_INFO, "\n" );
	R_LOG( rch_init, SEV_INFO, "VK_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	R_LOG( rch_init, SEV_INFO, "VK_MAX_TEXTURE_UNITS: %d\n", glConfig.numTextureUnits );
#else
	const char *enablestrings[] = { "disabled", "enabled" };

	R_LOG( rch_init, SEV_INFO, "\n" );
	R_LOG( rch_init, SEV_INFO, "GL_VENDOR: %s\n", glConfig.vendor_string );
	R_LOG( rch_init, SEV_INFO, "GL_RENDERER: %s\n", glConfig.renderer_string );
	R_LOG( rch_init, SEV_INFO, "GL_VERSION: %s\n", glConfig.version_string );
	R_LOG( rch_init, SEV_DEBUG, "GL_EXTENSIONS: " );
	R_PrintLongString( glConfig.extensions_string );
	R_LOG( rch_init, SEV_INFO, "\n" );
	R_LOG( rch_init, SEV_INFO, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	R_LOG( rch_init, SEV_INFO, "GL_MAX_TEXTURE_UNITS_ARB: %d\n", glConfig.numTextureUnits );
#endif

	R_LOG( rch_init, SEV_INFO, "\n" );
	R_LOG( rch_init, SEV_INFO, "PIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
#ifdef USE_VULKAN
	R_LOG( rch_init, SEV_INFO, " presentation: %s\n", vk_format_string( vk.present_format.format ) );
	if ( vk.color_format != vk.present_format.format ) {
		R_LOG( rch_init, SEV_INFO, " color: %s\n", vk_format_string( vk.color_format ) );
	}
	if ( vk.capture_format != vk.present_format.format || vk.capture_format != vk.color_format ) {
		R_LOG( rch_init, SEV_INFO, " capture: %s\n", vk_format_string( vk.capture_format ) );
	}
	R_LOG( rch_init, SEV_INFO, " depth: %s\n", vk_format_string( vk.depth_format ) );

	// HDR pipeline state snapshot (captured during
	// setup_surface_formats + vk_create_attachments). Extends the
	// PIXELFORMAT block above with the full HDR plumbing —
	// requested vs. effective r_hdr, SFLOAT capability, bloom
	// format / pass count.
	R_LOG( rch_init, SEV_INFO, "\n" );
	vk_hdr_state_print();
#endif
	if ( glConfig.isFullscreen )
	{
		const char *modefs = ri.Cvar_VariableString( "r_modeFullscreen" );
		if ( *modefs )
			mode = atoi( modefs );
		else
			mode = ri.Cvar_VariableIntegerValue( "r_mode" );
		fs = fsstrings[1];
	}
	else
	{
		mode = ri.Cvar_VariableIntegerValue( "r_mode" );
		fs = fsstrings[0];
	}

	if ( glConfig.vidWidth != gls.windowWidth || glConfig.vidHeight != gls.windowHeight )
	{
		R_LOG( rch_init, SEV_INFO, "RENDER: %d x %d, MODE: %d, %d x %d %s hz:", glConfig.vidWidth, glConfig.vidHeight, mode, gls.windowWidth, gls.windowHeight, fs );
	}
	else
	{
		R_LOG( rch_init, SEV_INFO, "MODE: %d, %d x %d %s hz:", mode, gls.windowWidth, gls.windowHeight, fs );
	}

	if ( glConfig.displayFrequency )
	{
		R_LOG( rch_init, SEV_INFO, "%d\n", glConfig.displayFrequency );
	}
	else
	{
		R_LOG( rch_init, SEV_INFO, "N/A\n" );
	}

#ifndef USE_VULKAN
	R_LOG( rch_init, SEV_INFO, "multitexture: %s\n", enablestrings[qglActiveTextureARB != 0] );
	R_LOG( rch_init, SEV_INFO, "compiled vertex arrays: %s\n", enablestrings[qglLockArraysEXT != 0 ] );
	R_LOG( rch_init, SEV_INFO, "texenv add: %s\n", enablestrings[glConfig.textureEnvAddAvailable != 0] );
	R_LOG( rch_init, SEV_INFO, "compressed textures: %s\n", enablestrings[glConfig.textureCompression!=TC_NONE] );
#endif
}


/*
================
VarInfo

Prints info that may change every R_Init() call
================
*/
static void VarInfo( void )
{
	/* overbright bits removed; r_brightness drives the
	 * pre-tonemap exposure_bias spec constant. Log records hardware-
	 * vs-software gamma path only. */
	if ( glConfig.deviceSupportsGamma ) {
		R_LOG( rch_init, SEV_DEBUG, "GAMMA: hardware path\n" );
	} else {
		R_LOG( rch_init, SEV_DEBUG, "GAMMA: software path\n" );
	}

	R_LOG( rch_init, SEV_INFO, "texturemode: %s\n", r_textureMode->string );
	R_LOG( rch_init, SEV_INFO, "texture bits: %d\n", r_textureBits->integer ? r_textureBits->integer : 32 );
	R_LOG( rch_init, SEV_INFO, "picmip: %d%s\n", r_picmip->integer, r_nomip->integer ? ", worldspawn only" : "" );

#ifdef USE_VULKAN
	if ( r_vertexLight->integer ) {
		R_LOG( rch_init, SEV_INFO, "HACK: using vertex lightmap approximation\n" );
	}
#else
	if ( r_vertexLight->integer || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		R_LOG( rch_init, SEV_INFO, "HACK: using vertex lightmap approximation\n" );
	} else if ( glConfig.hardwareType == GLHW_RAGEPRO ) {
		R_LOG( rch_init, SEV_INFO, "HACK: ragePro approximations\n" );
	} else if ( glConfig.hardwareType == GLHW_RIVA128 ) {
		R_LOG( rch_init, SEV_INFO, "HACK: riva128 approximations\n" );
	}
#endif
	if ( r_finish->integer ) {
		R_LOG( rch_init, SEV_INFO, "Forcing glFinish\n" );
	}
}


/*
===============
GfxInfo_f
===============
*/
static void GfxInfo_f( void )
{
	GfxInfo();
	VarInfo();
}


#ifdef USE_VULKAN
static void VkInfo_f( void )
{
	R_LOG( rch_init, SEV_INFO, "max_vertex_usage: %iKb\n", (int)((vk.stats.vertex_buffer_max + 1023) / 1024) );
	R_LOG( rch_init, SEV_INFO, "max_push_size: %ib\n", vk.stats.push_size_max );

	R_LOG( rch_init, SEV_INFO, "pipeline handles: %i\n", vk.pipeline_create_count );
	R_LOG( rch_init, SEV_INFO, "pipeline descriptors: %i, base: %i\n", vk.pipelines_count, vk.pipelines_world_base );
	R_LOG( rch_init, SEV_INFO, "image chunks: %i\n", vk_world.num_image_chunks );
}
#endif


/*
===============
RE_SyncRender
===============
*/
static void RE_SyncRender( void )
{
#ifdef USE_VULKAN
	if ( vk.device )
		vk_wait_idle();
#else
	if ( qglFinish && backEnd.doneSurfaces )
		qglFinish();
#endif
}


qboolean R_ShadowDlightActive( void ) {
	// dlight-omni stays its own LATCH sub-toggle: it renders only from `cast` up AND
	// when r_dlightShadows is on, keeping its 6-pass-per-frame cost opt-in.
	if ( !r_shadows->integer )
		return qfalse;
	return ( r_dlightShadows && r_dlightShadows->integer ) ? qtrue : qfalse;
}

/*
===============
R_Register
===============
*/
static void R_Register( void )
{
	// the screenshot command + sibling introspection commands
	// (imagelist, testdds, shaderlist, skinlist, modellist, screenshot,
	// gfxinfo, bspdump, vkinfo, ral_textures)
	// were registered here and deregistered in the RE_Shutdown loop, which
	// meant they vanished during the REF_LEVEL_ONLY window of every map
	// transition. They are now registered once per renderer-DLL load via
	// RE_RegisterPersistentCommands (called from GetRefAPI) and deregistered
	// only when the DLL is about to be unloaded (RE_Shutdown's
	// code != REF_LEVEL_ONLY path).

	// Per-build stamp (CVAR_ROM) — this renderer DLL's OWN embedded id/date, read
	// by the engine's `sysinfo` for the renderer row. The DLL is compiled with its
	// own copy of WIRED_BUILD_ID/DATE, so a stale renderer self-reports an older
	// stamp than the engine. ri.Cvar_Get is the renderer→engine cvar route (no ABI
	// change), mirroring how gfxinfo state is surfaced.
	ri.Cvar_Get( "r_buildId",   WIRED_BUILD_ID_STR, CVAR_ROM );
	ri.Cvar_Get( "r_buildDate", WIRED_BUILD_DATE,   CVAR_ROM );

	//
	// temporary latched variables that can only change over a restart
	//
	r_fullbright = ri.Cvar_Get( "r_fullbright", "0", CVAR_LATCH );
	ri.Cvar_SetDescription( r_fullbright, "Debugging tool to render the entire level without lighting." );

	// r_brightness is a runtime-live pre-tonemap exposure multiplier
	// (tonemap.frag spec constant id 1, applied on linear-radiance scene
	// values before the tonemap operator). CVG_RENDERER group membership
	// wires it into tr_cmds.c's per-frame check that rebakes the
	// post-process pipeline spec constants on modificationCount change;
	// no vid_restart required.
	//
	// default is 1.0 — the true
	// identity for the now self-consistently linear pipeline. (During an
	// earlier transition window the plan was an interim 0.85 to take
	// the edge off the still-gamma-byte stock-map texel path; that window
	// closed when the worldLinearize* / srgb gate was retired,
	// so 1.0 is the committed value.) Eser tunes from console
	// if the scene reads hot on visual verification.
	r_brightness = ri.Cvar_Get( "r_brightness", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_brightness, "0.25", "32", CV_FLOAT );
	ri.Cvar_SetDescription( r_brightness,
		"Pre-tonemap exposure multiplier.\n"
		"Range 0.25-32, default 1.0 (no boost).\n"
		"Values >1 brighten the scene; tonemap operator\n"
		"rolls off highlights smoothly instead of clipping.\n"
		"Values <1 darken atmospherically.\n"
		"Live: takes effect on next frame via the renderer\n"
		"post-process pipeline group." );
	ri.Cvar_SetGroup( r_brightness, CVG_RENDERER );

	r_intensity = ri.Cvar_Get( "r_intensity", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_intensity, "1", "255", CV_FLOAT );
	ri.Cvar_SetDescription( r_intensity,
		"Global texture lighting scale (range 1-255, default 1 = off).\n"
		"Legacy brighten-only byte-space multiply baked into sRGB texture\n"
		"bytes at upload (R_LightScaleTexture); not domain-correct as a\n"
		"linear lighting multiplier — kept as-is for backward compatibility\n"
		"with user-calibrated values. Vid_restart required." );
	r_singleShader = ri.Cvar_Get( "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_singleShader, "Debugging tool that only uses the default shader for all rendering." );
	r_defaultImage = ri.Cvar_Get( "r_defaultImage", "", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_defaultImage, "Replace default (missing) image texture by either exact file or solid #rgb|#rrggbb background color." );

	r_simpleMipMaps = ri.Cvar_Get( "r_simpleMipMaps", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_simpleMipMaps, "Whether or not to use a simple mipmapping algorithm or a more correct one:\n 0: off (proper linear filter)\n 1: on (for slower machines)" );
	r_vertexLight = ri.Cvar_Get( "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_vertexLight, "Set to 1 to use vertex light instead of lightmaps, collapse all multi-stage shaders into single-stage ones, might cause rendering artifacts." );

	r_picmip = ri.Cvar_Get( "r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_picmip, "0", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_picmip, "Set texture quality, lower is better." );

	r_nomip = ri.Cvar_Get( "r_nomip", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_nomip, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_nomip, "Apply picmip only on worldspawn textures." );

	r_neatsky = ri.Cvar_Get( "r_neatsky", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_neatsky, "Disables texture mipping for skies." );
	r_roundImagesDown = ri.Cvar_Get ("r_roundImagesDown", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_roundImagesDown, "When images are scaled, round images down instead of up." );
	r_colorMipLevels = ri.Cvar_Get ("r_colorMipLevels", "0", CVAR_LATCH );
	ri.Cvar_SetDescription( r_colorMipLevels, "Debugging tool to artificially color different mipmap levels so that they are more apparent." );
	r_detailTextures = ri.Cvar_Get( "r_detailtextures", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_detailTextures, "Enables usage of shader stages flagged as detail." );
	r_textureBits = ri.Cvar_Get( "r_textureBits", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_textureBits, "Number of texture bits per texture." );

	// when 1 (default), R_CreateImage creates a parallel RAL
	// r_useRALTextures / r_useRALBuffers /
	// r_useRALPipelines retired; RAL backend is unconditional now
	// (the swapchain itself is a RAL consumer).

	r_mergeLightmaps = ri.Cvar_Get( "r_mergeLightmaps", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_mergeLightmaps, "Merge built-in small lightmaps into bigger lightmaps (atlases)." );

	// lightmap atlas optimization alias. In the Vulkan renderer the atlas
	// is already implemented by R_LoadMergedLightmaps() in tr_map.c,
	// but we expose r_lightmapAtlas as an alias so the cvar name matches
	// CNQ3 and the GL1 renderer. Both cvars must be non-zero for the
	// atlas pack to be built. Vulkan descriptor sets still benefit from
	// fewer image views, so leave this on by default.
	r_lightmapAtlas = ri.Cvar_Get( "r_lightmapAtlas", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_lightmapAtlas,
		"Pack individual BSP lightmaps into larger atlas textures.\n"
		" 1: pack into atlases (default, fewer image/descriptor allocations)\n"
		" 0: one image per lightmap (legacy behaviour)\n"
		" Works together with r_mergeLightmaps." );

	// cap presentation while map is loading to reduce CPU/GPU contention
	// with BSP parse, lightmap upload, shader compile.
	r_loadingFpsCap = ri.Cvar_Get( "r_loadingFpsCap", "10", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_loadingFpsCap, "0", "60", CV_INTEGER );
	ri.Cvar_SetDescription( r_loadingFpsCap,
		"Maximum render backend frames per second while loading a map.\n"
		" 0: no cap (legacy behaviour)\n"
		" 1-60: cap presentation to this rate. Default 10." );
#if defined (USE_VULKAN) && defined (USE_VBO)
	r_vbo = ri.Cvar_Get( "r_vbo", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_vbo, "Use Vertex Buffer Objects to cache static map geometry, may improve FPS on modern GPUs, increases hunk memory usage by 15-30MB (map-dependent)." );
#endif

	// r_mapSaturation is baked into world texture pixel data and
	// fog vertex colors at BSP load (R_FindImageFile, R_LoadFogs).
	// Lightmaps are NOT affected — see r_lightmapSaturation.
	// Vid_restart required.
	r_mapSaturation = ri.Cvar_Get( "r_mapSaturation", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_mapSaturation, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_mapSaturation,
		"World texture and fog saturation multiplier baked\n"
		"into pixel data at BSP load. Range 0.0-2.0,\n"
		"default 1.0.\n"
		"  0.0 = grayscale\n"
		"  1.0 = full color (identity)\n"
		"  2.0 = super-saturated (clamps on 8-bit)\n"
		"Vid_restart required. Independent of\n"
		"r_lightmapSaturation and r_saturation." );

	// r_lightmapSaturation is baked into lightmap pixel data at BSP
	// load (R_ColorShiftLightingBytes). World textures and fog are
	// NOT affected — see r_mapSaturation. Vid_restart required.
	r_lightmapSaturation = ri.Cvar_Get( "r_lightmapSaturation", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_lightmapSaturation, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_lightmapSaturation,
		"Lightmap saturation multiplier baked into pixel\n"
		"data at BSP load. Range 0.0-2.0, default 1.0.\n"
		"  0.0 = grayscale\n"
		"  1.0 = full color (identity)\n"
		"  2.0 = super-saturated (clamps on 8-bit)\n"
		"Vid_restart required. Independent of\n"
		"r_mapSaturation and r_saturation." );

	// World-lightmap overbright. The base-pass world shader multiplies
	// diffuse × lightmap × r_lightmapBoost in LINEAR space. Vanilla Quake 3
	// applied its overbright (×2, or ×4 with r_overBrightBits) in GAMMA space,
	// which — because the value is sRGB-encoded for display afterwards — yields a
	// LARGER effective brightening than the same factor applied in linear space:
	// a linear ×N reads as N^(1/2.2) at display. So matching vanilla's gamma-space
	// ×2 needs a linear boost of 2^2.2 ≈ 4.6 (the default); a linear 2.0 would only
	// reach 2^(1/2.2) ≈ 1.37× and leaves the world ~1.46× darker than vanilla.
	// Fed live to the world shader via the per-draw set-0 UBO (NOT a spec constant),
	// so a runtime change takes effect next frame with no pipeline rebuild.
	//   4.6  = vanilla ×2 (default)
	//   ~21  = vanilla ×4 (r_overBrightBits 2; usually too hot)
	r_lightmapBoost = ri.Cvar_Get( "r_lightmapBoost", "4.6", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_lightmapBoost, "1", "24", CV_FLOAT );
	ri.Cvar_SetDescription( r_lightmapBoost,
		"World-lightmap overbright (linear-domain boost equivalent of vanilla\n"
		"overbright-bits). diffuse × lightmap × this, in linear space.\n"
		"  4.6  = vanilla ×2 (default; 2^2.2 — matches vanilla brightness)\n"
		"  ~21  = vanilla ×4 (usually too hot)\n"
		"  2.0  = the old under-brightened value (~1.46× darker than vanilla)\n"
		"Range 1.0-24.0. Requires \\r_fbo 1. Live: takes effect next frame." );

	r_subdivisions = ri.Cvar_Get( "r_subdivisions", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription(r_subdivisions, "Distance to subdivide bezier curved surfaces. Higher values mean less subdivision and less geometric complexity.");

	r_maxpolys = ri.Cvar_Get( "r_maxpolys", XSTRING( MAX_POLYS ), CVAR_LATCH );
	// Bound the upper end (and forbid <= 0): these feed sizeof(srfPoly_t)*max_polys
	// + sizeof(polyVert_t)*max_polyverts in R_Init; an unclamped huge/negative
	// value overflows that multiply or requests an absurd allocation.
	ri.Cvar_CheckRange( r_maxpolys, XSTRING( MAX_POLYS ), "262144", CV_INTEGER );
	ri.Cvar_SetDescription( r_maxpolys, "Maximum number of polygons to draw in a scene." );
	r_maxpolyverts = ri.Cvar_Get( "r_maxpolyverts", XSTRING( MAX_POLYVERTS ), CVAR_LATCH );
	ri.Cvar_CheckRange( r_maxpolyverts, XSTRING( MAX_POLYVERTS ), "1048576", CV_INTEGER );
	ri.Cvar_SetDescription( r_maxpolyverts, "Maximum number of polygon vertices to draw in a scene." );

	//
	// archived variables that can change at any time
	//
	r_lodCurveError = ri.Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_lodCurveError, "-1", "8192", CV_FLOAT );
	ri.Cvar_SetDescription( r_lodCurveError, "Level of detail error on curved surface grids. Higher values result in better quality at a distance." );
	r_lodbias = ri.Cvar_Get( "r_lodbias", "-2", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_lodbias, "Sets the level of detail of in-game models:\n -2: Ultra (further delays LOD transition in the distance)\n -1: Very High (delays LOD transition in the distance)\n 0: High\n 1: Medium\n 2: Low" );
	r_flares = ri.Cvar_Get ("r_flares", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_flares, "Enables halo effects on light sources." );
	r_lens = ri.Cvar_Get ("r_lens", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_lens, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_lens, "Depth-sampled lens occlusion oracle for flares (replaces the per-flare dot-probe with a smooth N-tap visibility). Requires r_fbo 1 + r_flares 1." );
	r_halos = ri.Cvar_Get ("r_halos", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_halos, "Enables direction-independent halo glows on lights, independent of r_flares surface flares." );
	r_znear = ri.Cvar_Get( "r_znear", "4", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_znear, "0.001", "200", CV_FLOAT );
	ri.Cvar_SetDescription( r_znear, "Viewport distance from view origin (how close objects can be to the player before they're clipped out of the scene)." );
	r_zproj = ri.Cvar_Get( "r_zproj", "64", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_zproj, "Projected viewport frustum." );
	r_stereoSeparation = ri.Cvar_Get( "r_stereoSeparation", "64", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_stereoSeparation, "Control eye separation. Resulting separation is \\r_zproj divided by this value in standard units." );
	r_ignoreGLErrors = ri.Cvar_Get( "r_ignoreGLErrors", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_ignoreGLErrors, "Ignore OpenGL errors." );
	r_teleporterFlash = ri.Cvar_Get( "r_teleporterFlash", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_teleporterFlash, "Show a white screen instead of a black screen when being teleported in hyperspace." );
	r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_fastsky, "Draw flat colored skies." );
	// sky renders by default (modernized-experience default),
	// uniformly across Q1 (RB_StageIteratorGeneric SURF_SKY path) and Q3
	// (RB_StageIteratorSky). 0 disables sky on all maps.
	r_drawSky = ri.Cvar_Get( "r_drawSky", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_drawSky, "Draw skies (0 disables the sky pass on all maps)." );
	// sun disc renders by default wherever sky renders.
	// tr.sunDirection comes from the BSP (`sun` keyword on Q3) or the Q1
	// per-sky table; 0 disables the disc on all maps.
	r_drawSun = ri.Cvar_Get( "r_drawSun", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_drawSun, "Draw sun shader in skies." );
	// Single 3-value dynamic-light control (per-pixel only; the VQ3 'fake' tier is
	// retired). 0 = off, 1 = per-pixel world/brush surfaces only, 2 = world + MD3
	// models. Default 2 (full quality, including ARM — the old ARM split existed only
	// to default the fake tier, which is gone; the PMLIGHT path is the only one now).
	// Every site that used to ask "is the fake path off / is PMLIGHT on" now asks
	// `!= 0`; every "MD3 too" site asks `== 2`.
	r_dynamiclight = ri.Cvar_Get( "r_dynamiclight", "2", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_dynamiclight, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_dynamiclight, "Dynamic lights:\n 0: off\n 1: per-pixel, world only\n 2: per-pixel, world + models" );
#ifdef USE_PMLIGHT
	r_dlightScale = ri.Cvar_Get( "r_dlightScale", "0.5", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_dlightScale, "0.1", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_dlightScale, "Scales dynamic light radius." );
	r_dlightIntensity = ri.Cvar_Get( "r_dlightIntensity", "1.0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_dlightIntensity, "0.1", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_dlightIntensity, "Adjusts dynamic light intensity but not radius." );
#endif // USE_PMLIGHT

	r_dlightSaturation = ri.Cvar_Get( "r_dlightSaturation", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_dlightSaturation, "0", "1", CV_FLOAT );

	r_dlightBacks = ri.Cvar_Get( "r_dlightBacks", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_dlightBacks, "Whether or not dynamic lights should light up back-face culled geometry, affects only VQ3 dynamic lights." );
	r_finish = ri.Cvar_Get( "r_finish", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_finish, "Force a glFinish call after rendering a scene." );
	r_textureMode = ri.Cvar_Get( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_textureMode, "Texture interpolation mode:\n GL_NEAREST: Nearest neighbor interpolation and will therefore appear similar to Quake II except with the added colored lighting\n GL_LINEAR: Linear interpolation and will appear to blend in objects that are closer than the resolution that the textures are set as\n GL_NEAREST_MIPMAP_NEAREST: Nearest neighbor interpolation with mipmapping for bilinear hardware, mipmapping will blend objects that are farther away than the resolution that they are set as\n GL_LINEAR_MIPMAP_NEAREST: Linear interpolation with mipmapping for bilinear hardware\n GL_NEAREST_MIPMAP_LINEAR: Nearest neighbor interpolation with mipmapping for trilinear hardware\n GL_LINEAR_MIPMAP_LINEAR: Linear interpolation with mipmapping for trilinear hardware" );
	ri.Cvar_SetGroup( r_textureMode, CVG_RENDERER );
	r_gamma = ri.Cvar_Get( "r_gamma", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_gamma, "0.5", "3", CV_FLOAT );
	ri.Cvar_SetDescription( r_gamma, "Gamma correction factor." );
	ri.Cvar_SetGroup( r_gamma, CVG_RENDERER );
	r_facePlaneCull = ri.Cvar_Get ("r_facePlaneCull", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_facePlaneCull, "Enables culling of planar surfaces with back side test." );

	r_ambientScale = ri.Cvar_Get( "r_ambientScale", "0.6", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ambientScale, "Light grid ambient light scaling on entity models." );
	r_directedScale = ri.Cvar_Get( "r_directedScale", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_directedScale, "Light grid direct light scaling on entity models." );

	//r_anaglyphMode = ri.Cvar_Get( "r_anaglyphMode", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	//ri.Cvar_SetDescription( r_anaglyphMode, "Enable rendering of anaglyph images. Valid options for 3D glasses types:\n 0: Disabled\n 1: Red-cyan\n 2: Red-blue\n 3: Red-green\n 4: Green-magenta" );

	// r_saturation drives the post-process saturation multiplier
	// (tonemap.frag spec constant id 2). Range [0, 2]; 1.0 is
	// identity. Below 1.0 desaturates toward grey; above 1.0
	// super-saturates. Live: takes effect on next frame via CVG_RENDERER
	// group rebake.
	// applied POST-tonemap (display domain), so the
	// multiplier is perceptual — 1.0 is the identity regardless of input
	// domain; verified correct for the linear pipeline. No retune.
	r_saturation = ri.Cvar_Get( "r_saturation", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_saturation, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_saturation,
		"Post-process saturation multiplier.\n"
		"Range 0.0-2.0, default 1.0.\n"
		"  0.0 = grayscale\n"
		"  1.0 = full color (identity)\n"
		"  2.0 = super-saturated (may clamp on 8-bit)\n"
		"Requires r_fbo 1. Live: takes effect on next frame." );
	ri.Cvar_SetGroup( r_saturation, CVG_RENDERER );

	r_dither = ri.Cvar_Get( "r_dither", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_dither, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription(r_dither, "Set dithering mode:\n 0 - disabled\n 1 - ordered\n 2 - blue-noise\nRequires " S_COLOR_CYAN "\\r_fbo 1." );

	// Chromatic aberration (lens fringe) strength, 0..1. Opt-in stylistic effect;
	// default 0 = off (zero cost, byte-identical) to keep competitive readability.
	r_chromaticAberration = ri.Cvar_Get( "r_chromaticAberration", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_chromaticAberration, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_chromaticAberration, "Chromatic aberration (radial R/G/B lens fringe) strength, 0..1.\n 0 - off\nRequires " S_COLOR_CYAN "\\r_fbo 1." );
	ri.Cvar_SetGroup( r_chromaticAberration, CVG_RENDERER );

#if FEAT_DEPTH_CLAMP
	r_depthClamp = ri.Cvar_Get( "r_depthClamp", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_depthClamp, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_depthClamp, "Disable near-plane vertex clipping. Prevents seeing through objects at high FOV." );
#endif
	ri.Cvar_SetGroup( r_dither, CVG_RENDERER );

	r_presentBits = ri.Cvar_Get( "r_presentBits", "24", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_presentBits, "16", "30", CV_INTEGER );
	ri.Cvar_SetDescription( r_presentBits, "Select color bits used for presentation surfaces\nRequires " S_COLOR_CYAN "\\r_fbo 1." );

	//
	// temporary variables that can change at any time
	//
	r_showImages = ri.Cvar_Get( "r_showImages", "0", CVAR_TEMP );
	ri.Cvar_SetDescription( r_showImages, "Draw all images currently loaded into memory:\n 0: Disabled\n 1: Show images set to uniform size\n 2: Show images with scaled relative to largest image" );

	// r_layoutDump — visual-regression instrumentation hook. The renderer
	// does not consume this; it is registered here for global visibility so
	// the client-side Wired UI layout pass (cl_wired_layout_dump.c) can read
	// it. When non-zero, every WUI_LayoutMenu pass appends each named item's
	// resolved pixel rect + authored colours to layoutdump.jsonl (in the
	// process working directory). Consumed by tools/visual_regression.
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_layoutDump", "0", CVAR_TEMP ),
		"Visual-regression instrumentation: when non-zero, the Wired UI layout pass appends each named item's resolved rect + authored colours to layoutdump.jsonl (working directory) every frame. 0 = disabled." );

	r_debugLight = ri.Cvar_Get( "r_debugLight", "0", CVAR_TEMP );
	ri.Cvar_SetDescription( r_debugLight, "Debugging tool to print ambient and directed lighting information." );
	r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_debugSort, "Debugging tool to filter out shaders with depth sorting order values higher than the set value." );
	r_printShaders = ri.Cvar_Get( "r_printShaders", "0", 0 );
	ri.Cvar_SetDescription( r_printShaders, "Debugging tool to print on console of the number of shaders used." );
	r_saveFontData = ri.Cvar_Get( "r_saveFontData", "0", 0 );

	r_nocurves = ri.Cvar_Get ("r_nocurves", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_nocurves, "Set to 1 to disable drawing world bezier curves. Set to 0 to enable." );
	r_drawWorld = ri.Cvar_Get ("r_drawWorld", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawWorld, "Set to 0 to disable drawing the world. Set to 1 to enable." );
	r_lightmap = ri.Cvar_Get ("r_lightmap", "0", 0 );
	ri.Cvar_SetDescription( r_lightmap, "Show only lightmaps on all world surfaces." );
	r_portalOnly = ri.Cvar_Get ("r_portalOnly", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_portalOnly, "Set to 1 to render only first portal view if it is present on the scene." );

	r_flareSize = ri.Cvar_Get( "r_flareSize", "22", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_flareSize, "Radius of light flares. Requires \\r_flares 1." );
	ri.Cvar_CheckRange( r_flareSize, "1", "40", CV_FLOAT );

	r_flareFade = ri.Cvar_Get( "r_flareFade", "10", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_flareFade, "Distance to fade out light flares. Requires \\r_flares 1." );
	r_flareCoeff = ri.Cvar_Get( "r_flareCoeff", "150", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_flareCoeff, "0.1", NULL, CV_FLOAT );
	ri.Cvar_SetDescription( r_flareCoeff, "Coefficient for the light flare intensity falloff function. Requires \\r_flares 1." );
	r_flareTarget = ri.Cvar_Get( "r_flareTarget", "1.0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_flareTarget, "0.1", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_flareTarget, "Exposure-invariant target brightness for light flares — the flare reads at a fixed display brightness regardless of auto-exposure. Requires \\r_flares 1." );

	/* c2-shadertime-pin — dev-only override of wall-clock-driven shader
	 * animation time, used to make C2 captures deterministic. Pinning
	 * tr.refdef.{time,floatTime} to a fixed value freezes the input to
	 * tess.shaderTime / EvalWaveForm / R_NoiseGet4f / deform evaluation
	 * → wave-driven emissive surfaces sit at a fixed animation phase
	 * across launches. CVAR_CHEAT (dev-only, non-archive), default 0.0
	 * (= disabled = today's behaviour). The C2 smoke harness sets it
	 * via `+set r_pinShaderTime 1.0`. */
	r_pinShaderTime = ri.Cvar_Get( "r_pinShaderTime", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_pinShaderTime, "Dev/C2-smoke only: pin shader animation time (seconds) for deterministic captures. 0 = off (default, wall-clock-driven gameplay). Non-zero = pin tr.refdef.floatTime to this value." );

#if FEAT_FOG_SYSTEM
	r_useGlFog = ri.Cvar_Get( "r_useGlFog", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_useGlFog, "Use enhanced fog pipeline (Vulkan push constant upload)." );
	r_defaultFogParmsType = ri.Cvar_Get( "r_defaultFogParmsType", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_defaultFogParmsType, "Default fogType_t for maps without explicit fog: 0=linear, 1=exp, 2=exp2." );
	ri.Cvar_CheckRange( r_defaultFogParmsType, "0", "2", CV_INTEGER );
	r_globalLinearFogDrawSky = ri.Cvar_Get( "r_globalLinearFogDrawSky", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_globalLinearFogDrawSky, "Draw sky surfaces through linear global fog (Spearmint compat)." );
#endif

	r_skipBackEnd = ri.Cvar_Get ("r_skipBackEnd", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_skipBackEnd, "Skips loading rendering backend." );

	r_lodscale = ri.Cvar_Get( "r_lodscale", "5", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_lodscale, "Set scale for level of detail adjustment." );
	r_norefresh = ri.Cvar_Get ("r_norefresh", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_norefresh, "Bypasses refreshing of the rendered scene." );
	r_drawEntities = ri.Cvar_Get ("r_drawEntities", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawEntities, "Draw all world entities." );
	r_nocull = ri.Cvar_Get ("r_nocull", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_nocull, "Draw all culled objects." );
	r_gpuBatchDecomp = ri.Cvar_Get( "r_gpuBatchDecomp", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_gpuBatchDecomp, "1 = drive the VBO-eligible world draw from a host frame-current cull re-derivation (CPU per-surface recursion retired); 0 = CPU recursion (default)." );
	r_novis = ri.Cvar_Get ("r_novis", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_novis, "Disables usage of PVS." );
	r_showCluster = ri.Cvar_Get ("r_showCluster", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_showCluster, "Shows current cluster index." );
	r_speeds = ri.Cvar_Get ("r_speeds", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_speeds, "Prints out various debugging stats from PVS:\n 0: Disabled\n 1: Backend BSP\n 2: Frontend grid culling\n 3: Current view cluster index\n 4: Dynamic lighting\n 5: zFar clipping\n 6: Flares" );
	r_gpuSpeeds = ri.Cvar_Get( "r_gpuSpeeds", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_gpuSpeeds, "Per-pass GPU timestamp report.\n 0: off\n 1: 200-frame averages\n N(>=2): only frames where total GPU time >= N ms" );
	// Profiling instrumentation is local and frame-byte-inert; unlike visual cheat
	// cvars it must survive normal local-server map startup so a capture can be
	// armed without enabling gameplay cheats. Default remains off.
	r_profileMarkers = ri.Cvar_Get( "r_profileMarkers", "0", 0 );
	ri.Cvar_CheckRange( r_profileMarkers, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_profileMarkers,
		"Emit semantic RAL dynamic-rendering GPU debug labels when Vulkan debug-utils is available.\n"
		" 0: off (default)\n 1: on\n"
		"Use `ral_dump live markers` for a one-frame backend receipt." );
	r_temporalInputTest = ri.Cvar_Get( "r_temporalInputTest", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_temporalInputTest, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalInputTest,
		"Default-off Phase 7.10 projection-jitter diagnostic. This is not a TAA resolve.\n"
		" 0: canonical unjittered renderer path (default)\n"
		" 1: primary world views consume the backend-neutral temporal sequence\n"
		"Use `ral_dump live temporal` for the latest committed receipt." );
	r_vkDebugTiming = ri.Cvar_Get( "r_vkDebugTiming", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_vkDebugTiming, "Print Vulkan host-side timing averages every 200 frames.\n 0: off\n 1: on (fence, acquire, submit, present, draw calls, pipeline binds)" );
	r_frameSpikeUs = ri.Cvar_Get( "r_frameSpikeUs", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_frameSpikeUs, "Per-frame host-side stage-timing report (CPU side of the Vulkan pipeline).\n 0: off\n N>0: print stage breakdown for each frame whose total host time >= N us\n Recommended: 12000 (>12ms is a perceptible spike at 120Hz)" );
	r_debugSurface = ri.Cvar_Get ("r_debugSurface", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_debugSurface, "Backend visual debugging tool for bezier mesh surfaces." );
	r_nobind = ri.Cvar_Get ("r_nobind", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_nobind, "Backend debugging tool: Disables texture binding." );
	r_showTris = ri.Cvar_Get ("r_showTris", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_showTris, "Debugging tool: Wireframe rendering of polygon triangles in the world." );
	r_showNormals = ri.Cvar_Get( "r_showNormals", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_showNormals, "Debugging tool: Show wireframe surface normals." );
	r_clear = ri.Cvar_Get( "r_clear", "0", 0 );
	ri.Cvar_SetDescription( r_clear, "Forces screen buffer clearing every frame, removing any hall of mirrors effect in void.\n Use \\r_clearColor to set color." );
	r_offsetFactor = ri.Cvar_Get( "r_offsetFactor", "-2", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_offsetFactor, "Offset factor for shaders with polygonOffset stages." );
	r_offsetUnits = ri.Cvar_Get( "r_offsetunits", "-1", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_offsetUnits, "Offset units for shaders with polygonOffset stages." );
	r_drawBuffer = ri.Cvar_Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawBuffer, "Sets which frame buffer to draw into." );
	r_lockpvs = ri.Cvar_Get ("r_lockpvs", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_lockpvs, "Debugging tool: Locks to current potentially visible set. Useful for testing vis-culling in maps." );
	r_noportals = ri.Cvar_Get( "r_noportals", "0", 0 );
	ri.Cvar_SetDescription(r_noportals, "Disables in-game portals, valid values: 0: Portals enabled\n 1: Portals disabled\n 2: Portals and mirrors disabled" );
	r_shadows = ri.Cvar_Get( "r_shadows", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_shadows, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_shadows,
		"Sun-driven cascaded shadow map with PCF filtering: 0 = off, 1 = cast\n"
		" (directional CSM sun-cast; + dlight-omni if \\r_dlightShadows). Requires\n"
		" \\r_fbo 1. Takes effect on the next frame." );
	ri.Cvar_SetGroup( r_shadows, CVG_RENDERER );

	r_marksOnTriangleMeshes = ri.Cvar_Get("r_marksOnTriangleMeshes", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_marksOnTriangleMeshes, "Enables impact marks on triangle mesh surfaces (ie: MD3 models.) Requires impact marks to be enabled in the game code." );

	r_gpuDecals = ri.Cvar_Get( "r_gpuDecals", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_gpuDecals, "Draw the GPU decal ring as surface-aligned projector quads in the main pass. With the ring empty (normal play) this is byte-identical to off." );

	r_atmosphericGPU = ri.Cvar_Get( "r_atmosphericGPU", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_atmosphericGPU, "GPU-resident atmospheric weather (rain/snow): 1 = draw the GPU compute pool (spawn/integrate/collide/draw on the GPU), 0 = skip it (no weather rendered). Gates the renderer's atmospheric compute + draw." );

	r_particles = ri.Cvar_Get( "r_particles", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_particles, "Draw the GPU particle pass (RB_DrawParticles). Also registers particles as a scene-depth consumer so the soft-particle depth fade has a fresh depth copy." );

	r_aviMotionJpegQuality = ri.Cvar_Get( "r_aviMotionJpegQuality", "100", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_aviMotionJpegQuality, "Controls quality of Jpeg video capture when \\cl_aviMotionJpeg 1." );
	r_screenshotJpegQuality = ri.Cvar_Get( "r_screenshotJpegQuality", "100", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_screenshotJpegQuality, "Controls quality of Jpeg screenshots when using screenshotJpeg." );

	// Block 3 (colour closure): bloom-extract is now a soft knee in
	// bloom.frag — extraction = `max(metric(base) - threshold, 0)`
	// (over-threshold *excess*, not the full pixel) across all three
	// threshold modes. The default rebases to 0.32 (≈ sRGBToLinear(0.6),
	// the pre-linear-pipeline 0.6 cutoff in linear radiance): low-radiance
	// pixels contribute nothing, mid-bright surfaces contribute only their
	// over-threshold margin — no more frame-wide blue veil from the sky.
	// r_bloomThresholdMode selects the metric (per-channel / average /
	// luma) that drives the subtraction. The legacy r_bloomModulate cvar
	// and its bloom.frag `base_modulate` spec constant were both removed —
	// the soft knee subsumes that post-tweak.
	r_bloomThreshold = ri.Cvar_Get( "r_bloomThreshold", "0.32", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_bloomThreshold, "Linear-radiance knee below which a pixel contributes nothing to bloom; over-threshold excess is extracted via a soft-knee subtraction. Default 0.32." );
	ri.Cvar_SetGroup( r_bloomThreshold, CVG_RENDERER );

	// Block 3 (colour closure): the three modes now select the *metric*
	// that drives the soft-knee threshold subtraction (over-threshold
	// excess) — not a hard pass/fail gate that emits the full pixel.
	// Default unchanged (0 = per-channel knee).
	r_bloomThresholdMode = ri.Cvar_Get( "r_bloomThresholdMode", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_bloomThresholdMode, "Bloom soft-knee metric:\n 0: per-channel  excess = max(rgb - threshold, 0)\n 1: average      knee on (r+g+b)/3, hue-preserving scale\n 2: luma         knee on Rec.709 luma, hue-preserving scale" );
	ri.Cvar_SetGroup( r_bloomThresholdMode, CVG_RENDERER );

	// blend factor left at 0.5. blend.frag adds
	// factor × Σ(blurred bloom mips) into vk.color_image; additive bloom
	// composites correctly in linear domain (it's the gamma-domain version
	// that was wrong), so the factor's meaning is unchanged. Default unchanged.
	r_bloomIntensity = ri.Cvar_Get( "r_bloomIntensity", "0.5", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_bloomIntensity, "Final bloom blend factor, default is 0.5." );
	ri.Cvar_SetGroup( r_bloomIntensity, CVG_RENDERER );

	// Block 3 (colour closure): r_bloomModulate removed — the bloom soft-
	// knee in bloom.frag replaced its "emphasise the bright part" post-
	// tweak (the matching base_modulate spec constant is gone too).

	if ( glConfig.vidWidth )
		return;

	//
	// latched and archived variables that can only change over a vid_restart
	//
	r_allowExtensions = ri.Cvar_Get( "r_allowExtensions", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_allowExtensions, "Use all of the OpenGL extensions your card is capable of." );
	r_ext_compressed_textures = ri.Cvar_Get( "r_ext_compressed_textures", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ext_compressed_textures, "Enables texture compression." );
	r_ext_multitexture = ri.Cvar_Get( "r_ext_multitexture", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ext_multitexture, "Enables hardware multi-texturing (0: off, 1: on)." );
	r_ext_compiled_vertex_array = ri.Cvar_Get( "r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ext_compiled_vertex_array, "Enables hardware-compiled vertex array rendering method." );
	r_ext_texture_env_add = ri.Cvar_Get( "r_ext_texture_env_add", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ext_texture_env_add, "Enables additive blending in multitexturing. Requires \\r_ext_multitexture 1." );

	r_ext_texture_filter_anisotropic = ri.Cvar_Get( "r_ext_texture_filter_anisotropic",	"1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_texture_filter_anisotropic, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_texture_filter_anisotropic, "Allow anisotropic filtering." );

	r_ext_max_anisotropy = ri.Cvar_Get( "r_ext_max_anisotropy", "8", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_max_anisotropy, "1", NULL, CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_max_anisotropy, "Sets maximum anisotropic level for your graphics driver. Requires \\r_ext_texture_filter_anisotropic." );

	//r_stencilBits = ri.Cvar_Get( "r_stencilBits", "8", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	r_ignorehwgamma = ri.Cvar_Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ignorehwgamma, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ignorehwgamma, "Overrides hardware gamma capabilities." );

	r_showSky = ri.Cvar_Get( "r_showSky", "0", CVAR_LATCH );
	ri.Cvar_SetDescription( r_showSky, "Forces sky in front of all surfaces." );
#ifdef USE_VULKAN
	r_device = ri.Cvar_Get( "r_device", "-1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_device, "-2", NULL, CV_INTEGER );
	ri.Cvar_SetDescription( r_device, "Select physical device to render:\n" \
		" 0+ - use explicit device index\n" \
		" -1 - first discrete GPU\n" \
		" -2 - first integrated GPU" );
	s_r_device_mod = r_device->modificationCount;

	// r_fbo live conversion. Previously this was CVAR_LATCH
	// because flipping the FBO chain on/off rebuilds the swapchain
	// (sRGB selection + present format), every render pass, every
	// framebuffer, every attachment image, and the static post-process
	// pipelines. Now wired through CVG_RENDERER ->
	// vk_update_post_process_pipelines, which detects r_fbo change and
	// calls vk_rebuild_for_fbo_change. Expect a noticeable hitch on the
	// change frame (~50-200 ms — closer to vid_restart than r_hdr's
	// FBO-only rebuild because the swapchain itself is recreated).
	r_fbo = ri.Cvar_Get( "r_fbo", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_fbo, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fbo,
		"Use framebuffer objects: enables gamma correction in windowed mode\n"
		"and allows arbitrary video size and screenshot/video capture.\n"
		"Required for bloom, HDR rendering, anti-aliasing and post-process\n"
		"saturation/tonemap effects.\n"
		"Live: takes effect on next frame; expect a noticeable hitch as the\n"
		"swapchain + FBO chain rebuild." );
	ri.Cvar_SetGroup( r_fbo, CVG_RENDERER );
	// r_hdr live conversion. Previously this was CVAR_LATCH
	// because flipping the color attachment format requires tearing
	// down + recreating the FBO chain (color_image + render_pass.main
	// + dependent pipelines + framebuffers). Now wired through
	// CVG_RENDERER -> vk_update_post_process_pipelines, which detects
	// r_hdr change and calls vk_rebuild_fbo_for_hdr_change. Expect a
	// one-frame hitch on the change frame.
	// legacy-mainpath-retire: r_bindlessMainPath cvar registration
	// retired (the smoke-harness parity gate is gone — there is no longer a
	// legacy main path to A/B against). Caps-based bindless selection is
	// now unconditional; see the cap-decline branch in vk.c:vk_initialize
	// for the recoverable-failure path when bindless is unsupported.

	r_hdr = ri.Cvar_Get( "r_hdr", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdr, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_hdr,
		"High dynamic range frame buffer texture format. Requires \\r_fbo 1.\n"
		"  0: 8 bit BGRA, no HDR pipeline (legacy fallback)\n"
		"  1: 16 bit SFLOAT, true HDR (default), supports values >1.0,\n"
		"     foundation for tonemap, auto-exposure, and PBR shading.\n"
		"  2: 16 bit UNORM, clamped HDR [0,1] range, enhanced precision\n"
		"     but no highlights above white. For GPUs lacking SFLOAT\n"
		"     storage / blend support — engine downgrades 1->2 automatically.\n"
		"Live: takes effect on next frame via the renderer post-process\n"
		"pipeline group; expect a one-frame hitch on the change frame as\n"
		"the FBO color attachment + render passes + pipelines rebuild." );
	ri.Cvar_SetGroup( r_hdr, CVG_RENDERER );

	// HDR10 display output. r_hdr (above) controls the
	// internal scene-buffer FORMAT (R16F linear render target); r_hdrDisplay
	// controls the SWAPCHAIN colorspace (BT.2020 + PQ ST.2084 vs BT.709 +
	// sRGB). Default 0 = SDR sRGB swapchain (bit-identical to pre-d8).
	// CVG_RENDERER live — a change recreates the swapchain (re-negotiating the
	// colorspace) via the same path r_fbo uses; expect a one-frame hitch. The
	// negotiated/effective state shows in `gfxinfo`.
	r_hdrDisplay = ri.Cvar_Get( "r_hdrDisplay", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrDisplay, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hdrDisplay,
		"HDR10 (PQ / ST.2084, BT.2020) display output. Requires \\r_fbo 1,\n"
		"\\r_hdr 1, an HDR-capable display in HDR mode, and a Vulkan surface\n"
		"that enumerates VK_COLOR_SPACE_HDR10_ST2084_EXT (else falls back to\n"
		"SDR with a warning).\n"
		"  0: SDR sRGB swapchain (default)\n"
		"  1: HDR10 swapchain — A2B10G10R10_UNORM_PACK32 + HDR10_ST2084.\n"
		"     The gamma pass PQ-encodes; the tonemap operator uses an HDR\n"
		"     shoulder peaking at r_hdrPeakLuminance.\n"
		"Use `gfxinfo` to confirm the negotiated swapchain colorspace.\n"
		"Live: takes effect on next frame (swapchain recreate; one-frame\n"
		"hitch) via the renderer post-process pipeline group." );
	ri.Cvar_SetGroup( r_hdrDisplay, CVG_RENDERER );
	// r_hdrPeakLuminance — display peak luminance in nits. Feeds (a) the
	// tonemap operator's HDR shoulder (peak_norm = nits/100, so a fully-
	// bright scene element maps to that normalised value), and (b) the
	// HDR10 mastering-metadata MaxCLL / max-display-luminance hint.
	// "Graphics white" (UI / diffuse-white tonemapped 1.0) maps to ~100
	// nits regardless; only scene highlights reach the peak.
	r_hdrPeakLuminance = ri.Cvar_Get( "r_hdrPeakLuminance", "1000", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrPeakLuminance, "100", "10000", CV_INTEGER );
	ri.Cvar_SetDescription( r_hdrPeakLuminance,
		"HDR10 display peak luminance (nits, 100-10000, default 1000).\n"
		"Sets the tonemap HDR shoulder and the mastering-metadata hint.\n"
		"Live: takes effect on next frame (post-process pipeline rebuild +\n"
		"vkSetHdrMetadataEXT re-issue). No effect unless r_hdrDisplay 1." );
	ri.Cvar_SetGroup( r_hdrPeakLuminance, CVG_RENDERER );
	r_hdrMinLuminance = ri.Cvar_Get( "r_hdrMinLuminance", "0.01", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrMinLuminance, "0.0001", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_hdrMinLuminance,
		"HDR10 display minimum luminance (nits, default 0.01) — the\n"
		"mastering-metadata black-level hint only. Live: re-issues\n"
		"vkSetHdrMetadataEXT. No effect unless r_hdrDisplay 1." );
	ri.Cvar_SetGroup( r_hdrMinLuminance, CVG_RENDERER );

	// Histogram auto-exposure. The toggle gates the (future) compute path and
	// may rebake; the tuning cvars feed the per-frame scene-exposure UBO, so
	// they are deliberately left OUT of CVG_RENDERER — a runtime tweak updates
	// the buffer next frame with no post-process pipeline rebuild.
	r_hdrAutoExposure = ri.Cvar_Get( "r_hdrAutoExposure", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrAutoExposure, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hdrAutoExposure,
		"Histogram-based automatic scene exposure (eye adaptation).\n"
		"  0: off — exposure is the manual r_brightness value\n"
		"  1: on (default) — exposure adapts to scene luminance.\n"
		"Requires \\r_fbo 1 and \\r_hdr 1 or 2." );
	ri.Cvar_SetGroup( r_hdrAutoExposure, CVG_RENDERER );

#ifndef NDEBUG
	// Developer GPU->CPU readback diagnostics. Pure readback+log — no effect on
	// the rendered frame. Fenced out of release builds (matches debug_hold_loading
	// / net_forceSendError model): not registered when NDEBUG is defined.
	r_hdrHistogramDebug = ri.Cvar_Get( "r_hdrHistogramDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_hdrHistogramDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_hdrHistogramDebug,
		"Developer: read the auto-exposure luminance histogram back to the CPU\n"
		"and log a bin summary, to verify the compute pass populates it. No effect\n"
		"on the rendered frame.\n"
		"  1: ~1 Hz summary (exposure_bias + bin peaks)\n"
		"  2: PER-FRAME exposure_bias + bin summary (for measuring adaptation swing)." );

	r_brdfLutDebug = ri.Cvar_Get( "r_brdfLutDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_brdfLutDebug, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_brdfLutDebug,
		"Developer: read the split-sum BRDF integration LUT back to the CPU and\n"
		"log a few decoded texels once, to verify the one-shot boot compute pass\n"
		"populated it. No effect on the rendered frame." );

	r_probeSourceDebug = ri.Cvar_Get( "r_probeSourceDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_probeSourceDebug, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_probeSourceDebug,
		"Developer: read the IBL analytic-sky source cube back to the CPU and log\n"
		"a few decoded texels once, to verify the one-shot boot compute pass filled\n"
		"a plausible directional sky. No effect on the rendered frame." );

	r_probeRadianceDebug = ri.Cvar_Get( "r_probeRadianceDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_probeRadianceDebug, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_probeRadianceDebug,
		"Developer: read the IBL convolved radiance (mip0) + irradiance cubes back\n"
		"to the CPU and log a few decoded texels once, to verify the convolve passes\n"
		"converged (mip0 tracks the source, irradiance smoothed). No frame effect." );
#endif

	r_hdrExposureKey = ri.Cvar_Get( "r_hdrExposureKey", "0.0123", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrExposureKey, "0.01", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_hdrExposureKey,
		"Auto-exposure middle-grey key (default 0.0123): the average scene\n"
		"luminance is exposed to land at this value. 0.0123 is this engine's\n"
		"measured middle-grey radiance scale, so a well-lit scene auto-exposes\n"
		"near 1x (not washed out) and a dark scene lifts modestly to readable.\n"
		"Live: next frame." );

	r_hdrExposurePctLow = ri.Cvar_Get( "r_hdrExposurePctLow", "0.5", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrExposurePctLow, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_hdrExposurePctLow,
		"Auto-exposure low histogram percentile clipped before averaging\n"
		"(default 0.5) — discards the darkest fraction. Live: next frame." );

	r_hdrExposurePctHigh = ri.Cvar_Get( "r_hdrExposurePctHigh", "0.2", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrExposurePctHigh, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_hdrExposurePctHigh,
		"Auto-exposure high histogram percentile clipped before averaging\n"
		"(default 0.2) — discards the brightest fraction. Live: next frame." );

	r_hdrAdaptionRateUp = ri.Cvar_Get( "r_hdrAdaptionRateUp", "1.5", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrAdaptionRateUp, "0.0", "100.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_hdrAdaptionRateUp,
		"Auto-exposure adaptation speed when the scene brightens\n"
		"(per second, default 1.5 — faster than darkening). Live: next frame." );

	r_hdrAdaptionRateDown = ri.Cvar_Get( "r_hdrAdaptionRateDown", "0.5", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrAdaptionRateDown, "0.0", "100.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_hdrAdaptionRateDown,
		"Auto-exposure adaptation speed when the scene darkens\n"
		"(per second, default 0.5). Live: next frame." );

	r_hdrExposureMin = ri.Cvar_Get( "r_hdrExposureMin", "0.25", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrExposureMin, "0.01", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_hdrExposureMin,
		"Auto-exposure clamp floor (default 0.25): the lowest exposure\n"
		"multiplier auto-exposure may apply. Live: next frame." );

	r_hdrExposureMax = ri.Cvar_Get( "r_hdrExposureMax", "8.0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_hdrExposureMax, "1.0", "64.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_hdrExposureMax,
		"Auto-exposure clamp ceiling (default 8.0): the highest exposure\n"
		"multiplier auto-exposure may apply. Live: next frame." );

	r_bloom = ri.Cvar_Get( "r_bloom", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_bloom, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription(r_bloom, "Enables bloom post-processing effect. Requires \\r_fbo 1.");
	r_bloomPasses = ri.Cvar_Get( "r_bloomPasses", "2", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_bloomPasses, "1", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_bloomPasses, "Number of bloom pyramid levels (1-4). Lower = fewer GPU render passes = higher FPS. Default 2 is a good balance between quality and performance." );
	r_vrs = ri.Cvar_Get( "r_vrs", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vrs, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vrs, "Variable-rate shading on the bloom post-process pass only (2x2). Reduces bloom GPU cost; imperceptible since bloom is low-frequency. No effect on the HUD, world, or crosshair (those always render at full rate). Requires VRS-capable hardware (else 1x1). Default off." );
#ifdef __APPLE__
	r_vkApplePinkBarrier = ri.Cvar_Get( "r_vkApplePinkBarrier", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_vkApplePinkBarrier, "MoltenVK: emit explicit tile-cache barrier between main and gamma passes. Disable (0) to test FPS impact; re-enable (1) if pink glitch appears." );
#endif

	r_ext_supersample = ri.Cvar_Get( "r_ext_supersample", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_supersample, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_supersample, "Super-sample anti-aliasing, requires \\r_fbo 1." );
#if 0
	r_ext_alpha_to_coverage = ri.Cvar_Get( "r_ext_alpha_to_coverage", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_alpha_to_coverage, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_alpha_to_coverage, "Enables alpha-to-coverage multisampling, requires \\r_fbo 1." );
#endif

	r_renderWidth = ri.Cvar_Get( "r_renderWidth", "800", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_renderWidth, "96", NULL, CV_INTEGER );
	ri.Cvar_SetDescription( r_renderWidth, "Video width to render to when \\r_renderScale > 0." );
	r_renderHeight = ri.Cvar_Get( "r_renderHeight", "600", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_renderHeight, "72", NULL, CV_INTEGER );
	ri.Cvar_SetDescription( r_renderHeight, "Video height to render to when \\r_renderScale > 0." );

	r_renderScale = ri.Cvar_Get( "r_renderScale", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_renderScale, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_renderScale, "Scaling mode to be used with custom render resolution:\n"
		" 0 - disabled\n"
		" 1 - nearest filtering, stretch to full size\n"
		" 2 - nearest filtering, preserve aspect ratio (black bars on sides)\n"
		" 3 - linear filtering, stretch to full size\n"
		" 4 - linear filtering, preserve aspect ratio (black bars on sides)\n" );

	r_depthFade = ri.Cvar_Get( "r_depthFade", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_depthFade, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_depthFade, "Soft particle edges: fade transparent surfaces near opaque geometry.\n"
		" Requires \\r_fbo 1." );

	r_depthFadeScale = ri.Cvar_Get( "r_depthFadeScale", "2.0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_depthFadeScale, "Soft-particle depth-fade distance scale (larger = wider, softer\n"
		" intersection band). Baked into the shader at pipeline creation (spec constant),\n"
		" so a change applies on \\vid_restart. Default 2.0." );

#if FEAT_PARALLAX_MAPPING
	r_parallaxMapping = ri.Cvar_Get( "r_parallaxMapping", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_parallaxMapping, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_parallaxMapping, "Steep parallax mapping on surfaces with normalMap textures.\n"
		" Height data from normalmap alpha channel." );
#endif

#if FEAT_SSAO
	// Default 1 — GTAO is on by default. Still toggleable: CVAR_ARCHIVE|LATCH means
	// a player can set \r_ssao 0 (applies on vid_restart).
	r_ssao = ri.Cvar_Get( "r_ssao", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ssao, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssao, "Ground-truth ambient occlusion (GTAO).\n"
		" Requires \\r_fbo 1. Default on; set 0 to disable." );
	// Live-tunable GTAO knobs (no vid_restart — read each frame by the dispatch).
	r_ssaoRadius = ri.Cvar_Get( "r_ssaoRadius", "12", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_ssaoRadius, "1", "512", CV_FLOAT );
	ri.Cvar_SetDescription( r_ssaoRadius, "GTAO world-space sample radius (units)." );
	r_ssaoQuality = ri.Cvar_Get( "r_ssaoQuality", "2", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_ssaoQuality, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssaoQuality, "GTAO quality tier: 0 low, 1 medium, 2 high, 3 ultra (slices/steps)." );
	r_ssaoIntensity = ri.Cvar_Get( "r_ssaoIntensity", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_ssaoIntensity, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_ssaoIntensity, "GTAO strength: 0 off (no darkening), 1 full." );
	// SSCS directional contact-shadow tuning (the on/off is r_shadows ≥ cast +
	// a real map sun; these only shape it). Live-tunable, read each frame.
	r_sscsRadius = ri.Cvar_Get( "r_sscsRadius", "6", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_sscsRadius, "0", "64", CV_FLOAT );
	ri.Cvar_SetDescription( r_sscsRadius, "Directional contact-shadow (SSCS) march length in world units — the fine model-on-ground contact scale toward the sun. Folded into GTAO; active only when \\r_shadows casts and the map has a real sun. 0 disables the contact fold." );
	r_sscsStrength = ri.Cvar_Get( "r_sscsStrength", "0.6", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_sscsStrength, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_sscsStrength, "Directional contact-shadow (SSCS) darkening strength where the sun-march hits a near occluder. 0 off, 1 fully dark." );
	// Debug-visibility view (NOT a developer cvar): output the denoised AO buffer as
	// grayscale instead of compositing it, so the visual gate can capture + assert the
	// AO field directly. Default 0 (normal composite). LATCH (baked into the SSAO
	// tonemap variant spec const). Registered whenever the current build enables
	// FEAT_SSAO (the product default; USE_SSAO=OFF is the explicit permutation).
	r_showAO = ri.Cvar_Get( "r_showAO", "0", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_showAO, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_showAO, "Debug: show the GTAO AO buffer grayscale (visual-gate isolation view)." );
#endif

	// Async-compute: DEFAULT 1 now that GTAO is default-on. When the device exposes a
	// dedicated compute queue family, GTAO runs on RAL_QUEUE_COMPUTE reading the
	// previous frame's depth snapshot (1-frame-lag), overlapping the
	// frame's graphics (+24% measured). When the queue aliases graphics (WebGPU/
	// MoltenVK/no-dedicated-family), vk_gtao_async_begin returns early on
	// !caps->asyncCompute → the serial graphics-queue GTAO runs instead, BYTE-IDENTICAL
	// (verified). So default-on is safe on every HW: dedicated-queue GPUs get the
	// overlap, single-queue GPUs get the identical serial result. LATCH: the per-frame
	// compute command buffers + semaphores are allocated at init. Set 0 to force serial.
	r_asyncCompute = ri.Cvar_Get( "r_asyncCompute", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_asyncCompute, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_asyncCompute, "Run eligible compute (GTAO) on the dedicated async-compute queue,\n"
		" overlapping graphics. Default on; falls back to serial (byte-identical) on\n"
		" GPUs without a separate compute queue. Set 0 to force serial." );

	r_asyncTextureUpload = ri.Cvar_Get( "r_asyncTextureUpload", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_asyncTextureUpload, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_asyncTextureUpload, "Pipeline eligible texture uploads on the dedicated transfer queue\n"
		" (the copy submits without blocking; the texture shows the default placeholder\n"
		" until a per-frame drain swaps in the real texture and a graphics-queue barrier\n"
		" makes it visible to sampling). Default 1. Boot-critical fonts + the cursor load\n"
		" synchronously at boot; rare fonts lazy-load on first use (block-until-resident);\n"
		" menu decor streams in-frame. Falls back to synchronous when there is no dedicated\n"
		" transfer queue or for built-in / mip-gen / sub-region uploads. Set 0 to force the\n"
		" synchronous path." );

#if FEAT_TONEMAP
	// r_tonemap is in CVG_RENDERER (runtime-live): the operator
	// selection is wired through tonemap_mode (tonemap.frag spec
	// constant id 17). Group membership triggers tonemap pipeline
	// rebake on modificationCount changes; same mechanism as
	// r_brightness, r_saturation, etc.
	// default 0 -> 1 (Reinhard). Previously the default 0
	// defeated the linear HDR pipeline because exposure_bias scaled
	// HDR values past the LDR clamp without roll-off. Mode 0 is still
	// reachable (`r_tonemap 0`) and now means "identity passthrough,
	// hard clip at LDR range" — the legacy behaviour.
	//
	// this cvar and the r_tonemapExposure / r_lottes_*
	// family below all carry defaults verified correct for linear-radiance
	// input — no retune. Rationale per cvar:
	//   r_tonemap 1       = PBR Neutral, the modern hue-preserving operator
	//                       (mid-tones below ~0.8 pass through, soft shoulder
	//                       above) — the right neutral default for linear.
	//   r_tonemapExposure = a *linear* pre-operator multiplier, folds with
	//                       r_brightness; 1.0 is the identity.
	//   r_lottes_*        = canonical Lottes GDC-2016 shape parameters
	//                       (mid_in 0.18 is the photographic 18%-grey LINEAR
	//                       reference; hdr_max 8.0 is a sensible linear HDR
	//                       ceiling) — not domain-sensitive.
	r_tonemap = ri.Cvar_Get( "r_tonemap", "3", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_tonemap, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_tonemap,
		"HDR tone mapping operator. Compresses scene radiance to LDR\n"
		"display range. All operators are hue-preserving except where\n"
		"noted.\n"
		"  0 - identity passthrough (HDR clipped at display range, legacy)\n"
		"  1 - PBR Neutral (default, glTF 2.0 reference, minimal manipulation,\n"
		"      preserves LDR mid-tones, soft highlight roll-off)\n"
		"  2 - AgX (modern hue-preserving, sigmoid curve, polished cinematic feel)\n"
		"  3 - Lottes (configurable filmic, tunable via r_lottes_* cvars)\n"
		"  4 - Reinhard (classical 1985 reference, alters mid-tones, hue-shift)\n"
		"Requires r_fbo 1. Live: takes effect on next frame via the\n"
		"renderer post-process pipeline group." );
	ri.Cvar_SetGroup( r_tonemap, CVG_RENDERER );

	// r_tonemapExposure is the pre-tonemap multiplier (gamma.frag
	// spec constant id 18). Values <1 darken atmospherically
	// before the operator's shoulder kicks in; values >1 brighten.
	// Only takes effect when r_tonemap > 0 — when r_tonemap is 0,
	// the TONEMAP_VAR_BASE varIdx bit is unset and the gamma-with-
	// tonemap pipeline isn't selected, so the exposure write
	// doesn't reach a running shader.
	r_tonemapExposure = ri.Cvar_Get( "r_tonemapExposure", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_tonemapExposure, "0.1", "8.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_tonemapExposure,
		"Pre-tonemap exposure multiplier.\n"
		"Range 0.1-8.0, default 1.0.\n"
		"Multiplies HDR colour before the tonemap operator;\n"
		"values below 1.0 darken atmospherically (Doom-3-style\n"
		"shoulder compression), values above 1.0 brighten.\n"
		"Requires r_tonemap > 0 and r_fbo 1. Live: takes\n"
		"effect on next frame." );
	ri.Cvar_SetGroup( r_tonemapExposure, CVG_RENDERER );

	// Lottes (r_tonemap 3) configurable filmic parameters. All five
	// are CVG_RENDERER live cvars — the per-frame check in
	// tr_cmds.c:441 polls the group and triggers
	// vk_update_post_process_pipelines on any change, so a Lottes
	// tweak rebakes the tonemap pipeline's spec constants on the
	// next frame without vid_restart. Defaults match Timothy Lottes's
	// GDC 2016 reference curve.
	r_lottes_contrast = ri.Cvar_Get( "r_lottes_contrast", "1.6", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_lottes_contrast, "0.5", "3.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lottes_contrast,
		"Lottes tonemap contrast parameter. Higher = steeper mid-tones.\n"
		"Default 1.6 (canonical Lottes 2016 reference).\n"
		"Effective only when r_tonemap 3 is active. Live cvar." );
	ri.Cvar_SetGroup( r_lottes_contrast, CVG_RENDERER );

	r_lottes_shoulder = ri.Cvar_Get( "r_lottes_shoulder", "0.977", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_lottes_shoulder, "0.5", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lottes_shoulder,
		"Lottes tonemap highlight shoulder softness. Higher = softer roll-off.\n"
		"Default 0.977 (canonical).\n"
		"Effective only when r_tonemap 3 is active. Live cvar." );
	ri.Cvar_SetGroup( r_lottes_shoulder, CVG_RENDERER );

	r_lottes_mid_in = ri.Cvar_Get( "r_lottes_mid_in", "0.18", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_lottes_mid_in, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lottes_mid_in,
		"Lottes tonemap input mid-point. Anchors the curve's middle.\n"
		"Default 0.18 (canonical, 18% gray photographic reference).\n"
		"Effective only when r_tonemap 3 is active. Live cvar." );
	ri.Cvar_SetGroup( r_lottes_mid_in, CVG_RENDERER );

	r_lottes_mid_out = ri.Cvar_Get( "r_lottes_mid_out", "0.267", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_lottes_mid_out, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lottes_mid_out,
		"Lottes tonemap output mid-point. The display value mid_in maps to.\n"
		"Default 0.267 (canonical).\n"
		"Effective only when r_tonemap 3 is active. Live cvar." );
	ri.Cvar_SetGroup( r_lottes_mid_out, CVG_RENDERER );

	r_lottes_hdr_max = ri.Cvar_Get( "r_lottes_hdr_max", "8.0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_lottes_hdr_max, "1.0", "64.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lottes_hdr_max,
		"Lottes tonemap maximum HDR input value (mapped to display white).\n"
		"Default 8.0 (canonical).\n"
		"Effective only when r_tonemap 3 is active. Live cvar." );
	ri.Cvar_SetGroup( r_lottes_hdr_max, CVG_RENDERER );
#endif

#if FEAT_COLOR_GRADING
	// r_colorGrading is the master enable for the colour-grading post-
	// process pass. Stays CVAR_LATCH because flipping it toggles which
	// shader variant (USE_COLOR_GRADING) the pipeline binds — variant
	// selection is rebuilt by vk_update_post_process_pipelines on the
	// next group reset, but the enable itself requires a vid_restart
	// for the shader-module table to repopulate consistently. Once
	// enabled, the five r_grade_* knobs below are live.
	r_colorGrading = ri.Cvar_Get( "r_colorGrading", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_colorGrading, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_colorGrading, "Color grading post-process (tint, saturation, contrast).\n"
		" 0: off (default)\n"
		" 1: on — bind USE_COLOR_GRADING tonemap variant; r_grade_*\n"
		"        live cvars then control the look.\n"
		"Requires \\r_fbo 1. vid_restart required when toggling on/off." );

	// live colour-grading knobs. CVG_RENDERER routes
	// every change through vk_update_post_process_pipelines, which
	// rewrites cg_tint_r/g/b, cg_saturation, cg_contrast spec
	// constants on the next tonemap variant rebuild. Effective only
	// when r_colorGrading 1 (master gate).
	r_grade_tint_r = ri.Cvar_Get( "r_grade_tint_r", "1.0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_grade_tint_r, "0.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_grade_tint_r,
		"Colour grading red tint multiplier. 1.0 = neutral, <1.0 less\n"
		"red, >1.0 more red. Live; requires r_colorGrading 1." );
	ri.Cvar_SetGroup( r_grade_tint_r, CVG_RENDERER );

	r_grade_tint_g = ri.Cvar_Get( "r_grade_tint_g", "1.0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_grade_tint_g, "0.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_grade_tint_g,
		"Colour grading green tint multiplier. 1.0 = neutral, <1.0 less\n"
		"green, >1.0 more green. Live; requires r_colorGrading 1." );
	ri.Cvar_SetGroup( r_grade_tint_g, CVG_RENDERER );

	r_grade_tint_b = ri.Cvar_Get( "r_grade_tint_b", "1.0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_grade_tint_b, "0.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_grade_tint_b,
		"Colour grading blue tint multiplier. 1.0 = neutral, <1.0 less\n"
		"blue, >1.0 more blue. Live; requires r_colorGrading 1." );
	ri.Cvar_SetGroup( r_grade_tint_b, CVG_RENDERER );

	r_grade_saturation = ri.Cvar_Get( "r_grade_saturation", "1.0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_grade_saturation, "0.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_grade_saturation,
		"Colour grading saturation. 1.0 = identity, 0.0 = greyscale,\n"
		"2.0 = oversaturated. Distinct from r_saturation (radiance\n"
		"domain, pre-tonemap); this applies post-tonemap in display\n"
		"domain. Live; requires r_colorGrading 1." );
	ri.Cvar_SetGroup( r_grade_saturation, CVG_RENDERER );

	r_grade_contrast = ri.Cvar_Get( "r_grade_contrast", "1.0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_grade_contrast, "0.5", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_grade_contrast,
		"Colour grading contrast. 1.0 = identity, <1.0 flatter, >1.0\n"
		"punchier. Applied around the 0.5 luminance pivot. Live;\n"
		"requires r_colorGrading 1." );
	ri.Cvar_SetGroup( r_grade_contrast, CVG_RENDERER );
#endif

#if FEAT_PBR
	// r_pbr live conversion. Previously this was CVAR_LATCH;
	// the gate at tr_shade.c (PBR pipeline swap on materials with a
	// pbrMap stage) now reads r_pbr->integer live. CVG_RENDERER group
	// membership is documentation-only here — the pipeline cache
	// (vk_find_pipeline_ext) naturally selects PBR vs non-PBR variants
	// per draw based on the live r_pbr value, so no rebuild is
	// triggered, just a different cached pipeline gets bound.
	r_pbr = ri.Cvar_Get( "r_pbr", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_pbr, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr,
		"Physically based rendering on surfaces with pbrMap textures.\n"
		" pbrMap follows glTF 2.0 ORM packing:\n"
		"   R = ambient occlusion (linear)\n"
		"   G = roughness (linear)\n"
		"   B = metalness (linear)\n"
		" Authors using Substance, Blender, Godot, UE5 glTF export\n"
		" produce this packing by default.\n"
		" Ambient occlusion consumed by the IBL path.\n"
		" Live: 0 disables the PBR pipeline swap, 1 enables it." );
	ri.Cvar_SetGroup( r_pbr, CVG_RENDERER );
#endif

	// Tiled (Forward+) dynamic lighting. A depth-aware tile-classification compute
	// bins the dynamic light list into screen tiles so the lit fragment iterates
	// only its tile's lights, instead of the per-light forward pass. 0 (default) =
	// the existing PMLIGHT per-light-pass, byte-identical. LATCH: the tile compute +
	// the lit pipeline are built at init, so a toggle applies on vid_restart.
	r_forwardPlus = ri.Cvar_Get( "r_forwardPlus", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_forwardPlus, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_forwardPlus,
		"Tiled (Forward+) dynamic lighting: a compute pass bins lights into screen\n"
		" tiles; the lit fragment iterates only its tile's lights. Scales the dynamic\n"
		" light count. 0 = the per-light-pass PMLIGHT path (default, byte-identical).\n"
		" Latched: applies on vid_restart." );
	ri.Cvar_SetGroup( r_forwardPlus, CVG_RENDERER );

	// Extract BSP `light` entities into the Forward+ dlight set (so static map
	// lighting is dynamic — scales past the 32-dlight PMLIGHT limit), and dim the
	// baked lightmap by a tuned factor to bound the additive double-count (the
	// lightmap is a single pre-summed value → a surgical per-light un-bake is
	// impossible; the global dim is the best runtime approximation). The ON look is
	// an accepted approximation, NOT byte-identical. 0 (default) = don't extract,
	// don't dim → byte-identical. Latched (the extraction runs at map load, the fp
	// pipeline is built at init), so a toggle applies on vid_restart.
	r_unbakeStaticLights = ri.Cvar_Get( "r_unbakeStaticLights", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_unbakeStaticLights, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_unbakeStaticLights,
		"Extract BSP static `light` entities into Forward+ dynamic lighting and dim\n"
		" the baked lightmap to bound the double-count. 0 = off (default, byte-\n"
		" identical). 1 = extract + tuned global dim (an approximation — the baked\n"
		" lightmap can't be surgically un-baked). Latched: applies on vid_restart." );
	ri.Cvar_SetGroup( r_unbakeStaticLights, CVG_RENDERER );

#if FEAT_SHADOW_MAPPING
	// Point-light (omni dlight) shadows. 1 (default) = the brightest visible runtime
	// dlights cast an omni shadow (a 2D-atlas cubemap of the scene depth from the light,
	// sampled per-fragment for occlusion). 0 = dlights cast no shadows (light through
	// walls), byte-identical to the pre-feature path. The shadowed-light budget is
	// r_dlightShadowK (top-K brightest). Runtime dlights only (BSP static lights stay
	// lightmap-baked).
	r_dlightShadows = ri.Cvar_Get( "r_dlightShadows", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_dlightShadows, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_dlightShadows,
		"Omni shadows for dynamic point lights: the brightest visible runtime dlights\n"
		" render the scene depth from their position (a 2D-atlas cubemap) and the lit\n"
		" fragment depth-compares for occlusion. 1 = on (default). 0 = no dlight shadows\n"
		" (byte-identical to the pre-feature path). Live: toggling allocates/releases the\n"
		" atlas at the next frame boundary (no vid_restart)." );
	ri.Cvar_SetGroup( r_dlightShadows, CVG_RENDERER );

	// Number of shadow-casting dynamic lights (the top-K brightest visible runtime
	// dlights each render an omni shadow into a wider shared atlas strip). 1 (default)
	// = the single brightest light, byte-identical to a one-light budget. Live: a
	// change re-allocates the atlas at the next frame boundary (no vid_restart).
	r_dlightShadowK = ri.Cvar_Get( "r_dlightShadowK", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_dlightShadowK, "1", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_dlightShadowK,
		"Number of shadow-casting dynamic point lights (top-K brightest). 1 (default)\n"
		" = the single brightest light. Range 1..4. Live (no vid_restart)." );
	ri.Cvar_SetGroup( r_dlightShadowK, CVG_RENDERER );

	// Test-only (CVAR_CHEAT): inject a synthetic dlight each scene so the omni-shadow
	// path can be exercised + spatially verified headless (real dlights need gameplay
	// firing — the firing wall). 0 = off (default, no injection → byte-identical). Value
	// = the light radius; the light is placed at a fixed offset above the view origin.
	r_dlightShadowTest = ri.Cvar_Get( "r_dlightShadowTest", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_dlightShadowTest, "0", "4096", CV_INTEGER );

	// Test-only (CVAR_CHEAT): how many synthetic dlights r_dlightShadowTest injects (1..4),
	// in a ring around the view, to exercise + verify the K-light shadow budget headless.
	r_dlightShadowTestN = ri.Cvar_Get( "r_dlightShadowTestN", "1", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_dlightShadowTestN, "1", "4", CV_INTEGER );

	r_dlightShadowCount = ri.Cvar_Get( "r_dlightShadowCount", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_dlightShadowCount, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_dlightShadowCount,
		"Diagnostic: log the per-frame count + running peak of shadow-eligible dynamic\n"
		" lights (radius > 0 in the view's dlight set) — the candidate set a K-light\n"
		" shadow budget would draw from. 0 = off." );

	// Diagnostic (CVAR_CHEAT): wall-clock the dlight-shadow bake. When set, the omni
	// depth bake is wrapped with a CPU microsecond timer (the deterministic, automatable
	// proxy for the GPU submit cost); the accumulated us + face-pass count over a
	// ~1-second window is logged. NOT pure GPU time — the CPU-submit floor. 0 = off.
	r_dlightShadowProfile = ri.Cvar_Get( "r_dlightShadowProfile", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_dlightShadowProfile, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_dlightShadowProfile,
		"Diagnostic: log the dlight-shadow bake CPU-submit cost (us/frame) + face-pass\n"
		" count over a 1-second window. 0 = off." );

	// Test-only (CVAR_CHEAT): inject a synthetic alpha-tested quad into the CSM
	// caster set so the cut-out shadow path can be exercised + verified headless.
	// The quad uses a procedural half-opaque/half-cut-out texture, so its cast
	// shadow shows a computable hole pattern. 0 = off (no injection). 1 = real
	// alpha-test discard (holed shadow). 2 = the same quad with the discard func set
	// to 0 (solid silhouette) — the failure mode the holed-ness gate must catch.
	// Not latched: the quad (and its discard func) is rebuilt every shadow render,
	// so toggling takes effect next frame with no pipeline rebuild.
	r_shadowAtestTest = ri.Cvar_Get( "r_shadowAtestTest", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_shadowAtestTest, "0", "2", CV_INTEGER );
#endif

#if FEAT_ADVANCED_WATER
	ri.Cvar_Get( "r_waterRefraction", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_waterRefraction", "1", 0 ),
		"Advanced water with screen-space refraction, Fresnel, and ripple noise." );
#endif

#if FEAT_SHADOW_MAPPING
	// r_shadows (registered above) and r_shadowMapSize are CVG_RENDERER: changing
	// either rebuilds the FBO chain on the next frame, no vid_restart. CSM
	// sun-shadows default on — the dynamic-light path (PMLIGHT + lightgrid ambient
	// + sun-shadows) is the default lit pipeline; with r_fbo 1 and the engine's
	// default sun direction it renders shadows out of the box.
	// Deliver the per-entity model/MVP transform via a frame-wide storage buffer
	// indexed by gl_InstanceIndex (one bind per frame), instead of the per-draw
	// uniform-buffer ring (a dynamic-offset rebind per draw). Same matrices, same
	// pixels — this is the portable-binding surface (a clone of the shadow depth
	// pass), not a draw-count or speed change. Latched: the shader read site is
	// selected at pipeline build, so the value cannot change mid-frame.
	r_entitySSBO = ri.Cvar_Get( "r_entitySSBO", "0", CVAR_ARCHIVE | CVAR_LATCH | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( ri.Cvar_Get( "r_entitySSBO", "0", 0 ), "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_entitySSBO", "0", 0 ),
		"Deliver per-entity matrices via a frame-wide storage buffer indexed by\n"
		" instance, instead of the per-draw uniform ring. Render-identical; a\n"
		" portable-binding surface. Latched: takes effect on vid_restart." );
	ri.Cvar_SetGroup( ri.Cvar_Get( "r_entitySSBO", "0", 0 ), CVG_RENDERER );
	ri.Cvar_Get( "r_shadowPCF", "5", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( ri.Cvar_Get( "r_shadowPCF", "5", 0 ), "1", "9", CV_INTEGER );
	ri.Cvar_Get( "r_shadowMapSize", "2048", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( ri.Cvar_Get( "r_shadowMapSize", "2048", 0 ), "512", "4096", CV_INTEGER );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_shadowMapSize", "2048", 0 ),
		"Shadow-map resolution (per cascade). Live: rebuilt on the next frame." );
	ri.Cvar_SetGroup( ri.Cvar_Get( "r_shadowMapSize", "2048", 0 ), CVG_RENDERER );
	ri.Cvar_Get( "r_csmCascades", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( ri.Cvar_Get( "r_csmCascades", "1", 0 ), "1", "4", CV_INTEGER ); // 4 == SHADOWMAP_MAX_CASCADES
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_csmCascades", "1", 0 ),
		"Number of cascaded shadow-map cascades to render (1..4). Cascades beyond\n"
		" this count are left fully lit. Live (re-read per frame)." );
	ri.Cvar_SetGroup( ri.Cvar_Get( "r_csmCascades", "1", 0 ), CVG_RENDERER );

	// CSM polish knobs (all live via CVG_RENDERER).
	ri.Cvar_Get( "r_csmBias", "0.005", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( ri.Cvar_Get( "r_csmBias", "0.005", 0 ), "0", "0.1", CV_FLOAT );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_csmBias", "0.005", 0 ),
		"Shadow depth-bias intensity. Drives the depth pass's slope-scaled bias\n"
		" (0.005 == the prior fixed conservative value). Lower → acne, higher → peter-panning." );
	ri.Cvar_SetGroup( ri.Cvar_Get( "r_csmBias", "0.005", 0 ), CVG_RENDERER );

	ri.Cvar_Get( "r_csmLambda", "0.5", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( ri.Cvar_Get( "r_csmLambda", "0.5", 0 ), "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_csmLambda", "0.5", 0 ),
		"Practical Split blend: 0 = uniform splits, 1 = logarithmic splits." );
	ri.Cvar_SetGroup( ri.Cvar_Get( "r_csmLambda", "0.5", 0 ), CVG_RENDERER );

	ri.Cvar_Get( "r_csmMaxDistance", "4096", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( ri.Cvar_Get( "r_csmMaxDistance", "4096", 0 ), "256", "16384", CV_FLOAT );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_csmMaxDistance", "4096", 0 ),
		"Far distance covered by the last cascade (clamps the view far plane for CSM)." );
	ri.Cvar_SetGroup( ri.Cvar_Get( "r_csmMaxDistance", "4096", 0 ), CVG_RENDERER );

	ri.Cvar_Get( "r_csmShowCascades", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( ri.Cvar_Get( "r_csmShowCascades", "0", 0 ), "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_csmShowCascades", "0", 0 ),
		"Debug: tint lit-pass output by the cascade sampled per pixel\n"
		" (0 = red, 1 = green, 2 = blue, 3 = yellow)." );
	ri.Cvar_SetGroup( ri.Cvar_Get( "r_csmShowCascades", "0", 0 ), CVG_RENDERER );

	ri.Cvar_Get( "r_csmCull", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( ri.Cvar_Get( "r_csmCull", "1", 0 ), "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_csmCull", "1", 0 ),
		"Per-cascade caster cull: skip an entity caster (brush-model / animated\n"
		" mesh / skinned player) from a cascade its bounding sphere doesn't reach.\n"
		" Correctness-preserving (shadows identical); reduces shadow draw calls.\n"
		" 0 draws every entity caster into every cascade (the A/B baseline). Live." );
	ri.Cvar_SetGroup( ri.Cvar_Get( "r_csmCull", "1", 0 ), CVG_RENDERER );
#endif

#if FEAT_SUNRAYS
	// Live-toggle (no latch): a change is picked up by the CVG_RENDERER group, which
	// rebuilds the tonemap variant pipeline (vk_update_post_process_pipelines). When a
	// depth-fade consumer (r_ssao) is already on, depthFade is already set up so only the
	// variant swaps; when sun-rays are the sole consumer, vk_tonemap requests a deferred
	// depth-fade rebuild at the next safe frame boundary.
	r_drawSunRays = ri.Cvar_Get( "r_drawSunRays", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_drawSunRays, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_drawSunRays, "Screen-space crepuscular sun-rays from the map sun.\n"
		" Requires \\r_fbo 1." );
	ri.Cvar_SetGroup( r_drawSunRays, CVG_RENDERER );
	// Look-tuning for the sun-ray accumulation, fed per-frame into the exposure UBO
	// (no latch — adjustable live). Intensity scales the additive ray contribution;
	// decay is the per-sample falloff along the march toward the sun.
	r_sunRayIntensity = ri.Cvar_Get( "r_sunRayIntensity", "0.4", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_sunRayIntensity, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_sunRayIntensity, "Sun-ray brightness (additive). 0 = off." );
	r_sunRayDecay = ri.Cvar_Get( "r_sunRayDecay", "0.95", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_sunRayDecay, "0.5", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_sunRayDecay, "Sun-ray per-sample falloff along the sun march." );
#endif

	// live conversion. Previously this was CVAR_LATCH because SMAA
	// resources (input/edges/blend images + descriptors + pipelines)
	// were lifecycle-tied to r_smaa->integer. Resources are now allocated
	// unconditionally when r_fbo 1 (vk.c:9114 — vk.smaa.active = fboActive),
	// and r_smaa is just the per-frame dispatch gate + quality preset
	// selector for pipeline rebuild. CVG_RENDERER group membership wires
	// changes into vk_update_post_process_pipelines.
	// no SMAA re-tune needed for the linear pipeline.
	// The edge-detection threshold (preset, or r_smaa_threshold override)
	// is a *relative-luma delta* (delta / max(L, eps)) — unitless, so it
	// reads the same at any input magnitude (the design point); the
	// neighbourhood-blend pass (smaa_resolve.frag) is a weight-normalised
	// bilinear blend that becomes *more* correct once vk.color_image holds
	// linear radiance. SMAA thus needs no re-tune for the linear pipeline;
	// its defaults are unchanged (unlike the bloom threshold, which is an
	// absolute level and was rebased).
	// SMAA output route: it reads and
	// writes the tonemapped image (img 265), dispatched from the 3D->2D
	// transition (RB_TransitionToUI, tr_backend.c) between vk_tonemap()
	// and vk_open_ui_pass(qfalse) — i.e. after tonemap, before the HUD
	// pass. The HUD composes on top of the anti-aliased scene and is
	// itself left un-AA'd. (vk_tonemap() is also decoupled from the
	// UI pass — pure-2D frames skip tonemap entirely via render_pass.ui_clear.)
	r_smaa = ri.Cvar_Get( "r_smaa", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_smaa, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_smaa, "SMAA anti-aliasing quality:\n"
		" 0 - disabled\n"
		" 1 - low     (threshold 0.15, 4-step search)\n"
		" 2 - medium  (threshold 0.10, 8-step search)\n"
		" 3 - high    (threshold 0.10, 16-step search + diagonal + corner rounding)\n"
		" 4 - ultra   (threshold 0.05, 32-step search + diagonal + corner rounding)\n"
		"Threshold is a relative-luma delta (same semantic at LDR and HDR).\n"
		"Override the preset threshold via r_smaa_threshold.\n"
		"Anti-aliases the tonemapped scene (img 265) before the HUD pass —\n"
		"the HUD stays sharp. Requires r_fbo 1. Live: takes effect on next\n"
		"frame via the renderer post-process pipeline group." );
	ri.Cvar_SetGroup( r_smaa, CVG_RENDERER );

	// r_smaa_threshold — Overrides the quality preset
	// edge threshold when > 0; default 0 means "use r_smaa preset".
	// Live via CVG_RENDERER (rebakes the SMAA edge pipeline's spec
	// constant on cvar change). Range [0, 0.5] matches sane relative-
	// luma deltas: 0.5 means "neighbour must be at least 50% off-center"
	// which only catches the sharpest discontinuities; 0.001 catches
	// everything.
	r_smaa_threshold = ri.Cvar_Get( "r_smaa_threshold", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_smaa_threshold, "0", "0.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_smaa_threshold,
		"SMAA edge detection threshold override (relative-luma delta).\n"
		" 0       - use r_smaa quality preset (default)\n"
		" 0.001+  - explicit threshold value; overrides preset\n"
		"Higher = fewer edges detected (less smoothing).\n"
		"Lower  = more edges detected (more smoothing).\n"
		"Live: takes effect on next frame via the renderer\n"
		"post-process pipeline group." );
	ri.Cvar_SetGroup( r_smaa_threshold, CVG_RENDERER );

	r_lerpLightstyles = ri.Cvar_Get( "r_lerpLightstyles", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_lerpLightstyles, "Interpolate lightstyle animation between pattern characters for smooth fading (1=on, 0=stepped 10Hz)." );
#endif // USE_VULKAN
}

#define EPSILON 1e-6f

/*
===============
R_Init
===============
*/
void R_Init( void ) {
#ifndef USE_VULKAN
	int	err;
#endif
	byte *ptr;

	R_LOG( rch_init, SEV_INFO, "----- R_Init -----\n" );

	// clear all our internal state
	memset( &tr, 0, sizeof( tr ) );
	memset( &backEnd, 0, sizeof( backEnd ) );
	memset( &tess, 0, sizeof( tess ) );
	memset( &glState, 0, sizeof( glState ) );

	// glconfig_t ABI tripwire. Was 11332; bumped to 11340 when vidWidthLogical/
	// vidHeightLogical (2 ints, +8 bytes) were appended for HiDPI font scaling.
	// All native modules (engine + renderer DLLs + gamecl/gamesv) are rebuilt
	// from the same tr_types.h, so they agree on the new size; this constant is
	// the human tripwire that forces that coordinated rebuild.
	if ( sizeof( glconfig_t ) != 11340 )
		ri.Terminate( TERM_UNRECOVERABLE, "Mod ABI incompatible: sizeof(glconfig_t) == %u != 11340", (unsigned int) sizeof( glconfig_t ) );

	if ( (intptr_t)tess.xyz & 15 ) {
		R_LOG( rch_init, SEV_WARN, "tess.xyz not 16 byte aligned\n" );
	}
	memset( tess.constantColor255, 255, sizeof( tess.constantColor255 ) );

	//
	// init function tables
	//
	for ( int i = 0; i < FUNCTABLE_SIZE; i++ ) {
		tr.sinTable[i] = sin( DEG2RAD( i * 360.0f / FUNCTABLE_SIZE ) + 0.0001f );
		tr.squareTable[i] = (i < FUNCTABLE_SIZE / 2) ? 1.0f : -1.0f;
		if ( i == 0 ) {
			tr.sawToothTable[i] = EPSILON;
		} else {
			tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		}
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];
		if ( i < FUNCTABLE_SIZE / 2 ) {
			if ( i < FUNCTABLE_SIZE / 4 ) {
				if ( i == 0 ) {
					tr.triangleTable[i] = EPSILON;
				} else {
					tr.triangleTable[i] = (float)i / (FUNCTABLE_SIZE / 4);
				}
			} else {
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i - FUNCTABLE_SIZE / 4];
			}
		} else {
			tr.triangleTable[i] = -tr.triangleTable[i - FUNCTABLE_SIZE / 2];
		}
	}

	R_InitFogTable();

	R_NoiseInit();

	R_Register();

	max_polys = r_maxpolys->integer;
	max_polyverts = r_maxpolyverts->integer;

	// Allocate backEndData in a persistent arena so it survives
	// Hunk_ClearLevel between async spawn phases AND shows up
	// in /meminfo as "RendererBackEnd". One allocation per arena; the
	// arena is destroyed only on full-shutdown teardown (RE_Shutdown's
	// code != REF_LEVEL_ONLY branch below). Re-grows when r_maxpolys /
	// r_maxpolyverts changes via vid_restart: destroy + recreate.
	{
		size_t bdSize = sizeof( *backEndData )
			+ sizeof( srfPoly_t ) * max_polys
			+ sizeof( polyVert_t ) * max_polyverts;
		if ( !s_backEndStorage || s_backEndStorageSize < bdSize ) {
			if ( s_backEndArena ) {
				ri.Arena_Destroy( s_backEndArena );
				s_backEndArena = NULL;
			}
			/* +256 covers Arena_Alloc's 16-byte alignment padding plus a
			 * little internal arena header slack; the single allocation
			 * we make consumes bdSize bytes after alignment.
			 * ri.Arena_* bridges to qcommon/arena.c in the engine — the
			 * renderer DLL has no direct link to that translation unit. */
			s_backEndArena       = ri.Arena_Create( "RendererBackEnd", bdSize + 256 );
			s_backEndStorage     = ri.Arena_Alloc( s_backEndArena, bdSize, 16 );
			s_backEndStorageSize = bdSize;
			if ( !s_backEndStorage ) {
				ri.Terminate( TERM_UNRECOVERABLE, "R_Init: failed to allocate backEndData (%zu bytes)", bdSize );
			}
		}
		ptr = (byte*)s_backEndStorage;
	}
	backEndData = (backEndData_t *) ptr;
	backEndData->polys = (srfPoly_t *) ((char *) ptr + sizeof( *backEndData ));
	backEndData->polyVerts = (polyVert_t *) ((char *) ptr + sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys);

	R_InitNextFrame();

	InitOpenGL();

	// If InitOpenGL set the recoverable-init-failure flag via R_DeclineInit
	// (e.g. bindless caps decline), bail out of R_Init before R_InitImages
	// and the rest of the texture/pipeline/shader setup — they all assume a
	// working Vulkan state. cl_main's post-BeginRegistration poll picks up
	// re.initFailed and advances cl_renderer to the next renderer.
	if ( s_re.initFailed ) {
		R_LOG( rch_init, SEV_INFO, "R_Init: bailing — renderer declined init (initFailed=qtrue)\n" );
		return;
	}

	R_InitImages();

#ifdef USE_VULKAN
	// eagerly populate the particle pipeline's per-class
	// sampler array (binding 3) with tr.whiteImage now that
	// R_InitImages has created it. vk_init_particle runs from
	// InitOpenGL above — earlier than R_InitImages — so it cannot
	// touch this binding itself. Re-runs on every R_Init, so
	// vid_restart correctly repopulates against the freshly-recreated
	// tr.whiteImage.
	vk_init_particle_textures();

	// Same eager-init for the decal projector's per-texture sampler array
	// (binding 2): vk_init_decal also runs from InitOpenGL before R_InitImages,
	// so its binding-2 array can only be populated here, against tr.whiteImage.
	vk_init_decal_textures();

	// Same eager-init pattern for the shared primitive-shader image
	// registry consumed by the ribbon pipeline (binding 2). All 64
	// slots default to tr.whiteImage; per-shader registrations via
	// RE_RegisterPrimitiveShader (called later from cgame init)
	// overwrite the slots they need.
	vk_init_primitive_shader_images();
#endif

	VarInfo();

#ifdef USE_VULKAN
	vk_create_pipelines();
#endif

	R_InitShaders();

	R_InitSkins();

	R_ModelInit();

	R_InitFreeType();

#ifndef USE_VULKAN
	err = qglGetError();
	if ( err != GL_NO_ERROR )
		R_LOG( rch_init, SEV_WARN, "glGetError() = 0x%x\n", err );
#endif

	R_LOG( rch_init, SEV_INFO, "----- finished R_Init -----\n" );
}


/*
===============
RE_RegisterPersistentCommands
RE_RemovePersistentCommands

process-lifetime command registration.

The screenshot family + sibling introspection commands used to be added by
R_Register (per-level) and removed by the RE_Shutdown loop, which meant they
were absent during every REF_LEVEL_ONLY window — including the loading screen.
They are now registered ONCE per renderer-DLL load from GetRefAPI, and removed
only when the DLL is about to be unloaded (RE_Shutdown's code != REF_LEVEL_ONLY
path). Function-pointer targets live in this DLL so the lifecycle correctly
matches DLL load/unload, not level transitions.
===============
*/
static void RE_RegisterPersistentCommands( void ) {
	ri.Cmd_AddCommand( "imagelist", R_ImageList_f );
	ri.Cmd_AddCommand( "testdds", R_TestDDS_f );
	// Phase 7.15.4-b test harness (default-inert, manual, render-thread): force-evict
	// N oldest unpinned textures, and restore evicted ones — exercises the
	// vk_ral_reregister_image round-trip without the real pressure-driven eviction.
	ri.Cmd_AddCommand( "r_texEvictForce", R_TexEvictForce_f );
	ri.Cmd_AddCommand( "r_texReregisterAll", R_TexReregisterAll_f );
	ri.Cmd_AddCommand( "r_texResidencyBudgetTest", R_TexResidencyBudgetTest_f );
	ri.Cmd_AddCommand( "r_texResidencyMipTest", R_TexResidencyMipTest_f );
	ri.Cmd_AddCommand( "r_texResidencyMaterialTest", R_TexResidencyMaterialTest_f );
	// Phase 7.15.4-c synthetic-pressure test (default-inert): drive the AUTOMATIC
	// eviction drain without real CRITICAL pressure (~4% never triggers it).
	ri.Cmd_AddCommand( "r_texEvictPressureTest", R_TexEvictPressureTest_f );
	ri.Cmd_AddCommand( "shaderlist", R_ShaderList_f );
	ri.Cmd_AddCommand( "skinlist", R_SkinList_f );
	ri.Cmd_AddCommand( "modellist", R_Modellist_f );
	// `screenshot` grammar lives in renderercommon/tr_screenshot.c — one
	// shared parser across all renderer DLLs.
	R_ScreenshotRegisterCommands();
	ri.Cmd_AddCommand( "gfxinfo", GfxInfo_f );
	ri.Cmd_AddCommand( "bspdump", Cmd_BSPDump_f );
#ifdef USE_VULKAN
	ri.Cmd_AddCommand( "vkinfo", VkInfo_f );
	ri.Cmd_AddCommand( "ral_textures", vk_ral_textures_diag_dump );
#endif
}

static void RE_RemovePersistentCommands( void ) {
	ri.Cmd_RemoveCommand( "imagelist" );
	ri.Cmd_RemoveCommand( "testdds" );
	ri.Cmd_RemoveCommand( "shaderlist" );
	ri.Cmd_RemoveCommand( "skinlist" );
	ri.Cmd_RemoveCommand( "modellist" );
	R_ScreenshotUnregisterCommands();
	ri.Cmd_RemoveCommand( "gfxinfo" );
	ri.Cmd_RemoveCommand( "bspdump" );
#ifdef USE_VULKAN
	ri.Cmd_RemoveCommand( "vkinfo" );
	ri.Cmd_RemoveCommand( "ral_textures" );
#endif
}


/*
===============
RE_Shutdown
===============
*/
static void RE_Shutdown( refShutdownCode_t code ) {
	R_LOG( rch_init, SEV_INFO, "RE_Shutdown( %i )\n", code );

	// the persistent introspection + screenshot command set
	// is owned by RE_RegisterPersistentCommands / RE_RemovePersistentCommands.
	// Only deregister when the DLL is about to be unloaded (any path other
	// than REF_LEVEL_ONLY — CL_ShutdownRef unconditionally unloads the DLL
	// after re.Shutdown returns). On REF_LEVEL_ONLY we deliberately keep them
	// alive so screenshots etc. continue to work across map transitions.
	if ( code != REF_LEVEL_ONLY ) {
		RE_RemovePersistentCommands();
	}

	// `shaderstate` is never registered in renderervk's R_Register, but the
	// removal is preserved as a defensive no-op (matches the historical
	// behaviour of the RE_Shutdown removal loop, and harmlessly catches the
	// case where a future patch starts registering it).
	ri.Cmd_RemoveCommand( "shaderstate" );

	// Release the prop lightmap page-pointer array. The image_t* within it are
	// hunk-owned and freed by R_DeleteTextures; only the ri.Malloc'd array itself
	// needs explicit cleanup.
	if ( tr.propLightmaps ) {
		ri.Free( tr.propLightmaps );
		tr.propLightmaps    = NULL;
		tr.numPropLightmaps = 0;
		tr.maxPropLightmaps = 0;
	}

	// Destroy textures unconditionally — including for REF_LEVEL_ONLY.
	//
	// The original gate `if ( code != REF_LEVEL_ONLY )` was intended to
	// preserve GPU textures across map-load transitions so the frame loop
	// could draw the loading screen between spawn phases.  In practice
	// Wired's Hunk_ClearLevel (qcommon/common.c) zeroes both hunk_high AND
	// hunk_low, freeing the image_t structs that tr.images[] points to.
	// Skipping the destroy on the level-only path therefore left VkImages
	// alive on the GPU with no remaining bookkeeping to find or destroy
	// them — they leaked until vkDestroyDevice
	// (VUID-vkDestroyDevice-device-05137).
	//
	// Vulkan ordering: VkImages must be destroyed BEFORE their backing
	// VkDeviceMemory (image_chunks) is freed; pair R_DeleteTextures with
	// vk_release_resources so the order is always correct.
	//
	// Teardown window: from here the renderer releases GPU resources (textures,
	// then the RAL backend / adopted images / pipelines). Do NOT pump a render
	// frame in this window — it would reference freed GPU resources mid-tear-down.
	R_DeleteTextures();
#ifdef USE_VULKAN
	if ( code != REF_LEVEL_ONLY ) {
		// Temporal's active-only generic layout borrows the RAL bindless BGL.
		// Release layout -> targets -> A2a payload/adoption while that parent and
		// the backend are still alive; raw entMat teardown remains in vk_shutdown.
		vk_temporal_motion_release_before_ral_shutdown();
	}
	// destroyWindow gate threaded through
	// so REF_LEVEL_ONLY (map-transition partial teardown) leaves the RAL
	// backend + sibling pipelines + adopted bindgroups alive across the
	// transition. Mirrors vk_shutdown's preservation of vk.device et al.
	// REF_LEVEL_ONLY = 0 = !destroyWindow ⇒ vk_ral_textures_shutdown
	// returns after the diag dump. REF_KEEP_WINDOW / REF_DESTROY_WINDOW /
	// REF_UNLOAD_DLL run the full teardown.
	vk_ral_textures_shutdown( code != REF_LEVEL_ONLY );
	vk_release_resources();
#endif

	if ( code != REF_LEVEL_ONLY ) {
		R_DoneFreeType();
	}

#ifdef USE_VULKAN
	if ( r_device->modificationCount != s_r_device_mod ) {
		code = REF_UNLOAD_DLL;
	}
#endif

	// shut down platform specific OpenGL/Vulkan stuff
	if ( code != REF_LEVEL_ONLY ) {
#ifdef USE_VULKAN
		vk_shutdown( code );

		memset( &glState, 0, sizeof( glState ) );

		if ( code != REF_KEEP_WINDOW ) {
			if ( ri.VKimp_Shutdown ) {
				ri.VKimp_Shutdown( code == REF_UNLOAD_DLL ? qtrue : qfalse );
			}
			memset( &glConfig, 0, sizeof( glConfig ) );
		}
#else
		R_ClearSymTables();
		memset( &glState, 0, sizeof( glState ) );

		if ( code != REF_KEEP_WINDOW ) {
			if ( ri.GLimp_Shutdown ) {
				ri.GLimp_Shutdown( code == REF_UNLOAD_DLL ? qtrue : qfalse );
			}
			memset( &glConfig, 0, sizeof( glConfig ) );
		}
#endif
	}

	if ( code != REF_LEVEL_ONLY ) {
		ri.FreeAll();
		// Also release the persistent backEndData arena.
		if ( s_backEndArena ) {
			ri.Arena_Destroy( s_backEndArena );
			s_backEndArena = NULL;
		}
		s_backEndStorage     = NULL;
		s_backEndStorageSize = 0;
	}

	tr.registered = qfalse;
	tr.inited = qfalse;
}


/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
static void RE_EndRegistration( void ) {
#ifdef USE_VULKAN
	vk_wait_idle();
	// command buffer is not in recording state at this stage
	// so we can't issue RB_ShowImages() there
#else
	R_IssuePendingRenderCommands();
	if ( !ri.Sys_LowPhysicalMemory() ) {
		RB_ShowImages();
	}
#endif
}


/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI
@@@@@@@@@@@@@@@@@@@@@
*/
#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t* QDECL GetRefAPI ( int apiVersion, refimport_t *rimp ) {
#else
refexport_t *GetRefAPI ( int apiVersion, refimport_t *rimp ) {
#endif

#define re s_re

	ri = *rimp;

	memset( &re, 0, sizeof( re ) );

	if ( apiVersion != REF_API_VERSION ) {
		R_LOG( rch_init, SEV_INFO, "Mismatched REF_API_VERSION: expected %i, got %i\n",
			REF_API_VERSION, apiVersion );
		return NULL;
	}

	// the RE_ functions are Renderer Entry points

	re.Shutdown = RE_Shutdown;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.RegisterShaderLightMap = RE_RegisterShaderLightMap;
	re.RegisterMSDFShader = RE_RegisterMSDFShader;
	re.RegisterPrimitiveShader = RE_RegisterPrimitiveShader;
	re.PinShaderImages = RE_PinShaderImages;
	re.LoadWorld = RE_LoadWorldMap;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;

	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;
	re.GetGpuProfileSample = vk_gpu_profile_sample;

	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;

	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddRefEntityToSceneTemporal = RE_AddRefEntityToSceneTemporal;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.LightForPoint = R_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.AddLinearLightToScene = RE_AddLinearLightToScene;
	re.AddRibbonToScene = RE_AddRibbonToScene;
	re.AddBeamToScene = RE_AddBeamToScene;
	re.AddRailRibbonToScene = RE_AddRailRibbonToScene;
	re.AddSpriteToScene = RE_AddSpriteToScene;
	re.EmitParticles = RE_EmitParticles;
	re.AddDecalToScene = RE_AddDecalToScene;
	re.RegisterParticleClass = RE_RegisterParticleClass;
	re.SetAtmosphere = RE_SetAtmosphere;
	re.SetAtmosphereHeightgrid = RE_SetAtmosphereHeightgrid;
	re.AddLensSourceToScene = RE_AddLensSourceToScene;
	re.GetLensVisibility = RE_GetLensVisibility;
#if FEAT_HALO
	re.AddHaloToScene = RE_AddHaloToScene;
#endif
#if FEAT_FOG_SYSTEM
	re.GetGlobalFog = RE_GetGlobalFog;
	re.GetViewFog = RE_GetViewFog;
#endif

	re.RenderScene = RE_RenderScene;

	re.SetColor = RE_SetColor;
	re.SetClipRegion = RE_SetClipRegion;
	re.SetMSDFOutline = RE_SetMSDFOutline;
	re.SetMSDFShadow  = RE_SetMSDFShadow;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawMenuBackdrop = RE_DrawMenuBackdrop;
	re.DrawStretchPicOverlay = RE_StretchPicOverlay;
	re.DrawRotatedPic = RE_RotatedPic;
	re.DrawLine = RE_DrawLine;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;

	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = RE_RemapShader;
	re.GetEntityToken = RE_GetEntityToken;
	re.inPVS = R_inPVS;

	re.TakeVideoFrame = RE_TakeVideoFrame;
	re.SetColorMappings = R_SetColorMappings;

	re.ThrottleBackend = RE_ThrottleBackend;
	re.FinishBloom = RE_FinishBloom;
	re.CanMinimize = RE_CanMinimize;
	re.GetConfig = RE_GetConfig;
	re.GetMemoryBudget = vk_ral_query_memory_budget;
	re.VertexLighting = RE_VertexLighting;
	re.SyncRender = RE_SyncRender;

#if FEAT_IQM
	re.GetIQMAnimations = R_GetIQMAnimations;
#endif // FEAT_IQM
	re.GetMDLAnimations = R_GetMDLAnimations;

	re.SetLightstylePattern = RE_SetLightstylePattern;

	// register the persistent command set ONCE per
	// renderer-DLL load. ri (the engine import table) is populated above
	// (`ri = *rimp;`) so ri.Cmd_AddCommand is valid here. Survives
	// REF_LEVEL_ONLY teardown; removed by RE_Shutdown when code != REF_LEVEL_ONLY
	// (immediately before CL_ShutdownRef unloads the DLL).
	RE_RegisterPersistentCommands();

	return &re;
#undef re
}
