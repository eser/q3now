#!/usr/bin/env node

// TASK-142 C-15: Clay is the only WiredUI tree-layout authority. Keep unit and
// authored-value helpers, but reject resurrection of the retired recursive
// flex/menu pre-pass or a second loading presentation path.

import fs from "node:fs";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const read = (rel) => fs.readFileSync(path.join(root, rel), "utf8");

function fail(message) {
  console.error(`FAIL: ${message}`);
  process.exitCode = 1;
}

const layout = read("code/client/wired/ui/cl_wired_layout.c");
const layoutHeader = read("code/client/wired/ui/cl_wired_layout.h");
const clay = read("code/client/wired/ui/cl_wired_clay.c");
const loading = read("code/client/cl_loading_ui.c");

for (const [name, source] of [["layout source", layout], ["layout header", layoutHeader], ["Clay emitter", clay]]) {
  for (const retired of ["WUI_LayoutMenu", "WUI_LayoutFlex", "WUI_LayoutItem"]) {
    if (source.includes(retired)) fail(`${name} resurrects retired ${retired}`);
  }
}

if (/\bwiredItemDef_t\b|\bchildCount\b|\balloca\s*\(/.test(layout)) {
  fail("cl_wired_layout.c contains a descendant-tree walk primitive");
}
if (layout.split("\n").length > 140) {
  fail(`cl_wired_layout.c grew beyond helper-only scope (${layout.split("\n").length} lines)`);
}

for (const required of [
  "wui_clay_resolve_panel_root",
  "wui_clay_effective_item_rect",
  "wui_clay_seed_panel_child",
  "wui_clay_sync_panel_rects",
]) {
  if (!clay.includes(required)) fail(`Clay authority helper missing: ${required}`);
}

const endLayout = clay.lastIndexOf("cmds = Clay_EndLayout();");
const sync = clay.indexOf("wui_clay_sync_panel_rects( menu );", endLayout);
const dump = clay.indexOf("WUI_DumpLayout( menu );", sync);
if (endLayout < 0 || sync < endLayout || dump < sync) {
  fail("Clay_EndLayout must precede compatibility snapshot sync and layout dump");
}

// CLAY is a single-iteration for-loop whose increment expression closes the
// current element. Returning from its body bypasses that epilogue and leaves
// Clay's open-element stack corrupt. Call-vote exposed this with one leaked
// scope per empty list feeder. The listbox emitter intentionally keeps the
// outer empty frame and conditionally omits rows, so it must contain no return
// after opening that scope.
const listboxStart = clay.indexOf("static void wui_clay_emit_listbox(");
const listboxEnd = clay.indexOf("static void wui_clay_emit_multidropdown(", listboxStart);
const listbox = clay.slice(listboxStart, listboxEnd);
const listboxClayScope = listbox.indexOf("CLAY({");
if (listboxStart < 0 || listboxEnd < 0 || listboxClayScope < 0) {
  fail("could not locate the listbox Clay scope");
} else if (/\breturn\s*;/.test(listbox.slice(listboxClayScope))) {
  fail("listbox emitter returns from inside a CLAY scope and can leak layout elements");
}

// cl_loading_ui.c remains a data/custom-provider module; it may not regain the
// retired competing full-screen presentation entry point.
if (!loading.includes("WiredLoadingCustomDraws_RegisterAll")) {
  fail("loading custom-draw provider registration is missing");
}
if (!loading.includes("CL_PublishLoadingState")) {
  fail("loading state publisher is missing");
}
if (/\bCL_DrawLoadingScreen\s*\(/.test(loading)) {
  fail("cl_loading_ui.c regained a competing full-screen draw entry point");
}

if (!process.exitCode) {
  console.log("PASS: Clay is sole WiredUI tree-layout authority; loading remains data/custom-provider only");
}
