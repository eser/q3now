// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static unsigned begins, barriers, ends, layerCount;
static VkRenderingAttachmentInfo captured[RAL_MAX_COLOR_ATTACHMENTS];
static VKAPI_ATTR void VKAPI_CALL Begin(VkCommandBuffer cb, const VkRenderingInfo *info) {
	(void)cb;
	++begins;
	layerCount = info->layerCount;
	memcpy(captured, info->pColorAttachments, info->colorAttachmentCount * sizeof(captured[0]));
}
static VKAPI_ATTR void VKAPI_CALL End(VkCommandBuffer cb) { (void)cb; ++ends; }
static VKAPI_ATTR void VKAPI_CALL Barrier(VkCommandBuffer cb, VkPipelineStageFlags src,
	VkPipelineStageFlags dst, VkDependencyFlags flags, uint32_t nm, const VkMemoryBarrier *m,
	uint32_t nb, const VkBufferMemoryBarrier *b, uint32_t ni, const VkImageMemoryBarrier *i) {
	(void)cb; (void)src; (void)dst; (void)flags; (void)nm; (void)m;
	(void)nb; (void)b; (void)ni; (void)i; ++barriers;
}
int main(void) {
	ralBackend_t backend = {0};
	ralCommandBuffer_t command = {0};
	ralTexture_t cube = {0}, single, resolve;
	ralRenderingInfo_t pass = {0}, bad;
	VkImageView views[6], resolveViews[6];
	unsigned i;
	backend.type = RAL_BACKEND_VULKAN;
	backend.vk.CmdBeginRendering = Begin;
	backend.vk.CmdEndRendering = End;
	backend.vk.CmdPipelineBarrier = Barrier;
	command.backend = &backend;
	command.cb = (VkCommandBuffer)(uintptr_t)1;
	command.queue = RAL_QUEUE_GRAPHICS;
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	cube.backend = &backend;
	cube.image = (VkImage)(uintptr_t)2;
	cube.defaultView = (VkImageView)(uintptr_t)3;
	cube.type = RAL_TEXTURE_CUBE;
	cube.arrayLayers = cube.depthOrArrayLayers = 6;
	cube.mipLevels = cube.sampleCount = 1;
	cube.width = cube.height = 16;
	cube.ralFormat = RAL_FORMAT_R8G8B8A8_UNORM;
	cube.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
	cube.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	cube.usage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	for (i = 0; i < 6; ++i) {
		views[i] = (VkImageView)(uintptr_t)(10 + i);
		resolveViews[i] = (VkImageView)(uintptr_t)(20 + i);
	}
	cube.layerViews = views; cube.numLayerViews = 6;
	resolve = cube; resolve.layerViews = resolveViews;
	single = cube; single.type = RAL_TEXTURE_2D;
	single.arrayLayers = single.depthOrArrayLayers = 1;
	single.layerViews = NULL; single.numLayerViews = 0;
	pass.numColorAttachments = 2;
	pass.colorAttachments[0] = &cube;
	pass.colorAttachments[1] = &single;
	pass.resolveAttachments[0] = &resolve;
	pass.renderArea.width = pass.renderArea.height = 16;
	for (i = 0; i < 6; ++i) {
		pass.colorAttachmentLayerIndices[0] = i;
		pass.resolveAttachmentLayerIndices[0] = 5 - i;
		Ral_BeginRendering(&command, &pass);
		CHECK(begins == i + 1 && command.renderingActive && layerCount == 1);
		CHECK(captured[0].imageView == views[i]);
		CHECK(captured[0].resolveImageView == resolveViews[5 - i]);
		CHECK(captured[1].imageView == single.defaultView);
		Ral_EndRendering(&command);
	}
	CHECK(ends == 6);
	pass.colorAttachments[1] = &resolve;
	pass.colorAttachmentLayerIndices[1] = 3;
	Ral_BeginRendering(&command, &pass);
	CHECK(captured[0].imageView == views[5] && captured[1].imageView == resolveViews[3]);
	Ral_EndRendering(&command);
	pass.colorAttachments[1] = &single;
	pass.colorAttachmentLayerIndices[1] = 0;
#define REJECT(change) do { \
	bad = pass; change; begins = barriers = 0; \
	cube.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED; \
	Ral_BeginRendering(&command, &bad); \
	CHECK(!begins && !barriers && !command.renderingActive); \
	CHECK(cube.currentLayout == VK_IMAGE_LAYOUT_UNDEFINED); \
} while (0)
	REJECT(bad.colorAttachmentLayerIndices[0] = 6);
	REJECT(bad.resolveAttachmentLayerIndices[0] = 6);
	REJECT(bad.colorAttachmentLayerIndices[1] = 1);
	REJECT(bad.colorAttachments[0] = NULL);
	views[5] = VK_NULL_HANDLE;
	REJECT((void)0);
	views[5] = (VkImageView)(uintptr_t)15;
	cube.layerViews = NULL; cube.numLayerViews = 0;
	REJECT(bad.colorAttachmentLayerIndices[0] = 0);
#undef REJECT
	puts("Vulkan color/resolve layer selection: PASS");
	return 0;
}
