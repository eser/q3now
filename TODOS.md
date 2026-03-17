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

- [ ] **`g_q3now` cvar** — add a cvar that prints mod version + active features to console on map start (e.g., `q3now v2.0 | instagib walljump excessive promode grapple lms koth ghost`). Closes the silent-failure gap where mod loads but features are inactive.

- [ ] **`\q3now_engine` command** — add a game-side console command using `trap_Cvar_*` syscalls to print engine capabilities (renderer backend, JIT arch, `vm_rtChecks` value). Zero engine changes — uses only existing game API.

- [ ] **`q3now.cfg`** — ship a `q3now.cfg` inside `zz-q3now.pk3` with competitive defaults:
  ```
  seta com_maxfps "240"
  seta cl_autoNudge "1"
  seta r_backend "vulkan"
  seta cg_drawFPS "1"
  seta rate "25000"
  ```
  This sets Vulkan as default renderer without any engine changes.

- [ ] **`make bench`** — timedemo benchmark target: launches `q3now` with `+timedemo 1 +demo four` and prints avg FPS. Check that `demos/four.dm_68` exists before launching; print a clear error if not.

- [ ] **`make diff-api`** — diff Quake3e's `g_public.h`, `cg_public.h`, `ui_public.h` API headers against q3now's modified versions, highlighting any new syscalls or changed enums. Essential for safe upstream sync.

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
