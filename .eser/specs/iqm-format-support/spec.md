# Spec: iqm-format-support

## Status: completed

## Concerns: beautiful-product, long-lived, move-fast

## Discovery Answers

### status_quo

{"status_quo":"IQM loader exists in all 3 renderers (inherited from ioquake3) but is unconditionally compiled with no feature flag. SHADER_MAX_VERTEXES limit (1024) rejects most real-world IQM models. CPU-only skinning in GL1/renderervk, partial GPU skinning scaffold in renderer2. Player models are hardcoded to three-part MD3 split (lower/upper/head). No single-mesh player model support. Tangent data parsed but unused. MD3 remains the only practical model format.","pain":"High for skeletal content pipeline. Remastered Q1/Q2 assets are MD5 (skeletal) and need IQM as the target format. Current vertex limits reject most models silently. No GPU skinning on Vulkan (primary renderer). Cannot use single-mesh player models.","dream_state":"FEAT_IQM-gated IQM support across all 3 renderers with GPU skinning (phased: renderer2 reference → renderervk main → GL1 fallback). 65536 vert/surface limit with heap-allocated skinning arrays. body.iqm single-mesh player models with named joint attachment points. IQM embedded animation names mapped to Q3 anim enums. Tangent space wired to normal mapping shaders. Static mesh support for skeletonless IQM. MD3 fully preserved.","scope":"IN: FEAT_IQM flag, heap-allocated skinning arrays (65536 limit), GPU skinning all 3 renderers (phased), body.iqm player convention, IQM anim name→enum mapping, tangent space→normal mapping, static mesh for jointless IQM. OUT: format conversion (Go launcher), BSP/texture changes, LOD, animation blending/IK/ragdoll, vertex animation (MD3 stays for that).","risks":"Vulkan pipeline work (Phase 2) is heaviest lift — new SPIR-V shaders, UBO for bone matrices, pipeline variants. GL1 GPU skinning (Phase 3) needs ARB/GLSL extensions with CPU fallback. body.iqm touches cgame player rendering hotpath. 32-bit indexes already supported (glIndex_t is uint32_t). Stack-allocated arrays must move to Hunk_Alloc.","success":"IQM models load and render correctly in all 3 renderers. GPU skinning verified with skeletal animated IQM. body.iqm player models work with named joint attachments. Embedded IQM animation names map to Q3 anim enums. Tangent data flows to normal mapping shaders. MD3 path completely unaffected. All code behind FEAT_IQM guard."}

### ambition

1-star: Wrap existing ioquake3 IQM code behind FEAT_IQM. No vertex limit fix, no GPU skinning, no player model support. Loads trivial test models, rejects anything real-world. 5-star (MVP, this spec): FEAT_IQM gated. Heap-allocated arrays with 65536 vert limit. GPU skinning in all 3 renderers (phased: renderer2 → renderervk → GL1 with CPU fallback). body.iqm single-mesh player models with named joint attachments. Embedded IQM anim names → Q3 enum mapping with animation.cfg fallback. Tangent space wired to normal mapping shaders. Content creator warnings for missing joints, unmapped anims, shader mismatches. 10-star (future): LOD via surface naming. Animation blending/layering. IK and ragdoll. Morph targets. Full PBR pipeline. In-engine IQM debug visualization (skeleton overlay, bone names, anim preview). Hot-reload. Blender exporter preset.

### reversibility

Fully reversible. FEAT_IQM=0 removes all IQM code at compile time. No database migrations, no protocol changes, no file format changes. IQM v2 is a stable spec (unchanged since 2011). Architecture mirrors existing MD3/MDR patterns. The only consideration is social — once community content depends on IQM, removing support would break content, but the code itself is trivially toggleable. Will be correct in 2 years: IQM v2 has been stable for 13+ years, the phased GPU skinning approach follows established renderer patterns, and FEAT_IQM gating keeps it isolated.

### user_impact

