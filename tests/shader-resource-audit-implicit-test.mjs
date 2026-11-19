#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const fixture = fs.mkdtempSync(path.join(os.tmpdir(), "q3now-shader-audit-implicit."));

function writeQ3Bsp(file, materials, used = materials.map((_, index) => index)) {
  const headerBytes = 8 + 17 * 8;
  const shaderBytes = materials.length * 72;
  const data = Buffer.alloc(headerBytes + shaderBytes + used.length * 104);
  data.write("IBSP", 0, "ascii");
  data.writeInt32LE(46, 4);
  data.writeInt32LE(headerBytes, 16);
  data.writeInt32LE(shaderBytes, 20);
  data.writeInt32LE(headerBytes + shaderBytes, 112);
  data.writeInt32LE(used.length * 104, 116);
  used.forEach((shader, index) => {
    data.writeInt32LE(shader, headerBytes + shaderBytes + index * 104);
    data.writeInt32LE(1, headerBytes + shaderBytes + index * 104 + 8);
  });
  for (let i = 0; i < materials.length; ++i)
    data.write(materials[i], headerBytes + i * 72, 64, "utf8");
  fs.writeFileSync(file, data);
}

function run(strict) {
  const args = [
    path.join(root, "tools/shader-resource-audit.mjs"),
    "--imported-root", path.join(fixture, "imported"),
    "--owned-root", path.join(fixture, "owned"),
    "--content-root", path.join(fixture, "content"),
    "--map-root", path.join(fixture, "maps-root"),
    "--aliases", path.join(fixture, "fs-aliases.lua")
  ];
  if (strict) args.push("--strict-map-materials");
  return spawnSync(process.execPath, args, { encoding: "utf8" });
}

try {
  for (const directory of [
    "imported/scripts", "owned/scripts", "content/textures", "maps-root/maps"
  ]) fs.mkdirSync(path.join(fixture, directory), { recursive: true });
  fs.writeFileSync(path.join(fixture, "content/textures/present.png"), "fixture");
  fs.writeFileSync(path.join(fixture, "fs-aliases.lua"), "return { files = {} }\n");
  writeQ3Bsp(path.join(fixture, "maps-root/maps/audit.bsp"), [
    "textures/present", "textures/missing"
  ]);

  const reportRun = run(false);
  assert.equal(reportRun.status, 0, reportRun.stderr);
  const report = JSON.parse(reportRun.stdout);
  assert.equal(report.maps.implicitImageMaterials, 2);
  assert.equal(report.maps.implicitResolved, 1);
  assert.equal(report.maps.implicitMissing, 1);
  assert.deepEqual(report.maps.implicitDiagnostics.missing, [{
    material: "textures/missing",
    maps: ["maps/audit.bsp"],
    alternatives: []
  }]);

  const strictRun = run(true);
  assert.equal(strictRun.status, 1, "strict shipping corpus audit must reject the missing implicit image");
  const strictReport = JSON.parse(strictRun.stdout);
  assert.equal(strictReport.strictMapMaterials, true);
  assert.equal(strictReport.maps.implicitMissing, 1);

  fs.writeFileSync(path.join(fixture, "content/textures/missing.webp"), "fixture");
  const completeRun = run(true);
  assert.equal(completeRun.status, 0, completeRun.stderr);
  const completeReport = JSON.parse(completeRun.stdout);
  assert.equal(completeReport.maps.implicitResolved, 2);
  assert.equal(completeReport.maps.implicitMissing, 0);
  writeQ3Bsp(path.join(fixture, "maps-root/maps/audit.bsp"), [
    "textures/present", "textures/missing-unused"
  ], [0]);
  const unusedRun = run(true);
  assert.equal(unusedRun.status, 0, "unused BSP shader rows must not require images");
  assert.equal(JSON.parse(unusedRun.stdout).maps.implicitResolved, 1);
  writeQ3Bsp(path.join(fixture, "maps-root/maps/audit.bsp"), ["textures/present"], [1]);
  const invalidRun = run(true);
  assert.equal(invalidRun.status, 1, "out-of-range surface shader index must fail closed");
  console.log("shader resource audit implicit BSP materials: PASS");
} finally {
  fs.rmSync(fixture, { recursive: true, force: true });
}
