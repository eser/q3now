// SPDX-License-Identifier: GPL-3.0-or-later

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

static int failures;
static VkFormatFeatureFlags fakeFeatures;
static VkFormat fakeLastFormat;

qboolean ralVk_HasExtension( const VkExtensionProperties *exts,
		uint32_t count, const char *name ) {
	(void)exts; (void)count; (void)name; return qfalse;
}

static VKAPI_ATTR VkResult VKAPI_CALL FakeEnumerateDeviceExtensions(
		VkPhysicalDevice physicalDevice, const char *layerName,
		uint32_t *count, VkExtensionProperties *properties ) {
	(void)physicalDevice; (void)layerName; (void)properties; *count = 0; return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL FakeEnumerateInstanceExtensions(
		const char *layerName, uint32_t *count, VkExtensionProperties *properties ) {
	(void)layerName; (void)properties; *count = 0; return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL FakeGetPhysicalDeviceFeatures2(
		VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *features ) {
	(void)physicalDevice; (void)features;
}

static VKAPI_ATTR void VKAPI_CALL FakeGetPhysicalDeviceProperties2(
		VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2 *properties ) {
	(void)physicalDevice; (void)properties;
}

#define CHECK(expr) do { \
	if ( !( expr ) ) { \
		fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
		failures++; \
	} \
} while ( 0 )

static VKAPI_ATTR void VKAPI_CALL FakeGetPhysicalDeviceFormatProperties(
	VkPhysicalDevice physicalDevice, VkFormat format,
	VkFormatProperties *properties ) {
	(void)physicalDevice;
	fakeLastFormat = format;
	memset( properties, 0, sizeof( *properties ) );
	properties->optimalTilingFeatures = fakeFeatures;
}

static void TestWriteMask( void ) {
	ralColorBlendAttachment_t blend;
	memset( &blend, 0, sizeof( blend ) );
	CHECK( ralVk_ColorWriteMask( NULL ) == RAL_COLOR_WRITE_ALL );
	CHECK( ralVk_ColorWriteMask( &blend ) == RAL_COLOR_WRITE_ALL );
	blend.writeMask = RAL_COLOR_WRITE_R | RAL_COLOR_WRITE_G;
	CHECK( ralVk_ColorWriteMask( &blend ) == ( RAL_COLOR_WRITE_R | RAL_COLOR_WRITE_G ) );
	blend.writeMask = 0;
	blend.writeMaskExplicit = qtrue;
	CHECK( ralVk_ColorWriteMask( &blend ) == 0 );
}

static void TestIndependentBlend( void ) {
	CHECK( ralVk_IndependentBlendEnabled( qtrue, qtrue, VK_TRUE ) );
	CHECK( !ralVk_IndependentBlendEnabled( qtrue, qfalse, VK_TRUE ) );
	CHECK( !ralVk_IndependentBlendEnabled( qtrue, qtrue, VK_FALSE ) );
	// Imported/adopted device: support is not proof that the feature was enabled.
	CHECK( !ralVk_IndependentBlendEnabled( qfalse, qtrue, VK_TRUE ) );
}

static void TestPipelineBlendGate( void ) {
	ralGraphicsPipelineCreateInfo_t ci;
	ralColorBlendAttachment_t blends[3];
	memset( &ci, 0, sizeof( ci ) );
	memset( blends, 0, sizeof( blends ) );
	ci.numColorFormats = 3;
	ci.colorBlends = blends;
	ci.numColorBlends = 3;

	// Three legacy-zero entries all materialize as no-blend/write-all.
	CHECK( ralVk_ColorBlendStatesSupported( &ci, qfalse ) );
	blends[1].writeMask = RAL_COLOR_WRITE_ALL;
	CHECK( ralVk_ColorBlendStatesSupported( &ci, qfalse ) );
	blends[1].writeMask = 0;
	blends[1].writeMaskExplicit = qtrue;
	CHECK( !ralVk_ColorBlendStatesSupported( &ci, qfalse ) );
	CHECK( ralVk_ColorBlendStatesSupported( &ci, qtrue ) );

	blends[1].writeMaskExplicit = qfalse;
	blends[1].blendEnable = qtrue;
	CHECK( !ralVk_ColorBlendStatesSupported( &ci, qfalse ) );
	blends[1] = blends[0];
	blends[1].srcColor = RAL_BLEND_ONE;
	CHECK( !ralVk_ColorBlendStatesSupported( &ci, qfalse ) );

	// An omitted entry uses the exact same no-blend/write-all default.
	memset( blends, 0, sizeof( blends ) );
	ci.numColorBlends = 1;
	CHECK( ralVk_ColorBlendStatesSupported( &ci, qfalse ) );
	blends[0].writeMaskExplicit = qtrue;
	CHECK( !ralVk_ColorBlendStatesSupported( &ci, qfalse ) );

	ci.numColorFormats = 1;
	CHECK( ralVk_ColorBlendStatesSupported( &ci, qfalse ) );
	ci.numColorBlends = 1;
	ci.colorBlends = NULL;
	CHECK( !ralVk_ColorBlendStatesSupported( &ci, qtrue ) );
	CHECK( !ralVk_ColorBlendStatesSupported( NULL, qtrue ) );
}

static void TestFormatUsage( void ) {
	ralBackend_t backend;
	const ralTextureUsage_t requested = (ralTextureUsage_t)( RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
	                                                        | RAL_TEXTURE_USAGE_TRANSFER_SRC );
	const VkImageUsageFlags actual = ralVk_TextureUsage( requested );
	const VkFormatFeatureFlags required = ralVk_TextureUsageFormatFeatures( requested );
	static const ralFormat_t motionFormats[] = {
		RAL_FORMAT_R16G16_SFLOAT,
		RAL_FORMAT_R8_UNORM
	};
	static const VkFormatFeatureFlags motionRequiredBits[] = {
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
		VK_FORMAT_FEATURE_TRANSFER_SRC_BIT,
		VK_FORMAT_FEATURE_TRANSFER_DST_BIT,
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
	};
	uint32_t i, j;

	CHECK( ( actual & VK_IMAGE_USAGE_SAMPLED_BIT ) != 0 );
	CHECK( ( actual & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) != 0 );
	CHECK( ( actual & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) != 0 );
	CHECK( ( actual & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) != 0 );
	CHECK( ( required & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT ) != 0 );
	CHECK( ( required & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT ) != 0 );
	CHECK( ( required & VK_FORMAT_FEATURE_TRANSFER_DST_BIT ) != 0 );
	CHECK( ( required & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT ) != 0 );

	memset( &backend, 0, sizeof( backend ) );
	backend.physicalDevice = (VkPhysicalDevice)(uintptr_t)1;
	backend.vk.GetPhysicalDeviceFormatProperties = FakeGetPhysicalDeviceFormatProperties;
	for ( i = 0; i < sizeof( motionFormats ) / sizeof( motionFormats[0] ); ++i ) {
		fakeFeatures = required;
		fakeLastFormat = VK_FORMAT_UNDEFINED;
		CHECK( Ral_TextureFormatSupports( &backend, motionFormats[i], requested ) );
		CHECK( fakeLastFormat == ralVk_TranslateFormat( motionFormats[i] ) );
		// Every requested or implicit requirement is independently authoritative.
		for ( j = 0; j < sizeof( motionRequiredBits ) / sizeof( motionRequiredBits[0] ); ++j ) {
			fakeFeatures = required & ~motionRequiredBits[j];
			CHECK( !Ral_TextureFormatSupports( &backend, motionFormats[i], requested ) );
		}
	}

	// The public API is general: storage and depth/stencil attachment usage
	// carry their own feature requirements in addition to the implicit trio.
	{
		const ralTextureUsage_t storageUsage = RAL_TEXTURE_USAGE_STORAGE;
		const VkFormatFeatureFlags storageRequired = ralVk_TextureUsageFormatFeatures( storageUsage );
		fakeFeatures = storageRequired;
		CHECK( Ral_TextureFormatSupports( &backend, RAL_FORMAT_R16G16_SFLOAT, storageUsage ) );
		fakeFeatures = storageRequired & ~VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
		CHECK( !Ral_TextureFormatSupports( &backend, RAL_FORMAT_R16G16_SFLOAT, storageUsage ) );
	}
	{
		const ralTextureUsage_t depthUsage = RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT;
		const VkFormatFeatureFlags depthRequired = ralVk_TextureUsageFormatFeatures( depthUsage );
		fakeFeatures = depthRequired;
		CHECK( Ral_TextureFormatSupports( &backend, RAL_FORMAT_D32_SFLOAT, depthUsage ) );
		fakeFeatures = depthRequired & ~VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
		CHECK( !Ral_TextureFormatSupports( &backend, RAL_FORMAT_D32_SFLOAT, depthUsage ) );
	}

	CHECK( !Ral_TextureFormatSupports( NULL, RAL_FORMAT_R8_UNORM, requested ) );
	CHECK( !Ral_TextureFormatSupports( &backend, RAL_FORMAT_UNDEFINED, requested ) );
	CHECK( !Ral_TextureFormatSupports( &backend, RAL_FORMAT_R8_UNORM,
		(ralTextureUsage_t)( 1u << 30 ) ) );
	backend.vk.GetPhysicalDeviceFormatProperties = NULL;
	CHECK( !Ral_TextureFormatSupports( &backend, RAL_FORMAT_R8_UNORM, requested ) );
}

static void TestStorageRangeCap( void ) {
	ralBackend_t backend;
	memset( &backend, 0, sizeof( backend ) );
	backend.physProps.apiVersion = VK_API_VERSION_1_3;
	backend.physProps.limits.maxStorageBufferRange = UINT32_MAX;
	backend.physProps.limits.maxPerStageDescriptorSampledImages = 1;
	backend.vk.EnumerateDeviceExtensionProperties = FakeEnumerateDeviceExtensions;
	backend.vk.EnumerateInstanceExtensionProperties = FakeEnumerateInstanceExtensions;
	backend.vk.GetPhysicalDeviceFeatures2 = FakeGetPhysicalDeviceFeatures2;
	backend.vk.GetPhysicalDeviceProperties2 = FakeGetPhysicalDeviceProperties2;
	ralVk_FillCaps( &backend );
	CHECK( backend.caps.maxStorageBufferRange == (uint64_t)UINT32_MAX );
	CHECK( offsetof( ralCaps_t, maxStorageBufferRange )
		> offsetof( ralCaps_t, independentBlend ) );
	CHECK( offsetof( ralCaps_t, maxStorageBufferRange )
		+ sizeof( backend.caps.maxStorageBufferRange ) == sizeof( ralCaps_t ) );
}

int main( void ) {
	TestWriteMask();
	TestIndependentBlend();
	TestPipelineBlendGate();
	TestFormatUsage();
	TestStorageRangeCap();
	if ( failures ) return 1;
	puts( "PASS RAL MRT write-mask, independent-blend and exact format-usage contract" );
	return 0;
}
