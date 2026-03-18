# q3now — TODOs

Remaining work after the Quake3e engine swap.

---

## P3 — Infrastructure (open)

- [ ] **`make smoke` in CI** — once a PAK0 mirror or test map is available in CI, wire `make smoke` into the GitHub Actions workflow. Currently CI only verifies QVM file existence; smoke would verify runtime loading.

- [ ] **Custom app icon** — replace `quake3_flat.icns` with a q3now-specific icon. Separate from the engine swap; cosmetic only.

- [ ] **Windows build** — the cmake build compiles on Windows (MSVC or MinGW) but the QVM toolchain (`code/tools/lcc/`) requires a Unix-like environment (requires porting cmake `ExternalProject` to work with MSVC). Deferred until macOS is verified stable.

