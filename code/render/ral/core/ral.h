// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral.h — umbrella header for the Wired Renderer Abstraction Layer.
//
// The RAL is the cross-API GPU surface described in
// docs/phase-7-ral-design.md. Renderer code talks to GPU work exclusively
// through Ral_* functions and ral*_t types; backend implementations
// (Vulkan, Metal, OpenGL 4.6 Core and WebGPU) live under
// code/render/ral/backends/<backend>/ and never leak native types upward.
//
//   Layering:  game / cgame
//                  ↓
//              renderer (tr_*) ─ stays imperative; owns scene + materials
//                  ↓
//              [ frame graph — optional ]
//                  ↓
//              RAL  ← this header
//                  ↓
//              backends/{vulkan,metal,opengl,webgpu}
//
// The v1 API surface (this header's contents) is frozen by §3 of the design
// doc. Later work adds capabilities (VRS, streaming hooks, …) but does not
// break what is here.
//
// Include this single header; it pulls in the per-area headers below. RAL is a
// mandatory layer of the Vulkan renderer; it is not an optional feature flag.
//
// ── Swapchain coexistence during the migration ────────
// The renderer migration onto the RAL starts while the swapchain
// (VkSurfaceKHR + VkSwapchainKHR + present queue) still lives in
// code/render/ral/backends/vulkan/renderer/vk.c — `ral_swapchain.h` is reserved for the real RAL
// swapchain API that lands later. Until then the two layers
// coexist via the **decoupled** model (option α from the planning
// doc):
//
//   1. The RAL renderer renders into an offscreen RAL-owned texture
//      (`ralTexture_t` with RAL_TEXTURE_USAGE_COLOR_ATTACHMENT).
//   2. At present time the legacy code/render/ral/backends/vulkan/renderer/ swapchain path blits /
//      copies that RAL texture's `VkImage` into the acquired swapchain
//      image and submits `vkQueuePresentKHR` as before.
//
// Why α over option β (RAL imports the qvk VkImage via a new
// Ral_ImportTexture path): α keeps the RAL backend's VkInstance / VkDevice
// genuinely independent of `code/render/ral/backends/vulkan/renderer/`'s — they're already separate
// objects, so cross-instance image sharing would need VK_KHR_external_memory
// plumbing that adds non-trivial setup for a model we're replacing later
// anyway. α also lets either side render-target-format/colorspace transcode
// during the blit, which is useful while HDR / scRGB / HDR10 paths are
// being validated end-to-end. The handoff cost is one full-screen blit per
// frame — a single small price during the coexistence window.
//
// A later change replaces this with the real RAL swapchain surface
// (ral_swapchain.h) + present queue ownership; option β / external-memory
// considerations disappear at that point.

#ifndef WIRED_RAL_H
#define WIRED_RAL_H

#define RAL_API_VERSION 1

#include "ral_types.h"      // result codes, opaque handles, enums, small structs
#include "ral_backend.h"    // backend lifecycle, caps, memory budget, availability probe
#include "ral_capability.h" // versioned backend-neutral support/fallback profile
#include "ral_backend_conformance.h" // live cross-backend ownership/copy/recreate proof
#include "ral_allocation.h" // generation-bound logical placement and pressure receipt
#include "ral_memory_recovery.h" // bounded OOM/fragmentation/device-loss decisions
#include "ral_suballocation.h" // deterministic aligned block slices and coalescing
#include "ral_transient.h" // bounded alias planning and fence/cancel lifecycle
#include "ral_transient_texture.h" // candidate-first physical transient texture cohorts
#include "ral_transfer.h" // generation-bound upload/readback authority
#include "ral_readback.h" // generation-bound staging/submission/map owner
#include "ral_buffer_map.h" // typed READ/WRITE map tickets; Vulkan-ready/WebGPU-pending lifecycle
#include "ral_command_lifecycle.h" // generation-bound encoder/submit authority
#include "ral_resource.h"   // buffers, textures, samplers, bind groups
#include "ral_transition.h" // portable resource usages, ranges and logical queue transitions
#include "ral_shader_abi.h" // versioned reflection/artifact schema and semantic keys
#include "ral_legacy_material.h" // backend-neutral legacy material variants/fallbacks
#include "ral_pipeline.h"   // graphics / compute pipelines, pipeline cache
#include "ral_command.h"    // command buffers, submission, dynamic rendering, draw/dispatch/copy/barrier
#include "ral_sync.h"       // fences, semaphores (binary + timeline)
#include "ral_swapchain.h"  // presentation surface, HDR metadata
#include "ral_query.h"      // query pools (timestamps, occlusion, pipeline stats)
#include "ral_residency.h"  // backend-neutral page lifecycle, ranking and budgets
#include "ral_profile.h"    // semantic GPU timing accumulation + topology epochs

#endif // WIRED_RAL_H
