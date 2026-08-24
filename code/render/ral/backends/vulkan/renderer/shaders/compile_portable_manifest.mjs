#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Resolves every reflected lowering against the exact translated WGSL corpus.
// The result is the native-free portable module manifest consumed by canonical
// MSL/WGSL publication; no runtime compiler or Vulkan handle participates.

import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { resolveShaderModule } from './shader_manifest_compose.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const DEFAULT_REFLECTION = join(HERE, 'spirv', 'ral_shader_reflection_catalog.json');
const DEFAULT_OVERRIDES = join(HERE, 'spirv', 'ral_shader_portability_overrides.json');
const DEFAULT_MANIFEST = join(HERE, 'spirv', 'ral_shader_portable_manifest.json');
const IQM_VEC3 = new Set(['iqm_skinning_vert_spv', 'iqm_temporal_exact3_vert_spv',
	'shadow_depth_skinned_vert_spv', 'ral_opengl_product_vert_spv']);
// These modules share the effects ring at set 1/binding 0. SPIR-V reflection
// can identify the UBO but not the host's dynamic-offset binding policy, so the
// portable catalog compiler owns that exact semantic for Vulkan and WebGPU.
const DYNAMIC_EFFECT_UBO = new Set([
	'beam_frag_spv', 'beam_vert_spv', 'ribbon_frag_spv', 'ribbon_spiral_vert_spv',
	'ribbon_vert_spv', 'sprite_frag_spv', 'sprite_vert_spv',
]);

function parseArgs(argv) {
	const result = { reflection: DEFAULT_REFLECTION, overrides: DEFAULT_OVERRIDES,
		manifest: DEFAULT_MANIFEST, check: false };
	for (let i = 0; i < argv.length; ++i) {
		if (argv[i] === '--check') result.check = true;
		else if (['--reflection', '--translation-dir', '--overrides', '--manifest'].includes(argv[i])) {
			if (++i >= argv.length) throw new Error(`missing value for ${argv[i - 1]}`);
			result[argv[i - 1].slice(2).replace('-dir', 'Dir')] = argv[i];
		} else throw new Error(`unknown argument: ${argv[i]}`);
	}
	if (!result.translationDir) throw new Error('--translation-dir is required');
	return result;
}

function digestExact(a, b) { return a?.lane0 === b?.lane0 && a?.lane1 === b?.lane1; }
function key(set, binding) { return `${set}:${binding}`; }

function wgslBindings(source) {
	const result = new Map();
	const pattern = /@group\((\d+)\)\s*@binding\((\d+)\)\s*\n?\s*var(?:<[^>]+>)?\s+[A-Za-z_][A-Za-z0-9_]*\s*:\s*([^;]+);/g;
	let match;
	while ((match = pattern.exec(source)) !== null) {
		const type = match[3];
		result.set(key(Number(match[1]), Number(match[2])), type.includes('sampler_comparison')
			? 'RAL_SHADER_BIND_COMPARISON_SAMPLER'
			: type.includes('sampler') ? 'RAL_SHADER_BIND_FILTERING_SAMPLER' : null);
	}
	return result;
}

function vertexFormat(symbol, input) {
	if (input.name === 'in_bone_indices') return 'RAL_FORMAT_R8G8B8A8_UINT';
	if (input.name.includes('color')) return 'RAL_FORMAT_R8G8B8A8_UNORM';
	if (input.shaderType === 'float') return 'RAL_FORMAT_R32_SFLOAT';
	if (input.shaderType === 'vec2') return 'RAL_FORMAT_R32G32_SFLOAT';
	if (input.shaderType === 'vec4') return 'RAL_FORMAT_R32G32B32A32_SFLOAT';
	if (input.shaderType === 'vec3' && ['in_position', 'in_normal'].includes(input.name))
		return IQM_VEC3.has(symbol) ? 'RAL_FORMAT_R32G32B32_SFLOAT'
			: 'RAL_FORMAT_R32G32B32A32_SFLOAT';
	throw new Error(`unmapped vertex ABI: ${symbol}:${input.location}:${input.name}:${input.shaderType}`);
}

