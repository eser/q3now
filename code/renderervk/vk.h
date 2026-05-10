#pragma once

#include "../renderercommon/vulkan/vulkan.h"
#include "tr_common.h"
#include "../qcommon/q_feats.h"

#define MAX_SWAPCHAIN_IMAGES 8
#define MIN_SWAPCHAIN_IMAGES_IMM 3
#define MIN_SWAPCHAIN_IMAGES_FIFO   3
#define MIN_SWAPCHAIN_IMAGES_FIFO_0 4
#define MIN_SWAPCHAIN_IMAGES_MAILBOX 3
#define MIN_SWAPCHAIN_IMAGES_FIFO_LATEST_READY 3

#define MAX_VK_SAMPLERS 32
#define MAX_VK_PIPELINES ((1024 + 128)*2)

#define VERTEX_BUFFER_SIZE     (4 * 1024 * 1024)  /* by default */
#define VERTEX_BUFFER_SIZE_HI  (8 * 1024 * 1024)

#define STAGING_BUFFER_SIZE    (2 * 1024 * 1024)  /* by default */
#define STAGING_BUFFER_SIZE_HI (24 * 1024 * 1024) /* enough for max.texture size upload with all mip levels at once */

#define IMAGE_CHUNK_SIZE (32 * 1024 * 1024)
#define MAX_IMAGE_CHUNKS 56

#define NUM_COMMAND_BUFFERS 2	// double-buffered: paces CPU with GPU per-frame, prevents race-ahead bursting (matches MoltenVK's 3-drawable cap)

#define USE_REVERSED_DEPTH

#define USE_UPLOAD_QUEUE

#define VK_NUM_BLOOM_PASSES 4

#ifndef _DEBUG
#define USE_DEDICATED_ALLOCATION
#endif
//#define MIN_IMAGE_ALIGN (128*1024)
#define MAX_ATTACHMENTS_IN_POOL (8+VK_NUM_BLOOM_PASSES*2+3) // +3 for SMAA edges/blend/input when active

#define VK_DESC_STORAGE      0
#define VK_DESC_UNIFORM      0
#define VK_DESC_TEXTURE0     1
#define VK_DESC_TEXTURE1     2
#define VK_DESC_TEXTURE2     3
#define VK_DESC_FOG_COLLAPSE 4
#define VK_DESC_DEPTH_FADE   5
#define VK_DESC_COUNT        6  // base descriptor count (sets 0-5)
#define VK_DESC_NORMALMAP    6  // set=6: parallax normalmap or Q1 anim next-frame sampler

#define VK_DESC_TEXTURE_BASE VK_DESC_TEXTURE0
#define VK_DESC_FOG_ONLY     VK_DESC_TEXTURE1
#define VK_DESC_FOG_DLIGHT   VK_DESC_TEXTURE1

typedef enum {
	TYPE_COLOR_BLACK,
	TYPE_COLOR_WHITE,
	TYPE_COLOR_GREEN,
	TYPE_COLOR_RED,
	TYPE_FOG_ONLY,
	TYPE_DOT,
	TYPE_MSDF,

	TYPE_SINGLE_TEXTURE_LIGHTING,
	TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR,
#if FEAT_PARALLAX_MAPPING
	TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX,
	TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX_LINEAR,
#endif
#if FEAT_ADVANCED_WATER
	TYPE_WATER,
#endif
#if FEAT_SHADOW_MAPPING
	TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW,
	TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW_LINEAR,
	TYPE_SHADOW_DEPTH,
#endif
#if FEAT_PBR
	TYPE_SINGLE_TEXTURE_LIGHTING_PBR,
	TYPE_SINGLE_TEXTURE_LIGHTING_PBR_LINEAR,
#endif

	TYPE_SINGLE_TEXTURE_DF,

	TYPE_LIGHTSTYLES,		// Q1 4-style lightmap blend, animChain lerp via set=6
	TYPE_LIGHTSTYLES_ARRAY,	// Q1 4-style lightmap blend, GPU time-driven texture array

	TYPE_GENERIC_BEGIN, // start of non-env/env shader pairs
	TYPE_SIGNLE_TEXTURE = TYPE_GENERIC_BEGIN,
	TYPE_SINGLE_TEXTURE_ENV,

	TYPE_SINGLE_TEXTURE_IDENTITY,
	TYPE_SINGLE_TEXTURE_IDENTITY_ENV,

	TYPE_SINGLE_TEXTURE_FIXED_COLOR,
	TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV,

	TYPE_SINGLE_TEXTURE_ENT_COLOR,
	TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV,

	TYPE_MULTI_TEXTURE_ADD2_IDENTITY,
	TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV,

	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV,

	TYPE_MULTI_TEXTURE_MUL2,
	TYPE_MULTI_TEXTURE_MUL2_ENV,
	TYPE_MULTI_TEXTURE_ADD2_1_1,
	TYPE_MULTI_TEXTURE_ADD2_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD2,
	TYPE_MULTI_TEXTURE_ADD2_ENV,

	TYPE_MULTI_TEXTURE_MUL3,
	TYPE_MULTI_TEXTURE_MUL3_ENV,
	TYPE_MULTI_TEXTURE_ADD3_1_1,
	TYPE_MULTI_TEXTURE_ADD3_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD3,
	TYPE_MULTI_TEXTURE_ADD3_ENV,

	TYPE_BLEND2_ADD,
	TYPE_BLEND2_ADD_ENV,
	TYPE_BLEND2_MUL,
	TYPE_BLEND2_MUL_ENV,
	TYPE_BLEND2_ALPHA,
	TYPE_BLEND2_ALPHA_ENV,
	TYPE_BLEND2_ONE_MINUS_ALPHA,
	TYPE_BLEND2_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND2_MIX_ALPHA,
	TYPE_BLEND2_MIX_ALPHA_ENV,

	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND2_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_BLEND3_ADD,
	TYPE_BLEND3_ADD_ENV,
	TYPE_BLEND3_MUL,
	TYPE_BLEND3_MUL_ENV,
	TYPE_BLEND3_ALPHA,
	TYPE_BLEND3_ALPHA_ENV,
	TYPE_BLEND3_ONE_MINUS_ALPHA,
	TYPE_BLEND3_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND3_MIX_ALPHA,
	TYPE_BLEND3_MIX_ALPHA_ENV,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND3_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_GENERIC_END = TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV

} Vk_Shader_Type;

// used with cg_shadows == 2
typedef enum {
	SHADOW_DISABLED,
	SHADOW_EDGES,
	SHADOW_FS_QUAD,
} Vk_Shadow_Phase;

typedef enum {
	TRIANGLE_LIST = 0,
	TRIANGLE_STRIP,
	LINE_LIST,
	POINT_LIST
} Vk_Primitive_Topology;

typedef enum {
	DEPTH_RANGE_NORMAL,		// [0..1]
	DEPTH_RANGE_ZERO,		// [0..0]
	DEPTH_RANGE_ONE,		// [1..1]
	DEPTH_RANGE_WEAPON,		// [0..0.3]
	DEPTH_RANGE_COUNT
}  Vk_Depth_Range;

typedef struct {
	VkSamplerAddressMode address_mode; // clamp/repeat texture addressing mode
	int gl_mag_filter;		// GL_XXX mag filter
	int gl_min_filter;		// GL_XXX min filter
	qboolean max_lod_1_0;	// fixed 1.0 lod
	qboolean noAnisotropy;
} Vk_Sampler_Def;

typedef enum {
	RENDER_PASS_MAIN = 0,
	RENDER_PASS_SCREENMAP,
	RENDER_PASS_POST_BLOOM,
	RENDER_PASS_COUNT
} renderPass_t;

