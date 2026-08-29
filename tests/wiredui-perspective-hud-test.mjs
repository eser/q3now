// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const read = path => readFileSync(join(root, path), "utf8");
const hud = read("modfiles/ui/perspective.wui");
const classic = read("modfiles/ui/classic.wui");
const header = read("code/client/wired/ui/cl_wired_ui.h");
const parser = read("code/client/wired/ui/cl_wired_parse.c");
const clay = read("code/client/wired/ui/cl_wired_clay.c");
const ui = read("code/client/wired/ui/cl_wired_ui.c");
const publicAbi = read("code/render/frontend/tr_public.h");
const submission = read("code/render/frontend/render_submission.c");
const submissionUi = read("code/render/frontend/render_submission_ui.c");
const metalPresent = read("code/render/ral/backends/metal/ral_metal_present.mm");
const vulkanBackend = read("code/render/ral/backends/vulkan/renderer/tr_backend.c");

assert.match(header, /float\s+perspective\s*;/, "item contract lacks perspective");
assert.match(parser, /WP_F\(\s*"perspective"[^\n]*perspective/, "parser lacks perspective keyword");
assert.match(clay, /item->isFlexContainer[\s\S]{0,160}item->perspective/,
  "paint transform is not restricted to flex containers");
assert.doesNotMatch(clay, /WUI_PERSPECTIVE_BANDS|wui_clay_draw_perspective_fill/,
  "perspective regressed to chrome-only strip rendering");
assert.match(publicAbi, /SetUiTransform\)\(\s*const refUiTransform_t \*transform\s*\)/,
  "renderer ABI lacks subtree paint-transform state");
assert.match(clay, /wuiCustomDrawCommand_t[\s\S]{0,900}perspectiveOwner/,
  "detached custom draws do not retain their authored perspective ancestor");
assert.match(clay, /wui_clay_command_tag\(\s*perspectiveOwner[\s\S]{0,300}WUI_TEXT_TAG_SHADOW/,
  "text commands do not carry inherited perspective ancestry");
assert.match(clay, /wui_clay_apply_perspective_paint[\s\S]{0,700}SetUiTransform/,
  "renderer ABI does not receive the perspective paint transform");
assert.match(clay, /wui_clay_apply_perspective_owner[\s\S]{0,900}wui_clay_apply_perspective_paint/,
  "flex subtree does not apply its inherited paint transform");
assert.match(clay, /CLAY_RENDER_COMMAND_TYPE_CUSTOM[\s\S]{0,500}custom->perspectiveOwner/,
  "floating custom commands do not restore their perspective owner");
assert.match(clay, /wui_clay_alloc_custom_command[\s\S]{0,500}Arena_Alloc[\s\S]{0,300}memset\(\s*cmd,\s*0,\s*sizeof\(\s*\*cmd\s*\)\s*\)/,
  "recycled custom-command carriers are not fully initialized");
assert.equal([...clay.matchAll(/cmd\s*=\s*\(wuiCustomDrawCommand_t\s*\*\)\s*Arena_Alloc/g)].length, 1,
  "custom-command emitter bypasses the sole zero-initializing allocator");
assert.doesNotMatch(clay, /wui_clay_end_perspective_for_border|wui_clay_record_perspective_paint/,
  "perspective ancestry must not depend on Clay command ordering");
assert.match(submission, /RenderSubmission_TransformUiPoint[\s\S]{0,1800}primitive->positions/,
  "RAL frontend does not project every submitted UI primitive");
assert.match(submissionUi, /RenderUi_ProjectPoint[\s\S]{0,3000}denominator[\s\S]{0,500}projectedU/,
  "HUD perspective is not a true four-corner homography");
assert.match(submissionUi, /q\[0\]\[1\].*0\.65f[\s\S]{0,500}q\[2\]\[1\].*1\.25f/,
  "HUD plane lacks the reference-calibrated roll and far-edge foreshortening");
assert.match(metalPresent, /primitive->positions\[corner\]/,
  "Metal does not consume projected descendant corners");
assert.match(vulkanBackend, /RB_AddQuadStamp2D\( cmd->positions/,
  "Vulkan does not consume projected descendant corners");

assert.match(hud, /menuDef\s*\{[\s\S]*?name\s+"perspective"/, "selectable HUD root missing");
assert.match(hud, /name\s+"hud_perspective_status"[\s\S]*?type\s+container[\s\S]*?perspective\s+0\.20/,
  "left instrument is not a projected flex container");
assert.match(hud, /name\s+"hud_perspective_ammo"[\s\S]*?type\s+container[\s\S]*?perspective\s+\$hud_perspective_inward/,
  "right instrument does not mirror the projection");
assert.match(hud, /projects the complete painted subtree|projects ordinary[\s\S]*painted subtree/,
  "authored HUD contract does not require bars, text and icons on the same plane");
assert.match(hud, /qw_hud_crosshair\.wui/, "perspective HUD dropped the crosshair");
for (const widget of ["overlay_messages", "objectives", "scoreboard_bar", "msgqueue",
  "rewards", "warmup", "telemetry", "kill_feed", "weapon_carousel", "markers", "subtitle"]) {
  assert.match(hud, new RegExp(`qw_hud_${widget}\\.wui`), `perspective HUD dropped ${widget}`);
}

assert.match(classic, /name\s+"classic"/, "classic HUD was replaced");
assert.match(classic,
  /name\s+"hud_bottom_center"[\s\S]*?type\s+container[\s\S]*?qw_hud_weapon_carousel\.wui/,
  "classic weapon carousel is not owned by a resolved bottom-center flex container");
assert.match(ui, /Cvar_Get\(\s*"hud",\s*"classic"/, "classic must remain the default HUD");

console.log("WiredUI perspective HUD contract: PASS");
