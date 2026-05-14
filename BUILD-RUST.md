# BUILD-RUST.md — Rust toolchain for the WGSL shader-translation path

**Build-time dependency only.** No Rust code links into the Wired engine at
runtime. The Phase 7 shader pipeline (see `docs/phase-7-ral-design.md §16.2`)
uses [`naga`](https://github.com/gfx-rs/wgpu/tree/trunk/naga) to translate
SPIR-V into WGSL for the WebGPU backend. We invoke `naga` as a CLI from the
offline `shader_xlate` tool (Phase 7.3b) — the "Option B2" integration: the
Rust toolchain is a *developer / CI* dependency, never shipped with the game.

Wired's other backends (MSL, GLSL 4.30, GLSL ES 300) come from
[SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross), which is a C++
submodule at `src/libs/SPIRV-Cross/` and needs no Rust.

## When you need Rust

- You're building the WebGPU backend (Phase 7.1+ RAL → 7.3c pipeline → ...).
- You're running `shader_xlate` end-to-end on the q3now shader corpus and
  want the `*.wgsl` outputs.
- You're a CI runner that produces translated shader artefacts.

If you don't need WGSL output, you can skip Rust entirely: `shader_xlate`
detects the absence of `naga` on `PATH` and emits a one-line note per shader
(`[xlate] <name> wgsl=skip(naga unavailable)`). MSL / GLSL / GLSL ES still
build via SPIRV-Cross.

## Installation

### All platforms — rustup

[rustup](https://rustup.rs/) is the official installer. It pulls a pinned
toolchain into `~/.cargo/` and `~/.rustup/`.

- **Windows (MSYS2 / MinGW or native)**: download and run the rustup
  installer from <https://rustup.rs/>, accept the defaults. Reopen the shell.
- **macOS / Linux**: `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh`

After `rustup` finishes, verify:

```sh
cargo  --version    # → cargo 1.80+ (or whatever the current stable is)
rustc  --version
```

### Install `naga`

Two paths — pick one.

**(a) `cargo install` (recommended for development boxes / CI)**

```sh
cargo install naga-cli --locked
# Puts `naga` (or `naga.exe`) in ~/.cargo/bin — make sure that's on PATH.
```

This is the fastest path and is what `shader_xlate` documents as the default.

**(b) Build from the vendored submodule (canonical for the engine repo)**

The Phase 7 plan vendors the
[`gfx-rs/wgpu`](https://github.com/gfx-rs/wgpu) monorepo as a git submodule at
`src/libs/wgpu/` (the `naga` crate lives inside it). Once the submodule is in
place:

```sh
cargo build --manifest-path src/libs/wgpu/Cargo.toml \
            --release -p naga-cli
# Output: src/libs/wgpu/target/release/naga(.exe).
# Either copy it onto PATH or add the target/release/ dir to PATH.
```

(Adding the submodule is a one-time `git submodule add` — see "Submodules"
below.)

## Submodules

For an end-to-end set-up matching `phase-7-ral-design.md`:

```sh
# SPIRV-Cross (C++ — required for MSL / GLSL / GLSL ES output)
git submodule add https://github.com/KhronosGroup/SPIRV-Cross.git src/libs/SPIRV-Cross
cd src/libs/SPIRV-Cross && git checkout vulkan-sdk-1.4.341.0 && cd ../../..

# wgpu / naga (Rust — required for WGSL output only)
git submodule add https://github.com/gfx-rs/wgpu.git src/libs/wgpu
cd src/libs/wgpu && git checkout v22.1.0 && cd ../../..   # or whatever current stable

git add .gitmodules src/libs/SPIRV-Cross src/libs/wgpu
git commit -m "deps: vendor SPIRV-Cross + wgpu (naga) as submodules for Phase 7.3b"
```

Then to bring up a fresh clone:

```sh
git clone … && cd q3now
git submodule update --init --recursive
```

## Versions / MSRV

- **Rust**: the latest `naga` crate tracks recent stable Rust; **MSRV ~1.76**
  as of the wgpu 22.x line. `rustup` defaults to the latest stable, which is
  always ≥ MSRV.
- **`naga-cli`**: tag track the wgpu release that vendors it. The engine's
  pinned tag (the one Phase 7.3c will lock in) is whichever wgpu the
  submodule check-out resolves to.

A `rust-toolchain.toml` at repo root will pin the exact toolchain in a later
phase; until then, stable-default is fine.

## What `shader_xlate` does when `naga` is missing

Nothing fatal. The tool runs the SPIRV-Cross targets (MSL / GLSL / GLSL ES)
to completion, then logs:

```
[xlate] <basename> wgsl=skip(naga unavailable)
```

`*.wgsl` files for those shaders simply aren't produced this run. Re-run
once `naga` is installed and they appear alongside.

## Cross-reference

- `docs/phase-7-ral-design.md` §16.2 — the toolchain decision (SPIRV-Cross
  for MSL/GLSL/GLSL ES, naga for WGSL).
- `code/tools/shader_xlate/README.md` — tool usage.
- `code/tools/shader_xlate_spike/README.md` — the Phase 7 pre-flight spike
  (validates the toolchain shape; throwaway, replaced by `shader_xlate`).
