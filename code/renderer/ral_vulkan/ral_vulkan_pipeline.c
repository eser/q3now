// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_pipeline.c — Vulkan backend: graphics & compute pipelines and
// the on-disk VkPipelineCache (phase-7-ral-design.md §3.6, §6.3, §7.4, §8).
// Dynamic rendering only (no VkRenderPass) — colour-attachment
// formats + depth format come from ralGraphicsPipelineCreateInfo_t and feed
// VkPipelineRenderingCreateInfo (§6).
//
// A backend-wide VkPipelineCache seeds every vkCreate*Pipelines call and is
// saved/loaded by Ral_{Save,Load}PipelineCache. A small flat layout cache
// (RAL_VK_LAYOUT_CACHE_MAX entries, linear scan with FNV-1a fast-reject)
// shares one VkPipelineLayout across every pipeline whose (bindGroupLayouts[],
// pushConstantSize, pushConstantStages) tuple matches — the renderer
// migration hits this on every shader variant for the same
// bind-set lineage.
//
// SPIR-V translation to other backends is OFFLINE only (compile_xlate.mjs /
// code/tools/shader_xlate); the runtime hands SPIR-V straight to
// the driver. No naga linkage.

#include "ral_vulkan_internal.h"
#include "pipeline_test_spv.h"   // embedded SPIR-V for the \ral_dump pipeline smoke test

#include <stdio.h>     // fopen/fwrite/fread/fclose for the pipeline-cache file

// ════════════════════════════════════════════════════════════════════════
// RAL-enum → Vulkan translation helpers
// ════════════════════════════════════════════════════════════════════════
static VkPrimitiveTopology ralVk_Topology( ralPrimitiveTopology_t t ) {
	switch ( t ) {
	case RAL_TOPOLOGY_POINT_LIST:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case RAL_TOPOLOGY_LINE_LIST:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case RAL_TOPOLOGY_LINE_STRIP:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case RAL_TOPOLOGY_TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	case RAL_TOPOLOGY_TRIANGLE_LIST:
	default:                          return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}
}
static VkPolygonMode ralVk_PolygonMode( ralPolygonMode_t m ) {
	switch ( m ) {
	case RAL_POLYGON_LINE:   return VK_POLYGON_MODE_LINE;
	case RAL_POLYGON_POINT:  return VK_POLYGON_MODE_POINT;
	case RAL_POLYGON_FILL:
	default:                 return VK_POLYGON_MODE_FILL;
	}
}
static VkCullModeFlags ralVk_CullMode( ralCullMode_t m ) {
	switch ( m ) {
	case RAL_CULL_FRONT: return VK_CULL_MODE_FRONT_BIT;
	case RAL_CULL_BACK:  return VK_CULL_MODE_BACK_BIT;
	case RAL_CULL_NONE:
	default:             return VK_CULL_MODE_NONE;
	}
}
static VkFrontFace ralVk_FrontFace( ralFrontFace_t f ) {
	return ( f == RAL_FRONT_FACE_CW ) ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}
static VkBlendFactor ralVk_BlendFactor( ralBlendFactor_t f ) {
	switch ( f ) {
	case RAL_BLEND_ZERO:                return VK_BLEND_FACTOR_ZERO;
	case RAL_BLEND_ONE:                 return VK_BLEND_FACTOR_ONE;
	case RAL_BLEND_SRC_COLOR:           return VK_BLEND_FACTOR_SRC_COLOR;
	case RAL_BLEND_ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case RAL_BLEND_DST_COLOR:           return VK_BLEND_FACTOR_DST_COLOR;
	case RAL_BLEND_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case RAL_BLEND_SRC_ALPHA:           return VK_BLEND_FACTOR_SRC_ALPHA;
	case RAL_BLEND_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case RAL_BLEND_DST_ALPHA:           return VK_BLEND_FACTOR_DST_ALPHA;
	case RAL_BLEND_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	case RAL_BLEND_SRC_ALPHA_SATURATE:  return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	default:                            return VK_BLEND_FACTOR_ONE;
	}
}

// stencil op translator. Vulkan VkStencilOp values 0..7
// are exactly RAL_STENCIL_OP_KEEP..DECREMENT_AND_WRAP, but we still go through
// a switch so the mapping is visible + defensive for future RAL surface
// additions. ralVk_FillStencilOpState calls ralVk_PipelineCompareOp (declared
// below) — forward-declare to keep the helpers grouped.
static VkCompareOp ralVk_PipelineCompareOp( ralCompareOp_t c );

static VkStencilOp ralVk_StencilOp( ralStencilOp_t o ) {
	switch ( o ) {
	case RAL_STENCIL_OP_ZERO:                return VK_STENCIL_OP_ZERO;
	case RAL_STENCIL_OP_REPLACE:             return VK_STENCIL_OP_REPLACE;
	case RAL_STENCIL_OP_INCREMENT_AND_CLAMP: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
	case RAL_STENCIL_OP_DECREMENT_AND_CLAMP: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
	case RAL_STENCIL_OP_INVERT:              return VK_STENCIL_OP_INVERT;
	case RAL_STENCIL_OP_INCREMENT_AND_WRAP:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
	case RAL_STENCIL_OP_DECREMENT_AND_WRAP:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
	case RAL_STENCIL_OP_KEEP:
	default:                                 return VK_STENCIL_OP_KEEP;
	}
}

static void ralVk_FillStencilOpState( VkStencilOpState *out, const ralStencilOpState_t *in ) {
	out->failOp      = ralVk_StencilOp( in->failOp );
	out->passOp      = ralVk_StencilOp( in->passOp );
	out->depthFailOp = ralVk_StencilOp( in->depthFailOp );
	out->compareOp   = ralVk_PipelineCompareOp( in->compareOp );
	out->compareMask = in->compareMask;
	out->writeMask   = in->writeMask;
	out->reference   = in->reference;
}
static VkBlendOp ralVk_BlendOp( ralBlendOp_t o ) {
	switch ( o ) {
	case RAL_BLEND_OP_SUBTRACT:         return VK_BLEND_OP_SUBTRACT;
	case RAL_BLEND_OP_REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
	case RAL_BLEND_OP_MIN:              return VK_BLEND_OP_MIN;
	case RAL_BLEND_OP_MAX:              return VK_BLEND_OP_MAX;
	case RAL_BLEND_OP_ADD:
	default:                            return VK_BLEND_OP_ADD;
	}
}

// Variable-rate-shading rate → fragment-size VkExtent2D. 1x1 (default) is never
// passed here (the caller only builds the VRS state for a coarser rate). Unknown
// rates fall back to 1x1 (a no-op fragment size).
static VkCompareOp ralVk_PipelineCompareOp( ralCompareOp_t c ) {
	switch ( c ) {
	case RAL_COMPARE_LESS:          return VK_COMPARE_OP_LESS;
	case RAL_COMPARE_EQUAL:         return VK_COMPARE_OP_EQUAL;
	case RAL_COMPARE_LESS_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
	case RAL_COMPARE_GREATER:       return VK_COMPARE_OP_GREATER;
	case RAL_COMPARE_NOT_EQUAL:     return VK_COMPARE_OP_NOT_EQUAL;
	case RAL_COMPARE_GREATER_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case RAL_COMPARE_ALWAYS:        return VK_COMPARE_OP_ALWAYS;
	case RAL_COMPARE_NEVER:
	default:                        return VK_COMPARE_OP_NEVER;
	}
}
static VkVertexInputRate ralVk_InputRate( ralVertexInputRate_t r ) {
	return ( r == RAL_VERTEX_INPUT_PER_INSTANCE ) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
}
static VkShaderStageFlags ralVk_PushConstantStages( uint32_t s ) {
	VkShaderStageFlags v = 0;
	if ( s & RAL_STAGE_VERTEX   )  v |= VK_SHADER_STAGE_VERTEX_BIT;
	if ( s & RAL_STAGE_FRAGMENT )  v |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if ( s & RAL_STAGE_COMPUTE  )  v |= VK_SHADER_STAGE_COMPUTE_BIT;
	if ( v == 0 ) v = VK_SHADER_STAGE_ALL_GRAPHICS;   // sensible default for a graphics pipeline
	return v;
}

// ════════════════════════════════════════════════════════════════════════
// pipeline-layer init + shutdown (called from Ral_CreateBackend / Destroy)
// ════════════════════════════════════════════════════════════════════════
qboolean ralVk_InitPipelineLayer( ralBackend_t *b ) {
	VkPipelineCacheCreateInfo pci;
	RAL_ZERO( pci );
	pci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	// Start empty; Ral_LoadPipelineCache may seed it later. Some drivers
	// reject zero initialData explicitly, but `nullptr/0` is spec-legal.
	if ( b->vk.CreatePipelineCache( b->device, &pci, NULL, &b->pipelineCache ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "ralVk_InitPipelineLayer: vkCreatePipelineCache failed\n" );
		b->pipelineCache = VK_NULL_HANDLE;   // pipelines still build; just no cache hits
	}
	b->layoutCache    = (ralVkLayoutCacheEntry_t *)malloc( RAL_VK_LAYOUT_CACHE_MAX * sizeof( ralVkLayoutCacheEntry_t ) );
	if ( !b->layoutCache ) {
		RAL_VK_LOG( SEV_WARN, "ralVk_InitPipelineLayer: layoutCache malloc failed\n" );
		return qfalse;
	}
	memset( b->layoutCache, 0, RAL_VK_LAYOUT_CACHE_MAX * sizeof( ralVkLayoutCacheEntry_t ) );
	b->numLayoutCache = 0;
	return qtrue;
}

// pipeline-layout cache slot count.
// Returns the high-water number of allocated cache entries (b->numLayoutCache).
// Holes from defer-destroyed slots stay counted because the scan inside
// ralVk_GetOrCreatePipelineLayout reuses them by index without compacting.
uint32_t Ral_GetPipelineLayoutCacheSlotCount( ralBackend_t *b ) {
	if ( !b ) return 0u;
	return b->numLayoutCache;
}

