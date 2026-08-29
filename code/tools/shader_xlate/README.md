# shader_xlate — offline SPIR-V → MSL / GLSL 4.60 / WGSL translator

The real Phase 7.3b shader-translation tool. Reads a `.spv` produced by
`glslangValidator` (the same pipeline `compile.mjs` already uses to emit
`shader_data.c`) and emits the three backend forms next to it.

The Phase 7 pre-flight version lived at `code/tools/shader_xlate_spike/`; that
spike validated the *toolchain shape*. This one is the integrated
implementation used by `compile_xlate.mjs` (Phase 7.3c) to build the per-
backend translated-shader corpus.

## What it produces

| Output            | Backend (SPIRV-Cross / naga)            | Target              |
|-------------------|------------------------------------------|---------------------|
| `<base>.msl`      | `CompilerMSL` (`platform = macOS`)       | Metal Shading Lang. |
| `<base>.glsl460`  | `CompilerGLSL` (`version=460, es=false, vulkan_semantics=true`) | Portable GLSL 4.60 input for deterministic OpenGL lowering |
| `<base>.wgsl`     | pinned `naga` CLI (direct child process)  | WebGPU (WGSL)       |

The corpus driver deterministically lowers Vulkan-only GLSL syntax and push
constants into desktop OpenGL 4.6 binding/uniform forms after translation; MSL
keeps push constants as argument-buffer data. The pinned translator rewrites
Naga's non-WebGPU `immediate` address space to the first free group-0 uniform
binding, exactly matching the resolved portable manifest. See
`docs/phase-7-ral-design.md §8.3`.

Canonical WGSL generation requires Wired's pinned
`naga 30.0.0+wired-portable-v1`; build it with `build_pinned_naga.sh` as
documented in `BUILD-RUST.md`. Optional target runs may omit WGSL, but
`--require-wgsl` fails closed on a missing or wrong identity.

## Build

The tool is gated by the explicit `WIRED_BUILD_SHADER_XLATE` CMake option
(default OFF). RAL itself is mandatory in the Vulkan renderer; translator
tooling remains an independent build-time choice.

```sh
cmake -DWIRED_BUILD_SHADER_XLATE=ON -G Ninja -S . -B build
ninja -C build shader_xlate
```

Pre-flight: vendor `src/libs/SPIRV-Cross/` (tag `vulkan-sdk-1.4.341.0`) as a
git submodule — see `BUILD-RUST.md` for the exact command. The build prints
a clear warning + skips the target if the submodule is absent.

## Run

```sh
# Single shader:
build/shader_xlate path/to/shader.spv path/to/output_dir/

# fail closed instead of accepting a missing naga/WGSL artifact:
build/shader_xlate --require-wgsl path/to/shader.spv path/to/output_dir/

# build the exact translator identity consumed above:
code/tools/shader_xlate/build_pinned_naga.sh build/tools/naga/naga

# deterministic SPIRV-Cross JSON reflection (no target-language emission):
build/shader_xlate --reflect-only path/to/shader.spv path/to/reflection.json

# All shaders in shader_data.c (the way compile.mjs emits the corpus):
node code/tools/shader_xlate/extract_spv.mjs \
     code/render/ral/backends/vulkan/renderer/shaders/spirv/shader_data.c \
     build/spv_extracted/
mkdir -p build/translated_shaders/
for f in build/spv_extracted/*.spv; do
    build/shader_xlate "$f" build/translated_shaders/
done

# Deterministic manifest-driven corpus orchestration (one symbol shown):
node code/render/ral/backends/vulkan/renderer/shaders/compile_xlate.mjs \
  --translator build/shader_xlate \
  --output-dir build/translated_shaders \
  --symbol color_vert_spv

# Reflect the canonical 299-artifact corpus into the native-free ABI catalog:
node code/render/ral/backends/vulkan/renderer/shaders/compile_reflect.mjs \
  --translator build/shader_xlate \
  --output code/render/ral/backends/vulkan/renderer/shaders/spirv/ral_shader_reflection_catalog.json \
  --check

# Resolve every explicit portability decision and verify the committed corpus:
node code/render/ral/backends/vulkan/renderer/shaders/compile_portable_manifest.mjs \
  --translation-dir code/render/ral/backends/vulkan/renderer/shaders/portable --check
node code/render/ral/backends/vulkan/renderer/shaders/compile_xlate.mjs \
  --translator build/shader_xlate \
  --output-dir code/render/ral/backends/vulkan/renderer/shaders/portable \
  --targets msl,wgsl --require-wgsl --check
```

Output per shader is a parseable one-liner per backend:

```
[xlate] <base> msl=ok
[xlate] <base> glsl460=ok
[xlate] <base> wgsl=ok                          # or wgsl=skip(naga unavailable) or wgsl=FAIL: <msg>
[xlate] <base> <target>=FAIL: <error message>
```

Exit codes: `0` = all SPIRV-Cross targets succeeded, `1` = input unreadable,
`2` = at least one SPIRV-Cross target failed.

## Files

- `main.cpp` — the translator (C++ 17, links `spirv-cross-{msl,glsl,core}`).
- `build_pinned_naga.sh` + `patches/` — reproducible pinned WGSL translator.
- `extract_spv.mjs` — node helper that splits the C-array form (`shader_data.c`)
  into individual `.spv` files. Mirrors the spike's helper byte-for-byte;
  python3 isn't on the dev image, the project already uses node for
  `compile.mjs`.
- `README.md` — this file.

## Relationship to the spike

`code/tools/shader_xlate_spike/` and `code/libs/SPIRV-Cross/` (the spike's
tarball-copied SPIRV-Cross) stay in place while Phase 7.3c integration lands.
Deletable once the production path (`src/libs/SPIRV-Cross/` submodule +
`code/tools/shader_xlate/` + `compile_xlate.mjs`) is verified end-to-end on
macOS and Linux. See `docs/phase-7-ral-design.md §16.2` and the 7.3b report.
