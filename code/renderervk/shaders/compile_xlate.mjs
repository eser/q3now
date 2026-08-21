#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Deterministic offline SPIR-V corpus translation. No runtime compiler path.

import { spawnSync } from 'node:child_process';
import {
	existsSync, mkdirSync, mkdtempSync, readFileSync,
	readdirSync, rmSync, writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { ralShaderArtifactDigest } from './shader_artifact_catalog.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const DEFAULT_SHADER_DATA = join(HERE, 'spirv', 'shader_data.c');
const DEFAULT_CATALOG = join(HERE, 'spirv', 'ral_shader_artifact_catalog.inc');
const DEFAULT_PORTABLE_MANIFEST = join(HERE, 'spirv', 'ral_shader_portable_manifest.json');
const TARGETS = ['msl', 'glsl430', 'glsles300', 'wgsl'];

function hex64(value) { return value.toString(16).padStart(16, '0'); }
function digestJson(digest) { return { lane0: hex64(digest.lane0), lane1: hex64(digest.lane1) }; }
function digestExactHex(digest, lane0, lane1) {
	return hex64(digest.lane0) === lane0.toLowerCase().padStart(16, '0')
		&& hex64(digest.lane1) === lane1.toLowerCase().padStart(16, '0');
}

export function parseProvenanceCatalog(text) {
	const rows = [];
	const re = /RAL_SHADER_SPIRV_ARTIFACT\((\d+)u,\s*([A-Za-z_][A-Za-z0-9_]*),\s*(RAL_STAGE_(?:VERTEX|FRAGMENT|COMPUTE)),\s*(\d+)u,\s*0x([0-9a-fA-F]{16})ull,\s*0x([0-9a-fA-F]{16})ull,\s*0x([0-9a-fA-F]{16})ull,\s*0x([0-9a-fA-F]{16})ull\)/g;
	let match;
	while ((match = re.exec(text)) !== null) rows.push({
		ordinal: Number(match[1]), symbol: match[2], stage: match[3], byteCount: Number(match[4]),
		inputDigest: { lane0: match[5].toLowerCase(), lane1: match[6].toLowerCase() },
		artifactDigest: { lane0: match[7].toLowerCase(), lane1: match[8].toLowerCase() },
	});
	if (!rows.length || rows.length > 4096 || rows.some((row, i) => row.ordinal !== i)
			|| new Set(rows.map((row) => row.symbol)).size !== rows.length)
		throw new Error('invalid or noncanonical SPIR-V provenance catalog');
	return rows;
}

export function parseShaderData(text) {
	const blobs = [];
	const re = /const\s+unsigned\s+char\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*(\d+)\s*\]\s*=\s*\{([\s\S]*?)\}\s*;/g;
	let match;
	while ((match = re.exec(text)) !== null) {
		const bytes = Uint8Array.from(match[3].split(',').map((part) => part.trim())
			.filter(Boolean).map((part) => Number.parseInt(part, 16)));
		if (bytes.byteLength !== Number(match[2])) throw new Error(`SPIR-V byte count drift: ${match[1]}`);
		blobs.push({ symbol: match[1], bytes });
	}
	if (!blobs.length || blobs.length > 4096) throw new Error('invalid shader_data.c corpus');
	return blobs;
}

export function validateSourceCorpus(rows, blobs) {
	if (rows.length !== blobs.length) throw new Error('catalog/shader corpus count mismatch');
	return rows.map((row, i) => {
		const blob = blobs[i]; const digest = ralShaderArtifactDigest(blob.bytes);
		if (row.symbol !== blob.symbol || row.byteCount !== blob.bytes.byteLength
				|| !digestExactHex(digest, row.artifactDigest.lane0, row.artifactDigest.lane1))
			throw new Error(`catalog/shader corpus identity mismatch at ${i}`);
		return { ...row, bytes: blob.bytes };
	});
}

function parseTargets(value) {
	const targets = value.split(',');
	if (!targets.length || targets.some((target) => !TARGETS.includes(target))
			|| new Set(targets).size !== targets.length
			|| targets.some((target, index) => TARGETS.indexOf(target) <= (index ? TARGETS.indexOf(targets[index - 1]) : -1)))
		throw new Error('targets must be a unique canonical subset of msl,glsl430,glsles300,wgsl');
	return targets;
}

