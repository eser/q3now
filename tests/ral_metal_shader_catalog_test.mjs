// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import { cpSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { openMetalShaderCatalog } from '../code/render/ral/backends/metal/ral_metal_shader_catalog.mjs';

const root = fileURLToPath(new URL('..', import.meta.url));
const source = join(root, 'code/render/ral/backends/vulkan/renderer/shaders/portable');
const catalog = openMetalShaderCatalog(source);
assert.equal(catalog.moduleCount, 322);
assert.match(catalog.toolchainIdentity, /^wired-shader-xlate\/2 spirv-cross\//);
const color = catalog.get('color_vert_spv');
assert.equal(color.stage, 'RAL_STAGE_VERTEX');
assert.equal(color.vertexInputs[0].format, 'RAL_FORMAT_R32G32B32A32_SFLOAT');
assert.match(color.code, /#include <metal_stdlib>/);
const histogram = catalog.get('hdr_histogram_comp_spv');
assert.equal(histogram.inlineData.byteSize, 8);
assert.doesNotMatch(histogram.code, /\bVk|#\s*include\s*[<\"]vulkan\//i);
const skySh = catalog.get('project_sky_sh_comp_spv');
assert.equal(skySh.inlineData.byteSize, 4);
assert.match(skySh.code, /device SkySH\*/);
// Depth direction belongs to the common shader, never an MSL text patch.
const particleSource = readFileSync(join(source, '../particle.frag'), 'utf8');
assert.match(particleSource, /abs\( depthValid \) < 0\.5/);
assert.match(particleSource, /depthValid > 0\.0 && sceneDepth <= 0\.0/);
assert.match(particleSource, /depthValid < 0\.0 && sceneDepth >= 1\.0/);
assert.match(particleSource, /\( gl_FragCoord\.z - sceneDepth \) \* sign\( depthValid \)/);
assert.doesNotMatch(readFileSync(join(source, '../compile_xlate.mjs'), 'utf8'),
	/item\.symbol === 'particle_frag_spv'/);
const particle = catalog.get('particle_frag_spv');
assert.match(particle.code, /abs\([^)]*depthValid\)/);
assert.match(particle.code, /sign\([^)]*depthValid\)/);
assert.match(particle.code, /sceneDepth >= 1\.0/);
assert.throws(() => catalog.get('missing_spv'), /unknown MSL module/);

const tempRoot = mkdtempSync(join(tmpdir(), 'wired-metal-catalog-'));
const temp = join(tempRoot, 'portable'); cpSync(source, temp, { recursive: true });
cpSync(join(root, 'code/render/ral/backends/vulkan/renderer/shaders/spirv'), join(tempRoot, 'spirv'), { recursive: true });
writeFileSync(join(temp, 'color_vert_spv.msl'), `${readFileSync(join(temp, 'color_vert_spv.msl'))} `);
assert.throws(() => openMetalShaderCatalog(temp), /stale MSL artifact/);
console.log('RAL Metal shader catalog: PASS');