typedef struct {
	Vk_Shader_Type shader_type;
	unsigned int state_bits; // GLS_XXX flags
	cullType_t face_culling;
	qboolean polygon_offset;
	qboolean mirror;
	Vk_Shadow_Phase shadow_phase;
	Vk_Primitive_Topology primitives;
	int line_width;
	int fog_stage; // off, fog-in / fog-out
	int abs_light;
	int allow_discard;
	int acff; // none, rgb, rgba, alpha
	struct {
		byte rgb;
		byte alpha;
	} color;
} Vk_Pipeline_Def;

typedef struct VK_Pipeline {
	Vk_Pipeline_Def def;
	VkPipeline handle[ RENDER_PASS_COUNT ];
} VK_Pipeline_t;

// this structure must be in sync with shader uniforms!
typedef struct vkUniform_s {
	// light/env parameters:
	vec4_t eyePos;				// vertex
	union {
		struct {
			vec4_t pos;			// vertex: light origin
			vec4_t color;		// fragment: rgb + 1/(r*r)
			vec4_t vector;		// fragment: linear dynamic light
		} light;
		struct {
			vec4_t color[3];	// ent.color[3]
		} ent;
	};
	// fog parameters:
	vec4_t fogDistanceVector;	// vertex
	vec4_t fogDepthVector;		// vertex
	vec4_t fogEyeT;				// vertex
	vec4_t fogColor;			// fragment
	// lightstyle per-surface blend weights (x=slot0, y=slot1, z=slot2, w=slot3)
	vec4_t q1StyleIntensities;
} vkUniform_t;

#define TESS_XYZ   (1)
#define TESS_RGBA0 (2)
#define TESS_RGBA1 (4)
#define TESS_RGBA2 (8)
#define TESS_ST0   (16)
#define TESS_ST1   (32)
#define TESS_ST2   (64)
#define TESS_NNN   (128)
#define TESS_VPOS  (256)  // uniform with eyePos
#define TESS_ENV   (512)  // mark shader stage with environment mapping
#define TESS_ENT0  (1024) // uniform with ent.color[0]
#define TESS_ENT1  (2048) // uniform with ent.color[1]
#define TESS_ENT2  (4096) // uniform with ent.color[2]
//
// Initialization.
//

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize( void );

// Called after initialization or renderer restart
void vk_init_descriptors( void );

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown( refShutdownCode_t code );

// Releases vulkan resources allocated during program execution.
// This effectively puts vulkan subsystem into initial state (the state we have after vk_initialize call).
void vk_release_resources( void );

void vk_wait_idle( void );
void vk_queue_wait_idle( void );

#if FEAT_SHADOW_MAPPING
struct dlight_s;
void vk_render_shadow_map( const struct dlight_s *dl );
#endif

//
// Resources allocation.
//
void vk_create_image( image_t *image, int width, int height, int mip_levels );
void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int miplevels, byte *pixels, int size, qboolean update, uint32_t baseArrayLayer );
void vk_update_descriptor_set( image_t *image, qboolean mipmap );
void vk_destroy_image_resources( VkImage *image, VkImageView *imageView );
void vk_update_attachment_descriptors( void );
void vk_destroy_samplers( void );

uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );
void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def );

void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height );
void vk_create_pipelines( void );

//
// Rendering setup.
//

void vk_clear_color( const vec4_t color );
void vk_clear_depth( qboolean clear_stencil );
void vk_begin_frame( void );
void vk_end_frame( void );
void vk_present_frame( void );

void vk_end_render_pass( void );
void vk_begin_main_render_pass( void );
void vk_depth_fade_copy( void );
void vk_smaa( void );

void vk_bind_pipeline( uint32_t pipeline );
void vk_bind_index( void );
void vk_bind_index_ext( const int numIndexes, const uint32_t*indexes );
void vk_bind_geometry( uint32_t flags );
void vk_bind_lighting( int stage, int bundle );
void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed );
void vk_draw_dot( uint32_t storage_offset );

void vk_read_pixels( byte* buffer, uint32_t width, uint32_t height ); // screenshots
qboolean vk_bloom( void );

qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size );
void vk_update_mvp( const float *m );
#if FEAT_FOG_SYSTEM
// Push fog parameters as a fragment-stage push constant.
// Layout: vec4 fogColor (rgb + density), vec4 fogTypeFarClip (type, farClip, enabled, _pad)
// offset = 64 bytes (after MVP), size = 32 bytes.
void vk_update_fog_push( const vec4_t color, int fogType, float density, float farClip, qboolean enabled );
#endif
// Set a 2D scissor rect on the current command buffer. Pass NULL to restore
// the fullscreen scissor (equivalent to "no clip region").
void vk_set_2d_scissor( const int *rect );
void vk_update_msdf_outline( float outlineWidth, const float *outlineColor,
                              float glowWidth, const float *glowColor,
                              const float *shadowOffset, const float *shadowColor );

uint32_t vk_tess_index( uint32_t numIndexes, const void *src );
void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset );
#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex );
#endif
void vk_reset_descriptor( int index );
void vk_update_descriptor( int index, VkDescriptorSet descriptor );
void vk_update_descriptor_offset( int index, uint32_t offset );

void vk_update_post_process_pipelines( void );

const char *vk_format_string( VkFormat format );

// Phase 9C: print the HDR pipeline state snapshot captured during
// setup_surface_formats(). Called from GfxInfo (tr_init.c) so the
// existing `gfxinfo` console command shows current HDR plumbing
// without per-frame logging.
void vk_hdr_state_print( void );

void VBO_PrepareQueues( void );
void VBO_PrepareSubqueue( int start, int count );
int  VBO_GetQueueCount( void );
uint32_t VBO_GetQueueItemStylesPacked( int pos );
void VBO_RenderIBOItems( void );
void VBO_ClearQueue( void );

#if FEAT_IQM
// IQM GPU skinning
void vk_init_iqm_gpu_skinning( void );
void vk_shutdown_iqm_gpu_skinning( void );
qboolean vk_create_iqm_vbo( VkBuffer *outVertBuf, VkDeviceMemory *outVertMem,
	VkBuffer *outIdxBuf, VkDeviceMemory *outIdxMem,
	const byte *vertData, int vertSize,
	const byte *idxData, int idxSize );
void vk_destroy_iqm_vbo( VkBuffer *vertBuf, VkDeviceMemory *vertMem,
	VkBuffer *idxBuf, VkDeviceMemory *idxMem );
void vk_draw_iqm_gpu( VkBuffer vertBuffer, VkBuffer idxBuffer,
	int firstIndex, int numIndexes,
	const float *boneMats, int numBones,
	VkDescriptorSet textureDescriptor,
	const float *mvp );
#endif

typedef struct vk_tess_s {
	VkCommandBuffer command_buffer;

	VkSemaphore image_acquired;
	uint32_t	swapchain_image_index;
	qboolean	swapchain_image_acquired;
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished2;
#endif
	VkFence rendering_finished_fence;
	qboolean waitForFence;

	VkBuffer vertex_buffer;
	byte *vertex_buffer_ptr; // pointer to mapped vertex buffer
	uint32_t vertex_buffer_offset; // VkDeviceSize

	VkDescriptorSet uniform_descriptor;
	uint32_t		uniform_read_offset;
	VkDeviceSize	buf_offset[8];
	VkDeviceSize	vbo_offset[8];

	VkBuffer		curr_index_buffer;
	uint32_t		curr_index_offset;

	struct {
		uint32_t		start, end;
		VkDescriptorSet	current[VK_DESC_COUNT + 1]; // 0:uniform, 1:color0, 2:color1, 3:color2, 4:fog, 5:depth_fade, 6:normalmap/Q1-anim-next
		uint32_t		offset[1]; // 0 (uniform)
	} descriptor_set;

	Vk_Depth_Range		depth_range;
	VkPipeline			last_pipeline;
	/* Layout the most-recently-bound pipeline was created with. Used by
	 * vk_bind_descriptor_sets so the BindDescriptorSets call passes a layout
	 * compatible with the bound pipeline (matters for the MSDF pipeline,
	 * which uses vk.pipeline_layout_msdf — different push-constant ranges
	 * from the main vk.pipeline_layout).
	 * VK_NULL_HANDLE = no pipeline bound; bind path falls back to
	 * vk.pipeline_layout (the historical hardcoded default). */
	VkPipelineLayout	last_pipeline_layout;

	uint32_t num_indexes; // value from most recent vk_bind_index() call

	VkRect2D scissor_rect;
} vk_tess_t;


