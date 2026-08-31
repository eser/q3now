#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

// Offline maintenance/audit tool for the exact-extension VFS policy.
// Imported id shader sources remain byte-for-byte unchanged; compatibility is
// expressed only through the source-controlled fs-aliases.lua catalog. Wired-
// owned shaders, on the other hand, must spell the canonical packaged path.

import fs from "node:fs";
import path from "node:path";

const IMAGE_EXTENSIONS = new Set([".tga", ".jpg", ".jpeg", ".png", ".webp", ".dds"]);

function usage() {
  console.error("usage: shader-resource-audit.mjs --imported-root DIR --owned-root DIR --content-root DIR [--content-root DIR ...] --map-root DIR [--map-root DIR ...] --aliases FILE [--fix]");
  process.exit(2);
}

const args = process.argv.slice(2);
const options = { importedRoot: "", ownedRoot: "", contentRoots: [], mapRoots: [], aliases: "", fix: false };
for (let i = 0; i < args.length; i++) {
  switch (args[i]) {
    case "--imported-root": options.importedRoot = args[++i] ?? ""; break;
    case "--owned-root": options.ownedRoot = args[++i] ?? ""; break;
    case "--content-root": options.contentRoots.push(args[++i] ?? ""); break;
    case "--map-root": options.mapRoots.push(args[++i] ?? ""); break;
    case "--aliases": options.aliases = args[++i] ?? ""; break;
    case "--fix": options.fix = true; break;
    default: usage();
  }
}
if (!options.importedRoot || !options.ownedRoot || !options.contentRoots.length || !options.mapRoots.length || !options.aliases) usage();

function walk(root, out = []) {
  if (!fs.existsSync(root)) return out;
  for (const entry of fs.readdirSync(root, { withFileTypes: true })) {
    const full = path.join(root, entry.name);
    if (entry.isDirectory()) walk(full, out);
    else out.push(full);
  }
  return out;
}

function qpath(root, file) {
  return path.relative(root, file).split(path.sep).join("/");
}

