// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* The Vulkan layer's view of the renderer: the backend-neutral renderer header
 * (code/render/renderer/tr_local.h, which names no Vulkan type) plus the Vulkan
 * instance and the Vulkan-typed entry points. Files under this directory include
 * this header; the neutral renderer files include tr_local.h and therefore cannot
 * reach a Vulkan type at all (the compiler enforces it, TASK-231). */

#ifndef VK_LOCAL_H
#define VK_LOCAL_H

#include "../../../../renderer/tr_local.h"
#include "vk.h"

/* Legacy GL spellings the Vulkan-side code still uses; the neutral renderer headers
 * spell the same types as plain C types. */
#define GLint				int
#define GLuint				unsigned int
#define GLboolean			uint32_t

/* The neutral header spells three Vulkan types without including Vulkan; these keep
 * the spellings honest. */
_Static_assert( sizeof( trSamplerAddressMode_t ) == sizeof( VkSamplerAddressMode ), "sampler address mode size" );
_Static_assert( sizeof( trDescriptorSetHandle_t ) == sizeof( VkDescriptorSet ), "descriptor set handle size" );

extern Vk_Instance	vk;				// shouldn't be cleared during ref re-init
extern Vk_World		vk_world;		// this data is cleared during ref re-init

qboolean vk_particle_shadow_get_class( uint32_t classIndex,
	const particleClassGPU_t **outClass );
qboolean vk_particle_shadow_write_spawn( uint32_t requestIndex,
	const particleSpawnGPU_t *request );
qboolean vk_particle_shadow_write_atmosphere_profile( uint32_t profileIndex,
	const atmosphereEffectProfile_t *profile );
qboolean vk_particle_shadow_write_class( uint32_t classIndex,
	const particleClassGPU_t *particleClass );

// vk.c — write one slot of the projector's decal-texture sampler array
// (binding 2 of the decal render descriptor set) on every per-frame descriptor
// set. Called from RE_AddDecalToScene's find-or-add registry when a new decal
// shader claims a slot. Publication rebuilds the current immutable RAL group.
void vk_decal_set_texture_image( int slot, struct image_s *image );
qboolean vk_decal_flush_pending_images( void );
qboolean vk_decal_shadow_read( uint32_t slot, decalGPU_t *decal );
qboolean vk_decal_shadow_write( uint32_t slot, const decalGPU_t *decal );

/* DDS loader: reports the Vulkan format of the decoded data (moved out of the neutral tr_common.h). */
void R_LoadDDS( const char *name, byte **pic, int *width, int *height, VkFormat *picFormat, int *numMips, int *dataSize, ddsImageInfo_t *info );

#endif
