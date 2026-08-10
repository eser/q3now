# FU-10 — 4K + ultrawide capture fallback (Phase 3e)

Per spec, the canonical 4-resolution multimodal acceptance set is
`{1280×720, 1920×1080, 3840×2160 (4K), 3440×1440 (ultrawide)}`. The
Phase 3e investigation requested 4K and ultrawide captures via the
existing window-creation path (`r_mode -1 +set r_customwidth N +set
r_customheight M +set r_fullscreen 0`).

**Result:** the engine creates an SDL window at the requested
`(3846 × 2189)` for 4K (logged: "...created window@3,22 (3846x2189)"),
but the OS clips the actual client area to the display's available
pixels. The screenshot capture path reads the live framebuffer / swap
chain, so the resulting PNG matches the display's clipped area
(`2574×1431` on this developer's display — Windows-DPI-scaled
`2560×1440` monitor). The same fallback fires for ultrawide
`3440×1440`.

The engine has no offscreen / headless render path that would
populate a swap chain at a larger-than-display resolution. Adding one
would touch `sdl_glimp.c` window creation, the Vulkan swapchain
selection (`vk.c`), and the screenshot framebuffer-readback path
(`tr_screenshot.c`) — out of scope for Phase 3e.

**Forward to Phase 3f:** FU-10b "Offscreen render path for screenshot
capture at arbitrary resolution." Likely shape: a `+screenshotAt W H`
command that allocates a sized Vulkan render target, executes one
frame at that size, reads back, and tears down — independent of the
display window.

**Phase 3e canonical 4-resolution disposition:** the Phase 3d gate
already captured at `{1280×720, 1600×900, 1920×1080, 2560×1440}`. The
top end of that set (`2560×1440`) matches this developer's display max
and is the practical proxy for the spec's 4K target until FU-10b
lands. Phase 3e re-runs the canonical 4-resolution multimodal at:

  - `1280×720`     — spec
  - `1920×1080`    — spec
  - `2560×1440`    — display-max proxy for 4K (cited)
  - `1600×900`     — secondary aspect-coverage anchor (Phase 3d retained)

All four captured at native pixel resolution; `2574×1431` (the OS-
clipped 4K attempt) intentionally NOT retained — it is the same
viewport content as `2560×1440` plus a few display-decoration pixels
and would be misleading filed under the 4K name.