function shaderReferences(root) {
  const scripts = path.join(root, "scripts");
  const refs = [];
  for (const file of walk(scripts).filter((name) => name.toLowerCase().endsWith(".shader"))) {
    const source = fs.readFileSync(file, "utf8");
    const lines = source.split(/\r?\n/);
    let depth = 0;
    let pendingMaterial = "";
    let material = "";
    for (let index = 0; index < lines.length; index++) {
      const line = lines[index].replace(/\/\/.*$/, "").trim();
      if (!line) continue;
      if (depth === 0 && line !== "{") {
        pendingMaterial = line.split(/[\s{]/, 1)[0];
      }
      const opens = (line.match(/{/g) ?? []).length;
      const closes = (line.match(/}/g) ?? []).length;
      if (depth === 0 && opens > 0) material = pendingMaterial;
      const direct = line.match(/^(?:map|clampmap)\s+([^\s{}]+)/i);
      if (direct) refs.push({ file, sourceFile: qpath(root, file), line: index + 1, material, requested: direct[1] });
      const animated = line.match(/^(?:animmap|clampanimmap)\s+[^\s{}]+\s+(.+)$/i);
      if (animated) {
        for (const requested of animated[1].split(/\s+/).filter((token) => token && token !== "{" && token !== "}")) {
          refs.push({ file, sourceFile: qpath(root, file), line: index + 1, material, requested });
        }
      }
      depth += opens - closes;
      if (depth <= 0) {
        depth = 0;
        material = "";
        if (closes > 0) pendingMaterial = "";
      }
    }
  }
  return refs.filter((ref) => IMAGE_EXTENSIONS.has(path.extname(ref.requested).toLowerCase()));
}

function shaderMaterials(root) {
  const materials = new Set();
  const scripts = path.join(root, "scripts");
  for (const file of walk(scripts).filter((name) => name.toLowerCase().endsWith(".shader"))) {
    let depth = 0;
    let pending = "";
    for (const sourceLine of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
      const line = sourceLine.replace(/\/\/.*$/, "").trim();
      if (!line) continue;
      if (depth === 0 && line !== "{") pending = line.split(/[\s{]/, 1)[0].toLowerCase();
      const opens = (line.match(/{/g) ?? []).length;
      const closes = (line.match(/}/g) ?? []).length;
      if (depth === 0 && opens > 0 && pending) materials.add(pending);
      depth = Math.max(0, depth + opens - closes);
      if (depth === 0 && closes > 0) pending = "";
    }
  }
  return materials;
}

function md3References(root, declaredMaterials) {
  const refs = [];
  for (const file of walk(root).filter((name) => name.toLowerCase().endsWith(".md3"))) {
    const data = fs.readFileSync(file);
    if (data.length < 108 || data.toString("ascii", 0, 4) !== "IDP3") continue;
    const surfaces = data.readInt32LE(84);
    let surfaceOffset = data.readInt32LE(100);
    for (let surface = 0; surface < surfaces && surfaceOffset + 108 <= data.length; surface++) {
      const shaders = data.readInt32LE(surfaceOffset + 76);
      const shaderOffset = data.readInt32LE(surfaceOffset + 92);
      const endOffset = data.readInt32LE(surfaceOffset + 104);
      for (let shader = 0; shader < shaders; shader++) {
        const cursor = surfaceOffset + shaderOffset + shader * 68;
        if (cursor < 0 || cursor + 68 > data.length) break;
        const nul = data.indexOf(0, cursor);
        const end = nul >= cursor && nul < cursor + 64 ? nul : cursor + 64;
        const requested = data.toString("utf8", cursor, end).replaceAll("\\\\", "/");
        const extension = path.extname(requested).toLowerCase();
        const material = requested.slice(0, requested.length - extension.length).toLowerCase();
        // MD3 slots may name either a raw image or an extension-decorated shader.
        // A declared shader owns its own stage resources; only raw-image slots need
        // exact-extension compatibility aliases.
        if (IMAGE_EXTENSIONS.has(extension) && !declaredMaterials.has(material)) {
          refs.push({ file, sourceFile: qpath(root, file), line: surface + 1, material: "", requested });
        }
      }
      if (endOffset <= 0) break;
      surfaceOffset += endOffset;
    }
  }
  return refs;
}

function bspInventory(roots) {
  const maps = [];
  const materials = new Set();
  let q1Maps = 0;
  for (const root of roots) {
    const mapsRoot = path.join(root, "maps");
    for (const file of walk(mapsRoot).filter((name) => name.toLowerCase().endsWith(".bsp"))) {
      const data = fs.readFileSync(file);
      if (data.length < 8) continue;
      const ident = data.toString("ascii", 0, 4);
      const version = data.readInt32LE(4);
      const q1Version = data.readInt32LE(0);
      const name = qpath(root, file);
      if (ident !== "IBSP" || version !== 46) {
        if (q1Version === 29) q1Maps++;
        maps.push({ name, format: q1Version === 29 ? "q1" : `${ident}:${version}`, materials: 0 });
        continue;
      }
      const offset = data.readInt32LE(16);
      const length = data.readInt32LE(20);
      if (offset < 0 || length < 0 || offset + length > data.length || length % 72 !== 0) {
        throw new Error(`invalid Q3 shader lump in ${file}`);
      }
      const local = new Set();
      for (let cursor = offset; cursor < offset + length; cursor += 72) {
        const nul = data.indexOf(0, cursor);
        const end = nul >= cursor && nul < cursor + 64 ? nul : cursor + 64;
        const material = data.toString("utf8", cursor, end).toLowerCase();
        if (material && material !== "noshader") {
          local.add(material);
          materials.add(material);
        }
      }
      maps.push({ name, format: "q3", materials: local.size });
    }
  }
  return { maps, materials, q1Maps };
}

const content = new Map();
for (const root of options.contentRoots) {
  for (const file of walk(root)) {
    const name = qpath(root, file);
    if (!content.has(name.toLowerCase())) content.set(name.toLowerCase(), name);
  }
}

let aliasSource = fs.readFileSync(options.aliases, "utf8");
const aliases = new Map();
const duplicateAliases = new Set();
for (const match of aliasSource.matchAll(/\[\s*"([^"]+)"\s*\]\s*=\s*"([^"]+)"/g)) {
  const key = match[1].toLowerCase();
  if (aliases.has(key)) duplicateAliases.add(key);
  aliases.set(key, match[2].toLowerCase());
}

function alternatives(requested) {
  const lower = requested.toLowerCase();
  const stem = lower.slice(0, lower.length - path.extname(lower).length);
  return [...content.entries()]
    .filter(([name]) => IMAGE_EXTENSIONS.has(path.extname(name)) && name.slice(0, name.length - path.extname(name).length) === stem)
    .map(([, canonical]) => canonical)
    .sort((a, b) => a.localeCompare(b));
}

function classify(refs, allowAliases) {
  const exact = [], aliased = [], candidates = [], unresolved = [], brokenAliases = [];
  for (const ref of refs) {
    const requested = ref.requested.toLowerCase();
    if (content.has(requested)) { exact.push(ref); continue; }
    const target = aliases.get(requested);
    if (target) {
      if (content.has(target) && allowAliases) aliased.push({ ...ref, target: content.get(target) });
      else if (content.has(target)) candidates.push({ ...ref, target: content.get(target) });
      else brokenAliases.push({ ...ref, target });
      continue;
    }
    const found = alternatives(ref.requested);
    if (found.length === 1) candidates.push({ ...ref, target: found[0] });
    else unresolved.push({ ...ref, alternatives: found });
  }
  return { exact, aliased, candidates, unresolved, brokenAliases };
}

const declaredMaterials = new Set([
  ...shaderMaterials(options.importedRoot),
  ...shaderMaterials(options.ownedRoot)
]);
const importedShaderRefs = shaderReferences(options.importedRoot);
const importedModelRefs = md3References(options.importedRoot, declaredMaterials);
const imported = classify([...importedShaderRefs, ...importedModelRefs], true);
// Owned shader sources must name packaged resources directly even when a
// compatibility alias happens to cover their legacy spelling.
const owned = classify(shaderReferences(options.ownedRoot), false);
const bsp = bspInventory(options.mapRoots);
const brokenCatalogTargets = [...aliases.entries()]
  .filter(([, target]) => !content.has(target))
  .map(([requested, target]) => ({ requested, target }));

function reachable(result) {
  const select = (records) => records.filter((record) => bsp.materials.has(record.material.toLowerCase()));
  return {
    exact: select(result.exact),
    aliased: select(result.aliased),
    candidates: select(result.candidates),
    unresolved: select(result.unresolved),
    brokenAliases: select(result.brokenAliases)
  };
}

const reachableImported = reachable(imported);
const reachableOwned = reachable(owned);

function uniqueMappings(records) {
  const grouped = new Map();
  for (const record of records) {
    const key = record.requested.toLowerCase();
    if (!grouped.has(key)) grouped.set(key, { requested: record.requested, target: record.target, sources: new Set() });
    grouped.get(key).sources.add(record.sourceFile);
  }
  return [...grouped.values()].sort((a, b) => a.requested.localeCompare(b.requested));
}

if (options.fix) {
  // Wired-owned content has no legacy spelling contract: update the source.
  const byFile = new Map();
  for (const record of owned.candidates) {
    if (!byFile.has(record.file)) byFile.set(record.file, new Map());
    byFile.get(record.file).set(record.requested, record.target);
  }
  for (const [file, replacements] of byFile) {
    let source = fs.readFileSync(file, "utf8");
    for (const [from, to] of replacements) source = source.split(from).join(to);
    fs.writeFileSync(file, source);
  }

  // Imported sources remain untouched. Keep source-file provenance beside
  // every alias they consume, including aliases that predate this audit.
  const provenance = new Map();
  for (const record of shaderReferences(options.importedRoot)) {
    const key = record.requested.toLowerCase();
    if (!aliases.has(key)) continue;
    if (!provenance.has(key)) provenance.set(key, new Set());
    provenance.get(key).add(record.sourceFile);
  }
  aliasSource = aliasSource.split(/\r?\n/).map((line) => {
    const match = line.match(/^(\s*)\["([^"]+)"\]\s*=\s*"([^"]+)",(.*)$/);
    if (!match) return line;
    const sources = provenance.get(match[2].toLowerCase());
    if (!sources) return line;
    const existing = match[4].replace(/\s*--\s*used by pax01.*$/, "").trimEnd();
    const separator = existing.includes("--") ? "; " : " -- ";
    return `${match[1]}[${JSON.stringify(match[2])}] = ${JSON.stringify(match[3])},${existing}${separator}used by pax01 ${[...sources].sort().join(", ")}`;
  }).join("\n");

  // Append deterministic exact aliases that were not already present;
  // running the maintenance pass twice is a no-op.
  const mappings = uniqueMappings(imported.candidates);
  if (mappings.length) {
    const insertion = aliasSource.lastIndexOf("    },");
    if (insertion < 0) throw new Error(`could not find files table terminator in ${options.aliases}`);
    const lines = mappings.map(({ requested, target, sources }) =>
      `        [${JSON.stringify(requested)}] = ${JSON.stringify(target)}, -- used by pax01 ${[...sources].sort().join(", ")}`
    );
    aliasSource = `${aliasSource.slice(0, insertion)}${lines.join("\n")}\n${aliasSource.slice(insertion)}`;
  }
  fs.writeFileSync(options.aliases, aliasSource);
}

const importedMappings = uniqueMappings(imported.candidates);
const ownedMappings = uniqueMappings(owned.candidates);
console.log(JSON.stringify({
  imported: {
    references: imported.exact.length + imported.aliased.length + imported.candidates.length + imported.unresolved.length + imported.brokenAliases.length,
    candidateAliases: importedMappings.length,
    unresolved: uniqueMappings(imported.unresolved.map((item) => ({ ...item, target: "" }))).length,
    brokenAliases: imported.brokenAliases.length
  },
  models: {
    rawImageReferences: importedModelRefs.length
  },
  owned: {
    candidateCanonicalizations: ownedMappings.length,
    unresolved: uniqueMappings(owned.unresolved.map((item) => ({ ...item, target: "" }))).length,
    brokenAliases: owned.brokenAliases.length
  },
  maps: {
    scanned: bsp.maps.length,
    q3: bsp.maps.length - bsp.q1Maps,
    q1: bsp.q1Maps,
    referencedMaterials: bsp.materials.size,
    reachableImportedUnresolved: uniqueMappings(reachableImported.unresolved.map((item) => ({ ...item, target: "" }))).length,
    reachableOwnedUnresolved: uniqueMappings(reachableOwned.unresolved.map((item) => ({ ...item, target: "" }))).length,
    reachableBrokenAliases: reachableImported.brokenAliases.length + reachableOwned.brokenAliases.length
  },
  aliasCatalog: {
    mappings: aliases.size,
    duplicateKeys: duplicateAliases.size,
    missingTargets: brokenCatalogTargets.length
  },
  changed: options.fix
}, null, 2));

// Imported id shader sets contain optional and duplicate material definitions
// whose source images were never shipped in the selected data packs. Report
// those unresolved references for review, but exact-extension conformance only
// fails on an actionable canonical alternative, a broken alias, or owned
// content. The runtime map-transition gate fail-closes on the definition the
// renderer actually selects.
if (duplicateAliases.size || brokenCatalogTargets.length || reachableOwned.unresolved.length || reachableImported.brokenAliases.length || reachableOwned.brokenAliases.length || (!options.fix && (importedMappings.length || ownedMappings.length))) process.exit(1);
