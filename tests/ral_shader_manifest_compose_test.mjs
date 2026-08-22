// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';
import { composeShaderManifest } from '../code/renderervk/shaders/shader_manifest_compose.mjs';

const root = fileURLToPath(new URL('..', import.meta.url));
const reflection = JSON.parse(readFileSync(join(root, 'code/renderervk/shaders/spirv/ral_shader_reflection_catalog.json')));
const overrides = JSON.parse(readFileSync(join(root, 'code/renderervk/shaders/spirv/ral_shader_portability_overrides.json')));

const graphics = composeShaderManifest(reflection, overrides,
	{ generation: 1, modules: ['color_vert_spv', 'color_frag_spv'] });
assert.equal(graphics.pipelineKind, 'RAL_SHADER_PIPELINE_GRAPHICS');
assert.deepEqual(graphics.vertexInputs, [{ location: 0, format: 'RAL_FORMAT_R32G32B32A32_SFLOAT' }]);
assert.equal(graphics.bindings[0].stageFlags, 1);
assert.equal(graphics.specConstants[0].constantId, 4);

const portableCompute = composeShaderManifest(reflection, overrides,
	{ generation: 2, modules: ['brdf_lut_comp_spv'] });
assert.equal(portableCompute.pipelineKind, 'RAL_SHADER_PIPELINE_COMPUTE');
assert.equal(portableCompute.bindings[0].bindingClass, 'RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE');

const pushCompute = composeShaderManifest(reflection, overrides,
	{ generation: 3, modules: ['hdr_histogram_comp_spv'] });
assert.equal(pushCompute.inlineData.byteSize, 8);
assert.equal(pushCompute.inlineData.webgpuUniformBinding, 3);
assert.equal(pushCompute.bindings.length, 4);
assert.equal(pushCompute.bindings[3].bindingClass, 'RAL_SHADER_BIND_UNIFORM_BUFFER');

const splitCombined = composeShaderManifest(reflection, overrides,
	{ generation: 4, modules: ['atmospheric_vert_spv', 'atmospheric_frag_spv'] });
assert.equal(splitCombined.bindings.find((binding) => binding.set === 0 && binding.binding === 2)
	.bindingClass, 'RAL_SHADER_BIND_SAMPLED_TEXTURE');
assert.equal(splitCombined.bindings.find((binding) => binding.set === 0 && binding.binding === 34)
	.bindingClass, 'RAL_SHADER_BIND_FILTERING_SAMPLER');

const dynamicEffects = composeShaderManifest(reflection, overrides,
	{ generation: 5, modules: ['beam_vert_spv', 'beam_frag_spv'] });
assert.equal(dynamicEffects.bindings.find((binding) => binding.set === 1 && binding.binding === 0)
	.dynamicOffset, true);

assert.throws(() => composeShaderManifest(reflection, { ...overrides, entries: [] },
	{ generation: 1, modules: ['color_vert_spv', 'color_frag_spv'] }), /unresolved lowering/);
assert.throws(() => composeShaderManifest(reflection, overrides,
	{ generation: 1, modules: ['bloom_frag_spv'] }), /invalid module cohort|unresolved lowering/);
const stale = structuredClone(overrides);
stale.entries.find((entry) => entry.symbol === 'color_vert_spv').spirvDigest.lane0 = '0000000000000001';
assert.throws(() => composeShaderManifest(reflection, stale,
	{ generation: 1, modules: ['color_vert_spv', 'color_frag_spv'] }), /stale override/);
const unordered = structuredClone(overrides); unordered.entries.reverse();
assert.throws(() => composeShaderManifest(reflection, unordered,
	{ generation: 1, modules: ['color_vert_spv', 'color_frag_spv'] }), /ordered/);
const duplicateLocation = structuredClone(overrides);
const duplicateVertex = duplicateLocation.entries.find((entry) => entry.symbol === 'color_vert_spv');
duplicateVertex.vertexFormats.push(duplicateVertex.vertexFormats[0]);
assert.throws(() => composeShaderManifest(reflection, duplicateLocation,
	{ generation: 1, modules: ['color_vert_spv', 'color_frag_spv'] }), /ordered/);
const unknownField = structuredClone(overrides); unknownField.entries[0].vertexFormat = [];
assert.throws(() => composeShaderManifest(reflection, unknownField,
	{ generation: 1, modules: ['color_vert_spv', 'color_frag_spv'] }), /identity/);
const invalidFormat = structuredClone(overrides);
invalidFormat.entries.find((entry) => entry.symbol === 'color_vert_spv')
	.vertexFormats[0].format = 'RAL_FORMAT_NOT_REAL';
assert.throws(() => composeShaderManifest(reflection, invalidFormat,
	{ generation: 1, modules: ['color_vert_spv', 'color_frag_spv'] }), /vertex format/);
