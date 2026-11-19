// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import {
	chmodSync, mkdtempSync, readFileSync, unlinkSync, writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import {
	buildShaderArtifactRow, renderShaderArtifactCatalog,
} from '../code/render/ral/backends/vulkan/renderer/shaders/shader_artifact_catalog.mjs';
import { lowerGlsl460 } from '../code/render/ral/backends/vulkan/renderer/shaders/compile_xlate.mjs';

// Both compute consumers must lower their explicit texture/sampler pair to
// desktop GLSL, without changing the shared histogram/visibility algorithm.
for (const [symbol, texture, sampler] of [
	['lens_occlusion_comp_spv', 'depthTex', 'depthSampler'],
	['hdr_histogram_comp_spv', 'colorImage', 'colorSampler'],
]) {
	const source = `#version 460
layout(set = 0, binding = 0) uniform texture2D ${texture};
layout(set = 0, binding = 1) uniform sampler ${sampler};
layout(push_constant) uniform Dims { uvec2 extent; } dims;
void main() { vec4 a = texelFetch(sampler2D(${texture}, ${sampler}), ivec2(0), 0);
vec4 b = texelFetch(sampler2D(${texture}, ${sampler}), ivec2(1), 0); }
`;
	const entry = { symbol, reflection: { bindings: [
		{ set: 0, binding: 0, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 1 },
		{ set: 0, binding: 1, bindingClass: 'RAL_SHADER_BIND_NONFILTERING_SAMPLER', arrayCount: 1 },
	], inlineData: { byteSize: 8, webgpuUniformBinding: 3 } } };
	const lowered = lowerGlsl460(Buffer.from(source), entry).toString('utf8');
	assert.ok(lowered.includes(`layout(binding = 0) uniform sampler2D ${texture};`));
	assert.ok(lowered.includes('layout(std140, binding = 3) uniform Dims'));
	assert.equal(lowered.split(`texelFetch(${texture},`).length - 1, 2);
	assert.doesNotMatch(lowered, /uniform texture2D|uniform sampler\s|push_constant/);
	assert.throws(() => lowerGlsl460(Buffer.from(source.replace(
		`uniform sampler ${sampler};`, 'uniform sampler unexpectedSampler;')), entry),
		/combined-sampler lowering failed/);
}