// ── shared primitive shader image registry ──────────────────────────
//
// Global registry of resolved image_t* per primitive shader, used by
// wired primitive pipelines that need texturing. **Slot index ≠ qhandle.**
// Phase 5K decoupled the two: the qhandle space (MAX_SHADERS=16384) is
// monotonic across all RE_RegisterShader calls (world textures, models,
// UI, primitive); the registry has only PRIMITIVE_SHADER_IMAGE_MAX = 64
// slots. RE_RegisterPrimitiveShader allocates a slot per shader via
// vk_alloc_primitive_shader_image_slot and records the mapping in
// vk.qhandle_to_prim_slot[]. SSBO write sites translate cgame-supplied
// qhandles to slots via vk_qhandle_to_prim_slot at submit time.
//
// Slot 0 is reserved for tr.whiteImage. Slots 1..63 are dynamically
// assigned to registered primitive shaders. Out-of-range or
// unregistered qhandles map to slot 0, rendering "untextured" (white
// texel × vertex color = vertex color) instead of producing OOB SSBO
// reads.
//
// Consumers: ribbon (binding 2), beam (binding 1). Particle has its
// own per-class registry (vk.particle.classImages[]) because it
// indexes by particleClassHandle_t, not qhandle_t.
//
// vk_register_primitive_shader_image idempotently writes both the
// host array slot and every dependent primitive's descriptor set.
#define PRIMITIVE_SHADER_IMAGE_MAX 64

// Phase 5K: upper bound on the qhandle space used to size the
// qhandle→primitive-slot indirection table. Must equal MAX_SHADERS
// in tr_local.h (1<<SHADERNUM_BITS = 16384). vk.h is included by
// tr_local.h before MAX_SHADERS is defined, so we need a parallel
// constant here; a _Static_assert in vk.c verifies they agree.
#define VK_PRIM_QHANDLE_MAX 16384

extern struct image_s *vk_primitive_shader_images[PRIMITIVE_SHADER_IMAGE_MAX];

void vk_init_primitive_shader_images( void );
// Phase 5K: takes a primitive registry slot index (NOT a qhandle).
// Slot range [1, PRIMITIVE_SHADER_IMAGE_MAX); slot 0 is reserved for
// tr.whiteImage. Callers from RE_RegisterPrimitiveShader should go
// through vk_alloc_primitive_shader_image_slot instead, which is the
// stable allocator that maintains the qhandle→slot lookup table.
void vk_register_primitive_shader_image( int slot, struct image_s *image );

// Phase 5F: allocate (or reuse) a registry slot for a stage>0 image
// whose qhandle slot is already taken by stage 0's image. Linear
// search: returns existing slot if `image` is already registered;
// otherwise writes into the first slot still holding tr.whiteImage
// and broadcasts the descriptor write. Returns -1 on exhaustion.
int  vk_alloc_primitive_shader_image_slot( struct image_s *image );
void vk_shutdown_primitive_stages( void );

// Phase 5K: translate a cgame qhandle (whatever value
// RE_RegisterShader returned for the primitive shader) into the
// engine-internal primitive registry slot (0..PRIMITIVE_SHADER_IMAGE_MAX-1).
// Used at SSBO write time to pack a small slot index into the GPU
// header where the previous design had relied on (qhandle == slot).
// Out-of-range or unregistered qhandles return slot 0 (whiteImage),
// rendering as untextured rather than producing OOB SSBO reads.
unsigned int vk_qhandle_to_prim_slot( qhandle_t h );

// Phase 5F: per-stage rendering parameters for multi-stage primitive
// shaders, packed for the GPU SSBO. std430, 32 bytes per entry,
// indexed [shaderHandle * PRIMITIVE_STAGE_MAX + stageNumber].
//
// Trailing entries (stageNumber >= stageCount) are zero-initialized
// and should not be drawn — caller checks
// vk.primitive_shader_stage_counts[handle] before per-stage draws.
//
// blendPacked encodes (srcBlend << 16) | dstBlend so the 32B target
// fits without padding. GLSL unpacks via shifts.
#define VK_PRIMITIVE_STAGE_BYTES 32u

typedef struct {
	uint32_t imageSlot;          // bytes  0..3
	uint32_t blendPacked;        // bytes  4..7   (src << 16) | dst
	uint32_t rgbGen;             // bytes  8..11  primRgbGen_t
	uint32_t alphaGen;           // bytes 12..15  primAlphaGen_t
	float    uvScale[2];         // bytes 16..23
	float    uvScroll[2];        // bytes 24..31
} VkPrimitiveStageGPU;

#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
_Static_assert( sizeof( VkPrimitiveStageGPU ) == VK_PRIMITIVE_STAGE_BYTES,
	"VkPrimitiveStageGPU must be 32 bytes (std430-aligned, vec4-stride friendly)" );
#endif

// ── primitive particle (host-side type definitions) ────────────────
//
// These live outside Vk_Instance because C does not allow nested
// typedefs inside a struct definition. The particle subsystem state
// itself (buffers / pipelines / descriptor sets / cursors) lives as
// the `vk.particle` member inside Vk_Instance below.
//
// PARTICLE_BYTES (64) and PARTICLE_CLASS_GPU_BYTES (400) are the
// std430 sizes computed for the matching GLSL structs in
// particle_integrate.comp / particle.vert. PARTICLES_PER_POOL is the
// fixed pool capacity. _Static_assert in vk_init_particle catches
// any drift between this C layout and the std430 stride.
#define PARTICLES_PER_POOL          16384u
#define PARTICLE_BYTES                 64u  // sizeof(GPU Particle), std430
#define PARTICLE_CLASS_GPU_BYTES      400u  // sizeof(ParticleClassGPU), std430

// Host-side mirror of GLSL std430 ParticleClassGPU. Field order +
// trailing pads MUST exactly match particle_integrate.comp /
// particle.vert. Padding is required because std430 rounds the
// struct's stride up to its largest member alignment (vec4 = 16);
// the natural C packing would leave 8 trailing bytes off, causing
// silent misreads from element 1 onward in the classes[] SSBO.
//
// shaderBlendIsAdditive (consumed phase 5): derived at class
// registration time from `cls->shader`'s stateBits (additive vs
// alpha-blend). Replaces the cgame-supplied PRIM_FLAG_ADDITIVE bit
// for blend-variant filtering — particle pipeline now honors the
// underlying shader script's blendFunc, matching CPU rendering
// semantics. The cgame `renderFlags` field still ships through but
// is informational only; see primitives.h.
typedef struct {
	uint32_t shader;
	uint32_t renderFlags;
	uint32_t emitMode;
	uint32_t scatterShape;
	float    scatterMagnitude;
	uint32_t velocityShape;
	float    axialSpeed;
	float    cubeJitter;
	float    coneHalfAngle;
	float    lifetimeMean;
	float    lifetimeJitter;
	int32_t  paletteCount;
	vec4_t   colorPalette[16];   // PARTICLE_CLASS_MAX_PALETTE
	vec4_t   colorEndMult;
	float    sizeStart;
	float    sizeEnd;
	float    gravityScale;
	float    drag;
	uint32_t shaderBlendIsAdditive;  // was pad0 — phase 5; 0=alpha, 1=additive
	uint32_t pad1;
	uint32_t pad2;
	uint32_t pad3;
	// Expressivity extension. Appended at the end so previously-set
	// offsets (palette, colorEndMult, size/gravity/drag,
	// shaderBlendIsAdditive) stay byte-identical. .w lanes on the two
	// vec4s are unused — only .xyz carry data — but storing as vec4
	// avoids std430 vec3-stride traps and keeps offsets 16-aligned.
	vec4_t   velocityBias;           // 352..367; .xyz = constant added to vel post-shape
	vec4_t   velocityBiasJitter;     // 368..383; .xyz = symmetric crandom() per-axis jitter
	float    speedJitter;            // 384..387; axialSpeed scatter, picked at emit
	float    sizeJitter;             // 388..391; sizeStart scatter, picked at emit
	uint32_t pad4;                   // 392..395
	uint32_t pad5;                   // 396..399; total stride 400 B (= 25 * vec4)
} particleClassGPU_t;