const invalidDynamic = structuredClone(overrides);
invalidDynamic.entries.find((entry) => entry.symbol === 'ribbon_frag_spv')
	.dynamicOffsets[0] = { set: 0, binding: 2 };
assert.throws(() => composeShaderManifest(reflection, invalidDynamic,
	{ generation: 1, modules: ['ribbon_vert_spv', 'ribbon_frag_spv'] }), /dynamic offset/);

const syntheticDigest = { lane0: '1111111111111111', lane1: '2222222222222222' };
const syntheticReflection = { schemaVersion: 1, sourceCount: 1, entries: [{
	ordinal: 0, symbol: 'synthetic_comp_spv', stage: 'RAL_STAGE_COMPUTE',
	sourceDigest: { lane0: '3333333333333333', lane1: '4444444444444444' },
	spirv: { byteCount: 16, digest: syntheticDigest }, reflection: {
		schemaVersion: 1, entryPoint: 'main', stage: 'RAL_STAGE_COMPUTE',
		bindings: [
			{ set: 0, binding: 0, bindingClass: 'RAL_SHADER_BIND_STORAGE_BUFFER_READ', arrayCount: 1,
				stageFlags: 'RAL_STAGE_COMPUTE', minBufferBindingSize: null, viewDimension: 0,
				sampleType: 0, storageTextureFormat: 'RAL_FORMAT_UNDEFINED', dynamicOffset: false },
			{ set: 0, binding: 1, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 0,
				stageFlags: 'RAL_STAGE_COMPUTE', minBufferBindingSize: 0, viewDimension: 'RAL_SHADER_VIEW_2D',
				sampleType: 'RAL_SHADER_SAMPLE_FLOAT', storageTextureFormat: 'RAL_FORMAT_UNDEFINED', dynamicOffset: false },
			{ set: 0, binding: 2, bindingClass: 'RAL_SHADER_BIND_STORAGE_TEXTURE_READ_WRITE_UNRESOLVED', arrayCount: 1,
				stageFlags: 'RAL_STAGE_COMPUTE', minBufferBindingSize: 0, viewDimension: 'RAL_SHADER_VIEW_2D',
				sampleType: 0, storageTextureFormat: 'RAL_FORMAT_UNDEFINED', dynamicOffset: false },
		], vertexInputs: [], inlineData: { byteSize: 0, stageFlags: 0, webgpuUniformSet: 0, webgpuUniformBinding: 0 },
		specConstants: [], portable: false,
		loweringRequirements: ['buffer-min-size:0:0', 'runtime-array:0:1', 'storage-access:0:2', 'storage-format:0:2'],
	},
}] };
const syntheticOverrides = { schemaVersion: 1, generation: 1, entries: [{
	symbol: 'synthetic_comp_spv', spirvDigest: syntheticDigest,
	arrayCounts: [{ set: 0, binding: 1, arrayCount: 8 }],
	bufferMinimums: [{ set: 0, binding: 0, minBufferBindingSize: 64 }],
	storageTextures: [{ set: 0, binding: 2, bindingClass: 'RAL_SHADER_BIND_STORAGE_TEXTURE_READ',
		storageTextureFormat: 'RAL_FORMAT_R16G16B16A16_SFLOAT' }],
}] };
const synthetic = composeShaderManifest(syntheticReflection, syntheticOverrides,
	{ generation: 5, modules: ['synthetic_comp_spv'] });
assert.equal(synthetic.bindings[0].minBufferBindingSize, 64);
assert.equal(synthetic.bindings[1].arrayCount, 8);
assert.equal(synthetic.bindings[2].bindingClass, 'RAL_SHADER_BIND_STORAGE_TEXTURE_READ');
assert.equal(synthetic.bindings[2].storageTextureFormat, 'RAL_FORMAT_R16G16B16A16_SFLOAT');
const overCapacity = structuredClone(syntheticOverrides); overCapacity.entries[0].arrayCounts[0].arrayCount = 4097;
assert.throws(() => composeShaderManifest(syntheticReflection, overCapacity,
	{ generation: 4, modules: ['synthetic_comp_spv'] }), /array count/);

const conflictingReflection = structuredClone(reflection);
const colorFragment = conflictingReflection.entries.find((entry) => entry.symbol === 'color_frag_spv');
colorFragment.reflection.bindings.push({ ...graphics.bindings[0], bindingClass: 'RAL_SHADER_BIND_STORAGE_BUFFER_READ' });
assert.throws(() => composeShaderManifest(conflictingReflection, overrides,
	{ generation: 1, modules: ['color_vert_spv', 'color_frag_spv'] }), /conflicting binding/);

console.log('RAL shader manifest compose: PASS');
