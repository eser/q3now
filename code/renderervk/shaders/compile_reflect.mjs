#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Deterministic offline SPIR-V reflection catalog generation. Reflection is
// joined to the complete canonical provenance corpus before symbol selection.

import { spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import {
	parseProvenanceCatalog, parseShaderData, translatorIdentity, validateSourceCorpus,
} from './compile_xlate.mjs';
import { normalizeSpirvReflection } from './shader_reflection_abi.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const DEFAULT_SHADER_DATA = join(HERE, 'spirv', 'shader_data.c');
const DEFAULT_PROVENANCE = join(HERE, 'spirv', 'ral_shader_artifact_catalog.inc');

function parseArgs(argv) {
	const options = { symbols: [], check: false };
	for (let i = 0; i < argv.length; ++i) {
		const arg = argv[i];
		if (arg === '--check') options.check = true;
		else if (['--translator', '--shader-data', '--provenance', '--output', '--symbol'].includes(arg)) {
			if (++i >= argv.length) throw new Error(`missing value for ${arg}`);
			if (arg === '--symbol') options.symbols.push(argv[i]);
			else options[arg.slice(2)] = argv[i];
		} else throw new Error(`unknown argument: ${arg}`);
	}
	if (!options.translator || !options.output) throw new Error('--translator and --output are required');
	return options;
}

function reflect(translator, item, workDir) {
	const input = join(workDir, `${item.symbol}.spv`);
	const output = join(workDir, `${item.symbol}.json`);
	writeFileSync(input, item.bytes);
	const result = spawnSync(translator, ['--reflect-only', input, output], { encoding: 'utf8' });
	if (result.error || result.status !== 0)
		throw new Error(`reflection failed for ${item.symbol}: ${result.stderr || result.stdout || result.error}`);
	let raw;
	try { raw = JSON.parse(readFileSync(output, 'utf8')); }
	catch (error) { throw new Error(`invalid reflection JSON for ${item.symbol}: ${error.message}`); }
	let reflection;
	try { reflection = normalizeSpirvReflection(raw); }
	catch (error) { throw new Error(`normalization failed for ${item.symbol}: ${error.message}`); }
	if (reflection.stage !== item.stage)
		throw new Error(`reflection/provenance stage mismatch for ${item.symbol}`);
	return reflection;
}

export function renderReflectionCatalog(items, sourceCount, toolchainIdentity) {
		const entries = items.map((item) => ({
			ordinal: item.ordinal, symbol: item.symbol, stage: item.stage,
			sourceDigest: item.inputDigest,
			spirv: { byteCount: item.byteCount, digest: item.artifactDigest },
			reflection: item.reflection,
		}));
	return `{"schemaVersion":1,"toolchainIdentity":${JSON.stringify(toolchainIdentity)},"sourceCount":${sourceCount},"entries":[\n${entries.map((entry) => JSON.stringify(entry)).join(',\n')}\n]}\n`;
}

export function runReflectionCorpus(options) {
	const identity = translatorIdentity(options.translator);
	const rows = parseProvenanceCatalog(readFileSync(options.provenance ?? DEFAULT_PROVENANCE, 'utf8'));
	const blobs = parseShaderData(readFileSync(options['shader-data'] ?? DEFAULT_SHADER_DATA, 'utf8'));
	let source = validateSourceCorpus(rows, blobs);
	if (options.symbols.length) {
		const wanted = new Set(options.symbols);
		source = source.filter((item) => wanted.delete(item.symbol));
		if (wanted.size) throw new Error(`unknown shader symbol: ${[...wanted].join(', ')}`);
	}
	const workDir = mkdtempSync(join(tmpdir(), 'wired-shader-reflect-'));
	try {
		const reflected = source.map((item) => ({
			...item, reflection: reflect(options.translator, item, workDir),
		}));
		const text = renderReflectionCatalog(reflected, rows.length, identity);
		const output = resolve(options.output);
		if (options.check) {
			if (readFileSync(output, 'utf8') !== text) throw new Error('reflection catalog is stale');
		} else writeFileSync(output, text);
		return reflected.length;
	} finally { rmSync(workDir, { recursive: true, force: true }); }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
	try {
		const count = runReflectionCorpus(parseArgs(process.argv.slice(2)));
		console.log(`reflected ${count} shader artifact(s); catalog ${process.argv.includes('--check') ? 'fresh' : 'written'}`);
	} catch (error) {
		console.error(`ERROR: ${error.message}`); process.exit(1);
	}
}
