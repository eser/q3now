// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Metal-shaped consumer for the immutable offline MSL corpus. The backend gets
// canonical MSL plus portable RAL ABI and never invokes an online compiler.

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { ralShaderArtifactDigest } from '../vulkan/renderer/shaders/shader_artifact_catalog.mjs';

const DEFAULT_DIR = fileURLToPath(new URL('../vulkan/renderer/shaders/portable/', import.meta.url));
function hex64(value) { return value.toString(16).padStart(16, '0'); }
function digestJson(value) { return { lane0: hex64(value.lane0), lane1: hex64(value.lane1) }; }
function exact(a, b) { return a?.lane0 === b?.lane0 && a?.lane1 === b?.lane1; }

export function openMetalShaderCatalog(directory = DEFAULT_DIR) {
	const translationText = readFileSync(join(directory, 'translation_catalog.json'), 'utf8');
	const translation = JSON.parse(translationText);
	const manifestPath = join(dirname(directory), 'spirv', 'ral_shader_portable_manifest.json');
	const manifestText = readFileSync(manifestPath, 'utf8');
	const manifest = JSON.parse(manifestText);
	const manifestDigest = digestJson(ralShaderArtifactDigest(Buffer.from(manifestText)));
	if (translation.schemaVersion !== 2 || manifest.schemaVersion !== 1
			|| !exact(translation.portableManifest?.digest, manifestDigest)
			|| translation.portableManifest.byteCount !== Buffer.byteLength(manifestText)
			|| translation.entries.length !== manifest.entries.length)
		throw new Error('MSL/portable manifest identity mismatch');
	const modules = new Map();
	for (let i = 0; i < translation.entries.length; ++i) {
		const translated = translation.entries[i]; const portable = manifest.entries[i];
		const artifact = translated.artifacts.find((item) => item.target === 'msl');
		if (translated.ordinal !== i || portable.ordinal !== i || translated.symbol !== portable.symbol
				|| translated.stage !== portable.stage || !artifact)
			throw new Error(`noncanonical MSL catalog row: ${i}`);
		const bytes = readFileSync(join(directory, `${translated.symbol}.msl`));
		if (artifact.byteCount !== bytes.byteLength
				|| !exact(artifact.digest, digestJson(ralShaderArtifactDigest(bytes))))
			throw new Error(`stale MSL artifact: ${translated.symbol}`);
		const code = bytes.toString('utf8');
		if (!code || /#\s*include\s*[<\"]vulkan\//i.test(code) || /\bVk[A-Z_a-z0-9]*\b/.test(code))
			throw new Error(`non-Metal shader artifact: ${translated.symbol}`);
		modules.set(translated.symbol, Object.freeze({
			schemaVersion: 1, symbol: translated.symbol, stage: portable.stage,
			entryPoint: portable.reflection.entryPoint, code, codeDigest: artifact.digest,
			sourceDigest: portable.sourceDigest,
			bindings: portable.reflection.bindings,
			vertexInputs: portable.reflection.vertexInputs,
			inlineData: portable.reflection.inlineData,
			specConstants: portable.reflection.specConstants,
		}));
	}
	return Object.freeze({ schemaVersion: 1, toolchainIdentity: translation.toolchainIdentity,
		portableManifest: translation.portableManifest, moduleCount: modules.size,
		get(symbol) { const value = modules.get(symbol); if (!value) throw new Error(`unknown MSL module: ${symbol}`); return value; } });
}