// Host-side mirror of GLSL std430 Particle (per-particle pool slot).
// Layout MUST exactly match the Particle struct in
// particle_integrate.comp / particle.vert. Size is fixed at 64 B
// (PARTICLE_BYTES); a sizeof check in vk_init_particle catches drift.
//
// In std430 a `vec3` followed by a `float` packs into a single 16 B
// slot (offset 0..11 for vec3, 12..15 for float), so two such pairs
// occupy 32 B. The next 16 B carries classHandle + paletteIndex +
// sizeJitterPick + pad1, and the trailing 16 B is reserved (pad2..5)
// to round the per-element stride up to 64 B (= 4 * vec4); std430
// rounds the array element stride up to the largest member's
// alignment (vec3 → 16 B), so sizes between 16-multiples would still
// pad implicitly — making the extra fields explicit avoids surprise
// at host-side memcpy time.
typedef struct {
	vec3_t   pos;            //  0..11
	float    age;            // 12..15
	vec3_t   vel;            // 16..27
	float    lifetimeInv;    // 28..31
	uint32_t classHandle;    // 32..35  (1..MAX_PARTICLE_CLASSES, 0 = dead)
	uint32_t paletteIndex;   // 36..39  index into class colorPalette[]; rand() % paletteCount at emit
	float    sizeJitterPick; // 40..43  per-particle sizeStart offset = crandom() * cls->sizeJitter
	uint32_t pad1;           // 44..47
	uint32_t pad2;           // 48..51
	uint32_t pad3;           // 52..55
	uint32_t pad4;           // 56..59
	uint32_t pad5;           // 60..63  total stride 64 B
} particleGPU_t;

// Host-side mirror of GLSL std140 ParticleFrame. 144 B; std140
// rounds vec4 / mat4 members to 16 B alignment, scalars to 4 B.
//
// Three write owners during a frame:
//   render region  (bytes   0..111, 112 B): mvp, viewLeft, viewUp,
//                                           eyeWorld. Filled in
//                                           RB_DrawParticles, where
//                                           backEnd.viewParms is valid.
//   compute region (bytes 112..127,  16 B): dt, poolSize, numClasses,
//                                           pingPongRead. Filled in
//                                           RB_RunParticleCompute,
//                                           which now runs from
//                                           vk_begin_frame BEFORE the
//                                           main render pass opens.
//   shared region  (bytes 128..143,  16 B): identityLight + pad.
//                                           tr.identityLight (CGEN_VERTEX
//                                           halving factor; 1/2^overbrightBits).
//                                           Read by particle.frag to
//                                           match the engine's
//                                           half-bright vertex-color
//                                           convention. Written from
//                                           RB_RunParticleCompute (the
//                                           earliest per-frame UBO
//                                           touch, runs before any
//                                           render-pass open). Updated
//                                           live when r_brightness or
//                                           r_mapBrightness changes;
//                                           the cvar onChange callback
//                                           updates tr.identityLight
//                                           and the next frame's UBO
//                                           write picks it up.
// Regions are contiguous and disjoint; do NOT memcpy the entire struct
// from either site — that races / overwrites the other site's fields.
typedef struct {
	// ── render region (filled in RB_DrawParticles) ──────────────
	float    mvp[16];        //   0..63
	float    viewLeft[4];    //  64..79
	float    viewUp[4];      //  80..95
	float    eyeWorld[4];    //  96..111
	// ── compute region (filled in RB_RunParticleCompute) ────────
	float    dt;             // 112..115
	uint32_t poolSize;       // 116..119
	uint32_t numClasses;     // 120..123
	uint32_t pingPongRead;   // 124..127
	// ── shared region (filled in RB_RunParticleCompute) ─────────
	float    identityLight;  // 128..131  tr.identityLight; consumed by particle.frag
	float    pad0;           // 132..135  std140 vec4-stride pad
	float    pad1;           // 136..139
	float    pad2;           // 140..143  total stride 144 B (= 9 * vec4)
} particleFrame_t;


// Vk_Instance contains engine-specific vulkan resources that persist entire renderer lifetime.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
typedef struct {
	VkPhysicalDevice physical_device;
	VkSurfaceFormatKHR base_format;
	VkSurfaceFormatKHR present_format;

	uint32_t queue_family_index;
	VkDevice device;
	VkQueue queue;

	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count;
	VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
	VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGES];
	VkSemaphore swapchain_rendering_finished[MAX_SWAPCHAIN_IMAGES];
	//uint32_t swapchain_image_index;

	VkCommandPool command_pool;
#ifdef USE_UPLOAD_QUEUE
	VkCommandBuffer staging_command_buffer;
