// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// tools/hal-to-ral-rename.mjs — Phase 7.4a-followup helper. Applies the
// HAL → RAL rename across a fixed list of in-tree files using designed,
// word-boundary-aware substitutions. The substitution table is the
// "manual review per occurrence" payload — every rule is a conscious
// design choice that's been verified to not collide with false-positive
// tokens like CA_CHALLENGING / CLI_MATCHALERTS (which contain "HAL" as
// a substring and must stay intact).
//
// Run with `node tools/hal-to-ral-rename.mjs` from the project root.
// The script is read-only by default; pass `--apply` to write changes
// back, or `--check` to verify zero remaining HAL references after a
// previous --apply pass.

import fs from "node:fs";
import path from "node:path";

const ARG_APPLY = process.argv.includes("--apply");
const ARG_CHECK = process.argv.includes("--check");

// Files in scope — every file containing rename-target HAL references
// (excluding docs/phase-7-ral-design.md which the user already renamed
// manually and whose historical-note "HAL → RAL" prose must stay).
const FILES = [
	"code/renderer/ral/ral.h",
	"code/renderer/ral/ral_backend.h",
	"code/renderer/ral/ral_command.h",
	"code/renderer/ral/ral_pipeline.h",
	"code/renderer/ral/ral_query.h",
	"code/renderer/ral/ral_resource.h",
	"code/renderer/ral/ral_swapchain.h",
	"code/renderer/ral/ral_sync.h",
	"code/renderer/ral/ral_types.h",
	"code/renderer/ral_vulkan/ral_vulkan_backend.c",
	"code/renderer/ral_vulkan/ral_vulkan_caps.c",
	"code/renderer/ral_vulkan/ral_vulkan_command.c",
	"code/renderer/ral_vulkan/ral_vulkan_internal.h",
	"code/renderer/ral_vulkan/ral_vulkan_memory.c",
	"code/renderer/ral_vulkan/ral_vulkan_pipeline.c",
	"code/renderer/ral_vulkan/ral_vulkan_query.c",
	"code/renderer/ral_vulkan/ral_vulkan_resource.c",
	"code/renderer/ral_vulkan/ral_vulkan_swapchain.c",
	"code/renderer/ral_vulkan/ral_vulkan_sync.c",
	"code/renderer/ral_vulkan/pipeline_test_spv.h",
	"code/renderer/ral_vulkan/test_shaders/pipeline_test_comp.glsl",
	"code/renderer/ral_vulkan/test_shaders/pipeline_test_vert.glsl",
	"code/renderer/ral_vulkan/test_shaders/pipeline_test_frag.glsl",
	"code/renderer/ral_vulkan/test_shaders/regen.sh",
	"code/renderervk/vk_ral_textures.c",
	"code/renderervk/vk_ral_textures.h",
	"code/renderervk/tr_local.h",
	"code/renderervk/tr_common.h",
	"code/renderervk/tr_init.c",
	"code/renderervk/tr_image.c",
	"code/renderervk/vk.c",
	"code/client/cl_main.c",
	"code/qcommon/q_feats.h",
	"CMakeLists.txt",
	"docs/health.md",
	"code/tools/shader_xlate/README.md",
	"BUILD-RUST.md",
];