function buildOverride(entry, wgsl) {
	const requirements = entry.reflection.loweringRequirements;
	const declarations = wgslBindings(wgsl);
	const override = { symbol: entry.symbol, spirvDigest: entry.spirv.digest };
	const vertexFormats = entry.reflection.vertexInputs.map((input) => ({
		location: input.location, format: vertexFormat(entry.symbol, input),
	}));
	if (vertexFormats.length) override.vertexFormats = vertexFormats;
	const samplerKinds = []; const arrayCounts = []; const combinedSamplers = [];
	for (const requirement of requirements) {
		const [kind, setText, bindingText] = requirement.split(':');
		const set = Number(setText); const binding = Number(bindingText);
		if (kind === 'vertex-format') continue;
		if (kind === 'sampler-kind') {
			const bindingClass = declarations.get(key(set, binding));
			if (!bindingClass) throw new Error(`WGSL sampler declaration missing: ${entry.symbol}:${set}:${binding}`);
			samplerKinds.push({ set, binding, bindingClass });
		} else if (kind === 'runtime-array') {
			const reflected = entry.reflection.bindings.find((item) => item.set === set && item.binding === binding);
			if (!reflected) throw new Error(`runtime binding missing: ${entry.symbol}:${set}:${binding}`);
			arrayCounts.push({ set, binding, arrayCount: reflected.bindingClass.includes('SAMPLER') ? 32 : 4096 });
		} else if (kind === 'combined-sampler') {
			const samplerBinding = binding + 32;
			const samplerBindingClass = declarations.get(key(set, samplerBinding));
			if (!samplerBindingClass) throw new Error(`WGSL combined split missing: ${entry.symbol}:${set}:${binding}`);
			combinedSamplers.push({ set, binding, samplerSet: set, samplerBinding, samplerBindingClass });
		} else if (!['inline-uniform-binding'].includes(kind)) {
			throw new Error(`unsupported lowering requirement: ${entry.symbol}:${requirement}`);
		}
	}
	if (samplerKinds.length) override.samplerKinds = samplerKinds;
	if (arrayCounts.length) override.arrayCounts = arrayCounts.sort((a, b) => a.set - b.set || a.binding - b.binding);
	if (combinedSamplers.length) override.combinedSamplers = combinedSamplers;
	if (DYNAMIC_EFFECT_UBO.has(entry.symbol)) {
		const binding = entry.reflection.bindings.find((item) => item.set === 1 && item.binding === 0);
		if (!binding || binding.bindingClass !== 'RAL_SHADER_BIND_UNIFORM_BUFFER'
				|| binding.arrayCount !== 1 || binding.minBufferBindingSize !== 112)
			throw new Error(`effects dynamic UBO mismatch: ${entry.symbol}`);
		override.dynamicOffsets = [{ set: 1, binding: 0 }];
	}
	if (requirements.includes('inline-uniform-binding')) {
		const occupied = new Set(entry.reflection.bindings.filter((item) => item.set === 0).map((item) => item.binding));
		for (const item of combinedSamplers) if (item.samplerSet === 0) occupied.add(item.samplerBinding);
		let binding = 0; while (occupied.has(binding)) ++binding;
		if (binding >= 64 || !wgsl.includes(`@group(0) @binding(${binding})\nvar<uniform>`))
			throw new Error(`WGSL inline uniform mismatch: ${entry.symbol}`);
		override.inlineUniform = { set: 0, binding };
	}
	return override;
}

function canonical(value) { return JSON.stringify(value, null, 2) + '\n'; }
function publish(path, text, check) {
	if (check) {
		if (!existsSync(path) || readFileSync(path, 'utf8') !== text) throw new Error(`portable catalog is stale: ${path}`);
	} else writeFileSync(path, text);
}

export function runPortableManifest(options) {
	const reflection = JSON.parse(readFileSync(options.reflection, 'utf8'));
	const translation = JSON.parse(readFileSync(join(options.translationDir, 'translation_catalog.json'), 'utf8'));
	if (reflection.schemaVersion !== 1 || reflection.entries.length !== reflection.sourceCount
			|| translation.schemaVersion !== 2 || !translation.requiredTargets.includes('wgsl')
			|| reflection.toolchainIdentity !== translation.toolchainIdentity
			|| translation.entries.length !== reflection.entries.length)
		throw new Error('reflection/translation corpus mismatch');
	const overrides = { schemaVersion: 1, generation: 1, entries: [] };
	for (let i = 0; i < reflection.entries.length; ++i) {
		const entry = reflection.entries[i]; const translated = translation.entries[i];
		if (entry.ordinal !== i || translated.ordinal !== i || entry.symbol !== translated.symbol
				|| entry.stage !== translated.stage || !digestExact(entry.spirv.digest, translated.spirv.digest))
			throw new Error(`reflection/translation identity mismatch at ${i}`);
		const wgslPath = join(options.translationDir, `${entry.symbol}.wgsl`);
		overrides.entries.push(buildOverride(entry, readFileSync(wgslPath, 'utf8')));
	}
	overrides.entries.sort((a, b) => a.symbol.localeCompare(b.symbol));
	const manifest = { schemaVersion: 1, generation: 1,
		toolchainIdentity: reflection.toolchainIdentity, sourceCount: reflection.sourceCount,
		entries: reflection.entries.map((entry) => ({
			ordinal: entry.ordinal, symbol: entry.symbol, stage: entry.stage,
			sourceDigest: entry.sourceDigest, spirv: entry.spirv,
			reflection: resolveShaderModule(entry, overrides),
		})) };
	publish(resolve(options.overrides), canonical(overrides), options.check);
	publish(resolve(options.manifest), canonical(manifest), options.check);
	return manifest.entries.length;
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
	try {
		const count = runPortableManifest(parseArgs(process.argv.slice(2)));
		console.log(`resolved ${count} portable shader modules; catalogs ${process.argv.includes('--check') ? 'fresh' : 'written'}`);
	} catch (error) { console.error(`ERROR: ${error.message}`); process.exit(1); }
}
