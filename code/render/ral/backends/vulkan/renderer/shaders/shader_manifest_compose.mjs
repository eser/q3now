// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

const STAGE_BITS = Object.freeze({ RAL_STAGE_VERTEX: 1, RAL_STAGE_FRAGMENT: 2, RAL_STAGE_COMPUTE: 4 });
const SAMPLER_CLASSES = new Set(['RAL_SHADER_BIND_FILTERING_SAMPLER', 'RAL_SHADER_BIND_COMPARISON_SAMPLER']);
const VERTEX_FORMATS = new Set([
	'RAL_FORMAT_R32_SFLOAT', 'RAL_FORMAT_R32G32_SFLOAT', 'RAL_FORMAT_R32G32B32_SFLOAT',
	'RAL_FORMAT_R32G32B32A32_SFLOAT', 'RAL_FORMAT_R8G8B8A8_UINT', 'RAL_FORMAT_R8G8B8A8_UNORM',
]);
const OVERRIDE_KEYS = new Set(['symbol', 'spirvDigest', 'vertexFormats', 'samplerKinds',
	'arrayCounts', 'bufferMinimums', 'storageTextures', 'combinedSamplers', 'dynamicOffsets', 'inlineUniform']);

function digestExact(a, b) { return a?.lane0 === b?.lane0 && a?.lane1 === b?.lane1; }
function digestValid(value) {
	return value && /^[0-9a-f]{16}$/.test(value.lane0) && /^[0-9a-f]{16}$/.test(value.lane1)
		&& (value.lane0 !== '0000000000000000' || value.lane1 !== '0000000000000000');
}
function key(item) { return `${item.set}:${item.binding}`; }
function ordered(items, getKey) {
	for (let i = 1; i < items.length; ++i) if (getKey(items[i - 1]) >= getKey(items[i]))
		throw new Error('override rows must be unique and ordered');
}

function validateOverrideCatalog(catalog) {
	if (!catalog || catalog.schemaVersion !== 1 || !Number.isInteger(catalog.generation)
			|| catalog.generation <= 0 || !Array.isArray(catalog.entries))
		throw new Error('invalid override catalog');
	ordered(catalog.entries, (entry) => entry.symbol);
	for (const entry of catalog.entries) {
		if (typeof entry.symbol !== 'string' || !entry.symbol || !digestValid(entry.spirvDigest)
				|| Object.keys(entry).some((name) => !OVERRIDE_KEYS.has(name)))
			throw new Error('invalid override identity');
		for (const name of ['vertexFormats', 'samplerKinds', 'arrayCounts', 'bufferMinimums', 'storageTextures', 'combinedSamplers', 'dynamicOffsets']) {
			if (!Array.isArray(entry[name] ?? [])) throw new Error(`invalid ${name} override`);
		}
		ordered(entry.vertexFormats ?? [], (item) => item.location);
		for (const name of ['samplerKinds', 'arrayCounts', 'bufferMinimums', 'storageTextures', 'combinedSamplers', 'dynamicOffsets'])
			ordered(entry[name] ?? [], key);
	}
}

function removeRequirement(requirements, exact) {
	if (!requirements.delete(exact)) throw new Error(`override does not resolve ${exact}`);
}

function bindingAt(reflection, item) {
	const result = reflection.bindings.find((binding) => binding.set === item.set && binding.binding === item.binding);
	if (!result) throw new Error(`override binding is absent: ${key(item)}`);
	return result;
}