No breaking changes. Purely additive. MD3 loading path is completely untouched — same code, same behavior, same limits. body.iqm player model convention is opt-in: if body.iqm does not exist in the player model directory, falls back to the traditional three-part MD3 split (lower/upper/head). No config file changes, no cvar changes, no protocol changes. Existing mods, maps, and content work identically. The only user-visible change is that .iqm files now load successfully when placed in the game assets.

### verification

Verification strategy: Manual visual + compile gates. 1) Compile gate: build with FEAT_IQM=1 and FEAT_IQM=0 — both must compile cleanly with zero warnings. 2) Static IQM: load a static mesh IQM (no joints) — renders correctly as a prop/weapon. 3) Skeletal IQM: load a skeletal animated IQM — animation plays with correct bone deformation. 4) body.iqm player: single-mesh player model walks, attacks, dies with correct animations. Weapon attaches to tag_weapon joint. 5) MD3 regression: existing MD3 player models and world models render identically to pre-change. 6) All 3 renderers: verify IQM renders correctly in renderer (GL1), renderer2 (GL2), and renderervk (Vulkan). GPU skinning active in GL2/VK, CPU fallback in GL1 if no ARB support. 7) Anim mapping: IQM embedded anim names correctly map to Q3 enums. Console warnings appear for unmapped anims. 8) Tangent space: IQM with tangent data shows normal mapping when appropriate shaders are applied. Use make run-game DEV=1 for console diagnostics. No automated unit tests for renderer code — visual verification is the standard.

### scope_boundary

NOT in scope: Format conversion (handled by Go launcher). BSP format changes. Texture pipeline changes. Vertex animation for IQM (MD3 stays for that). LOD support (deferred). Animation blending/layering/IK/ragdoll (deferred). Morph targets (deferred). Hot-reload of IQM files (deferred). In-engine debug visualization — skeleton overlay, bone names, anim preview (deferred). Blender exporter preset (deferred). Technical debt introduced: CPU skinning arrays remain as fallback path in GL1 renderer — this is intentional, not dead code, since GL1 may lack ARB/GLSL support. The heap-allocated skinning arrays add a small memory footprint per IQM model. Three copies of IQM rendering code across three renderers (inherited pattern, not new debt). v2 polish items: LOD via surface naming convention. Animation blending/layering system. Debug visualization (skeleton overlay, bone names). Hot-reload for development workflow. Blender exporter preset tuned for q3now conventions.

## v1 vs v2 Scope (move-fast)

_To be addressed during execution._

## Out of Scope

- NOT in scope: Format conversion (handled by Go launcher)
- BSP format changes
- Texture pipeline changes
- Vertex animation for IQM (MD3 stays for that)
- LOD support (deferred)
- Animation blending/layering/IK/ragdoll (deferred)
- Morph targets (deferred)
- Hot-reload of IQM files (deferred)
- In-engine debug visualization — skeleton overlay, bone names, anim preview (deferred)
- Blender exporter preset (deferred)
- Technical debt introduced: CPU skinning arrays remain as fallback path in GL1 renderer — this is intentional, not dead code, since GL1 may lack ARB/GLSL support
- The heap-allocated skinning arrays add a small memory footprint per IQM model
- Three copies of IQM rendering code across three renderers (inherited pattern, not new debt). v2 polish items: LOD via surface naming convention
- Animation blending/layering system
- Debug visualization (skeleton overlay, bone names)
- Hot-reload for development workflow
- Blender exporter preset tuned for q3now conventions.

## Tasks

