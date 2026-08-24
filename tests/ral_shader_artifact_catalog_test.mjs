// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import {
	buildShaderArtifactRow,
	ralShaderArtifactDigest,
	renderShaderArtifactCatalog,
} from '../code/render/ral/backends/vulkan/renderer/shaders/shader_artifact_catalog.mjs';

const source = new TextEncoder().encode('#version 450\nvoid main() {}\n');
const spirv = Uint8Array.of(3, 2, 35, 7, 1, 0, 0, 0);
const base = { stage: 'vert', source: 'fixture.vert', defines: ['USE_A', 'COUNT=2'], output: 'fixture_vert_spv' };
const row = buildShaderArtifactRow(base, source, spirv, 0);
const canonical = renderShaderArtifactCatalog([row]);
assert.equal(canonical, renderShaderArtifactCatalog([
	buildShaderArtifactRow(base, source, spirv, 0),
]));
assert.match(canonical, /RAL_SHADER_SPIRV_ARTIFACT\(0u, fixture_vert_spv, RAL_STAGE_VERTEX, 8u,/);

for (const changed of [
	{ ...base, stage: 'frag' },
	{ ...base, source: 'other.vert' },
	{ ...base, defines: ['COUNT=2', 'USE_A'] },
	{ ...base, output: 'other_vert_spv' },
]) {
	assert.notEqual(renderShaderArtifactCatalog([
		buildShaderArtifactRow(changed, source, spirv, 0),
	]), canonical);
}
const changedSource = new Uint8Array(source); changedSource[0]++;
assert.notEqual(renderShaderArtifactCatalog([
	buildShaderArtifactRow(base, changedSource, spirv, 0),
]), canonical);
const changedSpirv = new Uint8Array(spirv); changedSpirv[4]++;
assert.notEqual(renderShaderArtifactCatalog([
	buildShaderArtifactRow(base, source, changedSpirv, 0),
]), canonical);

const second = buildShaderArtifactRow({ ...base, output: 'fixture_2_vert_spv' }, source, spirv, 1);
const ordered = renderShaderArtifactCatalog([row, second]);
assert.throws(() => renderShaderArtifactCatalog([second, row]));
assert.throws(() => renderShaderArtifactCatalog([row, { ...second, output: row.output }]));
assert.throws(() => renderShaderArtifactCatalog(new Array(4097)));
assert.notEqual(ordered, canonical);
assert.deepEqual(ralShaderArtifactDigest(spirv), row.artifactDigest);
assert.throws(() => buildShaderArtifactRow(base, source, new Uint8Array(), 0));

console.log('RAL shader artifact catalog: PASS');
