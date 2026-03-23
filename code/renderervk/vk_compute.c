/*
===========================================================================
vk_compute.c — Vulkan compute shader infrastructure (documentation + stubs)

The compute pipeline init/shutdown code lives in vk.c alongside all other
pipeline and descriptor code (which requires access to static qvk* function
pointers). This file provides the architectural documentation and any
exported helper functions that don't need direct Vulkan access.

Architecture:
  ┌────────────────────────────────────────────────────────────┐
  │ vk_begin_frame()                                           │
  │   ↓                                                        │
  │ vk_compute_dispatch_rail_trails()  ← compute phase         │
  │   ├─ bind compute pipeline                                 │
  │   ├─ bind descriptor set (per-frame output SSBO)           │
  │   ├─ vkCmdDispatch(numSegments/64, 1, 1)                  │
  │   └─ pipeline barrier (COMPUTE_WRITE → VERTEX_READ)        │
  │   ↓                                                        │
  │ vk_begin_main_render_pass()                                │
  │   ↓                                                        │
  │ ... normal scene rendering ...                             │
  │   ↓                                                        │
  │ vk_end_frame()                                             │
  └────────────────────────────────────────────────────────────┘

SSBO layout (per-frame, 2 buffers for 2 frames in flight):
  binding 0: input params (HOST_VISIBLE, CPU-mapped)
    - trail start, end, beamAxis, perpAxis[36]
    - frac, radius, spacing, width, color
    - numSegments
  binding 1: output vertices (DEVICE_LOCAL in release, HOST_VISIBLE in debug)
    - MAX_RAIL_SEGMENTS * 4 * sizeof(GPUVertex)
    - Written by compute shader, read by vertex shader

Key implementation files:
  vk.c          — vk_init_compute(), vk_shutdown_compute(), pipeline creation
  vk.h          — vk_t compute fields (set_layout_compute, pipeline_layout_compute, rail.*)
  tr_surface.c  — RB_DrawRailTrailGPU() graphics draw call
  tr_local.h    — function declarations

===========================================================================
*/

// This file intentionally contains only documentation.
// All compute infrastructure code is in vk.c (access to static qvk* pointers).
// See vk_init_compute() and vk_shutdown_compute() in vk.c.