// Substitution rules — applied in ORDER per file. Each rule is a
// designed pattern that handles a specific class of tokens. Earlier
// rules eat their tokens before later catch-alls run.
//
// Critical false-positive guard: word boundaries (\b) and lookarounds
// prevent eating substrings of unrelated identifiers like
// CA_CHALLENGING / CLI_MATCHALERTS / WATERLEVEL_HALFWAY / EF_BOUNCE_HALF.
const RULES = [
	// 1) Prose substitutions (longest patterns first)
	[/Hardware Abstraction Layer/g, "Renderer Abstraction Layer"],

	// 2) Header guards (specific to avoid catching nothing else)
	[/\bWIRED_HAL_/g, "WIRED_RAL_"],

	// 3) Build flag (specific — must precede bare HAL_)
	[/\bFEAT_HAL\b/g, "FEAT_RAL"],
	[/-DFEAT_HAL\b/g, "-DFEAT_RAL"],
	[/WIRED_BUILD_SHADER_XLATE/g, "WIRED_BUILD_SHADER_XLATE"], // no-op (kept to document intent)

	// 4) Cvar name — explicit (preserves the "RAL" capitalization in `r_useRALTextures`)
	[/\br_useHALTextures\b/g, "r_useRALTextures"],

	// 5) UPPER macros / enums: HAL_X → RAL_X (word boundary on the left)
	//    Excludes substrings like CA_CHALLENGING / CLI_MATCHALERTS where
	//    "HAL" appears mid-identifier.
	[/\bHAL_/g, "RAL_"],

	// 6) Cmake/build helpers: WIRED_LINK_HAL_VULKAN → WIRED_LINK_RAL_VULKAN
	//    Already covered by HAL_ → RAL_ (handles WIRED_LINK_HAL_ prefix).

	// 7) Log-tag string "[HAL]" → "[RAL]" — explicit (specific bracket pattern)
	[/\[HAL\]/g, "[RAL]"],

	// 8) Bare "HAL" word in prose (after specific patterns are done).
	//    Word boundary on both sides to skip CA_CHALLENGING / CLI_MATCHALERTS / etc.
	[/\bHAL\b/g, "RAL"],

	// 9) CamelCase functions: Hal_X → Ral_X (Hal_Dump, Hal_CreateBackend, …)
	[/\bHal_/g, "Ral_"],

	// 10) Backend-internal helpers: halVk_X → ralVk_X
	[/\bhalVk_/g, "ralVk_"],

	// 11) Lowercase snake-case identifier prefix: hal_X → ral_X.
	//     No word boundary on the left — vk_hal_textures contains hal_
	//     in the middle (between two underscores) and IS a rename target.
	//     Verified safe: no false-positive token like "Shall_" contains
	//     literal "hal_" (h,a,l,_) as a substring.
	[/hal_/g, "ral_"],

	// 12) Lowercase type prefix: halX where X is uppercase (halBackend_t,
	//     halBuffer_t, halUnsupported, halSuccess, halResult_t, …).
	//     Word boundary on left.
	[/\bhal([A-Z])/g, "ral$1"],

	// 13) Bare "hal" in prose (e.g., "the hal" — rare; whole-word).
	[/\bhal\b/g, "ral"],

	// 14) Snake-case identifier embedded HAL → RAL (e.g., VK_HAL_RECENT_NAMES,
	//     s_HAL_backend — rare; underscore-preceded UPPER HAL).
	[/_HAL_/g, "_RAL_"],
	[/_HAL\b/g, "_RAL"],
	[/_Hal_/g, "_Ral_"],
	[/_Hal([A-Z])/g, "_Ral$1"],
];

let totalChanges = 0;
let filesChanged = 0;

function processFile(filepath) {
	if (!fs.existsSync(filepath)) {
		console.error(`  MISSING: ${filepath}`);
		return;
	}
	const orig = fs.readFileSync(filepath, "utf8");
	let out = orig;
	let perFileChanges = 0;

	for (const [re, repl] of RULES) {
		const before = out;
		out = out.replace(re, repl);
		if (out !== before) {
			// Count substitutions for this rule (approx)
			const matches = before.match(re);
			perFileChanges += matches ? matches.length : 0;
		}
	}

	if (out !== orig) {
		filesChanged++;
		totalChanges += perFileChanges;
		console.log(`  ${perFileChanges.toString().padStart(5)} ${filepath}`);
		if (ARG_APPLY) {
			fs.writeFileSync(filepath, out, "utf8");
		}
	}
}

function checkRemainingHal() {
	// After --apply, sweep for any remaining HAL/Hal/hal tokens we missed.
	// Word-boundary aware. Skip the documented historical exception.
	const exempt = new Set([
		"docs/phase-7-ral-design.md",
	]);
	const hits = [];
	for (const f of FILES) {
		if (exempt.has(f) || !fs.existsSync(f)) continue;
		const text = fs.readFileSync(f, "utf8");
		const lines = text.split("\n");
		lines.forEach((line, i) => {
			// Use the same word-boundary regex used by rules.
			// Anything matching any of our patterns shouldn't be present
			// after apply (excluding the exempted file).
			if (/\bHAL\b|\bHAL_|\bHal_|\bhalVk_|hal_|\bhal[A-Z]|_HAL_|_HAL\b|_Hal_|_Hal[A-Z]/.test(line)) {
				hits.push(`${f}:${i + 1}: ${line.trim().slice(0, 100)}`);
			}
		});
	}
	if (hits.length === 0) {
		console.log("\n  ✓ check pass: zero HAL references in scoped files.");
	} else {
		console.log(`\n  ✗ check FAIL: ${hits.length} remaining HAL references:`);
		hits.slice(0, 30).forEach((h) => console.log(`    ${h}`));
		process.exit(1);
	}
}

console.log(`HAL → RAL rename (${ARG_APPLY ? "APPLY" : ARG_CHECK ? "CHECK" : "DRY-RUN"})`);
console.log(`Files to process: ${FILES.length}`);
console.log("Per-file change counts:");

for (const f of FILES) {
	processFile(f);
}

console.log(`---`);
console.log(`Files changed: ${filesChanged}`);
console.log(`Total substitutions: ${totalChanges}`);

if (ARG_CHECK) {
	checkRemainingHal();
}
