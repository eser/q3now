// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import { normalizeSpirvReflection } from '../code/render/ral/backends/vulkan/renderer/shaders/shader_reflection_abi.mjs';

const vertex = {
	entryPoints: [{ name: 'main', mode: 'vert' }],
	inputs: [{ type: 'vec3', name: 'in_position', location: 0 }],
	ubos: [{ type: '_17', name: 'UBO', block_size: 544, set: 0, binding: 0 }],
};
const normalized = normalizeSpirvReflection(vertex);
assert.equal(normalized.stage, 'RAL_STAGE_VERTEX');
assert.equal(normalized.portable, false);
assert.deepEqual(normalized.vertexInputs,
	[{ location: 0, name: 'in_position', shaderType: 'vec3', format: null }]);
assert.deepEqual(normalized.loweringRequirements, ['vertex-format:0']);
assert.equal(normalized.bindings[0].minBufferBindingSize, 544);

const compute = normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'comp', workgroup_size: [8, 8, 1] }],
	ssbos: [{ name: 'Input', block_size: 16, set: 0, binding: 0, readonly: true }],
	images: [{ name: 'Output', type: 'image2D', format: 'rgba16f', set: 0, binding: 1, writeonly: true }],
	types: { _push: { members: [{ name: 'value', type: 'vec4', offset: 0 }] } },
	push_constants: [{ name: 'Push', type: '_push' }],
	specialization_constants: [{ id: 7, type: 'uint', default_value: 4 }],
});
assert.equal(compute.portable, false);
assert.equal(compute.bindings[0].bindingClass, 'RAL_SHADER_BIND_STORAGE_BUFFER_READ');
assert.equal(compute.bindings[1].storageTextureFormat, 'RAL_FORMAT_R16G16B16A16_SFLOAT');
assert.equal(compute.inlineData.byteSize, 16);
assert.deepEqual(compute.loweringRequirements, ['inline-uniform-binding']);
assert.equal(normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'frag' }],
	inputs: [{ type: 'vec2', location: 0 }],
}).vertexInputs.length, 0);
assert.equal(normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'frag' }],
	specialization_constants: [{ id: 3, type: 'float', default_value: 0.32 }],
}).specConstants[0].defaultValue, 0x3ea3d70a);

const portableCompute = normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'comp' }],
	ssbos: [{ name: 'Input', block_size: 16, set: 0, binding: 0, readonly: true }],
});
assert.equal(portableCompute.portable, true);

const legacy = normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'frag' }],
	textures: [{ name: 'legacy', type: 'sampler2D', set: 0, binding: 0 }],
	separate_images: [{ name: 'bindless', type: 'texture2D', set: 1, binding: 0, array: [0] }],
	separate_samplers: [{ name: 'samplers', type: 'sampler', set: 1, binding: 1, array: [32] }],
});
assert.equal(legacy.portable, false);
assert.deepEqual(legacy.loweringRequirements,
	['combined-sampler:0:0', 'runtime-array:1:0', 'sampler-kind:1:1']);
assert.throws(() => normalizeSpirvReflection({ ...vertex, inputs: [...vertex.inputs, vertex.inputs[0]] }));
assert.throws(() => normalizeSpirvReflection({ ...vertex, ubos: [...vertex.ubos, vertex.ubos[0]] }));
assert.throws(() => normalizeSpirvReflection({ ...vertex, entryPoints: [] }));
assert.throws(() => normalizeSpirvReflection({ ...vertex,
	inputs: [{ type: 'mat4', location: 0 }] }));
assert.throws(() => normalizeSpirvReflection({ ...vertex,
	ubos: [{ ...vertex.ubos[0], set: 8 }] }));
assert.throws(() => normalizeSpirvReflection({ ...vertex,
	ubos: [{ ...vertex.ubos[0], binding: 64 }] }));
assert.throws(() => normalizeSpirvReflection({ ...vertex,
	ubos: [{ ...vertex.ubos[0], array: [4097] }] }));
assert.throws(() => normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'comp' }],
	push_constants: [{ name: 'TooLarge', block_size: 257 }],
}));
assert.throws(() => normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'comp' }],
	specialization_constants: [{ id: 1, default_value: 0 }, { id: 1, default_value: 1 }],
}));

const unresolvedSize = normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'comp' }],
	ssbos: [{ name: 'RuntimeBlock', block_size: 0, set: 0, binding: 0, readonly: true }],
});
assert.equal(unresolvedSize.bindings[0].minBufferBindingSize, null);
assert.deepEqual(unresolvedSize.loweringRequirements, ['buffer-min-size:0:0']);
const derivedSize = normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'comp' }],
	types: { _block: { members: [{ type: 'vec4', offset: 0 }, { type: 'float', offset: 16 }] } },
	ssbos: [{ name: 'DerivedBlock', type: '_block', set: 0, binding: 0, readonly: true }],
});
assert.equal(derivedSize.bindings[0].minBufferBindingSize, 20);
assert.equal(derivedSize.portable, true);
const runtimeMinimum = normalizeSpirvReflection({
	entryPoints: [{ name: 'main', mode: 'comp' }],
	types: { _block: { members: [{ type: 'vec4', offset: 0, array: [0], array_stride: 16 }] } },
	ssbos: [{ name: 'RuntimeBlock', type: '_block', set: 0, binding: 0, readonly: true }],
});
assert.equal(runtimeMinimum.bindings[0].minBufferBindingSize, 16);

console.log('RAL shader reflection ABI: PASS');
