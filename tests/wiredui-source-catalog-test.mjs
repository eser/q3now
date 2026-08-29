#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later

import { readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";

const root = resolve(import.meta.dirname, "..");
const uiRoot = join(root, "modfiles", "ui");
const extensions = /\.(?:wui|wmenu|whud)$/;

function walk(dir) {
  return readdirSync(dir).flatMap((entry) => {
    const path = join(dir, entry);
    return statSync(path).isDirectory() ? walk(path) : [path];
  });
}

function uiPath(path) {
  return `ui/${relative(uiRoot, path).replaceAll("\\", "/")}`;
}

const files = walk(uiRoot).filter((path) => extensions.test(path));
const sources = new Map(files.map((path) => [uiPath(path), readFileSync(path, "utf8")]));
const menuRoots = new Map();
const menuLayers = new Map();
const includes = new Map();
const nonShipping = new Map([
  ["ui/primitives/qw_plasma_bg.wui", "opt-in PlasmaBG authoring template"],
  ["ui/widgets/qw_hud_scoreboard_overlay.wui", "retired duplicate; scoreboard menus own presentation"],
  ["ui/widgets/qw_hud_weapon_placeholder.wui", "non-shipping viewport authoring placeholder"],
]);

for (const [path, source] of sources) {
  const menu = source.match(/\bmenuDef\s*\{[\s\S]*?\bname\s+"([^"]+)"/);
  if (menu) {
    menuRoots.set(path, menu[1]);
    menuLayers.set(path, source.match(/\bmenuDef\s*\{[\s\S]*?\blayer\s+"([^"]+)"/)?.[1] ?? "menu");
  }
  includes.set(path, [...source.matchAll(/^\s*#include\s+"(ui\/[^"\r\n]+)"/gm)].map((m) => m[1]));
}

const manifest = readFileSync(join(root, "modfiles", "scripts", "menus.lua"), "utf8");
const loaded = new Set([...manifest.matchAll(/load_menu\("(ui\/[^"\r\n]+)"\)/g)].map((m) => m[1]));
for (const path of [
  "ui/classic.wui",
  "ui/perspective.wui",
  "ui/loading_screen.wui",
  "ui/overlay.wui",
  "ui/debug_graph.wui",
  "ui/debug_netstats.wui",
]) loaded.add(path);

const failures = [];
for (const path of loaded) {
  if (!sources.has(path)) failures.push(`manifest/system root does not exist: ${path}`);
}
for (const path of menuRoots.keys()) {
  if (!loaded.has(path) && !nonShipping.has(path)) failures.push(`menuDef root is not loaded by menus.lua/system ownership: ${path}`);
}

const reachable = new Set();
function visit(path, owner) {
  if (reachable.has(path)) return;
  reachable.add(path);
  for (const child of includes.get(path) ?? []) {
    if (!sources.has(child)) {
      // wmenumacros.h and other non-WiredUI preprocessor inputs are outside
      // this catalog by design.
      if (extensions.test(child)) failures.push(`${owner} includes missing authored source: ${child}`);
      continue;
    }
    visit(child, owner);
  }
}
for (const path of loaded) if (sources.has(path)) visit(path, path);

const dataOnly = new Set([
  "ui/_tokens.wui",
  "ui/assets.wui",
  "ui/themes/ta/assets.wmenu",
  "ui/themes/amber/_tokens.wui",
  "ui/themes/blood/_tokens.wui",
  "ui/themes/cyan/_tokens.wui",
  "ui/themes/dark/_tokens.wui",
  "ui/themes/light/_tokens.wui",
  "ui/themes/toxic/_tokens.wui",
  "ui/themes/violet/_tokens.wui",
]);
for (const path of dataOnly) reachable.add(path);

// Authored templates/retired fragments are deliberately outside the shipping
// graph. Keeping this list executable prevents an orphan from being silently
// mistaken for covered UI while also preventing these files from accidentally
// entering the production manifest without an explicit catalog decision.
for (const [path] of nonShipping) {
  if (!sources.has(path)) failures.push(`catalogued non-shipping source does not exist: ${path}`);
  if (loaded.has(path) || reachable.has(path)) failures.push(`non-shipping source entered the shipping graph: ${path}`);
}

for (const path of sources.keys()) {
  if (!reachable.has(path) && !nonShipping.has(path)) failures.push(`orphan authored source has no shipping root owner: ${path}`);
}

if (failures.length) {
  for (const failure of failures) console.error(`FAIL: ${failure}`);
  process.exit(1);
}

for (const [path, name] of [...menuRoots].sort()) {
  if (!nonShipping.has(path)) console.log(`${path}\t${name}\t${menuLayers.get(path)}`);
}
console.error(`PASS WiredUI source catalog: ${sources.size} authored files, ${menuRoots.size - nonShipping.size} shipping render roots, ${nonShipping.size} explicit non-shipping sources, zero unclassified orphans`);
