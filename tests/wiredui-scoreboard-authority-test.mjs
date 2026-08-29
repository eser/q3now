#!/usr/bin/env node

// Pins the scoreboard ownership decision: authored .wui menus are the only
// primary presentation path.  The C HUD state may call the native scorelist
// drawers only from the bounded `sbMenu == NULL` recovery branch, so a missing
// or broken mod UI still leaves the match usable without double-emitting a
// scoreboard during normal play.

import fs from "node:fs";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const scoreboardMenus = [
  "ingame_scoreboard_ffa.wui",
  "ingame_scoreboard_duel.wui",
  "ingame_scoreboard_tdm.wui",
  "ingame_scoreboard_ctf.wui",
  "end_scoreboard_ffa.wui",
  "end_scoreboard_duel.wui",
  "end_scoreboard_tdm.wui",
  "end_scoreboard_ctf.wui",
];

function fail(message) {
  console.error(`FAIL: ${message}`);
  process.exitCode = 1;
}

for (const name of scoreboardMenus) {
  const source = fs.readFileSync(path.join(root, "modfiles/ui", name), "utf8");
  if (!/^\s*type\s+scorelist_widget\s*$/m.test(source)) {
    fail(`${name} has no declarative scorelist_widget`);
  }
}

const classic = fs.readFileSync(path.join(root, "modfiles/ui/classic.wui"), "utf8");
const classicWithoutComments = classic.replace(/\/\/.*$/gm, "");
if (/\bscorelist_widget\b/.test(classicWithoutComments)) {
  fail("classic.wui must not emit a normal-play scoreboard");
}

const statePath = path.join(root, "code/client/wired/ui/cl_wired_ui_hud_state.c");
const state = fs.readFileSync(statePath, "utf8");
const fallbackStart = state.indexOf("} else {", state.indexOf("if ( sbMenu )"));
const profileEnd = state.indexOf("cl_prof.whud_score", fallbackStart);
if (fallbackStart < 0 || profileEnd < 0) {
  fail("could not locate the bounded missing-menu fallback branch");
} else {
  const fallback = state.slice(fallbackStart, profileEnd);
  const primary = state.slice(0, fallbackStart) + state.slice(profileEnd);
  const directDraw = /\bWiredHud_Draw(?:ScorelistWidget|DuelBoard)\s*\(/g;
  const fallbackCalls = fallback.match(directDraw) ?? [];
  const primaryCalls = primary.match(directDraw) ?? [];

  // The two extern declarations are outside the branch; no executable direct
  // calls may be.  Four calls cover duel, two team cohorts and FFA recovery.
  if (primaryCalls.length !== 2) {
    fail(`HUD state has ${primaryCalls.length - 2} direct scoreboard call(s) outside fallback`);
  }
  if (fallbackCalls.length !== 4) {
    fail(`missing-menu fallback has ${fallbackCalls.length} direct draws; expected 4`);
  }
}

if (!process.exitCode) {
  console.log("PASS: declarative scoreboard owns all 8 primary menus; native draws are missing-menu fallback only");
}
