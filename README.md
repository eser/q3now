# q3now

[![build](../../workflows/build/badge.svg)](../../actions?query=workflow%3Abuild)

A modern fork of id Software's Quake III Arena engine (idTech 3).

## q3now vs Quake III Arena

|  | Quake III Arena (1999) | q3now (2026) |
|---|---|---|
| **Packages** | Legacy pk3 | SW3Z archives with LZ4 compression |
| **Renderer** | OpenGL 1.1 | Vulkan (+ OpenGL fallback) |
| **VM System** | Legacy QVM | WASM via WAMR (+ legacy fallback) |
| **Gameplay** | Vanilla Quake 3 | q3now competitive gameplay |

## Gameplay Features

### Game Modes

- **King of the Hill (KOTH)** — custom game type: control the marked zone to
  score
- **Last Man Standing (LMS)** — survival game type with limited lives

### Movement

- **Pro physics** — thanks to ancient knowledge of QuakeWorld, QWFix, Promode,
  CPMA, etc. adjusted physics for providing best competitive match experience
- **Wall jumps** — strafe into a wall to gain a vertical boost

### Combat

- **Weapon rebalancing** — plasma, rocket, grenade, machinegun, shotgun all
  tuned
- **Instagib mode** — `g_instagib 1` for one-shot railgun kills, players spawn
  with railgun only
- **Excessive mode** — over-the-top weapon damage and fire rates for chaotic fun
- **New armor system** — health/armor caps rebalanced; armor shards replaced
  with +5 health pickups
- **Spawn Protection** — attacker gets no points for spawnkills

### Weapons

- **Grappling hook** — bind `+button5` to fire the hook
- **Nailgun replaced** by plasma rifle; chaingun removed
- **Proximity launcher and BFG removed** — cleaner weapon roster
- **Lightning gun discharge** — touch enemies to discharge the LG
- **Teleporting missiles** — missiles can pass through teleportation devices

### HUD / UI

- **Colored player names** — extended 17-color palette (A–P codes)
- **Third-person mode** — allows third-person view
- **Weather conditions** — Rain, snow, clear weather modes
- **Lens flares** — Lens flares on missiles and light areas
- **Detailed scoreboards** — Keeps the weapon-specific stats

## Engine