#endif

	VkDeviceMemory image_memory[ MAX_ATTACHMENTS_IN_POOL ];
	uint32_t image_memory_count;

	struct {
		VkRenderPass main;
		VkRenderPass screenmap;
		VkRenderPass gamma;
		VkRenderPass capture;
		VkRenderPass bloom_extract;
		VkRenderPass blur[VK_NUM_BLOOM_PASSES*2]; // horizontal-vertical pairs
		VkRenderPass post_bloom;
		VkRenderPass depth_fade;	// loads color+depth, for soft particle rendering
		VkRenderPass smaa_edge;		// SMAA edge detection (R8G8)
		VkRenderPass smaa_blend;	// SMAA blend weight (RGBA8)
		VkRenderPass smaa_resolve;	// SMAA resolve (writes to color_image)
	} render_pass;

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout set_layout_sampler;	// combined image sampler
	VkDescriptorSetLayout set_layout_uniform;	// dynamic uniform buffer
	VkDescriptorSetLayout set_layout_storage;	// feedback buffer

	VkPipelineLayout pipeline_layout;			// default shaders
	VkPipelineLayout pipeline_layout_storage;	// flare test shader layout
	VkPipelineLayout pipeline_layout_post_process;	// post-processing
	VkPipelineLayout pipeline_layout_smaa;		// SMAA (push constants + 3 samplers)
	VkPipelineLayout pipeline_layout_blend;		// post-processing
	VkPipelineLayout pipeline_layout_msdf;		// MSDF text (112-byte push constant range)

	// ── primitive ribbon infrastructure ──────────────────────────────
	//
	// Self-contained pipeline for the ribbon primitive. Cgame submits
	// world-space control points + a header per ribbon via
	// RE_AddRibbonToScene; the renderer accumulates them into per-frame
	// host-coherent SSBOs and issues one vkCmdDraw per ribbon at the
	// translucent draw site in RB_DrawSurfs.
	//
	// Per-frame slots are indexed by vk.cmd_index; each frame in flight
	// has its own buffer pair so CPU writes don't race GPU reads.
	#define RIBBON_POINTS_PER_FRAME    16384u  // worst case: 8 ribbons × 2048 points
	#define RIBBON_HEADERS_PER_FRAME      256u // hard cap on submissions per frame
	#define RIBBON_POINT_BYTES             48u // sizeof(GPU RibbonPoint), std430
	                                           // (3 * vec4: posW, rgba, normal+pad)
	#define RIBBON_HEADER_BYTES            24u // sizeof(GPU RibbonHeader), std430
	                                           // 4 uints (pointOffset, pointCount, shaderHandle, flags) +
	                                           // vec2 uvScroll = 24 B. Struct alignment is 8 B (vec2);
	                                           // 24 is a multiple of 8 so no trailing pad. Phase 5U
	                                           // dropped the dormant spawnTime + _pad pair (no shader
	                                           // ever read it; ribbon is transient-only and uvScroll
	                                           // references the absolute frame clock).
	struct {
		// pipeline + layouts + shader-bound state
		VkDescriptorSetLayout	set_layout;          // 2 SSBOs: points, headers
		VkPipelineLayout		pipeline_layout;     // push(mvp+eyeWorld) + set0(SSBOs)
		VkPipeline				pipeline_alpha;      // SRC_ALPHA / ONE_MINUS_SRC_ALPHA
		VkPipeline				pipeline_additive;   // SRC_ALPHA / ONE
		// per-frame staging (host-coherent, mapped)
		VkBuffer				points_buffer  [NUM_COMMAND_BUFFERS];
		VkDeviceMemory			points_memory  [NUM_COMMAND_BUFFERS];
		byte					*points_ptr    [NUM_COMMAND_BUFFERS];
		VkBuffer				headers_buffer [NUM_COMMAND_BUFFERS];
		VkDeviceMemory			headers_memory [NUM_COMMAND_BUFFERS];
		byte					*headers_ptr   [NUM_COMMAND_BUFFERS];
		VkDescriptorSet			descriptor     [NUM_COMMAND_BUFFERS];
		// per-frame write cursors (CPU-side, reset in RE_BeginFrame
		// at frontend frame begin, before cgame submissions)
		uint32_t				numPointsThisFrame;
		uint32_t				numHeadersThisFrame;
		qboolean				available;            // false if init failed
	} ribbon;

	// ── primitive beam ───────────────────────────────────────────────
	//
	// Camera-facing two-endpoint quad with optional axial-copy
	// expansion (cross pattern). Engine-managed pool with mixed
	// transient (one-frame) and persistent (lifetime + fade) entries.
	//
	// Pool layout: vk.beam.active[i] tracks slot occupancy. Persistent
	// metadata (spawnTime, duration, fadeIn, fadeOut) lives alongside
	// the descriptor copy; transient slots have duration == 0 and
	// expire at the end of each frame's RB_DrawBeams pass.
	//
	// SSBO write pattern: at draw time, RB_DrawBeams walks the pool,
	// resolves entity-attached endpoints + fade alpha, and writes
	// drawCount consecutive beamHeaderGPU_t entries to the per-frame
	// SSBO. Then issues a single vkCmdDraw with instance count =
	// drawCount and vertex count = 6 * BEAM_AXIAL_MAX. The vertex
	// shader expands each instance to (axialCopies × 6) vertices,
	// emitting degenerate clip-space-behind triangles for unused
	// axial copies (when axialCopies < BEAM_AXIAL_MAX).
	#define BEAM_POOL_MAX       128u   // max concurrent beams (transient + persistent combined). Phase 5P bump 64→128 gives headroom for 16-player matches and trail beam consumers (jumppad, rocket trails) on the roadmap.
	#define BEAM_AXIAL_MAX        8u   // max axialCopies per beam (vertex-shader fixed loop bound)
	#define BEAM_HEADER_BYTES    96u   // sizeof(GPU BeamHeader), std430:
	                                   //   4 vec4 (start, end, startColor, endColor)  = 64 B
	                                   //   vec2 uvScroll + 2 float widths + float spawnTime
	                                   //                                       + 3 uint  = 32 B
	                                   // total 96 B, naturally 16-aligned for vec4
	                                   // array stride; no manual padding needed.
	struct {
		// pipeline + layouts + shader-bound state
		VkDescriptorSetLayout	set_layout;            // 4 bindings: headers SSBO, image array, stages SSBO, stage-counts SSBO
		VkPipelineLayout		pipeline_layout;       // push(mvp+eyeWorld+frameParams+stageParams) + set0
		VkPipeline				pipeline;              // single ONE/ONE additive pipeline
		// Phase 5J: dedicated REPEAT-mode sampler for beam binding 1.
		// Beam UV scrolling produces large out-of-range UVs; REPEAT
		// wraps natively. Ribbon/sprite/particle continue to share
		// vk.particle.sampler (CLAMP_TO_EDGE).
		VkSampler				sampler_repeat;
		// per-frame staging (host-coherent, mapped)
		VkBuffer				header_buffer  [NUM_COMMAND_BUFFERS];
		VkDeviceMemory			header_memory  [NUM_COMMAND_BUFFERS];
		byte					*header_ptr    [NUM_COMMAND_BUFFERS];
		VkDescriptorSet			descriptor     [NUM_COMMAND_BUFFERS];
		// host-side pool state. Slots 0..BEAM_POOL_MAX-1.
		qboolean				active         [BEAM_POOL_MAX];
		float					spawnTime      [BEAM_POOL_MAX];
		float					duration       [BEAM_POOL_MAX];
		float					fadeIn         [BEAM_POOL_MAX];
		float					fadeOut        [BEAM_POOL_MAX];
		beamDesc_t				desc           [BEAM_POOL_MAX];
		uint32_t				drawCount;             // beams written to SSBO this frame (compacted from active slots)
		qboolean				available;             // false if init failed
	} beam;

	// ── primitive shader stage SSBO (Phase 5F) ─────────────────────────
	//
	// Per-stage rendering parameters for multi-stage primitive
	// shaders. Indexed [shaderHandle * PRIMITIVE_STAGE_MAX +
	// stageNumber]. Host-mapped, written at shader registration
	// time; consumers (Phase 5G beam.vert / RB_DrawBeams) read this to
	// drive multi-stage draws.
	//
	// Allocated lazily (first call to vk_init_primitive_shader_images
	// after init) and persisted across vid_restart — buffer survives
	// because it lives outside the descriptor pool. Contents are
	// re-zeroed on every vk_init_primitive_shader_images call so
	// vid_restart sees a clean slate before cgame re-registers
	// shaders.
	VkBuffer       primitive_stages_buffer;
	VkDeviceMemory primitive_stages_memory;
	void          *primitive_stages_mapped;
	// Indexed by primitive registry SLOT (0..PRIMITIVE_SHADER_IMAGE_MAX-1),
	// NOT by qhandle. Phase 5K decoupled the two when the qhandle space
	// (MAX_SHADERS=16384) outgrew the registry capacity.
	int            primitive_shader_stage_counts[PRIMITIVE_SHADER_IMAGE_MAX];

	// Phase 5K: qhandle → primitive registry slot indirection.
	// Primitive registry has only PRIMITIVE_SHADER_IMAGE_MAX slots (64),
	// but qhandle is monotonic across all RE_RegisterShader calls (world
	// textures, models, UI, primitive). Map registered primitive shaders
	// into the compact slot space.
	//
	// Sentinel PRIMITIVE_SLOT_INVALID (0xFF) means "not registered as
	// primitive shader". Slot 0 is reserved for tr.whiteImage fallback
	// but is a legal allocation outcome — distinguish via the sentinel,
	// not the slot value.
	//
	// Sized via VK_PRIM_QHANDLE_MAX (declared above) which matches
	// MAX_SHADERS in tr_local.h. The mismatch is detected by a
	// _Static_assert in vk.c at first compile.
	//
	// Initialized to all 0xFF in vk_init_primitive_shader_stages
	// (memset 0xFF), repopulated by RE_RegisterPrimitiveShader on each
	// successful registration.
	uint8_t        qhandle_to_prim_slot[VK_PRIM_QHANDLE_MAX];

	// Phase 5G: per-shader stage count SSBO. Tiny (PRIMITIVE_SHADER_IMAGE_MAX
	// uint32_t = 256 B), uploaded once per RB_DrawBeams frame. Bound at
	// beam descriptor set binding 3 so the vertex shader can cull
	// per-stage draws for shaders with fewer stages than the current
	// loop index.
	VkBuffer       primitive_stage_counts_buffer;
	VkDeviceMemory primitive_stage_counts_memory;
	void          *primitive_stage_counts_mapped;

	// ── primitive sprite (billboard quad) ────────────────────────────
	//
	// Self-contained pipeline for the sprite primitive. Cgame submits
	// world-space billboard sprites via RE_AddSpriteToScene; the
	// renderer accumulates them into a per-frame host-coherent SSBO
	// of SpriteHeaders and issues one direct vkCmdDraw per blend
	// variant (alpha / additive) at the translucent draw site in
	// RB_DrawSurfs, with vertexCount=6 and instanceCount=N.
	//
	// Per-frame slots are indexed by vk.cmd_index; each frame in
	// flight has its own buffer so CPU writes don't race GPU reads.
	#define SPRITES_PER_FRAME    4096u  // hard cap on submissions per frame
	#define SPRITE_HEADER_BYTES    48u  // sizeof(GPU SpriteHeader), std430
	struct {
		// pipeline + layouts + shader-bound state
		VkDescriptorSetLayout	set_layout;          // 1 SSBO: headers
		VkPipelineLayout		pipeline_layout;     // push(mvp+viewLeft+viewUp) + set0(SSBO)
		VkPipeline				pipeline_alpha;      // SRC_ALPHA / ONE_MINUS_SRC_ALPHA
		VkPipeline				pipeline_additive;   // SRC_ALPHA / ONE
		// per-frame staging (host-coherent, mapped)
		VkBuffer				headers_buffer [NUM_COMMAND_BUFFERS];
		VkDeviceMemory			headers_memory [NUM_COMMAND_BUFFERS];
		byte					*headers_ptr   [NUM_COMMAND_BUFFERS];
		VkDescriptorSet			descriptor     [NUM_COMMAND_BUFFERS];
		// per-frame write cursor (CPU-side, reset in RE_BeginFrame
		// at frontend frame begin, before cgame submissions)
		uint32_t				numThisFrame;
		qboolean				available;            // false if init failed
	} sprite;

	// ── primitive particle (compute-integrated GPU pool) ────────────
	//
	// Self-contained compute + render pipeline for the particle
	// primitive. Particles live in a fixed-size GPU pool (ping-pong'd
	// between two SSBOs); each frame's compute pass reads from one
	// pool, integrates physics + age, and writes to the other. The
	// render pass reads from the just-written pool and emits a
	// billboard quad per live particle.
	//
	// Particle classes (the data-driven recipe registered by cgame
	// via CG_RegisterParticleClass) are mirrored into a separate
	// classes SSBO that both compute and render shaders read.
	//
	// The Particle struct in std430 has hard-fixed size 48 B
	// (vec3+float pairs round to 16 B each, plus 16 B of trailing
	// uints for classHandle + pad). Pool memory: 16384 × 48 B =
	// 768 KB per buffer × 2 buffers = 1.5 MB total.
	//
	// The ParticleClassGPU host-side mirror has hard-fixed size 352 B
	// to match GLSL std430's computed stride (vec4-aligned struct).
	// A static_assert in vk_init_particle catches any drift.
	//
	// Type definitions (particleClassGPU_t, particleFrame_t) and the
	// PARTICLES_PER_POOL / *_BYTES constants live above this struct,
	// outside the Vk_Instance typedef — C disallows nested typedefs
	// inside a struct.
	struct {
		// pipeline state
		VkDescriptorSetLayout	compute_set_layout;  // 4 bindings: UBO + 3 SSBOs
		VkDescriptorSetLayout	render_set_layout;   // 3 bindings: UBO + 2 SSBOs
		VkPipelineLayout		compute_pipeline_layout;
		VkPipelineLayout		render_pipeline_layout;
		VkPipeline				compute_pipeline;
		VkPipeline				render_pipeline_alpha;
		VkPipeline				render_pipeline_additive;

		// ping-pong particle pool (host-coherent for emit-time CPU
		// writes in phase 3). Indexed by ping-pong bit, NOT by
		// cmd_index — semantically distinct, even though they happen
		// to advance in lockstep at NUM_COMMAND_BUFFERS == 2.
		VkBuffer				pool_buffer  [2];
		VkDeviceMemory			pool_memory  [2];
		byte					*pool_ptr    [2];

		// class shadow SSBO (host-coherent, mapped). Renderer-side
		// shadow registry: phase-3 RE_RegisterParticleClass writes
		// into shadow_ptr at offset (handle-1)*PARTICLE_CLASS_GPU_BYTES.
		VkBuffer				classes_buffer;
		VkDeviceMemory			classes_memory;
		byte					*classes_ptr;
		uint32_t				numClasses;          // current registry count

		// Phase-5 texturing: per-class image cache + shared sampler.
		// At RE_RegisterParticleClass time the resolved image_t
		// (shader→stages[0]→bundle[0]→image[0] with three-tier
		// fallback) is cached here so vk_init_descriptors's re-alloc
		// path can re-walk the registry after a descriptor pool
		// reset and re-write the sampler-array binding. The shared
		// sampler is created once at vk_init_particle (linear,
		// clamp-to-edge, no anisotropy) and used for every slot of
		// the array — billboard particles don't need per-class
		// filter/wrap variations.
		struct image_s			*classImages[64];    // MAX_PARTICLE_CLASSES; NULL = unregistered slot, fall back to tr.whiteImage
		VkSampler				sampler;

		// per-frame uniform buffer (host-coherent, mapped). One slot
		// per cmd_index; frame N writes uniform[N%NUM_COMMAND_BUFFERS]
		// before recording compute + render commands.
		VkBuffer				frame_buffer [NUM_COMMAND_BUFFERS];
		VkDeviceMemory			frame_memory [NUM_COMMAND_BUFFERS];
		byte					*frame_ptr   [NUM_COMMAND_BUFFERS];

		// descriptor sets. compute_descriptor[i] binds read=pool[i],
		// write=pool[1-i]; render_descriptor[i] binds read=pool[1-i]
		// (the pool the compute pass just wrote). Frame N selects
		// index pingPongRead.
		VkDescriptorSet			compute_descriptor[NUM_COMMAND_BUFFERS];
		VkDescriptorSet			render_descriptor [NUM_COMMAND_BUFFERS];

		// frame-to-frame state
		uint32_t				pingPongRead;        // 0 or 1, flipped each frame
		float					prevSceneTime;       // backEnd.refdef.floatTime at last
		                                             // RB_RunParticleCompute call

		// emit-time state (host-side cursor for RE_EmitParticles).
		// Round-robin allocation index into the host-coherent pool;
		// wrap-around overwrites the oldest slot.
		uint32_t				nextSlot;

		qboolean				available;           // false if init failed
	} particle;

