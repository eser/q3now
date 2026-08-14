// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_PROFILE_IMGUI_RAL_H
#define WIRED_PROFILE_IMGUI_RAL_H

#include <stdint.h>

#include "ral_types.h"

struct ImDrawData;
struct ImGuiContext;

typedef struct wiredProfileImGuiRalRenderer wiredProfileImGuiRalRenderer_t;

typedef struct {
	uint32_t commandLists;
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t drawCalls;
	uint32_t scissors;
	uint32_t textureBinds;
	uint32_t fontWidth;
	uint32_t fontHeight;
	uint64_t fontTextureId;
	uint64_t vertexCapacityBytes;
	uint64_t indexCapacityBytes;
} wiredProfileImGuiRalReceipt_t;

// Creates the tool-only renderer and uploads the current context's RGBA32 font
// atlas. The renderer does not own `backend` or `context`. On failure `out`
// is untouched. Only ImGuiBackendFlags_RendererHasVtxOffset is advertised;
// the ImGui 1.92 dynamic texture protocol is intentionally not claimed.
int WiredProfileImGuiRal_Create( ralBackend_t *backend, ralFormat_t colorFormat,
		ImGuiContext *context, wiredProfileImGuiRalRenderer_t **out );

// Rebuilds only the format-dependent graphics pipeline. On failure the old
// pipeline and format remain usable.
int WiredProfileImGuiRal_EnsureColorFormat(
		wiredProfileImGuiRalRenderer_t *renderer, ralFormat_t colorFormat );

// Adds a caller-owned texture view/sampler pair to the backend's numeric
// ImTextureID table. The returned bind group is renderer-owned; the view and
// sampler and the view's underlying texture must belong to the renderer's
// backend and remain live until Unregister. The caller must keep the texture
// in a shader-readable layout while it is drawn. Font-atlas ownership stays
// with the renderer. Unknown/stale IDs are rejected before command recording.
int WiredProfileImGuiRal_RegisterTexture( wiredProfileImGuiRalRenderer_t *renderer,
		ralTextureView_t *view, ralSampler_t *sampler, uint64_t *outTextureId );
int WiredProfileImGuiRal_UnregisterTexture( wiredProfileImGuiRalRenderer_t *renderer,
		uint64_t textureId );

// Records ImGui indexed draws inside an already-active RAL rendering scope.
// The entire draw stream is validated before buffers are grown/mapped or any
// command is emitted. Invalid input and allocation failures leave `receipt`
// untouched. Texture IDs must be the font atlas ID or a live registered ID.
// The renderer owns one streaming vertex/index-buffer pair: every prior GPU
// use of this renderer must be complete before the next Record call. The
// standalone host enforces this with its per-frame fence and queue-idle path.
int WiredProfileImGuiRal_Record( wiredProfileImGuiRalRenderer_t *renderer,
		const ImDrawData *drawData, ralCommandBuffer_t *commandBuffer,
		uint32_t framebufferWidth, uint32_t framebufferHeight,
		wiredProfileImGuiRalReceipt_t *receipt );

void WiredProfileImGuiRal_Destroy( wiredProfileImGuiRalRenderer_t *renderer );

#endif // WIRED_PROFILE_IMGUI_RAL_H
