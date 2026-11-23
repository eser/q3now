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
assert.equal(runPortableManifest({ reflection, overrides, manifest, translationDir, check: true }), 370);
const resolved = JSON.parse(readFileSync(manifest));
const decisions = JSON.parse(readFileSync(overrides));
assert.equal(resolved.sourceCount, 370); assert.equal(decisions.entries.length, 370);
assert.ok(resolved.entries.every((entry, i) => entry.ordinal === i
	&& entry.reflection.portable === true && entry.reflection.loweringRequirements.length === 0
	&& entry.reflection.bindings.every((binding) => !binding.bindingClass.includes('UNRESOLVED'))));
assert.equal(decisions.entries.filter((entry) => entry.inlineUniform).length, 26);
// Twelve world IBL variants no longer sample current/previous diffuse cubes;
// atmospheric_frag gained the semantic-emitter class texture array (TASK-239).
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.combinedSamplers?.length ?? 0), 0), 148);
const iblCrossfadeDecisions = decisions.entries.filter((entry) =>
	entry.symbol.includes('_ibl') && entry.symbol.startsWith('frag_tx'));
assert.equal(iblCrossfadeDecisions.length, 12);
for (const decision of iblCrossfadeDecisions) {
	for (const binding of [3, 6])
		assert.ok(decision.combinedSamplers?.some((sampler) =>
			sampler.set === 2 && sampler.binding === binding));
	for (const binding of [2, 5])
		assert.ok(!decision.combinedSamplers?.some((sampler) =>
			sampler.set === 2 && sampler.binding === binding));
}
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.arrayCounts?.length ?? 0), 0), 321);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.vertexFormats?.length ?? 0), 0), 540);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.dynamicOffsets?.length ?? 0), 0), 7);
assert.equal(resolved.entries.filter((entry) => entry.reflection.bindings.some((binding) => binding.dynamicOffset)).length, 7);
const particleComputeDecision = decisions.entries.find((entry) => entry.symbol === 'particle_integrate_comp_spv');
assert.deepEqual(particleComputeDecision.combinedSamplers, [{ set: 0, binding: 7,
	samplerSet: 0, samplerBinding: 39,
	samplerBindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER' }]);
const atmosphereCompositeDecision = decisions.entries.find((entry) =>
	entry.symbol === 'atmosphere_composite_comp_spv');
assert.ok(atmosphereCompositeDecision?.combinedSamplers?.some((binding) =>
	binding.set === 0 && binding.binding === 5 && binding.samplerSet === 0
	&& binding.samplerBinding === 37
	&& binding.samplerBindingClass === 'RAL_SHADER_BIND_FILTERING_SAMPLER'));
const skySurfaceCompositeDecision = decisions.entries.find((entry) =>
	entry.symbol === 'sky_surface_composite_comp_spv');
assert.equal(skySurfaceCompositeDecision?.combinedSamplers?.length, 5);
const symbols = decisions.entries.map((entry) => entry.symbol);
assert.deepEqual(symbols, [...symbols].sort()); assert.equal(new Set(symbols).size, 370);
// Six outer faces and eight authored stages fit the existing RAL/WebGPU
// binding budgets, including the portable lowering of push constants.
const capture = resolved.entries.find((entry) => entry.symbol === 'ral_sky_capture_frag_spv');
assert.ok(capture);
const captureBindings = capture.reflection.bindings;
assert.deepEqual([0, 1, 2].map((set) => captureBindings.filter((b) => b.set === set).length), [10, 16, 1]);
assert.equal(captureBindings.filter((b) => b.bindingClass === 'RAL_SHADER_BIND_SAMPLED_TEXTURE').length, 15);
assert.equal(captureBindings.filter((b) => b.bindingClass === 'RAL_SHADER_BIND_FILTERING_SAMPLER').length, 10);
assert.equal(captureBindings.find((b) => b.set === 2 && b.binding === 1)?.minBufferBindingSize, 3616);
assert.deepEqual(capture.reflection.inlineData, {
	byteSize: 256, stageFlags: 'RAL_STAGE_FRAGMENT', webgpuUniformSet: 0, webgpuUniformBinding: 9,
});
const skySh = resolved.entries.find((entry) => entry.symbol === 'project_sky_sh_comp_spv');
assert.equal(skySh.reflection.bindings[2].bindingClass,
	'RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE');
assert.equal(skySh.reflection.bindings[2].minBufferBindingSize, 144);
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
