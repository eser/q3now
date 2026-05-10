#include "tr_local.h"
#include "vk.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#include "smaa_area_texture.h"
#include "smaa_search_texture.h"
#include "../qcommon/q_feats.h"

#if FEAT_IQM
// Bone UBO size: 128 joints * 3 vec4 rows = 6144 bytes
// Must match IQM_MAX_JOINTS (128) from iqm.h and the shader's boneMats[128*3]
#define IQM_GPU_MAX_JOINTS 128
#define IQM_BONE_UBO_SIZE (IQM_GPU_MAX_JOINTS * 3 * sizeof(vec4_t))
#endif

#if defined (_DEBUG)
#if defined (_WIN32)
#define USE_VK_VALIDATION
#include <windows.h> // for win32 debug callback
#endif
#endif

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;

static VkInstance vk_instance = VK_NULL_HANDLE;
static VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

#ifdef USE_VK_VALIDATION
static VkDebugUtilsMessengerEXT vk_debug_messenger = VK_NULL_HANDLE;
#endif

static int vk_diag_fence_ms, vk_diag_submit_ms, vk_diag_present_ms, vk_diag_acquire_ms, vk_diag_frames;
static int vk_diag_ft_fence_ms; // background fence thread: accumulated vkWaitForFences duration
static int vk_diag_drawcalls, vk_diag_pipebinds, vk_diag_msdf_draws, vk_diag_msdf_binds;
static qboolean vk_diag_msdf_active;

// Per-frame µs timestamps for r_frameSpikeUs profiler (single-threaded, main thread only).
static int64_t vk_frame_t_start;
static int64_t vk_frame_t_after_fence;
static int64_t vk_frame_t_after_acquire;
static int64_t vk_frame_t_after_begincb;
static int64_t vk_frame_t_rec_start;
static int64_t vk_frame_t_rec_end;
static int64_t vk_frame_t_submit_start;
static int64_t vk_frame_t_after_submit;
static int64_t vk_frame_t_present_start;
static int64_t vk_frame_t_after_present;
static qboolean vk_frame_present_done;

//
// Vulkan API functions used by the renderer.
//
static PFN_vkCreateInstance								qvkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;

static PFN_vkCreateDevice								qvkCreateDevice;
static PFN_vkDestroyInstance							qvkDestroyInstance;
static PFN_vkEnumerateDeviceExtensionProperties			qvkEnumerateDeviceExtensionProperties;
static PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
static PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
static PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
static PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties		qvkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR			qvkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR			qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
static PFN_vkCreateDebugUtilsMessengerEXT				qvkCreateDebugUtilsMessengerEXT;
static PFN_vkDestroyDebugUtilsMessengerEXT				qvkDestroyDebugUtilsMessengerEXT;
#endif
static PFN_vkAllocateCommandBuffers						qvkAllocateCommandBuffers;
static PFN_vkAllocateDescriptorSets						qvkAllocateDescriptorSets;
static PFN_vkAllocateMemory								qvkAllocateMemory;
static PFN_vkBeginCommandBuffer							qvkBeginCommandBuffer;
static PFN_vkBindBufferMemory							qvkBindBufferMemory;
static PFN_vkBindImageMemory							qvkBindImageMemory;
static PFN_vkCmdBeginRenderPass							qvkCmdBeginRenderPass;
static PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
static PFN_vkCmdBindIndexBuffer							qvkCmdBindIndexBuffer;
static PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
static PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
static PFN_vkCmdBlitImage								qvkCmdBlitImage;
static PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
static PFN_vkCmdCopyBuffer								qvkCmdCopyBuffer;
static PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
static PFN_vkCmdCopyImage								qvkCmdCopyImage;
static PFN_vkCmdDispatch								qvkCmdDispatch;
static PFN_vkCmdDraw									qvkCmdDraw;
static PFN_vkCmdDrawIndexed								qvkCmdDrawIndexed;
static PFN_vkCmdEndRenderPass							qvkCmdEndRenderPass;
static PFN_vkCmdNextSubpass								qvkCmdNextSubpass;
static PFN_vkCmdPipelineBarrier							qvkCmdPipelineBarrier;
static PFN_vkCmdPushConstants							qvkCmdPushConstants;
static PFN_vkCmdResetQueryPool							qvkCmdResetQueryPool;
static PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
static PFN_vkCmdSetScissor								qvkCmdSetScissor;
static PFN_vkCmdSetViewport								qvkCmdSetViewport;
static PFN_vkCmdWriteTimestamp							qvkCmdWriteTimestamp;
static PFN_vkCreateBuffer								qvkCreateBuffer;
static PFN_vkCreateCommandPool							qvkCreateCommandPool;
static PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
static PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
static PFN_vkCreateFence								qvkCreateFence;
static PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
static PFN_vkCreateComputePipelines						qvkCreateComputePipelines;
static PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
static PFN_vkCreateImage								qvkCreateImage;
static PFN_vkCreateImageView							qvkCreateImageView;
static PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
static PFN_vkCreateQueryPool							qvkCreateQueryPool;
static PFN_vkCreatePipelineCache						qvkCreatePipelineCache;
static PFN_vkCreateRenderPass							qvkCreateRenderPass;
static PFN_vkCreateSampler								qvkCreateSampler;
static PFN_vkCreateSemaphore							qvkCreateSemaphore;
static PFN_vkCreateShaderModule							qvkCreateShaderModule;
static PFN_vkDestroyBuffer								qvkDestroyBuffer;
static PFN_vkDestroyCommandPool							qvkDestroyCommandPool;
static PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
static PFN_vkDestroyDescriptorSetLayout					qvkDestroyDescriptorSetLayout;
static PFN_vkDestroyDevice								qvkDestroyDevice;
static PFN_vkDestroyFence								qvkDestroyFence;
static PFN_vkDestroyFramebuffer							qvkDestroyFramebuffer;
static PFN_vkDestroyImage								qvkDestroyImage;
static PFN_vkDestroyImageView							qvkDestroyImageView;
static PFN_vkDestroyPipeline							qvkDestroyPipeline;
static PFN_vkDestroyPipelineCache						qvkDestroyPipelineCache;
static PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
static PFN_vkDestroyQueryPool							qvkDestroyQueryPool;
static PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
static PFN_vkDestroySampler								qvkDestroySampler;
static PFN_vkDestroySemaphore							qvkDestroySemaphore;
static PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
static PFN_vkDeviceWaitIdle								qvkDeviceWaitIdle;
static PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
static PFN_vkFlushMappedMemoryRanges					qvkFlushMappedMemoryRanges;
static PFN_vkFreeCommandBuffers							qvkFreeCommandBuffers;
static PFN_vkFreeDescriptorSets							qvkFreeDescriptorSets;
static PFN_vkFreeMemory									qvkFreeMemory;
static PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
static PFN_vkGetDeviceQueue								qvkGetDeviceQueue;
static PFN_vkGetImageMemoryRequirements					qvkGetImageMemoryRequirements;
static PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
static PFN_vkGetQueryPoolResults						qvkGetQueryPoolResults;
static PFN_vkInvalidateMappedMemoryRanges				qvkInvalidateMappedMemoryRanges;
static PFN_vkMapMemory									qvkMapMemory;
static PFN_vkQueueSubmit								qvkQueueSubmit;
static PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
static PFN_vkResetCommandBuffer							qvkResetCommandBuffer;
static PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
static PFN_vkResetFences								qvkResetFences;
static PFN_vkUnmapMemory								qvkUnmapMemory;
static PFN_vkUpdateDescriptorSets						qvkUpdateDescriptorSets;
static PFN_vkWaitForFences								qvkWaitForFences;
static PFN_vkAcquireNextImageKHR						qvkAcquireNextImageKHR;
static PFN_vkCreateSwapchainKHR							qvkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR						qvkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR						qvkGetSwapchainImagesKHR;
static PFN_vkQueuePresentKHR							qvkQueuePresentKHR;

static PFN_vkGetBufferMemoryRequirements2KHR			qvkGetBufferMemoryRequirements2KHR;
static PFN_vkGetImageMemoryRequirements2KHR				qvkGetImageMemoryRequirements2KHR;

static PFN_vkDebugMarkerSetObjectNameEXT				qvkDebugMarkerSetObjectNameEXT;

////////////////////////////////////////////////////////////////////////////

// forward declaration
VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index );

static uint32_t find_memory_type( uint32_t memory_type_bits, VkMemoryPropertyFlags properties ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ((memory_type_bits & (1 << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: failed to find matching memory type with requested properties" );
	return ~0U;
}


static uint32_t find_memory_type2( uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ( (memory_type_bits & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
			if ( outprops ) {
				*outprops = memory_properties.memoryTypes[i].propertyFlags;
			}
			return i;
		}
	}

	return ~0U;
}


static const char *pmode_to_str( VkPresentModeKHR mode )
{
	static char buf[32];

	switch ( mode ) {
		case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
		case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
		case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
		case VK_PRESENT_MODE_FIFO_LATEST_READY_EXT: return "FIFO_LATEST_READY";
		default: sprintf( buf, "mode#%x", mode ); return buf;
	};
}


#define CASE_STR(x) case (x): return #x

const char *vk_format_string( VkFormat format )
{
	static char buf[16];

	switch ( format ) {
		// color formats
		CASE_STR( VK_FORMAT_R5G5B5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G5R5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R5G6B5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G6R5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B8G8R8A8_SRGB );
		CASE_STR( VK_FORMAT_R8G8B8A8_SRGB );
		CASE_STR( VK_FORMAT_B8G8R8A8_SNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_SNORM );
		CASE_STR( VK_FORMAT_B8G8R8A8_UNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_UNORM );
		CASE_STR( VK_FORMAT_B4G4R4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R4G4B4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R16G16B16A16_UNORM );
		CASE_STR( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_A2R10G10B10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_B10G11R11_UFLOAT_PACK32 );
		// depth formats
		CASE_STR( VK_FORMAT_D16_UNORM );
		CASE_STR( VK_FORMAT_D16_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_X8_D24_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_D24_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_D32_SFLOAT );
		CASE_STR( VK_FORMAT_D32_SFLOAT_S8_UINT );
	default:
		Com_sprintf( buf, sizeof( buf ), "#%i", format );
		return buf;
	}
}


static const char *vk_result_string( VkResult code ) {
	static char buffer[32];

	switch ( code ) {
		CASE_STR( VK_SUCCESS );
		CASE_STR( VK_NOT_READY );
		CASE_STR( VK_TIMEOUT );
		CASE_STR( VK_EVENT_SET );
		CASE_STR( VK_EVENT_RESET );
		CASE_STR( VK_INCOMPLETE );
		CASE_STR( VK_ERROR_OUT_OF_HOST_MEMORY );
		CASE_STR( VK_ERROR_OUT_OF_DEVICE_MEMORY );
		CASE_STR( VK_ERROR_INITIALIZATION_FAILED );
		CASE_STR( VK_ERROR_DEVICE_LOST );
		CASE_STR( VK_ERROR_MEMORY_MAP_FAILED );
		CASE_STR( VK_ERROR_LAYER_NOT_PRESENT );
		CASE_STR( VK_ERROR_EXTENSION_NOT_PRESENT );
		CASE_STR( VK_ERROR_FEATURE_NOT_PRESENT );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DRIVER );
		CASE_STR( VK_ERROR_TOO_MANY_OBJECTS );
		CASE_STR( VK_ERROR_FORMAT_NOT_SUPPORTED );
		CASE_STR( VK_ERROR_FRAGMENTED_POOL );
		CASE_STR( VK_ERROR_UNKNOWN );
		CASE_STR( VK_ERROR_OUT_OF_POOL_MEMORY );
		CASE_STR( VK_ERROR_INVALID_EXTERNAL_HANDLE );
		CASE_STR( VK_ERROR_FRAGMENTATION );
		CASE_STR( VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS );
		CASE_STR( VK_ERROR_SURFACE_LOST_KHR );
		CASE_STR( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
		CASE_STR( VK_SUBOPTIMAL_KHR );
		CASE_STR( VK_ERROR_OUT_OF_DATE_KHR );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
		CASE_STR( VK_ERROR_VALIDATION_FAILED_EXT );
		CASE_STR( VK_ERROR_INVALID_SHADER_NV );
		CASE_STR( VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT );
		CASE_STR( VK_ERROR_NOT_PERMITTED_EXT );
		CASE_STR( VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT );
		CASE_STR( VK_THREAD_IDLE_KHR );
		CASE_STR( VK_THREAD_DONE_KHR );
		CASE_STR( VK_OPERATION_DEFERRED_KHR );
		CASE_STR( VK_OPERATION_NOT_DEFERRED_KHR );
		CASE_STR( VK_PIPELINE_COMPILE_REQUIRED_EXT );
	default:
		sprintf( buffer, "code %i", code );
		return buffer;
	}
}
#undef CASE_STR

#define VK_CHECK( function_call ) { \
	VkResult res = function_call; \
	if ( res < 0 ) { \
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: %s returned %s", #function_call, vk_result_string( res ) ); \
	} \
}


/*
static VkFlags get_composite_alpha( VkCompositeAlphaFlagsKHR flags )
{
	const VkCompositeAlphaFlagBitsKHR compositeFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for ( int i = 1; i < ARRAY_LEN( compositeFlags ); i++ ) {
		if ( flags & compositeFlags[i] ) {
			return compositeFlags[i];
		}
	}

	return compositeFlags[0];
}
*/


static VkCommandBuffer begin_command_buffer( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkCommandBufferAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;

	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = vk.command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &command_buffer ) );

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( command_buffer, &begin_info ) );

	return command_buffer;
}


static void end_command_buffer( VkCommandBuffer command_buffer, const char *location )
{
#ifdef USE_UPLOAD_QUEUE
	const VkPipelineStageFlags wait_dst_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waits;
#endif
	VkSubmitInfo submit_info;
	VkCommandBuffer cmdbuf[1];

	cmdbuf[0] = command_buffer;

	VK_CHECK( qvkEndCommandBuffer( command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
#ifdef USE_UPLOAD_QUEUE
	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else
#endif
	{
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = cmdbuf;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, VK_NULL_HANDLE ) );

	vk_queue_wait_idle();

	qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, cmdbuf );
}


static void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override ) {
	VkImageMemoryBarrier barrier;
	uint32_t src_stage, dst_stage;

	switch ( old_layout ) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			if ( src_stage_override != 0 )
				src_stage = src_stage_override;
			else
				src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		default:
			ri.Terminate( TERM_CLIENT_DROP, "unsupported old layout %i", old_layout );
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
	}

	switch ( new_layout ) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;
		default:
			ri.Terminate( TERM_CLIENT_DROP, "unsupported new layout %i", new_layout);
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
	}


	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	//barrier.srcAccessMask = src_access_flags;
	//barrier.dstAccessMask = dst_access_flags;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	qvkCmdPipelineBarrier( command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier );
}


// debug markers
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

static void vk_set_object_name( uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType )
{
	if ( qvkDebugMarkerSetObjectNameEXT && obj )
	{
		VkDebugMarkerObjectNameInfoEXT info;
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.pNext = NULL;
		info.objectType = objType;
		info.object = obj;
		info.pObjectName = objName;
		qvkDebugMarkerSetObjectNameEXT( vk.device, &info );
	}
}


static void vk_create_swapchain( VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format, VkSwapchainKHR *swapchain, qboolean verbose ) {
	VkImageViewCreateInfo view;
	VkSurfaceCapabilitiesKHR surface_caps;
	VkExtent2D image_extent;
	uint32_t present_mode_count, i;
	VkPresentModeKHR present_mode;
	VkPresentModeKHR *present_modes;
	uint32_t image_count;
	VkSwapchainCreateInfoKHR desc;
	qboolean mailbox_supported = qfalse;
	qboolean immediate_supported = qfalse;
	qboolean fifo_relaxed_supported = qfalse;
	qboolean fifo_latest_ready_supported = qfalse;
	int v;

	VK_CHECK( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device, surface, &surface_caps ) );

	image_extent = surface_caps.currentExtent;
	if ( image_extent.width == 0xffffffff && image_extent.height == 0xffffffff ) {
		image_extent.width = MIN( surface_caps.maxImageExtent.width, MAX( surface_caps.minImageExtent.width, (uint32_t) glConfig.vidWidth ) );
		image_extent.height = MIN( surface_caps.maxImageExtent.height, MAX( surface_caps.minImageExtent.height, (uint32_t) glConfig.vidHeight ) );
	}

	vk.clearAttachment = qtrue;

	if ( !vk.fboActive ) {
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
		if ( ( surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) {
			vk.clearAttachment = qfalse;
			ri.Log( SEV_WARN, "VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain, \\r_clear might not work\n" );
		}
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required in order to take screenshots.
		if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
			ri.Terminate( TERM_UNRECOVERABLE, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported by the swapchain");
		}
	}

	// determine present mode and swapchain image count
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL));

	present_modes = (VkPresentModeKHR *) ri.Malloc( present_mode_count * sizeof( VkPresentModeKHR ) );
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes));

	if ( verbose ) {
		ri.Log( SEV_INFO, "...presentation modes:" );
	}
	for ( i = 0; i < present_mode_count; i++ ) {
		if ( verbose ) {
			ri.Log( SEV_INFO, " %s", pmode_to_str( present_modes[i] ) );
		}
		if ( present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR )
			mailbox_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR )
			immediate_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR )
			fifo_relaxed_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_LATEST_READY_EXT )
			fifo_latest_ready_supported = qtrue;
	}
	if ( verbose ) {
		ri.Log( SEV_INFO, "\n" );
	}

	ri.Free( present_modes );

	if ( ( v = ri.Cvar_VariableIntegerValue( "r_swapInterval" ) ) != 0 ) {
		if ( v == 2 && mailbox_supported )
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if ( fifo_relaxed_supported )
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		else
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
	} else {
		if ( immediate_supported ) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_IMM, surface_caps.minImageCount );
		} else if ( mailbox_supported ) {
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_MAILBOX, surface_caps.minImageCount );
		} else if ( fifo_latest_ready_supported ) {
			/* macOS/MoltenVK: presents most-recently-completed frame at vblank,
			   avoiding the 16ms FIFO stall while remaining tear-free */
			present_mode = VK_PRESENT_MODE_FIFO_LATEST_READY_EXT;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO_LATEST_READY, surface_caps.minImageCount );
		} else if ( fifo_relaxed_supported ) {
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		} else {
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		}
	}

	if ( image_count < 2 ) {
		image_count = 2;
	}

	if ( surface_caps.maxImageCount == 0 && present_mode == VK_PRESENT_MODE_FIFO_KHR ) {
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO_0, surface_caps.minImageCount );
	} else if ( surface_caps.maxImageCount > 0 ) {
		image_count = MIN( MIN( image_count, surface_caps.maxImageCount ), MAX_SWAPCHAIN_IMAGES );
	}

	if ( verbose ) {
		ri.Log( SEV_INFO, "...selected presentation mode: %s, image count: %i\n", pmode_to_str( present_mode ), image_count );
	}

	// create swap chain
	desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.surface = surface;
	desc.minImageCount = image_count;
	desc.imageFormat = surface_format.format;
	desc.imageColorSpace = surface_format.colorSpace;
	desc.imageExtent = image_extent;
	desc.imageArrayLayers = 1;
	desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( !vk.fboActive ) {
		desc.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.preTransform = surface_caps.currentTransform;
	//desc.compositeAlpha = get_composite_alpha( surface_caps.supportedCompositeAlpha );
	desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	desc.presentMode = present_mode;
	desc.clipped = VK_TRUE;
	desc.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK( qvkCreateSwapchainKHR( device, &desc, NULL, swapchain ) );

	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, NULL ) );
	vk.swapchain_image_count = MIN( vk.swapchain_image_count, MAX_SWAPCHAIN_IMAGES );
	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images ) );

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.pNext = NULL;
		view.flags = 0;
		view.image = vk.swapchain_images[i];
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = vk.present_format.format;
		view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &vk.swapchain_image_views[i] ) );

		SET_OBJECT_NAME( vk.swapchain_images[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.swapchain_image_views[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		VkSemaphoreCreateInfo s;
		s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		s.pNext = NULL;
		s.flags = 0;
		VK_CHECK( qvkCreateSemaphore( vk.device, &s, NULL, &vk.swapchain_rendering_finished[i] ) );
		SET_OBJECT_NAME( vk.swapchain_rendering_finished[i], va( "swapchain_rendering_finished semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
	}

	if ( vk.initSwapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
		VkCommandBuffer command_buffer = begin_command_buffer();

		for ( i = 0; i < vk.swapchain_image_count; i++ ) {
			record_image_layout_transition( command_buffer, vk.swapchain_images[i],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, vk.initSwapchainLayout, 0, 0 );
		}

		end_command_buffer( command_buffer, __func__ );
	}
}


static void vk_create_render_passes( void )
{
	VkAttachmentDescription attachments[3]; // color | depth | msaa color
	VkAttachmentReference colorResolveRef;
	VkAttachmentReference colorRef0;
	VkAttachmentReference depthRef0;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[3];
	VkRenderPassCreateInfo desc;
	VkFormat depth_format;
	VkDevice device;
	uint32_t i;

	depth_format = vk.depth_format;
	device = vk.device;

	if ( r_fbo->integer == 0 )
	{
		// presentation
		attachments[0].flags = 0;
		attachments[0].format = vk.present_format.format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for presentation
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = vk.initSwapchainLayout;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else
	{
		// resolve/color buffer
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

#ifdef USE_BUFFER_CLEAR
		// Always CLEAR the color/resolve target — DONT_CARE leaves
		// garbage (pink) on TBDR GPUs (Apple Silicon via MoltenVK)
		// because MTLLoadActionDontCare truly discards tile contents.
		attachments[ 0 ].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[ 0 ].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif

		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	// depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = vkSamples;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	if ( r_bloom->integer || vk.depthFade.active ) {
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // keep it for post-bloom/depth-fade pass
		attachments[1].stencilStoreOp = glConfig.stencilBits ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
	} else {
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;

	desc.subpassCount = 1;
	desc.attachmentCount = 2;

	if ( vk.msaaActive )
	{
		attachments[2].flags = 0;
		attachments[2].format = vk.color_format;
		attachments[2].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		if ( r_bloom->integer ) {
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // keep it for post-bloom pass
		} else {
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Intermediate storage (not written)
		}
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // msaa image attachment
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorResolveRef.attachment = 0; // resolve image attachment
		colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pResolveAttachments = &colorResolveRef;
	}

	// subpass dependencies

	memset( &deps, 0, sizeof( deps ) );

	deps[2].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[2].dstSubpass = 0;
	deps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[2].dependencyFlags = 0;

	if ( r_fbo->integer == 0 )
	{
		desc.dependencyCount = 1;
		desc.pDependencies = &deps[2];

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
		SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		return;
	}

	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// What pipeline stage must have completed for the dependency
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;						// What access scopes are influence the dependency
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // What access scopes are waiting on the dependency
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// Fragment data has been written
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// Don't start shading until data is available
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			// Waiting for color data to be written
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;						// Don't read things from the shader before ready
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
	SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

#if FEAT_FBO_DEBUG
	ri.Log( SEV_INFO, "^3[FBO_DEBUG] Main render pass created:\n" );
	ri.Log( SEV_INFO, "^3[FBO_DEBUG]   color format=%d  loadOp=%d  initialLayout=%d  finalLayout=%d\n",
		attachments[0].format, attachments[0].loadOp, attachments[0].initialLayout, attachments[0].finalLayout );
	ri.Log( SEV_INFO, "^3[FBO_DEBUG]   depth format=%d  loadOp=%d\n",
		attachments[1].format, attachments[1].loadOp );
	ri.Log( SEV_INFO, "^3[FBO_DEBUG]   msaaActive=%d  r_fbo=%d\n", vk.msaaActive, r_fbo->integer );
#endif

	// depth fade pass: loads color+depth from main pass, renders soft transparents
	if ( vk.depthFade.active ) {
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = glConfig.stencilBits ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if ( vk.msaaActive ) {
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.depth_fade ) );
		SET_OBJECT_NAME( vk.render_pass.depth_fade, "render pass - depth_fade", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// restore main pass settings for subsequent render pass creation
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		if ( r_bloom->integer || vk.depthFade.active ) {
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].stencilStoreOp = glConfig.stencilBits ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		} else {
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		if ( vk.msaaActive ) {
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
	}

	if ( r_bloom->integer ) {

		// post-bloom pass
		// color buffer
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // load from previous pass
		 // depth buffer
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if ( vk.msaaActive ) {
			// msaa render target
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.post_bloom ) );
		SET_OBJECT_NAME( vk.render_pass.post_bloom, "render pass - post_bloom", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// bloom extraction, using resolved/main fbo as a source
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.bloom_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;		// DONT_CARE leaves pink on TBDR
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.bloom_extract ) );
		SET_OBJECT_NAME( vk.render_pass.bloom_extract, "render pass - bloom_extract", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ )
		{
			VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.blur[i] ) );
			SET_OBJECT_NAME( vk.render_pass.blur[i], va( "render pass - blur %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		}
	}

	// SMAA render passes
	if ( vk.smaa.active )
	{
		desc.attachmentCount = 1;
		desc.dependencyCount = 2;
		desc.pDependencies = &deps[0];

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		// SMAA edge detection pass: R8G8_UNORM, clear
		attachments[0].flags = 0;
		attachments[0].format = VK_FORMAT_R8G8_UNORM;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.smaa_edge ) );
		SET_OBJECT_NAME( vk.render_pass.smaa_edge, "render pass - smaa_edge", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// SMAA blend weight pass: R8G8B8A8_UNORM, clear
		attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.smaa_blend ) );
		SET_OBJECT_NAME( vk.render_pass.smaa_blend, "render pass - smaa_blend", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// SMAA resolve pass: color_format, clear to black (fullscreen triangle covers all, but safety)
		attachments[0].format = vk.color_format;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.smaa_resolve ) );
		SET_OBJECT_NAME( vk.render_pass.smaa_resolve, "render pass - smaa_resolve", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	// capture render pass
	if ( vk.capture.image )
	{
		memset( &subpass, 0, sizeof( subpass ) );

		attachments[0].flags = 0;
		attachments[0].format = vk.capture_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // DONT_CARE leaves pink on TBDR GPUs
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.capture ) );
		SET_OBJECT_NAME( vk.render_pass.capture, "render pass - capture", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	desc.attachmentCount = 1;

	memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;

	// gamma post-processing
	attachments[0].flags = 0;
	attachments[0].format = vk.present_format.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // needed for presentation
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = vk.initSwapchainLayout;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	desc.dependencyCount = 1;
	desc.pDependencies = &deps[2];

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.gamma ) );
	SET_OBJECT_NAME( vk.render_pass.gamma, "render pass - gamma", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// screenmap
	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	// screenmap resolve/color buffer
	attachments[0].flags = 0;
	attachments[0].format = vk.color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
	// Always CLEAR — DONT_CARE leaves pink on TBDR GPUs (Apple Silicon)
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// screenmap depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = vk.screenMapSamples;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;
	desc.subpassCount = 1;
	desc.attachmentCount = 2;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {

		attachments[2].flags = 0;
		attachments[2].format = vk.color_format;
		attachments[2].samples = vk.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // screenmap msaa image attachment
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorResolveRef.attachment = 0; // screenmap resolve image attachment
		colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pResolveAttachments = &colorResolveRef;
	}

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.screenmap ) );

	SET_OBJECT_NAME( vk.render_pass.screenmap, "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
}


static void allocate_and_bind_image_memory(VkImage image) {
	VkMemoryRequirements memory_requirements;
	VkDeviceSize alignment;
	ImageChunk *chunk;

	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);

	if ( memory_requirements.size > vk.image_chunk_size ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: could not allocate memory, image is too large (%ikbytes).",
			(int)(memory_requirements.size/1024) );
	}

	chunk = NULL;

	// Try to find an existing chunk of sufficient capacity.
	alignment = memory_requirements.alignment;
	for ( int i = 0; i < vk_world.num_image_chunks; i++ ) {
		// ensure that memory region has proper alignment
		VkDeviceSize offset = PAD( vk_world.image_chunks[i].used, alignment );

		if ( offset + memory_requirements.size <= vk.image_chunk_size ) {
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == NULL) {
		VkMemoryAllocateInfo alloc_info;
		VkDeviceMemory memory;

		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS) {
			ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: image chunk limit has been reached" );
		}

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = vk.image_chunk_size;
		alloc_info.memoryTypeIndex = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;

		SET_OBJECT_NAME( memory, va( "image memory chunk %i", vk_world.num_image_chunks ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

		vk_world.num_image_chunks++;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}


static void vk_clean_staging_buffer( void )
{
	if ( vk.staging_buffer.handle != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.staging_buffer.handle, NULL );
		vk.staging_buffer.handle = VK_NULL_HANDLE;
	}

	//if ( vk.staging_buffer.ptr != NULL )
	//	qvkUnmapMemory( vk.device, vk.staging_buffer.memory ) {
	//	vk.staging_buffer.ptr = NULL;
	//}

	if ( vk.staging_buffer.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.staging_buffer.memory, NULL );
		vk.staging_buffer.memory = VK_NULL_HANDLE;
	}

	vk.staging_buffer.ptr = NULL;
	vk.staging_buffer.size = 0;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
}


#ifdef USE_UPLOAD_QUEUE
static qboolean vk_wait_staging_buffer( void )
{
	if ( vk.aux_fence_wait ) {
		VkResult res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Terminate( TERM_UNRECOVERABLE, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __func__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
		vk.staging_buffer.offset = 0; // FIXME: is this correct?
		vk.aux_fence_wait = qfalse;
		return qtrue;
	}
	return qfalse;
}


static void vk_flush_staging_buffer( qboolean final )
{
	const VkPipelineStageFlags wait_dst_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waits;
	VkSubmitInfo submit_info;
	VkResult res;

	if ( vk.staging_buffer.offset == 0 ) {
		return;
	}

	//ri.Log( SEV_WARN, S_COLOR_CYAN ">>> flush %i bytes (final=%i)<<<\n", (int)vk_world.staging_buffer_offset, final );

	vk.staging_buffer.offset = 0;

	VK_CHECK( qvkEndCommandBuffer( vk.staging_command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;

	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		// first call after previous queue submission?
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.staging_command_buffer;

	if ( vk.image_uploaded != VK_NULL_HANDLE ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: incorrect state during image upload" );
	}
	if ( final ) {
		// final submission before recording
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.image_uploaded2;
		vk.image_uploaded = vk.image_uploaded2;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.aux_fence ) );
		vk.aux_fence_wait = qtrue;
	} else {
		// if submission before another upload then do explicit wait
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.aux_fence ) );
		res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Terminate( TERM_UNRECOVERABLE, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __func__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
	}
}
#endif // USE_UPLOAD_QUEUE


static void vk_alloc_staging_buffer( VkDeviceSize size )
{
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	uint32_t memory_type;
	void *data;

	vk_clean_staging_buffer();

	vk.staging_buffer.size = MAX( size, STAGING_BUFFER_SIZE );
	vk.staging_buffer.size = PAD( vk.staging_buffer.size, 1024 * 1024 );

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = vk.staging_buffer.size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &vk.staging_buffer.handle));

	qvkGetBufferMemoryRequirements( vk.device, vk.staging_buffer.handle, &memory_requirements );

	memory_type = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.staging_buffer.memory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk.staging_buffer.handle, vk.staging_buffer.memory, 0));

	VK_CHECK(qvkMapMemory(vk.device, vk.staging_buffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk.staging_buffer.ptr = (byte*)data;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
	SET_OBJECT_NAME( vk.staging_buffer.handle, "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.staging_buffer.memory, "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


#ifdef USE_VK_VALIDATION
/* Routes validation-layer messages to the engine log (cat=renderer) instead
 * of spawning a modal MessageBox per error.  ri.Log → Com_Logv internally,
 * so output lands in qconsole.jsonl with the renderer category. */
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT       severity_bits,
	VkDebugUtilsMessageTypeFlagsEXT              type_flags,
	const VkDebugUtilsMessengerCallbackDataEXT  *data,
	void                                        *user_data )
{
	log_severity_t sev;
	const char *msg;

	(void)type_flags;
	(void)user_data;

	if ( !data || !data->pMessage )
		return VK_FALSE;
	msg = data->pMessage;

	if ( severity_bits & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT )
		sev = SEV_ERROR;
	else if ( severity_bits & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT )
		sev = SEV_WARN;
	else if ( severity_bits & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT )
		sev = SEV_INFO;
	else
		sev = SEV_DEBUG;

	ri.Log( sev, "Vulkan: %s\n", msg );
	return VK_FALSE;
}
#endif


static qboolean used_instance_extension( const char *ext )
{
	const char *u;

	// allow all VK_*_surface extensions
	u = strrchr( ext, '_' );
	if ( u && Q_stricmp( u + 1, "surface" ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_DISPLAY_EXTENSION_NAME ) == 0 )
		return qtrue; // needed for KMSDRM instances/devices?

	if ( Q_stricmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 )
		return qtrue;

	return qfalse;
}


static void create_instance( void )
{
#ifdef USE_VK_VALIDATION
	const char* validation_layer_name = "VK_LAYER_LUNARG_standard_validation";
	const char* validation_layer_name2 = "VK_LAYER_KHRONOS_validation";
#endif
	VkInstanceCreateInfo desc;
	VkInstanceCreateFlags flags;
	VkExtensionProperties *extension_properties;
	VkResult res;
	const char **extension_names;
	uint32_t i, n, count, extension_count;
	VkApplicationInfo appInfo;

	flags = 0;
	count = 0;
	extension_count = 0;
	VK_CHECK(qvkEnumerateInstanceExtensionProperties(NULL, &count, NULL));

	extension_properties = (VkExtensionProperties *)ri.Malloc(sizeof(VkExtensionProperties) * count);
	extension_names = (const char**)ri.Malloc(sizeof(char *) * count);

	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &count, extension_properties ) );
	for ( i = 0; i < count; i++ ) {
		const char *ext = extension_properties[i].extensionName;

		if ( !used_instance_extension( ext ) ) {
			continue;
		}

		// search for duplicates
		for ( n = 0; n < extension_count; n++ ) {
			if ( Q_stricmp( ext, extension_names[ n ] ) == 0 ) {
				break;
			}
		}
		if ( n != extension_count ) {
			continue;
		}

		extension_names[ extension_count++ ] = ext;

		if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 ) {
			flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}
	}

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = NULL; // WIRED_ENGINE_VERSION;
	appInfo.applicationVersion = 0x0;
	appInfo.pEngineName = NULL;
	appInfo.engineVersion = 0x0;
#ifdef _DEBUG
	appInfo.apiVersion = VK_API_VERSION_1_1;
#else
	appInfo.apiVersion = VK_API_VERSION_1_0;
#endif

	// create instance
	desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = flags;
	desc.pApplicationInfo = &appInfo;
	desc.enabledExtensionCount = extension_count;
	desc.ppEnabledExtensionNames = extension_names;

#ifdef USE_VK_VALIDATION
	desc.enabledLayerCount = 1;
	desc.ppEnabledLayerNames = &validation_layer_name;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );

	if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

		desc.enabledLayerCount = 1;
		desc.ppEnabledLayerNames = &validation_layer_name2;

		res = qvkCreateInstance( &desc, NULL, &vk_instance );

		if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

			ri.Log( SEV_WARN, "...validation layer is not available\n" );

			// try without validation layer
			desc.enabledLayerCount = 0;
			desc.ppEnabledLayerNames = NULL;

			res = qvkCreateInstance( &desc, NULL, &vk_instance );
		}
	}
#else
	desc.enabledLayerCount = 0;
	desc.ppEnabledLayerNames = NULL;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );
#endif

	ri.Free( (void*)extension_names );
	ri.Free( extension_properties );

	if ( res != VK_SUCCESS ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: instance creation failed with %s", vk_result_string( res ) );
	}
}


static VkFormat get_depth_format( VkPhysicalDevice physical_device ) {
	VkFormatProperties props;
	VkFormat formats[2];

	if ( glConfig.stencilBits > 0 ) {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
	} else {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
	}

	for ( int i = 0; i < ARRAY_LEN( formats ); i++ ) {
		qvkGetPhysicalDeviceFormatProperties( physical_device, formats[i], &props );
		if ( ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0 ) {
			return formats[i];
		}
	}

	ri.Terminate( TERM_UNRECOVERABLE, "get_depth_format: failed to find depth attachment format" );
	return VK_FORMAT_UNDEFINED; // never get here
}


// Check if we can use vkCmdBlitImage for the given source and destination image formats.
static qboolean vk_blit_enabled( VkPhysicalDevice physical_device, const VkFormat srcFormat, const VkFormat dstFormat )
{
	VkFormatProperties formatProps;

	qvkGetPhysicalDeviceFormatProperties( physical_device, srcFormat, &formatProps );
	if ( ( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == 0 ) {
		return qfalse;
	}

	qvkGetPhysicalDeviceFormatProperties( physical_device, dstFormat, &formatProps );
	if ( ( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == 0 ) {
		return qfalse;
	}

	return qtrue;
}


static VkFormat get_hdr_format( VkFormat base_format )
{
	if ( r_fbo->integer == 0 ) {
		return base_format;
	}

	switch ( r_hdr->integer ) {
		case -1: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
		case 1: return VK_FORMAT_R16G16B16A16_UNORM;
		default: return base_format;
	}
}

typedef struct {
	int bits;
	VkFormat rgb;
	VkFormat bgr;
} present_format_t;

static const present_format_t present_formats[] = {
	//{12, VK_FORMAT_B4G4R4A4_UNORM_PACK16, VK_FORMAT_R4G4B4A4_UNORM_PACK16},
	//{15, VK_FORMAT_B5G5R5A1_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16},
	{16, VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16},
	{24, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM},
	{30, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32},
	//{32, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32}
};

static void get_present_format( int present_bits, VkFormat *bgr, VkFormat *rgb ) {
	const present_format_t *pf, *sel;

	sel = NULL;
	pf = present_formats;
	for ( int i = 0; i < ARRAY_LEN( present_formats ); i++, pf++ ) {
		if ( pf->bits <= present_bits  ) {
			sel = pf;
		}
	}
	if ( !sel ) {
		*bgr = VK_FORMAT_B8G8R8A8_UNORM;
		*rgb = VK_FORMAT_R8G8B8A8_UNORM;
	} else {
		*bgr = sel->bgr;
		*rgb = sel->rgb;
	}
}


static qboolean vk_select_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface )
{
	VkFormat base_bgr, base_rgb;
	VkFormat ext_bgr, ext_rgb;
	VkSurfaceFormatKHR *candidates;
	uint32_t format_count;
	VkResult res;

	res = qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, NULL );
	if ( res < 0 ) {
		ri.Log( SEV_ERROR, "vkGetPhysicalDeviceSurfaceFormatsKHR returned %s\n", vk_result_string( res ) );
		return qfalse;
	}

	if ( format_count == 0 ) {
		ri.Log( SEV_ERROR, "...no surface formats found\n" );
		return qfalse;
	}

	candidates = (VkSurfaceFormatKHR*)ri.Malloc( format_count * sizeof(VkSurfaceFormatKHR) );

	VK_CHECK( qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, candidates ) );

	get_present_format( 24, &base_bgr, &base_rgb );

	if ( r_fbo->integer ) {
		get_present_format( r_presentBits->integer, &ext_bgr, &ext_rgb );
	} else {
		ext_bgr = base_bgr;
		ext_rgb = base_rgb;
	}

	if ( format_count == 1 && candidates[0].format == VK_FORMAT_UNDEFINED ) {
		// special case that means we can choose any format
		vk.base_format.format = base_bgr;
		vk.base_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		vk.present_format.format = ext_bgr;
		vk.present_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	}
	else {
		uint32_t i;
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == base_bgr || candidates[i].format == base_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.base_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.base_format = candidates[0];
		}
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == ext_bgr || candidates[i].format == ext_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.present_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.present_format = vk.base_format;
		}
	}

	if ( !r_fbo->integer ) {
		vk.present_format = vk.base_format;
	}

	ri.Free( candidates );

	return qtrue;
}


static void setup_surface_formats( VkPhysicalDevice physical_device )
{
	vk.depth_format = get_depth_format( physical_device );

	vk.color_format = get_hdr_format( vk.base_format.format );

	vk.capture_format = VK_FORMAT_R8G8B8A8_UNORM;

	vk.bloom_format = vk.base_format.format;

	vk.blitEnabled = vk_blit_enabled( physical_device, vk.color_format, vk.capture_format );

	if ( !vk.blitEnabled )
	{
		vk.capture_format = vk.color_format;
	}

#if FEAT_FBO_DEBUG
	ri.Log( SEV_INFO, "^3[FBO_DEBUG] Format selection:\n" );
	ri.Log( SEV_INFO, "^3[FBO_DEBUG]   r_fbo=%d r_hdr=%d r_presentBits=%d\n", r_fbo->integer, r_hdr->integer, r_presentBits->integer );
	ri.Log( SEV_INFO, "^3[FBO_DEBUG]   base_format=%d  color_format=%d  present_format=%d\n", vk.base_format.format, vk.color_format, vk.present_format.format );
	ri.Log( SEV_INFO, "^3[FBO_DEBUG]   bloom_format=%d  capture_format=%d  depth_format=%d\n", vk.bloom_format, vk.capture_format, vk.depth_format );
	ri.Log( SEV_INFO, "^3[FBO_DEBUG]   blitEnabled=%d\n", vk.blitEnabled );
#endif
}


static const char *renderer_name( const VkPhysicalDeviceProperties *props ) {
	static char buf[sizeof( props->deviceName ) + 64];
	const char *device_type;

	switch ( props->deviceType ) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "Virtual"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
		default: device_type = "OTHER"; break;
	}

	Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x",
		device_type, props->deviceName, props->deviceID );

	return buf;
}


static qboolean vk_create_device( VkPhysicalDevice physical_device, int device_index ) {

#ifdef _DEBUG
	VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore;
	VkPhysicalDeviceVulkanMemoryModelFeatures memory_model;
	VkPhysicalDeviceBufferDeviceAddressFeatures devaddr_features;
	VkPhysicalDevice8BitStorageFeatures storage_8bit_features;
#endif

	ri.Log( SEV_INFO, "...selected physical device: %i\n", device_index );

	// select surface format
	if ( !vk_select_surface_format( physical_device, vk_surface ) ) {
		return qfalse;
	}

	setup_surface_formats( physical_device );

	// select queue family
	{
		VkQueueFamilyProperties *queue_families;
		uint32_t queue_family_count;
		uint32_t i;

		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, NULL );
		queue_families = (VkQueueFamilyProperties*)ri.Malloc( queue_family_count * sizeof( VkQueueFamilyProperties ) );
		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, queue_families );

		// select queue family with presentation and graphics support
		vk.queue_family_index = ~0U;
		for (i = 0; i < queue_family_count; i++) {
			VkBool32 presentation_supported;
			VK_CHECK( qvkGetPhysicalDeviceSurfaceSupportKHR( physical_device, i, vk_surface, &presentation_supported ) );

			if (presentation_supported && (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
				vk.queue_family_index = i;
				break;
			}
		}

		ri.Free( queue_families );

		if ( vk.queue_family_index == ~0U ) {
			ri.Log( SEV_ERROR, "...failed to find graphics queue family\n" );

			return qfalse;
		}
	}

	// create VkDevice
	{
		const char *device_extension_list[8];
		uint32_t device_extension_count;
		const char *ext, *end;
		char *str;
		const float priority = 1.0;
		VkExtensionProperties *extension_properties;
		VkDeviceQueueCreateInfo queue_desc;
		VkPhysicalDeviceFeatures device_features;
		VkPhysicalDeviceFeatures features;
		VkDeviceCreateInfo device_desc;
		VkResult res;
		qboolean swapchainSupported = qfalse;
		qboolean dedicatedAllocation = qfalse;
		qboolean memoryRequirements2 = qfalse;
		qboolean debugMarker = qfalse;
#ifdef _DEBUG
		qboolean timelineSemaphore = qfalse;
		qboolean memoryModel = qfalse;
		qboolean devAddrFeat = qfalse;
		qboolean storage8bit = qfalse;
		const void** pNextPtr;
#endif
		uint32_t i, len, count = 0;

		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, NULL ) );
		extension_properties = (VkExtensionProperties*)ri.Malloc( count * sizeof( VkExtensionProperties ) );
		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, extension_properties ) );

		// fill glConfig.extensions_string
		str = glConfig.extensions_string; *str = '\0';
		end = &glConfig.extensions_string[ sizeof( glConfig.extensions_string ) - 1];

		for ( i = 0; i < count; i++ ) {
			ext = extension_properties[i].extensionName;
			if ( strcmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 ) {
				swapchainSupported = qtrue;
			} else if ( strcmp( ext, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME ) == 0 ) {
				dedicatedAllocation = qtrue;
			} else if ( strcmp( ext, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME ) == 0 ) {
				memoryRequirements2 = qtrue;
			} else if ( strcmp( ext, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
				debugMarker = qtrue;
#ifdef _DEBUG
			} else if ( strcmp( ext, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME ) == 0 ) {
				timelineSemaphore = qtrue;
			} else if ( strcmp( ext, VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME ) == 0 ) {
				memoryModel = qtrue;
			} else if ( strcmp( ext, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ) == 0 ) {
				devAddrFeat = qtrue;
			} else if ( strcmp( ext, VK_KHR_8BIT_STORAGE_EXTENSION_NAME ) == 0 ) {
				storage8bit = qtrue;
#endif
			}
			// add this device extension to glConfig
			if ( i != 0 ) {
				if ( str + 1 >= end )
					continue;
				str = Q_stradd( str, " " );
			}
			len = (uint32_t)strlen( ext );
			if ( str + len >= end )
				continue;
			str = Q_stradd( str, ext );
		}

		ri.Free( extension_properties );

		device_extension_count = 0;

		if ( !swapchainSupported ) {
			ri.Log( SEV_ERROR, "...required device extension is not available: %s\n", VK_KHR_SWAPCHAIN_EXTENSION_NAME );
			return qfalse;
		}

		if ( !memoryRequirements2 )
			dedicatedAllocation = qfalse;
		else
			vk.dedicatedAllocation = dedicatedAllocation;

#ifndef USE_DEDICATED_ALLOCATION
		vk.dedicatedAllocation = qfalse;
#endif

		device_extension_list[ device_extension_count++ ] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

		if ( vk.dedicatedAllocation ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
		}

		if ( debugMarker ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
			vk.debugMarkers = qtrue;
		}
#ifdef _DEBUG
		if ( timelineSemaphore ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
		}

		if ( memoryModel ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME;
		}

		if ( devAddrFeat ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
		}

		if ( storage8bit ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_8BIT_STORAGE_EXTENSION_NAME;
		}
#endif // _DEBUG
		qvkGetPhysicalDeviceFeatures( physical_device, &device_features );

		if ( device_features.fillModeNonSolid == VK_FALSE ) {
			ri.Log( SEV_ERROR, "...fillModeNonSolid feature is not supported\n" );
			return qfalse;
		}

		queue_desc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_desc.pNext = NULL;
		queue_desc.flags = 0;
		queue_desc.queueFamilyIndex = vk.queue_family_index;
		queue_desc.queueCount = 1;
		queue_desc.pQueuePriorities = &priority;

		memset( &features, 0, sizeof( features ) );
		features.fillModeNonSolid = VK_TRUE;

#ifdef _DEBUG
		if ( device_features.shaderInt64 ) {
			features.shaderInt64 = VK_TRUE;
		}
#endif
		if ( device_features.wideLines ) { // needed for RB_SurfaceAxis
			features.wideLines = VK_TRUE;
			vk.wideLines = qtrue;
		}

		if ( device_features.fragmentStoresAndAtomics && device_features.vertexPipelineStoresAndAtomics ) {
			features.vertexPipelineStoresAndAtomics = VK_TRUE;
			features.fragmentStoresAndAtomics = VK_TRUE;
			vk.fragmentStores = qtrue;
		}

		if ( r_ext_texture_filter_anisotropic->integer && device_features.samplerAnisotropy ) {
			features.samplerAnisotropy = VK_TRUE;
			vk.samplerAnisotropy = qtrue;
		}

		device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_desc.pNext = NULL;
		device_desc.flags = 0;
		device_desc.queueCreateInfoCount = 1;
		device_desc.pQueueCreateInfos = &queue_desc;
		device_desc.enabledLayerCount = 0;
		device_desc.ppEnabledLayerNames = NULL;
		device_desc.enabledExtensionCount = device_extension_count;
		device_desc.ppEnabledExtensionNames = device_extension_list;
		device_desc.pEnabledFeatures = &features;

#ifdef _DEBUG
		pNextPtr = (const void **)&device_desc.pNext;

		if ( timelineSemaphore ) {
			*pNextPtr = &timeline_semaphore;
			timeline_semaphore.pNext = NULL;
			timeline_semaphore.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
			timeline_semaphore.timelineSemaphore = VK_TRUE;
			pNextPtr = (const void **)&timeline_semaphore.pNext;
		}

		if ( memoryModel ) {
			*pNextPtr = &memory_model;
			memory_model.pNext = NULL;
			memory_model.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
			memory_model.vulkanMemoryModel = VK_TRUE;
			memory_model.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
			memory_model.vulkanMemoryModelDeviceScope = VK_TRUE;
			pNextPtr = (const void **)&memory_model.pNext;
		}

		if ( devAddrFeat ) {
			*pNextPtr = &devaddr_features;
			devaddr_features.pNext = NULL;
			devaddr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
			devaddr_features.bufferDeviceAddress = VK_TRUE;
			devaddr_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
			devaddr_features.bufferDeviceAddressMultiDevice = VK_FALSE;
			pNextPtr = (const void **)&devaddr_features.pNext;
		}

		if ( storage8bit ) {
			*pNextPtr = &storage_8bit_features;
			storage_8bit_features.pNext = NULL;
			storage_8bit_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
			storage_8bit_features.storageBuffer8BitAccess = VK_TRUE;
			storage_8bit_features.storagePushConstant8 = VK_FALSE;
			storage_8bit_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
			pNextPtr = (const void **)&storage_8bit_features.pNext;
		}
#endif
		res = qvkCreateDevice( physical_device, &device_desc, NULL, &vk.device );
		if ( res < 0 ) {
			ri.Log( SEV_ERROR, "vkCreateDevice returned %s\n", vk_result_string( res ) );
			return qfalse;
		}
	}

	return qtrue;
}


#define INIT_INSTANCE_FUNCTION(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func); \
	if (q##func == NULL) {											\
		ri.Terminate( TERM_UNRECOVERABLE, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func);


#define INIT_DEVICE_FUNCTION(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);\
	if (q##func == NULL) {											\
		ri.Terminate( TERM_UNRECOVERABLE, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_DEVICE_FUNCTION_EXT(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);


static void vk_destroy_instance( void ) {
	if ( vk_surface != VK_NULL_HANDLE ) {
		if ( qvkDestroySurfaceKHR != NULL ) {
			qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		}
		vk_surface = VK_NULL_HANDLE;
	}

#ifdef USE_VK_VALIDATION
	if ( vk_debug_messenger != VK_NULL_HANDLE ) {
		if ( qvkDestroyDebugUtilsMessengerEXT != NULL ) {
			qvkDestroyDebugUtilsMessengerEXT( vk_instance, vk_debug_messenger, NULL );
		}
		vk_debug_messenger = VK_NULL_HANDLE;
	}
#endif

	if ( vk_instance != VK_NULL_HANDLE ) {
		if ( qvkDestroyInstance ) {
			qvkDestroyInstance( vk_instance, NULL );
		}
		vk_instance = VK_NULL_HANDLE;
	}
}


static void init_vulkan_library( void )
{
	VkPhysicalDeviceProperties props;
	VkPhysicalDevice *physical_devices;
	uint32_t device_count;
	int device_index, i;
	VkResult res;

	memset( &vk, 0, sizeof( vk ) );

	if ( vk_instance == VK_NULL_HANDLE ) {

		// force cleanup
		vk_destroy_instance();

		// Get functions that do not depend on VkInstance (vk_instance == nullptr at this point).
		INIT_INSTANCE_FUNCTION( vkCreateInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateInstanceExtensionProperties )

		// Get instance level functions.
		create_instance();

		INIT_INSTANCE_FUNCTION( vkCreateDevice )
		INIT_INSTANCE_FUNCTION( vkDestroyInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
		INIT_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
		INIT_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFormatProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )
		INIT_INSTANCE_FUNCTION( vkDestroySurfaceKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceFormatsKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfacePresentModesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceSupportKHR )

#ifdef USE_VK_VALIDATION
		INIT_INSTANCE_FUNCTION_EXT( vkCreateDebugUtilsMessengerEXT )
		INIT_INSTANCE_FUNCTION_EXT( vkDestroyDebugUtilsMessengerEXT )

		/* Wire validation-layer output into the engine log.  Skipped silently
		 * when the loader didn't expose VK_EXT_debug_utils — same behavior as
		 * before for drivers that lack the extension. */
		if ( qvkCreateDebugUtilsMessengerEXT && qvkDestroyDebugUtilsMessengerEXT ) {
			VkDebugUtilsMessengerCreateInfoEXT desc;
			desc.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			desc.pNext           = NULL;
			desc.flags           = 0;
			desc.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
			                     | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			desc.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
			                     | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
			                     | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			desc.pfnUserCallback = debug_utils_callback;
			desc.pUserData       = NULL;

			VK_CHECK( qvkCreateDebugUtilsMessengerEXT( vk_instance, &desc, NULL, &vk_debug_messenger ) );
		}
#endif

		// create surface
		if ( !ri.VK_CreateSurface( vk_instance, &vk_surface ) ) {
			ri.Terminate( TERM_UNRECOVERABLE, "Error creating Vulkan surface" );
			return;
		}
	} // vk_instance == VK_NULL_HANDLE

	res = qvkEnumeratePhysicalDevices( vk_instance, &device_count, NULL );
	if ( device_count == 0 ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: no physical devices found" );
		return;
	}
	if ( res < 0 ) {
		ri.Terminate( TERM_UNRECOVERABLE, "vkEnumeratePhysicalDevices returned %s", vk_result_string( res ) );
		return;
	}

	physical_devices = (VkPhysicalDevice*)ri.Malloc( device_count * sizeof( VkPhysicalDevice ) );
	VK_CHECK( qvkEnumeratePhysicalDevices( vk_instance, &device_count, physical_devices ) );

	// initial physical device index
	device_index = r_device->integer;

	ri.Log( SEV_INFO, ".......................\nAvailable physical devices:\n" );
	for ( i = 0; i < device_count; i++ ) {
		qvkGetPhysicalDeviceProperties( physical_devices[ i ], &props );
		ri.Log( SEV_INFO, " %i: %s\n", i, renderer_name( &props ) );
		if ( device_index == -1 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			device_index = i;
		} else if ( device_index == -2 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
			device_index = i;
		}
	}
	ri.Log( SEV_INFO, ".......................\n" );

	vk.physical_device = VK_NULL_HANDLE;
	for ( i = 0; i < device_count; i++, device_index++ ) {
		if ( device_index >= device_count || device_index < 0 ) {
			device_index = 0;
		}
		if ( vk_create_device( physical_devices[ device_index ], device_index ) ) {
			vk.physical_device = physical_devices[ device_index ];
			break;
		}
	}

	ri.Free( physical_devices );

	if ( vk.physical_device == VK_NULL_HANDLE ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: unable to find any suitable physical device" );
		return;
	}

	//
	// Get device level functions.
	//
	INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
	INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkAllocateMemory)
	INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
	INIT_DEVICE_FUNCTION(vkBindBufferMemory)
	INIT_DEVICE_FUNCTION(vkBindImageMemory)
	INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
	INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
	INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
	INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
	INIT_DEVICE_FUNCTION(vkCmdBlitImage)
	INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
	INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImage)
	INIT_DEVICE_FUNCTION(vkCmdDispatch)
	INIT_DEVICE_FUNCTION(vkCmdDraw)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
	INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdNextSubpass)
	INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
	INIT_DEVICE_FUNCTION(vkCmdPushConstants)
	INIT_DEVICE_FUNCTION(vkCmdResetQueryPool)
	INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
	INIT_DEVICE_FUNCTION(vkCmdSetScissor)
	INIT_DEVICE_FUNCTION(vkCmdSetViewport)
	INIT_DEVICE_FUNCTION(vkCmdWriteTimestamp)
	INIT_DEVICE_FUNCTION(vkCreateBuffer)
	INIT_DEVICE_FUNCTION(vkCreateCommandPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkCreateFence)
	INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
	INIT_DEVICE_FUNCTION(vkCreateComputePipelines)
	INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
	INIT_DEVICE_FUNCTION(vkCreateImage)
	INIT_DEVICE_FUNCTION(vkCreateImageView)
	INIT_DEVICE_FUNCTION(vkCreatePipelineCache)
	INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
	INIT_DEVICE_FUNCTION(vkCreateQueryPool)
	INIT_DEVICE_FUNCTION(vkCreateRenderPass)
	INIT_DEVICE_FUNCTION(vkCreateSampler)
	INIT_DEVICE_FUNCTION(vkCreateSemaphore)
	INIT_DEVICE_FUNCTION(vkCreateShaderModule)
	INIT_DEVICE_FUNCTION(vkDestroyBuffer)
	INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkDestroyDevice)
	INIT_DEVICE_FUNCTION(vkDestroyFence)
	INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
	INIT_DEVICE_FUNCTION(vkDestroyImage)
	INIT_DEVICE_FUNCTION(vkDestroyImageView)
	INIT_DEVICE_FUNCTION(vkDestroyPipeline)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineCache)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
	INIT_DEVICE_FUNCTION(vkDestroyQueryPool)
	INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
	INIT_DEVICE_FUNCTION(vkDestroySampler)
	INIT_DEVICE_FUNCTION(vkDestroySemaphore)
	INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
	INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
	INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
	INIT_DEVICE_FUNCTION(vkFlushMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
	INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
	INIT_DEVICE_FUNCTION(vkFreeMemory)
	INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
	INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
	INIT_DEVICE_FUNCTION(vkGetQueryPoolResults)
	INIT_DEVICE_FUNCTION(vkInvalidateMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkMapMemory)
	INIT_DEVICE_FUNCTION(vkQueueSubmit)
	INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
	INIT_DEVICE_FUNCTION(vkResetCommandBuffer)
	INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
	INIT_DEVICE_FUNCTION(vkResetFences)
	INIT_DEVICE_FUNCTION(vkUnmapMemory)
	INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkWaitForFences)
	INIT_DEVICE_FUNCTION(vkAcquireNextImageKHR)
	INIT_DEVICE_FUNCTION(vkCreateSwapchainKHR)
	INIT_DEVICE_FUNCTION(vkDestroySwapchainKHR)
	INIT_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)
	INIT_DEVICE_FUNCTION(vkQueuePresentKHR)

	if ( vk.dedicatedAllocation ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferMemoryRequirements2KHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetImageMemoryRequirements2KHR);
		if ( !qvkGetBufferMemoryRequirements2KHR || !qvkGetImageMemoryRequirements2KHR ) {
			vk.dedicatedAllocation = qfalse;
		}
	}

	if ( vk.debugMarkers ) {
		INIT_DEVICE_FUNCTION_EXT(vkDebugMarkerSetObjectNameEXT)
	}
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT

static void deinit_instance_functions( void )
{
	qvkCreateInstance = NULL;
	qvkEnumerateInstanceExtensionProperties = NULL;

	// instance functions:
	qvkCreateDevice = NULL;
	qvkDestroyInstance = NULL;
	qvkEnumerateDeviceExtensionProperties = NULL;
	qvkEnumeratePhysicalDevices = NULL;
	qvkGetDeviceProcAddr = NULL;
	qvkGetPhysicalDeviceFeatures = NULL;
	qvkGetPhysicalDeviceFormatProperties = NULL;
	qvkGetPhysicalDeviceMemoryProperties = NULL;
	qvkGetPhysicalDeviceProperties = NULL;
	qvkGetPhysicalDeviceQueueFamilyProperties = NULL;
	qvkDestroySurfaceKHR = NULL;
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
	qvkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceSupportKHR = NULL;
#ifdef USE_VK_VALIDATION
	qvkCreateDebugUtilsMessengerEXT = NULL;
	qvkDestroyDebugUtilsMessengerEXT = NULL;
#endif
}


static void deinit_device_functions( void )
{
	// device functions:
	qvkAllocateCommandBuffers					= NULL;
	qvkAllocateDescriptorSets					= NULL;
	qvkAllocateMemory							= NULL;
	qvkBeginCommandBuffer						= NULL;
	qvkBindBufferMemory							= NULL;
	qvkBindImageMemory							= NULL;
	qvkCmdBeginRenderPass						= NULL;
	qvkCmdBindDescriptorSets					= NULL;
	qvkCmdBindIndexBuffer						= NULL;
	qvkCmdBindPipeline							= NULL;
	qvkCmdBindVertexBuffers						= NULL;
	qvkCmdBlitImage								= NULL;
	qvkCmdClearAttachments						= NULL;
	qvkCmdCopyBuffer							= NULL;
	qvkCmdCopyBufferToImage						= NULL;
	qvkCmdCopyImage								= NULL;
	qvkCmdDispatch								= NULL;
	qvkCmdDraw									= NULL;
	qvkCmdDrawIndexed							= NULL;
	qvkCmdEndRenderPass							= NULL;
	qvkCmdNextSubpass							= NULL;
	qvkCmdPipelineBarrier						= NULL;
	qvkCmdPushConstants							= NULL;
	qvkCmdResetQueryPool						= NULL;
	qvkCmdSetDepthBias							= NULL;
	qvkCmdSetScissor							= NULL;
	qvkCmdSetViewport							= NULL;
	qvkCmdWriteTimestamp						= NULL;
	qvkCreateBuffer								= NULL;
	qvkCreateCommandPool						= NULL;
	qvkCreateDescriptorPool						= NULL;
	qvkCreateDescriptorSetLayout				= NULL;
	qvkCreateFence								= NULL;
	qvkCreateFramebuffer						= NULL;
	qvkCreateComputePipelines					= NULL;
	qvkCreateGraphicsPipelines					= NULL;
	qvkCreateImage								= NULL;
	qvkCreateImageView							= NULL;
	qvkCreatePipelineCache						= NULL;
	qvkCreatePipelineLayout						= NULL;
	qvkCreateQueryPool							= NULL;
	qvkCreateRenderPass							= NULL;
	qvkCreateSampler							= NULL;
	qvkCreateSemaphore							= NULL;
	qvkCreateShaderModule						= NULL;
	qvkDestroyBuffer							= NULL;
	qvkDestroyCommandPool						= NULL;
	qvkDestroyDescriptorPool					= NULL;
	qvkDestroyDescriptorSetLayout				= NULL;
	qvkDestroyDevice							= NULL;
	qvkDestroyFence								= NULL;
	qvkDestroyFramebuffer						= NULL;
	qvkDestroyImage								= NULL;
	qvkDestroyImageView							= NULL;
	qvkDestroyPipeline							= NULL;
	qvkDestroyPipelineCache						= NULL;
	qvkDestroyPipelineLayout					= NULL;
	qvkDestroyQueryPool							= NULL;
	qvkDestroyRenderPass						= NULL;
	qvkDestroySampler							= NULL;
	qvkDestroySemaphore							= NULL;
	qvkDestroyShaderModule						= NULL;
	qvkDeviceWaitIdle							= NULL;
	qvkEndCommandBuffer							= NULL;
	qvkFlushMappedMemoryRanges					= NULL;
	qvkFreeCommandBuffers						= NULL;
	qvkFreeDescriptorSets						= NULL;
	qvkFreeMemory								= NULL;
	qvkGetBufferMemoryRequirements				= NULL;
	qvkGetDeviceQueue							= NULL;
	qvkGetImageMemoryRequirements				= NULL;
	qvkGetImageSubresourceLayout				= NULL;
	qvkGetQueryPoolResults						= NULL;
	qvkInvalidateMappedMemoryRanges				= NULL;
	qvkMapMemory								= NULL;
	qvkQueueSubmit								= NULL;
	qvkQueueWaitIdle							= NULL;
	qvkResetCommandBuffer						= NULL;
	qvkResetDescriptorPool						= NULL;
	qvkResetFences								= NULL;
	qvkUnmapMemory								= NULL;
	qvkUpdateDescriptorSets						= NULL;
	qvkWaitForFences							= NULL;
	qvkAcquireNextImageKHR						= NULL;
	qvkCreateSwapchainKHR						= NULL;
	qvkDestroySwapchainKHR						= NULL;
	qvkGetSwapchainImagesKHR					= NULL;
	qvkQueuePresentKHR							= NULL;

	qvkGetBufferMemoryRequirements2KHR			= NULL;
	qvkGetImageMemoryRequirements2KHR			= NULL;

	qvkDebugMarkerSetObjectNameEXT				= NULL;
}


static VkShaderModule SHADER_MODULE(const uint8_t *bytes, const int count) {
	VkShaderModuleCreateInfo desc;
	VkShaderModule module;

	if ( count % 4 != 0 ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: SPIR-V binary buffer size is not a multiple of 4" );
	}

	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = count;
	desc.pCode = (const uint32_t*)bytes;

	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));

	return module;
}


static void vk_create_layout_binding( int binding, VkDescriptorType type, VkShaderStageFlags flags, VkDescriptorSetLayout *layout )
{
	VkDescriptorSetLayoutBinding bind;
	VkDescriptorSetLayoutCreateInfo desc;

	bind.binding = binding;
	bind.descriptorType = type;
	bind.descriptorCount = 1;
	bind.stageFlags = flags;
	bind.pImmutableSamplers = NULL;

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = 1;
	desc.pBindings = &bind;

	VK_CHECK( qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, layout ) );
}


void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;

	info.buffer = buffer;
	info.offset = 0;
	info.range = sizeof( vkUniform_t );

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
}


static VkSampler vk_find_sampler( const Vk_Sampler_Def *def ) {
	VkSamplerAddressMode address_mode;
	VkSamplerCreateInfo desc;
	VkSampler sampler;
	VkFilter mag_filter;
	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	float maxLod;

	// Look for sampler among existing samplers.
	for ( int i = 0; i < vk.samplers.count; i++ ) {
		const Vk_Sampler_Def *cur_def = &vk.samplers.def[i];
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			return vk.samplers.handle[i];
		}
	}

	// Create new sampler.
	if ( vk.samplers.count >= MAX_VK_SAMPLERS ) {
		ri.Terminate( TERM_CLIENT_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n" );
		// return VK_NULL_HANDLE;
	}

	address_mode = def->address_mode;

	if (def->gl_mag_filter == GL_NEAREST) {
		mag_filter = VK_FILTER_NEAREST;
	} else if (def->gl_mag_filter == GL_LINEAR) {
		mag_filter = VK_FILTER_LINEAR;
	} else {
		ri.Terminate( TERM_UNRECOVERABLE, "vk_find_sampler: invalid gl_mag_filter");
		return VK_NULL_HANDLE;
	}

	maxLod = vk.maxLod;

	if (def->gl_min_filter == GL_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else {
		ri.Terminate( TERM_UNRECOVERABLE, "vk_find_sampler: invalid gl_min_filter");
		return VK_NULL_HANDLE;
	}

	if ( def->max_lod_1_0 ) {
		maxLod = 1.0f;
	}

	desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.magFilter = mag_filter;
	desc.minFilter = min_filter;
	desc.mipmapMode = mipmap_mode;
	desc.addressModeU = address_mode;
	desc.addressModeV = address_mode;
	desc.addressModeW = address_mode;
	desc.mipLodBias = 0.0f;

	if ( def->noAnisotropy || mipmap_mode == VK_SAMPLER_MIPMAP_MODE_NEAREST || mag_filter == VK_FILTER_NEAREST ) {
		desc.anisotropyEnable = VK_FALSE;
		desc.maxAnisotropy = 1.0f;
	} else {
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
		if ( desc.anisotropyEnable ) {
			desc.maxAnisotropy = MIN( r_ext_max_anisotropy->integer, vk.maxAnisotropy );
		}
	}

	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = (maxLod == vk.maxLod) ? VK_LOD_CLAMP_NONE : maxLod;
	desc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( qvkCreateSampler( vk.device, &desc, NULL, &sampler ) );

	SET_OBJECT_NAME( sampler, va( "image sampler %i", vk.samplers.count ), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );

	vk.samplers.def[ vk.samplers.count ] = *def;
	vk.samplers.handle[ vk.samplers.count ] = sampler;
	vk.samplers.count++;

	return sampler;
}


void vk_destroy_samplers( void )
{
	for ( int i = 0; i < vk.samplers.count; i++ ) {
		qvkDestroySampler( vk.device, vk.samplers.handle[i], NULL );
		memset( &vk.samplers.def[i], 0x0, sizeof( vk.samplers.def[i] ) );
		vk.samplers.handle[i] = VK_NULL_HANDLE;
	}

	vk.samplers.count = 0;
}


void vk_update_attachment_descriptors( void ) {

#if FEAT_FBO_DEBUG
	ri.Log( SEV_INFO, "^3[FBO_DEBUG] vk_update_attachment_descriptors: color_image_view=%p fboActive=%d\n",
		(void*)vk.color_image_view, vk.fboActive );
#endif

	if ( vk.color_image_view )
	{
		VkDescriptorImageInfo info;
		VkWriteDescriptorSet desc;
		Vk_Sampler_Def sd;

		memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = vk.blitFilter;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );
		info.imageView = vk.color_image_view;
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc.dstSet = vk.color_descriptor;
		desc.dstBinding = 0;
		desc.dstArrayElement = 0;
		desc.descriptorCount = 1;
		desc.pNext = NULL;
		desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc.pImageInfo = &info;
		desc.pBufferInfo = NULL;
		desc.pTexelBufferView = NULL;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// screenmap
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.max_lod_1_0 = qfalse;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );

		info.imageView = vk.screenMap.color_image_view;
		desc.dstSet = vk.screenMap.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// bloom images
		if ( r_bloom->integer )
		{
			uint32_t i;
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				info.imageView = vk.bloom_image_view[i];
				desc.dstSet = vk.bloom_image_descriptor[i];

				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}

		// depth fade copy image
		if ( vk.depthFade.active && vk.depthFade.image != VK_NULL_HANDLE )
		{
			info.sampler = vk.depthFade.sampler;
			info.imageView = vk.depthFade.view;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.depthFade.descriptor;
			desc.dstBinding = 0;
			desc.dstArrayElement = 0;
			desc.descriptorCount = 1;

			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

#if FEAT_SHADOW_MAPPING
		// shadow map depth image
		if ( vk.shadowMap.active && vk.shadowMap.image != VK_NULL_HANDLE )
		{
			info.sampler = vk.shadowMap.sampler;
			info.imageView = vk.shadowMap.view;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.shadowMap.descriptor;
			desc.dstBinding = 0;
			desc.dstArrayElement = 0;
			desc.descriptorCount = 1;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}
#endif

		// SMAA descriptors
		if ( vk.smaa.active && vk.smaa.edges_image != VK_NULL_HANDLE )
		{
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstBinding = 0;
			desc.dstArrayElement = 0;
			desc.descriptorCount = 1;

			// edges: point sampler
			info.sampler = vk.smaa.point_sampler;
			info.imageView = vk.smaa.edges_view;
			desc.dstSet = vk.smaa.edges_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// blend weights: linear sampler
			info.sampler = vk.smaa.linear_sampler;
			info.imageView = vk.smaa.blend_view;
			desc.dstSet = vk.smaa.blend_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// input (color copy): linear sampler
			info.sampler = vk.smaa.linear_sampler;
			info.imageView = vk.smaa.input_view;
			desc.dstSet = vk.smaa.input_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// area LUT: linear sampler
			info.sampler = vk.smaa.linear_sampler;
			info.imageView = vk.smaa.area_view;
			desc.dstSet = vk.smaa.area_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// search LUT: point sampler
			info.sampler = vk.smaa.point_sampler;
			info.imageView = vk.smaa.search_view;
			desc.dstSet = vk.smaa.search_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}
	}
}


void vk_init_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;
	uint32_t i;

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_storage;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.storage.descriptor ) );

	info.buffer = vk.storage.buffer;
	info.offset = 0;
	info.range = sizeof( uint32_t );

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.storage.descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	// allocated and update descriptor set
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_uniform;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.tess[i].uniform_descriptor ) );

		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

		SET_OBJECT_NAME( vk.tess[ i ].uniform_descriptor, va( "uniform descriptor %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	if ( vk.color_image_view )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.color_descriptor ) );

		if ( r_bloom->integer )
		{
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.bloom_image_descriptor[i] ) );
			}
		}

		alloc.descriptorSetCount = 1;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.screenMap.color_descriptor ) ); // screenmap

		if ( vk.depthFade.active ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.depthFade.descriptor ) );
		}

#if FEAT_SHADOW_MAPPING
		if ( vk.shadowMap.active ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.shadowMap.descriptor ) );
		}
#endif

		if ( vk.smaa.active ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.edges_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.blend_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.input_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.area_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.search_descriptor ) );
		}

		vk_update_attachment_descriptors();
	}

#if FEAT_IQM
	// re-allocate IQM bone UBO descriptors after descriptor pool reset
	if ( vk.iqmGpu.available ) {
		VkDescriptorSetAllocateInfo boneAlloc;
		VkDescriptorBufferInfo boneBufDesc;
		VkWriteDescriptorSet boneWriteDesc;
		uint32_t j;

		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			memset( &boneAlloc, 0, sizeof( boneAlloc ) );
			boneAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			boneAlloc.descriptorPool = vk.descriptor_pool;
			boneAlloc.descriptorSetCount = 1;
			boneAlloc.pSetLayouts = &vk.iqmGpu.set_layout_bones;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &boneAlloc, &vk.iqmGpu.bone_descriptor[j] ) );

			memset( &boneBufDesc, 0, sizeof( boneBufDesc ) );
			boneBufDesc.buffer = vk.iqmGpu.bone_buffer[j];
			boneBufDesc.offset = 0;
			boneBufDesc.range = IQM_BONE_UBO_SIZE;

			memset( &boneWriteDesc, 0, sizeof( boneWriteDesc ) );
			boneWriteDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			boneWriteDesc.dstSet = vk.iqmGpu.bone_descriptor[j];
			boneWriteDesc.dstBinding = 0;
			boneWriteDesc.dstArrayElement = 0;
			boneWriteDesc.descriptorCount = 1;
			boneWriteDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			boneWriteDesc.pBufferInfo = &boneBufDesc;

			qvkUpdateDescriptorSets( vk.device, 1, &boneWriteDesc, 0, NULL );
		}
	}
#endif

	// re-allocate ribbon SSBO descriptors after descriptor pool reset.
	// The buffers (points, headers) and the set/pipeline layouts survive
	// the reset; only the descriptor sets need recreation + rewrite.
	if ( vk.ribbon.available ) {
		VkDescriptorSetAllocateInfo dsAlloc;
		VkWriteDescriptorSet writes[2];
		VkDescriptorBufferInfo bufInfos[2];
		const uint32_t pointsBytes  = RIBBON_POINTS_PER_FRAME  * RIBBON_POINT_BYTES;
		const uint32_t headersBytes = RIBBON_HEADERS_PER_FRAME * RIBBON_HEADER_BYTES;
		uint32_t j;

		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.ribbon.set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.ribbon.descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.ribbon.points_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = pointsBytes;
			bufInfos[1].buffer = vk.ribbon.headers_buffer[j];
			bufInfos[1].offset = 0;
			bufInfos[1].range  = headersBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.ribbon.descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];
			writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet          = vk.ribbon.descriptor[j];
			writes[1].dstBinding      = 1;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[1].pBufferInfo     = &bufInfos[1];

			qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
		}
	}

	// re-allocate sprite SSBO descriptor after descriptor pool reset.
	// The buffer (headers) and the set/pipeline layouts survive the
	// reset; only the descriptor sets need recreation + rewrite.
	if ( vk.sprite.available ) {
		VkDescriptorSetAllocateInfo dsAlloc;
		VkWriteDescriptorSet writes[1];
		VkDescriptorBufferInfo bufInfos[1];
		const uint32_t headersBytes = SPRITES_PER_FRAME * SPRITE_HEADER_BYTES;
		uint32_t j;

		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.sprite.set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.sprite.descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.sprite.headers_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = headersBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.sprite.descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];

			qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );
		}
	}

	// re-allocate beam SSBO descriptor after descriptor pool reset.
	// The header buffers and set/pipeline layouts survive the reset;
	// only the descriptor sets need recreation + rewrite of the
	// header SSBO binding (binding 0). Binding 1 (the sampler array)
	// is rewritten later by vk_init_primitive_shader_images, which
	// runs from R_Init AFTER this function. Without this block,
	// vk.beam.descriptor[j] holds stale handles after vid_restart
	// and the first call into the sampler-array write site fires
	// "Invalid VkDescriptorSet Object" validation errors.
	if ( vk.beam.available ) {
		VkDescriptorSetAllocateInfo dsAlloc;
		VkWriteDescriptorSet writes[3];
		VkDescriptorBufferInfo bufInfos[3];
		const uint32_t headerBytes = BEAM_POOL_MAX * BEAM_HEADER_BYTES;
		uint32_t j;

		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			uint32_t writeCount;

			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.beam.set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.beam.descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.beam.header_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = headerBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.beam.descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];

			writeCount = 1;

			// Phase 5G: bindings 2 (per-stage data) and 3 (stage counts)
			// must be re-written here too. The buffers themselves
			// survive vid_restart (allocated in
			// vk_init_primitive_shader_stages, which runs after this
			// from R_Init's R_InitImages → vk_init_primitive_shader_images
			// path); skip their writes if the buffers don't exist yet
			// — vk_init_primitive_shader_images will handle the late
			// case via its own descriptor write or the sampler-array
			// write site (which fires after R_Init).
			if ( vk.primitive_stages_buffer != VK_NULL_HANDLE ) {
				bufInfos[1].buffer = vk.primitive_stages_buffer;
				bufInfos[1].offset = 0;
				bufInfos[1].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
					* PRIMITIVE_STAGE_MAX * VK_PRIMITIVE_STAGE_BYTES;

				writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[1].dstSet          = vk.beam.descriptor[j];
				writes[1].dstBinding      = 2;
				writes[1].descriptorCount = 1;
				writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				writes[1].pBufferInfo     = &bufInfos[1];

				bufInfos[2].buffer = vk.primitive_stage_counts_buffer;
				bufInfos[2].offset = 0;
				bufInfos[2].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
					* sizeof( uint32_t );

				writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[2].dstSet          = vk.beam.descriptor[j];
				writes[2].dstBinding      = 3;
				writes[2].descriptorCount = 1;
				writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				writes[2].pBufferInfo     = &bufInfos[2];

				writeCount = 3;
			}

			qvkUpdateDescriptorSets( vk.device, writeCount, writes, 0, NULL );
		}

		// Pool slots' active flags survived the descriptor pool reset
		// in host memory, but the underlying GPU resources (descriptor
		// sets) got invalidated. Any in-flight transient or persistent
		// beam from before vid_restart is now decoupled from valid
		// state — drop them all so RB_DrawBeams starts fresh.
		memset( vk.beam.active,    0, sizeof( vk.beam.active ) );
		memset( vk.beam.spawnTime, 0, sizeof( vk.beam.spawnTime ) );
		memset( vk.beam.duration,  0, sizeof( vk.beam.duration ) );
		memset( vk.beam.fadeIn,    0, sizeof( vk.beam.fadeIn ) );
		memset( vk.beam.fadeOut,   0, sizeof( vk.beam.fadeOut ) );
		vk.beam.drawCount = 0;
	}

	// re-allocate particle compute + render descriptor sets after
	// descriptor pool reset. The buffers (pool ping-pong, classes
	// shadow, per-frame UBOs) and the set/pipeline layouts survive
	// the reset; only the descriptor sets need recreation + rewrite.
	// Without this block, particle bindings hold stale handles after
	// vk_release_resources / vid_restart and validation fires every
	// frame (Family 2 / Family 3).
	if ( vk.particle.available ) {
		VkDescriptorSetAllocateInfo dsAlloc;
		VkWriteDescriptorSet writes[4];
		VkDescriptorBufferInfo bufInfos[4];
		const uint32_t poolBytes    = PARTICLES_PER_POOL * PARTICLE_BYTES;
		const uint32_t classesBytes = MAX_PARTICLE_CLASSES * PARTICLE_CLASS_GPU_BYTES;
		const uint32_t frameBytes   = sizeof( particleFrame_t );
		uint32_t j;

		// compute descriptor sets — same layout as the freshly-init
		// pattern in vk_init_particle: UBO at 0, read pool at 1,
		// write pool at 2, class shadow at 3. Index i reads pool[i],
		// writes pool[1-i].
		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			uint32_t readPool  = j;
			uint32_t writePool = 1 - j;

			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.particle.compute_set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.particle.compute_descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.particle.frame_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = frameBytes;
			bufInfos[1].buffer = vk.particle.pool_buffer[readPool];
			bufInfos[1].offset = 0;
			bufInfos[1].range  = poolBytes;
			bufInfos[2].buffer = vk.particle.pool_buffer[writePool];
			bufInfos[2].offset = 0;
			bufInfos[2].range  = poolBytes;
			bufInfos[3].buffer = vk.particle.classes_buffer;
			bufInfos[3].offset = 0;
			bufInfos[3].range  = classesBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.particle.compute_descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];
			writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet          = vk.particle.compute_descriptor[j];
			writes[1].dstBinding      = 1;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[1].pBufferInfo     = &bufInfos[1];
			writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2].dstSet          = vk.particle.compute_descriptor[j];
			writes[2].dstBinding      = 2;
			writes[2].descriptorCount = 1;
			writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[2].pBufferInfo     = &bufInfos[2];
			writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[3].dstSet          = vk.particle.compute_descriptor[j];
			writes[3].dstBinding      = 3;
			writes[3].descriptorCount = 1;
			writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[3].pBufferInfo     = &bufInfos[3];

			qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
		}

		// render descriptor sets — UBO at 0, read pool at 1, classes
		// at 2. Index i reads pool[1-i] (the post-compute output when
		// compute_descriptor[i] just ran).
		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			uint32_t renderPool = 1 - j;

			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.particle.render_set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.particle.render_descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.particle.frame_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = frameBytes;
			bufInfos[1].buffer = vk.particle.pool_buffer[renderPool];
			bufInfos[1].offset = 0;
			bufInfos[1].range  = poolBytes;
			bufInfos[2].buffer = vk.particle.classes_buffer;
			bufInfos[2].offset = 0;
			bufInfos[2].range  = classesBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.particle.render_descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];
			writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet          = vk.particle.render_descriptor[j];
			writes[1].dstBinding      = 1;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[1].pBufferInfo     = &bufInfos[1];
			writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2].dstSet          = vk.particle.render_descriptor[j];
			writes[2].dstBinding      = 2;
			writes[2].descriptorCount = 1;
			writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[2].pBufferInfo     = &bufInfos[2];

			qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

			// Note on binding 3 (per-class sampler array): population
			// is deferred — vk_init_descriptors runs from InitOpenGL
			// BEFORE R_InitImages re-creates tr.whiteImage on a
			// vid_restart, so we cannot read tr.whiteImage->view
			// here. R_Init calls vk_init_particle_textures AFTER
			// R_InitImages to populate this binding eagerly against
			// the now-fresh tr.whiteImage.
		}
		// Pool reset invalidated the previously-populated sampler
		// array; R_DeleteTextures + R_InitImages also invalidate the
		// cached image_t pointers in classImages[] (the old image_t
		// structs were freed; new ones live at different addresses).
		// Clear both. R_Init's call to vk_init_particle_textures
		// (after R_InitImages re-creates tr.whiteImage) repopulates
		// the array; cgame re-registration later writes each class's
		// own slot in classImages[].
		memset( vk.particle.classImages, 0, sizeof( vk.particle.classImages ) );
		vk.particle.numClasses = 0;
	}
}


static void vk_release_geometry_buffers( void )
{
	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroyBuffer( vk.device, vk.tess[i].vertex_buffer, NULL );
		vk.tess[i].vertex_buffer = VK_NULL_HANDLE;
	}

	qvkFreeMemory( vk.device, vk.geometry_buffer_memory, NULL );
	vk.geometry_buffer_memory = VK_NULL_HANDLE;
}


static void vk_create_geometry_buffers( VkDeviceSize size )
{
	VkMemoryRequirements vb_memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	uint32_t memory_type_bits;
	uint32_t memory_type;
	void *data;
	int i;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	memset( &vb_memory_requirements, 0, sizeof( vb_memory_requirements ) );

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		desc.size = size;
		desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.tess[i].vertex_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.tess[i].vertex_buffer, &vb_memory_requirements );
	}

	memory_type_bits = vb_memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = vb_memory_requirements.size * NUM_COMMAND_BUFFERS;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.geometry_buffer_memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data ) );

	vertex_buffer_offset = 0;

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkBindBufferMemory( vk.device, vk.tess[i].vertex_buffer, vk.geometry_buffer_memory, vertex_buffer_offset );
		vk.tess[i].vertex_buffer_ptr = (byte*)data + vertex_buffer_offset;
		vk.tess[i].vertex_buffer_offset = 0;
		vertex_buffer_offset += vb_memory_requirements.size;

		SET_OBJECT_NAME( vk.tess[i].vertex_buffer, va( "geometry buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	}

	SET_OBJECT_NAME( vk.geometry_buffer_memory, "geometry buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	vk.geometry_buffer_size = vb_memory_requirements.size;

	memset( &vk.stats, 0, sizeof( vk.stats ) );
}


static void vk_create_storage_buffer( uint32_t size )
{
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	uint32_t memory_type_bits;
	uint32_t memory_type;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	memset( &memory_requirements, 0, sizeof( memory_requirements ) );

	desc.size = size;
	desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.storage.buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.storage.buffer, &memory_requirements );

	memory_type_bits = memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.storage.memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.storage.memory, 0, VK_WHOLE_SIZE, 0, (void**)&vk.storage.buffer_ptr ) );

	memset( vk.storage.buffer_ptr, 0, memory_requirements.size );

	qvkBindBufferMemory( vk.device, vk.storage.buffer, vk.storage.memory, 0 );

	SET_OBJECT_NAME( vk.storage.buffer, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.storage.descriptor, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	SET_OBJECT_NAME( vk.storage.memory, "storage buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


/*
===============
Primitive ribbon — self-contained pipeline.

Cgame submits world-space ribbons of N control points each via
RE_AddRibbonToScene. Each call appends `numPoints` GPU RibbonPoint
records to the per-frame points SSBO and one GPU RibbonHeader to
the per-frame headers SSBO. RB_DrawRibbons (called from
RB_DrawSurfs after world translucents) issues one direct
vkCmdDraw per submitted ribbon, with `firstInstance = headerIdx`
so the vertex shader can read its header via gl_InstanceIndex.

Push range layout (vertex stage only, 80 bytes):
    bytes  0..63  mat4  mvp        — world MVP, Y-flipped for Vulkan
    bytes 64..79  vec4  eyeWorld   — .xyz = world-space camera origin

Descriptor set 0:
    binding 0  STORAGE_BUFFER  RibbonPoint  points[]
    binding 1  STORAGE_BUFFER  RibbonHeader headers[]

Two pipeline variants (alpha and additive) selected per ribbon
from header.flags.
===============
*/

void vk_init_ribbon( void )
{
	VkDescriptorSetLayoutBinding binds[3];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkBufferCreateInfo bufInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkDescriptorSetAllocateInfo dsAlloc;
	VkWriteDescriptorSet writes[2];
	VkDescriptorBufferInfo bufInfos[2];
	uint32_t pointsBytes;
	uint32_t headersBytes;
	int i;

	// Phase 5M: anti-drift checks on the ribbon SSBO byte-layout
	// constants. The point assert is real layout protection — host
	// `ribbonPoint_t` is the std430-mirror of GPU `RibbonPoint`, so a
	// drift in either side fires the assert. The header assert is a
	// documentation tautology (no host typedef to take sizeof of —
	// SSBO writes go through raw float/uint pointers); kept for parity
	// with BEAM_HEADER_BYTES's assert and to flag bumps to the constant.
#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
	_Static_assert( sizeof( ribbonPoint_t ) == RIBBON_POINT_BYTES,
		"ribbonPoint_t (host) and GPU RibbonPoint must agree on layout: "
		"vec3 pos + float width + vec4 rgba + vec3 normal + float pad = 48 B" );
	_Static_assert( RIBBON_HEADER_BYTES == 32,
		"RIBBON_HEADER_BYTES must be 32 bytes for std430 RibbonHeader "
		"(4 uint header + vec2 uvScroll + float spawnTime + float pad = 32 B)" );
#endif

	memset( &vk.ribbon, 0, sizeof( vk.ribbon ) );

	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[1].binding         = 1;
	binds[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	// binding 2: per-shader-handle texture array. Populated host-side
	// from vk_primitive_shader_images[] by vk_init_primitive_shader_images
	// (called from R_Init AFTER tr.whiteImage exists; deferred because
	// vk_init_ribbon runs from InitOpenGL before R_InitImages). Fragment
	// stage only — only ribbon.frag samples it.
	binds[2].binding         = 2;
	binds[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[2].descriptorCount = PRIMITIVE_SHADER_IMAGE_MAX;
	binds[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 3;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.ribbon.set_layout ) );

	memset( &pushRange, 0, sizeof( pushRange ) );
	// VERTEX | FRAGMENT: the trailing vec4 frameParams (.x =
	// tr.identityLight) is consumed by ribbon.frag, so the range
	// must be visible to the fragment stage. The shape of the
	// vertex-only fields (mvp, eyeWorld) is unchanged.
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	                     | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset     = 0;
	pushRange.size       = 96; // mat4 mvp + vec4 eyeWorld + vec4 frameParams

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount         = 1;
	pipeLayoutInfo.pSetLayouts            = &vk.ribbon.set_layout;
	pipeLayoutInfo.pushConstantRangeCount = 1;
	pipeLayoutInfo.pPushConstantRanges    = &pushRange;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.ribbon.pipeline_layout ) );

	pointsBytes  = RIBBON_POINTS_PER_FRAME  * RIBBON_POINT_BYTES;
	headersBytes = RIBBON_HEADERS_PER_FRAME * RIBBON_HEADER_BYTES;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t memType;

		// points buffer (host-coherent, mapped)
		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = pointsBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.ribbon.points_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.ribbon.points_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.ribbon.points_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.ribbon.points_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.ribbon.points_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.ribbon.points_buffer[i], vk.ribbon.points_memory[i], 0 );

		// headers buffer (host-coherent, mapped)
		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = headersBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.ribbon.headers_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.ribbon.headers_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.ribbon.headers_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.ribbon.headers_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.ribbon.headers_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.ribbon.headers_buffer[i], vk.ribbon.headers_memory[i], 0 );
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.ribbon.set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.ribbon.descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.ribbon.points_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = pointsBytes;
		bufInfos[1].buffer = vk.ribbon.headers_buffer[i];
		bufInfos[1].offset = 0;
		bufInfos[1].range  = headersBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.ribbon.descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];
		writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet          = vk.ribbon.descriptor[i];
		writes[1].dstBinding      = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[1].pBufferInfo     = &bufInfos[1];

		qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
	}

	// Two graphics pipeline variants (alpha and additive). Everything
	// matches the existing translucent pattern: depthTest=LESS_OR_EQUAL,
	// depthWrite=OFF, cull=NONE, blend enabled. Variant differs only in
	// the destination blend factor.
	{
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineViewportStateCreateInfo viewportState;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;
		int variant;

		memset( stages, 0, sizeof( stages ) );
		stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.ribbon_vs;
		stages[0].pName  = "main";
		stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.ribbon_fs;
		stages[1].pName  = "main";

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates    = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width    = (float)vk.renderWidth;
		viewport.height   = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width  = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports    = &viewport;
		viewportState.scissorCount  = 1;
		viewportState.pScissors     = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth   = 1.0f;
		rasterizer.cullMode    = VK_CULL_MODE_NONE;            // two-sided
		rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = vk.msaaActive ? vkSamples : VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable  = VK_TRUE;
		depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments    = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount          = 2;
		gpInfo.pStages             = stages;
		gpInfo.pVertexInputState   = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState      = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState   = &multisampling;
		gpInfo.pDepthStencilState  = &depthStencil;
		gpInfo.pColorBlendState    = &colorBlend;
		gpInfo.pDynamicState       = &dynamicState;
		gpInfo.layout              = vk.ribbon.pipeline_layout;
		gpInfo.renderPass          = vk.render_pass.main;
		gpInfo.subpass             = 0;

		// 0 = alpha (SRC_ALPHA / ONE_MINUS_SRC_ALPHA),
		// 1 = additive (SRC_ALPHA / ONE).
		for ( variant = 0; variant < 2; variant++ ) {
			memset( &blendAttach, 0, sizeof( blendAttach ) );
			blendAttach.blendEnable         = VK_TRUE;
			blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttach.dstColorBlendFactor = (variant == 0)
				? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
				: VK_BLEND_FACTOR_ONE;
			blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.srcAlphaBlendFactor = blendAttach.srcColorBlendFactor;
			blendAttach.dstAlphaBlendFactor = blendAttach.dstColorBlendFactor;
			blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL,
				(variant == 0) ? &vk.ribbon.pipeline_alpha : &vk.ribbon.pipeline_additive ) );
		}
	}

	vk.ribbon.available = qtrue;
	ri.Log( SEV_INFO, "Vulkan ribbon primitive pipeline initialized (%u points / %u headers per frame)\n",
		(unsigned)RIBBON_POINTS_PER_FRAME, (unsigned)RIBBON_HEADERS_PER_FRAME );
}


void vk_shutdown_ribbon( void )
{
	int i;

	if ( vk.ribbon.pipeline_alpha != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ribbon.pipeline_alpha, NULL );
		vk.ribbon.pipeline_alpha = VK_NULL_HANDLE;
	}
	if ( vk.ribbon.pipeline_additive != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ribbon.pipeline_additive, NULL );
		vk.ribbon.pipeline_additive = VK_NULL_HANDLE;
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.ribbon.points_buffer[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.ribbon.points_buffer[i], NULL );
			vk.ribbon.points_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.ribbon.points_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.ribbon.points_memory[i], NULL );
			vk.ribbon.points_memory[i] = VK_NULL_HANDLE;
			vk.ribbon.points_ptr[i] = NULL;
		}
		if ( vk.ribbon.headers_buffer[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.ribbon.headers_buffer[i], NULL );
			vk.ribbon.headers_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.ribbon.headers_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.ribbon.headers_memory[i], NULL );
			vk.ribbon.headers_memory[i] = VK_NULL_HANDLE;
			vk.ribbon.headers_ptr[i] = NULL;
		}
	}

	if ( vk.ribbon.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.ribbon.pipeline_layout, NULL );
		vk.ribbon.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.ribbon.set_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.ribbon.set_layout, NULL );
		vk.ribbon.set_layout = VK_NULL_HANDLE;
	}

	vk.ribbon.available = qfalse;
}


void RB_DrawRibbons( void )
{
	VkCommandBuffer cmd;
	int frameIdx;
	VkPipeline lastPipeline;
	float pushBuf[24]; // mat4 mvp + vec4 eyeWorld + vec4 frameParams
	float mvp[16];
	const float *p;
	float proj[16];
	uint32_t i;

	// Pipelines were created against vk.render_pass.main only — skip if
	// we're being called inside the screenmap pass.
	if ( !vk.ribbon.available
	  || vk.ribbon.numHeadersThisFrame == 0
	  || vk.renderPassIndex != RENDER_PASS_MAIN )
		return;

	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// World MVP — same construction the standard 3D path uses
	// (get_mvp_transform): copy the projection, flip column 1
	// for Vulkan clip-space, then myGlMultMatrix( modelView, proj ).
	p = backEnd.viewParms.projectionMatrix;
	memcpy( proj, p, sizeof( proj ) );
	proj[5] = -p[5];
	myGlMultMatrix( backEnd.viewParms.world.modelMatrix, proj, mvp );

	memcpy( pushBuf,      mvp,                          64 );
	memcpy( pushBuf + 16, backEnd.viewParms.or.origin,  sizeof( vec3_t ) );
	pushBuf[19] = 0.0f;
	// frameParams: .x = tr.identityLight (CGEN_VERTEX-equivalent
	// halving for ribbon.frag); .y = currentTime (consumed by
	// ribbon.vert to drive uvScroll age); .zw reserved.
	pushBuf[20] = tr.identityLight;
	pushBuf[21] = (float)backEnd.refdef.floatTime;
	pushBuf[22] = 0.0f;
	pushBuf[23] = 0.0f;

	{
		VkViewport viewport;
		VkRect2D   scissor;
		memset( &viewport, 0, sizeof( viewport ) );
		viewport.x        = (float)backEnd.viewParms.viewportX;
		viewport.y        = (float)backEnd.viewParms.viewportY;
		viewport.width    = (float)backEnd.viewParms.viewportWidth;
		viewport.height   = (float)backEnd.viewParms.viewportHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.offset.x      = backEnd.viewParms.viewportX;
		scissor.offset.y      = backEnd.viewParms.viewportY;
		scissor.extent.width  = backEnd.viewParms.viewportWidth;
		scissor.extent.height = backEnd.viewParms.viewportHeight;
		qvkCmdSetViewport( cmd, 0, 1, &viewport );
		qvkCmdSetScissor ( cmd, 0, 1, &scissor );
	}

	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.ribbon.pipeline_layout, 0, 1, &vk.ribbon.descriptor[frameIdx], 0, NULL );

	qvkCmdPushConstants( cmd, vk.ribbon.pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( pushBuf ), pushBuf );

	lastPipeline = VK_NULL_HANDLE;
	for ( i = 0; i < vk.ribbon.numHeadersThisFrame; i++ ) {
		const uint32_t *hdr = (const uint32_t *)
			(vk.ribbon.headers_ptr[frameIdx] + i * RIBBON_HEADER_BYTES);
		uint32_t pointCount = hdr[1];
		uint32_t flags      = hdr[3];
		VkPipeline pipe = (flags & PRIM_FLAG_ADDITIVE)
			? vk.ribbon.pipeline_additive : vk.ribbon.pipeline_alpha;

		if ( pointCount < 2 )
			continue;

		if ( pipe != lastPipeline ) {
			qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe );
			lastPipeline = pipe;
		}

		// 6 verts per segment × (pointCount - 1) segments. firstInstance = i
		// so the vertex shader sees this header at gl_InstanceIndex.
		qvkCmdDraw( cmd, ( pointCount - 1 ) * 6, 1, 0, i );
	}

	// Invalidate cached pipeline / descriptor / depth-range so the next
	// standard draw rebinds correctly. Same cleanup pattern the IQM
	// draw uses.
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range   = DEPTH_RANGE_COUNT;
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end   = 0;
}


/*
===============
Primitive beam — self-contained pipeline.

Cgame submits two-endpoint camera-facing quads via RE_AddBeamToScene.
The renderer maintains a small mixed-mode pool (BEAM_POOL_MAX = 128
slots) holding both transient (one-frame) and persistent (lifetime
+ fade) beams. Each frame, RB_DrawBeams walks the pool, resolves
entity-attached endpoints into world space, computes fade alpha for
persistent beams, writes a compacted run of beamHeaderGPU_t entries
to the per-frame SSBO, and issues a single vkCmdDraw with
vertexCount = 6 * BEAM_AXIAL_MAX, instanceCount = drawCount.

The vertex shader (beam.vert) expands each instance into
axialCopies × 6 vertices, emitting clip-space-behind degenerate
output for axial copies above the beam's `axialCopies` field
(GPU rasterizer discards). axialCopies > 1 produces a "cross"
pattern of multiple camera-facing quads rotated around the beam
axis at equal angular intervals.

Push range layout (96 bytes):
    bytes  0..63   mat4  mvp           — world MVP, Y-flipped for Vulkan
    bytes 64..79   vec4  eyeWorld      — .xyz = camera world origin (axis-facing math)
    bytes 80..95   vec4  frameParams   — reserved (matches ribbon layout)

Descriptor set 0:
    binding 0  STORAGE_BUFFER          BeamHeader headers[]
    binding 1  COMBINED_IMAGE_SAMPLER  shaderImages[PRIMITIVE_SHADER_IMAGE_MAX]

Lifetime semantics:
    desc.duration == 0  → transient. Drawn this frame; slot freed
                          at the end of RB_DrawBeams. Cgame must
                          re-submit each frame to keep visible.
    desc.duration  > 0  → persistent. spawnTime captured at
                          RE_AddBeamToScene. Engine fades in/out
                          and frees the slot when age >= duration.

Entity attachment is translate-only: cachedStart =
tr.refdef.entities[startEntityNum].e.origin + startOffset.
Rotation is NOT applied (kept simple for the LG-style migration
sites that pass world coords directly with startEntityNum = -1).
===============
*/

void vk_init_beam( void )
{
	VkDescriptorSetLayoutBinding binds[4];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkBufferCreateInfo bufInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkDescriptorSetAllocateInfo dsAlloc;
	VkWriteDescriptorSet writes[3];
	VkDescriptorBufferInfo bufInfos[3];
	uint32_t headerBytes;
	int i;

	memset( &vk.beam, 0, sizeof( vk.beam ) );

	// Sanity check on the GPU header layout. The layout is described
	// in vk.h's BEAM_HEADER_BYTES comment and must match beam.vert's
	// BeamHeader struct.
#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
	_Static_assert( BEAM_HEADER_BYTES == 96,
		"BEAM_HEADER_BYTES must be 96 bytes for std430 BeamHeader "
		"(4 vec4 = 64 B (start, end, startColor, endColor); "
		"+ vec2 uvScroll + 2 float widths + float spawnTime + 3 uint "
		"= 32 B trailer; total 96 B, naturally 16-aligned)" );
	// Phase 5K: VK_PRIM_QHANDLE_MAX (parallel constant in vk.h, lives
	// outside the tr_local.h include order) must match MAX_SHADERS.
	_Static_assert( VK_PRIM_QHANDLE_MAX == MAX_SHADERS,
		"VK_PRIM_QHANDLE_MAX must equal MAX_SHADERS — qhandle→primitive-slot "
		"indirection table is sized by VK_PRIM_QHANDLE_MAX in vk.h but "
		"MAX_SHADERS is the actual qhandle bound" );
#endif

	// ── Descriptor set layout ────────────────────────────────────
	// Phase 5G: 4 bindings.
	//   0: header SSBO (per-instance beam pool, vertex stage)
	//   1: image array (sampler array, fragment stage)
	//   2: per-stage SSBO (multi-stage shader data, vertex+fragment)
	//   3: stage counts SSBO (PRIMITIVE_SHADER_IMAGE_MAX uints,
	//      vertex stage — for cheap per-stage cull)
	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[1].binding         = 1;
	binds[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].descriptorCount = PRIMITIVE_SHADER_IMAGE_MAX;
	binds[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[2].binding         = 2;
	binds[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[3].binding         = 3;
	binds[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[3].descriptorCount = 1;
	binds[3].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 4;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.beam.set_layout ) );

	memset( &pushRange, 0, sizeof( pushRange ) );
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset     = 0;
	// Phase 5G: 112 B = mat4 mvp (64) + vec4 eyeWorld (16) + vec4
	// frameParams (16) + vec4 stageParams (16). stageParams.x carries
	// the per-draw stageIdx (cast from float in the shader).
	pushRange.size       = 112;

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount         = 1;
	pipeLayoutInfo.pSetLayouts            = &vk.beam.set_layout;
	pipeLayoutInfo.pushConstantRangeCount = 1;
	pipeLayoutInfo.pPushConstantRanges    = &pushRange;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.beam.pipeline_layout ) );

	// ── Per-frame header SSBOs (host-coherent, mapped) ──────────
	headerBytes = BEAM_POOL_MAX * BEAM_HEADER_BYTES;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t memType;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = headerBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.beam.header_buffer[i] ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.beam.header_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.beam.header_memory[i] ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.beam.header_buffer[i], vk.beam.header_memory[i], 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.beam.header_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.beam.header_ptr[i] ) );
	}

	// ── Allocate descriptor sets and write bindings 0, 2, 3 ─────
	// Binding 1 (sampler array) is left unwritten here; it's
	// populated by vk_init_primitive_shader_images from R_Init,
	// AFTER tr.whiteImage exists and vk_init_particle has created
	// vk.particle.sampler.
	//
	// Bindings 2 and 3 (per-stage data, stage counts) come from
	// vk.primitive_stages_buffer / vk.primitive_stage_counts_buffer
	// allocated by vk_init_primitive_shader_stages, which is called
	// from vk_init_primitive_shader_images. The buffers may not
	// exist yet at vk_init_beam time (vk_init_beam runs before
	// R_Init); skip the writes here and re-emit at vid_restart re-
	// alloc once the buffers exist (vk_init_descriptors handles it).
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t writeCount;

		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.beam.set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.beam.descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.beam.header_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = headerBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.beam.descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];

		writeCount = 1;

		if ( vk.primitive_stages_buffer != VK_NULL_HANDLE ) {
			bufInfos[1].buffer = vk.primitive_stages_buffer;
			bufInfos[1].offset = 0;
			bufInfos[1].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
				* PRIMITIVE_STAGE_MAX * VK_PRIMITIVE_STAGE_BYTES;

			writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet          = vk.beam.descriptor[i];
			writes[1].dstBinding      = 2;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[1].pBufferInfo     = &bufInfos[1];

			bufInfos[2].buffer = vk.primitive_stage_counts_buffer;
			bufInfos[2].offset = 0;
			bufInfos[2].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
				* sizeof( uint32_t );

			writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2].dstSet          = vk.beam.descriptor[i];
			writes[2].dstBinding      = 3;
			writes[2].descriptorCount = 1;
			writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[2].pBufferInfo     = &bufInfos[2];

			writeCount = 3;
		}

		qvkUpdateDescriptorSets( vk.device, writeCount, writes, 0, NULL );
	}

	// ── Phase 5J: dedicated REPEAT-mode sampler for binding 1 ───
	// Beam UV scrolling can produce arbitrarily large out-of-range
	// UVs over a long match (-1.8 UV/sec × hundreds of seconds).
	// CLAMP_TO_EDGE saturates and kills animation; REPEAT wraps
	// natively, restoring the legacy CPU pipeline's texture-tiling
	// behaviour. Beam-only — ribbon/sprite/particle keep
	// vk.particle.sampler (CLAMP_TO_EDGE).
	{
		VkSamplerCreateInfo samplerInfo;
		memset( &samplerInfo, 0, sizeof( samplerInfo ) );
		samplerInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter     = VK_FILTER_LINEAR;
		samplerInfo.minFilter     = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor   = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		samplerInfo.minLod        = 0.0f;
		samplerInfo.maxLod        = VK_LOD_CLAMP_NONE;
		VK_CHECK( qvkCreateSampler( vk.device, &samplerInfo, NULL,
			&vk.beam.sampler_repeat ) );
		SET_OBJECT_NAME( vk.beam.sampler_repeat,
			"sampler - beam REPEAT",
			VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );
	}

	// ── Graphics pipeline ───────────────────────────────────────
	{
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineViewportStateCreateInfo viewportState;
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;

		memset( stages, 0, sizeof( stages ) );
		stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.beam_vs;
		stages[0].pName  = "main";
		stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.beam_fs;
		stages[1].pName  = "main";

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		// No vertex attributes; vertex shader pulls from the SSBO
		// via gl_VertexIndex / gl_InstanceIndex (same pattern as
		// ribbon and particle).

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates    = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width    = (float)vk.renderWidth;
		viewport.height   = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width  = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports    = &viewport;
		viewportState.scissorCount  = 1;
		viewportState.pScissors     = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth   = 1.0f;
		rasterizer.cullMode    = VK_CULL_MODE_NONE;            // two-sided
		rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = vk.msaaActive ? vkSamples : VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable  = VK_TRUE;
		depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &blendAttach, 0, sizeof( blendAttach ) );
		blendAttach.blendEnable         = VK_TRUE;
		// Phase 5G: ONE/ONE (pure additive) — matches `blendfunc add`
		// in q3 shader.script which every beam-consuming shader
		// (lightningBolt, lightningArc) uses. Source contribution is
		// `texel * fragColor.rgb`; src.alpha is ignored under ONE/ONE
		// (different from the prior SRC_ALPHA/ONE pipeline which
		// modulated the contribution by vertex alpha).
		blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments    = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount          = 2;
		gpInfo.pStages             = stages;
		gpInfo.pVertexInputState   = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState      = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState   = &multisampling;
		gpInfo.pDepthStencilState  = &depthStencil;
		gpInfo.pColorBlendState    = &colorBlend;
		gpInfo.pDynamicState       = &dynamicState;
		gpInfo.layout              = vk.beam.pipeline_layout;
		gpInfo.renderPass          = vk.render_pass.main;
		gpInfo.subpass             = 0;

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL, &vk.beam.pipeline ) );
	}

	vk.beam.available = qtrue;
	ri.Log( SEV_INFO, "Vulkan beam primitive pipeline initialized (%u pool slots, %u axial copies max)\n",
		(unsigned)BEAM_POOL_MAX, (unsigned)BEAM_AXIAL_MAX );
}


void vk_shutdown_beam( void )
{
	int i;

	if ( vk.beam.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.beam.pipeline, NULL );
		vk.beam.pipeline = VK_NULL_HANDLE;
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.beam.header_buffer[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.beam.header_buffer[i], NULL );
			vk.beam.header_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.beam.header_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.beam.header_memory[i], NULL );
			vk.beam.header_memory[i] = VK_NULL_HANDLE;
			vk.beam.header_ptr[i] = NULL;
		}
	}

	if ( vk.beam.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.beam.pipeline_layout, NULL );
		vk.beam.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.beam.set_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.beam.set_layout, NULL );
		vk.beam.set_layout = VK_NULL_HANDLE;
	}
	if ( vk.beam.sampler_repeat != VK_NULL_HANDLE ) {
		qvkDestroySampler( vk.device, vk.beam.sampler_repeat, NULL );
		vk.beam.sampler_repeat = VK_NULL_HANDLE;
	}

	vk.beam.available = qfalse;
}


void RB_DrawBeams( void )
{
	VkCommandBuffer cmd;
	int             frameIdx;
	// Phase 5G: 28 floats = 112 B push range = mat4 mvp + vec4 eyeWorld
	// + vec4 frameParams + vec4 stageParams (.x = stageIdx).
	float           pushBuf[28];
	float           mvp[16];
	const float    *p;
	float           proj[16];
	float           currentTime;
	uint32_t        i;
	uint32_t        stageIdx;
	byte           *headerBase;

	if ( !vk.beam.available
	  || vk.renderPassIndex != RENDER_PASS_MAIN ) {
		// Even if we skip the draw this frame, transient slots from
		// prior frames should still expire so they don't leak. Walk
		// the pool and free any transient slots; persistent slots
		// keep their state for the next viable frame.
		for ( i = 0; i < BEAM_POOL_MAX; i++ ) {
			if ( vk.beam.active[i] && vk.beam.duration[i] == 0.0f ) {
				vk.beam.active[i] = qfalse;
			}
		}
		vk.beam.drawCount = 0;
		return;
	}

	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;
	currentTime = (float)backEnd.refdef.floatTime;

	headerBase = vk.beam.header_ptr[frameIdx];
	vk.beam.drawCount = 0;

	// ── Pool walk: resolve, fade, write SSBO, and free expired slots
	for ( i = 0; i < BEAM_POOL_MAX; i++ ) {
		const beamDesc_t *desc;
		float    age;
		float    fadeAlpha;
		vec3_t   resolvedStart;
		vec3_t   resolvedEnd;
		uint32_t copies;
		float   *fdst;
		uint32_t *udst;
		byte    *slot;

		if ( !vk.beam.active[i] ) continue;

		desc = &vk.beam.desc[i];

		// Lifetime check.
		if ( vk.beam.duration[i] > 0.0f ) {
			age = currentTime - vk.beam.spawnTime[i];
			if ( age >= vk.beam.duration[i] ) {
				// Expired persistent beam — free the slot, skip drawing.
				vk.beam.active[i] = qfalse;
				continue;
			}

			// Compute fade alpha. fadeIn / fadeOut clamp to non-negative.
			fadeAlpha = 1.0f;
			if ( vk.beam.fadeIn[i] > 0.0f && age < vk.beam.fadeIn[i] ) {
				fadeAlpha = age / vk.beam.fadeIn[i];
			}
			if ( vk.beam.fadeOut[i] > 0.0f && age > vk.beam.duration[i] - vk.beam.fadeOut[i] ) {
				float fadeOutAge = age - ( vk.beam.duration[i] - vk.beam.fadeOut[i] );
				float k = 1.0f - fadeOutAge / vk.beam.fadeOut[i];
				if ( k < 0.0f ) k = 0.0f;
				if ( k < fadeAlpha ) fadeAlpha = k;
			}
		} else {
			// Transient: full opacity, single-frame.
			fadeAlpha = 1.0f;
		}

		// ── Resolve world-space start/end (entity attachment is
		// translate-only; entity rotation is intentionally not
		// applied — see header doc-comment).
		if ( desc->startEntityNum >= 0 && desc->startEntityNum < tr.refdef.num_entities ) {
			const trRefEntity_t *ent = &tr.refdef.entities[desc->startEntityNum];
			VectorAdd( ent->e.origin, desc->startOffset, resolvedStart );
		} else {
			VectorCopy( desc->start, resolvedStart );
		}
		if ( desc->endEntityNum >= 0 && desc->endEntityNum < tr.refdef.num_entities ) {
			const trRefEntity_t *ent = &tr.refdef.entities[desc->endEntityNum];
			VectorAdd( ent->e.origin, desc->endOffset, resolvedEnd );
		} else {
			VectorCopy( desc->end, resolvedEnd );
		}

		// Clamp axialCopies (defensive — RE_AddBeamToScene already
		// clamps, but a corrupt slot or future caller wouldn't have).
		copies = (uint32_t)desc->axialCopies;
		if ( copies < 1 )              copies = 1;
		if ( copies > BEAM_AXIAL_MAX ) copies = BEAM_AXIAL_MAX;

		// ── Write the GPU header at SSBO slot vk.beam.drawCount.
		// std430 layout (96 B):
		//   bytes  0..15  vec4  start         (.xyz, .w pad)
		//   bytes 16..31  vec4  end           (.xyz, .w pad)
		//   bytes 32..47  vec4  startColor    (alpha pre-multiplied with fade)
		//   bytes 48..63  vec4  endColor      (alpha pre-multiplied with fade)
		//   bytes 64..71  vec2  uvScroll      (8-byte aligned at offset 64)
		//   bytes 72..75  float startWidth
		//   bytes 76..79  float endWidth
		//   bytes 80..83  float spawnTime
		//   bytes 84..87  uint  shaderHandle
		//   bytes 88..91  uint  axialCopies
		//   bytes 92..95  uint  flags
		slot = headerBase + vk.beam.drawCount * BEAM_HEADER_BYTES;
		fdst = (float    *)slot;
		udst = (uint32_t *)slot;

		fdst[0] = resolvedStart[0];
		fdst[1] = resolvedStart[1];
		fdst[2] = resolvedStart[2];
		fdst[3] = 0.0f; // pad

		fdst[4] = resolvedEnd[0];
		fdst[5] = resolvedEnd[1];
		fdst[6] = resolvedEnd[2];
		fdst[7] = 0.0f; // pad

		// Premultiply fade into BOTH endpoints' alpha channels so the
		// linear-interpolated per-fragment alpha fades uniformly. RGB
		// stays as-is; the fragment shader does texel * fragColor and
		// the alpha modulation propagates correctly through the
		// additive blend.
		fdst[8]  = desc->startColor[0];
		fdst[9]  = desc->startColor[1];
		fdst[10] = desc->startColor[2];
		fdst[11] = desc->startColor[3] * fadeAlpha;

		fdst[12] = desc->endColor[0];
		fdst[13] = desc->endColor[1];
		fdst[14] = desc->endColor[2];
		fdst[15] = desc->endColor[3] * fadeAlpha;

		// Animation trailer (offsets 64..95). vk.beam.spawnTime[i]
		// was captured at RE_AddBeamToScene time; transient beams
		// (PRIM_FLAG_TRANSIENT in flags) ignore it shader-side and
		// use frameParams.y directly, persistent beams use
		// (frameParams.y - spawnTime).
		fdst[16] = desc->uvScroll[0];
		fdst[17] = desc->uvScroll[1];
		fdst[18] = desc->startWidth;
		fdst[19] = desc->endWidth;
		fdst[20] = vk.beam.spawnTime[i];
		// Phase 5K: translate cgame qhandle → primitive registry slot.
		// Out-of-range / unregistered qhandles map to slot 0 (whiteImage)
		// rather than producing OOB SSBO reads on the GPU.
		udst[21] = vk_qhandle_to_prim_slot( desc->shader );
		udst[22] = copies;
		udst[23] = (uint32_t)desc->flags;

		vk.beam.drawCount++;

		// Transient slots free immediately after the SSBO write so
		// the next frame requires re-submission.
		if ( vk.beam.duration[i] == 0.0f ) {
			vk.beam.active[i] = qfalse;
		}
	}

	if ( vk.beam.drawCount == 0 ) return;

	// ── World MVP push (same construction as ribbon/sprite).
	p = backEnd.viewParms.projectionMatrix;
	memcpy( proj, p, sizeof( proj ) );
	proj[5] = -p[5];
	myGlMultMatrix( backEnd.viewParms.world.modelMatrix, proj, mvp );

	memcpy( pushBuf,      mvp,                          64 );
	memcpy( pushBuf + 16, backEnd.viewParms.or.origin,  sizeof( vec3_t ) );
	pushBuf[19] = 0.0f;
	// frameParams: .x = identityLight (unused by beam — fragment doesn't
	// halve; set to identity for ribbon-shaped layout), .y = currentTime
	// (consumed by beam.vert to drive uvScroll age), .zw reserved.
	pushBuf[20] = 1.0f;
	pushBuf[21] = (float)backEnd.refdef.floatTime;
	pushBuf[22] = 0.0f;
	pushBuf[23] = 0.0f;
	// stageParams (.x = stageIdx) — set per dispatch in the loop below.
	pushBuf[24] = 0.0f;
	pushBuf[25] = 0.0f;
	pushBuf[26] = 0.0f;
	pushBuf[27] = 0.0f;

	// Phase 5G: refresh the stage-counts SSBO from the host-side
	// mirror. Cheap (256 B) and decouples shader-registration timing
	// from the descriptor write — the beam pipeline always reads the
	// current count array regardless of when shaders were registered.
	if ( vk.primitive_stage_counts_mapped != NULL ) {
		uint32_t *countDst = (uint32_t *)vk.primitive_stage_counts_mapped;
		for ( i = 0; i < PRIMITIVE_SHADER_IMAGE_MAX; i++ ) {
			countDst[i] = (uint32_t)vk.primitive_shader_stage_counts[i];
		}
	}

	{
		VkViewport viewport;
		VkRect2D   scissor;
		memset( &viewport, 0, sizeof( viewport ) );
		viewport.x        = (float)backEnd.viewParms.viewportX;
		viewport.y        = (float)backEnd.viewParms.viewportY;
		viewport.width    = (float)backEnd.viewParms.viewportWidth;
		viewport.height   = (float)backEnd.viewParms.viewportHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.offset.x      = backEnd.viewParms.viewportX;
		scissor.offset.y      = backEnd.viewParms.viewportY;
		scissor.extent.width  = backEnd.viewParms.viewportWidth;
		scissor.extent.height = backEnd.viewParms.viewportHeight;
		qvkCmdSetViewport( cmd, 0, 1, &viewport );
		qvkCmdSetScissor ( cmd, 0, 1, &scissor );
	}

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.beam.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.beam.pipeline_layout, 0, 1, &vk.beam.descriptor[frameIdx], 0, NULL );

	// Phase 5G: outer loop over stage index. Each iteration is one
	// instanced draw covering all beams in the pool; the vertex
	// shader culls beams whose registered stageCount <= stageIdx
	// (their fragments never rasterize). For homogeneous-shader
	// frames (e.g. only LG primary, stageCount=2) we issue 2 real
	// dispatches and 2 culled-empty dispatches; the empty ones
	// cost only a vertex-shader degenerate write per vertex.
	//
	// All stages share the single ONE/ONE additive pipeline (the
	// only blend used by every primitive shader in the current
	// asset base — `blendfunc add`). When mixed-blend primitive
	// shaders appear, extend with a per-stage pipeline cache.
	for ( stageIdx = 0; stageIdx < PRIMITIVE_STAGE_MAX; stageIdx++ ) {
		pushBuf[24] = (float)stageIdx;
		qvkCmdPushConstants( cmd, vk.beam.pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof( pushBuf ), pushBuf );

		// 6 verts × BEAM_AXIAL_MAX slots per instance × drawCount
		// instances. Vertex shader gates axialCopies > limit AND
		// stageIdx >= stageCount to degenerate output.
		qvkCmdDraw( cmd, 6 * BEAM_AXIAL_MAX, vk.beam.drawCount, 0, 0 );
	}

	// Invalidate cached binding state (same cleanup ribbon does).
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range   = DEPTH_RANGE_COUNT;
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end   = 0;
}


/*
===============
Primitive sprite — self-contained pipeline.

Cgame submits world-space billboard sprites via RE_AddSpriteToScene.
Each call appends one GPU SpriteHeader to the per-frame headers
SSBO. RB_DrawSprites (called from RB_DrawSurfs after RB_DrawRibbons)
sorts the headers into two blend groups (alpha and additive) and
issues at most one direct vkCmdDraw per group, with vertexCount=6
and instanceCount=N, so the vertex shader can read its header via
gl_InstanceIndex.

Push range layout (vertex stage only, 96 bytes):
    bytes  0..63   mat4  mvp        — world MVP, Y-flipped for Vulkan
    bytes 64..79   vec4  viewLeft   — .xyz = camera-left in world
    bytes 80..95   vec4  viewUp     — .xyz = camera-up   in world

Descriptor set 0:
    binding 0  STORAGE_BUFFER  SpriteHeader sprites[]

Two pipeline variants (alpha and additive) selected per submission
from header.flags.
===============
*/

void vk_init_sprite( void )
{
	VkDescriptorSetLayoutBinding binds[1];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkBufferCreateInfo bufInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkDescriptorSetAllocateInfo dsAlloc;
	VkWriteDescriptorSet writes[1];
	VkDescriptorBufferInfo bufInfos[1];
	uint32_t headersBytes;
	int i;

	memset( &vk.sprite, 0, sizeof( vk.sprite ) );

	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.sprite.set_layout ) );

	memset( &pushRange, 0, sizeof( pushRange ) );
	// VERTEX | FRAGMENT: the trailing vec4 frameParams (.x =
	// tr.identityLight) is consumed by sprite.frag.
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	                     | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset     = 0;
	pushRange.size       = 112; // mat4 mvp + vec4 viewLeft + vec4 viewUp + vec4 frameParams

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount         = 1;
	pipeLayoutInfo.pSetLayouts            = &vk.sprite.set_layout;
	pipeLayoutInfo.pushConstantRangeCount = 1;
	pipeLayoutInfo.pPushConstantRanges    = &pushRange;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.sprite.pipeline_layout ) );

	headersBytes = SPRITES_PER_FRAME * SPRITE_HEADER_BYTES;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t memType;

		// headers buffer (host-coherent, mapped)
		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = headersBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.sprite.headers_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.sprite.headers_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.sprite.headers_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.sprite.headers_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.sprite.headers_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.sprite.headers_buffer[i], vk.sprite.headers_memory[i], 0 );
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.sprite.set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.sprite.descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.sprite.headers_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = headersBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.sprite.descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];

		qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );
	}

	// Two graphics pipeline variants (alpha and additive). Same state
	// as the ribbon pipeline (depthTest enabled, depthWrite off, cull
	// none, blend enabled). Only the destination blend factor differs.
	{
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineViewportStateCreateInfo viewportState;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;
		int variant;

		memset( stages, 0, sizeof( stages ) );
		stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.sprite_vs;
		stages[0].pName  = "main";
		stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.sprite_fs;
		stages[1].pName  = "main";

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates    = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width    = (float)vk.renderWidth;
		viewport.height   = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width  = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports    = &viewport;
		viewportState.scissorCount  = 1;
		viewportState.pScissors     = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth   = 1.0f;
		rasterizer.cullMode    = VK_CULL_MODE_NONE;            // two-sided
		rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = vk.msaaActive ? vkSamples : VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable  = VK_TRUE;
		depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments    = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount          = 2;
		gpInfo.pStages             = stages;
		gpInfo.pVertexInputState   = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState      = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState   = &multisampling;
		gpInfo.pDepthStencilState  = &depthStencil;
		gpInfo.pColorBlendState    = &colorBlend;
		gpInfo.pDynamicState       = &dynamicState;
		gpInfo.layout              = vk.sprite.pipeline_layout;
		gpInfo.renderPass          = vk.render_pass.main;
		gpInfo.subpass             = 0;

		// 0 = alpha (SRC_ALPHA / ONE_MINUS_SRC_ALPHA),
		// 1 = additive (SRC_ALPHA / ONE).
		for ( variant = 0; variant < 2; variant++ ) {
			memset( &blendAttach, 0, sizeof( blendAttach ) );
			blendAttach.blendEnable         = VK_TRUE;
			blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttach.dstColorBlendFactor = (variant == 0)
				? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
				: VK_BLEND_FACTOR_ONE;
			blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.srcAlphaBlendFactor = blendAttach.srcColorBlendFactor;
			blendAttach.dstAlphaBlendFactor = blendAttach.dstColorBlendFactor;
			blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL,
				(variant == 0) ? &vk.sprite.pipeline_alpha : &vk.sprite.pipeline_additive ) );
		}
	}

	vk.sprite.available = qtrue;
	ri.Log( SEV_INFO, "Vulkan sprite primitive pipeline initialized (%u headers per frame)\n",
		(unsigned)SPRITES_PER_FRAME );
}


void vk_shutdown_sprite( void )
{
	int i;

	if ( vk.sprite.pipeline_alpha != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.sprite.pipeline_alpha, NULL );
		vk.sprite.pipeline_alpha = VK_NULL_HANDLE;
	}
	if ( vk.sprite.pipeline_additive != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.sprite.pipeline_additive, NULL );
		vk.sprite.pipeline_additive = VK_NULL_HANDLE;
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.sprite.headers_buffer[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.sprite.headers_buffer[i], NULL );
			vk.sprite.headers_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.sprite.headers_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.sprite.headers_memory[i], NULL );
			vk.sprite.headers_memory[i] = VK_NULL_HANDLE;
			vk.sprite.headers_ptr[i] = NULL;
		}
	}

	if ( vk.sprite.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.sprite.pipeline_layout, NULL );
		vk.sprite.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.sprite.set_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.sprite.set_layout, NULL );
		vk.sprite.set_layout = VK_NULL_HANDLE;
	}

	vk.sprite.available = qfalse;
}


void RB_DrawSprites( void )
{
	VkCommandBuffer cmd;
	int frameIdx;
	float pushBuf[28]; // mat4 mvp + vec4 viewLeft + vec4 viewUp + vec4 frameParams
	float mvp[16];
	const float *p;
	float proj[16];
	uint32_t firstAdditive, additiveCount, alphaCount;

	// Pipelines were created against vk.render_pass.main only — skip if
	// we're being called inside the screenmap pass.
	if ( !vk.sprite.available
	  || vk.sprite.numThisFrame == 0
	  || vk.renderPassIndex != RENDER_PASS_MAIN )
		return;

	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// World MVP — same construction the standard 3D path uses
	// (get_mvp_transform): copy the projection, flip column 1
	// for Vulkan clip-space, then myGlMultMatrix( modelView, proj ).
	p = backEnd.viewParms.projectionMatrix;
	memcpy( proj, p, sizeof( proj ) );
	proj[5] = -p[5];
	myGlMultMatrix( backEnd.viewParms.world.modelMatrix, proj, mvp );

	memcpy( pushBuf,      mvp,                          64 );
	memcpy( pushBuf + 16, backEnd.viewParms.or.axis[1], sizeof( vec3_t ) );
	pushBuf[19] = 0.0f;
	memcpy( pushBuf + 20, backEnd.viewParms.or.axis[2], sizeof( vec3_t ) );
	pushBuf[23] = 0.0f;
	// frameParams: .x = tr.identityLight; .y = currentTime (sprite
	// shader doesn't currently consume frameParams.y, but the field
	// is populated for symmetry with ribbon/beam — a future textured
	// sprite consumer with UV scroll could opt in without further
	// host-side changes); .zw reserved.
	pushBuf[24] = tr.identityLight;
	pushBuf[25] = (float)backEnd.refdef.floatTime;
	pushBuf[26] = 0.0f;
	pushBuf[27] = 0.0f;

	{
		VkViewport viewport;
		VkRect2D   scissor;
		memset( &viewport, 0, sizeof( viewport ) );
		viewport.x        = (float)backEnd.viewParms.viewportX;
		viewport.y        = (float)backEnd.viewParms.viewportY;
		viewport.width    = (float)backEnd.viewParms.viewportWidth;
		viewport.height   = (float)backEnd.viewParms.viewportHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.offset.x      = backEnd.viewParms.viewportX;
		scissor.offset.y      = backEnd.viewParms.viewportY;
		scissor.extent.width  = backEnd.viewParms.viewportWidth;
		scissor.extent.height = backEnd.viewParms.viewportHeight;
		qvkCmdSetViewport( cmd, 0, 1, &viewport );
		qvkCmdSetScissor ( cmd, 0, 1, &scissor );
	}

	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.sprite.pipeline_layout, 0, 1, &vk.sprite.descriptor[frameIdx], 0, NULL );

	qvkCmdPushConstants( cmd, vk.sprite.pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( pushBuf ), pushBuf );

	// Sort headers into two contiguous groups by blend mode, in place
	// in the SSBO — alpha first, additive second. Adjacent additive
	// entries get bubbled to the tail with a single linear pass; this
	// keeps the GPU draw to at most two vkCmdDraws per frame
	// (one per blend variant) without giving up the gl_InstanceIndex
	// indexing pattern.
	//
	// Two-pointer partition: scan from start, anything additive gets
	// swapped with the next-from-end alpha. O(N) headers, all in the
	// host-coherent ring slot we own this frame.
	{
		byte *base = vk.sprite.headers_ptr[frameIdx];
		uint32_t lo = 0;
		uint32_t hi = vk.sprite.numThisFrame;
		// hdrFlagsOf: read flags field at offset 36 in a header
		while ( lo < hi ) {
			uint32_t loFlags = *(const uint32_t *)( base + lo * SPRITE_HEADER_BYTES + 36 );
			if ( ( loFlags & PRIM_FLAG_ADDITIVE ) == 0 ) {
				lo++;
				continue;
			}
			// lo is additive — find the previous alpha at hi-1 and swap
			hi--;
			if ( lo >= hi )
				break;
			{
				uint32_t hiFlags = *(const uint32_t *)( base + hi * SPRITE_HEADER_BYTES + 36 );
				if ( ( hiFlags & PRIM_FLAG_ADDITIVE ) != 0 )
					continue; // already additive at the tail; keep shrinking hi
			}
			{
				byte tmp[SPRITE_HEADER_BYTES];
				byte *aSlot = base + lo * SPRITE_HEADER_BYTES;
				byte *bSlot = base + hi * SPRITE_HEADER_BYTES;
				memcpy( tmp,   aSlot, SPRITE_HEADER_BYTES );
				memcpy( aSlot, bSlot, SPRITE_HEADER_BYTES );
				memcpy( bSlot, tmp,   SPRITE_HEADER_BYTES );
			}
			lo++;
		}
		// After the partition: [0..lo) is alpha, [lo..numThisFrame) is additive.
		alphaCount    = lo;
		firstAdditive = lo;
		additiveCount = vk.sprite.numThisFrame - lo;
	}

	if ( alphaCount > 0 ) {
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.sprite.pipeline_alpha );
		qvkCmdDraw( cmd, 6, alphaCount, 0, 0 );
	}
	if ( additiveCount > 0 ) {
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.sprite.pipeline_additive );
		qvkCmdDraw( cmd, 6, additiveCount, 0, firstAdditive );
	}

	// Invalidate cached pipeline / descriptor / depth-range so the next
	// standard draw rebinds correctly. Same cleanup pattern the IQM
	// and ribbon draws use.
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range   = DEPTH_RANGE_COUNT;
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end   = 0;
}


/*
===============
Primitive particle — compute-driven GPU pool + billboard render.

Pool: PARTICLES_PER_POOL particles in 2 ping-pong SSBOs. Each frame's
compute pass reads from one pool, integrates physics + age, writes to
the other. Render pass reads from the just-written pool.

Per-frame uniform layout (std140, 128 B):
    bytes  0..63   mat4  mvp
    bytes 64..79   vec4  viewLeft   (.xyz from backEnd.viewParms.or.axis[1])
    bytes 80..95   vec4  viewUp     (.xyz from backEnd.viewParms.or.axis[2])
    bytes 96..111  vec4  eyeWorld   (.xyz from backEnd.viewParms.or.origin)
    bytes 112..127 float dt + uint poolSize + uint numClasses + uint pingPongRead

Compute descriptor set (set 0, 4 bindings):
    binding 0  UNIFORM_BUFFER  ParticleFrame
    binding 1  STORAGE_BUFFER  Particle pool (read)
    binding 2  STORAGE_BUFFER  Particle pool (write)
    binding 3  STORAGE_BUFFER  ParticleClassGPU classes[]

Render descriptor set (set 0, 3 bindings):
    binding 0  UNIFORM_BUFFER  ParticleFrame      (same UBO)
    binding 1  STORAGE_BUFFER  Particle pool (read — post-compute)
    binding 2  STORAGE_BUFFER  ParticleClassGPU classes[]

Two graphics pipeline variants (alpha and additive). The vertex shader
reads class.renderFlags and emits a degenerate triangle for particles
that don't match the bound pipeline's blend variant. The variant flag
is supplied via specialization constant 0 (PIPELINE_BLEND_MASK).
===============
*/

void vk_init_particle( void )
{
	VkDescriptorSetLayoutBinding binds[4];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkBufferCreateInfo bufInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkDescriptorSetAllocateInfo dsAlloc;
	VkWriteDescriptorSet writes[4];
	VkDescriptorBufferInfo bufInfos[4];
	const uint32_t poolBytes    = PARTICLES_PER_POOL * PARTICLE_BYTES;
	const uint32_t classesBytes = MAX_PARTICLE_CLASSES * PARTICLE_CLASS_GPU_BYTES;
	const uint32_t frameBytes   = sizeof( particleFrame_t );
	int i;

	// Layout sanity. If a future edit drifts the C struct out of sync
	// with std430, these fire at compile time, not as silent SSBO
	// misreads.
#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
	_Static_assert( sizeof( particleClassGPU_t ) == PARTICLE_CLASS_GPU_BYTES,
		"particleClassGPU_t must be 400 bytes to match GLSL std430 stride" );
	_Static_assert( sizeof( particleGPU_t ) == PARTICLE_BYTES,
		"particleGPU_t must be 64 bytes to match GLSL std430 stride" );
	_Static_assert( sizeof( particleFrame_t ) == 144,
		"particleFrame_t must be 144 bytes to match GLSL std140 layout" );
#else
	if ( sizeof( particleClassGPU_t ) != PARTICLE_CLASS_GPU_BYTES ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"particleClassGPU_t size mismatch: C=%u, std430=%u",
			(unsigned)sizeof( particleClassGPU_t ),
			(unsigned)PARTICLE_CLASS_GPU_BYTES );
	}
	if ( sizeof( particleGPU_t ) != PARTICLE_BYTES ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"particleGPU_t size mismatch: C=%u, std430=%u",
			(unsigned)sizeof( particleGPU_t ),
			(unsigned)PARTICLE_BYTES );
	}
	if ( sizeof( particleFrame_t ) != 144 ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"particleFrame_t size mismatch: C=%u, expected=144",
			(unsigned)sizeof( particleFrame_t ) );
	}
#endif

	memset( &vk.particle, 0, sizeof( vk.particle ) );

	// ── Compute descriptor set layout (4 bindings) ───────────────
	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding         = 1;
	binds[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[2].binding         = 2;
	binds[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[3].binding         = 3;
	binds[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[3].descriptorCount = 1;
	binds[3].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 4;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.particle.compute_set_layout ) );

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount = 1;
	pipeLayoutInfo.pSetLayouts    = &vk.particle.compute_set_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.particle.compute_pipeline_layout ) );

	// ── Render descriptor set layout (4 bindings) ────────────────
	// binding 0  UBO         — ParticleFrame
	// binding 1  STORAGE     — Particle pool (post-compute output)
	// binding 2  STORAGE     — ParticleClassGPU classes shadow
	// binding 3  IMAGE_SAMPLER × MAX_PARTICLE_CLASSES — per-class
	//            texture array, indexed in fragment shader by
	//            classHandle - 1. Phase 5.
	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[1].binding         = 1;
	binds[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[2].binding         = 2;
	binds[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[3].binding         = 3;
	binds[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].descriptorCount = MAX_PARTICLE_CLASSES;
	binds[3].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[3].pImmutableSamplers = NULL;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 4;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.particle.render_set_layout ) );

	// Shared sampler for the per-class texture array. Linear filter,
	// clamp-to-edge, no anisotropy — appropriate for billboard
	// particles regardless of class. vk_find_sampler deduplicates
	// against existing samplers so this may return an already-built
	// instance.
	{
		Vk_Sampler_Def sd;
		memset( &sd, 0, sizeof( sd ) );
		sd.address_mode  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.gl_mag_filter = GL_LINEAR;
		sd.gl_min_filter = GL_LINEAR;
		sd.noAnisotropy  = qtrue;
		vk.particle.sampler = vk_find_sampler( &sd );
	}

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount = 1;
	pipeLayoutInfo.pSetLayouts    = &vk.particle.render_set_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.particle.render_pipeline_layout ) );

	// ── Particle pool buffers (ping-pong, host-coherent) ─────────
	// Phase 5P: the ping-pong index math at the readPool/writePool
	// sites assumes NUM_COMMAND_BUFFERS == 2 (cmd buffer i ⇔ pool
	// (1-i)). Bumping NUM_COMMAND_BUFFERS for triple buffering would
	// silently underflow `1 - i` for i >= 2 and OOB-index pool_buffer.
	// Catch it at compile time; the HAL refactor will abstract this
	// via frame-in-flight, after which the assert can come out.
#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
	_Static_assert( NUM_COMMAND_BUFFERS == 2,
		"Particle ping-pong assumes double-buffering. Bumping "
		"NUM_COMMAND_BUFFERS requires reworking the readPool/writePool "
		"logic and pool_buffer[N] array sizing." );
#endif
	for ( i = 0; i < 2; i++ ) {
		uint32_t memType;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = poolBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.particle.pool_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.particle.pool_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.particle.pool_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.particle.pool_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.particle.pool_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.particle.pool_buffer[i], vk.particle.pool_memory[i], 0 );

		// Initialize all slots to dead (classHandle = 0).
		memset( vk.particle.pool_ptr[i], 0, poolBytes );
	}

	// ── Class shadow SSBO (host-coherent, mapped) ────────────────
	{
		uint32_t memType;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = classesBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.particle.classes_buffer ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.particle.classes_buffer, &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.particle.classes_memory ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.particle.classes_memory, 0, VK_WHOLE_SIZE, 0, (void**)&vk.particle.classes_ptr ) );
		qvkBindBufferMemory( vk.device, vk.particle.classes_buffer, vk.particle.classes_memory, 0 );

		memset( vk.particle.classes_ptr, 0, classesBytes );
	}

	// ── Per-frame uniform buffer (host-coherent, one per cmd_index) ─
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t memType;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = frameBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.particle.frame_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.particle.frame_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.particle.frame_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.particle.frame_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.particle.frame_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.particle.frame_buffer[i], vk.particle.frame_memory[i], 0 );

		memset( vk.particle.frame_ptr[i], 0, frameBytes );
	}

	// ── Allocate compute descriptor sets (NUM_COMMAND_BUFFERS slots) ─
	// compute_descriptor[i] reads pool[i], writes pool[1-i].
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t readPool  = (uint32_t)i;            // 0 or 1
		uint32_t writePool = (uint32_t)(1 - i);      // 1 or 0

		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.particle.compute_set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.particle.compute_descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.particle.frame_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = frameBytes;
		bufInfos[1].buffer = vk.particle.pool_buffer[readPool];
		bufInfos[1].offset = 0;
		bufInfos[1].range  = poolBytes;
		bufInfos[2].buffer = vk.particle.pool_buffer[writePool];
		bufInfos[2].offset = 0;
		bufInfos[2].range  = poolBytes;
		bufInfos[3].buffer = vk.particle.classes_buffer;
		bufInfos[3].offset = 0;
		bufInfos[3].range  = classesBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.particle.compute_descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];
		writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet          = vk.particle.compute_descriptor[i];
		writes[1].dstBinding      = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[1].pBufferInfo     = &bufInfos[1];
		writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet          = vk.particle.compute_descriptor[i];
		writes[2].dstBinding      = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[2].pBufferInfo     = &bufInfos[2];
		writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet          = vk.particle.compute_descriptor[i];
		writes[3].dstBinding      = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[3].pBufferInfo     = &bufInfos[3];

		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
	}

	// ── Allocate render descriptor sets (NUM_COMMAND_BUFFERS slots) ─
	// render_descriptor[i] reads pool[1-i] (the post-compute output
	// when compute_descriptor[i] just ran).
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t renderPool = (uint32_t)(1 - i);     // 1 or 0

		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.particle.render_set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.particle.render_descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.particle.frame_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = frameBytes;
		bufInfos[1].buffer = vk.particle.pool_buffer[renderPool];
		bufInfos[1].offset = 0;
		bufInfos[1].range  = poolBytes;
		bufInfos[2].buffer = vk.particle.classes_buffer;
		bufInfos[2].offset = 0;
		bufInfos[2].range  = classesBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.particle.render_descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];
		writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet          = vk.particle.render_descriptor[i];
		writes[1].dstBinding      = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[1].pBufferInfo     = &bufInfos[1];
		writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet          = vk.particle.render_descriptor[i];
		writes[2].dstBinding      = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[2].pBufferInfo     = &bufInfos[2];

		qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

		// Note on binding 3 (per-class sampler array): population is
		// deferred. vk_init_particle runs from InitOpenGL →
		// vk_initialize, BEFORE R_InitImages creates tr.whiteImage.
		// R_Init calls vk_init_particle_textures AFTER R_InitImages
		// to populate this binding eagerly against a valid
		// tr.whiteImage.
	}

	// ── Compute pipeline ──────────────────────────────────────────
	{
		VkComputePipelineCreateInfo cpInfo;
		memset( &cpInfo, 0, sizeof( cpInfo ) );
		cpInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		cpInfo.layout = vk.particle.compute_pipeline_layout;
		cpInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		cpInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
		cpInfo.stage.module = vk.modules.particle_integrate_cs;
		cpInfo.stage.pName  = "main";
		VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &cpInfo, NULL, &vk.particle.compute_pipeline ) );
	}

	// ── Render pipelines (alpha + additive variants) ──────────────
	{
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineViewportStateCreateInfo viewportState;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;
		VkSpecializationMapEntry specMap;
		VkSpecializationInfo specInfo;
		uint32_t blendMaskValue;
		int variant;

		// Specialization: vertex stage gets PIPELINE_BLEND_MASK = 0
		// (alpha) or 1 (additive). Filled per-variant inside the loop.
		memset( &specMap, 0, sizeof( specMap ) );
		specMap.constantID = 0;
		specMap.offset     = 0;
		specMap.size       = sizeof( uint32_t );
		memset( &specInfo, 0, sizeof( specInfo ) );
		specInfo.mapEntryCount = 1;
		specInfo.pMapEntries   = &specMap;
		specInfo.dataSize      = sizeof( uint32_t );
		specInfo.pData         = &blendMaskValue;

		memset( stages, 0, sizeof( stages ) );
		stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.particle_vs;
		stages[0].pName  = "main";
		stages[0].pSpecializationInfo = &specInfo;
		stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.particle_fs;
		stages[1].pName  = "main";

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates    = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width    = (float)vk.renderWidth;
		viewport.height   = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width  = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports    = &viewport;
		viewportState.scissorCount  = 1;
		viewportState.pScissors     = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth   = 1.0f;
		rasterizer.cullMode    = VK_CULL_MODE_NONE;
		rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = vk.msaaActive ? vkSamples : VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable  = VK_TRUE;
		depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments    = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount          = 2;
		gpInfo.pStages             = stages;
		gpInfo.pVertexInputState   = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState      = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState   = &multisampling;
		gpInfo.pDepthStencilState  = &depthStencil;
		gpInfo.pColorBlendState    = &colorBlend;
		gpInfo.pDynamicState       = &dynamicState;
		gpInfo.layout              = vk.particle.render_pipeline_layout;
		gpInfo.renderPass          = vk.render_pass.main;
		gpInfo.subpass             = 0;

		// 0 = alpha (SRC_ALPHA / ONE_MINUS_SRC_ALPHA),
		// 1 = additive (SRC_ALPHA / ONE).
		for ( variant = 0; variant < 2; variant++ ) {
			blendMaskValue = (uint32_t)variant;

			memset( &blendAttach, 0, sizeof( blendAttach ) );
			blendAttach.blendEnable         = VK_TRUE;
			blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttach.dstColorBlendFactor = (variant == 0)
				? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
				: VK_BLEND_FACTOR_ONE;
			blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.srcAlphaBlendFactor = blendAttach.srcColorBlendFactor;
			blendAttach.dstAlphaBlendFactor = blendAttach.dstColorBlendFactor;
			blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL,
				(variant == 0) ? &vk.particle.render_pipeline_alpha : &vk.particle.render_pipeline_additive ) );
		}
	}

	vk.particle.pingPongRead  = 0;
	vk.particle.prevSceneTime = 0.0f;
	vk.particle.numClasses    = 0;
	vk.particle.nextSlot      = 0;
	vk.particle.available     = qtrue;

	ri.Log( SEV_INFO, "Vulkan particle subsystem initialized (pool=%u particles, classes=%u, %u KB pool memory)\n",
		(unsigned)PARTICLES_PER_POOL,
		(unsigned)MAX_PARTICLE_CLASSES,
		(unsigned)( ( poolBytes * 2 + classesBytes ) / 1024 ) );
}


// Phase 5 eager texture init: populate the per-class sampler array
// (binding 3) on every per-frame render descriptor set. Walks
// classImages[] (mostly NULL at engine init) and falls back to
// tr.whiteImage for unregistered slots. Called once from R_Init
// AFTER R_InitImages has created tr.whiteImage; cannot run earlier
// because vk_init_particle and vk_init_descriptors both execute
// from InitOpenGL, before R_InitImages.
//
// On vid_restart, vk_init_descriptors's particle re-alloc clears
// classImages[] and numClasses; R_InitImages re-creates
// tr.whiteImage at a fresh address; this function then re-runs
// from the same R_Init call site to repopulate against the fresh
// pointers. RE_RegisterParticleClass overwrites per-class slots
// with their resolved shader images later, when cgame loads.
//
// The fatal-on-NULL guard converts a wrong-phase call into a clear
// startup failure rather than a silent unbound-texture issue.
void vk_init_particle_textures( void )
{
	VkDescriptorImageInfo imgInfos[MAX_PARTICLE_CLASSES];
	VkWriteDescriptorSet  imgWrite;
	uint32_t j, k;

	if ( !vk.particle.available ) return;
	if ( tr.whiteImage == NULL ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"vk_init_particle_textures: tr.whiteImage is NULL — "
			"called before R_InitImages?" );
		return;
	}

	for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
		for ( k = 0; k < MAX_PARTICLE_CLASSES; k++ ) {
			image_t *img = vk.particle.classImages[k]
			             ? vk.particle.classImages[k]
			             : tr.whiteImage;
			memset( &imgInfos[k], 0, sizeof( imgInfos[k] ) );
			imgInfos[k].imageView   = img->view;
			imgInfos[k].sampler     = vk.particle.sampler;
			imgInfos[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		memset( &imgWrite, 0, sizeof( imgWrite ) );
		imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imgWrite.dstSet          = vk.particle.render_descriptor[j];
		imgWrite.dstBinding      = 3;
		imgWrite.dstArrayElement = 0;
		imgWrite.descriptorCount = MAX_PARTICLE_CLASSES;
		imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imgWrite.pImageInfo      = imgInfos;
		qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
	}
}


void vk_particle_set_class_image( int handle, image_t *image )
{
	VkDescriptorImageInfo imgInfo;
	VkWriteDescriptorSet  imgWrite;
	uint32_t              j;

	// Phase 5: encapsulate the per-class sampler-array slot update
	// so RE_RegisterParticleClass (in tr_scene.c) doesn't need
	// access to the static qvk* function pointers in this TU.
	// Writes the same (image->view, vk.particle.sampler) pair into
	// slot (handle - 1) of binding 3 on every per-frame render
	// descriptor set. The other 63 slots were populated with
	// tr.whiteImage by vk_init_particle_textures (called from
	// R_Init after R_InitImages); this function only touches the
	// registered class's slot.
	if ( !vk.particle.available ) return;
	if ( handle < 1 || handle > MAX_PARTICLE_CLASSES ) return;
	if ( image == NULL ) return;

	memset( &imgInfo, 0, sizeof( imgInfo ) );
	imgInfo.imageView   = image->view;
	imgInfo.sampler     = vk.particle.sampler;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
		memset( &imgWrite, 0, sizeof( imgWrite ) );
		imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imgWrite.dstSet          = vk.particle.render_descriptor[j];
		imgWrite.dstBinding      = 3;
		imgWrite.dstArrayElement = (uint32_t)( handle - 1 );
		imgWrite.descriptorCount = 1;
		imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imgWrite.pImageInfo      = &imgInfo;
		qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
	}
}


// ── shared primitive shader image registry ──────────────────────────
// One global registry, indexed by qhandle_t. Today consumed only by
// ribbon (descriptor binding 2). Future beam pipeline will add itself
// to vk_register_primitive_shader_image's update broadcast in turn 5B.
// See vk.h for the registry contract.
image_t *vk_primitive_shader_images[PRIMITIVE_SHADER_IMAGE_MAX];


/*
================
vk_init_primitive_shader_images

Eager init of the primitive shader image registry + ribbon's binding-2
sampler array. Called from R_Init AFTER R_InitImages creates
tr.whiteImage; cannot run earlier because the descriptor write needs
a valid image_t. Pattern mirrors vk_init_particle_textures: walk all
NUM_COMMAND_BUFFERS descriptor sets and populate every slot with
either the registered image or tr.whiteImage as fallback.

Re-runs on every R_Init, so vid_restart correctly repopulates against
the freshly-recreated tr.whiteImage and any previously-registered
image pointers (those become dangling after R_DeleteTextures and must
be reset; this function clears the registry to tr.whiteImage as the
fresh-start state, and any consumer that wants a textured shader
re-registers via RE_RegisterPrimitiveShader).
================
*/
static void vk_init_primitive_shader_stages( void );

void vk_init_primitive_shader_images( void )
{
	int i;

	if ( tr.whiteImage == NULL ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"vk_init_primitive_shader_images: tr.whiteImage is NULL — "
			"called before R_InitImages?" );
		return;
	}

	// Reset the registry to a known fresh-start state. Any
	// previously-cached pointers from before vid_restart are now
	// dangling. Per-shader registrations re-run from cgame init
	// after R_Init returns and overwrite the slots they care about.
	for ( i = 0; i < PRIMITIVE_SHADER_IMAGE_MAX; i++ ) {
		vk_primitive_shader_images[i] = tr.whiteImage;
	}

	// Phase 5F: allocate (first call) and re-zero (every call) the
	// per-stage SSBO. cgame re-registers shaders post-vid_restart
	// via RE_RegisterPrimitiveShader, which re-populates entries.
	vk_init_primitive_shader_stages();

	// Sampler reuse: vk.particle.sampler is a shared linear/clamp/
	// no-anisotropy sampler created in vk_init_particle. Used by
	// ribbon binding 2 (helix has uvScroll=0 so CLAMP_TO_EDGE is
	// fine). Beam binding 1 uses vk.beam.sampler_repeat instead —
	// see Phase 5J: scrolling lightning shaders need REPEAT to
	// wrap large UVs without saturating to the texture edge.
	{
		VkDescriptorImageInfo imgInfosClamp[PRIMITIVE_SHADER_IMAGE_MAX];
		VkDescriptorImageInfo imgInfosRepeat[PRIMITIVE_SHADER_IMAGE_MAX];
		VkWriteDescriptorSet  imgWrite;
		uint32_t              j, k;

		// Build two parallel imageInfo arrays — same images, two
		// samplers. Cheap (each is 16 entries × NUM_COMMAND_BUFFERS
		// frames; rebuilt once per init/vid_restart, not per frame).
		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			for ( k = 0; k < PRIMITIVE_SHADER_IMAGE_MAX; k++ ) {
				image_t *img = vk_primitive_shader_images[k];
				memset( &imgInfosClamp[k], 0, sizeof( imgInfosClamp[k] ) );
				imgInfosClamp[k].imageView   = img->view;
				imgInfosClamp[k].sampler     = vk.particle.sampler;
				imgInfosClamp[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				memset( &imgInfosRepeat[k], 0, sizeof( imgInfosRepeat[k] ) );
				imgInfosRepeat[k].imageView   = img->view;
				imgInfosRepeat[k].sampler     = vk.beam.sampler_repeat;
				imgInfosRepeat[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			if ( vk.ribbon.available ) {
				memset( &imgWrite, 0, sizeof( imgWrite ) );
				imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				imgWrite.dstSet          = vk.ribbon.descriptor[j];
				imgWrite.dstBinding      = 2;
				imgWrite.dstArrayElement = 0;
				imgWrite.descriptorCount = PRIMITIVE_SHADER_IMAGE_MAX;
				imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				imgWrite.pImageInfo      = imgInfosClamp;
				qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
			}

			if ( vk.beam.available ) {
				memset( &imgWrite, 0, sizeof( imgWrite ) );
				imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				imgWrite.dstSet          = vk.beam.descriptor[j];
				imgWrite.dstBinding      = 1;
				imgWrite.dstArrayElement = 0;
				imgWrite.descriptorCount = PRIMITIVE_SHADER_IMAGE_MAX;
				imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				imgWrite.pImageInfo      = imgInfosRepeat;
				qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
			}

			// Phase 5G: bind the multi-stage SSBOs to the beam set's
			// bindings 2 and 3. vk_init_primitive_shader_stages
			// (called above) just allocated these buffers, so this
			// is the earliest time the writes are valid. On vid_restart
			// the descriptor pool reset has invalidated the previous
			// writes; re-emitting here keeps the set healthy.
			if ( vk.beam.available
			  && vk.primitive_stages_buffer != VK_NULL_HANDLE ) {
				VkDescriptorBufferInfo bufInfos[2];
				VkWriteDescriptorSet   bufWrites[2];

				memset( bufInfos, 0, sizeof( bufInfos ) );
				bufInfos[0].buffer = vk.primitive_stages_buffer;
				bufInfos[0].offset = 0;
				bufInfos[0].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
					* PRIMITIVE_STAGE_MAX * VK_PRIMITIVE_STAGE_BYTES;

				bufInfos[1].buffer = vk.primitive_stage_counts_buffer;
				bufInfos[1].offset = 0;
				bufInfos[1].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
					* sizeof( uint32_t );

				memset( bufWrites, 0, sizeof( bufWrites ) );
				bufWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				bufWrites[0].dstSet          = vk.beam.descriptor[j];
				bufWrites[0].dstBinding      = 2;
				bufWrites[0].descriptorCount = 1;
				bufWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				bufWrites[0].pBufferInfo     = &bufInfos[0];

				bufWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				bufWrites[1].dstSet          = vk.beam.descriptor[j];
				bufWrites[1].dstBinding      = 3;
				bufWrites[1].descriptorCount = 1;
				bufWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				bufWrites[1].pBufferInfo     = &bufInfos[1];

				qvkUpdateDescriptorSets( vk.device, 2, bufWrites, 0, NULL );
			}
		}
	}
}


/*
================
vk_register_primitive_shader_image

Idempotently register an image_t at registry slot `handle` and push
the change to every primitive descriptor set that consumes the
registry. Today: ribbon only. Beam (turn 5B) will append a second
update call here.

Out-of-range handles (>= PRIMITIVE_SHADER_IMAGE_MAX) are silently
ignored — the shader-side slot clamp at the fragment shader
(`slot = handle < 64u ? handle : 0u`) renders such submissions
through slot 0 (tr.whiteImage), so they appear untextured rather
than crashing.
================
*/
void vk_register_primitive_shader_image( int slot, image_t *image )
{
	VkDescriptorImageInfo imgInfoClamp;
	VkDescriptorImageInfo imgInfoRepeat;
	VkWriteDescriptorSet  imgWrite;
	uint32_t              j;

	// Phase 5K: parameter is now a primitive registry SLOT, not a qhandle.
	// Slot 0 reserved for tr.whiteImage; usable slots [1, PRIMITIVE_SHADER_IMAGE_MAX).
	if ( slot <= 0 || slot >= PRIMITIVE_SHADER_IMAGE_MAX ) return;
	if ( image == NULL ) return;
	if ( vk_primitive_shader_images[slot] == image ) return; // idempotent

	vk_primitive_shader_images[slot] = image;

	// Phase 5J: ribbon binding 2 uses vk.particle.sampler (CLAMP_TO_EDGE);
	// beam binding 1 uses vk.beam.sampler_repeat (REPEAT). Same image,
	// different sampler.
	memset( &imgInfoClamp, 0, sizeof( imgInfoClamp ) );
	imgInfoClamp.imageView   = image->view;
	imgInfoClamp.sampler     = vk.particle.sampler;
	imgInfoClamp.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	memset( &imgInfoRepeat, 0, sizeof( imgInfoRepeat ) );
	imgInfoRepeat.imageView   = image->view;
	imgInfoRepeat.sampler     = vk.beam.sampler_repeat;
	imgInfoRepeat.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
		if ( vk.ribbon.available ) {
			memset( &imgWrite, 0, sizeof( imgWrite ) );
			imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			imgWrite.dstSet          = vk.ribbon.descriptor[j];
			imgWrite.dstBinding      = 2;
			imgWrite.dstArrayElement = (uint32_t)slot;
			imgWrite.descriptorCount = 1;
			imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			imgWrite.pImageInfo      = &imgInfoClamp;
			qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
		}
		if ( vk.beam.available ) {
			memset( &imgWrite, 0, sizeof( imgWrite ) );
			imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			imgWrite.dstSet          = vk.beam.descriptor[j];
			imgWrite.dstBinding      = 1;
			imgWrite.dstArrayElement = (uint32_t)slot;
			imgWrite.descriptorCount = 1;
			imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			imgWrite.pImageInfo      = &imgInfoRepeat;
			qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
		}
	}
}


/*
================
vk_alloc_primitive_shader_image_slot

Phase 5F support: allocate (or reuse) a registry slot for a stage>0
image whose qhandle slot is already taken by stage 0's image of the
same shader. Linear scan returns an existing slot when the image is
already registered; otherwise writes into the first slot still
holding tr.whiteImage and broadcasts the descriptor write via
vk_register_primitive_shader_image.

Slots 0 (reserved for tr.whiteImage) and the qhandle-occupied slots
1..N are skipped during the free-slot search. Returns -1 if no free
slot remains.

The collision case (a future shader's qhandle landing on a slot
allocated here for a stage>0 image) is documented but unhandled in
this turn — most multi-stage shaders reuse the same image across
stages (the LG case), so the linear-scan reuse path covers them
without needing a new slot.
================
*/
int vk_alloc_primitive_shader_image_slot( image_t *image )
{
	int i;

	if ( image == NULL ) return -1;

	// Reuse path: if `image` is already registered anywhere, return
	// its slot. Includes slot 0 (tr.whiteImage) — but caller filters
	// that case before getting here for non-trivial images.
	for ( i = 1; i < PRIMITIVE_SHADER_IMAGE_MAX; i++ ) {
		if ( vk_primitive_shader_images[i] == image ) {
			return i;
		}
	}

	// Allocate path: scan from the end backward to minimize collision
	// with future qhandle assignments (which start low). First slot
	// still holding tr.whiteImage wins.
	for ( i = PRIMITIVE_SHADER_IMAGE_MAX - 1; i >= 1; i-- ) {
		if ( vk_primitive_shader_images[i] == tr.whiteImage ) {
			vk_register_primitive_shader_image( i, image );
			return i;
		}
	}

	return -1; // exhausted
}


/*
================
vk_qhandle_to_prim_slot

Phase 5K: translate a cgame qhandle into the engine-internal primitive
registry slot used by GPU SSBOs. Reads the indirection table populated
by RE_RegisterPrimitiveShader. Used at SSBO write sites to pack a
small (≤63) slot index into the GPU header where the previous design
had relied on (qhandle == slot), an assumption that broke as soon as
the qhandle counter exceeded PRIMITIVE_SHADER_IMAGE_MAX.

Out-of-range or unregistered qhandles return slot 0 (whiteImage),
rendering as untextured rather than producing OOB SSBO reads on the
GPU.
================
*/
unsigned int vk_qhandle_to_prim_slot( qhandle_t h )
{
	uint8_t slot;

	if ( h <= 0 || h >= VK_PRIM_QHANDLE_MAX ) return 0;

	slot = vk.qhandle_to_prim_slot[h];
	if ( slot == PRIMITIVE_SLOT_INVALID ) return 0;

	return (unsigned int)slot;
}


/*
================
vk_init_primitive_shader_stages

Phase 5F: lazy allocation of the per-stage SSBO that backs multi-
stage primitive shaders. Buffer is allocated on first call and
persists across vid_restart (it lives outside the descriptor pool,
so the pool reset doesn't invalidate it). Contents are zeroed on
every call so vid_restart sees a clean slate before cgame
re-registers shaders via RE_RegisterPrimitiveShader.

The buffer holds PRIMITIVE_SHADER_IMAGE_MAX × PRIMITIVE_STAGE_MAX
entries of VK_PRIMITIVE_STAGE_BYTES each = 64 × 4 × 32 = 8 KB.
Indexed [shaderHandle * PRIMITIVE_STAGE_MAX + stageNumber].

Bound to the beam pipeline by Phase 5G (this turn allocates only).
================
*/
static void vk_init_primitive_shader_stages( void )
{
	const VkDeviceSize stagesSize = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
		* PRIMITIVE_STAGE_MAX * VK_PRIMITIVE_STAGE_BYTES;
	const VkDeviceSize countsSize = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
		* sizeof( uint32_t );

	if ( vk.primitive_stages_buffer == VK_NULL_HANDLE ) {
		VkBufferCreateInfo bufInfo;
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo allocInfo;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = stagesSize;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL,
			&vk.primitive_stages_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device,
			vk.primitive_stages_buffer, &memReqs );

		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL,
			&vk.primitive_stages_memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.primitive_stages_buffer,
			vk.primitive_stages_memory, 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.primitive_stages_memory,
			0, stagesSize, 0, &vk.primitive_stages_mapped ) );
	}

	// Phase 5G: stage-counts companion buffer. 256 B; uploaded each
	// frame in RB_DrawBeams from the host-side count array. Decoupled
	// from the stages buffer so the vertex shader can cheaply cull
	// per-stage draws without reading every stage entry.
	if ( vk.primitive_stage_counts_buffer == VK_NULL_HANDLE ) {
		VkBufferCreateInfo bufInfo;
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo allocInfo;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = countsSize;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL,
			&vk.primitive_stage_counts_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device,
			vk.primitive_stage_counts_buffer, &memReqs );

		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL,
			&vk.primitive_stage_counts_memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.primitive_stage_counts_buffer,
			vk.primitive_stage_counts_memory, 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.primitive_stage_counts_memory,
			0, countsSize, 0, &vk.primitive_stage_counts_mapped ) );
	}

	// Zero-init: every call (vid_restart included). Trailing stages
	// (stageNumber >= stageCount[handle]) stay zero; consumers gate
	// per-stage draws on the count array.
	if ( vk.primitive_stages_mapped != NULL ) {
		memset( vk.primitive_stages_mapped, 0, (size_t)stagesSize );
	}
	if ( vk.primitive_stage_counts_mapped != NULL ) {
		memset( vk.primitive_stage_counts_mapped, 0, (size_t)countsSize );
	}
	memset( vk.primitive_shader_stage_counts, 0,
		sizeof( vk.primitive_shader_stage_counts ) );

	// Phase 5K: reset qhandle→primitive-slot lookup. 0xFF byte fill
	// means every entry is PRIMITIVE_SLOT_INVALID. Re-runs cleanly
	// on vid_restart; cgame re-registers primitive shaders post-restart
	// and repopulates entries through the slot allocator.
	memset( vk.qhandle_to_prim_slot, PRIMITIVE_SLOT_INVALID,
		sizeof( vk.qhandle_to_prim_slot ) );
}


/*
================
vk_shutdown_primitive_stages

Phase 5F: destroy the per-stage SSBO. Called from vk_shutdown only
(NOT from vk_release_resources / vid_restart — buffer survives
those because it's outside the descriptor pool).
================
*/
void vk_shutdown_primitive_stages( void )
{
	if ( vk.primitive_stages_buffer != VK_NULL_HANDLE ) {
		if ( vk.primitive_stages_mapped != NULL ) {
			qvkUnmapMemory( vk.device, vk.primitive_stages_memory );
			vk.primitive_stages_mapped = NULL;
		}
		qvkDestroyBuffer( vk.device, vk.primitive_stages_buffer, NULL );
		vk.primitive_stages_buffer = VK_NULL_HANDLE;
	}
	if ( vk.primitive_stages_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.primitive_stages_memory, NULL );
		vk.primitive_stages_memory = VK_NULL_HANDLE;
	}
	if ( vk.primitive_stage_counts_buffer != VK_NULL_HANDLE ) {
		if ( vk.primitive_stage_counts_mapped != NULL ) {
			qvkUnmapMemory( vk.device, vk.primitive_stage_counts_memory );
			vk.primitive_stage_counts_mapped = NULL;
		}
		qvkDestroyBuffer( vk.device, vk.primitive_stage_counts_buffer, NULL );
		vk.primitive_stage_counts_buffer = VK_NULL_HANDLE;
	}
	if ( vk.primitive_stage_counts_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.primitive_stage_counts_memory, NULL );
		vk.primitive_stage_counts_memory = VK_NULL_HANDLE;
	}
	memset( vk.primitive_shader_stage_counts, 0,
		sizeof( vk.primitive_shader_stage_counts ) );
}


void vk_shutdown_particle( void )
{
	int i;

	if ( vk.particle.render_pipeline_alpha != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.particle.render_pipeline_alpha, NULL );
		vk.particle.render_pipeline_alpha = VK_NULL_HANDLE;
	}
	if ( vk.particle.render_pipeline_additive != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.particle.render_pipeline_additive, NULL );
		vk.particle.render_pipeline_additive = VK_NULL_HANDLE;
	}
	if ( vk.particle.compute_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.particle.compute_pipeline, NULL );
		vk.particle.compute_pipeline = VK_NULL_HANDLE;
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.particle.frame_buffer[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.particle.frame_buffer[i], NULL );
			vk.particle.frame_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.particle.frame_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.particle.frame_memory[i], NULL );
			vk.particle.frame_memory[i] = VK_NULL_HANDLE;
			vk.particle.frame_ptr[i] = NULL;
		}
	}

	if ( vk.particle.classes_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.particle.classes_buffer, NULL );
		vk.particle.classes_buffer = VK_NULL_HANDLE;
	}
	if ( vk.particle.classes_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.particle.classes_memory, NULL );
		vk.particle.classes_memory = VK_NULL_HANDLE;
		vk.particle.classes_ptr = NULL;
	}

	for ( i = 0; i < 2; i++ ) {
		if ( vk.particle.pool_buffer[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.particle.pool_buffer[i], NULL );
			vk.particle.pool_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.particle.pool_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.particle.pool_memory[i], NULL );
			vk.particle.pool_memory[i] = VK_NULL_HANDLE;
			vk.particle.pool_ptr[i] = NULL;
		}
	}

	if ( vk.particle.render_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.particle.render_pipeline_layout, NULL );
		vk.particle.render_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.particle.compute_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.particle.compute_pipeline_layout, NULL );
		vk.particle.compute_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.particle.render_set_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.particle.render_set_layout, NULL );
		vk.particle.render_set_layout = VK_NULL_HANDLE;
	}
	if ( vk.particle.compute_set_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.particle.compute_set_layout, NULL );
		vk.particle.compute_set_layout = VK_NULL_HANDLE;
	}

	vk.particle.available = qfalse;
}


void RB_RunParticleCompute( void )
{
	VkCommandBuffer cmd;
	int frameIdx;
	float currentSceneTime, dt;
	particleFrame_t *frameDst;
	VkBufferMemoryBarrier barrier;
	uint32_t pingRead;

	if ( !vk.particle.available )
		return;

	// IMPORTANT: this function is called from vk_begin_frame, BEFORE
	// the main render pass opens. vkCmdDispatch is spec-forbidden
	// inside a render pass instance (VUID-vkCmdDispatch-renderpass);
	// keeping the call out here is the entire point of phase 2's
	// Hypothesis-A fix. Do NOT call this from inside RB_DrawSurfs.
	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// backEnd.refdef.floatTime here holds the PREVIOUS frame's value
	// — refdef isn't updated for the current frame until
	// RB_BeginDrawingView runs (later, inside the main render pass).
	// The dt computed here is therefore the delta from frame N-2 to
	// frame N-1, applied to particles being integrated for frame N's
	// render. One frame of integration latency. Acceptable for
	// particles; not a bug; do not "fix" by reading time elsewhere.
	//
	// dt clamp: covers map-change resets (negative delta, clamp to 0)
	// and long pauses / breakpoints (huge delta, clamp to 100ms so
	// particles skip a frame instead of teleporting).
	currentSceneTime = (float)backEnd.refdef.floatTime;
	dt = currentSceneTime - vk.particle.prevSceneTime;
	if ( dt < 0.0f ) dt = 0.0f;
	if ( dt > 0.1f ) dt = 0.1f;
	vk.particle.prevSceneTime = currentSceneTime;

	// Update the COMPUTE region of the per-frame UBO (bytes 112..127)
	// and the SHARED region (bytes 128..143). The render region
	// (bytes 0..111: mvp, viewLeft, viewUp, eyeWorld) is filled later
	// by RB_DrawParticles when backEnd.viewParms is valid. Field-by-
	// field assignment leaves the render region untouched.
	//
	// identityLight tracks tr.identityLight = 1/2^overbrightBits.
	// r_brightness is in CVG_RENDERER, so a runtime change runs
	// R_SetColorMappings via tr_cmds.c and rebakes the gamma
	// pipeline; the next frame's UBO write picks up the new value
	// here without further coordination.
	frameDst = (particleFrame_t *)vk.particle.frame_ptr[frameIdx];
	frameDst->dt            = dt;
	frameDst->poolSize      = PARTICLES_PER_POOL;
	frameDst->numClasses    = vk.particle.numClasses;
	frameDst->pingPongRead  = vk.particle.pingPongRead;
	frameDst->identityLight = tr.identityLight;

	pingRead = vk.particle.pingPongRead;

	// Bind compute pipeline + descriptor set, dispatch.
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.particle.compute_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.particle.compute_pipeline_layout, 0, 1,
		&vk.particle.compute_descriptor[pingRead], 0, NULL );

	// Dispatch ceil(PARTICLES_PER_POOL / 64). Workgroup size = 64.
	qvkCmdDispatch( cmd, ( PARTICLES_PER_POOL + 63 ) / 64, 1, 1 );

	// Barrier: compute writes to pool[1-pingRead] must be visible to
	// the next vertex shader read in RB_DrawParticles. Now outside any
	// render pass, so no self-dependency machinery is needed.
	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer              = vk.particle.pool_buffer[1 - pingRead];
	barrier.offset              = 0;
	barrier.size                = VK_WHOLE_SIZE;

	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		0,
		0, NULL,
		1, &barrier,
		0, NULL );

	// Flip ping-pong for next frame.
	vk.particle.pingPongRead = 1 - vk.particle.pingPongRead;
}


void RB_DrawParticles( void )
{
	VkCommandBuffer cmd;
	int frameIdx;
	uint32_t renderIdx;
	particleFrame_t *frameDst;
	const float *p;
	float proj[16];
	float mvp[16];

	if ( !vk.particle.available
	  || vk.renderPassIndex != RENDER_PASS_MAIN )
		return;

	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// Update the RENDER region of the per-frame UBO (bytes 0..111).
	// The compute region (bytes 112..127) was filled earlier in
	// RB_RunParticleCompute (called from vk_begin_frame). Field-by-
	// field assignment leaves the compute region untouched.
	//
	// MVP construction — same pattern as ribbon / sprite (proj y-flip
	// for Vulkan clip space, world.modelMatrix already has the q3
	// z-up → gl y-up flip baked in).
	p = backEnd.viewParms.projectionMatrix;
	memcpy( proj, p, sizeof( proj ) );
	proj[5] = -p[5];
	myGlMultMatrix( backEnd.viewParms.world.modelMatrix, proj, mvp );

	frameDst = (particleFrame_t *)vk.particle.frame_ptr[frameIdx];
	memcpy( frameDst->mvp,      mvp,                          64 );
	memcpy( frameDst->viewLeft, backEnd.viewParms.or.axis[1], sizeof( vec3_t ) );
	frameDst->viewLeft[3] = 0.0f;
	memcpy( frameDst->viewUp,   backEnd.viewParms.or.axis[2], sizeof( vec3_t ) );
	frameDst->viewUp[3] = 0.0f;
	memcpy( frameDst->eyeWorld, backEnd.viewParms.or.origin,  sizeof( vec3_t ) );
	frameDst->eyeWorld[3] = 0.0f;

	// renderIdx selects the descriptor set whose particle pool binding
	// is the post-compute output. compute_descriptor[i] writes pool[1-i],
	// so render_descriptor[i] reads pool[1-i] — index i matches.
	// RB_RunParticleCompute flipped pingPongRead at its tail, so the
	// PRE-flip value (= the index used by compute this frame) is
	// (vk.particle.pingPongRead ^ 1).
	renderIdx = vk.particle.pingPongRead ^ 1u;

	{
		VkViewport viewport;
		VkRect2D   scissor;
		memset( &viewport, 0, sizeof( viewport ) );
		viewport.x        = (float)backEnd.viewParms.viewportX;
		viewport.y        = (float)backEnd.viewParms.viewportY;
		viewport.width    = (float)backEnd.viewParms.viewportWidth;
		viewport.height   = (float)backEnd.viewParms.viewportHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.offset.x      = backEnd.viewParms.viewportX;
		scissor.offset.y      = backEnd.viewParms.viewportY;
		scissor.extent.width  = backEnd.viewParms.viewportWidth;
		scissor.extent.height = backEnd.viewParms.viewportHeight;
		qvkCmdSetViewport( cmd, 0, 1, &viewport );
		qvkCmdSetScissor ( cmd, 0, 1, &scissor );
	}

	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.particle.render_pipeline_layout, 0, 1,
		&vk.particle.render_descriptor[renderIdx], 0, NULL );

	// Two passes: alpha-blend particles, then additive. Each draw
	// dispatches the full pool (16K instances of 6 vertices); the
	// vertex shader emits degenerate triangles for slots whose
	// blend variant doesn't match the bound pipeline (and for dead
	// slots). 16K of small fully-culled quads is cheap on the GPU.
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.particle.render_pipeline_alpha );
	qvkCmdDraw( cmd, 6, PARTICLES_PER_POOL, 0, 0 );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.particle.render_pipeline_additive );
	qvkCmdDraw( cmd, 6, PARTICLES_PER_POOL, 0, 0 );

	// Invalidate cached pipeline / descriptor / depth-range so the
	// next standard draw rebinds correctly. Same cleanup pattern the
	// IQM, ribbon, and sprite draws use.
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range   = DEPTH_RANGE_COUNT;
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end   = 0;
}


#if FEAT_IQM
/*
===============
IQM GPU skinning — self-contained pipeline for skeletal IQM models
===============
*/

/*
===============
vk_init_iqm_gpu_skinning — create descriptor set layout, pipeline layout,
bone UBOs, descriptors, and the graphics pipeline
===============
*/
void vk_init_iqm_gpu_skinning( void )
{
	VkDescriptorSetLayoutBinding binds[1];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkDescriptorSetLayout setLayouts[2];
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkBufferCreateInfo bufInfo;
	VkDescriptorSetAllocateInfo allocDesc;
	VkDescriptorBufferInfo bufDesc;
	VkWriteDescriptorSet writeDesc;

	vk.iqmGpu.available = qfalse;

	// descriptor set layout for bone matrices (set 0, binding 0, UBO)
	memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	binds[0].pImmutableSamplers = NULL;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = binds;

	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.iqmGpu.set_layout_bones ) );

	// pipeline layout: push constants (MVP 64 bytes) + set 0 (bones) + set 1 (texture sampler)
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushRange.offset = 0;
	pushRange.size = 64; // mat4 MVP

	setLayouts[0] = vk.iqmGpu.set_layout_bones;
	setLayouts[1] = vk.set_layout_sampler;

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount = 2;
	pipeLayoutInfo.pSetLayouts = setLayouts;
	pipeLayoutInfo.pushConstantRangeCount = 1;
	pipeLayoutInfo.pPushConstantRanges = &pushRange;

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.iqmGpu.pipeline_layout ) );

	// per-frame bone UBOs (host-visible, persistently mapped)
	memset( &bufInfo, 0, sizeof( bufInfo ) );
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.size = IQM_BONE_UBO_SIZE;
	bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.iqmGpu.bone_buffer[i] ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.iqmGpu.bone_buffer[i], &memReqs );

		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.iqmGpu.bone_memory[i] ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.iqmGpu.bone_buffer[i], vk.iqmGpu.bone_memory[i], 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.iqmGpu.bone_memory[i], 0, IQM_BONE_UBO_SIZE, 0, (void **)&vk.iqmGpu.bone_ptr[i] ) );

		// allocate descriptor set
		memset( &allocDesc, 0, sizeof( allocDesc ) );
		allocDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocDesc.descriptorPool = vk.descriptor_pool;
		allocDesc.descriptorSetCount = 1;
		allocDesc.pSetLayouts = &vk.iqmGpu.set_layout_bones;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocDesc, &vk.iqmGpu.bone_descriptor[i] ) );

		// update descriptor to point at bone UBO
		memset( &bufDesc, 0, sizeof( bufDesc ) );
		bufDesc.buffer = vk.iqmGpu.bone_buffer[i];
		bufDesc.offset = 0;
		bufDesc.range = IQM_BONE_UBO_SIZE;

		memset( &writeDesc, 0, sizeof( writeDesc ) );
		writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDesc.dstSet = vk.iqmGpu.bone_descriptor[i];
		writeDesc.dstBinding = 0;
		writeDesc.dstArrayElement = 0;
		writeDesc.descriptorCount = 1;
		writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDesc.pBufferInfo = &bufDesc;

		qvkUpdateDescriptorSets( vk.device, 1, &writeDesc, 0, NULL );
	}

	// create the graphics pipeline
	{
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineViewportStateCreateInfo viewportState;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;
		VkVertexInputBindingDescription iqmBindings[1];
		VkVertexInputAttributeDescription iqmAttribs[6];

		// interleaved vertex layout: pos(3f) + normal(3f) + texcoord(2f) + tangent(4f) + weights(4f) + indices(4u8)
		// stride = 3*4 + 3*4 + 2*4 + 4*4 + 4*4 + 4*1 = 12+12+8+16+16+4 = 68 bytes
		uint32_t stride = 68;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		memset( stages, 0, sizeof( stages ) );

		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.iqm_skinning_vs;
		stages[0].pName = "main";

		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.iqm_skinning_fs;
		stages[1].pName = "main";

		// single interleaved binding
		iqmBindings[0].binding = 0;
		iqmBindings[0].stride = stride;
		iqmBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// location 0: position (vec3)
		iqmAttribs[0].location = 0;
		iqmAttribs[0].binding = 0;
		iqmAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		iqmAttribs[0].offset = 0;

		// location 1: normal (vec3)
		iqmAttribs[1].location = 1;
		iqmAttribs[1].binding = 0;
		iqmAttribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		iqmAttribs[1].offset = 12;

		// location 2: texcoord (vec2)
		iqmAttribs[2].location = 2;
		iqmAttribs[2].binding = 0;
		iqmAttribs[2].format = VK_FORMAT_R32G32_SFLOAT;
		iqmAttribs[2].offset = 24;

		// location 3: tangent (vec4) — xyz tangent + w bitangent sign
		iqmAttribs[3].location = 3;
		iqmAttribs[3].binding = 0;
		iqmAttribs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		iqmAttribs[3].offset = 32;

		// location 4: bone weights (vec4)
		iqmAttribs[4].location = 4;
		iqmAttribs[4].binding = 0;
		iqmAttribs[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		iqmAttribs[4].offset = 48;

		// location 5: bone indices (uvec4, stored as R8G8B8A8_UINT)
		iqmAttribs[5].location = 5;
		iqmAttribs[5].binding = 0;
		iqmAttribs[5].format = VK_FORMAT_R8G8B8A8_UINT;
		iqmAttribs[5].offset = 64;

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.vertexBindingDescriptionCount = 1;
		vertexInput.pVertexBindingDescriptions = iqmBindings;
		vertexInput.vertexAttributeDescriptionCount = 6;
		vertexInput.pVertexAttributeDescriptions = iqmAttribs;

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;

		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width = (float)vk.renderWidth;
		viewport.height = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;

		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = vk.msaaActive ? vkSamples : VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &blendAttach, 0, sizeof( blendAttach ) );
		blendAttach.blendEnable = VK_FALSE;
		blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount = 2;
		gpInfo.pStages = stages;
		gpInfo.pVertexInputState = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState = &multisampling;
		gpInfo.pDepthStencilState = &depthStencil;
		gpInfo.pColorBlendState = &colorBlend;
		gpInfo.pDynamicState = &dynamicState;
		gpInfo.layout = vk.iqmGpu.pipeline_layout;
		gpInfo.renderPass = vk.render_pass.main;
		gpInfo.subpass = 0;

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL, &vk.iqmGpu.pipeline ) );
	}

	vk.iqmGpu.available = qtrue;

	ri.Log( SEV_INFO, "IQM GPU skinning initialized (bone UBO %d bytes)\n",
		(int)IQM_BONE_UBO_SIZE );
}


/*
===============
vk_shutdown_iqm_gpu_skinning
===============
*/
void vk_shutdown_iqm_gpu_skinning( void )
{
	if ( vk.iqmGpu.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.iqmGpu.pipeline, NULL );
		vk.iqmGpu.pipeline = VK_NULL_HANDLE;
	}

	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.iqmGpu.bone_buffer[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.iqmGpu.bone_buffer[i], NULL );
			if ( vk.iqmGpu.bone_memory[i] != VK_NULL_HANDLE ) {
				qvkFreeMemory( vk.device, vk.iqmGpu.bone_memory[i], NULL );
			}
			vk.iqmGpu.bone_buffer[i] = VK_NULL_HANDLE;
			vk.iqmGpu.bone_memory[i] = VK_NULL_HANDLE;
			vk.iqmGpu.bone_ptr[i] = NULL;
		}
	}

	if ( vk.iqmGpu.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.iqmGpu.pipeline_layout, NULL );
		vk.iqmGpu.pipeline_layout = VK_NULL_HANDLE;
	}

	if ( vk.iqmGpu.set_layout_bones != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.iqmGpu.set_layout_bones, NULL );
		vk.iqmGpu.set_layout_bones = VK_NULL_HANDLE;
	}

	vk.iqmGpu.available = qfalse;
}


/*
===============
vk_create_iqm_vbo — create device-local vertex + index buffers for an IQM model
Uses the staging buffer to upload data.
===============
*/
qboolean vk_create_iqm_vbo( VkBuffer *outVertBuf, VkDeviceMemory *outVertMem,
	VkBuffer *outIdxBuf, VkDeviceMemory *outIdxMem,
	const byte *vertData, int vertSize,
	const byte *idxData, int idxSize )
{
	VkBufferCreateInfo bufDesc;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkCommandBuffer cmdBuf;
	VkBufferCopy copyRegion;

	if ( !vk.iqmGpu.available )
		return qfalse;

	// check that the staging buffer can hold the data
	// NOLINTNEXTLINE(bugprone-misplaced-widening-cast) — small int sum widened to VkDeviceSize for the comparison; both inputs are bounded, no precision loss
	if ( (VkDeviceSize)(vertSize + idxSize) > vk.staging_buffer.size ) {
		ri.Log( SEV_WARN, "vk_create_iqm_vbo: data too large for staging buffer (%d > %d)\n",
			vertSize + idxSize, (int)vk.staging_buffer.size );
		return qfalse;
	}

	// create device-local vertex buffer
	memset( &bufDesc, 0, sizeof( bufDesc ) );
	bufDesc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufDesc.size = vertSize;
	bufDesc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufDesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bufDesc, NULL, outVertBuf ) );

	qvkGetBufferMemoryRequirements( vk.device, *outVertBuf, &memReqs );
	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, outVertMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outVertBuf, *outVertMem, 0 ) );

	// create device-local index buffer
	bufDesc.size = idxSize;
	bufDesc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bufDesc, NULL, outIdxBuf ) );

	qvkGetBufferMemoryRequirements( vk.device, *outIdxBuf, &memReqs );
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, outIdxMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outIdxBuf, *outIdxMem, 0 ) );

	// upload vertex data via staging buffer
	{
		VkCommandBufferAllocateInfo cmdAlloc;
		VkCommandBufferBeginInfo beginInfo;
		VkSubmitInfo submitInfo;
		VkFence fence;
		VkFenceCreateInfo fenceInfo;

		memset( &cmdAlloc, 0, sizeof( cmdAlloc ) );
		cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAlloc.commandPool = vk.command_pool;
		cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAlloc.commandBufferCount = 1;
		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &cmdAlloc, &cmdBuf ) );

		memset( &beginInfo, 0, sizeof( beginInfo ) );
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK( qvkBeginCommandBuffer( cmdBuf, &beginInfo ) );

		// copy vertex data to staging, then staging to device
		memcpy( vk.staging_buffer.ptr, vertData, vertSize );
		memset( &copyRegion, 0, sizeof( copyRegion ) );
		copyRegion.size = vertSize;
		qvkCmdCopyBuffer( cmdBuf, vk.staging_buffer.handle, *outVertBuf, 1, &copyRegion );

		// copy index data to staging, then staging to device
		memcpy( vk.staging_buffer.ptr + vertSize, idxData, idxSize );
		copyRegion.srcOffset = vertSize;
		copyRegion.dstOffset = 0;
		copyRegion.size = idxSize;
		qvkCmdCopyBuffer( cmdBuf, vk.staging_buffer.handle, *outIdxBuf, 1, &copyRegion );

		VK_CHECK( qvkEndCommandBuffer( cmdBuf ) );

		memset( &fenceInfo, 0, sizeof( fenceInfo ) );
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VK_CHECK( qvkCreateFence( vk.device, &fenceInfo, NULL, &fence ) );

		memset( &submitInfo, 0, sizeof( submitInfo ) );
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submitInfo, fence ) );
		VK_CHECK( qvkWaitForFences( vk.device, 1, &fence, VK_TRUE, 1e10 ) );

		qvkDestroyFence( vk.device, fence, NULL );
		qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &cmdBuf );
	}

	return qtrue;
}


/*
===============
vk_destroy_iqm_vbo — release per-model VBO resources
===============
*/
void vk_destroy_iqm_vbo( VkBuffer *vertBuf, VkDeviceMemory *vertMem,
	VkBuffer *idxBuf, VkDeviceMemory *idxMem )
{
	if ( *vertBuf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, *vertBuf, NULL );
		*vertBuf = VK_NULL_HANDLE;
	}
	if ( *vertMem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *vertMem, NULL );
		*vertMem = VK_NULL_HANDLE;
	}
	if ( *idxBuf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, *idxBuf, NULL );
		*idxBuf = VK_NULL_HANDLE;
	}
	if ( *idxMem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *idxMem, NULL );
		*idxMem = VK_NULL_HANDLE;
	}
}


/*
===============
vk_draw_iqm_gpu — issue a GPU-skinned IQM draw call

Binds the IQM pipeline, uploads bone matrices, binds the per-model VBO,
binds the texture descriptor, and issues a drawIndexed call.
Must be called during an active render pass.
===============
*/
void vk_draw_iqm_gpu( VkBuffer vertBuffer, VkBuffer idxBuffer,
	int firstIndex, int numIndexes,
	const float *boneMats, int numBones,
	VkDescriptorSet textureDescriptor,
	const float *mvp )
{
	VkCommandBuffer cmd;
	int frameIdx;
	VkDeviceSize vertOffset;
	VkViewport viewport;
	VkRect2D scissor;

	if ( !vk.iqmGpu.available )
		return;

	if ( vk.geometry_buffer_size_new )
		return; // geometry buffer overflow this frame

	cmd = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// upload bone matrices to per-frame UBO
	// each bone is 3 * vec4 = 48 bytes
	memcpy( vk.iqmGpu.bone_ptr[frameIdx], boneMats, numBones * 3 * sizeof( vec4_t ) );

	// bind IQM skinning pipeline
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.iqmGpu.pipeline );

	// push MVP matrix
	qvkCmdPushConstants( cmd, vk.iqmGpu.pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT, 0, 64, mvp );

	// bind descriptor set 0: bone matrices
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.iqmGpu.pipeline_layout, 0, 1,
		&vk.iqmGpu.bone_descriptor[frameIdx], 0, NULL );

	// bind descriptor set 1: texture
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.iqmGpu.pipeline_layout, 1, 1,
		&textureDescriptor, 0, NULL );

	// bind vertex buffer
	vertOffset = 0;
	qvkCmdBindVertexBuffers( cmd, 0, 1, &vertBuffer, &vertOffset );

	// bind index buffer
	qvkCmdBindIndexBuffer( cmd, idxBuffer, 0, VK_INDEX_TYPE_UINT32 );

	// set viewport and scissor
	memset( &viewport, 0, sizeof( viewport ) );
	viewport.x = (float)backEnd.viewParms.viewportX;
	viewport.y = (float)backEnd.viewParms.viewportY;
	viewport.width = (float)backEnd.viewParms.viewportWidth;
	viewport.height = (float)backEnd.viewParms.viewportHeight;
	viewport.maxDepth = 1.0f;
	qvkCmdSetViewport( cmd, 0, 1, &viewport );

	memset( &scissor, 0, sizeof( scissor ) );
	scissor.offset.x = backEnd.viewParms.viewportX;
	scissor.offset.y = backEnd.viewParms.viewportY;
	scissor.extent.width = backEnd.viewParms.viewportWidth;
	scissor.extent.height = backEnd.viewParms.viewportHeight;
	qvkCmdSetScissor( cmd, 0, 1, &scissor );

	// draw
	qvkCmdDrawIndexed( cmd, numIndexes, 1, firstIndex, 0, 0 );

	// invalidate pipeline and descriptor state so the next standard draw
	// rebinds correctly (we bound a different pipeline layout)
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT; // force viewport/scissor update
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end = 0;
}
#endif // FEAT_IQM


#ifdef USE_VBO
void vk_release_vbo( void )
{
	if ( vk.vbo.vertex_buffer )
		qvkDestroyBuffer( vk.device, vk.vbo.vertex_buffer, NULL );
	vk.vbo.vertex_buffer = VK_NULL_HANDLE;

	if ( vk.vbo.buffer_memory )
		qvkFreeMemory( vk.device, vk.vbo.buffer_memory, NULL );
	vk.vbo.buffer_memory = VK_NULL_HANDLE;
}


qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size )
{
	VkMemoryRequirements vb_mem_reqs;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	VkDeviceSize allocationSize;
	uint32_t memory_type_bits;
	VkCommandBuffer command_buffer;
	VkBufferCopy copyRegion[1];
	VkDeviceSize uploadDone;

	vk_release_vbo();

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	// device-local buffer
	desc.size = vbo_size;
	desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.vbo.vertex_buffer ) );

	// memory requirements
	qvkGetBufferMemoryRequirements( vk.device, vk.vbo.vertex_buffer, &vb_mem_reqs );
	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = allocationSize;
	alloc_info.memoryTypeIndex = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.vbo.buffer_memory ) );
	qvkBindBufferMemory( vk.device, vk.vbo.vertex_buffer, vk.vbo.buffer_memory, vertex_buffer_offset );

	// staging buffers

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qfalse );
#endif
	// utilize existing staging buffer
	uploadDone = 0;
	while ( uploadDone < vbo_size ) {
		VkDeviceSize uploadSize = vk.staging_buffer.size;
		if ( uploadDone + uploadSize > vbo_size ) {
			uploadSize = vbo_size - uploadDone;
		}
		memcpy(vk.staging_buffer.ptr + 0, vbo_data + uploadDone, uploadSize);
		command_buffer = begin_command_buffer();
		copyRegion[0].srcOffset = 0;
		copyRegion[0].dstOffset = uploadDone;
		copyRegion[0].size = uploadSize;
		qvkCmdCopyBuffer( command_buffer, vk.staging_buffer.handle, vk.vbo.vertex_buffer, 1, &copyRegion[0] );
		end_command_buffer( command_buffer, __func__ );
		uploadDone += uploadSize;
	}

	SET_OBJECT_NAME( vk.vbo.vertex_buffer, "static VBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.vbo.buffer_memory, "static VBO memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

	return qtrue;
}
#endif

#include "shaders/spirv/shader_data.c"
#define SHADER_MODULE(name) SHADER_MODULE(name,sizeof(name))

static void vk_create_shader_modules( void )
{
	int i, j, k, l;

	vk.modules.vert.gen[0][0][0][0] = SHADER_MODULE( vert_tx0 );
	vk.modules.vert.gen[0][0][0][1] = SHADER_MODULE( vert_tx0_fog );
	vk.modules.vert.gen[0][0][1][0] = SHADER_MODULE( vert_tx0_env );
	vk.modules.vert.gen[0][0][1][1] = SHADER_MODULE( vert_tx0_env_fog );

	vk.modules.vert.gen[1][0][0][0] = SHADER_MODULE( vert_tx1 );
	vk.modules.vert.gen[1][0][0][1] = SHADER_MODULE( vert_tx1_fog );
	vk.modules.vert.gen[1][0][1][0] = SHADER_MODULE( vert_tx1_env );
	vk.modules.vert.gen[1][0][1][1] = SHADER_MODULE( vert_tx1_env_fog );

	vk.modules.vert.gen[1][1][0][0] = SHADER_MODULE( vert_tx1_cl );
	vk.modules.vert.gen[1][1][0][1] = SHADER_MODULE( vert_tx1_cl_fog );
	vk.modules.vert.gen[1][1][1][0] = SHADER_MODULE( vert_tx1_cl_env );
	vk.modules.vert.gen[1][1][1][1] = SHADER_MODULE( vert_tx1_cl_env_fog );

	vk.modules.vert.gen[2][0][0][0] = SHADER_MODULE( vert_tx2 );
	vk.modules.vert.gen[2][0][0][1] = SHADER_MODULE( vert_tx2_fog );
	vk.modules.vert.gen[2][0][1][0] = SHADER_MODULE( vert_tx2_env );
	vk.modules.vert.gen[2][0][1][1] = SHADER_MODULE( vert_tx2_env_fog );

	vk.modules.vert.gen[2][1][0][0] = SHADER_MODULE( vert_tx2_cl );
	vk.modules.vert.gen[2][1][0][1] = SHADER_MODULE( vert_tx2_cl_fog );
	vk.modules.vert.gen[2][1][1][0] = SHADER_MODULE( vert_tx2_cl_env );
	vk.modules.vert.gen[2][1][1][1] = SHADER_MODULE( vert_tx2_cl_env_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					const char *s = va( "%s-texture%s%s%s vertex module", tx[i], cl[j], env[k], fog[l] );
					SET_OBJECT_NAME( vk.modules.vert.gen[i][j][k][l], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
				}
			}
		}
	}

	// specialized depth-fragment shader
	vk.modules.frag.gen0_df = SHADER_MODULE( frag_tx0_df );
	SET_OBJECT_NAME( vk.modules.frag.gen0_df, "single-texture df fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	// fixed-color (1.0) shader modules
	vk.modules.vert.ident1[0][0][0] = SHADER_MODULE( vert_tx0_ident1 );
	vk.modules.vert.ident1[0][0][1] = SHADER_MODULE( vert_tx0_ident1_fog );
	vk.modules.vert.ident1[0][1][0] = SHADER_MODULE( vert_tx0_ident1_env );
	vk.modules.vert.ident1[0][1][1] = SHADER_MODULE( vert_tx0_ident1_env_fog );
	vk.modules.vert.ident1[1][0][0] = SHADER_MODULE( vert_tx1_ident1 );
	vk.modules.vert.ident1[1][0][1] = SHADER_MODULE( vert_tx1_ident1_fog );
	vk.modules.vert.ident1[1][1][0] = SHADER_MODULE( vert_tx1_ident1_env );
	vk.modules.vert.ident1[1][1][1] = SHADER_MODULE( vert_tx1_ident1_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture identity%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.ident1[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.ident1[0][0] = SHADER_MODULE( frag_tx0_ident1 );
	vk.modules.frag.ident1[0][1] = SHADER_MODULE( frag_tx0_ident1_fog );
	vk.modules.frag.ident1[1][0] = SHADER_MODULE( frag_tx1_ident1 );
	vk.modules.frag.ident1[1][1] = SHADER_MODULE( frag_tx1_ident1_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture identity%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ident1[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.vert.fixed[0][0][0] = SHADER_MODULE( vert_tx0_fixed );
	vk.modules.vert.fixed[0][0][1] = SHADER_MODULE( vert_tx0_fixed_fog );
	vk.modules.vert.fixed[0][1][0] = SHADER_MODULE( vert_tx0_fixed_env );
	vk.modules.vert.fixed[0][1][1] = SHADER_MODULE( vert_tx0_fixed_env_fog );
	vk.modules.vert.fixed[1][0][0] = SHADER_MODULE( vert_tx1_fixed );
	vk.modules.vert.fixed[1][0][1] = SHADER_MODULE( vert_tx1_fixed_fog );
	vk.modules.vert.fixed[1][1][0] = SHADER_MODULE( vert_tx1_fixed_env );
	vk.modules.vert.fixed[1][1][1] = SHADER_MODULE( vert_tx1_fixed_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture fixed-color%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.fixed[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.fixed[0][0] = SHADER_MODULE( frag_tx0_fixed );
	vk.modules.frag.fixed[0][1] = SHADER_MODULE( frag_tx0_fixed_fog );
	vk.modules.frag.fixed[1][0] = SHADER_MODULE( frag_tx1_fixed );
	vk.modules.frag.fixed[1][1] = SHADER_MODULE( frag_tx1_fixed_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture fixed-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.fixed[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.frag.ent[0][0] = SHADER_MODULE( frag_tx0_ent );
	vk.modules.frag.ent[0][1] = SHADER_MODULE( frag_tx0_ent_fog );
	//vk.modules.frag.ent[1][0] = SHADER_MODULE( frag_tx1_ent );
	//vk.modules.frag.ent[1][1] = SHADER_MODULE( frag_tx1_ent_fog );
	for ( i = 0; i < 1; i++ ) {
		const char *tx[] = { "single" /*, "double" */};
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture entity-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ent[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.frag.gen[0][0][0] = SHADER_MODULE( frag_tx0 );
	vk.modules.frag.gen[0][0][1] = SHADER_MODULE( frag_tx0_fog );

	vk.modules.frag.gen[1][0][0] = SHADER_MODULE( frag_tx1 );
	vk.modules.frag.gen[1][0][1] = SHADER_MODULE( frag_tx1_fog );

	vk.modules.frag.gen[1][1][0] = SHADER_MODULE( frag_tx1_cl );
	vk.modules.frag.gen[1][1][1] = SHADER_MODULE( frag_tx1_cl_fog );

	vk.modules.frag.gen[2][0][0] = SHADER_MODULE( frag_tx2 );
	vk.modules.frag.gen[2][0][1] = SHADER_MODULE( frag_tx2_fog );

	vk.modules.frag.gen[2][1][0] = SHADER_MODULE( frag_tx2_cl );
	vk.modules.frag.gen[2][1][1] = SHADER_MODULE( frag_tx2_cl_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture%s%s fragment module", tx[i], cl[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.frag.gen[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}


	// depth fade fragment shader variants (single-texture only)
	if ( vk.depthFade.active ) {
		vk.modules.frag.dfade_gen[0][0] = SHADER_MODULE( frag_tx0_dfade );
		vk.modules.frag.dfade_gen[0][1] = SHADER_MODULE( frag_tx0_dfade_fog );
		vk.modules.frag.dfade_ident1[0][0] = SHADER_MODULE( frag_tx0_ident1_dfade );
		vk.modules.frag.dfade_ident1[0][1] = SHADER_MODULE( frag_tx0_ident1_dfade_fog );
		vk.modules.frag.dfade_fixed[0][0] = SHADER_MODULE( frag_tx0_fixed_dfade );
		vk.modules.frag.dfade_fixed[0][1] = SHADER_MODULE( frag_tx0_fixed_dfade_fog );
		vk.modules.frag.dfade_ent[0][0] = SHADER_MODULE( frag_tx0_ent_dfade );
		vk.modules.frag.dfade_ent[0][1] = SHADER_MODULE( frag_tx0_ent_dfade_fog );
	}

	// SMAA shader modules
	if ( vk.smaa.active ) {
		vk.modules.smaa_edge_vs    = SHADER_MODULE( smaa_edge_vert_spv );
		vk.modules.smaa_edge_fs    = SHADER_MODULE( smaa_edge_frag_spv );
		vk.modules.smaa_blend_vs   = SHADER_MODULE( smaa_blend_vert_spv );
		vk.modules.smaa_blend_fs   = SHADER_MODULE( smaa_blend_frag_spv );
		vk.modules.smaa_resolve_vs = SHADER_MODULE( smaa_resolve_vert_spv );
		vk.modules.smaa_resolve_fs = SHADER_MODULE( smaa_resolve_frag_spv );
	}

	vk.modules.vert.light[0] = SHADER_MODULE( vert_light );
	vk.modules.vert.light[1] = SHADER_MODULE( vert_light_fog );
	SET_OBJECT_NAME( vk.modules.vert.light[0], "light vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.vert.light[1], "light fog vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.frag.light[0][0] = SHADER_MODULE( frag_light );
	vk.modules.frag.light[0][1] = SHADER_MODULE( frag_light_fog );
	vk.modules.frag.light[1][0] = SHADER_MODULE( frag_light_line );
	vk.modules.frag.light[1][1] = SHADER_MODULE( frag_light_line_fog );
	SET_OBJECT_NAME( vk.modules.frag.light[0][0], "light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[0][1], "light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][0], "linear light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][1], "linear light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

#if FEAT_PARALLAX_MAPPING
	vk.modules.vert.light_parallax[0] = SHADER_MODULE( vert_light_parallax );
	vk.modules.vert.light_parallax[1] = SHADER_MODULE( vert_light_parallax_fog );
	vk.modules.frag.light_parallax[0][0] = SHADER_MODULE( frag_light_parallax );
	vk.modules.frag.light_parallax[0][1] = SHADER_MODULE( frag_light_parallax_fog );
	vk.modules.frag.light_parallax[1][0] = SHADER_MODULE( frag_light_parallax_line );
	vk.modules.frag.light_parallax[1][1] = SHADER_MODULE( frag_light_parallax_line_fog );
#endif

	vk.modules.color_fs = SHADER_MODULE( color_frag_spv );
	vk.modules.color_vs = SHADER_MODULE( color_vert_spv );

	SET_OBJECT_NAME( vk.modules.color_vs, "single-color vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.color_fs, "single-color fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.fog_vs = SHADER_MODULE( fog_vert_spv );
	vk.modules.fog_fs = SHADER_MODULE( fog_frag_spv );

	SET_OBJECT_NAME( vk.modules.fog_vs, "fog-only vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.fog_fs, "fog-only fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.dot_vs = SHADER_MODULE( dot_vert_spv );
	vk.modules.dot_fs = SHADER_MODULE( dot_frag_spv );

	SET_OBJECT_NAME( vk.modules.dot_vs, "dot vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.dot_fs, "dot fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.bloom_fs = SHADER_MODULE( bloom_frag_spv );
	vk.modules.blur_fs = SHADER_MODULE( blur_frag_spv );
	vk.modules.blend_fs = SHADER_MODULE( blend_frag_spv );

	SET_OBJECT_NAME( vk.modules.bloom_fs, "bloom extraction fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blur_fs, "gaussian blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blend_fs, "final bloom blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

#if FEAT_ADVANCED_WATER
	vk.modules.water_fs = SHADER_MODULE( water_frag_spv );
#endif

#if FEAT_SHADOW_MAPPING
	vk.modules.shadow_depth_vs = SHADER_MODULE( shadow_depth_vert_spv );
	vk.modules.shadow_depth_fs = SHADER_MODULE( shadow_depth_frag_spv );
	vk.modules.light_shadow[0] = SHADER_MODULE( vert_light_shadow );
	vk.modules.light_shadow[1] = SHADER_MODULE( vert_light_shadow_fog );
	vk.modules.light_shadow_frag[0][0] = SHADER_MODULE( frag_light_shadow );
	vk.modules.light_shadow_frag[0][1] = SHADER_MODULE( frag_light_shadow_fog );
	vk.modules.light_shadow_frag[1][0] = SHADER_MODULE( frag_light_shadow_line );
	vk.modules.light_shadow_frag[1][1] = SHADER_MODULE( frag_light_shadow_line_fog );
#endif

#if FEAT_PBR
	vk.modules.light_pbr_frag[0][0] = SHADER_MODULE( frag_light_pbr );
	vk.modules.light_pbr_frag[0][1] = SHADER_MODULE( frag_light_pbr_fog );
	vk.modules.light_pbr_frag[1][0] = SHADER_MODULE( frag_light_pbr_line );
	vk.modules.light_pbr_frag[1][1] = SHADER_MODULE( frag_light_pbr_line_fog );
#endif

	vk.modules.gamma_fs = SHADER_MODULE( gamma_frag_spv );
	vk.modules.gamma_vs = SHADER_MODULE( gamma_vert_spv );

	// Load compiled gamma post-process variants
	memset( vk.gamma_variant_fs, 0, sizeof( vk.gamma_variant_fs ) );
#if FEAT_SSAO
	vk.gamma_variant_fs[ GAMMA_VAR_SSAO ] = SHADER_MODULE( gamma_ssao_frag_spv );
#endif
#if FEAT_TONEMAP
	vk.gamma_variant_fs[ GAMMA_VAR_TONEMAP ] = SHADER_MODULE( gamma_tonemap_frag_spv );
#endif
#if FEAT_COLOR_GRADING
	vk.gamma_variant_fs[ GAMMA_VAR_CG ] = SHADER_MODULE( gamma_colorgrade_frag_spv );
#endif
#if FEAT_FXAA
	vk.gamma_variant_fs[ GAMMA_VAR_FXAA ] = SHADER_MODULE( gamma_fxaa_frag_spv );
#endif
#if FEAT_SSAO && FEAT_TONEMAP
	vk.gamma_variant_fs[ GAMMA_VAR_SSAO | GAMMA_VAR_TONEMAP ] = SHADER_MODULE( gamma_ssao_tonemap_frag_spv );
#endif
#if FEAT_TONEMAP && FEAT_COLOR_GRADING
	vk.gamma_variant_fs[ GAMMA_VAR_TONEMAP | GAMMA_VAR_CG ] = SHADER_MODULE( gamma_tonemap_cg_frag_spv );
#endif
#if FEAT_SSAO && FEAT_TONEMAP && FEAT_COLOR_GRADING
	vk.gamma_variant_fs[ GAMMA_VAR_SSAO | GAMMA_VAR_TONEMAP | GAMMA_VAR_CG ] = SHADER_MODULE( gamma_full_frag_spv );
#endif
#if FEAT_FXAA && FEAT_SSAO
	vk.gamma_variant_fs[ GAMMA_VAR_FXAA | GAMMA_VAR_SSAO ] = SHADER_MODULE( gamma_fxaa_ssao_frag_spv );
#endif
#if FEAT_FXAA && FEAT_SSAO && FEAT_TONEMAP && FEAT_COLOR_GRADING
	vk.gamma_variant_fs[ GAMMA_VAR_FXAA | GAMMA_VAR_SSAO | GAMMA_VAR_TONEMAP | GAMMA_VAR_CG ] = SHADER_MODULE( gamma_all_frag_spv );
#endif
#if FEAT_GODRAYS
	vk.gamma_variant_fs[ GAMMA_VAR_GODRAYS ] = SHADER_MODULE( gamma_godrays_frag_spv );
#endif
#if FEAT_GODRAYS && FEAT_SSAO
	vk.gamma_variant_fs[ GAMMA_VAR_SSAO | GAMMA_VAR_GODRAYS ] = SHADER_MODULE( gamma_ssao_godrays_frag_spv );
#endif
#if FEAT_GODRAYS && FEAT_SSAO && FEAT_TONEMAP
	vk.gamma_variant_fs[ GAMMA_VAR_SSAO | GAMMA_VAR_GODRAYS | GAMMA_VAR_TONEMAP ] = SHADER_MODULE( gamma_ssao_godrays_tm_frag_spv );
#endif
#if FEAT_GODRAYS && FEAT_FXAA && FEAT_SSAO && FEAT_TONEMAP && FEAT_COLOR_GRADING
	vk.gamma_variant_fs[ GAMMA_VAR_FXAA | GAMMA_VAR_SSAO | GAMMA_VAR_GODRAYS | GAMMA_VAR_TONEMAP | GAMMA_VAR_CG ] = SHADER_MODULE( gamma_ultimate_frag_spv );
#endif

	SET_OBJECT_NAME( vk.modules.gamma_fs, "gamma post-processing fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.gamma_vs, "gamma post-processing vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

#if FEAT_IQM
	vk.modules.iqm_skinning_vs = SHADER_MODULE( iqm_skinning_vert_spv );
	vk.modules.iqm_skinning_fs = SHADER_MODULE( iqm_skinning_frag_spv );
	SET_OBJECT_NAME( vk.modules.iqm_skinning_vs, "IQM skinning vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.iqm_skinning_fs, "IQM skinning fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
#endif

	vk.modules.msdf_vs = SHADER_MODULE( msdf_vert_spv );
	vk.modules.msdf_fs = SHADER_MODULE( msdf_frag_spv );
	SET_OBJECT_NAME( vk.modules.msdf_vs, "MSDF text vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.msdf_fs, "MSDF text fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.ribbon_vs = SHADER_MODULE( ribbon_vert_spv );
	vk.modules.ribbon_fs = SHADER_MODULE( ribbon_frag_spv );
	SET_OBJECT_NAME( vk.modules.ribbon_vs, "ribbon vertex module",   VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ribbon_fs, "ribbon fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.sprite_vs = SHADER_MODULE( sprite_vert_spv );
	vk.modules.sprite_fs = SHADER_MODULE( sprite_frag_spv );
	SET_OBJECT_NAME( vk.modules.sprite_vs, "sprite vertex module",   VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.sprite_fs, "sprite fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.beam_vs = SHADER_MODULE( beam_vert_spv );
	vk.modules.beam_fs = SHADER_MODULE( beam_frag_spv );
	SET_OBJECT_NAME( vk.modules.beam_vs, "beam vertex module",   VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.beam_fs, "beam fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.particle_integrate_cs = SHADER_MODULE( particle_integrate_comp_spv );
	vk.modules.particle_vs           = SHADER_MODULE( particle_vert_spv );
	vk.modules.particle_fs           = SHADER_MODULE( particle_frag_spv );
	SET_OBJECT_NAME( vk.modules.particle_integrate_cs, "particle integrate compute module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.particle_vs,           "particle vertex module",            VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.particle_fs,           "particle fragment module",          VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.q1_ls_vs       = SHADER_MODULE( q1_ls_vert_spv );
	vk.modules.q1_ls_fs       = SHADER_MODULE( q1_ls_frag_spv );
	vk.modules.q1_ls_array_fs = SHADER_MODULE( q1_ls_array_frag_spv );
	SET_OBJECT_NAME( vk.modules.q1_ls_vs,       "lightstyle vertex module",         VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.q1_ls_fs,       "lightstyle fragment module",       VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.q1_ls_array_fs, "lightstyle array fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
}


static void vk_alloc_persistent_pipelines( void )
{
	unsigned int state_bits;
	Vk_Pipeline_Def def;

	// skybox
	{
		memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SINGLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.mirror = qfalse;
		vk.skybox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// stencil shadows
	{
		cullType_t cull_types[2] = { CT_FRONT_SIDED, CT_BACK_SIDED };
		qboolean mirror_flags[2] = { qfalse, qtrue };
		int i, j;

		memset(&def, 0, sizeof(def));
		def.polygon_offset = qfalse;
		def.state_bits = 0;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = SHADOW_EDGES;

		for (i = 0; i < 2; i++) {
			def.face_culling = cull_types[i];
			for (j = 0; j < 2; j++) {
				def.mirror = mirror_flags[j];
				vk.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
			}
		}
	}
	{
		memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;
		def.shadow_phase = SHADOW_FS_QUAD;
		def.primitives = TRIANGLE_STRIP;
		vk.shadow_finish_pipeline = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL,	// modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL			// additive
		};
		qboolean polygon_offset[2] = { qfalse, qtrue };
		int i, j, k;
#ifdef USE_PMLIGHT
		int l;
#endif

		memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;

		for ( i = 0; i < 2; i++ ) {
			unsigned fog_state = fog_state_bits[ i ];
			unsigned dlight_state = dlight_state_bits[ i ];

			for ( j = 0; j < 3; j++ ) {
				def.face_culling = j; // cullType_t value

				for ( k = 0; k < 2; k++ ) {
					def.polygon_offset = polygon_offset[ k ];
#ifdef USE_FOG_ONLY
					def.shader_type = TYPE_FOG_ONLY;
#else
					def.shader_type = TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk.fog_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );

					def.shader_type = TYPE_SIGNLE_TEXTURE;
					def.state_bits = dlight_state;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, r_dlightMode->integer == 0 ? qtrue : qfalse );
#else
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );
#endif
#endif
				}
			}
		}

#ifdef USE_PMLIGHT
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		//def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING;
		for (i = 0; i < 3; i++) { // cullType
			def.face_culling = i;
			for ( j = 0; j < 2; j++ ) { // polygonOffset
				def.polygon_offset = polygon_offset[j];
				for ( k = 0; k < 2; k++ ) {
					def.fog_stage = k; // fogStage
					for ( l = 0; l < 2; l++ ) {
						def.abs_light = l;
						def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING;
						vk.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
						def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR;
						vk.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
					}
				}
			}
		}
#endif // USE_PMLIGHT
	}

	// RT_BEAM surface
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = CT_FRONT_SIDED;
		def.primitives = TRIANGLE_STRIP;
		vk.surface_beam_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// axis for missing models
	{
		memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.face_culling = CT_TWO_SIDED;
		def.primitives = LINE_LIST;
		if ( vk.wideLines )
			def.line_width = 3;
		vk.surface_axis_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// flare visibility test dot
	if ( vk.fragmentStores )
	{
		memset( &def, 0, sizeof( def ) );
		//def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_DOT;
		def.primitives = POINT_LIST;
		vk.dot_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// MSDF text rendering pipeline
	{
		memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_MSDF;
		vk.msdf_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// Q1 4-style lightmap blend pipeline (animChain lerp via set=6)
	{
		memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_FRONT_SIDED;
		def.shader_type = TYPE_LIGHTSTYLES;
		vk.q1ls_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// Q1 4-style lightmap blend pipeline (GPU texture array animation)
	{
		memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_FRONT_SIDED;
		def.shader_type = TYPE_LIGHTSTYLES_ARRAY;
		vk.q1ls_array_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// DrawNormals()
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.normals_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_DebugPolygon()
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		vk.surface_debug_pipeline_solid = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.surface_debug_pipeline_outline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_ShowImages
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );

		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_COLOR_BLACK;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline2 = vk_find_pipeline_ext( 0, &def, qfalse );
	}
}

void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass );
static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry);

static void vk_create_smaa_pipelines( void )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;

	// quality presets: Low(1), Medium(2), High(3), Ultra(4)
	static const float thresholds[] = { 0.0f, 0.15f, 0.10f, 0.10f, 0.05f };
	static const int searchSteps[] = { 0, 4, 8, 16, 32 };
	static const int searchStepsDiag[] = { 0, 0, 0, 8, 16 };
	static const int cornerRounding[] = { 0, 0, 0, 25, 25 };

	int q = vk.smaa.quality;

	// specialization constants for edge detection fragment shader
	VkSpecializationMapEntry edge_spec_entry;
	VkSpecializationInfo edge_spec_info;
	float edge_threshold;

	// specialization constants for blend weight calculation
	VkSpecializationMapEntry blend_vert_spec_entry;
	VkSpecializationInfo blend_vert_spec_info;
	int blend_vert_max_search;

	struct SmaaBlendFragSpec {
		int max_search_steps;
		int max_search_steps_diag;
		int corner_rounding;
	} blend_frag_spec_data;
	VkSpecializationMapEntry blend_frag_spec_entries[3];
	VkSpecializationInfo blend_frag_spec_info;

	if ( !vk.smaa.active )
		return;

	// shared pipeline state
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexAttributeDescriptions = NULL;

	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)glConfig.vidWidth;
	viewport.height = (float)glConfig.vidHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = glConfig.vidWidth;
	scissor.extent.height = glConfig.vidHeight;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	memset( &attachment_blend_state, 0, sizeof( attachment_blend_state ) );
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	attachment_blend_state.blendEnable = VK_FALSE;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;
	depth_stencil_state.stencilTestEnable = VK_FALSE;

	memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = vk.pipeline_layout_smaa;
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	// --- Edge detection pipeline ---
	edge_threshold = thresholds[q];
	edge_spec_entry.constantID = 0;
	edge_spec_entry.offset = 0;
	edge_spec_entry.size = sizeof( float );
	edge_spec_info.mapEntryCount = 1;
	edge_spec_info.pMapEntries = &edge_spec_entry;
	edge_spec_info.dataSize = sizeof( float );
	edge_spec_info.pData = &edge_threshold;

	set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.smaa_edge_vs, "main" );
	set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.smaa_edge_fs, "main" );
	shader_stages[1].pSpecializationInfo = &edge_spec_info;

	create_info.renderPass = vk.render_pass.smaa_edge;

	if ( vk.smaa_edge_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_edge_pipeline, NULL );
		vk.smaa_edge_pipeline = VK_NULL_HANDLE;
	}
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.smaa_edge_pipeline ) );
	SET_OBJECT_NAME( vk.smaa_edge_pipeline, "SMAA edge detection pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	// --- Blend weight calculation pipeline ---
	blend_vert_max_search = searchSteps[q];
	blend_vert_spec_entry.constantID = 0;
	blend_vert_spec_entry.offset = 0;
	blend_vert_spec_entry.size = sizeof( int );
	blend_vert_spec_info.mapEntryCount = 1;
	blend_vert_spec_info.pMapEntries = &blend_vert_spec_entry;
	blend_vert_spec_info.dataSize = sizeof( int );
	blend_vert_spec_info.pData = &blend_vert_max_search;

	blend_frag_spec_data.max_search_steps = searchSteps[q];
	blend_frag_spec_data.max_search_steps_diag = searchStepsDiag[q];
	blend_frag_spec_data.corner_rounding = cornerRounding[q];

	blend_frag_spec_entries[0].constantID = 0;
	blend_frag_spec_entries[0].offset = offsetof( struct SmaaBlendFragSpec, max_search_steps );
	blend_frag_spec_entries[0].size = sizeof( int );
	blend_frag_spec_entries[1].constantID = 1;
	blend_frag_spec_entries[1].offset = offsetof( struct SmaaBlendFragSpec, max_search_steps_diag );
	blend_frag_spec_entries[1].size = sizeof( int );
	blend_frag_spec_entries[2].constantID = 2;
	blend_frag_spec_entries[2].offset = offsetof( struct SmaaBlendFragSpec, corner_rounding );
	blend_frag_spec_entries[2].size = sizeof( int );

	blend_frag_spec_info.mapEntryCount = 3;
	blend_frag_spec_info.pMapEntries = blend_frag_spec_entries;
	blend_frag_spec_info.dataSize = sizeof( blend_frag_spec_data );
	blend_frag_spec_info.pData = &blend_frag_spec_data;

	set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.smaa_blend_vs, "main" );
	shader_stages[0].pSpecializationInfo = &blend_vert_spec_info;
	set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.smaa_blend_fs, "main" );
	shader_stages[1].pSpecializationInfo = &blend_frag_spec_info;

	create_info.renderPass = vk.render_pass.smaa_blend;

	if ( vk.smaa_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_blend_pipeline, NULL );
		vk.smaa_blend_pipeline = VK_NULL_HANDLE;
	}
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.smaa_blend_pipeline ) );
	SET_OBJECT_NAME( vk.smaa_blend_pipeline, "SMAA blend weight pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	// --- Resolve (neighborhood blending) pipeline ---
	set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.smaa_resolve_vs, "main" );
	shader_stages[0].pSpecializationInfo = NULL;
	set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.smaa_resolve_fs, "main" );
	shader_stages[1].pSpecializationInfo = NULL;

	create_info.renderPass = vk.render_pass.smaa_resolve;

	if ( vk.smaa_resolve_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_resolve_pipeline, NULL );
		vk.smaa_resolve_pipeline = VK_NULL_HANDLE;
	}
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.smaa_resolve_pipeline ) );
	SET_OBJECT_NAME( vk.smaa_resolve_pipeline, "SMAA resolve pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}


void vk_update_post_process_pipelines( void )
{
	if ( vk.fboActive ) {
		// update gamma shader
		vk_create_post_process_pipeline( 0, 0, 0 );
		if ( vk.capture.image ) {
			// update capture pipeline
			vk_create_post_process_pipeline( 3, gls.captureWidth, gls.captureHeight );
		}
		if ( r_bloom->integer ) {
			// update bloom shaders
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;
			uint32_t i;

			vk_create_post_process_pipeline( 1, width, height ); // bloom extraction

			for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i += 2 ) {
				width /= 2;
				height /= 2;
				vk_create_blur_pipeline( i + 0, width, height, qtrue ); // horizontal
				vk_create_blur_pipeline( i + 1, width, height, qfalse ); // vertical
			}

			vk_create_post_process_pipeline( 2, glConfig.vidWidth, glConfig.vidHeight ); // bloom blending
		}
		if ( vk.smaa.active ) {
			vk_create_smaa_pipelines();
		}
		// Tear down any previously-active gamma variant pipelines.
		// Variant slots in vk.gamma_variants[] are indexed by varIdx
		// bitmask; toggling a feature cvar (r_tonemap, r_ssao, etc.)
		// changes the active index, so the previous slot's pipeline
		// must be explicitly freed. The per-create destroy guard inside
		// vk_create_post_process_pipeline only checks the CURRENT slot,
		// so it would leak any other slot's pipeline. Walking the whole
		// array here keeps "exactly one variant pipeline live at a
		// time" — the same invariant the variant-bind path at draw
		// time assumes (vk.c around line 12710). Cost: one
		// vk_wait_idle if any variant is alive (rare, only fires when
		// CVG_RENDERER cvars change), otherwise a no-op loop.
		{
			int i;
			for ( i = 0; i < ARRAY_LEN( vk.gamma_variants ); i++ ) {
				if ( vk.gamma_variants[i] != VK_NULL_HANDLE ) {
					vk_wait_idle();
					qvkDestroyPipeline( vk.device, vk.gamma_variants[i], NULL );
					vk.gamma_variants[i] = VK_NULL_HANDLE;
				}
			}
		}

		// Create gamma variant pipeline if any post-process features are active
		{
			int varIdx = 0;
#if FEAT_SSAO
			if ( r_ssao->integer ) varIdx |= GAMMA_VAR_SSAO;
#endif
#if FEAT_TONEMAP
			if ( r_tonemap->integer ) varIdx |= GAMMA_VAR_TONEMAP;
#endif
#if FEAT_COLOR_GRADING
			if ( r_colorGrading->integer ) varIdx |= GAMMA_VAR_CG;
#endif
#if FEAT_FXAA
			if ( r_fxaa->integer ) varIdx |= GAMMA_VAR_FXAA;
#endif
#if FEAT_GODRAYS
			if ( r_godRays->integer ) varIdx |= GAMMA_VAR_GODRAYS;
#endif
			if ( varIdx ) {
				vk_create_post_process_pipeline( 5, 0, 0 ); // gamma variant
			}
		}
	}
}


// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding) — fields ordered by semantics, not packing
typedef struct vk_attach_desc_s  {
	VkImage descriptor;
	VkImageView *image_view;
	VkImageUsageFlags usage;
	VkMemoryRequirements reqs;
	uint32_t memoryTypeIndex;
	VkDeviceSize  memory_offset;
	// for layout transition:
	VkImageAspectFlags aspect_flags;
	VkImageLayout image_layout;
	VkFormat image_format;
} vk_attach_desc_t;

static vk_attach_desc_t attachments[ MAX_ATTACHMENTS_IN_POOL ];
static uint32_t num_attachments = 0;


static void vk_clear_attachment_pool( void )
{
	num_attachments = 0;
}


static void vk_alloc_attachments( void )
{
	VkImageViewCreateInfo view_desc;
	VkMemoryDedicatedAllocateInfoKHR alloc_info2;
	VkMemoryAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkDeviceSize offset;
	uint32_t memoryTypeBits;
	uint32_t memoryTypeIndex;
	uint32_t i;

	if ( num_attachments == 0 ) {
		return;
	}

	if ( vk.image_memory_count >= ARRAY_LEN( vk.image_memory ) ) {
		ri.Terminate( TERM_CLIENT_DROP, "vk.image_memory_count == %i", (int)ARRAY_LEN( vk.image_memory ) );
	}

	memoryTypeBits = ~0U;
	offset = 0;

	for ( i = 0; i < num_attachments; i++ ) {
#ifdef MIN_IMAGE_ALIGN
		VkDeviceSize alignment = MAX( attachments[ i ].reqs.alignment, MIN_IMAGE_ALIGN );
#else
		VkDeviceSize alignment = attachments[ i ].reqs.alignment;
#endif
		memoryTypeBits &= attachments[ i ].reqs.memoryTypeBits;
		offset = PAD( offset, alignment );
		attachments[ i ].memory_offset = offset;
		offset += attachments[ i ].reqs.size;
#ifdef _DEBUG
		ri.Log( SEV_INFO, S_COLOR_CYAN "[%i] type %i, size %i, align %i\n", i,
			attachments[ i ].reqs.memoryTypeBits,
			(int)attachments[ i ].reqs.size,
			(int)attachments[ i ].reqs.alignment );
#endif
	}

	if ( num_attachments == 1 && attachments[ 0 ].usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ) {
		// try lazy memory
		memoryTypeIndex = find_memory_type2( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryTypeIndex == ~0U ) {
			memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		}
	} else {
		memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	}

#ifdef _DEBUG
	ri.Log( SEV_INFO, "memory type bits: %04x\n", memoryTypeBits );
	ri.Log( SEV_INFO, "memory type index: %04x\n", memoryTypeIndex );
	ri.Log( SEV_INFO, "total size: %i\n", (int)offset );
#endif

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = offset;
	alloc_info.memoryTypeIndex = memoryTypeIndex;

	if ( num_attachments == 1 ) {
		if ( vk.dedicatedAllocation ) {
			memset( &alloc_info2, 0, sizeof( alloc_info2 ) );
			alloc_info2.sType =  VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
			alloc_info2.image = attachments[ 0 ].descriptor;
			alloc_info.pNext = &alloc_info2;
		}
	}

	// allocate and bind memory
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

	vk.image_memory[ vk.image_memory_count++ ] = memory;

	for ( i = 0; i < num_attachments; i++ ) {

		VK_CHECK( qvkBindImageMemory( vk.device, attachments[i].descriptor, memory, attachments[i].memory_offset ) );

		// create color image view
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.pNext = NULL;
		view_desc.flags = 0;
		view_desc.image = attachments[ i ].descriptor;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = attachments[ i ].image_format;
		view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.subresourceRange.aspectMask = attachments[ i ].aspect_flags;
		view_desc.subresourceRange.baseMipLevel = 0;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.baseArrayLayer = 0;
		view_desc.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, attachments[ i ].image_view ) );
	}

	// perform layout transition
	command_buffer = begin_command_buffer();
	for ( i = 0; i < num_attachments; i++ ) {
		record_image_layout_transition( command_buffer,
			attachments[i].descriptor,
			attachments[i].aspect_flags,
			VK_IMAGE_LAYOUT_UNDEFINED, // old_layout
			attachments[i].image_layout,
			0, 0 );
	}
	end_command_buffer( command_buffer, __func__ );

	num_attachments = 0;
}


static void vk_add_attachment_desc( VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout )
{
	if ( num_attachments >= ARRAY_LEN( attachments ) ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Attachments array overflow" );
	} else {
		attachments[ num_attachments ].descriptor = desc;
		attachments[ num_attachments ].image_view = image_view;
		attachments[ num_attachments ].usage = usage;
		attachments[ num_attachments ].reqs = *reqs;
		attachments[ num_attachments ].aspect_flags = aspect_flags;
		attachments[ num_attachments ].image_layout = image_layout;
		attachments[ num_attachments ].image_format = image_format;
		attachments[ num_attachments ].memory_offset = 0;
		num_attachments++;
	}
}


static void vk_get_image_memory_erquirements( VkImage image, VkMemoryRequirements *memory_requirements )
{
	if ( vk.dedicatedAllocation ) {
		VkMemoryRequirements2KHR memory_requirements2;
		VkImageMemoryRequirementsInfo2KHR image_requirements2;
		VkMemoryDedicatedRequirementsKHR mem_req2;

		memset( &mem_req2, 0, sizeof( mem_req2 ) );
		mem_req2.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

		image_requirements2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
		image_requirements2.image = image;
		image_requirements2.pNext = NULL;

		memset( &memory_requirements2, 0, sizeof( memory_requirements2 ) );
		memory_requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
		memory_requirements2.pNext = &mem_req2;

		qvkGetImageMemoryRequirements2KHR( vk.device, &image_requirements2, &memory_requirements2 );

		*memory_requirements = memory_requirements2.memoryRequirements;
	} else {
		qvkGetImageMemoryRequirements( vk.device, image, memory_requirements );
	}
}


static void create_color_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkFormat format,
	VkImageUsageFlags usage, VkImage *image, VkImageView *image_view, VkImageLayout image_layout, qboolean multisample )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;

	if ( multisample && !( usage & VK_IMAGE_USAGE_SAMPLED_BIT ) )
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	// create color image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = usage;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout );
}


static void create_depth_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkImage *image, VkImageView *image_view, qboolean allowTransient )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkImageAspectFlags image_aspect_flags;

	// create depth image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( vk.depthFade.active ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	} else if ( allowTransient ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	}
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
}


static void vk_create_attachments( void )
{
	uint32_t i;

	vk_clear_attachment_pool();

	// It looks like resulting performance depends from order you're creating/allocating
	// memory for attachments in vulkan i.e. similar images grouped together will provide best results
	// so [resolve0][resolve1][msaa0][msaa1][depth0][depth1] is most optimal
	// while cases like [resolve0][depth0][color0][...] is the worst

	// TODO: preallocate first image chunk in attachment' memory pool?
	if ( vk.fboActive ) {

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// bloom
		if ( r_bloom->integer ) {
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
				usage, &vk.bloom_image[0], &vk.bloom_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

			for ( i = 1; i < ARRAY_LEN( vk.bloom_image ); i += 2 ) {
				width /= 2;
				height /= 2;
				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+0], &vk.bloom_image_view[i+0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+1], &vk.bloom_image_view[i+1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			}
		}

		// post-processing/msaa-resolve
		create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &vk.color_image, &vk.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

#if FEAT_FBO_DEBUG
		ri.Log( SEV_INFO, "^3[FBO_DEBUG] FBO color attachment created:\n" );
		ri.Log( SEV_INFO, "^3[FBO_DEBUG]   size=%dx%d  format=%d  layout=SHADER_READ_ONLY\n",
			glConfig.vidWidth, glConfig.vidHeight, vk.color_format );
		ri.Log( SEV_INFO, "^3[FBO_DEBUG]   usage=%d (COLOR|SAMPLED|TRANSFER_SRC)\n",
			(int)(usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) );
#endif

		// screenmap-msaa
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {
			create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.screenMap.color_image_msaa, &vk.screenMap.color_image_view_msaa, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue );
		}

		// screenmap/msaa-resolve
		create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage, &vk.screenMap.color_image, &vk.screenMap.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

		// screenmap depth
		create_depth_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, &vk.screenMap.depth_image, &vk.screenMap.depth_image_view, qtrue );

		if ( vk.msaaActive ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.msaa_image, &vk.msaa_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue );
		}

		if ( r_ext_supersample->integer ) {
			// capture buffer
			usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			create_color_attachment( gls.captureWidth, gls.captureHeight, VK_SAMPLE_COUNT_1_BIT, vk.capture_format,
				usage, &vk.capture.image, &vk.capture.image_view, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, qfalse );
		}
	} // if ( vk.fboActive )

	//vk_alloc_attachments();

	create_depth_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, &vk.depth_image, &vk.depth_image_view,
		(vk.fboActive && (r_bloom->integer || vk.depthFade.active)) ? qfalse : qtrue );

	// depth fade: create a non-MSAA depth copy image for sampling in fragment shaders
	if ( vk.depthFade.active ) {
		VkImageCreateInfo img_desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;
		VkImageViewCreateInfo view_desc;
		VkSamplerCreateInfo sampler_desc;
		uint32_t mem_type;

		// depth copy image (transfer dst + sampled)
		memset( &img_desc, 0, sizeof( img_desc ) );
		img_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		img_desc.imageType = VK_IMAGE_TYPE_2D;
		img_desc.format = vk.depth_format;
		img_desc.extent.width = glConfig.vidWidth;
		img_desc.extent.height = glConfig.vidHeight;
		img_desc.extent.depth = 1;
		img_desc.mipLevels = 1;
		img_desc.arrayLayers = 1;
		img_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		img_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		img_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		img_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		img_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &img_desc, NULL, &vk.depthFade.image ) );

		qvkGetImageMemoryRequirements( vk.device, vk.depthFade.image, &mem_req );

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = mem_req.size;
		mem_type = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		alloc_info.memoryTypeIndex = mem_type;

		// allocate dedicated memory for this image
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.depthFade.memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.depthFade.image, vk.depthFade.memory, 0 ) );

		// image view (depth-only aspect)
		memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.depthFade.image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = vk.depth_format;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.depthFade.view ) );

		// nearest-neighbor sampler
		memset( &sampler_desc, 0, sizeof( sampler_desc ) );
		sampler_desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_desc.magFilter = VK_FILTER_NEAREST;
		sampler_desc.minFilter = VK_FILTER_NEAREST;
		sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, &vk.depthFade.sampler ) );

		SET_OBJECT_NAME( vk.depthFade.image, "depth fade copy image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.depthFade.view, "depth fade copy view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

#if FEAT_SHADOW_MAPPING
	// Shadow map depth texture
	if ( vk.shadowMap.active ) {
		VkImageCreateInfo img_desc;
		VkImageViewCreateInfo view_desc;
		VkSamplerCreateInfo sampler_desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;
		VkAttachmentDescription att;
		VkAttachmentReference depthRef;
		VkSubpassDescription subpass;
		VkRenderPassCreateInfo rpDesc;
		VkFramebufferCreateInfo fbDesc;
		VkPushConstantRange pushRange;
		VkPipelineLayoutCreateInfo plDesc;
		uint32_t mapSize = vk.shadowMap.size;
		uint32_t mem_type;

		// depth image
		memset( &img_desc, 0, sizeof( img_desc ) );
		img_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		img_desc.imageType = VK_IMAGE_TYPE_2D;
		img_desc.format = vk.depth_format;
		img_desc.extent.width = mapSize;
		img_desc.extent.height = mapSize;
		img_desc.extent.depth = 1;
		img_desc.mipLevels = 1;
		img_desc.arrayLayers = 1;
		img_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		img_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		img_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		img_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &img_desc, NULL, &vk.shadowMap.image ) );
		qvkGetImageMemoryRequirements( vk.device, vk.shadowMap.image, &mem_req );

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = mem_req.size;
		mem_type = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		alloc_info.memoryTypeIndex = mem_type;
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.shadowMap.memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.shadowMap.image, vk.shadowMap.memory, 0 ) );

		// image view
		memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.shadowMap.image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = vk.depth_format;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.shadowMap.view ) );

		// nearest sampler with border clamp (outside shadow map = lit)
		memset( &sampler_desc, 0, sizeof( sampler_desc ) );
		sampler_desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_desc.magFilter = VK_FILTER_NEAREST;
		sampler_desc.minFilter = VK_FILTER_NEAREST;
		sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler_desc.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // out-of-bounds = lit (depth=1.0)
		VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, &vk.shadowMap.sampler ) );

		// render pass (depth-only, clear on begin)
		memset( &att, 0, sizeof( att ) );
		att.format = vk.depth_format;
		att.samples = VK_SAMPLE_COUNT_1_BIT;
		att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		att.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		depthRef.attachment = 0;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pDepthStencilAttachment = &depthRef;

		memset( &rpDesc, 0, sizeof( rpDesc ) );
		rpDesc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rpDesc.attachmentCount = 1;
		rpDesc.pAttachments = &att;
		rpDesc.subpassCount = 1;
		rpDesc.pSubpasses = &subpass;
		VK_CHECK( qvkCreateRenderPass( vk.device, &rpDesc, NULL, &vk.shadowMap.renderPass ) );

		// framebuffer
		memset( &fbDesc, 0, sizeof( fbDesc ) );
		fbDesc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbDesc.renderPass = vk.shadowMap.renderPass;
		fbDesc.attachmentCount = 1;
		fbDesc.pAttachments = &vk.shadowMap.view;
		fbDesc.width = mapSize;
		fbDesc.height = mapSize;
		fbDesc.layers = 1;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &fbDesc, NULL, &vk.shadowMap.framebuffer ) );

		// pipeline layout (push constants only — lightMVP)
		pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushRange.offset = 0;
		pushRange.size = 64; // mat4

		memset( &plDesc, 0, sizeof( plDesc ) );
		plDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		plDesc.pushConstantRangeCount = 1;
		plDesc.pPushConstantRanges = &pushRange;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &plDesc, NULL, &vk.shadowMap.depthLayout ) );

		SET_OBJECT_NAME( vk.shadowMap.image, "shadow map depth image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.shadowMap.renderPass, "render pass - shadow depth", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}
#endif

	// SMAA intermediate images (goes through attachment pool)
	if ( vk.smaa.active ) {
		VkImageUsageFlags usage;

		// edges: R8G8, full resolution
		usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		create_color_attachment( glConfig.vidWidth, glConfig.vidHeight,
			VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8_UNORM,
			usage, &vk.smaa.edges_image, &vk.smaa.edges_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

		// blend weights: RGBA8, full resolution
		create_color_attachment( glConfig.vidWidth, glConfig.vidHeight,
			VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM,
			usage, &vk.smaa.blend_image, &vk.smaa.blend_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

		// input copy: same format as color_image, needs TRANSFER_DST for copy
		usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		create_color_attachment( glConfig.vidWidth, glConfig.vidHeight,
			VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage, &vk.smaa.input_image, &vk.smaa.input_view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
	}

	vk_alloc_attachments();

	for ( i = 0; i < vk.image_memory_count; i++ )
	{
		SET_OBJECT_NAME( vk.image_memory[i], va( "framebuffer memory chunk %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
	}

	SET_OBJECT_NAME( vk.depth_image, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.depth_image_view, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	SET_OBJECT_NAME( vk.color_image, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.color_image_view, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	SET_OBJECT_NAME( vk.capture.image, "capture image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.capture.image_view, "capture image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ )
	{
		SET_OBJECT_NAME( vk.bloom_image[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.bloom_image_view[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	// SMAA LUT textures: area and search (dedicated memory, one-shot upload)
	if ( vk.smaa.active ) {
		VkImageCreateInfo img_desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;
		VkImageViewCreateInfo view_desc;
		VkSamplerCreateInfo sampler_desc;
		VkCommandBuffer command_buffer;
		VkBufferCreateInfo buf_desc;
		VkBuffer staging_buf;
		VkDeviceMemory staging_mem;
		VkDeviceSize staging_size;
		VkBufferImageCopy region;
		byte *mapped;
		uint32_t mem_type;

		// --- Area texture (160x560, R8G8) ---
		memset( &img_desc, 0, sizeof( img_desc ) );
		img_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		img_desc.imageType = VK_IMAGE_TYPE_2D;
		img_desc.format = VK_FORMAT_R8G8_UNORM;
		img_desc.extent.width = AREATEX_WIDTH;
		img_desc.extent.height = AREATEX_HEIGHT;
		img_desc.extent.depth = 1;
		img_desc.mipLevels = 1;
		img_desc.arrayLayers = 1;
		img_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		img_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		img_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		img_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		img_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &img_desc, NULL, &vk.smaa.area_image ) );

		qvkGetImageMemoryRequirements( vk.device, vk.smaa.area_image, &mem_req );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = mem_req.size;
		mem_type = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		alloc_info.memoryTypeIndex = mem_type;

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.smaa.area_memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.smaa.area_image, vk.smaa.area_memory, 0 ) );

		memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.smaa.area_image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = VK_FORMAT_R8G8_UNORM;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.smaa.area_view ) );

		// --- Search texture (64x16, R8) ---
		img_desc.format = VK_FORMAT_R8_UNORM;
		img_desc.extent.width = SEARCHTEX_WIDTH;
		img_desc.extent.height = SEARCHTEX_HEIGHT;

		VK_CHECK( qvkCreateImage( vk.device, &img_desc, NULL, &vk.smaa.search_image ) );

		qvkGetImageMemoryRequirements( vk.device, vk.smaa.search_image, &mem_req );
		alloc_info.allocationSize = mem_req.size;
		mem_type = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		alloc_info.memoryTypeIndex = mem_type;

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.smaa.search_memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.smaa.search_image, vk.smaa.search_memory, 0 ) );

		view_desc.image = vk.smaa.search_image;
		view_desc.format = VK_FORMAT_R8_UNORM;

		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.smaa.search_view ) );

		// --- Samplers ---
		memset( &sampler_desc, 0, sizeof( sampler_desc ) );
		sampler_desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		// point sampler (for edges and search)
		sampler_desc.magFilter = VK_FILTER_NEAREST;
		sampler_desc.minFilter = VK_FILTER_NEAREST;
		VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, &vk.smaa.point_sampler ) );

		// linear sampler (for area, blend, input)
		sampler_desc.magFilter = VK_FILTER_LINEAR;
		sampler_desc.minFilter = VK_FILTER_LINEAR;
		VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, &vk.smaa.linear_sampler ) );

		// --- Upload LUT data via staging buffer ---
		staging_size = AREATEX_SIZE + SEARCHTEX_SIZE;

		memset( &buf_desc, 0, sizeof( buf_desc ) );
		buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_desc.size = staging_size;
		buf_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, &staging_buf ) );

		qvkGetBufferMemoryRequirements( vk.device, staging_buf, &mem_req );
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &staging_mem ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, staging_buf, staging_mem, 0 ) );

		VK_CHECK( qvkMapMemory( vk.device, staging_mem, 0, VK_WHOLE_SIZE, 0, (void **)&mapped ) );
		memcpy( mapped, areaTexBytes, AREATEX_SIZE );
		memcpy( mapped + AREATEX_SIZE, searchTexBytes, SEARCHTEX_SIZE );
		qvkUnmapMemory( vk.device, staging_mem );

		// record copy commands
		command_buffer = begin_command_buffer();

		// transition area image to TRANSFER_DST
		record_image_layout_transition( command_buffer, vk.smaa.area_image,
			VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

		// transition search image to TRANSFER_DST
		record_image_layout_transition( command_buffer, vk.smaa.search_image,
			VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

		// copy area texture
		memset( &region, 0, sizeof( region ) );
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent.width = AREATEX_WIDTH;
		region.imageExtent.height = AREATEX_HEIGHT;
		region.imageExtent.depth = 1;

		qvkCmdCopyBufferToImage( command_buffer, staging_buf, vk.smaa.area_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

		// copy search texture
		region.bufferOffset = AREATEX_SIZE;
		region.imageExtent.width = SEARCHTEX_WIDTH;
		region.imageExtent.height = SEARCHTEX_HEIGHT;

		qvkCmdCopyBufferToImage( command_buffer, staging_buf, vk.smaa.search_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

		// transition both to SHADER_READ_ONLY
		record_image_layout_transition( command_buffer, vk.smaa.area_image,
			VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

		record_image_layout_transition( command_buffer, vk.smaa.search_image,
			VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

		end_command_buffer( command_buffer, __func__ );

		// free staging resources
		qvkDestroyBuffer( vk.device, staging_buf, NULL );
		qvkFreeMemory( vk.device, staging_mem, NULL );

		SET_OBJECT_NAME( vk.smaa.area_image, "SMAA area LUT", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.smaa.area_view, "SMAA area LUT view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		SET_OBJECT_NAME( vk.smaa.search_image, "SMAA search LUT", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.smaa.search_view, "SMAA search LUT view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		SET_OBJECT_NAME( vk.smaa.edges_image, "SMAA edges image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.smaa.blend_image, "SMAA blend image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.smaa.input_image, "SMAA input copy", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	}
}


static void vk_create_framebuffers( void )
{
	VkImageView attachments[3];
	VkFramebufferCreateInfo desc;
	uint32_t n;

	desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.layers = 1;

	for ( n = 0; n < vk.swapchain_image_count; n++ )
	{
		desc.renderPass = vk.render_pass.main;
		desc.attachmentCount = 2;
		if ( r_fbo->integer == 0 )
		{
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			attachments[0] = vk.swapchain_image_views[n];
			attachments[1] = vk.depth_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.main[n], va( "framebuffer - main %i", n ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
		else
		{
			// same framebuffer configuration for main and post-bloom render passes
			if ( n == 0 )
			{
				desc.width = glConfig.vidWidth;
				desc.height = glConfig.vidHeight;
				attachments[0] = vk.color_image_view;
				attachments[1] = vk.depth_image_view;
				if ( vk.msaaActive )
				{
					desc.attachmentCount = 3;
					attachments[2] = vk.msaa_image_view;
				}
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );
				SET_OBJECT_NAME( vk.framebuffers.main[n], "framebuffer - main", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
			else
			{
				vk.framebuffers.main[n] = vk.framebuffers.main[0];
			}

			// gamma correction
			desc.renderPass = vk.render_pass.gamma;
			desc.attachmentCount = 1;
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			attachments[0] = vk.swapchain_image_views[n];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.gamma[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.gamma[n], "framebuffer - gamma-correction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}

	if ( vk.fboActive )
	{
		// screenmap
		desc.renderPass = vk.render_pass.screenmap;
		desc.attachmentCount = 2;
		desc.width = vk.screenMapWidth;
		desc.height = vk.screenMapHeight;
		attachments[0] = vk.screenMap.color_image_view;
		attachments[1] = vk.screenMap.depth_image_view;
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		{
			desc.attachmentCount = 3;
			attachments[2] = vk.screenMap.color_image_view_msaa;
		}
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.screenmap ) );
		SET_OBJECT_NAME( vk.framebuffers.screenmap, "framebuffer - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		if ( vk.capture.image != VK_NULL_HANDLE )
		{
			attachments[0] = vk.capture.image_view;

			desc.renderPass = vk.render_pass.capture;
			desc.pAttachments = attachments;
			desc.attachmentCount = 1;
			desc.width = gls.captureWidth;
			desc.height = gls.captureHeight;

			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.capture ) );
			SET_OBJECT_NAME( vk.framebuffers.capture, "framebuffer - capture", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		if ( r_bloom->integer )
		{
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			// bloom color extraction
			desc.renderPass = vk.render_pass.bloom_extract;
			desc.width = width;
			desc.height = height;

			desc.attachmentCount = 1;
			attachments[0] = vk.bloom_image_view[0];

			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.bloom_extract ) );

			SET_OBJECT_NAME( vk.framebuffers.bloom_extract, "framebuffer - bloom extraction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n += 2 )
			{
				width /= 2;
				height /= 2;

				desc.renderPass = vk.render_pass.blur[n];
				desc.width = width;
				desc.height = height;

				desc.attachmentCount = 1;

				attachments[0] = vk.bloom_image_view[n+0+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+0] ) );

				attachments[0] = vk.bloom_image_view[n+1+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+1] ) );

				SET_OBJECT_NAME( vk.framebuffers.blur[n+0], va( "framebuffer - blur %i", n+0 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
				SET_OBJECT_NAME( vk.framebuffers.blur[n+1], va( "framebuffer - blur %i", n+1 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
		}

		// SMAA framebuffers
		if ( vk.smaa.active ) {
			desc.attachmentCount = 1;
			desc.width = glConfig.vidWidth;
			desc.height = glConfig.vidHeight;

			attachments[0] = vk.smaa.edges_view;
			desc.renderPass = vk.render_pass.smaa_edge;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_edge ) );
			SET_OBJECT_NAME( vk.framebuffers.smaa_edge, "framebuffer - smaa_edge", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			attachments[0] = vk.smaa.blend_view;
			desc.renderPass = vk.render_pass.smaa_blend;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_blend ) );
			SET_OBJECT_NAME( vk.framebuffers.smaa_blend, "framebuffer - smaa_blend", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			attachments[0] = vk.color_image_view;
			desc.renderPass = vk.render_pass.smaa_resolve;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_resolve ) );
			SET_OBJECT_NAME( vk.framebuffers.smaa_resolve, "framebuffer - smaa_resolve", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}
}


static void vk_create_sync_primitives( void ) {
	VkSemaphoreCreateInfo desc;
	VkFenceCreateInfo fence_desc;
	uint32_t i;

	desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.image_uploaded2 ) );
#endif

	// all commands submitted
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;

		// swapchain image acquired
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].image_acquired ) );

#ifdef USE_UPLOAD_QUEUE
		// second semaphore to synchronize additional tasks (e.g. image upload)
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].rendering_finished2 ) );
#endif
		fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_desc.pNext = NULL;
		//fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so it can be used to start rendering
		fence_desc.flags = 0; // non-signalled state

		VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.tess[i].rendering_finished_fence ) );
		vk.tess[i].waitForFence = qfalse;

		SET_OBJECT_NAME( vk.tess[i].image_acquired, va( "image_acquired semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#ifdef USE_UPLOAD_QUEUE
		SET_OBJECT_NAME( vk.tess[i].rendering_finished2, va( "rendering_finished2 semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#endif
		SET_OBJECT_NAME( vk.tess[i].rendering_finished_fence, va( "rendering_finished fence %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );
	}

	fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_desc.pNext = NULL;
	fence_desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.aux_fence ) );
	SET_OBJECT_NAME( vk.aux_fence, "aux fence", VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
	vk.aux_fence_wait = qfalse;
#endif
}


static void vk_destroy_sync_primitives( void  ) {
	uint32_t i;

#ifdef USE_UPLOAD_QUEUE
	qvkDestroySemaphore( vk.device, vk.image_uploaded2, NULL );
#endif

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroySemaphore( vk.device, vk.tess[i].image_acquired, NULL );
#ifdef USE_UPLOAD_QUEUE
		qvkDestroySemaphore( vk.device, vk.tess[i].rendering_finished2, NULL );
#endif
		qvkDestroyFence( vk.device, vk.tess[i].rendering_finished_fence, NULL );
		vk.tess[i].waitForFence = qfalse;
		vk.tess[i].swapchain_image_acquired = qfalse;
	}

#ifdef USE_UPLOAD_QUEUE
	qvkDestroyFence( vk.device, vk.aux_fence, NULL );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
#endif
}


static void vk_destroy_framebuffers( void ) {
	uint32_t n;

	for ( n = 0; n < vk.swapchain_image_count; n++ ) {
		if ( vk.framebuffers.main[n] != VK_NULL_HANDLE ) {
			if ( !vk.fboActive || n == 0 ) {
				qvkDestroyFramebuffer( vk.device, vk.framebuffers.main[n], NULL );
			}
			vk.framebuffers.main[n] = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.gamma[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.gamma[n], NULL );
			vk.framebuffers.gamma[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.bloom_extract, NULL );
		vk.framebuffers.bloom_extract = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.screenmap, NULL );
		vk.framebuffers.screenmap = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.capture != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.capture, NULL );
		vk.framebuffers.capture = VK_NULL_HANDLE;
	}

	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n++ ) {
		if ( vk.framebuffers.blur[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.blur[n], NULL );
			vk.framebuffers.blur[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.smaa_edge != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_edge, NULL );
		vk.framebuffers.smaa_edge = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.smaa_blend != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_blend, NULL );
		vk.framebuffers.smaa_blend = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.smaa_resolve != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_resolve, NULL );
		vk.framebuffers.smaa_resolve = VK_NULL_HANDLE;
	}
}


static void vk_destroy_swapchain( void ) {
	uint32_t i;

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		if ( vk.swapchain_image_views[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, vk.swapchain_image_views[i], NULL );
			vk.swapchain_image_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.swapchain_rendering_finished[i] != VK_NULL_HANDLE ) {
			qvkDestroySemaphore( vk.device, vk.swapchain_rendering_finished[i], NULL );
			vk.swapchain_rendering_finished[i] = VK_NULL_HANDLE;
		}
	}

	qvkDestroySwapchainKHR( vk.device, vk.swapchain, NULL );
	vk.swapchain = VK_NULL_HANDLE;
	memset( vk.swapchain_images, 0, sizeof( vk.swapchain_images ) );
	vk.swapchain_image_count = 0;
}

static void vk_destroy_attachments( void );
static void vk_destroy_render_passes( void );
static void vk_destroy_pipelines( qboolean resetCount );

static void vk_restart_swapchain( const char *funcname, VkResult res )
{
	uint32_t i;

#ifdef _DEBUG
	ri.Log( SEV_WARN, "%s(%s): restarting swapchain...\n", funcname, vk_result_string( res ) );
#else
	ri.Log( SEV_WARN, "%s(): restarting swapchain...\n", funcname );
#endif

	vk_wait_idle();

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkResetCommandBuffer( vk.tess[i].command_buffer, 0 );
	}

#ifdef USE_UPLOAD_QUEUE
	qvkResetCommandBuffer( vk.staging_command_buffer, 0 );
#endif

	vk_destroy_pipelines( qfalse );
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain();
	vk_destroy_sync_primitives();

	vk_select_surface_format( vk.physical_device, vk_surface );
	setup_surface_formats( vk.physical_device );

	vk_create_sync_primitives();
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qfalse );
	vk_create_attachments();
	vk_create_render_passes();
	vk_create_framebuffers();

	vk_update_attachment_descriptors();

	vk_update_post_process_pipelines();
}


static void vk_set_render_scale( void )
{
	if ( gls.windowWidth != glConfig.vidWidth || gls.windowHeight != glConfig.vidHeight )
	{
		if ( r_renderScale->integer > 0 )
		{
			int scaleMode = r_renderScale->integer - 1;
			if ( scaleMode & 1 )
			{
				// preserve aspect ratio (black bars on sides)
				float windowAspect = (float) gls.windowWidth / (float) gls.windowHeight;
				float renderAspect = (float) glConfig.vidWidth / (float) glConfig.vidHeight;
				if ( windowAspect >= renderAspect )
				{
					float scale = (float)gls.windowHeight / ( float ) glConfig.vidHeight;
					int bias = ( gls.windowWidth - scale * (float) glConfig.vidWidth ) / 2;
					vk.blitX0 += bias;
				}
				else
				{
					float scale = (float)gls.windowWidth / ( float ) glConfig.vidWidth;
					int bias = ( gls.windowHeight - scale * (float) glConfig.vidHeight ) / 2;
					vk.blitY0 += bias;
				}
			}
			// linear filtering
			if ( scaleMode & 2 )
				vk.blitFilter = GL_LINEAR;
			else
				vk.blitFilter = GL_NEAREST;
		}

		vk.windowAdjusted = qtrue;
	}

	if ( r_fbo->integer && r_ext_supersample->integer && !r_renderScale->integer )
	{
		vk.blitFilter = GL_LINEAR;
	}
}


static void vk_fence_thread_start( void );
static void vk_fence_thread_stop( void );
static void vk_gpu_ts_init( void );
static void vk_gpu_ts_shutdown( void );
static void vk_gpu_ts_write( const char *label );

void vk_initialize( void )
{
	char buf[64], driver_version[64];
	const char *vendor_name;
	VkPhysicalDeviceProperties props;
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	uint32_t maxSize;
	uint32_t i;

	init_vulkan_library();

	qvkGetDeviceQueue( vk.device, vk.queue_family_index, 0, &vk.queue );

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	vk.cmd = vk.tess + 0;
	vk.uniform_alignment = props.limits.minUniformBufferOffsetAlignment;
	vk.uniform_item_size = PAD( (uint32_t)sizeof( vkUniform_t ), vk.uniform_alignment );

	// for flare visibility tests
	vk.storage_alignment = MAX( props.limits.minStorageBufferOffsetAlignment, sizeof( uint32_t ) );

	vk.maxAnisotropy = props.limits.maxSamplerAnisotropy;

	vk.timestampPeriodNs  = props.limits.timestampPeriod;
	vk.timestampSupported = ( props.limits.timestampComputeAndGraphics && vk.timestampPeriodNs > 0.0f ) ? qtrue : qfalse;

	vk.blitFilter = GL_NEAREST;
	vk.windowAdjusted = qfalse;
	vk.blitX0 = vk.blitY0 = 0;

	vk_set_render_scale();

	if ( r_fbo->integer ) {
		vk.fboActive = qtrue;
		if ( r_ext_multisample->integer ) {
			vk.msaaActive = qtrue;
		}
	} else {
		vk.fboActive = qfalse;
	}

	// depth fade requires FBO for the depth copy — also needed for SSAO and god rays
	vk.depthFade.active = ( vk.fboActive && ( r_depthFade->integer
#if FEAT_SSAO
		|| r_ssao->integer
#endif
#if FEAT_GODRAYS
		|| r_godRays->integer
#endif
	) ) ? qtrue : qfalse;

#if FEAT_SHADOW_MAPPING
	{
		cvar_t *r_shadowMapping = ri.Cvar_Get( "r_shadowMapping", "0", 0 );
		cvar_t *r_shadowMapSize = ri.Cvar_Get( "r_shadowMapSize", "512", 0 );
		vk.shadowMap.active = ( r_shadowMapping->integer && vk.fboActive ) ? qtrue : qfalse;
		vk.shadowMap.size = r_shadowMapSize->integer;
		if ( vk.shadowMap.size < 256 ) vk.shadowMap.size = 256;
		if ( vk.shadowMap.size > 2048 ) vk.shadowMap.size = 2048;
	}
#endif

	// SMAA anti-aliasing requires FBO
	vk.smaa.active = ( r_smaa->integer && vk.fboActive ) ? qtrue : qfalse;
	vk.smaa.quality = r_smaa->integer;
	if ( vk.smaa.active ) {
		const char *names[] = { "", "Low", "Medium", "High", "Ultra" };
		ri.Log( SEV_INFO, "...SMAA: %s quality\n", names[vk.smaa.quality] );
		if ( r_ext_multisample->integer ) {
			ri.Log( SEV_WARN, "SMAA and MSAA are both active. MSAA is redundant with SMAA, consider r_ext_multisample 0\n" );
		}
	}

	// multisampling

	vkMaxSamples = MIN( props.limits.sampledImageColorSampleCounts, props.limits.sampledImageDepthSampleCounts );

	if ( /*vk.fboActive &&*/ vk.msaaActive ) {
		VkSampleCountFlags mask = vkMaxSamples;
		vkSamples = MAX( log2pad( r_ext_multisample->integer, 1 ), VK_SAMPLE_COUNT_2_BIT );
		while ( vkSamples > mask )
				vkSamples >>= 1;
		ri.Log( SEV_INFO, "...using %ix MSAA\n", vkSamples );
	} else {
		vkSamples = VK_SAMPLE_COUNT_1_BIT;
	}

	vk.screenMapSamples = MIN( vkMaxSamples, VK_SAMPLE_COUNT_4_BIT );

	vk.screenMapWidth = (float) glConfig.vidWidth / 16.0;
	if ( vk.screenMapWidth < 4 )
		vk.screenMapWidth = 4;

	vk.screenMapHeight = (float) glConfig.vidHeight / 16.0;
	if ( vk.screenMapHeight < 4 )
		vk.screenMapHeight = 4;

	vk.defaults.geometry_size = VERTEX_BUFFER_SIZE;
	vk.defaults.staging_size = STAGING_BUFFER_SIZE;

	// get memory size & defaults
	{
		VkPhysicalDeviceMemoryProperties props;
		VkDeviceSize maxDedicatedSize = 0;
		VkDeviceSize maxBARSize = 0;
		qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &props );
		for ( i = 0; i < props.memoryTypeCount; i++ ) {
			if ( props.memoryTypes[i].propertyFlags == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				maxDedicatedSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
			}
			else if ( props.memoryTypes[i].propertyFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				if ( maxDedicatedSize == 0 || props.memoryHeaps[props.memoryTypes[i].heapIndex].size > maxDedicatedSize ) {
					maxDedicatedSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
				}
			}
			if ( props.memoryTypes[i].propertyFlags == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				maxBARSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
			}
			else if ( (props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				if ( maxBARSize == 0 ) {
					maxBARSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
				}
			}
		}

		if ( maxDedicatedSize != 0 ) {
			ri.Log( SEV_INFO, "...device memory size: %iMB\n", (int)((maxDedicatedSize + (1024 * 1024) - 1) / (1024 * 1024)) );
		}
		if ( maxBARSize != 0 ) {
			if ( maxBARSize >= 128 * 1024 * 1024 ) {
				// user larger buffers to avoid potential reallocations
				vk.defaults.geometry_size = VERTEX_BUFFER_SIZE_HI;
				vk.defaults.staging_size = STAGING_BUFFER_SIZE_HI;
			}
#ifdef _DEBUG
			ri.Log( SEV_INFO, "...BAR memory size: %iMB\n", (int)((maxBARSize + (1024 * 1024) - 1) / (1024 * 1024)) );
#endif
		}
	}

	// fill glConfig information

	// maxTextureSize must not exceed IMAGE_CHUNK_SIZE
	maxSize = sqrtf( IMAGE_CHUNK_SIZE / 4 );
	// round down to next power of 2
	glConfig.maxTextureSize = MIN( props.limits.maxImageDimension2D, log2pad( maxSize, 0 ) );

	if ( glConfig.maxTextureSize > MAX_TEXTURE_SIZE )
		glConfig.maxTextureSize = MAX_TEXTURE_SIZE; // ResampleTexture() relies on that maximum

	// default chunk size, may be doubled on demand
	vk.image_chunk_size = IMAGE_CHUNK_SIZE;

	vk.maxLod = 1 + Q_log2( glConfig.maxTextureSize );

	if ( props.limits.maxPerStageDescriptorSamplers != 0xFFFFFFFF )
		glConfig.numTextureUnits = props.limits.maxPerStageDescriptorSamplers;
	else
		glConfig.numTextureUnits = props.limits.maxBoundDescriptorSets;
	if ( glConfig.numTextureUnits > MAX_TEXTURE_UNITS )
		glConfig.numTextureUnits = MAX_TEXTURE_UNITS;

	vk.maxBoundDescriptorSets = props.limits.maxBoundDescriptorSets;

	if ( r_ext_texture_env_add->integer != 0 )
		glConfig.textureEnvAddAvailable = qtrue;
	else
		glConfig.textureEnvAddAvailable = qfalse;

	glConfig.textureCompression = TC_NONE;

	major = VK_VERSION_MAJOR(props.apiVersion);
	minor = VK_VERSION_MINOR(props.apiVersion);
	patch = VK_VERSION_PATCH(props.apiVersion);

	// decode driver version
	switch ( props.vendorID ) {
		case 0x10DE: // NVidia
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i.%i",
				(props.driverVersion >> 22) & 0x3FF,
				(props.driverVersion >> 14) & 0x0FF,
				(props.driverVersion >> 6) & 0x0FF,
				(props.driverVersion >> 0) & 0x03F );
			break;
#ifdef _WIN32
		case 0x8086: // Intel
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i",
				(props.driverVersion >> 14),
				(props.driverVersion >> 0) & 0x3FFF );
			break;
#endif
		default:
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i",
				(props.driverVersion >> 22),
				(props.driverVersion >> 12) & 0x3FF,
				(props.driverVersion >> 0) & 0xFFF );
	}

	Com_sprintf( glConfig.version_string, sizeof( glConfig.version_string ), "API: %i.%i.%i, Driver: %s",
		major, minor, patch, driver_version );

#ifdef _WIN32
	// Intel iGPU drivers from 101.5333 to 101.6737 have a known bug that causes
	// VK_ERROR_DEVICE_LOST during vkQueueSubmit, see https://github.com/ec-/Quake3e/issues/312
	if ( props.vendorID == 0x8086 ) {
		uint32_t drvMajor = props.driverVersion >> 14;
		uint32_t drvMinor = props.driverVersion & 0x3FFF;
		if ( drvMajor == 101 && drvMinor >= 5333 && drvMinor <= 6737 ) {
			Com_sprintf( vk.driverNote, sizeof( vk.driverNote ), S_COLOR_WARNING
				"\nWARNING: Intel driver %i.%i is known to cause Vulkan crashes.\n"
				"Consider updating to driver >= 101.6790 or downgrading to <= 101.5186.\n",
				drvMajor, drvMinor );
		}
	}
#endif

	vk.offscreenRender = qtrue;

	if ( props.vendorID == 0x1002 ) {
		vendor_name = "Advanced Micro Devices, Inc.";
	} else if ( props.vendorID == 0x106B ) {
		vendor_name = "Apple Inc.";
	} else if ( props.vendorID == 0x10DE ) {
		// https://github.com/SaschaWillems/Vulkan/issues/493
		// we can't render to offscreen presentation surfaces on nvidia
		vk.offscreenRender = qfalse;
		vendor_name = "NVIDIA";
	} else if ( props.vendorID == 0x14E4 ) {
		vendor_name = "Broadcom Inc.";
	} else if ( props.vendorID == 0x1AE0 ) {
		vendor_name = "Google Inc.";
	} else if ( props.vendorID == 0x8086 ) {
		vendor_name = "Intel Corporation";
	} else if ( props.vendorID == VK_VENDOR_ID_MESA ) {
		vendor_name = "MESA";
	} else {
		Com_sprintf( buf, sizeof( buf ), "VendorID: %04x", props.vendorID );
		vendor_name = buf;
	}

	Q_strncpyz( glConfig.vendor_string, vendor_name, sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, renderer_name( &props ), sizeof( glConfig.renderer_string ) );

	SET_OBJECT_NAME( (intptr_t)vk.device, glConfig.renderer_string, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT );

	// do early texture mode setup to avoid redundant descriptor updates in GL_SetDefaultState()
	vk.samplers.filter_min = -1;
	vk.samplers.filter_max = -1;
	GL_TextureMode( r_textureMode->string );

	//
	// Sync primitives.
	//
	vk_create_sync_primitives();

	//
	// Command pool.
	//
	{
		VkCommandPoolCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		desc.queueFamilyIndex = vk.queue_family_index;

		VK_CHECK( qvkCreateCommandPool( vk.device, &desc, NULL, &vk.command_pool ) );

		SET_OBJECT_NAME( vk.command_pool, "command pool", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT );
	}

#ifdef USE_UPLOAD_QUEUE
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.staging_command_buffer ) );
	}
#endif

	//
	// Command buffers and color attachments.
	//
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.tess[i].command_buffer ) );

		//SET_OBJECT_NAME( vk.tess[i].command_buffer, va( "command buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}

	//
	// Descriptor pool.
	//
	{
		VkDescriptorPoolSize pool_size[5];
		VkDescriptorPoolCreateInfo desc;
		uint32_t i, maxSets;

		pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		// MAX_DRAWIMAGES per-image samplers + 9 specific ones (color,
		// screenmap, bloom variants, depth fade, smaa) + 384 for the
		// three primitive sampler arrays: particle binding 3, ribbon
		// binding 2, beam binding 1 — each 64 slots × NUM_COMMAND_BUFFERS
		// (= 2) descriptor sets = 128 entries, three primitives = 384.
		// Without this explicit budget the primitive arrays would
		// silently eat into MAX_DRAWIMAGES headroom and pool exhaustion
		// could fire on heavy maps that load close to MAX_DRAWIMAGES
		// images.
		pool_size[0].descriptorCount = MAX_DRAWIMAGES + 1 + 1 + 1 + VK_NUM_BLOOM_PASSES * 2 + 1 + 5
		                             + 384;

		pool_size[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		pool_size[1].descriptorCount = NUM_COMMAND_BUFFERS;

		//pool_size[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		//pool_size[2].descriptorCount = NUM_COMMAND_BUFFERS;

		pool_size[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		pool_size[2].descriptorCount = 1 + NUM_COMMAND_BUFFERS; // generic storage + reserved per-frame slots

		pool_size[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		// First-launch consumption (ribbon, sprite, particle, beam
		// each double-allocate — once in vk_init_*, once in
		// vk_init_descriptors's re-alloc block; a vid_restart skips
		// the first allocation since vk.active stays true and
		// vk_initialize doesn't re-run):
		//   ribbon:   2 SSBOs × 2 cmd × 2 alloc =  8
		//   sprite:   1 SSBO  × 2 cmd × 2 alloc =  4
		//   particle: ~10 SSBOs across compute+render × 2 alloc ≈ 20
		//   beam:     3 SSBOs (header + stages + counts) × 2 cmd × 2 alloc = 12
		//   total: ≈ 44.
		// vid_restart-only path: ≈ 22 (single allocation).
		// 64-slot pool gives ~31% headroom (44/64) post-Phase 5G; the
		// underlying double-alloc waste is tracked in docs/health.md
		// and deferred to the HAL refactor that will rebuild
		// descriptor management entirely.
		pool_size[3].descriptorCount = 64;

		// IQM GPU skinning: per-frame bone matrix UBOs (non-dynamic uniform buffers)
		// Particle subsystem also uses non-dynamic UBOs for per-frame
		// state (NUM_COMMAND_BUFFERS slots × 2 descriptor sets [compute
		// and render] = 4). Total: NUM_COMMAND_BUFFERS for IQM + 4 for
		// particle, with headroom.
		pool_size[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_size[4].descriptorCount = NUM_COMMAND_BUFFERS + 8;

		for ( i = 0, maxSets = 0; i < ARRAY_LEN( pool_size ); i++ ) {
			maxSets += pool_size[i].descriptorCount;
		}

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.maxSets = maxSets;
		desc.poolSizeCount = ARRAY_LEN( pool_size );
		desc.pPoolSizes = pool_size;

		VK_CHECK( qvkCreateDescriptorPool( vk.device, &desc, NULL, &vk.descriptor_pool ) );
	}

	//
	// Descriptor set layout.
	//
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_sampler );
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, &vk.set_layout_uniform );
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, &vk.set_layout_storage );
	//vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_input );

	//
	// Pipeline layouts.
	//
	{
		VkDescriptorSetLayout set_layouts[8]; // sized for potential parallax set 6
		VkPipelineLayoutCreateInfo desc;
		VkPushConstantRange push_range;
#if FEAT_FOG_SYSTEM
		VkPushConstantRange push_ranges[2];
#endif

		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = 64; // 16 floats

#if FEAT_FOG_SYSTEM
		// Range 0: MVP matrix (vertex stage, bytes 0-63)
		push_ranges[0] = push_range;
		// Range 1: Enhanced fog parameters (fragment stage, bytes 64-95).
		// Consumed by vk_update_fog_push; reserved even if no shader
		// currently reads it so the layout is forward-compatible.
		push_ranges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_ranges[1].offset = 64;
		push_ranges[1].size = 32;
#endif

		// standard pipelines
		set_layouts[0] = vk.set_layout_uniform; // fog/dlight parameters
		set_layouts[1] = vk.set_layout_sampler; // diffuse
		set_layouts[2] = vk.set_layout_sampler; // lightmap / fog-only
		set_layouts[3] = vk.set_layout_sampler; // blend
		set_layouts[4] = vk.set_layout_sampler; // collapsed fog texture
		set_layouts[5] = vk.set_layout_sampler; // depth fade texture
		set_layouts[6] = vk.set_layout_sampler; // normalmap (parallax) or Q1 anim next-frame
		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		{
			int layoutCount = (vk.maxBoundDescriptorSets >= VK_DESC_COUNT) ? VK_DESC_COUNT : 4;
			if ( vk.maxBoundDescriptorSets >= 7 )
				layoutCount = 7; // set=6: normalmap (parallax) or Q1 anim next-frame sampler
			desc.setLayoutCount = layoutCount;
		}
		desc.pSetLayouts = set_layouts;
#if FEAT_FOG_SYSTEM
		desc.pushConstantRangeCount = 2;
		desc.pPushConstantRanges = push_ranges;
#else
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;
#endif

		VK_CHECK(qvkCreatePipelineLayout(vk.device, &desc, NULL, &vk.pipeline_layout));

		// MSDF pipeline layout: 128 bytes (64 MVP + 64 outline/glow/shadow params), VERTEX|FRAGMENT
		{
			VkPushConstantRange msdfPush;
			msdfPush.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			msdfPush.offset = 0;
			msdfPush.size = 128; // 64 (mat4 mvp) + 4 (outlineWidth) + 4 (glowWidth) + 8 (shadowOffset) + 16 (outlineColor) + 16 (glowColor) + 16 (shadowColor)

			VkPipelineLayoutCreateInfo msdfLayoutInfo;
			memset(&msdfLayoutInfo, 0, sizeof(msdfLayoutInfo));
			msdfLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			msdfLayoutInfo.setLayoutCount = desc.setLayoutCount;
			msdfLayoutInfo.pSetLayouts = set_layouts;
			msdfLayoutInfo.pushConstantRangeCount = 1;
			msdfLayoutInfo.pPushConstantRanges = &msdfPush;

			VK_CHECK(qvkCreatePipelineLayout(vk.device, &msdfLayoutInfo, NULL, &vk.pipeline_layout_msdf));
			SET_OBJECT_NAME(vk.pipeline_layout_msdf, "pipeline layout: MSDF text", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
		}

		// flare test pipeline
		set_layouts[0] = vk.set_layout_storage; // dynamic storage buffer

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_storage ) );

		// post-processing pipeline
		set_layouts[0] = vk.set_layout_sampler; // sampler
		set_layouts[1] = vk.set_layout_sampler; // sampler
		set_layouts[2] = vk.set_layout_sampler; // sampler
		set_layouts[3] = vk.set_layout_sampler; // sampler

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_post_process ) );

		desc.setLayoutCount = VK_NUM_BLOOM_PASSES;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_blend ) );

#if FEAT_SSAO
		// SSAO pipeline layout: 2 sampler sets (color + depth)
		desc.setLayoutCount = 2;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_ssao ) );
#endif

#if FEAT_GODRAYS
		{
			// Godrays pipeline layout: 2 samplers + push constants for sun position
			VkPushConstantRange godrayPush;
			godrayPush.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			godrayPush.offset = 0;
			godrayPush.size = 16; // vec2 sunScreenPos + float intensity + float decay
			desc.setLayoutCount = 2;
			desc.pushConstantRangeCount = 1;
			desc.pPushConstantRanges = &godrayPush;
			VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_godrays ) );
			desc.pushConstantRangeCount = 0;
			desc.pPushConstantRanges = NULL;
		}
#endif

		// SMAA pipeline layout: 3 sampler sets + push constants (vec4 rtMetrics)
		if ( vk.smaa.active ) {
			push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			push_range.offset = 0;
			push_range.size = 16; // vec4: 1/w, 1/h, w, h

			set_layouts[0] = vk.set_layout_sampler;
			set_layouts[1] = vk.set_layout_sampler;
			set_layouts[2] = vk.set_layout_sampler;

			desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			desc.pNext = NULL;
			desc.flags = 0;
			desc.setLayoutCount = 3;
			desc.pSetLayouts = set_layouts;
			desc.pushConstantRangeCount = 1;
			desc.pPushConstantRanges = &push_range;

			VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_smaa ) );
			SET_OBJECT_NAME( vk.pipeline_layout_smaa, "pipeline layout - smaa", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		}

		SET_OBJECT_NAME( vk.pipeline_layout, "pipeline layout - main", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_post_process, "pipeline layout - post-processing", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_blend, "pipeline layout - blend", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
	}

	vk.geometry_buffer_size_new = vk.defaults.geometry_size;
	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	vk_create_storage_buffer( MAX_FLARES * vk.storage_alignment );

	vk_create_shader_modules();

	{
		VkPipelineCacheCreateInfo ci;
		memset( &ci, 0, sizeof( ci ) );
		ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK( qvkCreatePipelineCache( vk.device, &ci, NULL, &vk.pipelineCache ) );
	}

	vk.renderPassIndex = RENDER_PASS_MAIN; // default render pass

	// swapchain
	vk.initSwapchainLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//vk.initSwapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qtrue );

	// color/depth attachments
	vk_create_attachments();

	// renderpasses — must run before vk_init_iqm_gpu_skinning() because it
	// creates a graphics pipeline that reads vk.render_pass.main; with the
	// previous ordering that field was still VK_NULL_HANDLE, tripping
	// VUID-VkGraphicsPipelineCreateInfo-dynamicRendering-06576 on every
	// Debug startup.
	vk_create_render_passes();

	// Primitive ribbon — must be after shader modules, descriptor pool, and
	// render passes (graphics pipelines read vk.render_pass.main).
	vk_init_ribbon();

	// Primitive sprite — same prerequisites as ribbon.
	vk_init_sprite();

	// Primitive particle — same prerequisites as ribbon/sprite, plus
	// uses compute pipeline. Must be after vk_init_sprite() so the
	// descriptor pool sizing already accounts for sprite's slots.
	vk_init_particle();

	// Primitive beam — must be after vk_init_particle() because
	// beam reuses vk.particle.sampler for the binding-1 sampler
	// array. Beam's binding-1 descriptor write is deferred to
	// vk_init_primitive_shader_images (called from R_Init), so the
	// sampler reuse happens at that later point — ordering here is
	// for documentation rather than strict dependency.
	vk_init_beam();

#if FEAT_IQM
	// IQM GPU skinning — must be after shader modules, descriptor pool, AND
	// render passes (graphics pipeline reads vk.render_pass.main).
	vk_init_iqm_gpu_skinning();
#endif

	// framebuffers for each swapchain image
	vk_create_framebuffers();

	// preallocate staging buffer
	if ( vk.defaults.staging_size == STAGING_BUFFER_SIZE_HI ) {
		vk_alloc_staging_buffer( vk.defaults.staging_size );
	}

	vk_fence_thread_start();
	vk_gpu_ts_init();

	vk.active = qtrue;
}


void vk_create_pipelines( void )
{
	vk_alloc_persistent_pipelines();

	vk.pipelines_world_base = vk.pipelines_count;
}


static void vk_destroy_attachments( void )
{
	uint32_t i;

	if ( vk.bloom_image[0] ) {
		for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ ) {
			qvkDestroyImage( vk.device, vk.bloom_image[i], NULL );
			qvkDestroyImageView( vk.device, vk.bloom_image_view[i], NULL );
			vk.bloom_image[i] = VK_NULL_HANDLE;
			vk.bloom_image_view[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.color_image ) {
		qvkDestroyImage( vk.device, vk.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.color_image_view, NULL );
		vk.color_image = VK_NULL_HANDLE;
		vk.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.msaa_image ) {
		qvkDestroyImage( vk.device, vk.msaa_image, NULL );
		qvkDestroyImageView( vk.device, vk.msaa_image_view, NULL );
		vk.msaa_image = VK_NULL_HANDLE;
		vk.msaa_image_view = VK_NULL_HANDLE;
	}

	qvkDestroyImage( vk.device, vk.depth_image, NULL );
	qvkDestroyImageView( vk.device, vk.depth_image_view, NULL );
	vk.depth_image = VK_NULL_HANDLE;
	vk.depth_image_view = VK_NULL_HANDLE;

	if ( vk.depthFade.image ) {
		qvkDestroyImage( vk.device, vk.depthFade.image, NULL );
		qvkDestroyImageView( vk.device, vk.depthFade.view, NULL );
		qvkDestroySampler( vk.device, vk.depthFade.sampler, NULL );
		qvkFreeMemory( vk.device, vk.depthFade.memory, NULL );
		vk.depthFade.image = VK_NULL_HANDLE;
		vk.depthFade.view = VK_NULL_HANDLE;
		vk.depthFade.sampler = VK_NULL_HANDLE;
		vk.depthFade.memory = VK_NULL_HANDLE;
	}

#if FEAT_SHADOW_MAPPING
	if ( vk.shadowMap.image ) {
		qvkDestroyImage( vk.device, vk.shadowMap.image, NULL );
		qvkDestroyImageView( vk.device, vk.shadowMap.view, NULL );
		qvkDestroySampler( vk.device, vk.shadowMap.sampler, NULL );
		qvkFreeMemory( vk.device, vk.shadowMap.memory, NULL );
		qvkDestroyRenderPass( vk.device, vk.shadowMap.renderPass, NULL );
		qvkDestroyFramebuffer( vk.device, vk.shadowMap.framebuffer, NULL );
		qvkDestroyPipelineLayout( vk.device, vk.shadowMap.depthLayout, NULL );
		if ( vk.shadowMap.depthPipeline )
			qvkDestroyPipeline( vk.device, vk.shadowMap.depthPipeline, NULL );
		memset( &vk.shadowMap, 0, sizeof( vk.shadowMap ) );
	}
#endif

	// SMAA intermediate images (edges/blend/input are in attachment pool, destroyed with image_memory below)
	if ( vk.smaa.edges_image ) {
		qvkDestroyImage( vk.device, vk.smaa.edges_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.edges_view, NULL );
		vk.smaa.edges_image = VK_NULL_HANDLE;
		vk.smaa.edges_view = VK_NULL_HANDLE;
	}
	if ( vk.smaa.blend_image ) {
		qvkDestroyImage( vk.device, vk.smaa.blend_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.blend_view, NULL );
		vk.smaa.blend_image = VK_NULL_HANDLE;
		vk.smaa.blend_view = VK_NULL_HANDLE;
	}
	if ( vk.smaa.input_image ) {
		qvkDestroyImage( vk.device, vk.smaa.input_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.input_view, NULL );
		vk.smaa.input_image = VK_NULL_HANDLE;
		vk.smaa.input_view = VK_NULL_HANDLE;
	}

	// SMAA LUT textures (dedicated memory)
	if ( vk.smaa.area_image ) {
		qvkDestroyImage( vk.device, vk.smaa.area_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.area_view, NULL );
		qvkFreeMemory( vk.device, vk.smaa.area_memory, NULL );
		vk.smaa.area_image = VK_NULL_HANDLE;
		vk.smaa.area_view = VK_NULL_HANDLE;
		vk.smaa.area_memory = VK_NULL_HANDLE;
	}
	if ( vk.smaa.search_image ) {
		qvkDestroyImage( vk.device, vk.smaa.search_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.search_view, NULL );
		qvkFreeMemory( vk.device, vk.smaa.search_memory, NULL );
		vk.smaa.search_image = VK_NULL_HANDLE;
		vk.smaa.search_view = VK_NULL_HANDLE;
		vk.smaa.search_memory = VK_NULL_HANDLE;
	}

	// SMAA samplers
	if ( vk.smaa.point_sampler ) {
		qvkDestroySampler( vk.device, vk.smaa.point_sampler, NULL );
		vk.smaa.point_sampler = VK_NULL_HANDLE;
	}
	if ( vk.smaa.linear_sampler ) {
		qvkDestroySampler( vk.device, vk.smaa.linear_sampler, NULL );
		vk.smaa.linear_sampler = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.color_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view, NULL );
		vk.screenMap.color_image = VK_NULL_HANDLE;
		vk.screenMap.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.color_image_msaa ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image_msaa, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view_msaa, NULL );
		vk.screenMap.color_image_msaa = VK_NULL_HANDLE;
		vk.screenMap.color_image_view_msaa = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.depth_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.depth_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.depth_image_view, NULL );
		vk.screenMap.depth_image = VK_NULL_HANDLE;
		vk.screenMap.depth_image_view = VK_NULL_HANDLE;
	}

	if ( vk.capture.image ) {
		qvkDestroyImage( vk.device, vk.capture.image, NULL );
		qvkDestroyImageView( vk.device, vk.capture.image_view, NULL );
		vk.capture.image = VK_NULL_HANDLE;
		vk.capture.image_view = VK_NULL_HANDLE;
	}

	for ( i = 0; i < vk.image_memory_count; i++ ) {
		qvkFreeMemory( vk.device, vk.image_memory[i], NULL );
	}

	vk.image_memory_count = 0;
}


static void vk_destroy_render_passes( void )
{
	uint32_t i;

	if ( vk.render_pass.main != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main, NULL );
		vk.render_pass.main = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.bloom_extract, NULL );
		vk.render_pass.bloom_extract = VK_NULL_HANDLE;
	}

	for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
		if ( vk.render_pass.blur[i] != VK_NULL_HANDLE ) {
			qvkDestroyRenderPass( vk.device, vk.render_pass.blur[i], NULL );
			vk.render_pass.blur[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.render_pass.post_bloom != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.post_bloom, NULL );
		vk.render_pass.post_bloom = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.screenmap, NULL );
		vk.render_pass.screenmap = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.gamma != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.gamma, NULL );
		vk.render_pass.gamma = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.capture != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.capture, NULL );
		vk.render_pass.capture = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.depth_fade != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.depth_fade, NULL );
		vk.render_pass.depth_fade = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.smaa_edge != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.smaa_edge, NULL );
		vk.render_pass.smaa_edge = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.smaa_blend != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.smaa_blend, NULL );
		vk.render_pass.smaa_blend = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.smaa_resolve != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.smaa_resolve, NULL );
		vk.render_pass.smaa_resolve = VK_NULL_HANDLE;
	}
}


static void vk_destroy_pipelines( qboolean resetCounter )
{
	uint32_t i, j;

	for ( i = 0; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
	}

	if ( resetCounter ) {
		memset( &vk.pipelines, 0, sizeof( vk.pipelines ) );
		vk.pipelines_count = 0;
	}

	if ( vk.gamma_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.gamma_pipeline, NULL );
		vk.gamma_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.capture_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.capture_pipeline, NULL );
		vk.capture_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.bloom_extract_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_extract_pipeline, NULL );
		vk.bloom_extract_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.bloom_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_blend_pipeline, NULL );
		vk.bloom_blend_pipeline = VK_NULL_HANDLE;
	}

	for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i++ ) {
		if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
			vk.blur_pipeline[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.smaa_edge_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_edge_pipeline, NULL );
		vk.smaa_edge_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.smaa_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_blend_pipeline, NULL );
		vk.smaa_blend_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.smaa_resolve_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_resolve_pipeline, NULL );
		vk.smaa_resolve_pipeline = VK_NULL_HANDLE;
	}
}


void vk_shutdown( refShutdownCode_t code )
{
	int i, j, k, l;

	if ( qvkQueuePresentKHR == NULL ) { // not fully initialized
		goto __cleanup;
	}

	// Drain GPU work so all pending fences are signaled, then stop the fence
	// thread before any Vulkan resources (fences, device) are destroyed.
	qvkQueueWaitIdle( vk.queue );
	vk_fence_thread_stop();
	vk_gpu_ts_shutdown();

	vk_destroy_framebuffers();

	vk_destroy_pipelines( qtrue ); // reset counter

	vk_destroy_render_passes();

	vk_destroy_attachments();

	vk_destroy_swapchain();

	if ( vk.pipelineCache != VK_NULL_HANDLE ) {
		qvkDestroyPipelineCache( vk.device, vk.pipelineCache, NULL );
		vk.pipelineCache = VK_NULL_HANDLE;
	}

	qvkDestroyCommandPool( vk.device, vk.command_pool, NULL );

	qvkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);

	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_sampler, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_uniform, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_storage, NULL);

	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout, NULL);
	if ( vk.pipeline_layout_msdf != VK_NULL_HANDLE )
		qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_msdf, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_storage, NULL);
	vk_shutdown_ribbon();
	vk_shutdown_sprite();
	vk_shutdown_beam();
	vk_shutdown_particle();
	vk_shutdown_primitive_stages();
#if FEAT_IQM
	vk_shutdown_iqm_gpu_skinning();
#endif
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_post_process, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_blend, NULL);
	if ( vk.pipeline_layout_smaa != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_smaa, NULL );
	}
#ifdef USE_VBO
	vk_release_vbo();
#endif

	vk_clean_staging_buffer();

	vk_release_geometry_buffers();

	vk_destroy_samplers();

	vk_destroy_sync_primitives();

	qvkDestroyBuffer( vk.device, vk.storage.buffer, NULL );
	qvkFreeMemory( vk.device, vk.storage.memory, NULL );

	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					if ( vk.modules.vert.gen[i][j][k][l] != VK_NULL_HANDLE ) {
						qvkDestroyShaderModule( vk.device, vk.modules.vert.gen[i][j][k][l], NULL );
						vk.modules.vert.gen[i][j][k][l] = VK_NULL_HANDLE;
					}
				}
			}
		}
	}
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				if ( vk.modules.frag.gen[i][j][k] != VK_NULL_HANDLE ) {
					qvkDestroyShaderModule( vk.device, vk.modules.frag.gen[i][j][k], NULL );
					vk.modules.frag.gen[i][j][k] = VK_NULL_HANDLE;
				}
			}
		}
	}
	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.vert.light[i] != VK_NULL_HANDLE ) {
			qvkDestroyShaderModule( vk.device, vk.modules.vert.light[i], NULL );
			vk.modules.vert.light[i] = VK_NULL_HANDLE;
		}
		for ( j = 0; j < 2; j++ ) {
			if ( vk.modules.frag.light[i][j] != VK_NULL_HANDLE ) {
				qvkDestroyShaderModule( vk.device, vk.modules.frag.light[i][j], NULL );
				vk.modules.frag.light[i][j] = VK_NULL_HANDLE;
			}
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.ident1[i][j][k], NULL );
				vk.modules.vert.ident1[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ident1[i][j], NULL );
			vk.modules.frag.ident1[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.fixed[i][j][k], NULL );
				vk.modules.vert.fixed[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.fixed[i][j], NULL );
			vk.modules.frag.fixed[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 1; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ent[i][j], NULL );
			vk.modules.frag.ent[i][j] = VK_NULL_HANDLE;
		}
	}

	qvkDestroyShaderModule( vk.device, vk.modules.frag.gen0_df, NULL );

	// depth fade shader modules
	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.frag.dfade_gen[0][i] )
			qvkDestroyShaderModule( vk.device, vk.modules.frag.dfade_gen[0][i], NULL );
		if ( vk.modules.frag.dfade_ident1[0][i] )
			qvkDestroyShaderModule( vk.device, vk.modules.frag.dfade_ident1[0][i], NULL );
		if ( vk.modules.frag.dfade_fixed[0][i] )
			qvkDestroyShaderModule( vk.device, vk.modules.frag.dfade_fixed[0][i], NULL );
		if ( vk.modules.frag.dfade_ent[0][i] )
			qvkDestroyShaderModule( vk.device, vk.modules.frag.dfade_ent[0][i], NULL );
	}

	if ( vk.modules.msdf_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.msdf_fs, NULL );
	if ( vk.modules.msdf_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.msdf_vs, NULL );

	if ( vk.modules.ribbon_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.ribbon_fs, NULL );
	if ( vk.modules.ribbon_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.ribbon_vs, NULL );

	if ( vk.modules.sprite_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.sprite_fs, NULL );
	if ( vk.modules.sprite_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.sprite_vs, NULL );

	if ( vk.modules.beam_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.beam_fs, NULL );
	if ( vk.modules.beam_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.beam_vs, NULL );

	if ( vk.modules.particle_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.particle_fs, NULL );
	if ( vk.modules.particle_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.particle_vs, NULL );
	if ( vk.modules.particle_integrate_cs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.particle_integrate_cs, NULL );

	if ( vk.modules.q1_ls_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.q1_ls_fs, NULL );
	if ( vk.modules.q1_ls_array_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.q1_ls_array_fs, NULL );
	if ( vk.modules.q1_ls_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.q1_ls_vs, NULL );

	qvkDestroyShaderModule( vk.device, vk.modules.color_fs, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.color_vs, NULL );

	qvkDestroyShaderModule(vk.device, vk.modules.fog_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fog_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.dot_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.dot_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.bloom_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blur_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blend_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.gamma_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.gamma_fs, NULL);

	// SMAA shader modules
	if ( vk.modules.smaa_edge_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_edge_vs, NULL );
	if ( vk.modules.smaa_edge_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_edge_fs, NULL );
	if ( vk.modules.smaa_blend_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_blend_vs, NULL );
	if ( vk.modules.smaa_blend_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_blend_fs, NULL );
	if ( vk.modules.smaa_resolve_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_resolve_vs, NULL );
	if ( vk.modules.smaa_resolve_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_resolve_fs, NULL );

#define DESTROY_SM(m) \
	do { if ( (m) != VK_NULL_HANDLE ) { \
		qvkDestroyShaderModule( vk.device, (m), NULL ); \
		(m) = VK_NULL_HANDLE; } } while ( 0 )

#if FEAT_PARALLAX_MAPPING
	for ( i = 0; i < 2; i++ ) {
		DESTROY_SM( vk.modules.vert.light_parallax[i] );
		for ( j = 0; j < 2; j++ )
			DESTROY_SM( vk.modules.frag.light_parallax[i][j] );
	}
#endif

#if FEAT_ADVANCED_WATER
	DESTROY_SM( vk.modules.water_fs );
#endif

#if FEAT_SHADOW_MAPPING
	DESTROY_SM( vk.modules.shadow_depth_vs );
	DESTROY_SM( vk.modules.shadow_depth_fs );
	for ( i = 0; i < 2; i++ ) {
		DESTROY_SM( vk.modules.light_shadow[i] );
		for ( j = 0; j < 2; j++ )
			DESTROY_SM( vk.modules.light_shadow_frag[i][j] );
	}
#endif

#if FEAT_PBR
	for ( i = 0; i < 2; i++ )
		for ( j = 0; j < 2; j++ )
			DESTROY_SM( vk.modules.light_pbr_frag[i][j] );
#endif

#if FEAT_IQM
	/* vk_shutdown_iqm_gpu_skinning above only destroys the pipeline/buffers/layout. */
	DESTROY_SM( vk.modules.iqm_skinning_vs );
	DESTROY_SM( vk.modules.iqm_skinning_fs );
#endif

	/* gamma_variant_fs is a sparse array indexed by feature-flag bitmask. */
	for ( i = 0; i < GAMMA_VAR_COUNT; i++ )
		DESTROY_SM( vk.gamma_variant_fs[i] );

#undef DESTROY_SM

__cleanup:
	if ( vk.device != VK_NULL_HANDLE ) {
		qvkDestroyDevice( vk.device, NULL );
	}

	deinit_device_functions();

	memset( &vk, 0, sizeof( vk ) );
	memset( &vk_world, 0, sizeof( vk_world ) );

	if ( code != REF_KEEP_CONTEXT ) {
		vk_destroy_instance();
		deinit_instance_functions();
	}
}


void vk_wait_idle( void )
{
	VK_CHECK( qvkDeviceWaitIdle( vk.device ) );
}


void vk_queue_wait_idle( void )
{
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) );
}


void vk_release_resources( void ) {
	int i, j;

	vk_wait_idle();

#if FEAT_IQM
	// destroy per-model IQM GPU skinning VBOs before hunk is reset
	if ( vk.iqmGpu.available ) {
		for ( i = 0; i < tr.numModels; i++ ) {
			model_t *mod = tr.models[i];
			if ( mod && mod->type == MOD_IQM && mod->modelData ) {
				iqmData_t *data = (iqmData_t *)mod->modelData;
				if ( data->vk_gpu_skinning ) {
					vk_destroy_iqm_vbo( &data->vk_vertex_buffer, &data->vk_vertex_memory,
						&data->vk_index_buffer, &data->vk_index_memory );
					data->vk_gpu_skinning = qfalse;
				}
			}
		}
	}
#endif

	for (i = 0; i < vk_world.num_image_chunks; i++)
		qvkFreeMemory(vk.device, vk_world.image_chunks[i].memory, NULL);

	vk_clean_staging_buffer();

	// vk_destroy_samplers();

	for ( i = vk.pipelines_world_base; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
		memset( &vk.pipelines[i], 0, sizeof( vk.pipelines[0] ) );
	}
	vk.pipelines_count = vk.pipelines_world_base;

	VK_CHECK( qvkResetDescriptorPool( vk.device, vk.descriptor_pool, 0 ) );

	if ( vk_world.num_image_chunks > 1 ) {
		// if we allocated more than 2 image chunks - use doubled default size
		vk.image_chunk_size = (IMAGE_CHUNK_SIZE * 2);
	}
#if 0 // do not reduce chunk size
	else if ( vk_world.num_image_chunks == 1 ) {
		// otherwise set to default if used less than a half
		if ( vk_world.image_chunks[0].used < ( IMAGE_CHUNK_SIZE - (IMAGE_CHUNK_SIZE / 10) ) ) {
			vk.image_chunk_size = IMAGE_CHUNK_SIZE;
		}
	}
#endif

	memset( &vk_world, 0, sizeof( vk_world ) );

	// Reset geometry buffers offsets
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
	}

	memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );

	memset( &vk.stats, 0, sizeof( vk.stats ) );
}

#if 0
static void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset,
		VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
		VkAccessFlags src_access, VkAccessFlags dst_access) {

	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer;
	barrier.offset = offset;
	barrier.size = size;

	qvkCmdPipelineBarrier( cb, src_stages, dst_stages, 0, 0, NULL, 1, &barrier, 0, NULL );
}
#endif

void vk_create_image( image_t *image, int width, int height, int mip_levels ) {

	VkFormat format = image->internalFormat;

	if ( image->handle ) {
		qvkDestroyImage( vk.device, image->handle, NULL );
		image->handle = VK_NULL_HANDLE;
	}

	if ( image->view ) {
		qvkDestroyImageView( vk.device, image->view, NULL );
		image->view = VK_NULL_HANDLE;
	}

	// create image
	{
		VkImageCreateInfo desc;
		uint32_t layers = ( image->layerCount > 1 ) ? image->layerCount : 1;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = layers;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );

		allocate_and_bind_image_memory( image->handle );
	}

	// create image view
	{
		VkImageViewCreateInfo desc;
		uint32_t layers = ( image->layerCount > 1 ) ? image->layerCount : 1;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image->handle;
		desc.viewType = ( layers > 1 ) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		desc.format = format;
		desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = layers;

		VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &image->view ) );
	}

	// create associated descriptor set
	if ( image->descriptor == VK_NULL_HANDLE ) {
		VkDescriptorSetAllocateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &desc, &image->descriptor ) );
	}

	vk_update_descriptor_set( image, mip_levels > 1 ? qtrue : qfalse );

	SET_OBJECT_NAME( image->handle, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( image->view, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( image->descriptor, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );

#if FEAT_FBO_DEBUG
	if ( image->descriptor == VK_NULL_HANDLE || image->view == VK_NULL_HANDLE || image->handle == VK_NULL_HANDLE ) {
		ri.Log( SEV_INFO, "^1[FBO_DEBUG] INVALID image: '%s' handle=%p view=%p descriptor=%p\n",
			image->imgName, (void*)image->handle, (void*)image->view, (void*)image->descriptor );
	}
#endif
}


static byte *resample_image_data( const int target_format, byte *data, const int data_size, int *bytes_per_pixel )
{
	byte* buffer;
	uint16_t* p;
	int i, n;

	switch ( target_format ) {
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			byte a = data[i + 3];
			*p = (uint32_t)((a / 255.0) * 15.0 + 0.5) |
				((uint32_t)((r / 255.0) * 15.0 + 0.5) << 4) |
				((uint32_t)((g / 255.0) * 15.0 + 0.5) << 8) |
				((uint32_t)((b / 255.0) * 15.0 + 0.5) << 12);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			*p = (uint32_t)((b / 255.0) * 31.0 + 0.5) |
				((uint32_t)((g / 255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r / 255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_B8G8R8A8_UNORM:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size );
		for ( i = 0; i < data_size; i += 4 ) {
			buffer[i + 0] = data[i + 2];
			buffer[i + 1] = data[i + 1];
			buffer[i + 2] = data[i + 0];
			buffer[i + 3] = data[i + 3];
		}
		*bytes_per_pixel = 4;
		return buffer;

	case VK_FORMAT_R8G8B8_UNORM: {
		buffer = (byte*)ri.Hunk_AllocateTempMemory( (data_size * 3) / 4 );
		for ( i = 0, n = 0; i < data_size; i += 4, n += 3 ) {
			buffer[n + 0] = data[i + 0];
			buffer[n + 1] = data[i + 1];
			buffer[n + 2] = data[i + 2];
		}
		*bytes_per_pixel = 3;
		return buffer;
	}

	default:
		*bytes_per_pixel = 4;
		return data;
	}
}


void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, qboolean update, uint32_t baseArrayLayer ) {

	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	byte *buf;
	int n;

	int num_regions = 0;
	int buffer_size = 0;

	buf = resample_image_data( image->internalFormat, pixels, size, &n /*bpp*/ );

	while (qtrue) {
		memset(&region, 0, sizeof(region));
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = baseArrayLayer;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * n;

		if ( num_regions >= mipmaps || (width == 1 && height == 1) || num_regions >= ARRAY_LEN( regions ) )
			break;

		x >>= 1;
		y >>= 1;

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		// wait for vkQueueSubmit() completion before new upload
	}

	if ( vk.staging_buffer.size - vk.staging_buffer.offset < buffer_size ) {
		// try to flush staging buffer and reset offset
		vk_flush_staging_buffer( qfalse );
	}

	if ( vk.staging_buffer.size /* - vk_world.staging_buffer_offset */ < buffer_size ) {
		// if still not enough - reallocate staging buffer
		vk_alloc_staging_buffer( buffer_size );
	}

	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk.staging_buffer.offset;
	}

	memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, buf, buffer_size );

	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}

	//ri.Log( SEV_WARN, "batch @%6i + %i %s \n", (int)vk_world.staging_buffer_offset, (int)buffer_size, image->imgName );
	vk.staging_buffer.offset += buffer_size;

	command_buffer = vk.staging_command_buffer;

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}

	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );

	// final transition after upload comleted
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
#else
	if ( vk.staging_buffer.size < buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	memcpy( vk.staging_buffer.ptr, buf, buffer_size );

	command_buffer = begin_command_buffer();
	// record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	end_command_buffer( command_buffer, __func__ );
#endif

	if ( buf != pixels ) {
		ri.Hunk_FreeTempMemory( buf );
	}
}


void vk_update_descriptor_set( image_t *image, qboolean mipmap ) {
	Vk_Sampler_Def sampler_def;
	VkDescriptorImageInfo image_info;
	VkWriteDescriptorSet descriptor_write;

	memset( &sampler_def, 0, sizeof( sampler_def ) );

	sampler_def.address_mode = image->wrapClampMode;

	if ( mipmap ) {
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	} else {
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = qtrue;
	}

	image_info.sampler = vk_find_sampler( &sampler_def );
	image_info.imageView = image->view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = image->descriptor;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pNext = NULL;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_write.pImageInfo = &image_info;
	descriptor_write.pBufferInfo = NULL;
	descriptor_write.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &descriptor_write, 0, NULL );
}


void vk_destroy_image_resources( VkImage *image, VkImageView *imageView )
{
	if ( image != NULL ) {
		if ( *image != VK_NULL_HANDLE ) {
			// MoltenVK guard: swapchain images are owned by the swapchain and must
			// not be passed to vkDestroyImage — doing so crashes via
			// destroyPresentableSwapchainImage when handle aliasing occurs.
			qboolean is_swapchain = qfalse;
			uint32_t sci;
			for ( sci = 0; sci < vk.swapchain_image_count; sci++ ) {
				if ( *image == vk.swapchain_images[ sci ] ) {
					is_swapchain = qtrue;
					break;
				}
			}
			if ( !is_swapchain ) {
				qvkDestroyImage( vk.device, *image, NULL );
			}
			*image = VK_NULL_HANDLE;
		}
	}
	if ( imageView != NULL ) {
		if ( *imageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, *imageView, NULL );
			*imageView = VK_NULL_HANDLE;
		}
	}
}


static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry) {
	desc->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc->pNext = NULL;
	desc->flags = 0;
	desc->stage = stage;
	desc->module = shader_module;
	desc->pName = entry;
	desc->pSpecializationInfo = NULL;
}


#define FORMAT_DEPTH(format, r_bits, g_bits, b_bits) case(VK_FORMAT_##format): *r = r_bits; *b = b_bits; *g = g_bits; return qtrue;
static qboolean vk_surface_format_color_depth( VkFormat format, int *r, int *g, int *b ) {
	switch (format) {
		// Common formats from https://vulkan.gpuinfo.org/listsurfaceformats.php
		FORMAT_DEPTH(B8G8R8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(B8G8R8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2B10G10R10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R8G8B8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(R8G8B8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2R10G10B10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R5G6B5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(R8G8B8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_UNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SRGB_PACK32, 255, 255, 255)
			FORMAT_DEPTH(R16G16B16A16_UNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(R16G16B16A16_SNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(B5G6R5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(B8G8R8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(R4G4B4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(B4G4R4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(A1R5G5B5_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(R5G5B5A1_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(B5G5R5A1_UNORM_PACK16, 31, 31, 31)
	default:
		*r = 255; *g = 255; *b = 255; return qfalse;
	}
}


void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	VkSpecializationMapEntry spec_entries[13];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;
	VkShaderModule fsmodule;
	VkRenderPass renderpass;
	VkPipelineLayout layout;
	VkSampleCountFlagBits samples;
	const char *pipeline_name;
	qboolean blend;

	struct FragSpecData {
		float gamma;
		float overbright;
		float saturation;
		float bloom_threshold;
		float bloom_intensity;
		int bloom_threshold_mode;
		int bloom_modulate;
		int dither;
		int depth_r;
		int depth_g;
		int depth_b;
		// Tonemap fields wire gamma.frag spec constants 17/18.
		// They are present in the struct unconditionally even when
		// FEAT_TONEMAP isn't compiled — the spec_entries below always
		// include them, and the driver ignores spec entries whose
		// constant IDs aren't declared in the bound shader. Keeping
		// the struct shape stable simplifies offsetof maintenance.
		int tonemap_mode;
		float tonemap_exposure;
	} frag_spec_data;

	switch ( program_index ) {
		case 1: // bloom extraction
			pipeline = &vk.bloom_extract_pipeline;
			fsmodule = vk.modules.bloom_fs;
			renderpass = vk.render_pass.bloom_extract;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "bloom extraction pipeline";
			blend = qfalse;
			break;
		case 2: // final bloom blend
			pipeline = &vk.bloom_blend_pipeline;
			fsmodule = vk.modules.blend_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk.pipeline_layout_blend;
			samples = vkSamples;
			pipeline_name = "bloom blend pipeline";
			blend = qtrue;
			break;
		case 3: // capture buffer extraction
			pipeline = &vk.capture_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.capture;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "capture buffer pipeline";
			blend = qfalse;
			break;
		default: // gamma correction
			pipeline = &vk.gamma_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.gamma;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "gamma-correction pipeline";
			blend = qfalse;
			break;
		case 5: { // gamma variant (SSAO/tonemap/colorgrade/FXAA/godrays combo)
			int varIdx = 0;
#if FEAT_SSAO
			if ( r_ssao->integer )     varIdx |= GAMMA_VAR_SSAO;
#endif
#if FEAT_TONEMAP
			if ( r_tonemap->integer )  varIdx |= GAMMA_VAR_TONEMAP;
#endif
#if FEAT_COLOR_GRADING
			if ( r_colorGrading->integer ) varIdx |= GAMMA_VAR_CG;
#endif
#if FEAT_FXAA
			if ( r_fxaa->integer )     varIdx |= GAMMA_VAR_FXAA;
#endif
#if FEAT_GODRAYS
			if ( r_godRays->integer )  varIdx |= GAMMA_VAR_GODRAYS;
#endif
			pipeline = &vk.gamma_variants[ varIdx ];
			fsmodule = vk.gamma_variant_fs[ varIdx ];
			renderpass = vk.render_pass.gamma;
			// Select pipeline layout: godrays needs push constants, SSAO/godrays need depth sampler
			if ( varIdx & GAMMA_VAR_GODRAYS )
				layout = vk.pipeline_layout_godrays;
			else if ( varIdx & GAMMA_VAR_SSAO )
				layout = vk.pipeline_layout_ssao;
			else
				layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "gamma-variant pipeline";
			blend = qfalse;
			break;
		}
	}

	if ( *pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;

	// shaders
	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, fsmodule, "main" );

	frag_spec_data.gamma = 1.0 / (r_gamma->value);
	frag_spec_data.overbright = (float)(1 << tr.overbrightBits);
	frag_spec_data.saturation = r_saturation->value;
	frag_spec_data.bloom_threshold = r_bloom_threshold->value;
	frag_spec_data.bloom_intensity = r_bloom_intensity->value;
	frag_spec_data.bloom_threshold_mode = r_bloom_threshold_mode->integer;
	frag_spec_data.bloom_modulate = r_bloom_modulate->integer;
	frag_spec_data.dither = r_dither->integer;
#if FEAT_TONEMAP
	frag_spec_data.tonemap_mode = r_tonemap->integer;
	frag_spec_data.tonemap_exposure = r_tonemapExposure->value;
#else
	frag_spec_data.tonemap_mode = 1;       // Reinhard if compiled-in path runs without the cvar
	frag_spec_data.tonemap_exposure = 1.0f;
#endif

	if ( !vk_surface_format_color_depth( vk.present_format.format, &frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b ) )
		ri.Log( SEV_INFO, "Format %s not recognized, dither to assume 8bpc\n", vk_format_string( vk.base_format.format ) );

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( struct FragSpecData, gamma );
	spec_entries[0].size = sizeof( frag_spec_data.gamma );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( struct FragSpecData, overbright );
	spec_entries[1].size = sizeof( frag_spec_data.overbright );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( struct FragSpecData, saturation );
	spec_entries[2].size = sizeof( frag_spec_data.saturation );

	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( struct FragSpecData, bloom_threshold );
	spec_entries[3].size = sizeof( frag_spec_data.bloom_threshold );

	spec_entries[4].constantID = 4;
	spec_entries[4].offset = offsetof( struct FragSpecData, bloom_intensity );
	spec_entries[4].size = sizeof( frag_spec_data.bloom_intensity );

	spec_entries[5].constantID = 5;
	spec_entries[5].offset = offsetof( struct FragSpecData, bloom_threshold_mode );
	spec_entries[5].size = sizeof( frag_spec_data.bloom_threshold_mode );

	spec_entries[6].constantID = 6;
	spec_entries[6].offset = offsetof( struct FragSpecData, bloom_modulate );
	spec_entries[6].size = sizeof( frag_spec_data.bloom_modulate );

	spec_entries[7].constantID = 7;
	spec_entries[7].offset = offsetof( struct FragSpecData, dither );
	spec_entries[7].size = sizeof( frag_spec_data.dither );

	spec_entries[8].constantID = 8;
	spec_entries[8].offset = offsetof( struct FragSpecData, depth_r );
	spec_entries[8].size = sizeof( frag_spec_data.depth_r );

	spec_entries[9].constantID = 9;
	spec_entries[9].offset = offsetof(struct FragSpecData, depth_g);
	spec_entries[9].size = sizeof(frag_spec_data.depth_g);

	spec_entries[10].constantID = 10;
	spec_entries[10].offset = offsetof(struct FragSpecData, depth_b);
	spec_entries[10].size = sizeof(frag_spec_data.depth_b);

	// Tonemap spec constants in gamma.frag — IDs 17 and 18 per the
	// shader declaration. Pipelines without USE_TONEMAP defined
	// (the non-tonemap gamma variants) never declare these IDs;
	// the driver silently ignores spec entries whose IDs aren't
	// referenced by the bound shader, so it's safe to write the
	// entries unconditionally.
	spec_entries[11].constantID = 17;
	spec_entries[11].offset = offsetof(struct FragSpecData, tonemap_mode);
	spec_entries[11].size = sizeof(frag_spec_data.tonemap_mode);

	spec_entries[12].constantID = 18;
	spec_entries[12].offset = offsetof(struct FragSpecData, tonemap_exposure);
	spec_entries[12].size = sizeof(frag_spec_data.tonemap_exposure);

	frag_spec_info.mapEntryCount = 13;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	if ( program_index == 0 || program_index == 5 ) {
		// Gamma pipeline (case 0) and gamma feature variants
		// (case 5 — SSAO/tonemap/colorgrade/FXAA/godrays combos)
		// both target the gamma render pass at full window
		// dimensions. Both call sites pass (0, 0) for width/height
		// as a sentinel meaning "use the window extent". Without
		// this branch covering case 5, the variant pipeline would
		// fall into the else and produce a 0×0 viewport, which
		// vkCreateGraphicsPipelines rejects with the "pViewports[0]
		// .width (0.000000) is not greater than zero" validation
		// error. The blitX0/blitY0 offsets are the windowed-mode
		// blit insets; case 5 inherits the same insets because it
		// renders into the same gamma render pass.
		viewport.x = 0.0 + vk.blitX0;
		viewport.y = 0.0 + vk.blitY0;
		viewport.width = gls.windowWidth - vk.blitX0 * 2;
		viewport.height = gls.windowHeight - vk.blitY0 * 2;
	} else {
		// other post-processing
		viewport.x = 0.0;
		viewport.y = 0.0;
		viewport.width = width;
		viewport.height = height;
	}

	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	scissor.offset.x = viewport.x;
	scissor.offset.y = viewport.y;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
#if FEAT_DEPTH_CLAMP
	rasterization_state.depthClampEnable = r_depthClamp && r_depthClamp->integer ? VK_TRUE : VK_FALSE;
#else
	rasterization_state.depthClampEnable = VK_FALSE;
#endif
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_NONE;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = samples;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( blend ) {
		attachment_blend_state.blendEnable = VK_TRUE;
		attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	} else {
		attachment_blend_state.blendEnable = VK_FALSE;
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = VK_FALSE;
	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = (program_index == 2) ? &depth_stencil_state : NULL;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = layout;
	create_info.renderPass = renderpass;
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, pipeline_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}


void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	float frag_spec_data[3]; // x-offset, y-offset, correction
	VkSpecializationMapEntry spec_entries[3];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;

	pipeline = &vk.blur_pipeline[ index ];

	if ( *pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;

	// shaders
	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blur_fs, "main" );

	frag_spec_data[0] = 1.2 / (float) width; // x offset
	frag_spec_data[1] = 1.2 / (float) height; // y offset
	frag_spec_data[2] = 1.0; // intensity?

	if ( horizontal_pass ) {
		frag_spec_data[1] = 0.0;
	} else {
		frag_spec_data[0] = 0.0;
	}

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = 0 * sizeof( float );
	spec_entries[0].size = sizeof( float );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = 1 * sizeof( float );
	spec_entries[1].size = sizeof( float );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = 2 * sizeof( float );
	spec_entries[2].size = sizeof( float );

	frag_spec_info.mapEntryCount = 3;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = 3 * sizeof( float );
	frag_spec_info.pData = &frag_spec_data[0];

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	viewport.x = 0.0;
	viewport.y = 0.0;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	scissor.offset.x = viewport.x;
	scissor.offset.y = viewport.y;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
#if FEAT_DEPTH_CLAMP
	rasterization_state.depthClampEnable = r_depthClamp && r_depthClamp->integer ? VK_TRUE : VK_FALSE;
#else
	rasterization_state.depthClampEnable = VK_FALSE;
#endif
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_NONE;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = VK_FALSE;
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = NULL;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = vk.pipeline_layout_post_process; // one input attachment
	create_info.renderPass = vk.render_pass.blur[ index ];
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, va( "%s blur pipeline %i", horizontal_pass ? "horizontal" : "vertical", index/2 + 1 ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}


static VkVertexInputBindingDescription bindings[8];
static VkVertexInputAttributeDescription attribs[8];
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind( uint32_t binding, uint32_t stride )
{
	bindings[ num_binds ].binding = binding;
	bindings[ num_binds ].stride = stride;
	bindings[ num_binds ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	num_binds++;
}

static void push_attr( uint32_t location, uint32_t binding, VkFormat format )
{
	attribs[ num_attrs ].location = location;
	attribs[ num_attrs ].binding = binding;
	attribs[ num_attrs ].format = format;
	attribs[ num_attrs ].offset = 0;
	num_attrs++;
}


VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index ) {
	VkShaderModule *vs_module = NULL;
	VkShaderModule *fs_module = NULL;
	//int32_t vert_spec_data[1]; // clippping
	floatint_t frag_spec_data[12]; // 0:alpha-test-func, 1:alpha-test-value, 2:depth-fragment, 3:alpha-to-coverage, 4:color_mode, 5:abs_light, 6:multitexture mode, 7:discard mode, 8: ident.color, 9 - ident.alpha, 10 - acff, 11 - depth_fade_scale
	VkSpecializationMapEntry spec_entries[13];
	//VkSpecializationInfo vert_spec_info;
	VkSpecializationInfo frag_spec_info;
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkGraphicsPipelineCreateInfo create_info;
	VkPipeline pipeline;
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkBool32 alphaToCoverage = VK_FALSE;
	unsigned int atest_bits;
	unsigned int state_bits = def->state_bits;

	switch ( def->shader_type ) {

		case TYPE_SINGLE_TEXTURE_LIGHTING:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[1][0];
			break;

#if FEAT_PARALLAX_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX:
			vs_module = &vk.modules.vert.light_parallax[0];
			fs_module = &vk.modules.frag.light_parallax[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX_LINEAR:
			vs_module = &vk.modules.vert.light_parallax[0];
			fs_module = &vk.modules.frag.light_parallax[1][0];
			break;
#endif

#if FEAT_ADVANCED_WATER
		case TYPE_WATER:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.water_fs;
			break;
#endif

#if FEAT_SHADOW_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW:
			vs_module = &vk.modules.light_shadow[0];
			fs_module = &vk.modules.light_shadow_frag[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW_LINEAR:
			vs_module = &vk.modules.light_shadow[0];
			fs_module = &vk.modules.light_shadow_frag[1][0];
			break;

		case TYPE_SHADOW_DEPTH:
			vs_module = &vk.modules.shadow_depth_vs;
			fs_module = &vk.modules.shadow_depth_fs;
			state_bits |= GLS_DEPTHMASK_TRUE;
			break;
#endif

#if FEAT_PBR
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.light_pbr_frag[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.light_pbr_frag[1][0];
			break;
#endif

		case TYPE_SINGLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.gen0_df;
			break;

		case TYPE_SINGLE_TEXTURE_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_ENT_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE:
			vs_module = &vk.modules.vert.gen[0][0][0][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
			break;

		case TYPE_SINGLE_TEXTURE_ENV:
			vs_module = &vk.modules.vert.gen[0][0][1][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
			break;

		case TYPE_SINGLE_TEXTURE_IDENTITY:
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[0][1][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = &vk.modules.vert.ident1[1][0][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[1][1][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[1][0][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[1][1][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = &vk.modules.vert.gen[1][0][0][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = &vk.modules.vert.gen[1][0][1][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = &vk.modules.vert.gen[2][0][0][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = &vk.modules.vert.gen[2][0][1][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[1][1][0][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[1][1][1][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[2][1][0][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[2][1][1][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			vs_module = &vk.modules.color_vs;
			fs_module = &vk.modules.color_fs;
			break;

		case TYPE_FOG_ONLY:
			vs_module = &vk.modules.fog_vs;
			fs_module = &vk.modules.fog_fs;
			break;

		case TYPE_DOT:
			vs_module = &vk.modules.dot_vs;
			fs_module = &vk.modules.dot_fs;
			break;

		case TYPE_MSDF:
			vs_module = &vk.modules.msdf_vs;
			fs_module = &vk.modules.msdf_fs;
			break;

		case TYPE_LIGHTSTYLES:
			vs_module = &vk.modules.q1_ls_vs;
			fs_module = &vk.modules.q1_ls_fs;
			break;

		case TYPE_LIGHTSTYLES_ARRAY:
			vs_module = &vk.modules.q1_ls_vs;       // vertex stage is identical to TYPE_LIGHTSTYLES
			fs_module = &vk.modules.q1_ls_array_fs;
			break;

		default:
			ri.Terminate( TERM_CLIENT_DROP, "create_pipeline: unknown shader type %i\n", def->shader_type);
			return 0;
	}

	if ( def->fog_stage ) {
		switch ( def->shader_type ) {
			case TYPE_FOG_ONLY:
			case TYPE_DOT:
			case TYPE_MSDF:
			case TYPE_LIGHTSTYLES:
			case TYPE_LIGHTSTYLES_ARRAY:
			case TYPE_SINGLE_TEXTURE_DF:
			case TYPE_COLOR_BLACK:
			case TYPE_COLOR_WHITE:
			case TYPE_COLOR_GREEN:
			case TYPE_COLOR_RED:
				break;
			default:
				// switch to fogged modules
				vs_module++;
				fs_module++;
				break;
		}
	}

	// depth fade: swap fragment module for single-texture blended surfaces
	if ( vk.depthFade.active && (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ) {
		int fogIdx = def->fog_stage ? 1 : 0;
		switch ( def->shader_type ) {
			case TYPE_SIGNLE_TEXTURE:
			case TYPE_SINGLE_TEXTURE_ENV:
				fs_module = &vk.modules.frag.dfade_gen[0][fogIdx];
				break;
			case TYPE_SINGLE_TEXTURE_IDENTITY:
			case TYPE_SINGLE_TEXTURE_IDENTITY_ENV:
				fs_module = &vk.modules.frag.dfade_ident1[0][fogIdx];
				break;
			case TYPE_SINGLE_TEXTURE_FIXED_COLOR:
			case TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV:
				fs_module = &vk.modules.frag.dfade_fixed[0][fogIdx];
				break;
			case TYPE_SINGLE_TEXTURE_ENT_COLOR:
			case TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV:
				fs_module = &vk.modules.frag.dfade_ent[0][fogIdx];
				break;
			default:
				break; // no depth fade variant for multi-texture
		}
	}

	set_shader_stage_desc(shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, *vs_module, "main");
	set_shader_stage_desc(shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, *fs_module, "main");

	//memset( vert_spec_data, 0, sizeof( vert_spec_data ) );
	memset( frag_spec_data, 0, sizeof( frag_spec_data ) );

	//vert_spec_data[0] = def->clipping_plane ? 1 : 0;

	// fragment shader specialization data
	atest_bits = state_bits & GLS_ATEST_BITS;
	switch ( atest_bits ) {
		case GLS_ATEST_GT_0:
			frag_spec_data[0].i = 1; // not equal
			frag_spec_data[1].f = 0.0f;
			break;
		case GLS_ATEST_LT_80:
			frag_spec_data[0].i = 2; // less than
			frag_spec_data[1].f = 0.5f;
			break;
		case GLS_ATEST_GE_80:
			frag_spec_data[0].i = 3; // greater or equal
			frag_spec_data[1].f = 0.5f;
			break;
		default:
			frag_spec_data[0].i = 0;
			frag_spec_data[1].f = 0.0f;
			break;
	};

	// depth fragment threshold
	frag_spec_data[2].f = 0.85f;

	// depth fade scale (soft particles)
	frag_spec_data[11].f = 2.0f;

#if 0
	if ( r_ext_alpha_to_coverage->integer && vkSamples != VK_SAMPLE_COUNT_1_BIT && frag_spec_data[0].i ) {
		frag_spec_data[3].i = 1;
		alphaToCoverage = VK_TRUE;
	}
#endif

	// constant color
	switch ( def->shader_type ) {
		default: frag_spec_data[4].i = 0; break;
		case TYPE_COLOR_WHITE: frag_spec_data[4].i = 1; break;
		case TYPE_COLOR_GREEN: frag_spec_data[4].i = 2; break;
		case TYPE_COLOR_RED:   frag_spec_data[4].i = 3; break;
	}

	// abs lighting
	switch ( def->shader_type ) {
		case TYPE_SINGLE_TEXTURE_LIGHTING:
		case TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR:
#if FEAT_PARALLAX_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX:
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX_LINEAR:
#endif
#if FEAT_ADVANCED_WATER
		case TYPE_WATER:
#endif
#if FEAT_SHADOW_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW:
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW_LINEAR:
#endif
#if FEAT_PBR
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR:
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR_LINEAR:
#endif
			frag_spec_data[5].i = def->abs_light ? 1 : 0;
		default:
			break;
	}

	// multutexture mode
	switch ( def->shader_type ) {
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_MUL_ENV:
			frag_spec_data[6].i = 0;
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
			frag_spec_data[6].i = 1;
			break;

		case TYPE_MULTI_TEXTURE_ADD2:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
		case TYPE_MULTI_TEXTURE_ADD3:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_ADD_ENV:
			frag_spec_data[6].i = 2;
			break;

		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ALPHA_ENV:
			frag_spec_data[6].i = 3;
			break;

		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 4;
			break;

		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
			frag_spec_data[6].i = 5;
			break;

		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 6;
			break;

		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			frag_spec_data[6].i = 7;
			break;

		default:
			break;
	}

	frag_spec_data[8].f = ((float)def->color.rgb) / 255.0;
	frag_spec_data[9].f = ((float)def->color.alpha) / 255.0;

	if ( def->fog_stage ) {
		frag_spec_data[10].i = def->acff;
	} else {
		frag_spec_data[10].i = 0;
	}

	//
	// vertex module specialization data
	//
#if 0
	spec_entries[0].constantID = 0; // clip_plane
	spec_entries[0].offset = 0 * sizeof( int32_t );
	spec_entries[0].size = sizeof( int32_t );

	vert_spec_info.mapEntryCount = 1;
	vert_spec_info.pMapEntries = spec_entries + 0;
	vert_spec_info.dataSize = 1 * sizeof( int32_t );
	vert_spec_info.pData = &vert_spec_data[0];
	shader_stages[0].pSpecializationInfo = &vert_spec_info;
#endif
	shader_stages[0].pSpecializationInfo = NULL;

	//
	// fragment module specialization data
	//

	spec_entries[1].constantID = 0;  // alpha-test-function
	spec_entries[1].offset = 0 * sizeof( int32_t );
	spec_entries[1].size = sizeof( int32_t );

	spec_entries[2].constantID = 1; // alpha-test-value
	spec_entries[2].offset = 1 * sizeof( int32_t );
	spec_entries[2].size = sizeof( float );

	spec_entries[3].constantID = 2; // depth-fragment
	spec_entries[3].offset = 2 * sizeof( int32_t );
	spec_entries[3].size = sizeof( float );

	spec_entries[4].constantID = 3; // alpha-to-coverage
	spec_entries[4].offset = 3 * sizeof( int32_t );
	spec_entries[4].size = sizeof( int32_t );

	spec_entries[5].constantID = 4; // color_mode
	spec_entries[5].offset = 4 * sizeof( int32_t );
	spec_entries[5].size = sizeof( int32_t );

	spec_entries[6].constantID = 5; // abs_light
	spec_entries[6].offset = 5 * sizeof( int32_t );
	spec_entries[6].size = sizeof( int32_t );

	spec_entries[7].constantID = 6; // multitexture mode
	spec_entries[7].offset = 6 * sizeof( int32_t );
	spec_entries[7].size = sizeof( int32_t );

	spec_entries[8].constantID = 7; // discard mode
	spec_entries[8].offset = 7 * sizeof( int32_t );
	spec_entries[8].size = sizeof( int32_t );

	spec_entries[9].constantID = 8; // fixed color
	spec_entries[9].offset = 8 * sizeof( int32_t );
	spec_entries[9].size = sizeof( float );

	spec_entries[10].constantID = 9; // fixed alpha
	spec_entries[10].offset = 9 * sizeof( int32_t );
	spec_entries[10].size = sizeof( float );

	spec_entries[11].constantID = 10; // acff
	spec_entries[11].offset = 10 * sizeof( int32_t );
	spec_entries[11].size = sizeof( int32_t );

	spec_entries[12].constantID = 11; // depth_fade_scale
	spec_entries[12].offset = 11 * sizeof( int32_t );
	spec_entries[12].size = sizeof( float );

	frag_spec_info.mapEntryCount = 12;
	frag_spec_info.pMapEntries = spec_entries + 1;
	frag_spec_info.dataSize = sizeof( int32_t ) * 12;
	frag_spec_info.pData = &frag_spec_data[0];

	// MSDF fragment shader has its own specialization layout (constant_id=0 is
	// msdf_distance_range, not alpha-test-function). Shader compiled-in default
	// is 8.0, matching all shipped atlases (pxrange 8). Pass NULL to use default.
	if ( def->shader_type == TYPE_MSDF || def->shader_type == TYPE_LIGHTSTYLES
	  || def->shader_type == TYPE_LIGHTSTYLES_ARRAY )
		shader_stages[1].pSpecializationInfo = NULL;
	else
		shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	switch ( def->shader_type ) {

		case TYPE_FOG_ONLY:
		case TYPE_DOT:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_DF:
		case TYPE_SINGLE_TEXTURE_IDENTITY:
		case TYPE_SINGLE_TEXTURE_FIXED_COLOR:
		case TYPE_SINGLE_TEXTURE_ENT_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MSDF:
		case TYPE_SIGNLE_TEXTURE:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_IDENTITY_ENV:
		case TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV:
		case TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING:
		case TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR:
#if FEAT_PARALLAX_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX:
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX_LINEAR:
#endif
#if FEAT_ADVANCED_WATER
		case TYPE_WATER:
#endif
#if FEAT_SHADOW_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW:
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW_LINEAR:
#endif
#if FEAT_PBR
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR:
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR_LINEAR:
#endif
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( vec2_t ) );					// st0 array
			push_bind( 2, sizeof( vec4_t ) );					// normals array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

#if FEAT_SHADOW_MAPPING
		case TYPE_SHADOW_DEPTH:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array only
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;
#endif

		case TYPE_LIGHTSTYLES:
		case TYPE_LIGHTSTYLES_ARRAY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		default:
			ri.Terminate( TERM_CLIENT_DROP, "%s: invalid shader type - %i", __func__, def->shader_type );
			break;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.pVertexBindingDescriptions = bindings;
	vertex_input_state.pVertexAttributeDescriptions = attribs;
	vertex_input_state.vertexBindingDescriptionCount = num_binds;
	vertex_input_state.vertexAttributeDescriptionCount = num_attrs;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	switch ( def->primitives ) {
		case LINE_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case POINT_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case TRIANGLE_STRIP: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		default: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
	}

	//
	// Viewport.
	//
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = NULL; // dynamic viewport state
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = NULL; // dynamic scissor state

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
#if FEAT_DEPTH_CLAMP
	rasterization_state.depthClampEnable = r_depthClamp && r_depthClamp->integer ? VK_TRUE : VK_FALSE;
#else
	rasterization_state.depthClampEnable = VK_FALSE;
#endif
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	if ( def->shader_type == TYPE_DOT ) {
		rasterization_state.polygonMode = VK_POLYGON_MODE_POINT;
	} else {
		rasterization_state.polygonMode = (state_bits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	}

	switch ( def->face_culling ) {
		case CT_TWO_SIDED:
			rasterization_state.cullMode = VK_CULL_MODE_NONE;
			break;
		case CT_FRONT_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
			break;
		case CT_BACK_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
			break;
		default:
			ri.Terminate( TERM_CLIENT_DROP, "create_pipeline: invalid face culling mode %i\n", def->face_culling );
			break;
	}

	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order

	 // depth bias state
	if ( def->polygon_offset ) {
		rasterization_state.depthBiasEnable = VK_TRUE;
		rasterization_state.depthBiasClamp = 0.0f;
#ifdef USE_REVERSED_DEPTH
		rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
		rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
	} else {
		rasterization_state.depthBiasEnable = VK_FALSE;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	if ( def->line_width )
		rasterization_state.lineWidth = (float)def->line_width;
	else
		rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;

	multisample_state.rasterizationSamples = (renderPassIndex == RENDER_PASS_SCREENMAP) ? vk.screenMapSamples : vkSamples;

	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = alphaToCoverage;
	multisample_state.alphaToOneEnable = VK_FALSE;

	memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = (state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
	depth_stencil_state.depthWriteEnable = (state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
#endif
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = (def->shadow_phase != SHADOW_DISABLED) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = (def->face_culling == CT_FRONT_SIDED) ? VK_STENCIL_OP_INCREMENT_AND_CLAMP : VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;

	} else if (def->shadow_phase == SHADOW_FS_QUAD) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES || def->shader_type == TYPE_SINGLE_TEXTURE_DF)
		attachment_blend_state.colorWriteMask = 0;
	else
		attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	if (attachment_blend_state.blendEnable) {
		switch (state_bits & GLS_SRCBLEND_BITS) {
			case GLS_SRCBLEND_ZERO:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Terminate( TERM_CLIENT_DROP, "create_pipeline: invalid src blend state bits\n" );
				break;
		}
		switch (state_bits & GLS_DSTBLEND_BITS) {
			case GLS_DSTBLEND_ZERO:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Terminate( TERM_CLIENT_DROP, "create_pipeline: invalid dst blend state bits\n" );
				break;
		}

		attachment_blend_state.srcAlphaBlendFactor = attachment_blend_state.srcColorBlendFactor;
		attachment_blend_state.dstAlphaBlendFactor = attachment_blend_state.dstColorBlendFactor;
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;

		if ( def->allow_discard && vkSamples != VK_SAMPLE_COUNT_1_BIT ) {
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ) {
				frag_spec_data[7].i = 1;
			} else if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE ) {
				frag_spec_data[7].i = 2;
			}
		}
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_state_array );
	dynamic_state.pDynamicStates = dynamic_state_array;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = ARRAY_LEN(shader_stages);
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;

	if ( def->shader_type == TYPE_DOT )
		create_info.layout = vk.pipeline_layout_storage;
	else if ( def->shader_type == TYPE_MSDF )
		create_info.layout = vk.pipeline_layout_msdf;
	else
		create_info.layout = vk.pipeline_layout;

	if ( renderPassIndex == RENDER_PASS_SCREENMAP )
		create_info.renderPass = vk.render_pass.screenmap;
	else
		create_info.renderPass = vk.render_pass.main;

	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &pipeline ) );

	SET_OBJECT_NAME( pipeline, va( "pipeline def#%i, pass#%i", def_index, renderPassIndex ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	vk.pipeline_create_count++;

	return pipeline;
}


static uint32_t vk_alloc_pipeline( const Vk_Pipeline_Def *def ) {
	VK_Pipeline_t *pipeline;
	if ( vk.pipelines_count >= MAX_VK_PIPELINES ) {
		ri.Terminate( TERM_CLIENT_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached" );
		return 0;
	}
	int j;
	pipeline	  = &vk.pipelines[vk.pipelines_count];
	pipeline->def = *def;
	for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
		pipeline->handle[j] = VK_NULL_HANDLE;
	}
	return vk.pipelines_count++;
}


VkPipeline vk_gen_pipeline( uint32_t index ) {
	if ( index < vk.pipelines_count ) {
		VK_Pipeline_t *pipeline = vk.pipelines + index;
		const renderPass_t pass = vk.renderPassIndex;
		if ( pipeline->handle[ pass ] == VK_NULL_HANDLE ) {
			pipeline->handle[ pass ] = create_pipeline( &pipeline->def, pass, index );
		}
		return pipeline->handle[ pass ];
	}
	ri.Terminate( TERM_UNRECOVERABLE, "%s(%i): NULL pipeline", __func__, index );
	return VK_NULL_HANDLE;
}


uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use ) {
	const Vk_Pipeline_Def *cur_def;
	uint32_t index;

	for ( index = base; index < vk.pipelines_count; index++ ) {
		cur_def = &vk.pipelines[ index ].def;
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			goto found;
		}
	}

	index = vk_alloc_pipeline( def );
found:

	if ( use )
		vk_gen_pipeline( index );

	return index;
}


void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def ) {
	if ( pipeline >= vk.pipelines_count ) {
		memset( def, 0, sizeof( *def ) );
	} else {
		memcpy( def, &vk.pipelines[ pipeline ].def, sizeof( *def ) );
	}
}


static void get_viewport_rect(VkRect2D *r)
{
	if ( backEnd.projection2D )
	{
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk.renderWidth;
		r->extent.height = vk.renderHeight;
	}
	else
	{
		r->offset.x = backEnd.viewParms.viewportX * vk.renderScaleX;
		r->offset.y = vk.renderHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight) * vk.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;
	}
}

static void get_viewport(VkViewport *viewport, Vk_Depth_Range depth_range) {
	VkRect2D r;

	get_viewport_rect( &r );

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch ( depth_range ) {
		default:
#ifdef USE_REVERSED_DEPTH
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.6f;
			viewport->maxDepth = 1.0f;
			break;
#else
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.3f;
			break;
#endif
	}
}

static void get_scissor_rect(VkRect2D *r) {

	if ( backEnd.viewParms.portalView != PV_NONE )
	{
		r->offset.x = backEnd.viewParms.scissorX;
		r->offset.y = glConfig.vidHeight - backEnd.viewParms.scissorY - backEnd.viewParms.scissorHeight;
		r->extent.width = backEnd.viewParms.scissorWidth;
		r->extent.height = backEnd.viewParms.scissorHeight;
	}
	else
	{
		get_viewport_rect(r);

		if (r->offset.x < 0)
			r->offset.x = 0;
		if (r->offset.y < 0)
			r->offset.y = 0;

		if (r->offset.x + r->extent.width > glConfig.vidWidth)
			r->extent.width = glConfig.vidWidth - r->offset.x;
		if (r->offset.y + r->extent.height > glConfig.vidHeight)
			r->extent.height = glConfig.vidHeight - r->offset.y;
	}
}


static void get_mvp_transform( float *mvp )
{
	if ( backEnd.projection2D )
	{
		float mvp0 = 2.0f / glConfig.vidWidth;
		float mvp5 = 2.0f / glConfig.vidHeight;

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
#ifdef USE_REVERSED_DEPTH
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 0.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 1.0f; mvp[15] = 1.0f;
#else
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 1.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 0.0f; mvp[15] = 1.0f;
#endif
	}
	else
	{
		const float *p = backEnd.viewParms.projectionMatrix;
		float proj[16];
		memcpy( proj, p, 64 );

		// update q3's proj matrix (opengl) to vulkan conventions: z - [0, 1] instead of [-1, 1] and invert y direction
		proj[5] = -p[5];
		//proj[10] = ( p[10] - 1.0f ) / 2.0f;
		//proj[14] = p[14] / 2.0f;
		myGlMultMatrix( vk_world.modelview_transform, proj, mvp );
	}
}


void vk_clear_color( const vec4_t color ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect;

	if ( !vk.active )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = color[0];
	attachment.clearValue.color.float32[1] = color[1];
	attachment.clearValue.color.float32[2] = color[2];
	attachment.clearValue.color.float32[3] = color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	get_scissor_rect( &clear_rect.rect );
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, &clear_rect );
}


void vk_clear_depth( qboolean clear_stencil ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if ( !vk.active )
		return;

	if ( vk_world.dirty_depth_attachment == 0 )
		return;

	attachment.colorAttachment = 0;
#ifdef USE_REVERSED_DEPTH
	attachment.clearValue.depthStencil.depth = 0.0f;
#else
	attachment.clearValue.depthStencil.depth = 1.0f;
#endif
	attachment.clearValue.depthStencil.stencil = 0;
	if ( clear_stencil && glConfig.stencilBits > 0 ) {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	} else {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	get_scissor_rect( &clear_rect[0].rect );
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, clear_rect );
}


void vk_update_mvp( const float *m ) {
	float push_constants[16]; // mvp transform

	//
	// Specify push constants.
	//
	if ( m )
		memcpy( push_constants, m, sizeof( push_constants ) );
	else
		get_mvp_transform( push_constants );

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( push_constants ), push_constants );

	vk.stats.push_size += sizeof( push_constants );
}


/*
====================
vk_set_2d_scissor

Sets a scissor rect on the current command buffer. Accepts a 4-int array
{x, y, w, h} in pixel coordinates, or NULL to restore the full-screen
scissor. Used by RE_SetClipRegion to clamp 2D drawing to a rectangular
region. Wraps the static qvkCmdSetScissor binding so non-vk.c callers can
drive it.
====================
*/
void vk_set_2d_scissor( const int *rect ) {
	VkRect2D scissor;
	if ( !vk.active || !vk.cmd ) {
		return;
	}
	if ( rect ) {
		int x = rect[0];
		int y = rect[1];
		int w = rect[2];
		int h = rect[3];
		if ( x < 0 ) x = 0;
		if ( y < 0 ) y = 0;
		if ( w < 0 ) w = 0;
		if ( h < 0 ) h = 0;
		scissor.offset.x = x;
		scissor.offset.y = y;
		scissor.extent.width = (uint32_t)w;
		scissor.extent.height = (uint32_t)h;
	} else {
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = glConfig.vidWidth;
		scissor.extent.height = glConfig.vidHeight;
	}
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
}


#if FEAT_FOG_SYSTEM
/*
====================
vk_update_fog_push

Push the enhanced-fog parameters as a 32-byte fragment-stage push constant
at offset 64 (right after the 64-byte vertex MVP). This is a REAL push
constant upload via qvkCmdPushConstants; the pipeline layout has been
extended to reserve this range so any future fog fragment shader that wants
to read it can do so without further layout changes.

Layout (std430, 32 bytes):
  float fogColorR;    // offset 64
  float fogColorG;    // offset 68
  float fogColorB;    // offset 72
  float density;      // offset 76
  int   fogType;      // offset 80   (0=none, 1=linear, 2=exp, 3=exp2)
  float farClip;      // offset 84
  int   enabled;      // offset 88
  float _pad;         // offset 92
====================
*/
void vk_update_fog_push( const vec4_t color, int fogType, float density, float farClip, qboolean enabled ) {
	struct {
		float r, g, b, density;
		int   type;
		float farClip;
		int   enabled;
		float pad;
	} push;

	push.r       = color[0];
	push.g       = color[1];
	push.b       = color[2];
	push.density = density;
	push.type    = fogType;
	push.farClip = farClip;
	push.enabled = enabled ? 1 : 0;
	push.pad     = 0.0f;

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 64, sizeof( push ), &push );

	vk.stats.push_size += sizeof( push );
}
#endif // FEAT_FOG_SYSTEM


void vk_update_msdf_outline( float outlineWidth, const float *outlineColor,
                              float glowWidth, const float *glowColor,
                              const float *shadowOffset, const float *shadowColor )
{
	// Re-push MVP (bytes 0-63) via the MSDF layout so push constants are valid
	// for the currently bound MSDF pipeline.
	float mvp[16];
	get_mvp_transform( mvp );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_msdf,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( mvp ), mvp );

	// Push outline/glow/shadow params at offset 64 (64 bytes, std430 aligned)
	// Layout: outlineWidth(4) + glowWidth(4) + shadowOffset(8) + outlineColor(16) + glowColor(16) + shadowColor(16)
	struct {
		float outlineWidth;
		float glowWidth;
		float shadowOffset[2];
		float outlineColor[4];
		float glowColor[4];
		float shadowColor[4];
	} params;

	static const float zero4[4] = { 0, 0, 0, 0 };
	static const float zero2[2] = { 0, 0 };

	params.outlineWidth    = outlineWidth;
	params.glowWidth       = glowWidth;
	memcpy( params.shadowOffset, shadowOffset ? shadowOffset : zero2, sizeof( params.shadowOffset ) );
	memcpy( params.outlineColor, outlineColor ? outlineColor : zero4, sizeof( params.outlineColor ) );
	memcpy( params.glowColor,    glowColor    ? glowColor    : zero4, sizeof( params.glowColor ) );
	memcpy( params.shadowColor,  shadowColor  ? shadowColor  : zero4, sizeof( params.shadowColor ) );

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_msdf,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		64, sizeof( params ), &params );

	vk.stats.push_size += sizeof( mvp ) + sizeof( params );
}


static VkBuffer shade_bufs[8];
static int bind_base;
static int bind_count;

static void vk_bind_index_attr( int index )
{
	if ( bind_base == -1 ) {
		bind_base = index;
		bind_count = 1;
	} else {
		bind_count = index - bind_base + 1;
	}
}


static void vk_bind_attr( int index, unsigned int item_size, const void *src ) {
	const uint32_t offset = PAD( vk.cmd->vertex_buffer_offset, 32 );
	const uint32_t size = tess.numVertexes * item_size;

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
	} else {
		vk.cmd->buf_offset[ index ] = offset;
		memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	}

	vk_bind_index_attr( index );
}


uint32_t vk_tess_index( uint32_t numIndexes, const void *src ) {
	const uint32_t offset = vk.cmd->vertex_buffer_offset;
	const uint32_t size = numIndexes * sizeof( tess.indexes[0] );

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
		return ~0U;
	}
	memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
	vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	return offset;
}


void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset )
{
	if ( vk.cmd->curr_index_buffer != buffer || vk.cmd->curr_index_offset != offset )
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32 );

	vk.cmd->curr_index_buffer = buffer;
	vk.cmd->curr_index_offset = offset;
}


#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex )
{
	qvkCmdDrawIndexed( vk.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0 );
}
#endif


void vk_bind_index( void )
{
#ifdef USE_VBO
	if ( tess.vboIndex ) {
		vk.cmd->num_indexes = 0;
		//qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.vbo.index_buffer, tess.shader->iboOffset, VK_INDEX_TYPE_UINT32 );
		return;
	}
#endif

	vk_bind_index_ext( tess.numIndexes, tess.indexes );
}


void vk_bind_index_ext( const int numIndexes, const uint32_t *indexes )
{
	uint32_t offset	= vk_tess_index( numIndexes, indexes );
	if ( offset != ~0U ) {
		vk_bind_index_buffer( vk.cmd->vertex_buffer, offset );
		vk.cmd->num_indexes = numIndexes;
	} else {
		// overflowed
		vk.cmd->num_indexes = 0;
	}
}


void vk_bind_geometry( uint32_t flags )
{
	//unsigned int size;
	bind_base = -1;
	bind_count = 0;

	if ( ( flags & ( TESS_XYZ | TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2 | TESS_NNN | TESS_RGBA1 | TESS_RGBA2 ) ) == 0 )
		return;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.vbo.vertex_buffer;

		if ( flags & TESS_XYZ ) {  // 0
			vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
			vk_bind_index_attr( 0 );
		}

		if ( flags & TESS_RGBA0 ) { // 1
			vk.cmd->vbo_offset[1] = tess.shader->stages[ tess.vboStage ]->rgb_offset[0];
			vk_bind_index_attr( 1 );
		}

		if ( flags & TESS_ST0 ) {  // 2
			vk.cmd->vbo_offset[2] = tess.shader->stages[ tess.vboStage ]->tex_offset[0];
			vk_bind_index_attr( 2 );
		}

		if ( flags & TESS_ST1 ) {  // 3
			vk.cmd->vbo_offset[3] = tess.shader->stages[ tess.vboStage ]->tex_offset[1];
			vk_bind_index_attr( 3 );
		}

		if ( flags & TESS_ST2 ) {  // 4
			vk.cmd->vbo_offset[4] = tess.shader->stages[ tess.vboStage ]->tex_offset[2];
			vk_bind_index_attr( 4 );
		}

		if ( flags & TESS_NNN ) { // 5
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );
		}

		if ( flags & TESS_RGBA1 ) { // 6
			vk.cmd->vbo_offset[6] = tess.shader->stages[ tess.vboStage ]->rgb_offset[1];
			vk_bind_index_attr( 6 );
		}

		if ( flags & TESS_RGBA2 ) { // 7
			vk.cmd->vbo_offset[7] = tess.shader->stages[ tess.vboStage ]->rgb_offset[2];
			vk_bind_index_attr( 7 );
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->vbo_offset + bind_base );

	} else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.cmd->vertex_buffer;

		if ( flags & TESS_XYZ ) {
			vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		}

		if ( flags & TESS_RGBA0 ) {
			vk_bind_attr(1, sizeof( color4ub_t ), tess.svars.colors[0][0].rgba);
		}

		if ( flags & TESS_ST0 ) {
			vk_bind_attr(2, sizeof( vec2_t ), tess.svars.texcoordPtr[0]);
		}

		if ( flags & TESS_ST1 ) {
			vk_bind_attr(3, sizeof( vec2_t ), tess.svars.texcoordPtr[1]);
		}

		if ( flags & TESS_ST2 ) {
			vk_bind_attr(4, sizeof( vec2_t ), tess.svars.texcoordPtr[2]);
		}

		if ( flags & TESS_NNN ) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
		}

		if ( flags & TESS_RGBA1 ) {
			vk_bind_attr(6, sizeof( color4ub_t ), tess.svars.colors[1][0].rgba);
		}

		if ( flags & TESS_RGBA2 ) {
			vk_bind_attr(7, sizeof( color4ub_t ), tess.svars.colors[2][0].rgba);
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_bind_lighting( int stage, int bundle )
{
	bind_base = -1;
	bind_count = 0;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.vbo.vertex_buffer;

		vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
		vk.cmd->vbo_offset[1] = tess.shader->stages[ stage ]->tex_offset[ bundle ];
		vk.cmd->vbo_offset[2] = tess.shader->normalOffset;

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 3, shade_bufs, vk.cmd->vbo_offset + 0 );

	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.cmd->vertex_buffer;

		vk_bind_attr( 0, sizeof( tess.xyz[0] ), &tess.xyz[0] );
		vk_bind_attr( 1, sizeof( vec2_t ), tess.svars.texcoordPtr[ bundle ] );
		vk_bind_attr( 2, sizeof( tess.normal[0] ), tess.normal );

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_reset_descriptor( int index )
{
	vk.cmd->descriptor_set.current[ index ] = VK_NULL_HANDLE;
}


void vk_update_descriptor( int index, VkDescriptorSet descriptor )
{
	if ( vk.cmd->descriptor_set.current[ index ] != descriptor ) {
		vk.cmd->descriptor_set.start = ( index < vk.cmd->descriptor_set.start ) ? index : vk.cmd->descriptor_set.start;
		vk.cmd->descriptor_set.end = ( index > vk.cmd->descriptor_set.end ) ? index : vk.cmd->descriptor_set.end;
	}
	vk.cmd->descriptor_set.current[ index ] = descriptor;
}


void vk_update_descriptor_offset( int index, uint32_t offset )
{
	vk.cmd->descriptor_set.offset[ index ] = offset;
}


void vk_bind_descriptor_sets( void )
{
	uint32_t offsets[2], offset_count;
	uint32_t start, end, count, i;

	start = vk.cmd->descriptor_set.start;
	if ( start == ~0U )
		return;

	end = vk.cmd->descriptor_set.end;

	offset_count = 0;
	if ( /*start == VK_DESC_STORAGE || */ start == VK_DESC_UNIFORM ) { // uniform offset or storage offset
		offsets[ offset_count++ ] = vk.cmd->descriptor_set.offset[ start ];
	}

	count = end - start + 1;

	// fill NULL descriptor gaps
	for ( i = start + 1; i < end; i++ ) {
		if ( vk.cmd->descriptor_set.current[i] == VK_NULL_HANDLE ) {
			vk.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
		}
	}

	/* Bind via the layout the currently-bound pipeline was created with
	 * (set by vk_bind_pipeline). Falls back to vk.pipeline_layout when no
	 * pipeline is bound — matches the historical hardcoded behavior for
	 * that pre-bind window and after callers that reset last_pipeline =
	 * VK_NULL_HANDLE (IQM / bloom / etc.) to force a rebind. */
	{
		VkPipelineLayout layout = vk.cmd->last_pipeline != VK_NULL_HANDLE
		                          ? vk.cmd->last_pipeline_layout
		                          : vk.pipeline_layout;
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );
	}

	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
}


void vk_bind_pipeline( uint32_t pipeline ) {
	VkPipeline vkpipe;

	vkpipe = vk_gen_pipeline( pipeline );

	if ( vkpipe != vk.cmd->last_pipeline ) {
		/* Track which layout this pipeline was created with so
		 * vk_bind_descriptor_sets binds via a compatible layout. Mirrors the
		 * layout selection inside vk_create_pipeline (TYPE_DOT/TYPE_MSDF/default). */
		const int shader_type = vk.pipelines[ pipeline ].def.shader_type;
		if ( shader_type == TYPE_DOT )
			vk.cmd->last_pipeline_layout = vk.pipeline_layout_storage;
		else if ( shader_type == TYPE_MSDF )
			vk.cmd->last_pipeline_layout = vk.pipeline_layout_msdf;
		else
			vk.cmd->last_pipeline_layout = vk.pipeline_layout;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe );
		vk.cmd->last_pipeline = vkpipe;
		vk_diag_pipebinds++;
		vk_diag_msdf_active = ( pipeline == vk.msdf_pipeline );
		if ( vk_diag_msdf_active )
			vk_diag_msdf_binds++;
	}

	vk_world.dirty_depth_attachment |= ( vk.pipelines[ pipeline ].def.state_bits & GLS_DEPTHMASK_TRUE );
}

static void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk.cmd->depth_range != depth_range ) {
		VkRect2D scissor_rect;
		VkViewport viewport;

		vk.cmd->depth_range = depth_range;

		get_scissor_rect( &scissor_rect );

		if ( memcmp( &vk.cmd->scissor_rect, &scissor_rect, sizeof( scissor_rect ) ) != 0 ) {
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
			vk.cmd->scissor_rect = scissor_rect;
		}

		get_viewport( &viewport, depth_range );
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	}
}


void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed ) {

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	vk_update_depth_range( depth_range );

	// issue draw call(s)
#ifdef USE_VBO
	if ( tess.vboIndex )
		VBO_RenderIBOItems();
	else
#endif
	if ( indexed ) {
		qvkCmdDrawIndexed( vk.cmd->command_buffer, vk.cmd->num_indexes, 1, 0, 0, 0 );
	} else {
		qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
	}
	// NOLINTNEXTLINE(readability-misleading-indentation) — Q3 split-else-if / preprocessor-conditional idiom; statement is at correct enclosing scope
	vk_diag_drawcalls++;
	if ( vk_diag_msdf_active )
		vk_diag_msdf_draws++;
}


void vk_draw_dot( uint32_t storage_offset )
{
	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_storage, VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );

	// configure pipeline's dynamic state
	vk_update_depth_range( DEPTH_RANGE_NORMAL );

	qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
}


static void vk_begin_render_pass( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[3];

	// Begin render pass.

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if ( clearValues ) {
		// attachments layout:
		// [0] - resolve/color/presentation
		// [1] - depth/stencil
		// [2] - multisampled color, optional
		memset( clear_values, 0, sizeof( clear_values ) );
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		render_pass_begin_info.clearValueCount = vk.msaaActive ? 3 : 2;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	} else {
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}


/* Single-attachment variant for post-process passes (bloom_extract, blur,
 * capture, gamma).  Each of these passes was created with
 * VK_ATTACHMENT_LOAD_OP_CLEAR on its sole color attachment, so the begin
 * info must supply exactly one VkClearValue or VUID-clearValueCount-00902
 * fires.  vk_begin_render_pass()'s clearValueCount=2-or-3 shape is for
 * color+depth(+msaa) and doesn't fit here. */
static void vk_begin_render_pass_clear1( VkRenderPass renderPass, VkFramebuffer frameBuffer, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_value;

	memset( &clear_value, 0, sizeof( clear_value ) );

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.clearValueCount = 1;
	render_pass_begin_info.pClearValues = &clear_value;

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}


void vk_begin_main_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_MAIN;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.main, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}


void vk_begin_post_bloom_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_POST_BLOOM;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.post_bloom, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


/*
================
vk_depth_fade_copy

End current render pass, copy the depth buffer to the sampable depth copy image,
then begin the depth fade render pass (which loads color+depth without clearing).
This is called at the opaque->transparent transition to enable soft particle rendering.
================
*/
void vk_depth_fade_copy( void )
{
	VkImageMemoryBarrier barriers[2];
	VkFramebuffer frameBuffer;

	if ( !vk.depthFade.active || vk.depthFade.copied )
		return;

	// end the current render pass
	qvkCmdEndRenderPass( vk.cmd->command_buffer );

	// barrier: depth attachment -> transfer src
	memset( barriers, 0, sizeof( barriers ) );

	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = vk.depth_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		barriers[0].subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;

	// barrier: depth copy image -> transfer dst
	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].srcAccessMask = 0;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = vk.depthFade.image;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );

	// copy depth (resolve MSAA if needed)
	if ( vk.msaaActive ) {
		// for MSAA, we need vkCmdResolveImage — but depth resolve isn't widely supported
		// fallback: just copy (will work for non-MSAA depth, which is what we have when fboActive && !msaa)
		// TODO: handle MSAA depth resolve if hardware supports VK_KHR_depth_stencil_resolve
		VkImageCopy region;
		memset( &region, 0, sizeof( region ) );
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.srcSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.dstSubresource.layerCount = 1;
		region.extent.width = glConfig.vidWidth;
		region.extent.height = glConfig.vidHeight;
		region.extent.depth = 1;
		qvkCmdCopyImage( vk.cmd->command_buffer,
			vk.depth_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.depthFade.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region );
	} else {
		VkImageCopy region;
		memset( &region, 0, sizeof( region ) );
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.srcSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.dstSubresource.layerCount = 1;
		region.extent.width = glConfig.vidWidth;
		region.extent.height = glConfig.vidHeight;
		region.extent.depth = 1;
		qvkCmdCopyImage( vk.cmd->command_buffer,
			vk.depth_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.depthFade.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region );
	}

	// barrier: depth back to attachment, depth copy to shader read
	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );

	// begin depth fade render pass (loads color+depth)
	frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_MAIN; // still in "main" context for pipeline compatibility
	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.depth_fade, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );

	// bind the depth copy descriptor to set 5
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout, VK_DESC_DEPTH_FADE, 1, &vk.depthFade.descriptor, 0, NULL );

	vk.depthFade.copied = qtrue;
}


/*
================
vk_smaa

Perform SMAA anti-aliasing as a post-process:
  1. Copy color_image -> input_image
  2. Edge detection pass (input -> edges)
  3. Blend weight calculation (edges + area + search -> blend)
  4. Neighborhood blending / resolve (input + blend -> color_image)
================
*/
void vk_smaa( void )
{
	VkImageMemoryBarrier barriers[2];
	VkImageCopy copy_region;
	float rtMetrics[4];
	VkClearValue clear_value;

	if ( !vk.smaa.active )
		return;

	rtMetrics[0] = 1.0f / (float)glConfig.vidWidth;
	rtMetrics[1] = 1.0f / (float)glConfig.vidHeight;
	rtMetrics[2] = (float)glConfig.vidWidth;
	rtMetrics[3] = (float)glConfig.vidHeight;

	// end current render pass
	vk_end_render_pass();
	vk_gpu_ts_write( "smaa" ); // outside render pass — MoltenVK timestamps only resolve at encoder boundaries

	// --- Step 0: Copy color_image -> input_image ---

	// barrier: color_image SHADER_READ -> TRANSFER_SRC, input_image SHADER_READ -> TRANSFER_DST
	memset( barriers, 0, sizeof( barriers ) );

	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = vk.color_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].srcAccessMask = 0;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = vk.smaa.input_image;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );

	// copy
	memset( &copy_region, 0, sizeof( copy_region ) );
	copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.srcSubresource.layerCount = 1;
	copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.dstSubresource.layerCount = 1;
	copy_region.extent.width = glConfig.vidWidth;
	copy_region.extent.height = glConfig.vidHeight;
	copy_region.extent.depth = 1;

	qvkCmdCopyImage( vk.cmd->command_buffer,
		vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.smaa.input_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copy_region );

	// barrier: both back to SHADER_READ_ONLY
	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );

	// --- Pass 1: Edge detection ---
	memset( &clear_value, 0, sizeof( clear_value ) );
	{
		VkRenderPassBeginInfo rp_info;
		memset( &rp_info, 0, sizeof( rp_info ) );
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_info.renderPass = vk.render_pass.smaa_edge;
		rp_info.framebuffer = vk.framebuffers.smaa_edge;
		rp_info.renderArea.extent.width = glConfig.vidWidth;
		rp_info.renderArea.extent.height = glConfig.vidHeight;
		rp_info.clearValueCount = 1;
		rp_info.pClearValues = &clear_value;

		qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE );
	}

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.smaa_edge_pipeline );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_smaa,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( rtMetrics ), rtMetrics );

	// set 0: input image
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_smaa, 0, 1, &vk.smaa.input_descriptor, 0, NULL );

	qvkCmdDraw( vk.cmd->command_buffer, 3, 1, 0, 0 );
	qvkCmdEndRenderPass( vk.cmd->command_buffer );

	// --- Pass 2: Blend weight calculation ---
	{
		VkRenderPassBeginInfo rp_info;
		memset( &rp_info, 0, sizeof( rp_info ) );
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_info.renderPass = vk.render_pass.smaa_blend;
		rp_info.framebuffer = vk.framebuffers.smaa_blend;
		rp_info.renderArea.extent.width = glConfig.vidWidth;
		rp_info.renderArea.extent.height = glConfig.vidHeight;
		rp_info.clearValueCount = 1;
		rp_info.pClearValues = &clear_value;

		qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE );
	}

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.smaa_blend_pipeline );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_smaa,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( rtMetrics ), rtMetrics );

	// set 0: edges, set 1: area LUT, set 2: search LUT
	{
		VkDescriptorSet sets[3];
		sets[0] = vk.smaa.edges_descriptor;
		sets[1] = vk.smaa.area_descriptor;
		sets[2] = vk.smaa.search_descriptor;
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_smaa, 0, 3, sets, 0, NULL );
	}

	qvkCmdDraw( vk.cmd->command_buffer, 3, 1, 0, 0 );
	qvkCmdEndRenderPass( vk.cmd->command_buffer );

	// --- Pass 3: Neighborhood blending / resolve ---
	{
		VkRenderPassBeginInfo rp_info;
		VkClearValue resolve_clear;
		memset( &resolve_clear, 0, sizeof( resolve_clear ) );
		memset( &rp_info, 0, sizeof( rp_info ) );
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_info.renderPass = vk.render_pass.smaa_resolve;
		rp_info.framebuffer = vk.framebuffers.smaa_resolve;
		rp_info.renderArea.extent.width = glConfig.vidWidth;
		rp_info.renderArea.extent.height = glConfig.vidHeight;
		rp_info.clearValueCount = 1;
		rp_info.pClearValues = &resolve_clear;

		qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE );
	}

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.smaa_resolve_pipeline );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_smaa,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( rtMetrics ), rtMetrics );

	// set 0: input (original color), set 1: blend weights
	{
		VkDescriptorSet sets[3];
		sets[0] = vk.smaa.input_descriptor;
		sets[1] = vk.smaa.blend_descriptor;
		sets[2] = vk.smaa.blend_descriptor; // padding for layout compatibility (3 sets)
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_smaa, 0, 3, sets, 0, NULL );
	}

	qvkCmdDraw( vk.cmd->command_buffer, 3, 1, 0, 0 );
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
}


void vk_begin_bloom_extract_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.bloom_extract;

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth;
	vk.renderHeight = gls.captureHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_clear1( vk.render_pass.bloom_extract, frameBuffer, vk.renderWidth, vk.renderHeight );
}


void vk_begin_blur_render_pass( uint32_t index )
{
	VkFramebuffer frameBuffer = vk.framebuffers.blur[ index ];

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth / ( 2 << ( index / 2 ) );
	vk.renderHeight = gls.captureHeight / ( 2 << ( index / 2 ) );

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_clear1( vk.render_pass.blur[ index ], frameBuffer, vk.renderWidth, vk.renderHeight );
}


static void vk_begin_screenmap_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.screenmap;

	vk.renderPassIndex = RENDER_PASS_SCREENMAP;

	vk.renderWidth = vk.screenMapWidth;
	vk.renderHeight = vk.screenMapHeight;

	vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass( vk.render_pass.screenmap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}


void vk_end_render_pass( void )
{
	qvkCmdEndRenderPass( vk.cmd->command_buffer );

//	vk.renderPassIndex = RENDER_PASS_MAIN;
}


static qboolean vk_find_screenmap_drawsurfs( void )
{
	const void *curCmd = &backEndData->commands.cmds;
	const drawBufferCommand_t *db_cmd;
	const drawSurfsCommand_t *ds_cmd;

	for ( ;; ) {
		curCmd = PADP( curCmd, sizeof(void *) );
		switch ( *(const int *)curCmd ) {
			case RC_DRAW_BUFFER:
				db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
			case RC_DRAW_SURFS:
				ds_cmd = (const drawSurfsCommand_t *)curCmd;
				return ds_cmd->refdef.needScreenMap;
			default:
				return qfalse;
		}
	}
}


// ---------------------------------------------------------------------------
// Background fence-wait thread
// Moves vkWaitForFences + vkResetFences off the main thread so game simulation
// and command recording are not blocked by Metal's drawable-release latency.
// With NUM_COMMAND_BUFFERS=3 at 100 FPS (10ms frames), each slot is reused after
// 30ms. Metal releases the drawable after ~16.67ms. The thread finishes its wait
// ~13ms before the main thread ever needs the slot — making the main-thread wait
// effectively 0ms in steady state.
// ---------------------------------------------------------------------------
#ifndef _WIN32
typedef struct {
	int     slot;
	VkFence fence;
} vk_fence_work_t;

static pthread_t         vk_ft_thread;
static pthread_mutex_t   vk_ft_mutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t    vk_ft_cwork  = PTHREAD_COND_INITIALIZER;  // main -> thread: new fence
static pthread_cond_t    vk_ft_cready = PTHREAD_COND_INITIALIZER;  // thread -> main: slot free
static qboolean          vk_ft_running;
static qboolean          vk_slot_ready[ NUM_COMMAND_BUFFERS ];
static vk_fence_work_t   vk_ft_queue[ NUM_COMMAND_BUFFERS * 2 ];
static int               vk_ft_head;
static int               vk_ft_tail;

static void *vk_fence_worker( void *arg )
{
	int     slot;
	VkFence fen;

	pthread_mutex_lock( &vk_ft_mutex );
	while ( vk_ft_running || vk_ft_head != vk_ft_tail ) {
		while ( vk_ft_head == vk_ft_tail && vk_ft_running )
			pthread_cond_wait( &vk_ft_cwork, &vk_ft_mutex );

		if ( vk_ft_head == vk_ft_tail )
			break;

		slot = vk_ft_queue[ vk_ft_head ].slot;
		fen  = vk_ft_queue[ vk_ft_head ].fence;
		vk_ft_head = ( vk_ft_head + 1 ) % ( NUM_COMMAND_BUFFERS * 2 );
		pthread_mutex_unlock( &vk_ft_mutex );

		{
			int t0 = ri.Milliseconds();
			qvkWaitForFences( vk.device, 1, &fen, VK_FALSE, (uint64_t)10000000000ULL );
			qvkResetFences( vk.device, 1, &fen );
			pthread_mutex_lock( &vk_ft_mutex );
			vk_diag_ft_fence_ms += ri.Milliseconds() - t0;
		}
		vk_slot_ready[ slot ] = qtrue;
		pthread_cond_broadcast( &vk_ft_cready );
	}
	pthread_mutex_unlock( &vk_ft_mutex );
	return NULL;
}

static void vk_fence_thread_start( void )
{
	pthread_attr_t attr;
	vk_ft_head = vk_ft_tail = 0;
	vk_ft_running = qtrue;
	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		vk_slot_ready[ i ] = qfalse; // set to qtrue by fence thread after each vkResetFences
	pthread_attr_init( &attr );
#ifdef __APPLE__
	// Boost to user-interactive QoS so macOS schedules this thread on a performance
	// core and preempts it promptly — prevents the main thread from arriving at
	// vk_slot_wait before the fence thread has had a chance to run vkWaitForFences.
	pthread_attr_set_qos_class_np( &attr, QOS_CLASS_USER_INTERACTIVE, 0 );
#endif
	pthread_create( &vk_ft_thread, &attr, vk_fence_worker, NULL );
	pthread_attr_destroy( &attr );
}

static void vk_fence_thread_stop( void )
{
	pthread_mutex_lock( &vk_ft_mutex );
	vk_ft_running = qfalse;
	pthread_cond_signal( &vk_ft_cwork );
	pthread_mutex_unlock( &vk_ft_mutex );
	pthread_join( vk_ft_thread, NULL );
}

// Called after vkQueueSubmit: hand fence to background thread.
static void vk_fence_submit( int slot, VkFence fence )
{
	pthread_mutex_lock( &vk_ft_mutex );
	vk_ft_queue[ vk_ft_tail ].slot  = slot;
	vk_ft_queue[ vk_ft_tail ].fence = fence;
	vk_ft_tail = ( vk_ft_tail + 1 ) % ( NUM_COMMAND_BUFFERS * 2 );
	pthread_cond_signal( &vk_ft_cwork );
	pthread_mutex_unlock( &vk_ft_mutex );
}

// Called at start of vk_begin_frame: wait (usually instant) for slot to be free.
static void vk_slot_wait( int slot )
{
	pthread_mutex_lock( &vk_ft_mutex );
	while ( !vk_slot_ready[ slot ] )
		pthread_cond_wait( &vk_ft_cready, &vk_ft_mutex );
	vk_slot_ready[ slot ] = qfalse;
	pthread_mutex_unlock( &vk_ft_mutex );
}

#else
// Windows: no pthread — the thread functions are stubs; fence wait happens
// synchronously in vk_begin_frame via the original vkWaitForFences path.
static void vk_fence_thread_start( void ) {}
static void vk_fence_thread_stop( void )  {}
static void vk_fence_submit( int slot, VkFence fence ) { (void)slot; (void)fence; }
static void vk_slot_wait( int slot ) { (void)slot; }
#endif  // !_WIN32

#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

// ---------------------------------------------------------------------------
// GPU per-pass timestamp module
// Usage: r_gpuSpeeds 0=off 1=200f-avg N>=2=per-frame when total>=N ms
// ---------------------------------------------------------------------------

#define VK_GPU_TS_MAX 16

static qboolean    vk_gpu_ts_active;
static VkQueryPool vk_gpu_ts_pool;
static uint32_t    vk_gpu_ts_count;
static const char *vk_gpu_ts_labels[ VK_GPU_TS_MAX ];

static struct {
	qboolean    pending;
	uint32_t    base;
	uint32_t    count;
	const char *labels[ VK_GPU_TS_MAX ];
} vk_gpu_ts_inflight[ NUM_COMMAND_BUFFERS ];

static double      vk_gpu_ts_accum_ms[ VK_GPU_TS_MAX ];
static const char *vk_gpu_ts_accum_labels[ VK_GPU_TS_MAX ];
static uint32_t    vk_gpu_ts_accum_slots;
static uint32_t    vk_gpu_ts_accum_frames;

static void vk_gpu_ts_init( void )
{
	VkQueryPoolCreateInfo info;

	vk_gpu_ts_active = qfalse;
	vk_gpu_ts_pool = VK_NULL_HANDLE;
	vk_gpu_ts_count = 0;
	vk_gpu_ts_accum_slots = 0;
	vk_gpu_ts_accum_frames = 0;
	memset( vk_gpu_ts_inflight, 0, sizeof( vk_gpu_ts_inflight ) );
	memset( vk_gpu_ts_accum_ms, 0, sizeof( vk_gpu_ts_accum_ms ) );

	if ( !vk.timestampSupported ) {
		ri.Log( SEV_DEBUG, "r_gpuSpeeds: device lacks timestampComputeAndGraphics, disabled\n" );
		return;
	}

	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk_gpu_ts_inflight[ i ].base = i * VK_GPU_TS_MAX;
	}

	memset( &info, 0, sizeof( info ) );
	info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
	info.queryCount = VK_GPU_TS_MAX * NUM_COMMAND_BUFFERS;

	if ( qvkCreateQueryPool( vk.device, &info, NULL, &vk_gpu_ts_pool ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "r_gpuSpeeds: vkCreateQueryPool failed\n" );
		return;
	}

	vk_gpu_ts_active = qtrue;
}

static void vk_gpu_ts_shutdown( void )
{
	if ( vk_gpu_ts_pool != VK_NULL_HANDLE ) {
		qvkDestroyQueryPool( vk.device, vk_gpu_ts_pool, NULL );
		vk_gpu_ts_pool = VK_NULL_HANDLE;
	}
	vk_gpu_ts_active = qfalse;
}

static void vk_gpu_ts_frame_begin( void )
{
	// host-side readback of the previous frame's results for this command-buffer slot
	uint64_t results[ 2 * VK_GPU_TS_MAX ];  // (value, availability) pairs
	int slot;
	double total_ms;
	uint32_t i;
	int gate;

	if ( !vk_gpu_ts_active )
		return;

	if ( !r_gpuSpeeds || !r_gpuSpeeds->integer )
		return;

	gate = r_gpuSpeeds->integer;
	slot = vk.cmd_index;

	if ( vk_gpu_ts_inflight[ slot ].pending && vk_gpu_ts_inflight[ slot ].count >= 2 ) {
		memset( results, 0, sizeof( results ) );
		qvkGetQueryPoolResults( vk.device, vk_gpu_ts_pool,
			vk_gpu_ts_inflight[ slot ].base,
			vk_gpu_ts_inflight[ slot ].count,
			vk_gpu_ts_inflight[ slot ].count * 2 * sizeof( uint64_t ), results,
			2 * sizeof( uint64_t ),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT );

		// accumulate inter-timestamp deltas: label[i] covers [i-1 .. i]
		// Only update slots/labels on first valid readback so we never print null labels.
		for ( i = 1; i < vk_gpu_ts_inflight[ slot ].count; i++ ) {
			if ( results[ 2*i + 1 ] && results[ 2*(i-1) + 1 ] ) {
				double delta_ns = (double)( results[ 2*i ] - results[ 2*(i-1) ] ) * (double)vk.timestampPeriodNs;
				vk_gpu_ts_accum_ms[ i - 1 ] += delta_ns * 1e-6;
				vk_gpu_ts_accum_labels[ i - 1 ] = vk_gpu_ts_inflight[ slot ].labels[ i ];
				if ( i - 1 >= vk_gpu_ts_accum_slots )
					vk_gpu_ts_accum_slots = i;  // = i, not i-1, since slots is 1-based count
			}
		}
	}

	vk_gpu_ts_inflight[ slot ].pending = qfalse;

	++vk_gpu_ts_accum_frames;

	// print in averaged mode (gate==1) or threshold mode (gate>=2, per-frame)
	if ( gate == 1 && vk_gpu_ts_accum_frames < 200 )
		return;

	total_ms = 0.0;
	for ( i = 0; i < vk_gpu_ts_accum_slots; i++ )
		total_ms += vk_gpu_ts_accum_ms[ i ];

	if ( gate >= 2 ) {
		// per-frame threshold: only print if total >= gate ms
		if ( vk_gpu_ts_accum_frames < 1 || total_ms < (double)gate )
			goto reset_accum;
		ri.Log( SEV_DEBUG, "gpu (ms):" );
		for ( i = 0; i < vk_gpu_ts_accum_slots; i++ )
			ri.Log( SEV_DEBUG, "  %s=%.2f", vk_gpu_ts_accum_labels[ i ], vk_gpu_ts_accum_ms[ i ] );
		ri.Log( SEV_DEBUG, "  total=%.2f\n", total_ms );
	} else {
		// averaged mode
		double n = (double)vk_gpu_ts_accum_frames;
		ri.Log( SEV_DEBUG, "gpu (%df avg, ms):", vk_gpu_ts_accum_frames );
		for ( i = 0; i < vk_gpu_ts_accum_slots; i++ )
			ri.Log( SEV_DEBUG, "  %s=%.2f", vk_gpu_ts_accum_labels[ i ], vk_gpu_ts_accum_ms[ i ] / n );
		ri.Log( SEV_DEBUG, "  total=%.2f\n", total_ms / n );
	}

reset_accum:
	memset( vk_gpu_ts_accum_ms, 0, sizeof( vk_gpu_ts_accum_ms ) );
	vk_gpu_ts_accum_slots = 0;
	vk_gpu_ts_accum_frames = 0;
}

static void vk_gpu_ts_pool_reset( void )
{
	// CB-side reset + first "acquire" timestamp — must run after qvkBeginCommandBuffer
	if ( !vk_gpu_ts_active || !r_gpuSpeeds || !r_gpuSpeeds->integer )
		return;

	qvkCmdResetQueryPool( vk.cmd->command_buffer, vk_gpu_ts_pool,
		vk_gpu_ts_inflight[ vk.cmd_index ].base, VK_GPU_TS_MAX );

	vk_gpu_ts_count = 0;
	qvkCmdWriteTimestamp( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		vk_gpu_ts_pool,
		vk_gpu_ts_inflight[ vk.cmd_index ].base + vk_gpu_ts_count );
	vk_gpu_ts_labels[ vk_gpu_ts_count ] = "acquire";
	vk_gpu_ts_count++;
}

static void vk_gpu_ts_write( const char *label )
{
	if ( !vk_gpu_ts_active || !r_gpuSpeeds || !r_gpuSpeeds->integer )
		return;
	if ( vk_gpu_ts_count >= VK_GPU_TS_MAX )
		return;

	qvkCmdWriteTimestamp( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		vk_gpu_ts_pool,
		vk_gpu_ts_inflight[ vk.cmd_index ].base + vk_gpu_ts_count );
	vk_gpu_ts_labels[ vk_gpu_ts_count ] = label;
	vk_gpu_ts_count++;
}

static void vk_gpu_ts_frame_end( void )
{
	uint32_t i;

	if ( !vk_gpu_ts_active || !r_gpuSpeeds || !r_gpuSpeeds->integer )
		return;

	vk_gpu_ts_inflight[ vk.cmd_index ].count   = vk_gpu_ts_count;
	vk_gpu_ts_inflight[ vk.cmd_index ].pending  = qtrue;
	for ( i = 0; i < vk_gpu_ts_count; i++ )
		vk_gpu_ts_inflight[ vk.cmd_index ].labels[ i ] = vk_gpu_ts_labels[ i ];

	vk_gpu_ts_count = 0;
}

// ---------------------------------------------------------------------------

void vk_begin_frame( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkResult res;

	if ( vk.frame_count++ ) // might happen during stereo rendering
		return;

	vk_frame_t_start = ri.Microseconds();
	vk_frame_present_done = qfalse;

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qtrue );
#endif

	vk.cmd = &vk.tess[ vk.cmd_index ];

	{
		int t_diag = ri.Milliseconds();
		if ( vk.cmd->waitForFence ) {
			vk.cmd->waitForFence = qfalse;
#ifdef _WIN32
			// Windows: no background fence thread — wait synchronously
			res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
			if ( res != VK_SUCCESS ) {
				if ( res == VK_ERROR_DEVICE_LOST ) {
					ri.Log( SEV_WARN, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
				} else {
					ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
				}
			}
			VK_CHECK( qvkResetFences( vk.device, 1, &vk.cmd->rendering_finished_fence ) );
#else
			// Background fence thread already waited + reset the fence.
			// This call is nearly instant: with 3 buffers, the slot was freed
			// ~13ms before we need it again.
			vk_slot_wait( vk.cmd_index );
#endif
		}
		{
			int fence_ms = ri.Milliseconds() - t_diag;
			vk_diag_fence_ms += fence_ms;
			if ( r_gpuSpeeds && r_gpuSpeeds->integer >= 2 && fence_ms >= r_gpuSpeeds->integer )
				ri.Log( SEV_DEBUG, "fence spike: %dms\n", fence_ms );
		}
		vk_frame_t_after_fence = ri.Microseconds();
		if ( ++vk_diag_frames >= 200 ) {
			if ( r_vkDebugTiming && r_vkDebugTiming->integer )
				ri.Log( SEV_DEBUG, "vk timing (200f avg): fence=%dms/f  ft_fence=%dms/f  acquire=%dms/f  submit=%dms/f  present=%dms/f  draws=%d/f(msdf=%d)  pipebinds=%d/f(msdf=%d)\n",
					vk_diag_fence_ms / 200, vk_diag_ft_fence_ms / 200,
					vk_diag_acquire_ms / 200, vk_diag_submit_ms / 200, vk_diag_present_ms / 200,
					vk_diag_drawcalls / 200, vk_diag_msdf_draws / 200,
					vk_diag_pipebinds / 200, vk_diag_msdf_binds / 200 );
			vk_diag_fence_ms = vk_diag_ft_fence_ms = vk_diag_submit_ms = vk_diag_present_ms = vk_diag_acquire_ms = vk_diag_frames = 0;
			vk_diag_drawcalls = vk_diag_pipebinds = vk_diag_msdf_draws = vk_diag_msdf_binds = 0;
		}
	}

	// GPU timestamp readback: fence above guarantees this slot's GPU work is done.
	vk_gpu_ts_frame_begin();

	if ( !ri.CL_IsMinimized() && !vk.cmd->swapchain_image_acquired ) {
		int t_acquire = ri.Milliseconds();
		qboolean retry = qfalse;
_retry:
		res = qvkAcquireNextImageKHR( vk.device, vk.swapchain, 1 * 1000000000ULL, vk.cmd->image_acquired, VK_NULL_HANDLE, &vk.cmd->swapchain_image_index );
		// when running via RDP: "Application has already acquired the maximum number of images (0x2)"
		// probably caused by "device lost" errors
		if ( res < 0 ) {
			if ( res == VK_ERROR_OUT_OF_DATE_KHR && retry == qfalse ) {
				// swapchain re-creation needed
				retry = qtrue;
				vk_restart_swapchain( __func__, res );
				goto _retry;
			} else {
				ri.Terminate( TERM_UNRECOVERABLE, "vkAcquireNextImageKHR returned %s", vk_result_string( res ) );
			}
		}
		vk.cmd->swapchain_image_acquired = qtrue;
		vk_diag_acquire_ms += ri.Milliseconds() - t_acquire;
	}
	vk_frame_t_after_acquire = ri.Microseconds();

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.cmd->command_buffer, &begin_info ) );
	vk_frame_t_after_begincb = ri.Microseconds();

	vk_gpu_ts_pool_reset();

	// Ensure visibility of geometry buffers writes.
	//record_buffer_memory_barrier( vk.cmd->command_buffer, vk.cmd->vertex_buffer, vk.geometry_buffer_size, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT );

#if 0
	// add explicit layout transition dependency
	if ( vk.fboActive ) {
		record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( vk.cmd->command_buffer, vk.swapchain_images[ vk.swapchain_image_index ], VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0 );
	}
#endif

	if ( vk.cmd->vertex_buffer_offset > vk.stats.vertex_buffer_max ) {
		vk.stats.vertex_buffer_max = vk.cmd->vertex_buffer_offset;
	}

	if ( vk.stats.push_size > vk.stats.push_size_max ) {
		vk.stats.push_size_max = vk.stats.push_size;
	}

	vk.cmd->last_pipeline = VK_NULL_HANDLE;

	backEnd.screenMapDone = qfalse;
	vk.depthFade.copied = qfalse;

	// Particle compute pass — runs OUTSIDE any render pass, before the
	// main / screenmap pass begins this frame. vkCmdDispatch is
	// spec-forbidden inside a render pass instance; this is the only
	// safe location in the frame to record the dispatch + the
	// compute→vertex-shader buffer barrier. RB_RunParticleCompute
	// updates only the compute region of its per-frame UBO; the render
	// region is filled later by RB_DrawParticles when
	// backEnd.viewParms is valid for this frame.
	RB_RunParticleCompute();

	if ( vk_find_screenmap_drawsurfs() ) {
#if FEAT_FBO_DEBUG
		if ( vk.frame_count <= 3 )
			ri.Log( SEV_INFO, "^3[FBO_DEBUG] Frame %d: screenmap pass selected\n", vk.frame_count );
#endif
		vk_begin_screenmap_render_pass();
	} else {
#if FEAT_FBO_DEBUG
		if ( vk.frame_count <= 3 )
			ri.Log( SEV_INFO, "^3[FBO_DEBUG] Frame %d: main pass (no screenmap)\n", vk.frame_count );
#endif
		vk_begin_main_render_pass();
	}

	// dynamic vertex buffer layout
	vk.cmd->uniform_read_offset = 0;
	vk.cmd->vertex_buffer_offset = 0;
	memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );
	vk.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk.cmd->curr_index_offset = 0;
	vk.cmd->num_indexes = 0;

	memset( &vk.cmd->descriptor_set, 0, sizeof( vk.cmd->descriptor_set ) );
	vk.cmd->descriptor_set.start = ~0U;
	//vk.cmd->descriptor_set.end = 0;

	memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

	// other stats
	vk.stats.push_size = 0;
	vk_frame_t_rec_start = ri.Microseconds();
}


static void vk_resize_geometry_buffer( void )
{
	vk_end_render_pass();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );

	vk_wait_idle();

	vk_release_geometry_buffers();

	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

	ri.Log( SEV_DEBUG, "...geometry buffer resized to %iK\n", (int)( vk.geometry_buffer_size / 1024 ) );
}


void vk_end_frame( void )
{
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore waits[2], signals[2];
	const VkPipelineStageFlags wait_dst_stage_mask[2] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
#else
	const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif
	VkSubmitInfo submit_info;

	if ( vk.frame_count == 0 )
		return;

	vk.frame_count = 0;
	vk_frame_t_rec_end = ri.Microseconds();

	if ( vk.geometry_buffer_size_new )
	{
		vk_resize_geometry_buffer();
		// issue: one frame may be lost during video recording
		// solution: re-record all commands again? (might be complicated though)
		return;
	}

	if ( vk.fboActive )
	{
#if FEAT_FBO_DEBUG
		if ( vk.frame_count == 0 ) {
			ri.Log( SEV_INFO, "^3[FBO_DEBUG] vk_end_frame: fboActive=1 bloom=%d smaa=%d\n", r_bloom->integer, vk.smaa.active );
			ri.Log( SEV_INFO, "^3[FBO_DEBUG]   color_descriptor=%p gamma_pipeline=%p\n", (void*)vk.color_descriptor, (void*)vk.gamma_pipeline );
			ri.Log( SEV_INFO, "^3[FBO_DEBUG]   renderPassIndex=%d doneSurfaces=%d\n", vk.renderPassIndex, backEnd.doneSurfaces );
		}
#endif
		vk.cmd->last_pipeline = VK_NULL_HANDLE; // do not restore clobbered descriptors in vk_bloom()

		if ( r_bloom->integer )
		{
			vk_bloom();
		}

		if ( vk.smaa.active )
		{
			// vk_smaa() ends current render pass internally, runs 3 SMAA passes,
			// and does NOT re-enter a render pass when it returns.
			vk_smaa();
		}

		if ( backEnd.screenshotMask && vk.capture.image )
		{
			if ( !vk.smaa.active ) {
				// SMAA already ended the render pass; skip if it ran
				vk_end_render_pass();
			}

			// render to capture FBO
			vk_begin_render_pass_clear1( vk.render_pass.capture, vk.framebuffers.capture, gls.captureWidth, gls.captureHeight );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.capture_pipeline );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );

			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		}

		if ( !ri.CL_IsMinimized() )
		{
			if ( !vk.smaa.active || ( backEnd.screenshotMask && vk.capture.image ) ) {
				// If SMAA ran but there was no capture pass, we're already outside
				// a render pass -- skip end. If capture ran, end the capture pass.
				vk_end_render_pass();
			}

		}

		if ( !ri.CL_IsMinimized() )
		{
			vk.renderWidth = gls.windowWidth;
			vk.renderHeight = gls.windowHeight;

			vk.renderScaleX = 1.0;
			vk.renderScaleY = 1.0;

#ifdef __APPLE__
			// MoltenVK/TBDR: flush tile cache so gamma pass sees color_image writes (main pass or bloom composition).
			// Gate under r_vkApplePinkBarrier to measure FPS impact; set to 0 to test, 1 to restore pink-glitch fix.
			if ( vk.fboActive && r_vkApplePinkBarrier->integer ) {
				VkImageMemoryBarrier b;
				memset( &b, 0, sizeof( b ) );
				b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.image = vk.color_image;
				b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				b.subresourceRange.levelCount = 1;
				b.subresourceRange.layerCount = 1;
				qvkCmdPipelineBarrier( vk.cmd->command_buffer,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, NULL, 0, NULL, 1, &b );
			}
#endif

			vk_begin_render_pass_clear1( vk.render_pass.gamma, vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ], vk.renderWidth, vk.renderHeight );
			{
				// Build gamma variant index from active post-process cvars
				int varIdx = 0;
				qboolean needsDepth = qfalse;
#if FEAT_SSAO
				if ( r_ssao->integer && vk.depthFade.active ) { varIdx |= GAMMA_VAR_SSAO; needsDepth = qtrue; }
#endif
#if FEAT_TONEMAP
				if ( r_tonemap->integer ) varIdx |= GAMMA_VAR_TONEMAP;
#endif
#if FEAT_COLOR_GRADING
				if ( r_colorGrading->integer ) varIdx |= GAMMA_VAR_CG;
#endif
#if FEAT_FXAA
				if ( r_fxaa->integer ) varIdx |= GAMMA_VAR_FXAA;
#endif
#if FEAT_GODRAYS
				if ( r_godRays->integer && vk.depthFade.active ) { varIdx |= GAMMA_VAR_GODRAYS; needsDepth = qtrue; }
#endif
				if ( varIdx && vk.gamma_variants[ varIdx ] != VK_NULL_HANDLE ) {
					VkPipelineLayout pLayout;
					if ( varIdx & GAMMA_VAR_GODRAYS )
						pLayout = vk.pipeline_layout_godrays;
					else if ( varIdx & GAMMA_VAR_SSAO )
						pLayout = vk.pipeline_layout_ssao;
					else
						pLayout = vk.pipeline_layout_post_process;

					qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.gamma_variants[ varIdx ] );
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pLayout, 0, 1, &vk.color_descriptor, 0, NULL );
					if ( needsDepth ) {
						qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pLayout, 1, 1, &vk.depthFade.descriptor, 0, NULL );
					}
#if FEAT_GODRAYS
					if ( varIdx & GAMMA_VAR_GODRAYS ) {
						float pushData[4];
						pushData[0] = r_sunX->value;  // sun screen X (0-1)
						pushData[1] = r_sunY->value;  // sun screen Y (0-1)
						pushData[2] = 0.5f;           // intensity
						pushData[3] = 0.97f;          // decay
						qvkCmdPushConstants( vk.cmd->command_buffer, pLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, pushData );
					}
#endif
				} else {
					qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.gamma_pipeline );
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );
				}
			}

			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		}
	}

	// End whatever render pass is active.
	// Skip the call when no render pass is open:
	//   (a) SMAA + FBO + minimized + no capture — SMAA already ended its pass.
	//   (b) FBO + not minimized + acquire failed — gamma pass was never started.
	if ( !( vk.fboActive && !vk.cmd->swapchain_image_acquired && !ri.CL_IsMinimized() ) &&
	     !( vk.smaa.active && vk.fboActive && ri.CL_IsMinimized() && !( backEnd.screenshotMask && vk.capture.image ) ) ) {
		vk_end_render_pass();
	}

	vk_gpu_ts_write( "present_prep" ); // must be before EndCommandBuffer; render passes are all closed above
	vk_gpu_ts_frame_end();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.cmd->command_buffer;
	if ( !ri.CL_IsMinimized() && vk.cmd->swapchain_image_acquired ) {
#ifdef USE_UPLOAD_QUEUE
		if ( vk.image_uploaded != VK_NULL_HANDLE ) {
			waits[0] = vk.cmd->image_acquired;
			waits[1] = vk.image_uploaded;
			submit_info.waitSemaphoreCount = 2;
			submit_info.pWaitSemaphores = &waits[0];
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			signals[0] = vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
			signals[1] = vk.cmd->rendering_finished2;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = &signals[0];

			vk.rendering_finished = vk.cmd->rendering_finished2;
			vk.image_uploaded = VK_NULL_HANDLE;
		} else {
			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = &vk.cmd->image_acquired;
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
		}
#else
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &vk.cmd->image_acquired;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
#endif
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
	}

	{
		int t_submit = ri.Milliseconds();
		vk_frame_t_submit_start = ri.Microseconds();
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence ) );
		vk_diag_submit_ms += ri.Milliseconds() - t_submit;
		vk_frame_t_after_submit = ri.Microseconds();
	}
	// Hand this slot's fence to the background thread: it will vkWaitForFences +
	// vkResetFences off the main thread, then signal vk_slot_ready[cmd_index].
	vk_fence_submit( vk.cmd_index, vk.cmd->rendering_finished_fence );
	vk.cmd->waitForFence = qtrue;

	// presentation may take undefined time to complete, we can't measure it in a reliable way
	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;

	vk.renderPassIndex = RENDER_PASS_MAIN;
}


void vk_present_frame( void )
{
	VkPresentInfoKHR present_info;
	VkResult res;

	if ( ri.CL_IsMinimized() || !vk.cmd->swapchain_image_acquired ) {
		return;
	}

	if ( !vk.cmd->waitForFence ) {
		// nothing has been submitted this frame due to geometry buffer overflow?
		return;
	}

	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk.swapchain;
	present_info.pImageIndices = &vk.cmd->swapchain_image_index;
	present_info.pResults = NULL;

	vk.cmd->swapchain_image_acquired = qfalse;

	{
		int t_present = ri.Milliseconds();
		vk_frame_t_present_start = ri.Microseconds();
		res = qvkQueuePresentKHR( vk.queue, &present_info );
		vk_diag_present_ms += ri.Milliseconds() - t_present;
		vk_frame_t_after_present = ri.Microseconds();
		vk_frame_present_done = qtrue;
	}
	switch ( res ) {
		case VK_SUCCESS:
			break;
		case VK_SUBOPTIMAL_KHR:
		case VK_ERROR_OUT_OF_DATE_KHR:
			// swapchain re-creation needed
			vk_restart_swapchain( __func__, res );
			return;
		case VK_ERROR_DEVICE_LOST:
			// we can ignore that
			ri.Log( SEV_DEBUG, "vkQueuePresentKHR: device lost\n" );
			break;
		default:
			// or we don't
			ri.Terminate( TERM_UNRECOVERABLE, "vkQueuePresentKHR returned %s", vk_result_string( res ) );
	}

	// pickup next command buffer for rendering
	vk.cmd_index++;
	vk.cmd_index %= NUM_COMMAND_BUFFERS;
	vk.cmd = &vk.tess[ vk.cmd_index ];

	if ( vk_frame_present_done && r_frameSpikeUs && r_frameSpikeUs->integer > 0 ) {
		int64_t total = vk_frame_t_after_present - vk_frame_t_start;
		if ( total >= (int64_t)r_frameSpikeUs->integer ) {
			ri.Log( SEV_DEBUG,
				"spike total=%lldus  fence=%lld  acquire=%lld  cb_setup=%lld  "
				"begin_passes=%lld  record=%lld  post_passes=%lld  submit=%lld  present=%lld\n",
				(long long)total,
				(long long)( vk_frame_t_after_fence    - vk_frame_t_start ),
				(long long)( vk_frame_t_after_acquire  - vk_frame_t_after_fence ),
				(long long)( vk_frame_t_after_begincb  - vk_frame_t_after_acquire ),
				(long long)( vk_frame_t_rec_start      - vk_frame_t_after_begincb ),
				(long long)( vk_frame_t_rec_end        - vk_frame_t_rec_start ),
				(long long)( vk_frame_t_submit_start   - vk_frame_t_rec_end ),
				(long long)( vk_frame_t_after_submit   - vk_frame_t_submit_start ),
				(long long)( vk_frame_t_after_present  - vk_frame_t_present_start ) );
		}
	}
}


static qboolean is_bgr( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
			return qtrue;
		default:
			return qfalse;
	}
}


void vk_read_pixels( byte *buffer, uint32_t width, uint32_t height )
{
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkMemoryRequirements memory_requirements;
	VkMemoryPropertyFlags memory_reqs;
	VkMemoryPropertyFlags memory_flags;
	VkMemoryAllocateInfo alloc_info;
	VkImageSubresource subresource;
	VkSubresourceLayout layout;
	VkImageCreateInfo desc;
	VkImage srcImage;
	VkImageLayout srcImageLayout;
	VkImage dstImage;
	byte *buffer_ptr;
	byte *data;
	uint32_t pixel_width;
	uint32_t i, n;
	qboolean invalidate_ptr;

	VK_CHECK( qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e12 ) );

	if ( vk.fboActive ) {
		if ( vk.capture.image ) {
			// dedicated capture buffer
			srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			srcImage = vk.capture.image;
		} else {
			srcImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			srcImage = vk.color_image;
		}
	} else {
		srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		srcImage = vk.swapchain_images[ vk.cmd->swapchain_image_index ];
	}

	memset( &desc, 0, sizeof( desc ) );

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = vk.capture_format;
	desc.extent.width = width;
	desc.extent.height = height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &dstImage ) );

	qvkGetImageMemoryRequirements( vk.device, dstImage, &memory_requirements );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;

	// host_cached bit is desirable for fast reads
	memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
	if ( alloc_info.memoryTypeIndex == ~0 ) {
		// try less explicit flags, without host_coherent
		memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
		if ( alloc_info.memoryTypeIndex == ~0U ) {
			// slowest case
			memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
			if ( alloc_info.memoryTypeIndex == ~0U ) {
				ri.Terminate( TERM_UNRECOVERABLE, "%s(): failed to find matching memory type for image capture", __func__ );
			}
		}
	}

	if ( memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) {
		invalidate_ptr = qfalse;
	} else {
		 // according to specification - must be performed if host_coherent is not set
		invalidate_ptr = qtrue;
	}

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));
	VK_CHECK(qvkBindImageMemory(vk.device, dstImage, memory, 0));

	command_buffer = begin_command_buffer();

	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			srcImageLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			0, 0);
	}

	record_image_layout_transition( command_buffer, dstImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	// end_command_buffer( command_buffer );

	// command_buffer = begin_command_buffer();

	if ( vk.blitEnabled ) {
		VkImageBlit region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[0].x = 0;
		region.srcOffsets[0].y = 0;
		region.srcOffsets[0].z = 0;
		region.srcOffsets[1].x = width;
		region.srcOffsets[1].y = height;
		region.srcOffsets[1].z = 1;
		region.dstSubresource = region.srcSubresource;
		region.dstOffsets[0] = region.srcOffsets[0];
		region.dstOffsets[1] = region.srcOffsets[1];

		qvkCmdBlitImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST );

	} else {
		VkImageCopy region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffset.x = 0;
		region.srcOffset.y = 0;
		region.srcOffset.z = 0;
		region.dstSubresource = region.srcSubresource;
		region.dstOffset = region.srcOffset;
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		qvkCmdCopyImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	end_command_buffer( command_buffer, __func__ );

	// Copy data from destination image to memory buffer.
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	qvkGetImageSubresourceLayout( vk.device, dstImage, &subresource, &layout );

	VK_CHECK( qvkMapMemory( vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data ) );

	if ( invalidate_ptr )
	{
		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = NULL;
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		range.offset = 0;
		qvkInvalidateMappedMemoryRanges( vk.device, 1, &range );
	}

	data += layout.offset;

	switch ( vk.capture_format ) {
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16: pixel_width = 2; break;
		case VK_FORMAT_R16G16B16A16_UNORM: pixel_width = 8; break;
		default: pixel_width = 4; break;
	}

	buffer_ptr = buffer + width * (height - 1) * 3;
	for ( i = 0; i < height; i++ ) {
		switch ( pixel_width ) {
			case 2: {
				uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = ((src[n]>>12)&0xF)<<4;
					buffer_ptr[n*3+1] = ((src[n]>>8)&0xF)<<4;
					buffer_ptr[n*3+2] = ((src[n]>>4)&0xF)<<4;
				}
			} break;

			case 4: {
				for ( n = 0; n < width; n++ ) {
					memcpy( &buffer_ptr[n*3], &data[n*4], 3 );
					//buffer_ptr[n*3+0] = data[n*4+0];
					//buffer_ptr[n*3+1] = data[n*4+1];
					//buffer_ptr[n*3+2] = data[n*4+2];
				}
			} break;

			case 8: {
				const uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = src[n*4+0]>>8;
					buffer_ptr[n*3+1] = src[n*4+1]>>8;
					buffer_ptr[n*3+2] = src[n*4+2]>>8;
				}
			} break;
		}
		buffer_ptr -= width * 3;
		data += layout.rowPitch;
	}

	if ( is_bgr( vk.capture_format ) ) {
		buffer_ptr = buffer;
		for ( i = 0; i < width * height; i++ ) {
			byte tmp = buffer_ptr[0];
			buffer_ptr[0] = buffer_ptr[2];
			buffer_ptr[2] = tmp;
			buffer_ptr += 3;
		}
	}

	qvkDestroyImage( vk.device, dstImage, NULL );
	qvkFreeMemory( vk.device, memory, NULL );

	// restore previous layout
	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		command_buffer = begin_command_buffer();

		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			srcImageLayout, 0, 0 );

		end_command_buffer( command_buffer, "restore layout" );
	}
}


#if FEAT_SHADOW_MAPPING
/*
===================
vk_render_shadow_map

Renders the scene from the light's perspective into the shadow map depth texture.
Called once per dynamic light before its lit surface pass.
===================
*/
void vk_render_shadow_map( const dlight_t *dl ) {
	VkRenderPassBeginInfo rpBegin;
	VkClearValue clearVal;
	VkViewport viewport;
	VkRect2D scissor;
	float lightMVP[16];
	float lightView[16];
	float lightProj[16];
	vec3_t lightDir, up, right;
	float radius = dl->radius;
	uint32_t mapSize = vk.shadowMap.size;

	if ( !vk.shadowMap.framebuffer || !vk.shadowMap.renderPass )
		return;

	// End the current render pass (main scene) temporarily
	vk_end_render_pass();

	// Build light view matrix — look from light toward camera
	VectorSubtract( backEnd.viewParms.or.origin, dl->origin, lightDir );
	VectorNormalize( lightDir );

	// Choose an up vector that isn't parallel to lightDir
	if ( fabs( lightDir[2] ) < 0.9f ) {
		VectorSet( up, 0, 0, 1 );
	} else {
		VectorSet( up, 1, 0, 0 );
	}
	CrossProduct( lightDir, up, right );
	VectorNormalize( right );
	CrossProduct( right, lightDir, up );
	VectorNormalize( up );

	// View matrix (look-at)
	lightView[0]  = right[0]; lightView[4]  = right[1]; lightView[8]  = right[2]; lightView[12] = -DotProduct( right, dl->origin );
	lightView[1]  = up[0];    lightView[5]  = up[1];    lightView[9]  = up[2];    lightView[13] = -DotProduct( up, dl->origin );
	lightView[2]  = lightDir[0]; lightView[6] = lightDir[1]; lightView[10] = lightDir[2]; lightView[14] = -DotProduct( lightDir, dl->origin );
	lightView[3]  = 0;        lightView[7]  = 0;        lightView[11] = 0;        lightView[15] = 1;

	// Orthographic projection covering the light's radius
	{
		float l = -radius, r2 = radius, b = -radius, t = radius;
		float n = 1.0f, f = radius * 2.0f;
		memset( lightProj, 0, sizeof( lightProj ) );
		lightProj[0]  = 2.0f / (r2 - l);
		lightProj[5]  = 2.0f / (t - b);
		lightProj[10] = -1.0f / (f - n);
		lightProj[12] = -(r2 + l) / (r2 - l);
		lightProj[13] = -(t + b) / (t - b);
		lightProj[14] = -n / (f - n);
		lightProj[15] = 1.0f;
	}

	// lightMVP = lightProj * lightView
	{
		int i, j, k;
		for ( i = 0; i < 4; i++ ) {
			for ( j = 0; j < 4; j++ ) {
				lightMVP[i + j*4] = 0;
				for ( k = 0; k < 4; k++ ) {
					lightMVP[i + j*4] += lightProj[i + k*4] * lightView[k + j*4];
				}
			}
		}
	}

	// Begin shadow render pass
	memset( &clearVal, 0, sizeof( clearVal ) );
	clearVal.depthStencil.depth = 1.0f;

	rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBegin.pNext = NULL;
	rpBegin.renderPass = vk.shadowMap.renderPass;
	rpBegin.framebuffer = vk.shadowMap.framebuffer;
	rpBegin.renderArea.offset.x = 0;
	rpBegin.renderArea.offset.y = 0;
	rpBegin.renderArea.extent.width = mapSize;
	rpBegin.renderArea.extent.height = mapSize;
	rpBegin.clearValueCount = 1;
	rpBegin.pClearValues = &clearVal;

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE );

	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)mapSize;
	viewport.height = (float)mapSize;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = mapSize;
	scissor.extent.height = mapSize;
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );

	// Push the light MVP matrix
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.shadowMap.depthLayout,
		VK_SHADER_STAGE_VERTEX_BIT, 0, 64, lightMVP );

	// TODO: render shadow-casting geometry here
	// For now, the shadow map is cleared to depth=1.0 (fully lit)
	// Full implementation would iterate dl->head litSurfs and render their geometry
	// using the shadow depth pipeline + lightMVP push constants

	qvkCmdEndRenderPass( vk.cmd->command_buffer );

	// Store the lightMVP in the uniform buffer for the lit pass to read
	// (The lit pass fragment shader uses shadowCoord = shadowMVP * position)
	// This is done by extending the UBO in VK_SetLightParams()

	// Re-enter the main render pass
	vk_begin_main_render_pass();
}
#endif // FEAT_SHADOW_MAPPING


qboolean vk_bloom( void )
{
	uint32_t i;

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
	{
		return qfalse;
	}

	if ( backEnd.doneBloom || !backEnd.doneSurfaces || !vk.fboActive )
	{
		return qfalse;
	}

	vk_end_render_pass(); // end main
	vk_gpu_ts_write( "world_done" ); // outside render pass — MoltenVK timestamps only resolve at encoder boundaries

#ifdef __APPLE__
	// MoltenVK/TBDR: subpass external deps alone don't flush tile cache on Apple Silicon.
	// Explicit barrier makes main-pass writes to color_image visible to bloom_extract's sampler.
	if ( r_vkApplePinkBarrier->integer ) {
		VkImageMemoryBarrier b;
		memset( &b, 0, sizeof( b ) );
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = vk.color_image;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.levelCount = 1;
		b.subresourceRange.layerCount = 1;
		qvkCmdPipelineBarrier( vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, NULL, 0, NULL, 1, &b );
	}
#endif

	// bloom extraction
	vk_begin_bloom_extract_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_extract_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	{
	const int num_bloom_passes = Com_Clamp( 1, VK_NUM_BLOOM_PASSES, r_bloom_passes->integer );

	for ( i = 0; i < num_bloom_passes*2; i+=2 ) {
		// horizontal blur
		vk_begin_blur_render_pass( i+0 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+0], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass( i+1 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
#if 0
		// horizontal blur
		vk_begin_blur_render_pass( i+0 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+2], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass( i+1 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
#endif
	}

	vk_begin_post_bloom_render_pass(); // begin post-bloom
	{
		VkDescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ )
		{
			// pad unused slots with the last active (most diffuse) blur level
			int src = (i < num_bloom_passes) ? i : (num_bloom_passes - 1);
			dset[i] = vk.bloom_image_descriptor[(src+1)*2];
		}

		// blend downscaled buffers to main fbo
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_blend_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_blend, 0, ARRAY_LEN(dset), dset, 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	}
	} // num_bloom_passes scope

	// invalidate pipeline state cache
	//vk.cmd->last_pipeline = VK_NULL_HANDLE;

	if ( vk.cmd->last_pipeline != VK_NULL_HANDLE )
	{
		// restore last pipeline
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.cmd->last_pipeline );

		vk_update_mvp( NULL );

		// force depth range and viewport/scissor updates
		vk.cmd->depth_range = DEPTH_RANGE_COUNT;

		// restore clobbered descriptor sets
		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ ) {
			if ( vk.cmd->descriptor_set.current[i] != VK_NULL_HANDLE ) {
				if ( i == VK_DESC_UNIFORM /*|| i == VK_DESC_STORAGE*/ )
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 1, &vk.cmd->descriptor_set.offset[i] );
				else
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 0, NULL );
			}
		}
	}

	backEnd.doneBloom = qtrue;

	return qtrue;
}
