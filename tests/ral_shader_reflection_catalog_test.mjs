// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import { chmodSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseProvenanceCatalog } from '../code/render/ral/backends/vulkan/renderer/shaders/compile_xlate.mjs';
import { runReflectionCorpus } from '../code/render/ral/backends/vulkan/renderer/shaders/compile_reflect.mjs';
import { ralShaderArtifactDigest } from '../code/render/ral/backends/vulkan/renderer/shaders/shader_artifact_catalog.mjs';

const dir = mkdtempSync(join(tmpdir(), 'wired-reflection-host-'));
const shaderData = join(dir, 'shader_data.c');
const provenance = join(dir, 'provenance.inc');
const output = join(dir, 'reflection.json');
const translator = join(dir, 'fake-translator.mjs');
const bytes = Uint8Array.from([3, 2, 35, 7, 1, 0, 0, 0]);
const digest = ralShaderArtifactDigest(bytes);
const hex = (value) => value.toString(16).padStart(16, '0');

const root = fileURLToPath(new URL('..', import.meta.url));
const committed = JSON.parse(readFileSync(join(root,
	'code/render/ral/backends/vulkan/renderer/shaders/spirv/ral_shader_reflection_catalog.json'), 'utf8'));
const committedProvenance = parseProvenanceCatalog(readFileSync(join(root,
	'code/render/ral/backends/vulkan/renderer/shaders/spirv/ral_shader_artifact_catalog.inc'), 'utf8'));
assert.equal(committed.schemaVersion, 1);
assert.match(committed.toolchainIdentity, /naga\/30\.0\.0\+wired-portable-v1$/);
assert.equal(committed.sourceCount, 299);
assert.equal(committed.entries.length, committedProvenance.length);
for (let i = 0; i < committed.entries.length; ++i) {
	const entry = committed.entries[i]; const provenanceRow = committedProvenance[i];
	assert.equal(entry.ordinal, i); assert.equal(entry.symbol, provenanceRow.symbol);
	assert.equal(entry.stage, provenanceRow.stage); assert.equal(entry.spirv.byteCount, provenanceRow.byteCount);
	assert.deepEqual(entry.sourceDigest, provenanceRow.inputDigest);
	assert.deepEqual(entry.spirv.digest, provenanceRow.artifactDigest);
	assert.equal(entry.reflection.schemaVersion, 1); assert.equal(entry.reflection.stage, entry.stage);
}
const productionVertex = committed.entries.find((entry) => entry.symbol === 'color_vert_spv');
const productionCompute = committed.entries.find((entry) => entry.symbol === 'brdf_lut_comp_spv');
const productionPushCompute = committed.entries.find((entry) => entry.symbol === 'hdr_histogram_comp_spv');
assert.deepEqual(productionVertex.reflection.loweringRequirements, ['vertex-format:0']);
assert.equal(productionVertex.reflection.bindings[0].minBufferBindingSize, 544);
assert.equal(productionCompute.reflection.portable, true);
assert.equal(productionCompute.reflection.bindings[0].storageTextureFormat, 'RAL_FORMAT_R16G16_SFLOAT');
assert.deepEqual(productionPushCompute.reflection.loweringRequirements,
	['inline-uniform-binding', 'sampler-kind:0:1']);

writeFileSync(shaderData, `const unsigned char fake_comp_spv[8] = { ${[...bytes].map((v) => `0x${v.toString(16).padStart(2, '0')}`).join(', ')} };\n`);
writeFileSync(provenance,
	`RAL_SHADER_SPIRV_ARTIFACT(0u, fake_comp_spv, RAL_STAGE_COMPUTE, 8u, 0x1111111111111111ull, 0x2222222222222222ull, 0x${hex(digest.lane0)}ull, 0x${hex(digest.lane1)}ull)\n`);
writeFileSync(translator, `#!/usr/bin/env node
import { writeFileSync } from 'node:fs';
if (process.argv[2] === '--version') { console.log('wired-shader-xlate/2 spirv-cross/0123456789abcdef0123456789abcdef01234567 naga/30.0.0+wired-portable-v1'); process.exit(0); }
if (process.argv[2] !== '--reflect-only') process.exit(3);
writeFileSync(process.argv[4], JSON.stringify({
 entryPoints:[{name:'main',mode:'comp'}],
 ssbos:[{name:'Input',block_size:16,set:0,binding:0,readonly:true}]
}));
`);
chmodSync(translator, 0o755);

const options = { translator, output, 'shader-data': shaderData, provenance, symbols: [], check: false };
assert.equal(runReflectionCorpus(options), 1);
const catalog = JSON.parse(readFileSync(output, 'utf8'));
assert.equal(catalog.sourceCount, 1);
assert.equal(catalog.entries[0].symbol, 'fake_comp_spv');
assert.equal(catalog.entries[0].reflection.portable, true);
assert.equal(runReflectionCorpus({ ...options, check: true }), 1);

writeFileSync(output, `${readFileSync(output, 'utf8')} `);
assert.throws(() => runReflectionCorpus({ ...options, check: true }), /stale/);
writeFileSync(output, JSON.stringify(catalog, null, 2) + '\n');
writeFileSync(provenance, readFileSync(provenance, 'utf8').replace('8u,', '4u,'));
assert.throws(() => runReflectionCorpus(options), /identity mismatch/);

console.log('RAL shader reflection catalog: PASS');