#if FEAT_IQM
	// ── IQM GPU skinning infrastructure ──────────────────────────────
	//
	// Self-contained pipeline for skeletal IQM models.
	// Per-model VBOs (vertex+index) are stored in iqmData_t.
	// Per-frame bone matrices are uploaded to a host-visible UBO
	// and bound via a dedicated descriptor set + pipeline layout.
	//
	struct {
		VkDescriptorSetLayout	set_layout_bones;	// bone matrix UBO (set 0)
		VkPipelineLayout		pipeline_layout;	// push(mvp) + set0(bones) + set1(texture)
		VkPipeline				pipeline;			// graphics pipeline
		VkBuffer				bone_buffer[NUM_COMMAND_BUFFERS]; // per-frame bone UBOs
		VkDeviceMemory			bone_memory[NUM_COMMAND_BUFFERS];
		byte					*bone_ptr[NUM_COMMAND_BUFFERS];   // mapped pointers
		VkDescriptorSet			bone_descriptor[NUM_COMMAND_BUFFERS]; // per-frame bone descriptors
		qboolean				available;			// false if init failed
	} iqmGpu;
#endif

	VkDescriptorSet color_descriptor;

	VkImage color_image;
	VkImageView color_image_view;

	VkImage bloom_image[1+VK_NUM_BLOOM_PASSES*2];
	VkImageView bloom_image_view[1+VK_NUM_BLOOM_PASSES*2];

	VkDescriptorSet bloom_image_descriptor[1+VK_NUM_BLOOM_PASSES*2];

	VkImage depth_image;
	VkImageView depth_image_view;

	// depth fade (soft particles)
	struct {
		VkImage         image;
		VkImageView     view;
		VkDeviceMemory  memory;
		VkSampler       sampler;
		VkDescriptorSet descriptor;
		qboolean        active;
		qboolean        copied;		// depth was copied this frame
	} depthFade;

