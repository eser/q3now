// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { runPortableManifest } from '../code/renderervk/shaders/compile_portable_manifest.mjs';

const root = fileURLToPath(new URL('..', import.meta.url));
const reflection = join(root, 'code/renderervk/shaders/spirv/ral_shader_reflection_catalog.json');
const overrides = join(root, 'code/renderervk/shaders/spirv/ral_shader_portability_overrides.json');
const manifest = join(root, 'code/renderervk/shaders/spirv/ral_shader_portable_manifest.json');
const translationDir = join(root, 'code/renderervk/shaders/portable');
assert.equal(runPortableManifest({ reflection, overrides, manifest, translationDir, check: true }), 292);
const resolved = JSON.parse(readFileSync(manifest));
const decisions = JSON.parse(readFileSync(overrides));
assert.equal(resolved.sourceCount, 292); assert.equal(decisions.entries.length, 292);
assert.ok(resolved.entries.every((entry, i) => entry.ordinal === i
	&& entry.reflection.portable === true && entry.reflection.loweringRequirements.length === 0
	&& entry.reflection.bindings.every((binding) => !binding.bindingClass.includes('UNRESOLVED'))));
assert.equal(decisions.entries.filter((entry) => entry.inlineUniform).length, 15);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.combinedSamplers?.length ?? 0), 0), 91);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.arrayCounts?.length ?? 0), 0), 241);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.vertexFormats?.length ?? 0), 0), 444);
assert.equal(decisions.entries.reduce((sum, entry) => sum + (entry.dynamicOffsets?.length ?? 0), 0), 7);
assert.equal(resolved.entries.filter((entry) => entry.reflection.bindings.some((binding) => binding.dynamicOffset)).length, 7);
const symbols = decisions.entries.map((entry) => entry.symbol);
assert.deepEqual(symbols, [...symbols].sort()); assert.equal(new Set(symbols).size, 292);
console.log('RAL shader portable manifest: PASS');
