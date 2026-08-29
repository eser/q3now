// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { runPortableManifest } from '../code/render/ral/backends/vulkan/renderer/shaders/compile_portable_manifest.mjs';

const root = fileURLToPath(new URL('..', import.meta.url));
const reflection = join(root, 'code/render/ral/backends/vulkan/renderer/shaders/spirv/ral_shader_reflection_catalog.json');
const overrides = join(root, 'code/render/ral/backends/vulkan/renderer/shaders/spirv/ral_shader_portability_overrides.json');
const manifest = join(root, 'code/render/ral/backends/vulkan/renderer/shaders/spirv/ral_shader_portable_manifest.json');
const translationDir = join(root, 'code/render/ral/backends/vulkan/renderer/shaders/portable');
assert.equal(runPortableManifest({ reflection, overrides, manifest, translationDir, check: true }), 299);
const resolved = JSON.parse(readFileSync(manifest));
const decisions = JSON.parse(readFileSync(overrides));
assert.equal(resolved.sourceCount, 299); assert.equal(decisions.entries.length, 299);
assert.ok(resolved.entries.every((entry, i) => entry.ordinal === i
	&& entry.reflection.portable === true && entry.reflection.loweringRequirements.length === 0
	&& entry.reflection.bindings.every((binding) => !binding.bindingClass.includes('UNRESOLVED'))));
assert.equal(decisions.entries.filter((entry) => entry.inlineUniform).length, 17);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.combinedSamplers?.length ?? 0), 0), 92);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.arrayCounts?.length ?? 0), 0), 241);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.vertexFormats?.length ?? 0), 0), 449);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.dynamicOffsets?.length ?? 0), 0), 7);
assert.equal(resolved.entries.filter((entry) => entry.reflection.bindings.some((binding) => binding.dynamicOffset)).length, 7);
const particleComputeDecision = decisions.entries.find((entry) => entry.symbol === 'particle_integrate_comp_spv');
assert.deepEqual(particleComputeDecision.combinedSamplers, [{ set: 0, binding: 7,
	samplerSet: 0, samplerBinding: 39,
	samplerBindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER' }]);
const symbols = decisions.entries.map((entry) => entry.symbol);
assert.deepEqual(symbols, [...symbols].sort()); assert.equal(new Set(symbols).size, 299);
for (const [symbol, textureBinding, samplerBinding] of [
	['ribbon_frag_spv', 2, 3], ['beam_frag_spv', 1, 4],
]) {
	const decision = decisions.entries.find((entry) => entry.symbol === symbol);
	const module = resolved.entries.find((entry) => entry.symbol === symbol);
	assert.ok(decision && module);
	assert.equal(decision.combinedSamplers, undefined);
	assert.deepEqual(decision.samplerKinds, [{ set: 0, binding: samplerBinding,
		bindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER' }]);
	assert.ok(module.reflection.bindings.some((binding) => binding.set === 0
		&& binding.binding === textureBinding
		&& binding.bindingClass === 'RAL_SHADER_BIND_SAMPLED_TEXTURE'
		&& binding.arrayCount === 64));
	assert.ok(module.reflection.bindings.some((binding) => binding.set === 0
		&& binding.binding === samplerBinding
		&& binding.bindingClass === 'RAL_SHADER_BIND_FILTERING_SAMPLER'
		&& binding.arrayCount === 1));
}
console.log('RAL shader portable manifest: PASS');