// pipeline-layout cache slot dump.
// Prints per-slot push_constant_size + stage flags (V|F|C) + set count + raw
// VkDescriptorSetLayout handles + refCount (= pipelines referencing this
// slot). The cache is keyed by (set-layouts × push-constants) tuple via
// FNV-1a hash + linear-scan match in ralVk_GetOrCreatePipelineLayout; the
// dump shows the materialised slot count. §17.9 projects 3-5 slots once
// per-subsystem layouts adopt through followup; pre-followup-3 was 2.
// Called from both \ral_dump pipeline (throwaway-backend path) and the
// renderer-side \ral_dump live equivalent (live imported-mode backend).
void Ral_DumpPipelineLayoutCache( ralBackend_t *b ) {
	uint32_t i, j, liveSlots = 0;
	if ( !b ) {
		RAL_VK_LOG( SEV_INFO, "===== pipeline-layout cache slots (backend NULL) =====\n" );
		return;
	}
	RAL_VK_LOG( SEV_INFO, "===== pipeline-layout cache slots =====\n" );
	RAL_VK_LOG( SEV_INFO, "  slot count (high-water): %u of %u max\n",
	        b->numLayoutCache, (unsigned)RAL_VK_LAYOUT_CACHE_MAX );
	for ( i = 0; i < b->numLayoutCache; i++ ) {
		const ralVkLayoutCacheEntry_t *e = &b->layoutCache[i];
		char stageTags[8];
		int  stageOff = 0;
		char setHandles[256];
		int  setOff = 0;
		if ( e->layout == VK_NULL_HANDLE ) {
			RAL_VK_LOG( SEV_INFO, "    [%u] (defer-destroyed slot — refCount=%u)\n", i, e->refCount );
			continue;
		}
		liveSlots++;
		// Decode push_constant_stages into V|F|C tag string. RAL_STAGE_*
		// mirrors VkShaderStageFlagBits (see ral_pipeline.h).
		stageTags[0] = '\0';
		if ( e->pushConstantStages & VK_SHADER_STAGE_VERTEX_BIT   ) { if (stageOff>0) stageTags[stageOff++]='|'; stageTags[stageOff++]='V'; }
		if ( e->pushConstantStages & VK_SHADER_STAGE_FRAGMENT_BIT ) { if (stageOff>0) stageTags[stageOff++]='|'; stageTags[stageOff++]='F'; }
		if ( e->pushConstantStages & VK_SHADER_STAGE_COMPUTE_BIT  ) { if (stageOff>0) stageTags[stageOff++]='|'; stageTags[stageOff++]='C'; }
		if ( stageOff == 0 ) { stageTags[0]='-'; stageOff=1; }
		stageTags[stageOff] = '\0';
		// Render set-layout handles inline as hex pointers (the wrapper
		// debug label isn't reachable from a raw VkDescriptorSetLayout in
		// this layer; consumer reading qconsole.jsonl can cross-reference
		// against vk.ral_bgl_sampler.layout etc. if needed).
		setHandles[0] = '\0';
		for ( j = 0; j < e->numSetLayouts && setOff < (int)sizeof( setHandles ) - 24; j++ ) {
			int n = snprintf( setHandles + setOff, sizeof( setHandles ) - setOff,
			                     "%s%p", ( j > 0 ) ? ", " : "", (void *)e->setLayouts[j] );
			if ( n > 0 ) setOff += n;
		}
		RAL_VK_LOG( SEV_INFO,
			"    [%u] push_const=%uB (%s), set_layouts=%u [%s], usage=%u\n",
			i, e->pushConstantSize, stageTags, e->numSetLayouts, setHandles, e->refCount );
	}
	RAL_VK_LOG( SEV_INFO, "  live slots: %u; defer-destroyed holes: %u\n",
	        liveSlots, b->numLayoutCache - liveSlots );
	RAL_VK_LOG( SEV_INFO, "===== end pipeline-layout cache slots =====\n" );
}

void ralVk_ShutdownPipelineLayer( ralBackend_t *b ) {
	uint32_t i;
	if ( b->device == VK_NULL_HANDLE ) return;
	// Any VkPipelineLayouts still live (refCount>0) leaked from the consumer's
	// perspective, but we destroy them here regardless — backend shutdown is
	// the final fence.
	if ( b->layoutCache ) {
		for ( i = 0; i < b->numLayoutCache; i++ ) {
			ralVkLayoutCacheEntry_t *e = &b->layoutCache[i];
			if ( e->layout != VK_NULL_HANDLE ) {
				b->vk.DestroyPipelineLayout( b->device, e->layout, NULL );
				e->layout = VK_NULL_HANDLE;
			}
		}
		free( b->layoutCache );
		b->layoutCache = NULL;
	}
	b->numLayoutCache = 0;
	if ( b->pipelineCache != VK_NULL_HANDLE ) {
		b->vk.DestroyPipelineCache( b->device, b->pipelineCache, NULL );
		b->pipelineCache = VK_NULL_HANDLE;
	}
}

// ════════════════════════════════════════════════════════════════════════
// pipeline-layout cache (FNV-1a 64 hash + linear-scan match)
// ════════════════════════════════════════════════════════════════════════
static uint64_t ralVk_LayoutKeyHash( const VkDescriptorSetLayout *setLayouts, uint32_t numSetLayouts,
                                     uint32_t pushConstantSize, uint32_t pushConstantStages ) {
	uint64_t h = 0xCBF29CE484222325ull;   // FNV-1a 64 offset basis
	const uint64_t prime = 0x100000001B3ull;
	uint32_t i;
	for ( i = 0; i < numSetLayouts; i++ ) {
		uint64_t v = RAL_VK_H2U( setLayouts[i] );
		h ^= v; h *= prime;
	}
	h ^= (uint64_t)numSetLayouts;    h *= prime;
	h ^= (uint64_t)pushConstantSize; h *= prime;
	h ^= (uint64_t)pushConstantStages; h *= prime;
	return h;
}

uint32_t ralVk_GetOrCreatePipelineLayout( ralBackend_t *b,
                                          const VkDescriptorSetLayout *setLayouts, uint32_t numSetLayouts,
                                          uint32_t pushConstantSize, uint32_t pushConstantStages,
                                          VkPipelineLayout *outLayout ) {
	VkPipelineLayoutCreateInfo plci;
	VkPushConstantRange        pcr;
	ralVkLayoutCacheEntry_t   *e;
	uint64_t                   hash;
	uint32_t                   i;
	if ( !b || !outLayout || numSetLayouts > RAL_VK_MAX_PIPELINE_SETS ) { if ( outLayout ) *outLayout = VK_NULL_HANDLE; return 0xFFFFFFFFu; }
	*outLayout = VK_NULL_HANDLE;
	hash = ralVk_LayoutKeyHash( setLayouts, numSetLayouts, pushConstantSize, pushConstantStages );

	// scan for an existing live entry with the same hash + full key match
	for ( i = 0; i < b->numLayoutCache; i++ ) {
		e = &b->layoutCache[i];
		if ( e->layout == VK_NULL_HANDLE || e->hash != hash ) continue;
		if ( e->numSetLayouts != numSetLayouts ) continue;
		if ( e->pushConstantSize != pushConstantSize || e->pushConstantStages != pushConstantStages ) continue;
		if ( numSetLayouts > 0 && memcmp( e->setLayouts, setLayouts, numSetLayouts * sizeof( VkDescriptorSetLayout ) ) != 0 ) continue;
		e->refCount++;
		*outLayout = e->layout;
		return i;
	}

	// not found — create a new VkPipelineLayout, append (or reuse the first
	// dead slot we can find — keeps memory bounded under heavy churn).
	RAL_ZERO( pcr );
	pcr.offset     = 0;
	pcr.size       = pushConstantSize;
	pcr.stageFlags = ralVk_PushConstantStages( pushConstantStages );
	RAL_ZERO( plci );
	plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount         = numSetLayouts;
	plci.pSetLayouts            = numSetLayouts ? setLayouts : NULL;
	plci.pushConstantRangeCount = ( pushConstantSize > 0 ) ? 1 : 0;
	plci.pPushConstantRanges    = ( pushConstantSize > 0 ) ? &pcr : NULL;

	{
		VkPipelineLayout layout = VK_NULL_HANDLE;
		uint32_t         slot   = b->numLayoutCache;
		VkResult         r;
		// look for a dead slot to reuse
		for ( i = 0; i < b->numLayoutCache; i++ ) {
			if ( b->layoutCache[i].layout == VK_NULL_HANDLE ) { slot = i; break; }
		}
		if ( slot >= RAL_VK_LAYOUT_CACHE_MAX ) {
			RAL_VK_LOG( SEV_WARN, "ralVk_GetOrCreatePipelineLayout: cache full (%u entries) — refusing\n", RAL_VK_LAYOUT_CACHE_MAX );
			return 0xFFFFFFFFu;
		}
		r = b->vk.CreatePipelineLayout( b->device, &plci, NULL, &layout );
		if ( r != VK_SUCCESS || layout == VK_NULL_HANDLE ) {
			RAL_VK_LOG( SEV_WARN, "ralVk_GetOrCreatePipelineLayout: vkCreatePipelineLayout failed (VkResult %d)\n", (int)r );
			return 0xFFFFFFFFu;
		}
		e = &b->layoutCache[slot];
		RAL_ZERO( *e );
		e->numSetLayouts      = numSetLayouts;
		if ( numSetLayouts > 0 ) memcpy( e->setLayouts, setLayouts, numSetLayouts * sizeof( VkDescriptorSetLayout ) );
		e->pushConstantSize   = pushConstantSize;
		e->pushConstantStages = pushConstantStages;
		e->hash               = hash;
		e->layout             = layout;
		e->refCount           = 1;
		if ( slot == b->numLayoutCache ) b->numLayoutCache++;
		*outLayout = layout;
		return slot;
	}
}

void ralVk_ReleasePipelineLayout( ralBackend_t *b, uint32_t layoutCacheIndex ) {
	ralVkLayoutCacheEntry_t *e;
	if ( !b || layoutCacheIndex >= b->numLayoutCache ) return;
	e = &b->layoutCache[layoutCacheIndex];
	if ( e->refCount == 0 || e->layout == VK_NULL_HANDLE ) return;
	if ( --e->refCount == 0 ) {
		ralVk_DeferDestroy( b, RAL_RES_PIPELINE_LAYOUT, RAL_VK_H2U( e->layout ), 0, NULL );
		e->layout = VK_NULL_HANDLE;     // slot dead until reused; key fields left for diagnostic dumps
	}
}

// ════════════════════════════════════════════════════════════════════════
// VkShaderModule helper (modules are throw-away — created/destroyed around
// the vkCreate*Pipelines call, the SPIR-V is consumed there).
// ════════════════════════════════════════════════════════════════════════
static VkShaderModule ralVk_MakeShaderModule( ralBackend_t *b, const uint32_t *spirv, uint32_t spirvSize ) {
	VkShaderModuleCreateInfo smci;
	VkShaderModule           mod = VK_NULL_HANDLE;
	if ( !spirv || spirvSize == 0 || ( spirvSize & 3u ) != 0 ) {
		RAL_VK_LOG( SEV_WARN, "ralVk_MakeShaderModule: bad SPIR-V size %u (must be non-zero multiple of 4)\n", spirvSize );
		return VK_NULL_HANDLE;
	}
	RAL_ZERO( smci );
	smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.codeSize = spirvSize;
	smci.pCode    = spirv;
	if ( b->vk.CreateShaderModule( b->device, &smci, NULL, &mod ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "ralVk_MakeShaderModule: vkCreateShaderModule failed (%u bytes)\n", spirvSize );
		return VK_NULL_HANDLE;
	}
	return mod;
}