- [x] task-1: Add FEAT_IQM flag to q_feats.h and wrap all existing IQM code (iqm.h, tr_model_iqm.c, IQM branches in tr_model.c, tr_local.h structs) across all 3 renderers in #if defined(FEAT_IQM) guards. AC: compiles cleanly with FEAT_IQM=1 and FEAT_IQM=0.
- [x] task-2: Heap-allocate IQM skinning arrays (influenceVtxMat, influenceNrmMat) via Hunk_Alloc at model load time instead of stack. Raise IQM-specific vertex limit to 65536 per surface in R_LoadIQM loader validation. AC: IQM models with >1024 verts per surface load successfully. No stack overflow on large models.
- [x] task-3: GPU skinning Phase 1 — renderer2 (GL2). Wire existing bone matrix GLSL infrastructure (glState.boneAnimation, glState.boneMatrix) to IQM VBO path. Upload IQM vertex data (positions, normals, texcoords, tangents, bone indices, bone weights) to VBO at load time. Send bone matrices as uniforms per frame. Draw directly through VBO/VAO without tess. AC: skeletal IQM renders correctly in renderer2 with GPU skinning. This becomes the reference implementation.
- [x] task-4: GPU skinning Phase 2 — renderervk (Vulkan). Port renderer2 GPU skinning logic to Vulkan. Create new Vulkan pipeline variant with bone matrix UBO. Write SPIR-V vertex shader with bone transform. Upload IQM vertex data to Vulkan VBO at load time. AC: skeletal IQM renders correctly in renderervk matching renderer2 output.
- [x] task-5: GPU skinning Phase 3 — renderer (GL1). Add ARB_vertex_program or minimal GLSL path for bone transforms if extensions available at runtime. Fall back to CPU skinning (heap-allocated arrays from task-2) if no GPU skinning support. AC: skeletal IQM renders correctly in GL1 renderer. CPU fallback works when ARB/GLSL unavailable.
- [x] task-6: body.iqm player model convention. In cg_players.c, detect models/players/<name>/body.iqm. If exists load as single-mesh player model. Use named joints (tag_torso, tag_head, tag_weapon, tag_flash) as attachment points. Fall back to three-part MD3 (lower/upper/head) if body.iqm absent. AC: body.iqm player walks/attacks/dies with correct animations and weapon attachment.
- [x] task-7: IQM embedded animation name mapping. At load time map IQM iqmAnim names (e.g. LEGS_WALK, TORSO_ATTACK) to Q3 playerAnimations_t enums. Case-insensitive matching. Log warnings for Q3 enums with no matching IQM anim. Fallback to animation.cfg with raw frame ranges if num_anims==0. AC: IQM player model plays correct animations by name. Warnings appear for unmapped anims.
- [x] task-8: Wire IQM tangent vertex arrays to normal mapping shaders in renderer2 and renderervk. Tangent data already parsed in loader. Pass through VBO to vertex shader. AC: IQM models with tangent data show correct normal mapping effects.
- [x] task-9: Verification and regression. Compile gate: FEAT_IQM=0 and FEAT_IQM=1 both compile cleanly. Visual test: static IQM prop, skeletal animated IQM, body.iqm player model across all 3 renderers. MD3 regression: existing models render identically to pre-change. AC: all verification criteria from discovery pass.

## Verification

- Verification strategy: Manual visual + compile gates. 1) Compile gate: build with FEAT_IQM=1 and FEAT_IQM=0 — both must compile cleanly with zero warnings. 2) Static IQM: load a static mesh IQM (no joints) — renders correctly as a prop/weapon. 3) Skeletal IQM: load a skeletal animated IQM — animation plays with correct bone deformation. 4) body.iqm player: single-mesh player model walks, attacks, dies with correct animations
- Weapon attaches to tag_weapon joint. 5) MD3 regression: existing MD3 player models and world models render identically to pre-change. 6) All 3 renderers: verify IQM renders correctly in renderer (GL1), renderer2 (GL2), and renderervk (Vulkan)
- GPU skinning active in GL2/VK, CPU fallback in GL1 if no ARB support. 7) Anim mapping: IQM embedded anim names correctly map to Q3 enums
- Console warnings appear for unmapped anims. 8) Tangent space: IQM with tangent data shows normal mapping when appropriate shaders are applied
- Use make run-game DEV=1 for console diagnostics
- No automated unit tests for renderer code — visual verification is the standard.