Built on [Quake3e](https://github.com/ec-/Quake3e), with significant additions:

- optimized OpenGL renderer
- optimized Vulkan renderer
- raw mouse input support, enabled automatically instead of
  DirectInput(**\in_mouse 1**) if available
- unlagged mouse events processing, can be reverted by setting **\in_lagged 1**
- **\in_minimize** - hotkey for minimize/restore main window (win32-only, direct
  replacement for Q3Minimizer)
- **\video-pipe** - to use external ffmpeg binary as an encoder for better
  quality and smaller output files
- significantly reworked VM system (WASM via WAMR, legacy QVM fallback)
- game modules run as `.wasm` with auto-detect fallback ([details](WASM.md))
- improved server-side DoS protection, much reduced memory usage
- raised filesystem limits (up to 20,000 maps can be handled in a single
  directory)
- reworked Zone memory allocator, no more out-of-memory errors
- SDL3 backend (video, audio, input) with gamepad/controller support and Wayland
  preference on Linux
- audio device selection via `\s_sdlDevice` cvar (lists available playback
  devices at startup)
- SDL version printed at engine startup for diagnostics
- tons of bug fixes and other improvements

## Wired UI Menu Format (.wmenu/.whud)

q3now uses a normalized menu format (`.wmenu` for menus, `.whud` for HUD definitions) alongside legacy Q3 `.menu`/`.hud` files.

### Key differences from legacy .menu
- **Coordinates**: 0.0-1.0 normalized (not 640x480 pixels)
- **Font sizes**: Point-based (e.g., `font "oxanium" 12`) instead of textscale fractions
- **Anchor system**: `anchor TOP_LEFT` through `BOTTOM_RIGHT` for responsive positioning
- **Text offset**: `textoffset 0.01 0.005` (normalized) replaces textalignx/textaligny
- **Hover states**: Per-item `mouseEnter`/`mouseExit` for visual feedback
- **Macros**: `#include "ui/wmenumacros.h"` provides `WBUTTON`, `WLABEL`, `WYESNO`, `WSLIDER`, `WMULTI`, `WBIND`, `WEDITFIELD`, `WSUBWINDOW`, `WSECTION_HEADER`, `WFULLSCREEN_BACKGROUND`, `WBACKGROUND_GRID`

### Legacy compatibility
Legacy `.menu`/`.hud` files are automatically converted at load time through a shim (coordinates divided by 640/480, textscale multiplied by 48). Third-party and Team Arena menu files continue to work without modification.

### MSDF Fonts
Three MSDF fonts are available:
- **Sansman** (`"sansman"`) — Display/heading font
- **Oxanium** (`"oxanium"`) — UI labels and body text
- **Share Tech Mono** (`"console"`) — Console and monospace text

Font atlases are generated at build time via `tools/msdf/generate_atlases.sh`. See `tools/msdf/fonts/README.md` for licensing.

## Audio backend

q3now uses [miniaudio](https://github.com/mackron/miniaudio) (single-header, vendored at
`code/client/miniaudio.h`) as its single cross-platform audio output backend, replacing
the legacy `win_snd.c` (DirectSound), `linux_snd.c` (ALSA/OSS), and `sdl_snd.c` (SDL2 Audio)
that were inherited from the Quake3e fork.

### Supported platforms

- **Windows**: WASAPI (loaded dynamically by miniaudio)
- **macOS**: CoreAudio + AudioToolbox
- **Linux**: PulseAudio with ALSA fallback (PipeWire works through PulseAudio compat)

### Cvars

In addition to the existing `s_volume`, `s_musicvolume`, `s_separation`, `s_mixahead`,
and `s_khz`, three new cvars expose miniaudio configuration:

| Cvar | Default | Range | Purpose |
|---|---|---|---|
| `s_device` | `""` (system default) | any device name | Override audio output device. Empty string uses the system default. The Wired UI audio panel populates this from a live device dropdown. |
| `s_latency` | `6` | `2`–`20` ms | Period size hint in milliseconds. Lower = lower latency, higher = more underrun headroom. Requires `snd_restart` to take effect. |
| `s_underruns` | `0` | read-only counter | Increments when the audio callback runs out of painted data. Useful for diagnosing audio stuttering. |

### Architecture

The miniaudio backend follows the existing engine mixer's `SNDDMA_*` interface and stays
strictly lock-free in the audio callback (verified by `tools/check_audio_callback.sh`).
The mixer (`snd_mix.c`) and spatial audio model (`S_SpatializeOrigin`) are unchanged from
their classic id Software / Quake3e form.

The dedicated server build excludes the entire audio path via `#ifndef DEDICATED`.

## Vulkan renderer

Based on
[Quake-III-Arena-Kenny-Edition](https://github.com/kennyalive/Quake-III-Arena-Kenny-Edition)
with many additions:

- high-quality per-pixel dynamic lighting
- very fast flares (**\r_flares 1**)
- anisotropic filtering (**\r_ext_texture_filter_anisotropic**)
- greatly reduced API overhead (call/dispatch ratio)
- flexible vertex buffer memory management to allow loading huge maps
- multiple command buffers to reduce processing bottlenecks
- [reversed depth buffer](https://developer.nvidia.com/content/depth-precision-visualized)
  to eliminate z-fighting on big maps
- merged lightmaps (atlases)
- multitexturing optimizations
- static world surfaces cached in VBO (**\r_vbo 1**)
- useful debug markers for tools like [RenderDoc](https://renderdoc.org/)
- fixed framebuffer corruption on some Intel iGPUs
- offscreen rendering, enabled with **\r_fbo 1**, all following requires it
  enabled:
- `screenMap` texture rendering - to create realistic environment reflections
- multisample anti-aliasing (**\r_ext_multisample**)
- supersample anti-aliasing (**\r_ext_supersample**)
- per-window gamma-correction which is important for screen-capture tools like
  OBS
- you can minimize game window any time during **\video**|**\video-pipe**
  recording
- high dynamic range render targets (**\r_hdr 1**) to avoid color banding
- bloom post-processing effect
- arbitrary resolution rendering
- greyscale mode

In general, not counting offscreen rendering features you might expect from 10%
to 200%+ FPS increase comparing to KE's original version

Highly recommended to use on modern systems

## OpenGL renderer

Based on classic OpenGL renderers from
[idq3](https://github.com/id-Software/Quake-III-Arena)/[ioquake3](https://github.com/ioquake/ioq3)/[cnq3](https://bitbucket.org/CPMADevs/cnq3)/[openarena](https://github.com/OpenArena/engine),
features:

- OpenGL 1.1 compatible, uses features from newer versions whenever available
- high-quality per-pixel dynamic lighting, can be triggered by **\r_dlightMode**
  cvar
- merged lightmaps (atlases)
- static world surfaces cached in VBO (**\r_vbo 1**)
- all set of offscreen rendering features mentioned in Vulkan renderer, plus:
- bloom reflection post-processing effect

Performance is usually greater or equal to other opengl1 renderers

## Build & Release

See [BUILD.md](BUILD.md) for full setup instructions. Key targets:

**First-time setup** — after cloning, initialize submodules before building:
```
git submodule update --init --recursive
```


| Command                      | What it does                                                     |
| ---------------------------- | ---------------------------------------------------------------- |
| `make`                       | Configure + build Release (native + VM modules)                  |
| `make build-debug`           | Configure + build Debug                                          |
| `make create-launcher`       | Build the Go/Wails launcher                                      |
| `make create-packs`          | Package modfiles/ + VM modules into mod pack                     |
| `make run-launcher`          | Build + assemble + codesign + open launcher                      |
| `make run-game`              | Build + assemble + run engine (main menu)                        |
| `make run-game MAP=q3dm17`   | Build + run + load map                                           |
| `make run-game DEV=1`        | Debug build, native dylibs, developer mode                       |
| `make run-game VM=1`         | VM game modules (sv_pure 1)                                      |
| `make check`                 | Verify VM modules, dylibs, mod pack, codesign, JIT entitlement   |
| `make release`               | Full pipeline: check + assemble + codesign + DMG/tar.gz          |
| `make bundle-dmg`            | Package signed `q3now-<version>-<arch>.dmg` (macOS)              |
| `make bundle-tar`            | Package `q3now-<version>-<arch>.tar.gz` (Linux)                  |
| `make bundle-docker`         | Build Docker image for dedicated server                          |
| `make bench DEMO=four`       | Timedemo benchmark                                               |

**VM backend:** Set `USE_WASM=1` to compile VM game modules via WAMR.
See [WASM.md](WASM.md) for architecture, build instructions, and design
decisions.

**Archive format:** Set `USE_SW3Z=1` for sw3z archives (default: legacy pk3).

**Code signing:** `make bundle-codesign` applies ad-hoc codesigning with
`com.apple.security.cs.allow-jit` on macOS, enabling the ARM64 JIT interpreter
at full speed.

**In-game diagnostics:** type `\q3now_engine` in the server console to print
engine version, active renderer, `vm_rtChecks`, and `com_maxfps`.

## Docker Dedicated Server

[![Docker Hub](https://img.shields.io/docker/pulls/eserozvataf/q3now)](https://hub.docker.com/r/eserozvataf/q3now)

Run a q3now dedicated server with Docker:

```bash
docker run -d -p 27960:27960/udp \
  -v /path/to/your/baseq3:/home/q3now/baseq3 \
  -e Q3_HOSTNAME="My Server" \
  -e Q3_MAXCLIENTS=16 \
  eserozvataf/q3now +map q3dm17
```

Mount your game assets (`pak0.pk3`, custom maps, `server.cfg`) into the
`baseq3` volume. See `docker/docker-compose.yml` for a complete example with
all available environment variables.

| Environment Variable | Engine Cvar       | Description                 |
| -------------------- | ----------------- | --------------------------- |
| `Q3_HOSTNAME`        | `sv_hostname`     | Server name                 |
| `Q3_MAXCLIENTS`      | `sv_maxclients`   | Max players                 |
| `Q3_RCONPASSWORD`    | `rconpassword`    | Remote console password     |
| `Q3_GAMETYPE`        | `g_gametype`      | 0=DM, 1=Duel, 3=TDM, 4=CTF  |
| `Q3_FRAGLIMIT`       | `fraglimit`       | Frag limit                  |
| `Q3_TIMELIMIT`       | `timelimit`       | Time limit (minutes)        |
| `Q3_QUIC`            | `sv_quic`         | Enable QUIC transport       |
| `Q3_EXEC`            | `+exec`           | Execute a config file       |
| `Q3_EXTRA_ARGS`      | _(verbatim)_      | Arbitrary engine arguments  |

## [Build Instructions](BUILD.md)

_This repository does not contain any game content. To play, copy the resulting
binaries into your existing Quake III Arena installation._

## Contacts

Discord channel: https://discordapp.com/invite/X3Exs4C

## Thanks

q3now contains code from;

- BrightArena
- CNQ3
- ioEF engine
- ioquake3
- NetRadiant's q3map2
- OpenArena
- OpenMoHAA
- OSP2
- OSP2-BE
- Quake 3 Arena
- Quake 3 Team Arena
- Quake-III-Arena-Kenny-Edition
- Quake3e
- RTCW MP
- RTCW SP
- spearmint
- Tremulous
- Unlagged
- Wolfenstein: Enemy Territory
- World of Padman
- Xreal
- ZEQ2-lite
- ZNudge