// Build a VkSpecializationInfo from a ralSpecConstant_t array. The data lives
// in a caller-owned uint32_t scratch buffer; entries point into it. Returns
// qtrue when populated, qfalse for "no specialization (caller passes NULL)".
static qboolean ralVk_BuildSpecInfo( const ralSpecConstant_t *consts, uint32_t numConsts,
                                     VkSpecializationMapEntry *entries, uint32_t *dataScratch,
                                     VkSpecializationInfo *out ) {
	uint32_t i;
	if ( !consts || numConsts == 0 ) return qfalse;
	for ( i = 0; i < numConsts; i++ ) {
		entries[i].constantID = consts[i].constantId;
		entries[i].offset     = i * (uint32_t)sizeof( uint32_t );
		entries[i].size       = sizeof( uint32_t );
		dataScratch[i]        = consts[i].value;
	}
	RAL_ZERO( *out );
	out->mapEntryCount = numConsts;
	out->pMapEntries   = entries;
	out->dataSize      = numConsts * sizeof( uint32_t );
	out->pData         = dataScratch;
	return qtrue;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_CreateGraphicsPipeline
// ════════════════════════════════════════════════════════════════════════
#define RAL_VK_MAX_VERT_BINDINGS    8u
#define RAL_VK_MAX_VERT_ATTRIBUTES 16u
#define RAL_VK_MAX_SPEC_CONSTS     32u

ralPipeline_t *Ral_CreateGraphicsPipeline( ralBackend_t *b, const ralGraphicsPipelineCreateInfo_t *ci ) {
	ralPipeline_t                          *p;
	VkShaderModule                          modVert = VK_NULL_HANDLE, modFrag = VK_NULL_HANDLE;
	VkPipelineShaderStageCreateInfo         stages[2];
	VkVertexInputBindingDescription         vBinds[ RAL_VK_MAX_VERT_BINDINGS ];
	VkVertexInputAttributeDescription       vAttrs[ RAL_VK_MAX_VERT_ATTRIBUTES ];
	VkPipelineVertexInputStateCreateInfo    vi;
	VkPipelineInputAssemblyStateCreateInfo  ia;
	VkPipelineViewportStateCreateInfo       vp;
	VkPipelineRasterizationStateCreateInfo  rs;
	VkPipelineMultisampleStateCreateInfo    ms;
	VkPipelineDepthStencilStateCreateInfo   ds;
	VkPipelineColorBlendAttachmentState     cbAtt[ RAL_MAX_COLOR_ATTACHMENTS ];
	VkPipelineColorBlendStateCreateInfo     cb;
	VkDynamicState                          dyn[3];
	VkPipelineDynamicStateCreateInfo        dynState;
	VkFormat                                colorFmts[ RAL_MAX_COLOR_ATTACHMENTS ];
	VkPipelineRenderingCreateInfo           dynRendering;
	VkPipelineFragmentShadingRateStateCreateInfoKHR vrsState;
	VkGraphicsPipelineCreateInfo            gpci;
	VkPipelineLayout                        layout = VK_NULL_HANDLE;
	uint32_t                                layoutCacheIdx;
	VkDescriptorSetLayout                   setLayouts[ RAL_VK_MAX_PIPELINE_SETS ];
	VkSpecializationMapEntry                specEntries_v[ RAL_VK_MAX_SPEC_CONSTS ];
	VkSpecializationInfo                    specInfo_v;
	uint32_t                                specData_v[ RAL_VK_MAX_SPEC_CONSTS ];
	qboolean                                haveSpec;
	uint32_t                                i, nSetLayouts;
	VkResult                                r;
	VkPipeline                              vkPipe = VK_NULL_HANDLE;

	if ( !b || !ci ) return NULL;
	if ( !ci->vertexSpirv || ci->vertexSpirvSize == 0 || !ci->fragmentSpirv || ci->fragmentSpirvSize == 0 ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateGraphicsPipeline: missing vertex or fragment SPIR-V (%s)\n", ci->debugName ? ci->debugName : "?" );
		return NULL;
	}
	if ( ci->numVertexBindings   > RAL_VK_MAX_VERT_BINDINGS   ||
	     ci->numVertexAttributes > RAL_VK_MAX_VERT_ATTRIBUTES ||
	     ci->numColorBlends      > RAL_MAX_COLOR_ATTACHMENTS  ||
	     ci->numColorFormats     > RAL_MAX_COLOR_ATTACHMENTS  ||
	     ci->numBindGroupLayouts > RAL_VK_MAX_PIPELINE_SETS   ||
	     ci->numSpecConstants    > RAL_VK_MAX_SPEC_CONSTS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateGraphicsPipeline: input array(s) over the backend's per-pipeline ceiling (%s)\n", ci->debugName ? ci->debugName : "?" );
		return NULL;
	}
	if ( ci->pushConstantSize > b->caps.maxPushConstantSize ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateGraphicsPipeline: pushConstantSize %u > maxPushConstantSize %u (%s)\n",
		        ci->pushConstantSize, b->caps.maxPushConstantSize, ci->debugName ? ci->debugName : "?" );
		return NULL;
	}
	// Vulkan requires every VkPipelineColorBlendAttachmentState to be
	// identical when independentBlend was not enabled (VUID 00605). Reject
	// before layout or shader-module creation, using effective defaults and
	// the explicit-zero write-mask contract.
	if ( !ralVk_ColorBlendStatesSupported( ci, b->caps.independentBlend ) ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateGraphicsPipeline: distinct color attachment states require independentBlend (%s)\n",
		        ci->debugName ? ci->debugName : "?" );
		return NULL;
	}

	// ── pipeline layout ─────────────────────────────────────────────────
	// when `ci->externalLayout` is non-NULL, reuse the
	// caller-provided VkPipelineLayout instead of building one through the
	// backend's layoutCache. The created pipeline carries layoutCacheIdx =
	// 0xFFFFFFFFu (sentinel) so Ral_DestroyPipeline's ralVk_ReleasePipelineLayout
	// call no-ops — caller retains lifetime ownership of the external layout.
	// Identity-sharing the layout with the legacy descriptor-set binds is what
	// allows the parallel buffer's vkCmdDraw to be layout-compatible with the
	// renderer's vkCmdBindDescriptorSets recorded on the same buffer.
	if ( ci->externalLayout != NULL ) {
		layout         = ci->externalLayout->vkHandle;
		layoutCacheIdx = 0xFFFFFFFFu;
		(void)nSetLayouts; (void)setLayouts;
	} else {
		nSetLayouts = ci->numBindGroupLayouts;
		for ( i = 0; i < nSetLayouts; i++ ) setLayouts[i] = ci->bindGroupLayouts[i] ? ci->bindGroupLayouts[i]->layout : VK_NULL_HANDLE;
		layoutCacheIdx = ralVk_GetOrCreatePipelineLayout( b, setLayouts, nSetLayouts,
		                                                  ci->pushConstantSize, ci->pushConstantStages, &layout );
		if ( layoutCacheIdx == 0xFFFFFFFFu || layout == VK_NULL_HANDLE ) return NULL;
	}

	// ── shader modules + stages ─────────────────────────────────────────
	modVert = ralVk_MakeShaderModule( b, ci->vertexSpirv,   ci->vertexSpirvSize   );
	modFrag = ralVk_MakeShaderModule( b, ci->fragmentSpirv, ci->fragmentSpirvSize );
	if ( modVert == VK_NULL_HANDLE || modFrag == VK_NULL_HANDLE ) goto failModules;

	haveSpec = ralVk_BuildSpecInfo( ci->specConstants, ci->numSpecConstants, specEntries_v, specData_v, &specInfo_v );
	RAL_ZERO( stages[0] );
	stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = modVert;
	stages[0].pName  = ( ci->vertexEntry && ci->vertexEntry[0] ) ? ci->vertexEntry : "main";
	stages[0].pSpecializationInfo = haveSpec ? &specInfo_v : NULL;
	RAL_ZERO( stages[1] );
	stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = modFrag;
	stages[1].pName  = ( ci->fragmentEntry && ci->fragmentEntry[0] ) ? ci->fragmentEntry : "main";
	stages[1].pSpecializationInfo = haveSpec ? &specInfo_v : NULL;

	// ── vertex input ────────────────────────────────────────────────────
	for ( i = 0; i < ci->numVertexBindings; i++ ) {
		RAL_ZERO( vBinds[i] );
		vBinds[i].binding   = ci->vertexBindings[i].binding;
		vBinds[i].stride    = ci->vertexBindings[i].stride;
		vBinds[i].inputRate = ralVk_InputRate( ci->vertexBindings[i].inputRate );
	}
	for ( i = 0; i < ci->numVertexAttributes; i++ ) {
		RAL_ZERO( vAttrs[i] );
		vAttrs[i].location = ci->vertexAttributes[i].location;
		vAttrs[i].binding  = ci->vertexAttributes[i].binding;
		vAttrs[i].format   = ralVk_TranslateFormat( ci->vertexAttributes[i].format );
		vAttrs[i].offset   = ci->vertexAttributes[i].offset;
	}
	RAL_ZERO( vi );
	vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vi.vertexBindingDescriptionCount   = ci->numVertexBindings;
	vi.pVertexBindingDescriptions      = ci->numVertexBindings   ? vBinds : NULL;
	vi.vertexAttributeDescriptionCount = ci->numVertexAttributes;
	vi.pVertexAttributeDescriptions    = ci->numVertexAttributes ? vAttrs : NULL;

	// ── input assembly ──────────────────────────────────────────────────
	RAL_ZERO( ia );
	ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = ralVk_Topology( ci->topology );

	// ── viewport / scissor (dynamic — caller drives via Ral_CmdSetViewport/Scissor) ──
	RAL_ZERO( vp );
	vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount  = 1;

	// ── rasterization ───────────────────────────────────────────────────
	RAL_ZERO( rs );
	rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode             = ralVk_PolygonMode( ci->raster.polygonMode );
	rs.cullMode                = ralVk_CullMode( ci->raster.cullMode );
	rs.frontFace               = ralVk_FrontFace( ci->raster.frontFace );
	rs.depthClampEnable        = ci->raster.depthClampEnable ? VK_TRUE : VK_FALSE;
	rs.depthBiasEnable         = ci->raster.depthBiasEnable  ? VK_TRUE : VK_FALSE;
	rs.depthBiasConstantFactor = ci->raster.depthBiasConstant;
	rs.depthBiasSlopeFactor    = ci->raster.depthBiasSlope;
	rs.depthBiasClamp          = ci->raster.depthBiasClamp;
	// (§17.8 gap #4): pass caller's lineWidth through.
	// 0.0f sentinel → default to 1.0f. > 1.0f requires the device's wideLines
	// feature, which the imported-mode device enables when supported.
	rs.lineWidth               = ( ci->raster.lineWidth > 0.0f ) ? ci->raster.lineWidth : 1.0f;

	// ── multisample ─────────────────────────────────────────────────────
	RAL_ZERO( ms );
	ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = (VkSampleCountFlagBits)( ci->sampleCount ? ci->sampleCount : 1 );

	// ── depth/stencil ───────────────────────────────────────────────────
	RAL_ZERO( ds );
	ds.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable       = ci->depthStencil.depthTestEnable  ? VK_TRUE : VK_FALSE;
	ds.depthWriteEnable      = ci->depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
	ds.depthCompareOp        = ralVk_PipelineCompareOp( ci->depthStencil.depthCompareOp );
	ds.depthBoundsTestEnable = VK_FALSE;
	ds.stencilTestEnable     = ci->depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE;
	// (§17.8 gap #1): per-face stencil op state. Only
	// consulted by the driver when stencilTestEnable == VK_TRUE; safe to fill
	// unconditionally (ralStencilOpState_t fields default-zero to KEEP/NEVER/0).
	ralVk_FillStencilOpState( &ds.front, &ci->depthStencil.stencilFront );
	ralVk_FillStencilOpState( &ds.back,  &ci->depthStencil.stencilBack  );

	// ── color blend (one entry per color attachment; missing entries default to "write-all, no blend") ──
	for ( i = 0; i < ci->numColorFormats; i++ ) {
		const ralColorBlendAttachment_t *src = ( i < ci->numColorBlends ) ? &ci->colorBlends[i] : NULL;
		RAL_ZERO( cbAtt[i] );
		if ( src ) {
			cbAtt[i].blendEnable         = src->blendEnable ? VK_TRUE : VK_FALSE;
			cbAtt[i].srcColorBlendFactor = ralVk_BlendFactor( src->srcColor );
			cbAtt[i].dstColorBlendFactor = ralVk_BlendFactor( src->dstColor );
			cbAtt[i].colorBlendOp        = ralVk_BlendOp    ( src->colorOp  );
			cbAtt[i].srcAlphaBlendFactor = ralVk_BlendFactor( src->srcAlpha );
			cbAtt[i].dstAlphaBlendFactor = ralVk_BlendFactor( src->dstAlpha );
			cbAtt[i].alphaBlendOp        = ralVk_BlendOp    ( src->alphaOp  );
			cbAtt[i].colorWriteMask      = ralVk_ColorWriteMask( src );
		} else {
			cbAtt[i].colorWriteMask = RAL_COLOR_WRITE_ALL;
		}
	}
	RAL_ZERO( cb );
	cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = ci->numColorFormats;
	cb.pAttachments    = ci->numColorFormats ? cbAtt : NULL;

	// ── dynamic state (viewport + scissor + depth bias) ─────────────────
	dyn[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dyn[1] = VK_DYNAMIC_STATE_SCISSOR;
	dyn[2] = VK_DYNAMIC_STATE_DEPTH_BIAS;
	RAL_ZERO( dynState );
	dynState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynState.dynamicStateCount = ci->raster.depthBiasEnable ? 3u : 2u;
	dynState.pDynamicStates    = dyn;

	// ── dynamic rendering ───────────────────────────────────────────────
	for ( i = 0; i < ci->numColorFormats; i++ ) colorFmts[i] = ralVk_TranslateFormat( ci->colorFormats[i] );
	RAL_ZERO( dynRendering );
	dynRendering.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	dynRendering.colorAttachmentCount    = ci->numColorFormats;
	dynRendering.pColorAttachmentFormats = ci->numColorFormats ? colorFmts : NULL;
	dynRendering.depthAttachmentFormat   = ralVk_TranslateFormat( ci->depthFormat );
	// A combined depth+stencil depth format implies a stencil aspect on the same
	// attachment; the pipeline must declare a matching stencilAttachmentFormat
	// (else VUID-VkPipelineRenderingCreateInfo-depthAttachmentFormat-08546). For
	// a depth-only format (D16/D32/X8_D24) or no depth (color-only), leave it
	// UNDEFINED — X8_D24's X8 is unused padding, not a stencil aspect.
	dynRendering.stencilAttachmentFormat =
		( ci->depthFormat == RAL_FORMAT_D24_UNORM_S8_UINT ||
		  ci->depthFormat == RAL_FORMAT_D16_UNORM_S8_UINT ||
		  ci->depthFormat == RAL_FORMAT_D32_SFLOAT_S8_UINT )
		? ralVk_TranslateFormat( ci->depthFormat )
		: VK_FORMAT_UNDEFINED;

	// ── go ──────────────────────────────────────────────────────────────
	RAL_ZERO( gpci );
	gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	// RAL graphics pipelines use dynamic rendering exclusively. Legacy
	// VkRenderPass/VkFramebuffer adoption was a callerless migration bridge and
	// is deliberately absent from the public surface.
	gpci.pNext = &dynRendering;

	// Variable-rate shading (pipeline-static rate). Attached ONLY when the pipeline
	// requests a coarser-than-1x1 rate AND the device enabled the feature
	// (caps.variableRateShading) — otherwise the pipeline runs at 1x1 exactly as
	// before (byte-identical). The VRS state chains in front of whatever pNext was set
	// above (dynRendering / NULL), so dynamic rendering is preserved. combinerOps stay
	// KEEP/KEEP (no per-primitive or attachment rate this landing) so the pipeline rate
	// is the final rate. The fragment shader is unchanged — the hardware coarsens.
	if ( ci->shadingRate != RAL_SHADING_RATE_1x1 && b->caps.variableRateShading ) {
		RAL_ZERO( vrsState );
		vrsState.sType         = VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR;
		vrsState.pNext         = gpci.pNext;
		vrsState.fragmentSize  = ralVk_TranslateShadingRate( ci->shadingRate );
		vrsState.combinerOps[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
		vrsState.combinerOps[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
		gpci.pNext             = &vrsState;
	}
	gpci.stageCount          = 2;
	gpci.pStages             = stages;
	gpci.pVertexInputState   = &vi;
	gpci.pInputAssemblyState = &ia;
	gpci.pViewportState      = &vp;
	gpci.pRasterizationState = &rs;
	gpci.pMultisampleState   = &ms;
	gpci.pDepthStencilState  = ( ci->depthFormat != RAL_FORMAT_UNDEFINED ) ? &ds : NULL;
	gpci.pColorBlendState    = &cb;
	gpci.pDynamicState       = &dynState;
	gpci.layout              = layout;
	gpci.renderPass          = VK_NULL_HANDLE;
	gpci.subpass             = 0;

	r = b->vk.CreateGraphicsPipelines( b->device, b->pipelineCache, 1, &gpci, NULL, &vkPipe );
	b->vk.DestroyShaderModule( b->device, modVert, NULL );
	b->vk.DestroyShaderModule( b->device, modFrag, NULL );
	if ( r != VK_SUCCESS || vkPipe == VK_NULL_HANDLE ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateGraphicsPipeline: vkCreateGraphicsPipelines failed (VkResult %d) — %s\n", (int)r, ci->debugName ? ci->debugName : "?" );
		ralVk_ReleasePipelineLayout( b, layoutCacheIdx );
		return NULL;
	}

	p = (ralPipeline_t *)malloc( sizeof( *p ) );
	if ( !p ) { b->vk.DestroyPipeline( b->device, vkPipe, NULL ); ralVk_ReleasePipelineLayout( b, layoutCacheIdx ); return NULL; }
	RAL_ZERO( *p );
	p->header.refCount     = 1;
	p->backend             = b;
	p->pipeline            = vkPipe;
	p->layout              = layout;
	p->layoutCacheIndex    = layoutCacheIdx;
	p->bindPoint           = VK_PIPELINE_BIND_POINT_GRAPHICS;
	p->pushConstantSize    = ci->pushConstantSize;
	p->pushConstantStages  = ralVk_PushConstantStages( ci->pushConstantStages );
	ralVk_SetObjectName( b, (uint64_t)vkPipe, VK_OBJECT_TYPE_PIPELINE, ci->debugName );
	return p;

failModules:
	if ( modVert != VK_NULL_HANDLE ) b->vk.DestroyShaderModule( b->device, modVert, NULL );
	if ( modFrag != VK_NULL_HANDLE ) b->vk.DestroyShaderModule( b->device, modFrag, NULL );
	ralVk_ReleasePipelineLayout( b, layoutCacheIdx );
	return NULL;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_CreateComputePipeline
// ════════════════════════════════════════════════════════════════════════
ralPipeline_t *Ral_CreateComputePipeline( ralBackend_t *b, const ralComputePipelineCreateInfo_t *ci ) {
	ralPipeline_t                     *p;
	VkShaderModule                     mod = VK_NULL_HANDLE;
	VkPipelineShaderStageCreateInfo    stage;
	VkComputePipelineCreateInfo        cpci;
	VkPipelineLayout                   layout = VK_NULL_HANDLE;
	uint32_t                           layoutCacheIdx;
	VkDescriptorSetLayout              setLayouts[ RAL_VK_MAX_PIPELINE_SETS ];
	VkSpecializationMapEntry           specEntries[ RAL_VK_MAX_SPEC_CONSTS ];
	VkSpecializationInfo               specInfo;
	uint32_t                           specData[ RAL_VK_MAX_SPEC_CONSTS ];
	qboolean                           haveSpec;
	uint32_t                           i;
	VkResult                           r;
	VkPipeline                         vkPipe = VK_NULL_HANDLE;

	if ( !b || !ci ) return NULL;
	if ( !ci->computeSpirv || ci->computeSpirvSize == 0 ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateComputePipeline: missing compute SPIR-V (%s)\n", ci->debugName ? ci->debugName : "?" );
		return NULL;
	}
	if ( ci->numBindGroupLayouts > RAL_VK_MAX_PIPELINE_SETS || ci->numSpecConstants > RAL_VK_MAX_SPEC_CONSTS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateComputePipeline: input array over per-pipeline ceiling (%s)\n", ci->debugName ? ci->debugName : "?" );
		return NULL;
	}
	if ( ci->pushConstantSize > b->caps.maxPushConstantSize ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateComputePipeline: pushConstantSize %u > maxPushConstantSize %u (%s)\n",
		        ci->pushConstantSize, b->caps.maxPushConstantSize, ci->debugName ? ci->debugName : "?" );
		return NULL;
	}

	// same externalLayout opt-in as Ral_CreateGraphicsPipeline.
	if ( ci->externalLayout != NULL ) {
		layout         = ci->externalLayout->vkHandle;
		layoutCacheIdx = 0xFFFFFFFFu;
		(void)setLayouts;
	} else {
		for ( i = 0; i < ci->numBindGroupLayouts; i++ ) setLayouts[i] = ci->bindGroupLayouts[i] ? ci->bindGroupLayouts[i]->layout : VK_NULL_HANDLE;
		layoutCacheIdx = ralVk_GetOrCreatePipelineLayout( b, setLayouts, ci->numBindGroupLayouts,
		                                                  ci->pushConstantSize, RAL_STAGE_COMPUTE, &layout );
		if ( layoutCacheIdx == 0xFFFFFFFFu || layout == VK_NULL_HANDLE ) return NULL;
	}

	mod = ralVk_MakeShaderModule( b, ci->computeSpirv, ci->computeSpirvSize );
	if ( mod == VK_NULL_HANDLE ) { ralVk_ReleasePipelineLayout( b, layoutCacheIdx ); return NULL; }

	haveSpec = ralVk_BuildSpecInfo( ci->specConstants, ci->numSpecConstants, specEntries, specData, &specInfo );

	RAL_ZERO( stage );
	stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = mod;
	stage.pName  = ( ci->computeEntry && ci->computeEntry[0] ) ? ci->computeEntry : "main";
	stage.pSpecializationInfo = haveSpec ? &specInfo : NULL;

	RAL_ZERO( cpci );
	cpci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage  = stage;
	cpci.layout = layout;
	r = b->vk.CreateComputePipelines( b->device, b->pipelineCache, 1, &cpci, NULL, &vkPipe );
	b->vk.DestroyShaderModule( b->device, mod, NULL );
	if ( r != VK_SUCCESS || vkPipe == VK_NULL_HANDLE ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateComputePipeline: vkCreateComputePipelines failed (VkResult %d) — %s\n", (int)r, ci->debugName ? ci->debugName : "?" );
		ralVk_ReleasePipelineLayout( b, layoutCacheIdx );
		return NULL;
	}

	p = (ralPipeline_t *)malloc( sizeof( *p ) );
	if ( !p ) { b->vk.DestroyPipeline( b->device, vkPipe, NULL ); ralVk_ReleasePipelineLayout( b, layoutCacheIdx ); return NULL; }
	RAL_ZERO( *p );
	p->header.refCount     = 1;
	p->backend             = b;
	p->pipeline            = vkPipe;
	p->layout              = layout;
	p->layoutCacheIndex    = layoutCacheIdx;
	p->bindPoint           = VK_PIPELINE_BIND_POINT_COMPUTE;
	p->pushConstantSize    = ci->pushConstantSize;
	p->pushConstantStages  = VK_SHADER_STAGE_COMPUTE_BIT;
	ralVk_SetObjectName( b, (uint64_t)vkPipe, VK_OBJECT_TYPE_PIPELINE, ci->debugName );
	return p;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_DestroyPipeline (defer-destroy VkPipeline; release the layout refcount —
// when it hits 0 the layout itself gets defer-destroyed)
// ════════════════════════════════════════════════════════════════════════
void Ral_DestroyPipeline( ralPipeline_t *p ) {
	ralBackend_t *b;
	if ( !p ) return;
	if ( p->header.refCount > 1 ) { p->header.refCount--; return; }
	b = p->backend;
	if ( p->pipeline != VK_NULL_HANDLE )
		ralVk_DeferDestroy( b, RAL_RES_PIPELINE, RAL_VK_H2U( p->pipeline ), 0, NULL );
	ralVk_ReleasePipelineLayout( b, p->layoutCacheIndex );
	free( p );
}

// ════════════════════════════════════════════════════════════════════════
// Ral_SavePipelineCache / Ral_LoadPipelineCache (on-disk persistence per §7.4)
// ════════════════════════════════════════════════════════════════════════
void Ral_SavePipelineCache( ralBackend_t *b, const char *path ) {
	size_t       sz = 0;
	void        *data;
	VkResult     r;
	FILE        *f;
	if ( !b || !path || b->pipelineCache == VK_NULL_HANDLE ) {
		RAL_VK_LOG( SEV_INFO, "Ral_SavePipelineCache: entry skipped (b=%p, path=%s, cache=%s)\n",
			(void *)b, path ? path : "(null)",
			( b && b->pipelineCache != VK_NULL_HANDLE ) ? "live" : "NULL_HANDLE" );
		return;
	}
	r = b->vk.GetPipelineCacheData( b->device, b->pipelineCache, &sz, NULL );
	RAL_VK_LOG( SEV_INFO, "Ral_SavePipelineCache: entered (path='%s', cache size probe=%u bytes, VkResult=%d)\n",
		path, (unsigned)sz, (int)r );
	if ( r != VK_SUCCESS || sz == 0 ) {
		RAL_VK_LOG( SEV_INFO, "Ral_SavePipelineCache: no pipelines created this session, cache empty, no-op (VkResult %d)\n", (int)r );
		return;
	}
	data = malloc( sz );
	if ( !data ) return;
	r = b->vk.GetPipelineCacheData( b->device, b->pipelineCache, &sz, data );
	if ( r != VK_SUCCESS ) { free( data ); RAL_VK_LOG( SEV_WARN, "Ral_SavePipelineCache: vkGetPipelineCacheData failed (%d)\n", (int)r ); return; }
	f = fopen( path, "wb" );
	if ( !f ) { free( data ); RAL_VK_LOG( SEV_WARN, "Ral_SavePipelineCache: cannot open '%s' for writing\n", path ); return; }
	(void)fwrite( data, 1, sz, f );
	fclose( f );
	free( data );
	RAL_VK_LOG( SEV_INFO, "Ral_SavePipelineCache: wrote %u bytes to '%s'\n", (unsigned)sz, path );
}

void Ral_LoadPipelineCache( ralBackend_t *b, const char *path ) {
	FILE                      *f;
	long                       sz;
	void                      *data;
	VkPipelineCacheCreateInfo  pci;
	VkPipelineCache            newCache = VK_NULL_HANDLE;
	if ( !b || !path ) return;
	f = fopen( path, "rb" );
	if ( !f ) { RAL_VK_LOG( SEV_INFO, "Ral_LoadPipelineCache: '%s' not present — pipeline cache regenerating from scratch\n", path ); return; }
	fseek( f, 0, SEEK_END ); sz = ftell( f ); fseek( f, 0, SEEK_SET );
	if ( sz <= 0 ) { fclose( f ); RAL_VK_LOG( SEV_INFO, "Ral_LoadPipelineCache: '%s' empty — regenerating\n", path ); return; }
	data = malloc( (size_t)sz );
	if ( !data ) { fclose( f ); return; }
	if ( fread( data, 1, (size_t)sz, f ) != (size_t)sz ) { free( data ); fclose( f ); RAL_VK_LOG( SEV_WARN, "Ral_LoadPipelineCache: short read on '%s' — regenerating\n", path ); return; }
	fclose( f );
	RAL_ZERO( pci );
	pci.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	pci.initialDataSize = (size_t)sz;
	pci.pInitialData    = data;
	// vkCreatePipelineCache validates the header internally; a stale
	// VK_PIPELINE_CACHE_HEADER_VERSION_ONE / vendor/device UUID makes it
	// silently regenerate (VK_SUCCESS, internal cache reset). VK_ERROR_*
	// here means we can't seed at all — leave the old cache in place.
	if ( b->vk.CreatePipelineCache( b->device, &pci, NULL, &newCache ) != VK_SUCCESS || newCache == VK_NULL_HANDLE ) {
		RAL_VK_LOG( SEV_INFO, "Ral_LoadPipelineCache: vkCreatePipelineCache rejected seed data — pipeline cache regenerating\n" );
		free( data );
		return;
	}
	// The old VkPipelineCache is only consulted transiently inside
	// vkCreate*Pipelines — pipelines don't retain a reference to it. By the
	// time Ral_LoadPipelineCache runs we assume the caller hasn't started any
	// pipeline creation racing with this swap (typical: load happens at
	// backend init before the renderer kicks off). vkDeviceWaitIdle guards
	// the corner case of an out-of-band live creation.
	if ( b->pipelineCache != VK_NULL_HANDLE ) {
		b->vk.DeviceWaitIdle( b->device );
		b->vk.DestroyPipelineCache( b->device, b->pipelineCache, NULL );
	}
	b->pipelineCache = newCache;
	free( data );
	RAL_VK_LOG( SEV_INFO, "Ral_LoadPipelineCache: seeded %ld bytes from '%s'\n", sz, path );
}

// ════════════════════════════════════════════════════════════════════════
// \ral_dump pipeline — exercises pipeline create/destroy, the layout cache,
// dynamic rendering with a real draw + readback, a compute dispatch with a
// bind-group + push constant + readback, and the pipeline-cache save/load
// roundtrip.
// ════════════════════════════════════════════════════════════════════════

#define RAL_PIPELINE_TEST_RT_SIZE 64u    // render-target side; 64×64 keeps the readback under 16 KiB

// vec4 pos + vec4 color (16-byte aligned attributes) — the RAL surface has no
// R32G32B32_SFLOAT format yet, so we pad to vec4 and let the GLSL `in vec3`
// declaration drop the W lane. Stride 32 bytes.
typedef struct { float pos[4]; float color[4]; } ralPipelineTestVertex_t;

// triangle covering the centre of NDC — identity MVP, no z transform; pixel
// (32, 32) (centre) lies inside the triangle so the readback can verify a
// non-clear colour at that point.
static const ralPipelineTestVertex_t ral_pipeline_test_verts[3] = {
	{ {  0.0f, -0.5f, 0.5f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },   // top         (red)
	{ {  0.5f,  0.5f, 0.5f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },   // bottom right (green)
	{ { -0.5f,  0.5f, 0.5f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },   // bottom left  (blue)
};
static const uint16_t ral_pipeline_test_indices[3] = { 0, 1, 2 };

// identity matrix — 64 bytes, lined up at push-constant offset 0.
static const float ral_pipeline_test_identity[16] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f,
};

static ralPipeline_t *ralVk_BuildPipelineTestGraphicsPipeline( ralBackend_t *b, ralFormat_t colorFmt, ralFormat_t depthFmt, const char *name ) {
	ralVertexBinding_t              vbind;
	ralVertexAttribute_t            vattrs[2];
	ralColorBlendAttachment_t       cb;
	ralGraphicsPipelineCreateInfo_t gci;

	RAL_ZERO( vbind );
	vbind.binding   = 0;
	vbind.stride    = sizeof( ralPipelineTestVertex_t );
	vbind.inputRate = RAL_VERTEX_INPUT_PER_VERTEX;
	RAL_ZERO( vattrs[0] );
	vattrs[0].location = 0; vattrs[0].binding = 0; vattrs[0].format = RAL_FORMAT_R32G32B32A32_SFLOAT;   // pos.xyz (consumed as vec3 in shader; xyzw lane W ignored)
	vattrs[0].offset   = 0;
	RAL_ZERO( vattrs[1] );
	vattrs[1].location = 1; vattrs[1].binding = 0; vattrs[1].format = RAL_FORMAT_R32G32B32A32_SFLOAT;
	vattrs[1].offset   = (uint32_t)( sizeof( float ) * 4 );   // pos[4] precedes color[4]
	RAL_ZERO( cb );
	cb.writeMask = RAL_COLOR_WRITE_ALL;

	RAL_ZERO( gci );
	gci.vertexSpirv         = ral_pipeline_test_vert_spv;
	gci.vertexSpirvSize     = ral_pipeline_test_vert_spv_size;
	gci.fragmentSpirv       = ral_pipeline_test_frag_spv;
	gci.fragmentSpirvSize   = ral_pipeline_test_frag_spv_size;
	gci.vertexBindings      = &vbind;     gci.numVertexBindings   = 1;
	gci.vertexAttributes    = vattrs;     gci.numVertexAttributes = 2;
	gci.topology            = RAL_TOPOLOGY_TRIANGLE_LIST;
	gci.raster.polygonMode  = RAL_POLYGON_FILL;
	gci.raster.cullMode     = RAL_CULL_NONE;
	gci.raster.frontFace    = RAL_FRONT_FACE_CCW;
	gci.depthStencil.depthTestEnable  = qtrue;
	gci.depthStencil.depthWriteEnable = qtrue;
	gci.depthStencil.depthCompareOp   = RAL_COMPARE_LESS_EQUAL;
	gci.colorBlends         = &cb;        gci.numColorBlends      = 1;
	gci.colorFormats[0]     = colorFmt;
	gci.numColorFormats     = 1;
	gci.depthFormat         = depthFmt;
	gci.sampleCount         = 1;
	gci.pushConstantSize    = 64;
	gci.pushConstantStages  = RAL_STAGE_VERTEX;
	gci.debugName           = name;
	return Ral_CreateGraphicsPipeline( b, &gci );
}

void ralVk_RunPipelineTest( ralBackend_t *b ) {
	const ralFormat_t  COLOR_FMT  = RAL_FORMAT_R8G8B8A8_UNORM;
	const ralFormat_t  DEPTH_FMT  = RAL_FORMAT_D32_SFLOAT;
	const uint32_t     RT_SIZE    = RAL_PIPELINE_TEST_RT_SIZE;

	RAL_VK_LOG( SEV_INFO, "===== RAL pipeline test (Phase 7.3c) =====\n" );
	RAL_VK_LOG( SEV_INFO, "  pipelineCache present: %s; layoutCache slots in use: %u\n",
	        ( b->pipelineCache != VK_NULL_HANDLE ) ? "yes" : "no", b->numLayoutCache );

	// slot dump (pre-test snapshot).
	// On the \ral_dump pipeline throwaway-backend path this prints an empty
	// cache; on the live-backend path (via Ral_DumpLive's mirror call) it
	// shows real renderer state.
	Ral_DumpPipelineLayoutCache( b );

	// ── (1) create + destroy 100 graphics pipelines sharing one layout ──
	{
		ralPipeline_t *pipes[100];
		uint32_t       i, distinctLayouts0, distinctLayouts1, refCountSeen = 0, nCreated = 0;
		uint32_t       sharedSlot = 0xFFFFFFFFu;
		distinctLayouts0 = b->numLayoutCache;
		for ( i = 0; i < 100; i++ ) {
			pipes[i] = ralVk_BuildPipelineTestGraphicsPipeline( b, COLOR_FMT, DEPTH_FMT, "ral-pipeline-test-share" );
			if ( pipes[i] ) nCreated++;
		}
		distinctLayouts1 = b->numLayoutCache;
		for ( i = 0; i < 100; i++ ) if ( pipes[i] ) { sharedSlot = pipes[i]->layoutCacheIndex; break; }
		if ( sharedSlot != 0xFFFFFFFFu ) refCountSeen = b->layoutCache[sharedSlot].refCount;
		RAL_VK_LOG( SEV_INFO, "  layout-cache share: %u/100 pipelines created; distinct layout-cache entries grew %u → %u (expect +1); shared-slot refCount = %u (expect == created)\n",
		        nCreated, distinctLayouts0, distinctLayouts1, refCountSeen );
		for ( i = 0; i < 100; i++ ) if ( pipes[i] ) Ral_DestroyPipeline( pipes[i] );
		// drain the destroys
		for ( i = 0; i < RAL_VK_MAX_FRAMES_IN_FLIGHT + 1u; i++ ) { Ral_BeginFrame( b ); Ral_EndFrame( b ); }
		RAL_VK_LOG( SEV_INFO, "  layout-cache share: after destroy, shared-slot refCount = %u (expect 0; layout VkHandle = %s)\n",
		        ( sharedSlot != 0xFFFFFFFFu ) ? b->layoutCache[sharedSlot].refCount : 99u,
		        ( sharedSlot != 0xFFFFFFFFu && b->layoutCache[sharedSlot].layout == VK_NULL_HANDLE ) ? "VK_NULL_HANDLE (defer-destroyed)" : "still live" );
	}

	// ── (2) full draw + readback (BindPipeline, BindVertex/IndexBuffer, PushConstants, BeginRendering, DrawIndexed) ──
	{
		ralBufferCreateInfo_t  bci;
		ralTextureCreateInfo_t tci;
		ralBuffer_t           *vb = NULL, *ib = NULL, *readback = NULL, *vbStaging = NULL, *ibStaging = NULL;
		ralTexture_t          *color = NULL, *depth = NULL;
		ralPipeline_t         *pipe = NULL;
		ralFence_t            *fence = NULL;
		ralCommandBuffer_t    *cb = NULL;
		void                  *map;
		// vertex + index buffers (device-local, uploaded via staging)
		RAL_ZERO( bci ); bci.size = sizeof( ral_pipeline_test_verts ); bci.usage = RAL_BUFFER_VERTEX  | RAL_BUFFER_TRANSFER_DST; bci.memory = RAL_MEMORY_DEVICE_LOCAL; bci.debugName = "ral-pipeline-test-vb";
		vb = Ral_CreateBuffer( b, &bci );
		RAL_ZERO( bci ); bci.size = sizeof( ral_pipeline_test_indices ); bci.usage = RAL_BUFFER_INDEX | RAL_BUFFER_TRANSFER_DST; bci.memory = RAL_MEMORY_DEVICE_LOCAL; bci.debugName = "ral-pipeline-test-ib";
		ib = Ral_CreateBuffer( b, &bci );
		// readback buffer for the color target
		RAL_ZERO( bci ); bci.size = RT_SIZE * RT_SIZE * 4u; bci.usage = RAL_BUFFER_TRANSFER_DST; bci.memory = RAL_MEMORY_HOST_COHERENT; bci.debugName = "ral-pipeline-test-readback";
		readback = Ral_CreateBuffer( b, &bci );
		// targets
		RAL_ZERO( tci ); tci.type = RAL_TEXTURE_2D; tci.format = COLOR_FMT; tci.width = RT_SIZE; tci.height = RT_SIZE;
		tci.mipLevels = 1; tci.sampleCount = 1; tci.usage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT; tci.memory = RAL_MEMORY_DEVICE_LOCAL; tci.debugName = "ral-pipeline-test-color";
		color = Ral_CreateTexture( b, &tci );
		RAL_ZERO( tci ); tci.type = RAL_TEXTURE_2D; tci.format = DEPTH_FMT; tci.width = RT_SIZE; tci.height = RT_SIZE;
		tci.mipLevels = 1; tci.sampleCount = 1; tci.usage = RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT; tci.memory = RAL_MEMORY_DEVICE_LOCAL; tci.debugName = "ral-pipeline-test-depth";
		depth = Ral_CreateTexture( b, &tci );
		pipe  = ralVk_BuildPipelineTestGraphicsPipeline( b, COLOR_FMT, DEPTH_FMT, "ral-pipeline-test-draw" );

		if ( vb && ib && readback && color && depth && pipe ) {
			// async upload vertex + index data via staging (the upload path
			// is exercised by Ral_BufferUploadAsync which returns an already-
			// signaled fence per the async-upload semantics).
			ralFence_t *f1 = Ral_BufferUploadAsync( vb, 0, ral_pipeline_test_verts,   sizeof( ral_pipeline_test_verts   ) );
			ralFence_t *f2 = Ral_BufferUploadAsync( ib, 0, ral_pipeline_test_indices, sizeof( ral_pipeline_test_indices ) );
			if ( f1 ) { Ral_WaitFence( f1, ~0ull ); Ral_DestroyFence( f1 ); }
			if ( f2 ) { Ral_WaitFence( f2, ~0ull ); Ral_DestroyFence( f2 ); }

			cb    = Ral_AcquireCommandBuffer( b, RAL_QUEUE_GRAPHICS );
			fence = Ral_CreateFence( b );
			if ( cb && fence ) {
				ralCommandBuffer_t   *cbs[1]; ralSubmitInfo_t si;
				ralRenderingInfo_t    ri2;
				ralViewport_t         vp;
				ralRect_t             sc;
				ralBufferTextureCopy_t btc;
				VkImageMemoryBarrier  toSrc;

				Ral_BeginCommandBuffer( cb );
				Ral_CmdPipelineBarrier( cb, RAL_BARRIER_TRANSFER_TO_GRAPHICS );   // ensure the vertex/index uploads are visible to vertex input
				RAL_ZERO( vp ); vp.x = 0.0f; vp.y = 0.0f; vp.width = (float)RT_SIZE; vp.height = (float)RT_SIZE; vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
				RAL_ZERO( sc ); sc.x = 0; sc.y = 0; sc.width = RT_SIZE; sc.height = RT_SIZE;
				Ral_CmdSetViewport( cb, &vp );
				Ral_CmdSetScissor ( cb, &sc );
				RAL_ZERO( ri2 );
				ri2.colorAttachments[0]   = color;
				ri2.colorLoadOps  [0]     = RAL_LOAD_OP_CLEAR;
				ri2.colorStoreOps [0]     = RAL_STORE_OP_STORE;
				ri2.colorClears   [0].color[0] = 0.1f;
				ri2.colorClears   [0].color[1] = 0.1f;
				ri2.colorClears   [0].color[2] = 0.1f;
				ri2.colorClears   [0].color[3] = 1.0f;
				ri2.numColorAttachments   = 1;
				ri2.depthAttachment       = depth;
				ri2.depthLoadOp           = RAL_LOAD_OP_CLEAR;
				ri2.depthStoreOp          = RAL_STORE_OP_DONT_CARE;
				ri2.depthClear            = 1.0f;
				ri2.renderArea            = sc;
				Ral_BeginRendering( cb, &ri2 );
				Ral_CmdBindPipeline    ( cb, pipe );
				Ral_CmdBindVertexBuffer( cb, 0, vb, 0 );
				Ral_CmdBindIndexBuffer ( cb, ib, 0, RAL_INDEX_UINT16 );
				Ral_CmdPushConstants   ( cb, RAL_STAGE_VERTEX, 0, sizeof( ral_pipeline_test_identity ), ral_pipeline_test_identity );
				Ral_CmdDrawIndexed     ( cb, 3, 1, 0, 0, 0 );
				Ral_EndRendering( cb );
				// transition `color` → TRANSFER_SRC and copy to readback via the raw PFN
				RAL_ZERO( toSrc );
				toSrc.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				toSrc.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				toSrc.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
				toSrc.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				toSrc.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				toSrc.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
				toSrc.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
				toSrc.image                           = color->image;
				toSrc.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
				toSrc.subresourceRange.baseMipLevel   = 0;
				toSrc.subresourceRange.levelCount     = 1;
				toSrc.subresourceRange.baseArrayLayer = 0;
				toSrc.subresourceRange.layerCount     = 1;
				b->vk.CmdPipelineBarrier( cb->cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toSrc );
				color->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				RAL_ZERO( btc );
				btc.bufferOffset      = 0;
				btc.mipLevel          = 0;
				btc.arrayLayer        = 0;
				btc.imageRect.x       = 0;
				btc.imageRect.y       = 0;
				btc.imageRect.width   = RT_SIZE;
				btc.imageRect.height  = RT_SIZE;
				Ral_CmdCopyTextureToBuffer( cb, color, readback, &btc );
				Ral_EndCommandBuffer( cb );

				cbs[0] = cb; RAL_ZERO( si ); si.commandBuffers = cbs; si.numCommandBuffers = 1; si.signalFence = fence;
				Ral_Submit( b, RAL_QUEUE_GRAPHICS, &si );
				Ral_WaitFence( fence, ~0ull );

				map = Ral_MapBuffer( readback );
				if ( map ) {
					const uint8_t *px = (const uint8_t *)map + ( RT_SIZE / 2u ) * ( RT_SIZE * 4u ) + ( RT_SIZE / 2u ) * 4u;
					const uint8_t *clearPx = (const uint8_t *)map;   // (0,0) is outside the triangle → should still be ~0.1 grey
					uint32_t       brightPixels = 0, k;
					for ( k = 0; k < RT_SIZE * RT_SIZE; k++ ) {
						const uint8_t *p = (const uint8_t *)map + k * 4u;
						if ( p[0] > 32 || p[1] > 32 || p[2] > 32 ) brightPixels++;
					}
					RAL_VK_LOG( SEV_INFO, "  draw: pixel(32,32) RGBA = %u %u %u %u (expect non-grey: triangle interior); pixel(0,0) = %u %u %u %u (expect ~26 = 0.1×255 clear); bright (>0.125) pixels = %u/%u (expect a substantial fraction inside the centred triangle)\n",
					        px[0], px[1], px[2], px[3], clearPx[0], clearPx[1], clearPx[2], clearPx[3], brightPixels, RT_SIZE * RT_SIZE );
					Ral_UnmapBuffer( readback );
				} else RAL_VK_LOG( SEV_WARN, "  draw: readback map failed\n" );
			} else RAL_VK_LOG( SEV_WARN, "  draw: command buffer / fence acquisition failed\n" );
		} else RAL_VK_LOG( SEV_WARN, "  draw: resource creation failed (vb=%p ib=%p readback=%p color=%p depth=%p pipe=%p)\n",
		                (void*)vb, (void*)ib, (void*)readback, (void*)color, (void*)depth, (void*)pipe );
		(void)vbStaging; (void)ibStaging;
		if ( pipe )     Ral_DestroyPipeline( pipe );
		if ( cb )       Ral_DestroyCommandBuffer( cb );
		if ( fence )    Ral_DestroyFence( fence );
		if ( depth )    Ral_DestroyTexture( depth );
		if ( color )    Ral_DestroyTexture( color );
		if ( readback ) Ral_DestroyBuffer( readback );
		if ( ib )       Ral_DestroyBuffer( ib );
		if ( vb )       Ral_DestroyBuffer( vb );
	}

	// ── (3) compute dispatch + SSBO readback ──────────────────────────
	{
		const uint32_t                   COUNT = 256u;
		ralBufferCreateInfo_t            bci;
		ralBindEntry_t                   be;
		ralBindGroupLayoutCreateInfo_t   lci;
		ralBindGroupCreateInfo_t         bgci;
		ralBindingValue_t                bv;
		ralComputePipelineCreateInfo_t   cci;
		const ralBindGroupLayout_t      *layouts[1];
		ralBuffer_t                     *ssbo = NULL, *cReadback = NULL;
		ralBindGroupLayout_t            *bgl = NULL;
		ralBindGroup_t                  *bg  = NULL;
		ralPipeline_t                   *pipe = NULL;
		ralCommandBuffer_t              *cb   = NULL;
		ralFence_t                      *f    = NULL;
		void                            *map;

		RAL_ZERO( bci ); bci.size = COUNT * sizeof( uint32_t ); bci.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_SRC; bci.memory = RAL_MEMORY_DEVICE_LOCAL; bci.debugName = "ral-pipeline-test-ssbo";
		ssbo = Ral_CreateBuffer( b, &bci );
		RAL_ZERO( bci ); bci.size = COUNT * sizeof( uint32_t ); bci.usage = RAL_BUFFER_TRANSFER_DST; bci.memory = RAL_MEMORY_HOST_COHERENT; bci.debugName = "ral-pipeline-test-ssbo-readback";
		cReadback = Ral_CreateBuffer( b, &bci );

		RAL_ZERO( be ); be.binding = 0; be.type = RAL_BIND_STORAGE_BUFFER; be.count = 1; be.stageFlags = RAL_STAGE_COMPUTE;
		RAL_ZERO( lci ); lci.entries = &be; lci.numEntries = 1; lci.debugName = "ral-pipeline-test-comp-layout";
		bgl = Ral_CreateBindGroupLayout( b, &lci );

		if ( bgl && ssbo ) {
			RAL_ZERO( bv ); bv.binding = 0; bv.type = RAL_BIND_STORAGE_BUFFER; bv.buffer = ssbo; bv.bufferOffset = 0; bv.bufferRange = COUNT * sizeof( uint32_t );
			RAL_ZERO( bgci ); bgci.layout = bgl; bgci.values = &bv; bgci.numValues = 1; bgci.debugName = "ral-pipeline-test-comp-bg";
			bg = Ral_CreateBindGroup( b, &bgci );
		}

		layouts[0] = bgl;
		RAL_ZERO( cci );
		cci.computeSpirv        = ral_pipeline_test_comp_spv;
		cci.computeSpirvSize    = ral_pipeline_test_comp_spv_size;
		cci.bindGroupLayouts    = layouts;
		cci.numBindGroupLayouts = 1;
		cci.pushConstantSize    = 4;
		cci.debugName           = "ral-pipeline-test-comp";
		pipe = bgl ? Ral_CreateComputePipeline( b, &cci ) : NULL;

		if ( bgl && bg && ssbo && cReadback && pipe ) {
			cb = Ral_AcquireCommandBuffer( b, RAL_QUEUE_GRAPHICS );
			f  = Ral_CreateFence( b );
			if ( cb && f ) {
				ralCommandBuffer_t *cbs[1]; ralSubmitInfo_t si; ralBufferCopy_t copy;
				Ral_BeginCommandBuffer( cb );
				Ral_CmdBindPipeline ( cb, pipe );
				Ral_CmdBindBindGroup( cb, 0, bg );
				Ral_CmdPushConstants( cb, RAL_STAGE_COMPUTE, 0, sizeof( COUNT ), &COUNT );
				Ral_CmdDispatch     ( cb, ( COUNT + 63u ) / 64u, 1, 1 );
				Ral_CmdPipelineBarrier( cb, RAL_BARRIER_COMPUTE_TO_TRANSFER );   // compute write → transfer read
				RAL_ZERO( copy ); copy.size = COUNT * sizeof( uint32_t );
				Ral_CmdCopyBuffer( cb, ssbo, cReadback, &copy );
				Ral_EndCommandBuffer( cb );
				cbs[0] = cb; RAL_ZERO( si ); si.commandBuffers = cbs; si.numCommandBuffers = 1; si.signalFence = f;
				Ral_Submit( b, RAL_QUEUE_GRAPHICS, &si );
				Ral_WaitFence( f, ~0ull );
				map = Ral_MapBuffer( cReadback );
				if ( map ) {
					const uint32_t *data = (const uint32_t *)map;
					uint32_t k, badCount = 0, sampleBad = 0xFFFFFFFFu, sampleExpect = 0, sampleGot = 0;
					for ( k = 0; k < COUNT; k++ ) {
						uint32_t expect = k * 3u + 7u;
						if ( data[k] != expect ) {
							badCount++;
							if ( sampleBad == 0xFFFFFFFFu ) { sampleBad = k; sampleExpect = expect; sampleGot = data[k]; }
						}
					}
					if ( badCount == 0 )
						RAL_VK_LOG( SEV_INFO, "  compute: 256 elements all match idx*3+7 — data[10]=%u (expect 37), data[200]=%u (expect 607), data[255]=%u (expect 772)\n",
						        data[10], data[200], data[255] );
					else
						RAL_VK_LOG( SEV_WARN, "  compute: %u/%u elements mismatch — first bad at idx %u (expect %u, got %u)\n",
						        badCount, COUNT, sampleBad, sampleExpect, sampleGot );
					Ral_UnmapBuffer( cReadback );
				} else RAL_VK_LOG( SEV_WARN, "  compute: readback map failed\n" );
			} else RAL_VK_LOG( SEV_WARN, "  compute: command buffer / fence acquisition failed\n" );
		} else RAL_VK_LOG( SEV_WARN, "  compute: setup failed (ssbo=%p readback=%p layout=%p bg=%p pipe=%p)\n", (void*)ssbo, (void*)cReadback, (void*)bgl, (void*)bg, (void*)pipe );

		if ( cb )        Ral_DestroyCommandBuffer( cb );
		if ( f )         Ral_DestroyFence( f );
		if ( pipe )      Ral_DestroyPipeline( pipe );
		if ( bg )        Ral_DestroyBindGroup( bg );
		if ( bgl )       Ral_DestroyBindGroupLayout( bgl );
		if ( cReadback ) Ral_DestroyBuffer( cReadback );
		if ( ssbo )      Ral_DestroyBuffer( ssbo );
	}

	// ── (4) restricted residency-view sample + readback ──────────────
	{
		uint8_t                         mip0[4u * 4u * 4u];
		const uint8_t                   mip2[4] = { 0u, 255u, 0u, 255u };
		ralTextureCreateInfo_t          tci;
		ralTextureUploadDesc_t          upload;
		ralTextureViewCreateInfo_t      vci;
		ralSamplerCreateInfo_t          sci;
		ralBufferCreateInfo_t           bci;
		ralBindEntry_t                  entries[3];
		ralBindGroupLayoutCreateInfo_t  lci;
		ralBindingValue_t               values[3];
		ralBindGroupCreateInfo_t        bgci;
		ralComputePipelineCreateInfo_t  cci;
		const ralBindGroupLayout_t     *layouts[1];
		ralTexture_t                   *tex = NULL;
		ralTextureView_t               *fullView = NULL, *coarseView = NULL;
		ralSampler_t                   *sampler = NULL;
		ralBuffer_t                    *sampleOut = NULL, *sampleReadback = NULL;
		ralBindGroupLayout_t           *layout = NULL;
		ralBindGroup_t                 *group = NULL;
		ralPipeline_t                  *pipe = NULL;
		ralCommandBuffer_t             *cb = NULL, *readyCb = NULL;
		ralFence_t                     *fence = NULL, *uploadFence = NULL;
		ralSemaphore_t                 *readySemaphore = NULL;
		ralUploadTicket_t               acquireTicket;
		qboolean                        acquired = qfalse;
		uint32_t                        i;

		for ( i = 0; i < sizeof( mip0 ); i += 4u ) {
			mip0[i + 0u] = 255u; mip0[i + 1u] = 0u; mip0[i + 2u] = 0u; mip0[i + 3u] = 255u;
		}
		RAL_ZERO( tci );
		tci.type = RAL_TEXTURE_2D; tci.format = RAL_FORMAT_R8G8B8A8_UNORM;
		tci.width = 4u; tci.height = 4u; tci.depthOrArrayLayers = 1u;
		tci.mipLevels = 3u; tci.sampleCount = 1u;
		tci.usage = RAL_TEXTURE_USAGE_SAMPLED; tci.memory = RAL_MEMORY_DEVICE_LOCAL;
		tci.debugName = "ral-pipeline-test-residency-texture";
		tex = Ral_CreateTexture( b, &tci );
		if ( tex ) {
			RAL_ZERO( upload ); upload.mipLevel = 0u; upload.data = mip0; upload.dataSize = sizeof( mip0 );
			uploadFence = Ral_TextureUploadAsync( tex, &upload );
			if ( uploadFence ) { Ral_WaitFence( uploadFence, ~0ull ); Ral_DestroyFence( uploadFence ); uploadFence = NULL; }
			RAL_ZERO( upload ); upload.mipLevel = 2u; upload.data = mip2; upload.dataSize = sizeof( mip2 );
			uploadFence = Ral_TextureUploadAsync( tex, &upload );
			if ( uploadFence ) { Ral_WaitFence( uploadFence, ~0ull ); Ral_DestroyFence( uploadFence ); uploadFence = NULL; }
		}
		if ( tex ) {
			RAL_ZERO( vci ); vci.texture = tex; vci.viewType = RAL_TEXTURE_2D; vci.arrayLayerCount = 1u;
			fullView = Ral_CreateTextureView( b, &vci );
			vci.baseMipLevel = 2u; vci.mipLevelCount = 1u;
			coarseView = Ral_CreateTextureView( b, &vci );
		}
		RAL_ZERO( sci ); sci.magFilter = RAL_FILTER_NEAREST; sci.minFilter = RAL_FILTER_NEAREST;
		sci.mipmapMode = RAL_MIPMAP_NEAREST; sci.addressU = sci.addressV = sci.addressW = RAL_ADDRESS_CLAMP_TO_EDGE;
		sci.maxLod = 1.0f; sci.debugName = "ral-pipeline-test-residency-sampler";
		sampler = Ral_CreateSampler( b, &sci );
		RAL_ZERO( bci ); bci.size = sizeof( uint32_t ); bci.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_SRC;
		bci.memory = RAL_MEMORY_DEVICE_LOCAL; bci.debugName = "ral-pipeline-test-residency-output";
		sampleOut = Ral_CreateBuffer( b, &bci );
		RAL_ZERO( bci ); bci.size = sizeof( uint32_t ); bci.usage = RAL_BUFFER_TRANSFER_DST;
		bci.memory = RAL_MEMORY_HOST_COHERENT; bci.debugName = "ral-pipeline-test-residency-readback";
		sampleReadback = Ral_CreateBuffer( b, &bci );

		RAL_ZERO( entries[0] ); entries[0].binding = 0u; entries[0].type = RAL_BIND_SAMPLED_TEXTURE; entries[0].count = 1u; entries[0].stageFlags = RAL_STAGE_COMPUTE;
		RAL_ZERO( entries[1] ); entries[1].binding = 1u; entries[1].type = RAL_BIND_SAMPLER; entries[1].count = 1u; entries[1].stageFlags = RAL_STAGE_COMPUTE;
		RAL_ZERO( entries[2] ); entries[2].binding = 2u; entries[2].type = RAL_BIND_STORAGE_BUFFER; entries[2].count = 1u; entries[2].stageFlags = RAL_STAGE_COMPUTE;
		RAL_ZERO( lci ); lci.entries = entries; lci.numEntries = 3u; lci.debugName = "ral-pipeline-test-residency-layout";
		layout = Ral_CreateBindGroupLayout( b, &lci );
		if ( layout && fullView && sampler && sampleOut ) {
			RAL_ZERO( values[0] ); values[0].binding = 0u; values[0].type = RAL_BIND_SAMPLED_TEXTURE; values[0].textureView = fullView;
			RAL_ZERO( values[1] ); values[1].binding = 1u; values[1].type = RAL_BIND_SAMPLER; values[1].sampler = sampler;
			RAL_ZERO( values[2] ); values[2].binding = 2u; values[2].type = RAL_BIND_STORAGE_BUFFER; values[2].buffer = sampleOut; values[2].bufferRange = sizeof( uint32_t );
			RAL_ZERO( bgci ); bgci.layout = layout; bgci.values = values; bgci.numValues = 3u; bgci.debugName = "ral-pipeline-test-residency-group";
			group = Ral_CreateBindGroup( b, &bgci );
		}
		layouts[0] = layout;
		RAL_ZERO( cci ); cci.computeSpirv = ral_pipeline_test_residency_comp_spv;
		cci.computeSpirvSize = ral_pipeline_test_residency_comp_spv_size;
		cci.bindGroupLayouts = layouts; cci.numBindGroupLayouts = 1u; cci.debugName = "ral-pipeline-test-residency-pipeline";
		pipe = layout ? Ral_CreateComputePipeline( b, &cci ) : NULL;
		if ( group && coarseView && pipe && sampleReadback ) {
			// Exercise the portable transfer-to-graphics handoff with the exact
			// mip/layer range consumed below.  The signal is produced on the
			// graphics queue here so this remains valid on single-family devices;
			// distinct-family ownership is separate platform coverage.
			readySemaphore = Ral_CreateSemaphore( b, RAL_SEMAPHORE_BINARY );
			readyCb = Ral_AcquireCommandBuffer( b, RAL_QUEUE_GRAPHICS );
			if ( readySemaphore && readyCb ) {
				ralCommandBuffer_t *readyCbs[1];
				ralSemaphore_t *signals[1];
				ralSubmitInfo_t readySubmit;
				Ral_BeginCommandBuffer( readyCb );
				Ral_EndCommandBuffer( readyCb );
				readyCbs[0] = readyCb;
				signals[0] = readySemaphore;
				RAL_ZERO( readySubmit );
				readySubmit.commandBuffers = readyCbs;
				readySubmit.numCommandBuffers = 1u;
				readySubmit.signalSemaphores = signals;
				readySubmit.numSignalSemaphores = 1u;
				Ral_Submit( b, RAL_QUEUE_GRAPHICS, &readySubmit );
				RAL_ZERO( acquireTicket );
				acquireTicket.readySemaphore = readySemaphore;
				acquireTicket.texture = tex;
				acquireTicket.baseMipLevel = 2u;
				acquireTicket.mipLevelCount = 1u;
				acquireTicket.baseArrayLayer = 0u;
				acquireTicket.arrayLayerCount = 1u;
				acquireTicket.graphicsAcquireRequired = qtrue;
				acquired = Ral_TextureAcquireBatchToGraphics( b, &acquireTicket, 1u );
			}
			RAL_VK_LOG( acquired ? SEV_INFO : SEV_WARN,
			       "  residency acquire: baseMip=2 mipCount=1 baseLayer=0 layerCount=1 readySemaphore=%u result=%s\n",
			       readySemaphore ? 1u : 0u, acquired ? "ok" : "failed" );
			// The group starts on mip0 red; the portable sparse update must make
			// view-local LOD0 resolve to original mip2 green before dispatch.
			Ral_BindGroupSetTextureViewAt( group, 0u, coarseView );
			cb = Ral_AcquireCommandBuffer( b, RAL_QUEUE_GRAPHICS );
			fence = Ral_CreateFence( b );
			if ( cb && fence ) {
				ralCommandBuffer_t *cbs[1]; ralSubmitInfo_t si; ralBufferCopy_t copy;
				Ral_BeginCommandBuffer( cb );
				Ral_CmdBindPipeline( cb, pipe ); Ral_CmdBindBindGroup( cb, 0u, group ); Ral_CmdDispatch( cb, 1u, 1u, 1u );
				Ral_CmdPipelineBarrier( cb, RAL_BARRIER_COMPUTE_TO_TRANSFER );
				RAL_ZERO( copy ); copy.size = sizeof( uint32_t ); Ral_CmdCopyBuffer( cb, sampleOut, sampleReadback, &copy );
				Ral_EndCommandBuffer( cb ); cbs[0] = cb; RAL_ZERO( si ); si.commandBuffers = cbs; si.numCommandBuffers = 1u; si.signalFence = fence;
				Ral_Submit( b, RAL_QUEUE_GRAPHICS, &si ); Ral_WaitFence( fence, ~0ull );
				{
					const uint32_t *packed = (const uint32_t *)Ral_MapBuffer( sampleReadback );
					if ( packed ) {
						const uint32_t rgba = *packed;
						RAL_VK_LOG( SEV_INFO,
						       "  residency view: baseMip=2 sample RGBA = %u %u %u %u (expect coarse green; mip0 is red)\n",
						       rgba & 255u, ( rgba >> 8u ) & 255u, ( rgba >> 16u ) & 255u, ( rgba >> 24u ) & 255u );
						Ral_UnmapBuffer( sampleReadback );
					} else RAL_VK_LOG( SEV_WARN, "  residency view: readback map failed\n" );
				}
			}
		} else RAL_VK_LOG( SEV_WARN, "  residency view: setup failed (tex=%p full=%p coarse=%p sampler=%p out=%p readback=%p layout=%p group=%p pipe=%p)\n",
		                (void *)tex, (void *)fullView, (void *)coarseView, (void *)sampler, (void *)sampleOut,
		                (void *)sampleReadback, (void *)layout, (void *)group, (void *)pipe );
		if ( fence ) Ral_DestroyFence( fence );
		if ( cb ) Ral_DestroyCommandBuffer( cb );
		if ( readyCb ) Ral_DestroyCommandBuffer( readyCb );
		if ( readySemaphore ) Ral_DestroySemaphore( readySemaphore );
		if ( pipe ) Ral_DestroyPipeline( pipe );
		if ( group ) Ral_DestroyBindGroup( group );
		if ( layout ) Ral_DestroyBindGroupLayout( layout );
		if ( sampleReadback ) Ral_DestroyBuffer( sampleReadback );
		if ( sampleOut ) Ral_DestroyBuffer( sampleOut );
		if ( sampler ) Ral_DestroySampler( sampler );
		if ( coarseView ) Ral_DestroyTextureView( coarseView );
		if ( fullView ) Ral_DestroyTextureView( fullView );
		if ( tex ) Ral_DestroyTexture( tex );
	}

	// ── (5) pipeline cache save/load roundtrip ────────────────────────
	{
		const char *path = "ral_pipeline_cache.bin";
		FILE       *vf;
		long        size = 0;
		Ral_SavePipelineCache( b, path );
		vf = fopen( path, "rb" );
		if ( vf ) {
			fseek( vf, 0, SEEK_END );
			size = ftell( vf );
			fclose( vf );
		}
		RAL_VK_LOG( SEV_INFO, "  pipeline cache: saved '%s' (%ld bytes, %s)\n", path, size, ( size >= 32 ) ? "looks plausible: 32-byte header + driver-specific blob" : "EMPTY/missing" );
		Ral_LoadPipelineCache( b, path );   // exercises seed path; logs SEV_INFO if the cache regenerated
		remove( path );                     // cleanup the test artifact
	}

	// drain final frame so any tail-end deferred destroys land before the test returns
	{
		uint32_t i, liveLayouts = 0;
		for ( i = 0; i < RAL_VK_MAX_FRAMES_IN_FLIGHT + 1u; i++ ) { Ral_BeginFrame( b ); Ral_EndFrame( b ); }
		for ( i = 0; i < b->numLayoutCache; i++ )
			if ( b->layoutCache[i].layout != VK_NULL_HANDLE ) liveLayouts++;
		RAL_VK_LOG( SEV_INFO,
		       "  teardown: %u pending destroys, %u live allocations, %u live layout-cache slots (%u high-water)\n",
		       b->numPendingDestroy, b->numAllocations, liveLayouts, b->numLayoutCache );
	}
	RAL_VK_LOG( SEV_INFO, "===== end RAL pipeline test =====\n" );
}
