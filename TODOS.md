# q3now — TODOs

Remaining work after the Quake3e engine swap.

---

## P1 — Verification (do before releasing)

- [ ] **In-game feature test** — launch with `make run MAP=q3dm1` and verify:
  - Instagib mode: `/instagib 1` → railgun one-shot kills
  - Wall jumps: jump against a vertical wall → gain upward momentum
  - Excessive mode: `/g_excessive 1` → amplified weapon damage
  - Promode movement: `/g_promode 1` → CPM physics active
  - Grappling hook: `/g_grapple 1` → hook fires correctly
  - Colored player names render in scoreboard
  - Console shows "q3now mod loaded" (from autoexec.cfg echo)

- [ ] **Vulkan renderer** — in console: `\r_backend vulkan` + `\vid_restart` → verify Vulkan renders (requires MoltenVK: `brew install molten-vk`)

- [ ] **Native module debug mode** — `make run-dev MAP=q3dm1` → console should show `native` VM messages for cgame, qagame, ui

- [ ] **Smoke test with assets** — `make smoke` with `/Applications/q3now/baseq3/pak0.pk3` present → should print `PASS`

---

## P2 — Polish

*(All P2 items completed — see Completed section below)*

---

## P3 — Infrastructure

- [ ] **macOS code signing for ARM64 JIT** — Quake3e's ARM64 JIT generates executable code at runtime. macOS requires `com.apple.security.cs.allow-jit` entitlement for this. Without it, the JIT interpreter falls back silently. Add the entitlement to the cmake build (`q3now.entitlements`) and re-sign the `.app` as part of `make install`. See: `cmake/macos.cmake`.

- [ ] **`make smoke` in CI** — once a PAK0 mirror or test map is available in CI, wire `make smoke` into the GitHub Actions workflow. Currently CI only verifies QVM file existence; smoke would verify runtime loading.

- [ ] **Custom app icon** — replace `quake3_flat.icns` with a q3now-specific icon. Separate from the engine swap; cosmetic only.

- [ ] **macOS DMG installer** — package `q3now.app` + `baseq3/zz-q3now.pk3` + a README into a distributable `.dmg`. The cmake tree already has `installer.cmake` and `macos-dmg-setup.applescript.in` from Quake3e.

- [ ] **Windows build** — the cmake build compiles on Windows (MSVC or MinGW) but the QVM toolchain (`code/tools/lcc/`) requires a Unix-like environment (requires porting cmake `ExternalProject` to work with MSVC). Deferred until macOS is verified stable.

---

## Completed

- [x] Engine swap: ioquake3 → Quake3e
- [x] All game modules ported: cgame, qagame, ui (45 modified files + bg_promode.c/h)
- [x] QVM toolchain (lcc/q3asm) integrated via cmake ExternalProject
- [x] All QVM compilation errors fixed (const propagation, C89 compatibility)
- [x] `make check` passes all 7 verifications
- [x] `make install` deploys to `/Applications/q3now/`
- [x] CI workflow updated for cmake-based build with QVM verification
- [x] `tests/smoke.sh` headless smoke test (graceful skip without assets)
- [x] Ded server renamed: `q3now.ded` → `q3now-ded`
- [x] SDL2 → SDL3 migration: audio stream API, event restructuring, gamepad enabled, Wayland hint, SDL3 dylib bundling in `.app`
- [x] `g_q3now` cvar — mod version (1.0) + active feature print in `G_InitGame()` on every map start
- [x] `q3now_engine` server console command — prints engine version, renderer, `vm_rtChecks`, `com_maxfps`
- [x] `q3now.cfg` in `modfiles/` — competitive defaults (`com_maxfps 240`, `cl_autoNudge 1`, `cg_drawFPS 1`, `rate 25000`)
- [x] `make bench` — timedemo target with demo existence check and `DEMO=` variable
- [x] `make diff-api` — diffs `g_public.h`, `cg_public.h`, `ui_public.h` vs upstream Quake3e fork point
