// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const read = (rel) => fs.readFileSync(path.join(root, rel), "utf8");
const parser = read("code/client/wired/ui/cl_wired_parse.c");
const dump = read("code/client/wired/ui/cl_wired_layout_dump.c");
const header = read("code/client/wired/ui/cl_wired_layout.h");
const scripting = read("code/qcommon/wired/core/scripting/wired_scripting.c");
const luaHarness = read("code/client/wired/ui/cl_wired_lua_test.c");

function requireMatch(text, pattern, message) {
  if (!pattern.test(text)) throw new Error(message);
}

requireMatch(header, /void\s+WUI_InspectorInit\s*\(/,
  "native inspector lifecycle API missing");
for (const field of [
  "wired-ui-inspector/v1", "styleProvenance", "bindings", "generation",
  "rect", "layout", "focused", "hovered", "source", "layer", "parent", "depth",
]) {
  if (!dump.includes(field)) throw new Error(`inspector protocol field missing: ${field}`);
}
requireMatch(dump, /WiredStore_Get\s*\(/,
  "inspector must read binding provenance from the authoritative WiredStore");
requireMatch(dump, /WiredUI_ClayItemRenderedRect\s*\(/,
  "inspector must read computed layout from Clay authority");
requireMatch(dump, /Cmd_AddCommand\(\s*"wui_inspect"/,
  "selected-node inspector command missing");

const reloadStart = parser.indexOf("qboolean WiredUI_SafeReload");
const reloadEnd = parser.indexOf("// ── menus.lua support", reloadStart);
if (reloadStart < 0 || reloadEnd < 0) throw new Error("SafeReload function not found");
const reload = parser.slice(reloadStart, reloadEnd);
for (const invariant of [
  "WiredUI_BackupMenuChunks", "WiredUI_LoadMenusFromLua",
  "WiredUI_LoadExplicitMenus", "WiredUI_PurgeMenuChunks",
  "WiredUI_ReleaseBackupChunks", "keeping old menus",
]) {
  if (!reload.includes(invariant)) throw new Error(`transaction invariant missing: ${invariant}`);
}
if (reload.includes("WiredUI_ClearMenus();")) {
  throw new Error("SafeReload releases last-good chunks before staged validation");
}
requireMatch(parser, /qboolean\s+WiredUI_LoadMenusFromLua[\s\S]*WiredScript_TryExecFile/,
  "menus.lua execution failure is not propagated to the transaction");
requireMatch(parser, /Cvar_Get\(\s*"wired_ui_manifest"[\s\S]*WiredScript_TryExecFile\(\s*path\s*\)/,
  "distinct loose developer manifest is not routed through checked execution");
requireMatch(scripting, /qboolean\s+WiredScript_TryExecFile[\s\S]*return qtrue;/,
  "checked Lua file execution seam missing");

for (const invariant of [
  "WiredScript_TryExecFile", "CL_KeyEvent", "WiredStore_Set",
  "WiredStore_Get", "WiredUI_GetFocusedItem", "WiredUI_ClayItemRenderedRect",
  "WiredUI Lua harness: PASS", "screenshot",
]) {
  if (!luaHarness.includes(invariant)) {
    throw new Error(`Lua UI harness authority/action missing: ${invariant}`);
  }
}

const forbidden = `${dump}\n${parser}\n${luaHarness}`;
if (/RmlUi|Rml::|DocumentObjectModel|DOMNode/.test(forbidden)) {
  throw new Error("inspector introduced a DOM/RmlUi authority");
}

console.log("PASS WiredUI inspector protocol + transactional hot-reload source contract");