const productFragment = lowerGlsl460(Buffer.from(`#version 460
layout(set = 1, binding = 0) uniform texture2D baseTexture;
layout(set = 1, binding = 1) uniform sampler baseSampler;
layout(set = 1, binding = 2) uniform texture2D lightmapTexture;
layout(set = 1, binding = 3) uniform sampler lightmapSampler;
layout(set = 1, binding = 4) uniform texture2DArray staticRadianceTexture;
layout(set = 1, binding = 5) uniform sampler staticRadianceSampler;
layout(set = 1, binding = 6) uniform texture2DArray staticDirectionTexture;
layout(set = 1, binding = 7) uniform sampler staticDirectionSampler;
layout(set = 1, binding = 8) uniform texture2DArray staticVisibilityTexture;
layout(set = 1, binding = 9) uniform sampler staticVisibilitySampler;
layout(set = 1, binding = 10) uniform texture2D physicalSkyTexture;
layout(set = 1, binding = 11) uniform sampler physicalSkySampler;
layout(set = 1, binding = 12) uniform texture2DArray currentSkyRadianceTexture;
layout(set = 1, binding = 13) uniform sampler currentSkyRadianceSampler;
layout(set = 1, binding = 14) uniform texture2DArray previousSkyRadianceTexture;
layout(set = 1, binding = 15) uniform sampler previousSkyRadianceSampler;
vec3 wired_sample_sky_radiance(texture2DArray skyTexture, sampler skySampler, vec3 direction, float lod) {
	return textureLod(sampler2DArray(skyTexture, skySampler), direction, lod).xyz;
}
void main() { vec4 a = texture(sampler2D(baseTexture, baseSampler), vec2(0));
vec4 b = texture(sampler2D(lightmapTexture, lightmapSampler), vec2(0));
vec4 c = texture(sampler2DArray(staticRadianceTexture, staticRadianceSampler), vec3(0));
vec4 d = texture(sampler2DArray(staticDirectionTexture, staticDirectionSampler), vec3(0));
vec4 e = texture(sampler2DArray(staticVisibilityTexture, staticVisibilitySampler), vec3(0));
vec4 f = texture(sampler2D(physicalSkyTexture, physicalSkySampler), vec2(0));
vec4 g = vec4(wired_sample_sky_radiance(currentSkyRadianceTexture, currentSkyRadianceSampler, vec3(0), 0.0), 1);
vec4 h = vec4(wired_sample_sky_radiance(previousSkyRadianceTexture, previousSkyRadianceSampler, vec3(0), 0.0), 1); }
`), { symbol: 'ral_opengl_product_frag_spv', reflection: { bindings: [
	{ set: 1, binding: 0, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 1 },
	{ set: 1, binding: 1, bindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER', arrayCount: 1 },
	{ set: 1, binding: 2, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 1 },
	{ set: 1, binding: 3, bindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER', arrayCount: 1 },
	{ set: 1, binding: 4, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 1 },
	{ set: 1, binding: 5, bindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER', arrayCount: 1 },
	{ set: 1, binding: 6, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 1 },
	{ set: 1, binding: 7, bindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER', arrayCount: 1 },
	{ set: 1, binding: 8, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 1 },
	{ set: 1, binding: 9, bindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER', arrayCount: 1 },
	{ set: 1, binding: 10, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 1 },
	{ set: 1, binding: 11, bindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER', arrayCount: 1 },
	{ set: 1, binding: 12, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 1 },
	{ set: 1, binding: 13, bindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER', arrayCount: 1 },
	{ set: 1, binding: 14, bindingClass: 'RAL_SHADER_BIND_SAMPLED_TEXTURE', arrayCount: 1 },
	{ set: 1, binding: 15, bindingClass: 'RAL_SHADER_BIND_FILTERING_SAMPLER', arrayCount: 1 },
] } }).toString('utf8');
assert.match(productFragment, /layout\(binding = 0\) uniform sampler2D baseTexture;/);
assert.match(productFragment, /layout\(binding = 1\) uniform sampler2D lightmapTexture;/);
assert.match(productFragment, /layout\(binding = 2\) uniform sampler2DArray staticRadianceTexture;/);
assert.match(productFragment, /layout\(binding = 3\) uniform sampler2DArray staticDirectionTexture;/);
assert.match(productFragment, /layout\(binding = 4\) uniform sampler2DArray staticVisibilityTexture;/);
assert.match(productFragment, /layout\(binding = 5\) uniform sampler2D physicalSkyTexture;/);
assert.match(productFragment, /layout\(binding = 6\) uniform sampler2DArray currentSkyRadianceTexture;/);
assert.match(productFragment, /layout\(binding = 7\) uniform sampler2DArray previousSkyRadianceTexture;/);
assert.doesNotMatch(productFragment, /SkyIrradiance/);
assert.match(productFragment,
	/vec3 wired_sample_sky_radiance\(sampler2DArray skyTexture, vec3 direction, float lod\)/);
assert.match(productFragment,
	/wired_sample_sky_radiance\(currentSkyRadianceTexture, vec3\(0\), 0\.0\)/);
assert.doesNotMatch(productFragment,
	/uniform texture2D|uniform texture2DArray|baseSampler|lightmapSampler|staticRadianceSampler|staticDirectionSampler|staticVisibilitySampler|physicalSkySampler|currentSkyRadianceSampler|previousSkyRadianceSampler|currentSkyIrradianceSampler|previousSkyIrradianceSampler/);

const root = mkdtempSync(join(tmpdir(), 'wired-xlate-host-'));
const output = join(root, 'output'); const wgslOutput = join(root, 'wgsl-output');
const driver = new URL('../code/render/ral/backends/vulkan/renderer/shaders/compile_xlate.mjs', import.meta.url).pathname;
const source = new TextEncoder().encode('void main() {}\n');
const spirvA = Uint8Array.of(3, 2, 35, 7, 1, 0, 0, 0);
const spirvB = Uint8Array.of(3, 2, 35, 7, 2, 0, 0, 0);
const entryA = { stage: 'vert', source: 'a.vert', output: 'a_vert_spv' };
const entryB = { stage: 'frag', source: 'b.frag', output: 'b_frag_spv' };
const rows = [
	buildShaderArtifactRow(entryA, source, spirvA, 0),
	buildShaderArtifactRow(entryB, source, spirvB, 1),
];
const catalogPath = join(root, 'catalog.inc');
const shaderDataPath = join(root, 'shader_data.c');
const portablePath = join(root, 'portable.json');
const bytesC = (name, bytes) => `const unsigned char ${name}[${bytes.length}] = { ${[...bytes].map((v) => `0x${v.toString(16).padStart(2, '0')}`).join(', ')} };\n`;
writeFileSync(catalogPath, renderShaderArtifactCatalog(rows));
writeFileSync(shaderDataPath, bytesC(entryA.output, spirvA) + bytesC(entryB.output, spirvB));
const digestJson = (value) => ({ lane0: value.lane0.toString(16).padStart(16, '0'),
	lane1: value.lane1.toString(16).padStart(16, '0') });
writeFileSync(portablePath, JSON.stringify({ schemaVersion: 1, generation: 1, sourceCount: 2,
	entries: rows.map((row) => ({ ordinal: row.ordinal, symbol: row.output,
		stage: row.stage === 'vert' ? 'RAL_STAGE_VERTEX' : 'RAL_STAGE_FRAGMENT',
		sourceDigest: digestJson(row.inputDigest), spirv: { byteCount: row.byteCount,
			digest: digestJson(row.artifactDigest) }, reflection: { portable: true,
			loweringRequirements: [], bindings: [], vertexInputs: [] } })) }, null, 2) + '\n');

const fake = join(root, 'fake_xlate.mjs');
writeFileSync(fake, `#!/usr/bin/env node
import { readFileSync, writeFileSync } from 'node:fs';
import { basename, join } from 'node:path';
let args = process.argv.slice(2);
if (args[0] === '--version') { console.log(process.env.FAKE_VERSION ?? 'wired-shader-xlate/2 spirv-cross/0123456789abcdef0123456789abcdef01234567 naga/30.0.0+wired-portable-v1'); process.exit(0); }
if (args[0] === '--require-wgsl') args = args.slice(1);
let targets = ['msl','glsl460'];
if (args[0] === '--targets') { targets = args[1].split(','); args = args.slice(2); }
if (process.env.FAKE_FAIL === '1') process.exit(2);
const [input, out] = args; const base = basename(input, '.spv'); const bytes = readFileSync(input);
for (const target of targets) if (target !== 'wgsl' || process.env.FAKE_WGSL === '1') {
	const artifact = target === 'glsl460' ? Buffer.from('#version 460\\nvoid main() {}\\n')
		: target === 'wgsl' ? Buffer.from('@group(0) @binding(0)  \\nvar texture0: texture_2d<f32>;\\t\\n')
		: Buffer.concat([Buffer.from(target + ':'), bytes]);
	writeFileSync(join(out, base + '.' + target), artifact);
}
`);
chmodSync(fake, 0o755);

function run(extra = [], env = {}) {
	return spawnSync(process.execPath, [driver, '--translator', fake,
		'--shader-data', shaderDataPath, '--catalog', catalogPath,
		'--portable-manifest', portablePath,
		'--output-dir', output, ...extra], {
		encoding: 'utf8', env: { ...process.env, ...env },
	});
}

let result = run();
assert.equal(result.status, 0, result.stderr);
assert.equal(run(['--check']).status, 0);
const translation = JSON.parse(readFileSync(join(output, 'translation_catalog.json'), 'utf8'));
assert.equal(translation.entries.length, 2);
assert.deepEqual(translation.requiredTargets, ['msl', 'glsl460']);
assert.match(translation.toolchainIdentity, /^wired-shader-xlate\/2 /);
assert.equal(translation.entries[0].artifacts.length, 2);

writeFileSync(join(output, 'a_vert_spv.msl'), 'stale');
assert.notEqual(run(['--check']).status, 0);
assert.equal(run().status, 0);
writeFileSync(join(output, 'extra.bin'), 'extra');
assert.notEqual(run(['--check']).status, 0);
unlinkSync(join(output, 'extra.bin'));
unlinkSync(join(output, 'a_vert_spv.msl'));
assert.notEqual(run(['--check']).status, 0);
assert.equal(run().status, 0);

assert.notEqual(run(['--require-wgsl']).status, 0);
result = spawnSync(process.execPath, [driver, '--translator', fake,
	'--shader-data', shaderDataPath, '--catalog', catalogPath,
	'--portable-manifest', portablePath,
	'--output-dir', wgslOutput, '--require-wgsl'], {
	encoding: 'utf8', env: { ...process.env, FAKE_WGSL: '1' },
});
assert.equal(result.status, 0, result.stderr);
assert.equal(JSON.parse(readFileSync(join(wgslOutput, 'translation_catalog.json'), 'utf8'))
	.requiredTargets.at(-1), 'wgsl');
assert.doesNotMatch(readFileSync(join(wgslOutput, 'a_vert_spv.wgsl'), 'utf8'), /[ \t]+$/m);
assert.notEqual(run([], { FAKE_FAIL: '1' }).status, 0);
assert.notEqual(run(['--check'], { FAKE_VERSION: 'wired-shader-xlate/2 spirv-cross/ffffffffffffffffffffffffffffffffffffffff naga/30.0.0+wired-portable-v1' }).status, 0);
assert.notEqual(run(['--check'], { FAKE_VERSION: 'wired-shader-xlate/2 spirv-cross/0123456789abcdef0123456789abcdef01234567 naga/30.0.0' }).status, 0);

const portable = JSON.parse(readFileSync(portablePath)); portable.entries[0].spirv.digest.lane0 = '0000000000000001';
writeFileSync(portablePath, JSON.stringify(portable));
// Write mode can bootstrap a refreshed translation corpus from the previous
// portable manifest; strict freshness remains a --check responsibility.
assert.equal(run().status, 0);
assert.notEqual(run(['--check']).status, 0);
portable.entries[0].spirv.digest = digestJson(rows[0].artifactDigest);
writeFileSync(portablePath, JSON.stringify(portable));

writeFileSync(shaderDataPath, bytesC(entryA.output, spirvB) + bytesC(entryB.output, spirvB));
assert.notEqual(run().status, 0);
writeFileSync(shaderDataPath, bytesC(entryA.output, spirvA) + bytesC(entryB.output, spirvB));
const lines = renderShaderArtifactCatalog(rows).split('\n');
writeFileSync(catalogPath, [lines[0], lines[1], lines[2], lines[4], lines[3], ''].join('\n'));
assert.notEqual(run().status, 0);

console.log('RAL shader translation driver: PASS');
