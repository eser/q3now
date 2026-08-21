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
} from '../code/renderervk/shaders/shader_artifact_catalog.mjs';

const root = mkdtempSync(join(tmpdir(), 'wired-xlate-host-'));
const output = join(root, 'output'); const wgslOutput = join(root, 'wgsl-output');
const driver = new URL('../code/renderervk/shaders/compile_xlate.mjs', import.meta.url).pathname;
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
let targets = ['msl','glsl430','glsles300'];
if (args[0] === '--targets') { targets = args[1].split(','); args = args.slice(2); }
if (process.env.FAKE_FAIL === '1') process.exit(2);
const [input, out] = args; const base = basename(input, '.spv'); const bytes = readFileSync(input);
for (const target of targets) if (target !== 'wgsl' || process.env.FAKE_WGSL === '1') writeFileSync(join(out, base + '.' + target), Buffer.concat([Buffer.from(target + ':'), bytes]));
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
assert.deepEqual(translation.requiredTargets, ['msl', 'glsl430', 'glsles300']);
assert.match(translation.toolchainIdentity, /^wired-shader-xlate\/2 /);
assert.equal(translation.entries[0].artifacts.length, 3);

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
assert.notEqual(run([], { FAKE_FAIL: '1' }).status, 0);
assert.notEqual(run(['--check'], { FAKE_VERSION: 'wired-shader-xlate/2 spirv-cross/ffffffffffffffffffffffffffffffffffffffff naga/30.0.0+wired-portable-v1' }).status, 0);
assert.notEqual(run(['--check'], { FAKE_VERSION: 'wired-shader-xlate/2 spirv-cross/0123456789abcdef0123456789abcdef01234567 naga/30.0.0' }).status, 0);

const portable = JSON.parse(readFileSync(portablePath)); portable.entries[0].spirv.digest.lane0 = '0000000000000001';
writeFileSync(portablePath, JSON.stringify(portable));
assert.notEqual(run().status, 0);
portable.entries[0].spirv.digest = digestJson(rows[0].artifactDigest);
writeFileSync(portablePath, JSON.stringify(portable));

writeFileSync(shaderDataPath, bytesC(entryA.output, spirvB) + bytesC(entryB.output, spirvB));
assert.notEqual(run().status, 0);
writeFileSync(shaderDataPath, bytesC(entryA.output, spirvA) + bytesC(entryB.output, spirvB));
const lines = renderShaderArtifactCatalog(rows).split('\n');
writeFileSync(catalogPath, [lines[0], lines[1], lines[2], lines[4], lines[3], ''].join('\n'));
assert.notEqual(run().status, 0);

console.log('RAL shader translation driver: PASS');
