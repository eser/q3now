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
assert.equal(committed.sourceCount, 315);
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
const skyShCompute = committed.entries.find((entry) => entry.symbol === 'project_sky_sh_comp_spv');
assert.deepEqual(productionVertex.reflection.loweringRequirements, ['vertex-format:0']);
assert.equal(productionVertex.reflection.bindings[0].minBufferBindingSize, 544);
assert.equal(productionCompute.reflection.portable, true);
assert.equal(productionCompute.reflection.bindings[0].storageTextureFormat, 'RAL_FORMAT_R16G16_SFLOAT');
assert.deepEqual(productionPushCompute.reflection.loweringRequirements,
	['inline-uniform-binding', 'sampler-kind:0:1']);
assert.equal(skyShCompute.reflection.bindings[2].bindingClass,
	'RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE');
assert.equal(skyShCompute.reflection.bindings[2].minBufferBindingSize, 144);
assert.equal(skyShCompute.reflection.inlineData.byteSize, 4);

// The existing sky/analytic compositor must carry real weather media, not
// silently copy the scene whenever authored sky is selected.
const analyticComposite = committed.entries.find(e => e.symbol === 'sky_surface_composite_comp_spv');
assert.equal(analyticComposite.reflection.bindings.find(b => b.set === 0 && b.binding === 0)
	.minBufferBindingSize, 160, 'analytic atmosphere uniform includes density, colour and height');
const analyticCode = readFileSync(join(root,
	'code/render/ral/backends/vulkan/renderer/shaders/portable/sky_surface_composite_comp_spv.glsl460'), 'utf8');
assert.match(analyticCode, /exp\(/, 'analytic atmosphere must evaluate transmittance');
assert.match(analyticCode, /mediaColorDensity/, 'analytic atmosphere consumes weather media');

// Constant-colour lightmap receivers must add normals without inventing a
// vertex-colour stream: the production tessellator does not upload that stream.
for (const mode of ['ident1', 'fixed']) {
	for (const fog of ['', '_fog']) {
		const symbol = `vert_tx1_${mode}_ibl${fog}`;
		const entry = committed.entries.find((item) => item.symbol === symbol);
		assert.ok(entry, `missing IBL receiver: ${symbol}`);
		assert.deepEqual(entry.reflection.vertexInputs.map((input) => input.location),
			[0, 2, 3, 5], symbol);
		assert.ok(committed.entries.some((item) =>
			item.symbol === `frag_tx1_${mode}_ibl${fog}_bindless`));
	}
}

// Compiled world IBL variants must not read diffuse-irradiance descriptors:
// lightmap/vertex diffuse is already composed. Keep specular and BRDF inputs.
const worldIbl = committed.entries.filter(e => /^frag_tx.*_ibl(?:_fog)?_bindless$/.test(e.symbol));
assert.ok(worldIbl.length >= 8);
for (const entry of worldIbl) {
	const slots = entry.reflection.bindings.filter(b => b.set === 2).map(b => b.binding);
	assert.ok(!slots.includes(2) && !slots.includes(5), `${entry.symbol}: duplicate diffuse inputs`);
	for (const binding of [1, 3, 4, 6]) assert.ok(slots.includes(binding), `${entry.symbol}: missing specular/BRDF/AO ${binding}`);
	const translated = readFileSync(join(root,
		`code/render/ral/backends/vulkan/renderer/shaders/portable/${entry.symbol}.glsl460`), 'utf8');
	assert.match(translated, /vec3 F0 = mix\([^;]*materialAlbedo[^;]*materialBaseColor/,
		`${entry.symbol}: metallic F0 must use unlit albedo, not composed radiance`);
	const main = translated.slice(translated.indexOf('void main()'));
	assert.equal((main.match(/wired_bl_sample_domain\(/g) ?? []).length,
		entry.symbol.startsWith('frag_tx2_') ? 3 : 2,
		`${entry.symbol}: retain albedo from existing samples, no extra texture fetch`);
}
const productFragment = committed.entries.find(e => e.symbol === 'ral_opengl_product_frag_spv');
// Inspect actual SPIR-V-translated vertex programs, not only the template.
// Eye-minus-position is affine; normalizing at vertices destroys that property
// and makes specular response depend on the triangulation of a planar wall.
const iblVertices = committed.entries.filter(e => /^vert_tx.*_ibl(?:_fog)?$/.test(e.symbol));
assert.equal(iblVertices.length, worldIbl.length);
for (const entry of iblVertices) {
	const translated = readFileSync(join(root,
		`code/render/ral/backends/vulkan/renderer/shaders/portable/${entry.symbol}.glsl460`), 'utf8');
	assert.match(translated, /ibl_V\s*=\s*ubo\.eyePos\.xyz\s*-\s*in_position\s*;/,
		`${entry.symbol}: interpolate the unnormalized view vector`);
}
const productSlots = productFragment.reflection.bindings.filter(b => b.set === 1).map(b => b.binding);
for (const binding of [16, 17, 18, 19]) assert.ok(!productSlots.includes(binding), 'product duplicate diffuse input');
for (const binding of [12, 13, 14, 15]) assert.ok(productSlots.includes(binding), 'product specular input missing');

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