function parseArgs(argv) {
	const options = { symbols: [], requireWgsl: false, check: false,
		targets: ['msl', 'glsl430', 'glsles300'] };
	for (let i = 0; i < argv.length; ++i) {
		const arg = argv[i];
		if (arg === '--require-wgsl') options.requireWgsl = true;
		else if (arg === '--check') options.check = true;
		else if (['--translator', '--shader-data', '--catalog', '--portable-manifest', '--output-dir', '--symbol', '--targets'].includes(arg)) {
			if (++i >= argv.length) throw new Error(`missing value for ${arg}`);
			const key = arg === '--output-dir' ? 'outputDir' : arg.slice(2).replace(/-([a-z])/g, (_, c) => c.toUpperCase());
			if (arg === '--symbol') options.symbols.push(argv[i]);
			else if (arg === '--targets') options.targets = parseTargets(argv[i]);
			else options[key] = argv[i];
		} else throw new Error(`unknown argument: ${arg}`);
	}
	if (!options.translator || !options.outputDir) throw new Error('--translator and --output-dir are required');
	if (options.requireWgsl && !options.targets.includes('wgsl'))
		options.targets = parseTargets([...options.targets, 'wgsl'].join(','));
	return options;
}

function digestHexExact(a, b) { return a?.lane0 === b?.lane0 && a?.lane1 === b?.lane1; }
function validatePortableManifest(text, source) {
	let manifest;
	try { manifest = JSON.parse(text); } catch { throw new Error('invalid portable manifest JSON'); }
	if (manifest.schemaVersion !== 1 || !Number.isSafeInteger(manifest.generation) || manifest.generation <= 0
			|| manifest.sourceCount !== source.length || !Array.isArray(manifest.entries)
			|| manifest.entries.length !== source.length)
		throw new Error('portable manifest corpus mismatch');
	for (let i = 0; i < source.length; ++i) {
		const row = source[i]; const entry = manifest.entries[i];
		if (entry.ordinal !== i || entry.symbol !== row.symbol || entry.stage !== row.stage
				|| !digestHexExact(entry.sourceDigest, row.inputDigest)
				|| entry.spirv?.byteCount !== row.byteCount
				|| !digestHexExact(entry.spirv?.digest, row.artifactDigest)
				|| entry.reflection?.portable !== true
				|| !Array.isArray(entry.reflection?.loweringRequirements)
				|| entry.reflection.loweringRequirements.length !== 0
				|| entry.reflection.bindings?.some((binding) => String(binding.bindingClass).includes('UNRESOLVED'))
				|| entry.reflection.vertexInputs?.some((input) => !input.format))
			throw new Error(`portable manifest identity mismatch at ${i}`);
	}
	return { manifest, bytes: Buffer.from(text), digest: ralShaderArtifactDigest(Buffer.from(text)) };
}

export function translatorIdentity(translator) {
	const result = spawnSync(translator, ['--version'], { encoding: 'utf8' });
	if (result.error || result.status !== 0) throw new Error('translator identity query failed');
	const identity = result.stdout.trim();
	if (!/^wired-shader-xlate\/2 spirv-cross\/[0-9a-f]{40} naga\/30\.0\.0\+wired-portable-v1$/.test(identity))
		throw new Error(`invalid or unpinned translator identity: ${identity}`);
	return identity;
}

function runTranslator(translator, item, workDir, targets, requireWgsl) {
	const spv = join(workDir, `${item.symbol}.spv`);
	writeFileSync(spv, item.bytes);
	const args = [...(requireWgsl ? ['--require-wgsl'] : []), '--targets', targets.join(','), spv, workDir];
	const result = spawnSync(translator, args, { encoding: 'utf8' });
	if (result.error || result.status !== 0)
		throw new Error(`translator failed for ${item.symbol}: ${result.stderr || result.stdout || result.error}`);
	const artifacts = [];
	for (const target of targets) {
		const path = join(workDir, `${item.symbol}.${target}`);
		if (!existsSync(path)) {
			throw new Error(`translator omitted required ${target}: ${item.symbol}`);
		}
		const bytes = readFileSync(path);
		if (!bytes.length) throw new Error(`translator emitted empty ${target}: ${item.symbol}`);
		artifacts.push({ target, bytes, digest: ralShaderArtifactDigest(bytes) });
	}
	return artifacts;
}

