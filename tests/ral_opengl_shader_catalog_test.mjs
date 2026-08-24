// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { ralShaderArtifactDigest } from '../code/render/ral/backends/vulkan/renderer/shaders/shader_artifact_catalog.mjs';
import { generateProductShaderCatalog } from '../code/render/ral/backends/opengl/shaders/generate_product_shader_catalog.mjs';

const root = new URL('../code/render/ral/backends/opengl/shaders/portable/', import.meta.url).pathname;
const manifestPath = new URL('../code/render/ral/backends/vulkan/renderer/shaders/spirv/ral_shader_portable_manifest.json', import.meta.url).pathname;
const productCatalogPath = new URL('../code/render/ral/backends/opengl/ral_opengl_product_shader_catalog.inc', import.meta.url).pathname;
const text = readFileSync(join(root, 'translation_catalog.json'), 'utf8');
const catalog = JSON.parse(text);
const manifestText = readFileSync(manifestPath, 'utf8');
const manifest = JSON.parse(manifestText);
const hex = (value) => value.toString(16).padStart(16, '0');
const exact = (bytes, digest) => {
	const actual = ralShaderArtifactDigest(bytes);
	return hex(actual.lane0) === digest.lane0 && hex(actual.lane1) === digest.lane1;
};

assert.equal(catalog.schemaVersion, 2);
assert.deepEqual(catalog.requiredTargets, ['glsl460']);
assert.equal(catalog.entries.length, 294);
assert.equal(manifest.entries.length, 294);
assert.equal(catalog.portableManifest.byteCount, Buffer.byteLength(manifestText));
assert.ok(exact(Buffer.from(manifestText), catalog.portableManifest.digest));
assert.equal(readdirSync(root).filter((name) => name.endsWith('.glsl460')).length, 294);
for (let ordinal = 0; ordinal < 294; ++ordinal) {
	const entry = catalog.entries[ordinal];
	const portable = manifest.entries[ordinal];
	const artifact = entry.artifacts[0];
	const bytes = readFileSync(join(root, `${entry.symbol}.glsl460`));
	const source = bytes.toString('utf8');
	assert.equal(entry.ordinal, ordinal);
	assert.equal(entry.symbol, portable.symbol);
	assert.equal(artifact.target, 'glsl460');
	assert.equal(artifact.byteCount, bytes.length);
	assert.ok(exact(bytes, artifact.digest));
	assert.ok(source.startsWith('#version 460\n'));
	assert.doesNotMatch(source, /layout\([^)]*set\s*=|push_constant|nonuniformEXT|GL_EXT_nonuniform_qualifier|#version 3/);
}
assert.deepEqual(catalog.entries.slice(-2).map((entry) => entry.symbol), [
	'ral_opengl_product_vert_spv', 'ral_opengl_product_frag_spv',
]);
generateProductShaderCatalog({ portable: root, output: productCatalogPath, check: true });
const first = catalog.entries[0].artifacts[0];
const bad = Buffer.from(readFileSync(join(root, `${catalog.entries[0].symbol}.glsl460`)));
bad[bad.length - 1] ^= 1;
assert.equal(exact(bad, first.digest), false);
console.log('RAL OpenGL GLSL 4.60 catalog: PASS (294 modules; product artifact selected)');