#if FEAT_SHADOW_MAPPING
	// Shadow mapping
	struct {
		VkImage         image;
		VkImageView     view;
		VkDeviceMemory  memory;
		VkSampler       sampler;
		VkDescriptorSet descriptor;
		VkRenderPass    renderPass;
		VkFramebuffer   framebuffer;
		VkPipeline      depthPipeline;   // depth-only rendering pipeline
		VkPipelineLayout depthLayout;    // push constants only (lightMVP)
		qboolean        active;
		uint32_t        size;            // shadow map resolution (default 512)
	} shadowMap;
#endif

	// SMAA anti-aliasing
	struct {
		qboolean        active;
		int             quality;        // 1-4

		// LUT textures (static, created once)
		VkImage         area_image;
		VkImageView     area_view;
		VkDeviceMemory  area_memory;
		VkDescriptorSet area_descriptor;

		VkImage         search_image;
		VkImageView     search_view;
		VkDeviceMemory  search_memory;
		VkDescriptorSet search_descriptor;

		// intermediate textures (resolution-dependent)
		VkImage         edges_image;    // R8G8
		VkImageView     edges_view;
		VkDescriptorSet edges_descriptor;

		VkImage         blend_image;    // RGBA8
		VkImageView     blend_view;
		VkDescriptorSet blend_descriptor;

		VkImage         input_image;    // color_format (copy of color_image)
		VkImageView     input_view;
		VkDescriptorSet input_descriptor;

		VkSampler       point_sampler;
		VkSampler       linear_sampler;
	} smaa;

	VkImage msaa_image;
	VkImageView msaa_image_view;

	// screenMap
	struct {
		VkDescriptorSet color_descriptor;
		VkImage color_image;
		VkImageView color_image_view;

		VkImage color_image_msaa;
		VkImageView color_image_view_msaa;

		VkImage depth_image;
		VkImageView depth_image_view;

	} screenMap;

	struct {
		VkImage image;
		VkImageView image_view;
	} capture;

	struct {
		VkFramebuffer blur[VK_NUM_BLOOM_PASSES*2];
		VkFramebuffer bloom_extract;
		VkFramebuffer main[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer gamma[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer screenmap;
		VkFramebuffer capture;
		VkFramebuffer smaa_edge;
		VkFramebuffer smaa_blend;
		VkFramebuffer smaa_resolve;
	} framebuffers;

#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished;	// reference to vk.cmd->rendering_finished2
	VkSemaphore image_uploaded2;
	VkSemaphore image_uploaded;		// reference to vk.image_uploaded2
#endif

	vk_tess_t tess[ NUM_COMMAND_BUFFERS ], *cmd;
	int cmd_index;

	struct {
		VkBuffer		buffer;
		byte			*buffer_ptr;
		VkDeviceMemory	memory;
		VkDescriptorSet	descriptor;
	} storage;

	uint32_t uniform_item_size;
	uint32_t uniform_alignment;
	uint32_t storage_alignment;

	float    timestampPeriodNs;     // ns per timestamp tick, from VkPhysicalDeviceLimits
	qboolean timestampSupported;    // device supports CmdWriteTimestamp on graphics queue

	struct {
		VkBuffer vertex_buffer;
		VkDeviceMemory	buffer_memory;
	} vbo;

	// host visible memory that holds vertex, index and uniform data
	VkDeviceMemory geometry_buffer_memory;
	VkDeviceSize geometry_buffer_size;
	VkDeviceSize geometry_buffer_size_new;

	// statistics
	struct {
		VkDeviceSize vertex_buffer_max;
		uint32_t push_size;
		uint32_t push_size_max;
	} stats;

	//
	// Shader modules.
	//
	struct {
		struct {
			VkShaderModule gen[3][2][2][2]; // tx[0,1,2], cl[0,1] env0[0,1] fog[0,1]
			VkShaderModule ident1[2][2][2]; // tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule fixed[2][2][2];  // tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule light[2];        // fog[0,1]
#if FEAT_PARALLAX_MAPPING
			VkShaderModule light_parallax[2]; // fog[0,1]
#endif
		} vert;
		struct {
			VkShaderModule gen0_df;
			VkShaderModule gen[3][2][2]; // tx[0,1,2] cl[0,1] fog[0,1]
			VkShaderModule ident1[2][2]; // tx[0,1], fog[0,1]
			VkShaderModule fixed[2][2];  // tx[0,1], fog[0,1]
			VkShaderModule ent[1][2];    // tx[0], fog[0,1]
			VkShaderModule light[2][2];  // linear[0,1] fog[0,1]
#if FEAT_PARALLAX_MAPPING
			VkShaderModule light_parallax[2][2]; // linear[0,1] fog[0,1]
#endif
			// depth fade variants (single-texture only)
			VkShaderModule dfade_gen[1][2];    // tx[0], fog[0,1]
			VkShaderModule dfade_ident1[1][2]; // tx[0], fog[0,1]
			VkShaderModule dfade_fixed[1][2];  // tx[0], fog[0,1]
			VkShaderModule dfade_ent[1][2];    // tx[0], fog[0,1]
		} frag;

		VkShaderModule msdf_fs;
		VkShaderModule msdf_vs;

		VkShaderModule color_fs;
		VkShaderModule color_vs;

		VkShaderModule bloom_fs;
		VkShaderModule blur_fs;
		VkShaderModule blend_fs;

		// Phase 9D (6B3a): post-main HDR boost pass — applies obScale
		// to vk.color_image before bloom-extract reads it. Scaffolded
		// here; dispatch wiring lands in 6B3b.
		VkShaderModule boost_fs;

		VkShaderModule gamma_fs;
		VkShaderModule gamma_vs;

		VkShaderModule fog_fs;
		VkShaderModule fog_vs;

		VkShaderModule dot_fs;
		VkShaderModule dot_vs;

#if FEAT_ADVANCED_WATER
		VkShaderModule water_fs;
#endif

#if FEAT_SHADOW_MAPPING
		VkShaderModule shadow_depth_vs;
		VkShaderModule shadow_depth_fs;
		VkShaderModule light_shadow[2];      // vert: fog[0,1]
		VkShaderModule light_shadow_frag[2][2]; // frag: linear[0,1] fog[0,1]
#endif
#if FEAT_PBR
		VkShaderModule light_pbr_frag[2][2]; // frag: linear[0,1] fog[0,1]
#endif

		VkShaderModule smaa_edge_vs;
		VkShaderModule smaa_edge_fs;
		VkShaderModule smaa_blend_vs;
		VkShaderModule smaa_blend_fs;
		VkShaderModule smaa_resolve_vs;
		VkShaderModule smaa_resolve_fs;

		// Q1 4-style lightmap blend
		VkShaderModule q1_ls_vs;
		VkShaderModule q1_ls_fs;
		VkShaderModule q1_ls_array_fs;   // array variant; reuses q1_ls_vs

		// primitive ribbon
		VkShaderModule ribbon_vs;
		VkShaderModule ribbon_fs;

		// primitive sprite (billboarded quad, view-axis aligned)
		VkShaderModule sprite_vs;
		VkShaderModule sprite_fs;

		// primitive beam (two-endpoint camera-facing quad with axial-copy expansion)
		VkShaderModule beam_vs;
		VkShaderModule beam_fs;

		// primitive particle (compute-driven pool, billboard render)
		VkShaderModule particle_integrate_cs;
		VkShaderModule particle_vs;
		VkShaderModule particle_fs;

#if FEAT_IQM
		// IQM GPU skinning
		VkShaderModule iqm_skinning_vs;
		VkShaderModule iqm_skinning_fs;
#endif
	} modules;

	VkPipelineCache pipelineCache;

	VK_Pipeline_t pipelines[ MAX_VK_PIPELINES ];
	uint32_t pipelines_count;
	uint32_t pipelines_world_base;

	// pipeline statistics
	int32_t pipeline_create_count;

	//
	// Standard pipelines.
	//
	uint32_t skybox_pipeline;

	// dim 0: 0 - front side, 1 - back size
	// dim 1: 0 - normal view, 1 - mirror view
	uint32_t shadow_volume_pipelines[2][2];
	uint32_t shadow_finish_pipeline;

	// dim 0 is based on fogPass_t: 0 - corresponds to FP_EQUAL, 1 - corresponds to FP_LE.
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
	uint32_t fog_pipelines[2][3][2];

	// dim 0 is based on dlight additive flag: 0 - not additive, 1 - additive
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
#ifdef USE_LEGACY_DLIGHTS
	uint32_t dlight_pipelines[2][3][2];
#endif

	// cullType[3], polygonOffset[2], fogStage[2], absLight[2]
#ifdef USE_PMLIGHT
	uint32_t dlight_pipelines_x[3][2][2][2];
	uint32_t dlight1_pipelines_x[3][2][2][2];
#endif

	// debug visualization pipelines
	uint32_t tris_debug_pipeline;
	uint32_t tris_mirror_debug_pipeline;
	uint32_t tris_debug_green_pipeline;
	uint32_t tris_mirror_debug_green_pipeline;
	uint32_t tris_debug_red_pipeline;
	uint32_t tris_mirror_debug_red_pipeline;

	uint32_t normals_debug_pipeline;
	uint32_t surface_debug_pipeline_solid;
	uint32_t surface_debug_pipeline_outline;
	uint32_t images_debug_pipeline;
	uint32_t images_debug_pipeline2;
	uint32_t surface_beam_pipeline;
	uint32_t surface_axis_pipeline;
	uint32_t dot_pipeline;

	uint32_t msdf_pipeline;
	uint32_t q1ls_pipeline;		// Q1 4-style lightmap blend, animChain lerp
	uint32_t q1ls_array_pipeline;	// Q1 4-style lightmap blend, texture array animation

	VkPipeline gamma_pipeline;

	// Post-process gamma variants (combinatorial, indexed by feature bitmask)
	// Bit 0 = SSAO, Bit 1 = TONEMAP, Bit 2 = COLOR_GRADING, Bit 3 = FXAA
#define GAMMA_VAR_SSAO    1
#define GAMMA_VAR_TONEMAP 2
#define GAMMA_VAR_CG      4
#define GAMMA_VAR_FXAA    8
#define GAMMA_VAR_GODRAYS 16
#define GAMMA_VAR_COUNT   32
	VkPipeline gamma_variants[GAMMA_VAR_COUNT];
	VkShaderModule gamma_variant_fs[GAMMA_VAR_COUNT];
	VkPipelineLayout pipeline_layout_ssao;    // 2 samplers: color + depth (SSAO without godrays)
	VkPipelineLayout pipeline_layout_godrays; // 2 samplers + push constants (godrays sun position)
	VkPipeline capture_pipeline;
	VkPipeline bloom_extract_pipeline;
	VkPipeline blur_pipeline[VK_NUM_BLOOM_PASSES*2]; // horizontal & vertical pairs
	VkPipeline bloom_blend_pipeline;

	// Phase 9D (6B3a): boost pipeline — post-main HDR multiply.
	// Created via vk_create_post_process_pipeline case 4; not bound
	// to any dispatch this phase (6B3b wires the draw + render pass).
	VkPipeline boost_pipeline;
	VkPipeline smaa_edge_pipeline;
	VkPipeline smaa_blend_pipeline;
	VkPipeline smaa_resolve_pipeline;

	uint32_t frame_count;
	qboolean active;
	qboolean wideLines;
	qboolean samplerAnisotropy;
	qboolean fragmentStores;
	qboolean dedicatedAllocation;
	qboolean debugMarkers;

	float maxAnisotropy;
	float maxLod;

	VkFormat color_format;
	VkFormat capture_format;
	VkFormat depth_format;
	VkFormat bloom_format;

	VkImageLayout initSwapchainLayout;

	qboolean clearAttachment;		// requires VK_IMAGE_USAGE_TRANSFER_DST_BIT for swapchains
	qboolean fboActive;
	qboolean blitEnabled;
	qboolean msaaActive;

	qboolean offscreenRender;

	qboolean windowAdjusted;
	int		blitX0;
	int		blitY0;
	int		blitFilter;

	uint32_t renderWidth;
	uint32_t renderHeight;

	float renderScaleX;
	float renderScaleY;

	renderPass_t renderPassIndex;

	uint32_t screenMapWidth;
	uint32_t screenMapHeight;
	uint32_t screenMapSamples;

	uint32_t image_chunk_size;

	uint32_t maxBoundDescriptorSets;

#ifdef USE_UPLOAD_QUEUE
	VkFence aux_fence;
	qboolean aux_fence_wait;
#endif

	struct staging_buffer_s {
		VkBuffer handle;
		VkDeviceMemory memory;
		VkDeviceSize size;
		byte *ptr; // pointer to mapped staging buffer
#ifdef USE_UPLOAD_QUEUE
		VkDeviceSize offset;
#endif
	} staging_buffer;

	struct samplers_s {
		int count;
		Vk_Sampler_Def def[MAX_VK_SAMPLERS];
		VkSampler handle[MAX_VK_SAMPLERS];
		int filter_min;
		int filter_max;
	} samplers;

	struct defaults_t {
		VkDeviceSize staging_size;
		VkDeviceSize geometry_size;
	} defaults;

	char driverNote[200];

} Vk_Instance;

typedef struct {
	VkDeviceMemory memory;
	VkDeviceSize used;
} ImageChunk;

// Vk_World contains vulkan resources/state requested by the game code.
// It is reinitialized on a map change.
typedef struct {
	//
	// Memory allocations.
	//
	int num_image_chunks;
	ImageChunk image_chunks[MAX_IMAGE_CHUNKS];

	//
	// State.
	//

	// Descriptor sets corresponding to bound texture images.
	//VkDescriptorSet current_descriptor_sets[ MAX_TEXTURE_UNITS ];

	// This flag is used to decide whether framebuffer's depth attachment should be cleared
	// with vmCmdClearAttachment (dirty_depth_attachment != 0), or it have just been
	// cleared by render pass instance clear op (dirty_depth_attachment == 0).
	int dirty_depth_attachment;

	float modelview_transform[16];
} Vk_World;

extern Vk_Instance	vk;				// shouldn't be cleared during ref re-init
extern Vk_World		vk_world;		// this data is cleared during ref re-init
