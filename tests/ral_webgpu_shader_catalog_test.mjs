// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import { cpSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { openWebGpuShaderCatalog } from '../code/renderer/ral/ral_webgpu_shader_catalog.mjs';

const root = fileURLToPath(new URL('..', import.meta.url));
const source = join(root, 'code/renderervk/shaders/portable');
const catalog = openWebGpuShaderCatalog(source);
assert.equal(catalog.moduleCount, 292);
assert.match(catalog.toolchainIdentity, /naga\/30\.0\.0\+wired-portable-v1$/);
const color = catalog.get('color_vert_spv');
assert.equal(color.stage, 'RAL_STAGE_VERTEX');
assert.equal(color.vertexInputs[0].format, 'RAL_FORMAT_R32G32B32A32_SFLOAT');
const histogram = catalog.get('hdr_histogram_comp_spv');
assert.equal(histogram.inlineData.byteSize, 8);
assert.match(histogram.code, /@group\(0\) @binding\(3\)\s*\nvar<uniform>/);
assert.doesNotMatch(histogram.code, /var<immediate>|\bVk/);
assert.throws(() => catalog.get('missing_spv'), /unknown WGSL module/);

const tempRoot = mkdtempSync(join(tmpdir(), 'wired-webgpu-catalog-'));
const temp = join(tempRoot, 'portable'); cpSync(source, temp, { recursive: true });
cpSync(join(root, 'code/renderervk/shaders/spirv'), join(tempRoot, 'spirv'), { recursive: true });
writeFileSync(join(temp, 'color_vert_spv.wgsl'), `${readFileSync(join(temp, 'color_vert_spv.wgsl'))} `);
assert.throws(() => openWebGpuShaderCatalog(temp), /stale WGSL artifact/);
console.log('RAL WebGPU shader catalog: PASS');
