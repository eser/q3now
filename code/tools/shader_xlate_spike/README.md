# shader_xlate_spike — Phase 7 shader-translation toolchain pre-flight

**Throwaway.** Validates that the shader-translation toolchain chosen in
`docs/phase7-hal-design.md §16.2` (SPIRV-Cross → MSL / GLSL / GLSL-ES, naga →
WGSL) can be vendored, built, linked, and invoked on the target platforms,
*before* Phase 7.1 starts. Nothing in the engine links against this. Delete the
whole `code/tools/shader_xlate_spike/` directory (and the spike's vendored
`code/libs/SPIRV-Cross/`, and `build/shader_xlate_spike/`) once Phase 7's real
shader pipeline lands.

## What it does

`shader_xlate_spike <input.spv> [outdir]` reads a SPIR-V blob and translates it,
via the **SPIRV-Cross C++ API** (the way the eventual HAL would link it —
`spirv_cross::CompilerMSL` / `CompilerGLSL` + an `Options` struct + `compile()`),
to:

| Output | Backend | Target |
|---|---|---|
| `<base>.msl`        | `CompilerMSL`  (`platform = macOS`)  | Metal Shading Language |
| `<base>.430.glsl`   | `CompilerGLSL` (`version = 430, es = false`) | desktop OpenGL 4.3 |
| `<base>.300es.glsl` | `CompilerGLSL` (`version = 300, es = true`)  | WebGL2 (GLSL ES 3.00) |
| `<base>.wgsl`       | `naga` CLI (shelled out, if on `PATH`) | WebGPU (WGSL) |

and prints the first ~10 lines of each to stdout. Per-backend translation
failures are caught and reported (one unsupported construct won't abort the run).

WGSL is **not** a SPIRV-Cross backend — naga owns it. The spike calls the `naga`
CLI; if it isn't installed it prints a note (see "naga" below). That mirrors the
**Option B2** integration (naga as an offline build-time tool — see the report
and `docs/phase7-hal-design.md`): no Rust is linked into the engine.

## Build

Standalone CMake project — **not** wired into the engine's root `CMakeLists.txt`
(keep the spike contained):

```sh
# from the repo root, MSYS2 MINGW64 shell (or any platform's native toolchain):
export PATH=/c/msys64/mingw64/bin:$PATH        # MinGW only
cmake -G Ninja -S code/tools/shader_xlate_spike -B build/shader_xlate_spike -DCMAKE_BUILD_TYPE=Release
ninja -C build/shader_xlate_spike
```

This builds the vendored `code/libs/SPIRV-Cross/` (only the `core` + `glsl` +
`msl` static libs — CLI / tests / HLSL / C-API / reflect / util / cpp are off)
and links `shader_xlate_spike` against them.

`code/libs/SPIRV-Cross/` is a trimmed copy of KhronosGroup/SPIRV-Cross tag
**`vulkan-sdk-1.4.341.0`** (chosen to match the engine's pinned Vulkan SDK
`1.4.341.1`; the `shaders*` / `reference` / `samples` / `gn` test-fixture dirs
were deleted to keep it ~3 MB). The eventual real Phase 7 integration should
vendor it as a **git submodule** under `src/libs/SPIRV-Cross/` to match the
existing `src/libs/picoquic` / `recastnavigation` / `luajit` pattern (this spike
uses `code/libs/` + a copied tree only because it's throwaway and must not touch
git).

## Run

```sh
# extract a few real q3now SPIR-V blobs from the generated shader_data.c:
node code/tools/shader_xlate_spike/extract_spv.mjs \
     code/renderervk/shaders/spirv/shader_data.c /tmp/spvtest \
     shadow_depth_vert_spv tonemap_frag_spv q1_ls_frag_spv
# (or just use the .spv files that already exist:
#   code/renderervk/shaders/spirv/iqm_skinning_{vert,frag}.spv
#  or compile a shader fresh:
#   "$VULKAN_SDK/bin/glslangValidator" -V code/renderervk/shaders/tonemap.frag -o /tmp/tonemap.spv )

build/shader_xlate_spike/shader_xlate_spike.exe /tmp/spvtest/tonemap_frag_spv.spv /tmp/spvtest
```

## naga (WGSL)

naga needs a **Rust toolchain** (build-time only — nothing Rust ships in the
engine). Install via `rustup` (all platforms — see https://rustup.rs), then:

```sh
cargo install naga-cli            # puts `naga` on PATH (~/.cargo/bin)
# or, vendored: clone gfx-rs/wgpu, then  cargo build -p naga-cli --release
```

The spike's `--wgsl` step then shells out to `naga in.spv out.wgsl`. As of this
spike's run on the dev image, no Rust toolchain was installed, so the WGSL leg
is **untested locally** — see the report for the (low-risk) integration plan.

## Files

- `main.cpp` — the test executable.
- `CMakeLists.txt` — standalone build (pulls in `../../libs/SPIRV-Cross`).
- `extract_spv.mjs` — node helper: `shader_data.c` C-arrays → `.spv` files
  (python3 isn't on the dev image; the project already uses node for `compile.mjs`).
- `README.md` — this file.
