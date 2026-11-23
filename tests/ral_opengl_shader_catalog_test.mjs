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
assert.equal(catalog.entries.length, 376);
assert.equal(manifest.entries.length, 376);
assert.equal(catalog.portableManifest.byteCount, Buffer.byteLength(manifestText));
assert.ok(exact(Buffer.from(manifestText), catalog.portableManifest.digest));
assert.equal(readdirSync(root).filter((name) => name.endsWith('.glsl460')).length, 376);
for (let ordinal = 0; ordinal < 376; ++ordinal) {
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
assert.deepEqual(catalog.entries.slice(-20).map((entry) => entry.symbol), [
	'frag_tx1_ident1_shadow_fog_temporal_write_bindless',
	'frag_tx1_ident1_shadow_fog_temporal_invalidate_bindless',
	'vert_tx1_fixed_shadow_temporal_motion',
	'frag_tx1_fixed_shadow_temporal_write_bindless',
	'frag_tx1_fixed_shadow_temporal_invalidate_bindless',
	'vert_tx1_fixed_shadow_fog_temporal_motion',
	'frag_tx1_fixed_shadow_fog_temporal_write_bindless',
	'frag_tx1_fixed_shadow_fog_temporal_invalidate_bindless',
	'vert_tx2_shadow_temporal_motion',
	'frag_tx2_shadow_temporal_write_bindless',
	'frag_tx2_shadow_temporal_invalidate_bindless',
	'vert_tx2_shadow_fog_temporal_motion',
	'frag_tx2_shadow_fog_temporal_write_bindless',
	'frag_tx2_shadow_fog_temporal_invalidate_bindless',
	'vert_tx2_cl_shadow_temporal_motion',
	'frag_tx2_cl_shadow_temporal_write_bindless',
	'frag_tx2_cl_shadow_temporal_invalidate_bindless',
	'vert_tx2_cl_shadow_fog_temporal_motion',
	'frag_tx2_cl_shadow_fog_temporal_write_bindless',
	'frag_tx2_cl_shadow_fog_temporal_invalidate_bindless',
]);
generateProductShaderCatalog({ portable: root, output: productCatalogPath, check: true });
// Check the actual translated product shaders, not only host-side uploads.
const beamVertex = readFileSync(join(root, 'ral_opengl_product_vert_spv.glsl460'), 'utf8');
const beamFragment = readFileSync(join(root, 'ral_opengl_product_frag_spv.glsl460'), 'utf8');
const materialBlock = /layout\(binding = 2, std140\) uniform ProductMaterialFactors\s*\{([^}]+)\}/;
const materialFields = 'vec4 baseColor; vec4 factors; vec4 beamStartColor; vec4 beamEndColor; vec4 beamScaleOffset; uvec4 beamFlags;';
for (const source of [beamVertex, beamFragment]) {
	assert.match(source, materialBlock);
	assert.equal(source.match(materialBlock)[1].trim().replace(/\s+/g, ' '), materialFields);
	assert.match(source, /productMaterial\.beamFlags\.x != 0u/);
}
assert.match(beamVertex, /color = mix\(productMaterial\.beamStartColor, productMaterial\.beamEndColor, vec4\(in_tex_coord\.x\)\)/);
assert.match(beamVertex, /texCoord = \(in_tex_coord \* productMaterial\.beamScaleOffset\.xy\) \+ productMaterial\.beamScaleOffset\.zw/);
assert.match(beamFragment, /vec3 linearColor = mix\(pow\([\s\S]*?lessThanEqual\(color\.xyz,/);
// Beam emission consumes transmittance only; straight coverage remains in
// alpha for SRC_ALPHA/ONE, with no extra fog radiance or double alpha.
assert.match(beamFragment, /outColor = vec4\(\(base\.xyz \* linearColor\) \* wired_atmosphere_sample\(\)\.w, base\.w \* color\.w\);\s*return;/);
assert.match(beamFragment, /float viewDistance = dot\(worldPosition - productDraw\.atmosphereEyeDensity\.xyz, productDraw\.viewForward\.xyz\)/);
const histogram = readFileSync(join(root, 'hdr_histogram_comp_spv.glsl460'), 'utf8');
assert.match(histogram, /layout\(binding = 0\) uniform sampler2D colorImage;/);
assert.match(histogram, /texelFetch\(colorImage, ivec2\(coord\), 0\)/);
assert.doesNotMatch(histogram, /uniform texture2D|colorSampler/);
const fullscreen = readFileSync(join(root, 'gamma_vert_spv.glsl460'), 'utf8');
assert.match(fullscreen, /gl_VertexID/);
assert.doesNotMatch(fullscreen, /gl_VertexIndex/);
const tonemap = readFileSync(join(root, 'tonemap_tonemap_frag_spv.glsl460'), 'utf8');
assert.doesNotMatch(tonemap, /constant_id/);
assert.match(tonemap, /#ifndef WIRED_SPEC_17\n#define WIRED_SPEC_17 1\n#endif\nconst int tonemap_mode = WIRED_SPEC_17;/);
assert.match(tonemap, /#ifndef WIRED_SPEC_18/);
assert.match(tonemap, /base \*= eb.exposure_bias;/);
assert.match(tonemap, /float shadowExponent;/);
const first = catalog.entries[0].artifacts[0];
const bad = Buffer.from(readFileSync(join(root, `${catalog.entries[0].symbol}.glsl460`)));
bad[bad.length - 1] ^= 1;
assert.equal(exact(bad, first.digest), false);
console.log('RAL OpenGL GLSL 4.60 catalog: PASS (376 modules; product artifact selected)');
