// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

const STAGES = Object.freeze({ vert: 'RAL_STAGE_VERTEX', frag: 'RAL_STAGE_FRAGMENT', comp: 'RAL_STAGE_COMPUTE' });
const VERTEX_TYPES = new Set(['float', 'vec2', 'vec3', 'vec4', 'int', 'ivec2', 'ivec3', 'ivec4',
	'uint', 'uvec2', 'uvec3', 'uvec4']);

function arrayCount(resource) {
	if (!resource.array) return 1;
	if (!Array.isArray(resource.array) || resource.array.length === 0)
		throw new Error('invalid descriptor array');
	let count = 1;
	for (const extent of resource.array) {
		if (extent === 0) return 0;
		if (!Number.isInteger(extent) || extent < 0 || count > Math.floor(4096 / extent))
			throw new Error('descriptor array exceeds portable bound');
		count *= extent;
	}
	return count;
}

function binding(resource, bindingClass, stage, extra = {}) {
	if (!Number.isInteger(resource.set) || resource.set < 0 || resource.set >= 8
			|| !Number.isInteger(resource.binding) || resource.binding < 0 || resource.binding >= 64)
		throw new Error('descriptor set/binding out of portable range');
	return {
		set: resource.set, binding: resource.binding, bindingClass,
		arrayCount: arrayCount(resource), stageFlags: stage,
		minBufferBindingSize: 0, viewDimension: 0, sampleType: 0,
		storageTextureFormat: 'RAL_FORMAT_UNDEFINED', dynamicOffset: false,
		...extra,
	};
}

function bufferBinding(raw, resource, bindingClass, stage, lowerings) {
	const size = Number.isInteger(resource.block_size) && resource.block_size > 0
		? resource.block_size : reflectedTypeByteSize(raw, resource.type);
	const result = binding(resource, bindingClass, stage,
		{ minBufferBindingSize: Number.isInteger(size) && size > 0 ? size : null });
	if (result.minBufferBindingSize === null)
		lowerings.push(`buffer-min-size:${resource.set}:${resource.binding}`);
	return result;
}

function imageShape(type = '') {
	const lower = type.toLowerCase();
	const viewDimension = lower.includes('cube') ? (lower.includes('array') ? 'RAL_SHADER_VIEW_CUBE_ARRAY' : 'RAL_SHADER_VIEW_CUBE')
		: lower.includes('3d') ? 'RAL_SHADER_VIEW_3D'
		: lower.includes('array') ? 'RAL_SHADER_VIEW_2D_ARRAY' : 'RAL_SHADER_VIEW_2D';
	const sampleType = lower.startsWith('i') ? 'RAL_SHADER_SAMPLE_SINT'
		: lower.startsWith('u') ? 'RAL_SHADER_SAMPLE_UINT' : 'RAL_SHADER_SAMPLE_FLOAT';
	return { viewDimension, sampleType };
}

function storageFormat(format) {
	const formats = {
		r8: 'RAL_FORMAT_R8_UNORM', rg8: 'RAL_FORMAT_R8G8_UNORM', rgba8: 'RAL_FORMAT_R8G8B8A8_UNORM',
		r16f: 'RAL_FORMAT_R16_SFLOAT', rg16f: 'RAL_FORMAT_R16G16_SFLOAT', rgba16f: 'RAL_FORMAT_R16G16B16A16_SFLOAT',
		r32f: 'RAL_FORMAT_R32_SFLOAT', rg32f: 'RAL_FORMAT_R32G32_SFLOAT', rgba32f: 'RAL_FORMAT_R32G32B32A32_SFLOAT',
	};
	return formats[format] ?? null;
}

function primitiveByteSize(type) {
	if (['bool', 'int', 'uint', 'float'].includes(type)) return 4;
	let match = /^(?:[biu]?vec)([2-4])$/.exec(type);
	if (match) return Number(match[1]) * 4;
	match = /^mat([2-4])(?:x([2-4]))?$/.exec(type);
	if (match) return Number(match[1]) * Number(match[2] ?? match[1]) * 4;
	return null;
}

function reflectedTypeByteSize(raw, typeName, active = new Set()) {
	const primitive = primitiveByteSize(typeName);
	if (primitive !== null) return primitive;
	const type = raw.types?.[typeName];
	if (!type || !Array.isArray(type.members) || active.has(typeName)) return null;
	active.add(typeName);
	let size = 0;
	for (const member of type.members) {
		if (!Number.isInteger(member.offset) || member.offset < 0) return null;
		let memberSize;
		if (Number.isInteger(member.array_stride) && Array.isArray(member.array)) {
			const count = member.array.reduce((value, extent) =>
				Number.isInteger(extent) && extent >= 0 ? value * Math.max(extent, 1) : 0, 1);
			memberSize = count > 0 ? member.array_stride * count : null;
		} else if (Number.isInteger(member.matrix_stride)) {
			const matrix = /^mat([2-4])(?:x[2-4])?$/.exec(member.type);
			memberSize = matrix ? member.matrix_stride * Number(matrix[1]) : null;
		} else memberSize = reflectedTypeByteSize(raw, member.type, active);
		if (!Number.isInteger(memberSize) || memberSize <= 0) return null;
		size = Math.max(size, member.offset + memberSize);
	}
	active.delete(typeName);
	return size > 0 ? size : null;
}

function specializationDefault(spec) {
	if (spec.type === 'float' && Number.isFinite(spec.default_value)) {
		const bytes = new ArrayBuffer(4); const view = new DataView(bytes);
		view.setFloat32(0, spec.default_value, true); return view.getUint32(0, true);
	}
	if (['int', 'uint', 'bool'].includes(spec.type) && Number.isInteger(spec.default_value)
			&& spec.default_value >= -2147483648 && spec.default_value <= 0xffffffff)
		return spec.default_value >>> 0;
	return null;
}