function renderTranslationCatalog(items, requiredTargets, toolchainIdentity, portable) {
	return JSON.stringify({
		schemaVersion: 2,
		toolchainIdentity,
		portableManifest: { schemaVersion: portable.manifest.schemaVersion,
			generation: portable.manifest.generation, byteCount: portable.bytes.byteLength,
			digest: digestJson(portable.digest) },
		requiredTargets,
		entries: items.map((item) => ({
			ordinal: item.ordinal, symbol: item.symbol, stage: item.stage,
			spirv: { byteCount: item.byteCount, digest: item.artifactDigest },
			artifacts: item.artifacts.map((artifact) => ({
				target: artifact.target, byteCount: artifact.bytes.byteLength,
				digest: digestJson(artifact.digest),
			})),
		})),
	}, null, 2) + '\n';
}

function compareOrWrite(outputDir, translated, catalogText, check) {
	const expected = new Map([['translation_catalog.json', Buffer.from(catalogText)]]);
	for (const item of translated) for (const artifact of item.artifacts)
		expected.set(`${item.symbol}.${artifact.target}`, Buffer.from(artifact.bytes));
	if (check) {
		if (!existsSync(outputDir)) throw new Error('translation output directory is missing');
		const actualNames = readdirSync(outputDir).sort();
		const expectedNames = [...expected.keys()].sort();
		if (JSON.stringify(actualNames) !== JSON.stringify(expectedNames)) throw new Error('translation output file set is stale');
		for (const [name, bytes] of expected) {
			const actual = readFileSync(join(outputDir, name));
			if (!actual.equals(bytes)) throw new Error(`stale translated artifact: ${name}`);
		}
		return;
	}
	mkdirSync(outputDir, { recursive: true });
	const extras = readdirSync(outputDir).filter((name) => !expected.has(name));
	if (extras.length) throw new Error(`unexpected files in output directory: ${extras.join(', ')}`);
	for (const [name, bytes] of expected) writeFileSync(join(outputDir, name), bytes);
}

export function runCorpus(options) {
	const rows = parseProvenanceCatalog(readFileSync(options.catalog ?? DEFAULT_CATALOG, 'utf8'));
	const blobs = parseShaderData(readFileSync(options.shaderData ?? DEFAULT_SHADER_DATA, 'utf8'));
	let items = validateSourceCorpus(rows, blobs);
	const portable = validatePortableManifest(readFileSync(
		options.portableManifest ?? DEFAULT_PORTABLE_MANIFEST, 'utf8'), items);
	if (options.symbols.length) {
		const wanted = new Set(options.symbols);
		items = items.filter((item) => wanted.delete(item.symbol));
		if (wanted.size) throw new Error(`unknown shader symbol: ${[...wanted].join(', ')}`);
	}
	const workDir = mkdtempSync(join(tmpdir(), 'wired-shader-xlate-'));
	try {
		const identity = translatorIdentity(options.translator);
		const translated = items.map((item) => ({
			...item, artifacts: runTranslator(options.translator, item, workDir,
				options.targets, options.requireWgsl),
		}));
		const catalogText = renderTranslationCatalog(translated, options.targets, identity, portable);
		compareOrWrite(resolve(options.outputDir), translated, catalogText, options.check);
		return translated.length;
	} finally { rmSync(workDir, { recursive: true, force: true }); }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
	try {
		const options = parseArgs(process.argv.slice(2));
		const count = runCorpus(options);
		console.log(`translated ${count} shader artifact(s); output ${options.check ? 'fresh' : 'written'}`);
	} catch (error) {
		console.error(`ERROR: ${error.message}`); process.exit(1);
	}
}
