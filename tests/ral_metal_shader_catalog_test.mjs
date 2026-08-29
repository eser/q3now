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
assert.equal(catalog.moduleCount, 299);
assert.match(catalog.toolchainIdentity, /^wired-shader-xlate\/2 spirv-cross\//);
const color = catalog.get('color_vert_spv');
assert.equal(color.stage, 'RAL_STAGE_VERTEX');
assert.equal(color.vertexInputs[0].format, 'RAL_FORMAT_R32G32B32A32_SFLOAT');
assert.match(color.code, /#include <metal_stdlib>/);
const histogram = catalog.get('hdr_histogram_comp_spv');
assert.equal(histogram.inlineData.byteSize, 8);
assert.doesNotMatch(histogram.code, /\bVk|#\s*include\s*[<\"]vulkan\//i);
assert.throws(() => catalog.get('missing_spv'), /unknown MSL module/);

const tempRoot = mkdtempSync(join(tmpdir(), 'wired-metal-catalog-'));
const temp = join(tempRoot, 'portable'); cpSync(source, temp, { recursive: true });
cpSync(join(root, 'code/render/ral/backends/vulkan/renderer/shaders/spirv'), join(tempRoot, 'spirv'), { recursive: true });
writeFileSync(join(temp, 'color_vert_spv.msl'), `${readFileSync(join(temp, 'color_vert_spv.msl'))} `);
assert.throws(() => openMetalShaderCatalog(temp), /stale MSL artifact/);
console.log('RAL Metal shader catalog: PASS');