export function normalizeSpirvReflection(raw) {
	if (!raw || !Array.isArray(raw.entryPoints) || raw.entryPoints.length !== 1)
		throw new Error('exactly one SPIR-V entry point is required');
	const entry = raw.entryPoints[0]; const stage = STAGES[entry.mode];
	if (!stage || typeof entry.name !== 'string' || !entry.name) throw new Error('invalid entry point');
	const bindings = []; const lowerings = [];
	for (const resource of raw.ubos ?? []) bindings.push(bufferBinding(raw, resource,
		'RAL_SHADER_BIND_UNIFORM_BUFFER', stage, lowerings));
	for (const resource of raw.ssbos ?? []) bindings.push(bufferBinding(raw, resource,
		resource.readonly ? 'RAL_SHADER_BIND_STORAGE_BUFFER_READ' : 'RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE',
		stage, lowerings));
	for (const resource of raw.separate_images ?? []) bindings.push(binding(resource, 'RAL_SHADER_BIND_SAMPLED_TEXTURE', stage,
		imageShape(resource.type)));
	for (const resource of raw.separate_samplers ?? []) {
		bindings.push(binding(resource, 'RAL_SHADER_BIND_FILTERING_SAMPLER', stage));
		lowerings.push(`sampler-kind:${resource.set}:${resource.binding}`);
	}
	for (const resource of raw.textures ?? []) {
		bindings.push(binding(resource, 'RAL_SHADER_BIND_COMBINED_SAMPLER_UNRESOLVED', stage, imageShape(resource.type)));
		lowerings.push(`combined-sampler:${resource.set}:${resource.binding}`);
	}
	for (const resource of raw.images ?? []) {
		const format = storageFormat(resource.format);
		const exactRead = resource.readonly === true && resource.writeonly !== true;
		const exactWrite = resource.writeonly === true && resource.readonly !== true;
		const bindingClass = exactRead ? 'RAL_SHADER_BIND_STORAGE_TEXTURE_READ'
			: exactWrite ? 'RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE'
			: 'RAL_SHADER_BIND_STORAGE_TEXTURE_READ_WRITE_UNRESOLVED';
		bindings.push(binding(resource, bindingClass, stage,
			{ viewDimension: imageShape(resource.type).viewDimension,
				storageTextureFormat: format ?? 'RAL_FORMAT_UNDEFINED' }));
		if (!format) lowerings.push(`storage-format:${resource.set}:${resource.binding}`);
		if (!exactRead && !exactWrite) lowerings.push(`storage-access:${resource.set}:${resource.binding}`);
	}
	for (const item of bindings) if (item.arrayCount === 0)
		lowerings.push(`runtime-array:${item.set}:${item.binding}`);
	bindings.sort((a, b) => a.set - b.set || a.binding - b.binding);
	if (bindings.length > 64) throw new Error('too many reflected bindings');
	for (let i = 1; i < bindings.length; ++i)
		if (bindings[i - 1].set === bindings[i].set && bindings[i - 1].binding === bindings[i].binding)
			throw new Error('duplicate reflected binding');
	const vertexInputs = (stage === 'RAL_STAGE_VERTEX' ? raw.inputs ?? [] : []).map((input) => {
		if (!Number.isInteger(input.location) || input.location < 0 || input.location >= 16
				|| !VERTEX_TYPES.has(input.type))
			throw new Error('unsupported vertex input');
		lowerings.push(`vertex-format:${input.location}`);
		return { location: input.location, name: input.name ?? '', shaderType: input.type, format: null };
	}).sort((a, b) => a.location - b.location);
	for (let i = 1; i < vertexInputs.length; ++i)
		if (vertexInputs[i - 1].location === vertexInputs[i].location) throw new Error('duplicate vertex location');
	const pushes = raw.push_constants ?? [];
	const pushSize = pushes[0]
		? (Number.isInteger(pushes[0].block_size) ? pushes[0].block_size
			: reflectedTypeByteSize(raw, pushes[0].type)) : 0;
	if (pushes.length > 1 || (pushes[0] && (!Number.isInteger(pushSize) || pushSize <= 0 || pushSize > 256)))
		throw new Error('invalid push-data reflection');
	if (pushes[0]) lowerings.push('inline-uniform-binding');
	const specConstants = (raw.specialization_constants ?? []).map((spec) => ({
		constantId: spec.id, stageFlags: stage,
		defaultValue: specializationDefault(spec),
	})).sort((a, b) => a.constantId - b.constantId);
	if (specConstants.length > 32) throw new Error('too many specialization constants');
	for (const spec of specConstants) {
		if (!Number.isInteger(spec.constantId) || spec.constantId < 0 || spec.constantId >= 65536)
			throw new Error('invalid specialization id');
		if (spec.defaultValue === null) lowerings.push(`specialization-default:${spec.constantId}`);
	}
	for (let i = 1; i < specConstants.length; ++i)
		if (specConstants[i - 1].constantId === specConstants[i].constantId) throw new Error('duplicate specialization id');
	return {
		schemaVersion: 1, entryPoint: entry.name, stage,
		bindings, vertexInputs,
		inlineData: {
			byteSize: pushSize, stageFlags: pushes[0] ? stage : 0,
			webgpuUniformSet: pushes[0] ? null : 0,
			webgpuUniformBinding: pushes[0] ? null : 0,
		},
		specConstants, portable: lowerings.length === 0,
		loweringRequirements: [...new Set(lowerings)].sort(),
	};
}