function resolveModule(entry, override) {
	const reflection = structuredClone(entry.reflection);
	const requirements = new Set(reflection.loweringRequirements);
	if (override) {
		if (!digestExact(override.spirvDigest, entry.spirv.digest)) throw new Error(`stale override: ${entry.symbol}`);
		for (const item of override.vertexFormats ?? []) {
			const input = reflection.vertexInputs.find((value) => value.location === item.location);
			if (!input || !VERTEX_FORMATS.has(item.format))
				throw new Error('invalid vertex format override');
			removeRequirement(requirements, `vertex-format:${item.location}`); input.format = item.format;
		}
		for (const item of override.samplerKinds ?? []) {
			if (!SAMPLER_CLASSES.has(item.bindingClass)) throw new Error('invalid sampler kind override');
			const binding = bindingAt(reflection, item);
			removeRequirement(requirements, `sampler-kind:${key(item)}`); binding.bindingClass = item.bindingClass;
		}
		for (const item of override.arrayCounts ?? []) {
			if (!Number.isInteger(item.arrayCount) || item.arrayCount <= 0 || item.arrayCount > 4096)
				throw new Error('invalid array count override');
			const binding = bindingAt(reflection, item);
			removeRequirement(requirements, `runtime-array:${key(item)}`); binding.arrayCount = item.arrayCount;
		}
		for (const item of override.bufferMinimums ?? []) {
			if (!Number.isSafeInteger(item.minBufferBindingSize) || item.minBufferBindingSize <= 0)
				throw new Error('invalid buffer minimum override');
			const binding = bindingAt(reflection, item);
			removeRequirement(requirements, `buffer-min-size:${key(item)}`);
			binding.minBufferBindingSize = item.minBufferBindingSize;
		}
		for (const item of override.storageTextures ?? []) {
			const binding = bindingAt(reflection, item);
			if (item.bindingClass) {
				if (!['RAL_SHADER_BIND_STORAGE_TEXTURE_READ', 'RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE'].includes(item.bindingClass))
					throw new Error('invalid storage access override');
				removeRequirement(requirements, `storage-access:${key(item)}`); binding.bindingClass = item.bindingClass;
			}
			if (item.storageTextureFormat) {
				removeRequirement(requirements, `storage-format:${key(item)}`);
				binding.storageTextureFormat = item.storageTextureFormat;
			}
		}
		for (const item of override.dynamicOffsets ?? []) {
			const binding = bindingAt(reflection, item);
			if (!['RAL_SHADER_BIND_UNIFORM_BUFFER', 'RAL_SHADER_BIND_STORAGE_BUFFER_READ',
					'RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE'].includes(binding.bindingClass)
					|| binding.arrayCount !== 1 || binding.dynamicOffset !== false)
				throw new Error('invalid dynamic offset override');
			binding.dynamicOffset = true;
		}
		for (const item of override.combinedSamplers ?? []) {
			if (!SAMPLER_CLASSES.has(item.samplerBindingClass)
					|| !Number.isInteger(item.samplerSet) || item.samplerSet < 0 || item.samplerSet >= 8
					|| !Number.isInteger(item.samplerBinding) || item.samplerBinding < 0 || item.samplerBinding >= 64)
				throw new Error('invalid combined sampler override');
			const texture = bindingAt(reflection, item);
			if (texture.bindingClass !== 'RAL_SHADER_BIND_COMBINED_SAMPLER_UNRESOLVED'
					|| reflection.bindings.some((binding) => binding.set === item.samplerSet
						&& binding.binding === item.samplerBinding))
				throw new Error('invalid combined sampler split');
			removeRequirement(requirements, `combined-sampler:${key(item)}`);
			texture.bindingClass = 'RAL_SHADER_BIND_SAMPLED_TEXTURE';
			reflection.bindings.push({ set: item.samplerSet, binding: item.samplerBinding,
				bindingClass: item.samplerBindingClass, arrayCount: texture.arrayCount,
				stageFlags: reflection.stage, minBufferBindingSize: 0, viewDimension: 0,
				sampleType: 0, storageTextureFormat: 'RAL_FORMAT_UNDEFINED', dynamicOffset: false });
		}
		if (override.inlineUniform) {
			const item = override.inlineUniform;
			if (!Number.isInteger(item.set) || item.set < 0 || item.set >= 8
					|| !Number.isInteger(item.binding) || item.binding < 0 || item.binding >= 64
					|| reflection.bindings.some((binding) => binding.set === item.set && binding.binding === item.binding))
				throw new Error('invalid inline uniform override');
			removeRequirement(requirements, 'inline-uniform-binding');
			reflection.inlineData.webgpuUniformSet = item.set;
			reflection.inlineData.webgpuUniformBinding = item.binding;
			reflection.bindings.push({ set: item.set, binding: item.binding,
				bindingClass: 'RAL_SHADER_BIND_UNIFORM_BUFFER', arrayCount: 1,
				stageFlags: reflection.stage, minBufferBindingSize: reflection.inlineData.byteSize,
				viewDimension: 0, sampleType: 0, storageTextureFormat: 'RAL_FORMAT_UNDEFINED',
				dynamicOffset: false });
		}
	}
	if (requirements.size) throw new Error(`unresolved lowering for ${entry.symbol}: ${[...requirements].sort().join(',')}`);
	reflection.bindings.sort((a, b) => a.set - b.set || a.binding - b.binding);
	reflection.portable = true;
	reflection.loweringRequirements = [];
	return reflection;
}

export function resolveShaderModule(entry, overrideCatalog) {
	validateOverrideCatalog(overrideCatalog);
	if (!entry || typeof entry.symbol !== 'string' || !digestValid(entry.spirv?.digest))
		throw new Error('invalid reflection module');
	const override = overrideCatalog.entries.find((item) => item.symbol === entry.symbol);
	return resolveModule(entry, override);
}

function bindingSemantic(binding) {
	const copy = { ...binding }; delete copy.stageFlags; return JSON.stringify(copy);
}

