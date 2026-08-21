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

### Build the pinned `naga`

Wired requires the exact identity `30.0.0+wired-portable-v1`. The repository
builder fetches the published `naga`/`naga-cli` 30.0.0 crates, applies the
versioned Wired portability patch, builds with the crate's published lockfile,
and verifies the resulting identity:

```sh
code/tools/shader_xlate/build_pinned_naga.sh build/tools/naga/naga
export PATH="$PWD/build/tools/naga:$PATH"
naga --version
# 30.0.0+wired-portable-v1
```

An unpatched `cargo install naga-cli` binary is intentionally rejected. The
Wired patch contains only the SPIR-V portability behavior exercised by the
canonical corpus: derived scalar specialization operations, deterministic
combined-image/sampler splitting, and WGSL-compatible storage access widening.
The C++ wrapper additionally performs the pinned push-data `immediate`→uniform
rewrite before canonical publication; its binding is exact-joined to the
portable manifest and covered by freshness checks.

## Submodules

For an end-to-end set-up matching `phase-7-ral-design.md`:

```sh
# SPIRV-Cross (C++ — required for MSL / GLSL / GLSL ES output)
git submodule add https://github.com/KhronosGroup/SPIRV-Cross.git src/libs/SPIRV-Cross
cd src/libs/SPIRV-Cross && git checkout vulkan-sdk-1.4.341.0 && cd ../../..

git add .gitmodules src/libs/SPIRV-Cross
git commit -m "deps: vendor SPIRV-Cross for shader translation"
```

Then to bring up a fresh clone:

```sh
git clone … && cd q3now
git submodule update --init --recursive
```

## Versions / MSRV

- **Rust**: use a stable toolchain accepted by the published Naga 30 lockfile.
  Rust is build-time-only and does not enter the runtime ABI.
- **`naga-cli`**: exactly `30.0.0+wired-portable-v1`; both `shader_xlate` and
  the generated translation catalog record and reject any other identity.

## What `shader_xlate` does when `naga` is missing

Nothing fatal. The tool runs the SPIRV-Cross targets (MSL / GLSL / GLSL ES)
to completion, then logs:

```
[xlate] <basename> wgsl=skip(naga unavailable)
```

`*.wgsl` files for those shaders simply aren't produced in an optional-target
run. Canonical generation uses `--require-wgsl`, so missing or unpinned Naga
fails closed.

## Cross-reference

- `docs/phase-7-ral-design.md` §16.2 — the toolchain decision (SPIRV-Cross
  for MSL/GLSL/GLSL ES, naga for WGSL).
- `code/tools/shader_xlate/README.md` — tool usage.
- `code/tools/shader_xlate_spike/README.md` — the Phase 7 pre-flight spike
  (validates the toolchain shape; throwaway, replaced by `shader_xlate`).
