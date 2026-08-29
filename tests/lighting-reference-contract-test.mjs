// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const fixture = JSON.parse(readFileSync(resolve(root, 'tests/fixtures/lighting/lava-sanctum-v1.json'), 'utf8'));
const goldenRoot = resolve(root, 'tests/golden/lighting/lava-sanctum');
const golden = JSON.parse(readFileSync(resolve(goldenRoot, 'manifest.json'), 'utf8'));
const check = (value, message) => { if (!value) throw new Error(`lighting reference contract: ${message}`); };
const exact = (actual, expected, label) => check(JSON.stringify(actual) === JSON.stringify(expected), `${label} drift`);

check(fixture.schemaVersion === 1 && fixture.fixtureId === 'lava-sanctum', 'identity/schema');
exact(fixture.logicalResolution, { width: 1280, height: 720 }, 'widescreen logical resolution');
check(fixture.scene.map === 'arena7' && fixture.scene.camera.origin.length === 3, 'deterministic map/camera');
exact(fixture.scene.camera, { origin: [650, -500, -250], yaw: 225, pitch: 10 }, 'product-probed camera');
exact(fixture.scene.requiredAuthoredRoles, [
  'emissive-lava', 'emissive-fire', 'emissive-signage', 'warm-bounce-receiver',
  'cool-sky-fill', 'moving-entity', 'representative-occluder'
], 'authored roles');
exact(Object.keys(fixture.scene.roleEvidence), fixture.scene.requiredAuthoredRoles, 'authored role evidence inventory');
check(fixture.scene.roleEvidence['emissive-lava'].material === 'textures/liquids/lavahelldark'
  && fixture.scene.roleEvidence['emissive-lava'].sourceLight === 150, 'lava BSP/shader evidence');
check(fixture.scene.roleEvidence['emissive-fire'].material === 'textures/sfx/flame1'
  && fixture.scene.roleEvidence['emissive-fire'].sourceLight === 7500, 'fire BSP/shader evidence');
check(fixture.scene.roleEvidence['moving-entity'].lighting === 'local-sh-l1'
  && fixture.scene.roleEvidence['moving-entity'].path.length === 2, 'moving SH entity evidence');
exact(fixture.captures.map(c => c.id), [
  'emissive-only', 'direct-only', 'indirect-only', 'probe-only',
  'shadow-contact', 'atmosphere-only', 'final'
], 'decomposed captures');
for (const capture of fixture.captures) {
  check(capture.terms.length > 0 && new Set(capture.terms).size === capture.terms.length, `${capture.id} term authority`);
  check(typeof capture.authority === 'string' && capture.authority.length > 0, `${capture.id} runtime authority`);
}
exact(fixture.regions.map(r => r.id), ['lava-detail', 'shadow-side-form', 'warm-cool-separation', 'moving-entity'], 'ROI inventory');
for (const region of fixture.regions) {
  check(region.normalizedRect.length === 4 && region.normalizedRect.every(v => v >= 0 && v <= 1), `${region.id} bounds`);
  check(region.assertions.length >= 2, `${region.id} assertion teeth`);
}
check(fixture.exposureGate.sampleFrames === 3 && fixture.exposureGate.maximumClippedPixelRatio <= 0.01, 'exposure gate');
check(fixture.performanceGate.build === 'Release' && fixture.performanceGate.runs === 3 && fixture.performanceGate.minimumFps >= 250, 'Release N=3 performance gate');
exact(fixture.shadowBudget.candidates, [1, 2, 3, 4], 'shadow K candidates');
check(fixture.shadowBudget.enabledByDefault === true && fixture.shadowBudget.selectedK === 1, 'FPS-first K=1 shipping default');
check(fixture.shadowBudget.decision.policy === 'enabled-default-k1'
  && fixture.shadowBudget.decision.rationale.includes('0.175 ms'), 'shadow policy evidence');
exact(fixture.backends, ['vulkan', 'metal', 'opengl46', 'webgpu'], 'backend matrix');
check(fixture.unsupportedHostPolicy === 'explicit-skip', 'unsupported hosts must not fabricate parity');
check(golden.schemaVersion === 1 && golden.fixture === 'lava-sanctum-v1', 'golden manifest identity');
exact(golden.extent, [1280, 720], 'golden extent');
exact(Object.keys(golden.vulkan.images).sort(), fixture.captures.map(c => c.id).sort(), 'golden inventory');
for (const [name, expected] of Object.entries(golden.vulkan.images)) {
  const bytes = readFileSync(resolve(goldenRoot, 'vulkan', `${name}.png`));
  check(createHash('sha256').update(bytes).digest('hex') === expected.sha256, `${name} golden hash`);
}
check(golden.releasePerformance.runs === 3 && golden.releasePerformance.medianFps >= fixture.performanceGate.minimumFps,
  'published Release N=3 performance');
for (const run of golden.releasePerformance.perPassGpuMilliseconds) {
  for (const lane of ['atmosphere_compute', 'particle_compute', 'dlight_shadow', 'present_prep', 'total'])
    check(Number.isFinite(run[lane]) && run[lane] >= 0, `published ${lane} timing`);
}

console.log('lighting-reference-contract-test: ok');