export function composeShaderManifest(reflectionCatalog, overrideCatalog, request) {
	validateOverrideCatalog(overrideCatalog);
	if (!reflectionCatalog || reflectionCatalog.schemaVersion !== 1 || !Array.isArray(reflectionCatalog.entries)
			|| !request || !Number.isSafeInteger(request.generation) || request.generation <= 0
			|| !Array.isArray(request.modules)) throw new Error('invalid compose request');
	for (let i = 0; i < reflectionCatalog.entries.length; ++i) {
		const entry = reflectionCatalog.entries[i];
		if (entry.ordinal !== i || !digestValid(entry.sourceDigest) || !digestValid(entry.spirv?.digest))
			throw new Error('invalid reflection catalog identity');
	}
	const source = new Map(reflectionCatalog.entries.map((entry) => [entry.symbol, entry]));
	const overrides = new Map(overrideCatalog.entries.map((entry) => [entry.symbol, entry]));
	const entries = request.modules.map((symbol) => {
		const entry = source.get(symbol); if (!entry) throw new Error(`unknown module: ${symbol}`); return entry;
	});
	const graphics = entries.length === 2 && entries[0].stage === 'RAL_STAGE_VERTEX'
		&& entries[1].stage === 'RAL_STAGE_FRAGMENT';
	const compute = entries.length === 1 && entries[0].stage === 'RAL_STAGE_COMPUTE';
	if (!graphics && !compute) throw new Error('invalid module cohort');
	const resolved = entries.map((entry) => resolveModule(entry, overrides.get(entry.symbol)));
	const bindings = [];
	for (const reflection of resolved) for (const sourceBinding of reflection.bindings) {
		const item = { ...sourceBinding, stageFlags: STAGE_BITS[reflection.stage] };
		const existing = bindings.find((binding) => binding.set === item.set && binding.binding === item.binding);
		if (!existing) bindings.push(item);
		else if (bindingSemantic(existing) !== bindingSemantic(item)) throw new Error(`conflicting binding: ${key(item)}`);
		else existing.stageFlags |= item.stageFlags;
	}
	bindings.sort((a, b) => a.set - b.set || a.binding - b.binding);
	const inlineRows = resolved.filter((reflection) => reflection.inlineData.byteSize > 0);
	let inlineData = { byteSize: 0, stageFlags: 0, webgpuUniformSet: 0, webgpuUniformBinding: 0 };
	for (const row of inlineRows) {
		const current = { ...row.inlineData, stageFlags: STAGE_BITS[row.stage] };
		if (inlineData.byteSize === 0) inlineData = current;
		else if (inlineData.byteSize !== current.byteSize
				|| inlineData.webgpuUniformSet !== current.webgpuUniformSet
				|| inlineData.webgpuUniformBinding !== current.webgpuUniformBinding)
			throw new Error('conflicting inline-data ABI');
		else inlineData.stageFlags |= current.stageFlags;
	}
	const specs = [];
	for (const reflection of resolved) for (const sourceSpec of reflection.specConstants) {
		if (sourceSpec.defaultValue === null) throw new Error('unresolved specialization default');
		const item = { ...sourceSpec, stageFlags: STAGE_BITS[reflection.stage] };
		const existing = specs.find((spec) => spec.constantId === item.constantId);
		if (!existing) specs.push(item);
		else if (existing.defaultValue !== item.defaultValue) throw new Error('conflicting specialization constant');
		else existing.stageFlags |= item.stageFlags;
	}
	specs.sort((a, b) => a.constantId - b.constantId);
	return {
		schemaVersion: 1, generation: request.generation,
		pipelineKind: graphics ? 'RAL_SHADER_PIPELINE_GRAPHICS' : 'RAL_SHADER_PIPELINE_COMPUTE',
		modules: entries.map((entry) => ({ stage: STAGE_BITS[entry.stage], entryPoint: entry.reflection.entryPoint,
			sourceDigest: entry.sourceDigest, artifacts: [
				{ target: 'RAL_SHADER_ARTIFACT_SPIRV', byteCount: entry.spirv.byteCount, digest: entry.spirv.digest },
				{ target: 'RAL_SHADER_ARTIFACT_MSL', byteCount: 0, digest: { lane0: '0000000000000000', lane1: '0000000000000000' } },
				{ target: 'RAL_SHADER_ARTIFACT_WGSL', byteCount: 0, digest: { lane0: '0000000000000000', lane1: '0000000000000000' } },
			] })),
		bindings, vertexInputs: graphics ? resolved[0].vertexInputs.map(({ location, format }) => ({ location, format })) : [],
		inlineData, specConstants: specs,
		overrideGeneration: overrideCatalog.generation,
	};
}
