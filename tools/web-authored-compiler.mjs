// SPDX-License-Identifier: GPL-3.0-or-later

import { createHash } from "node:crypto";
import { readFile, writeFile, mkdir } from "node:fs/promises";
import { dirname, resolve } from "node:path";

const args = new Map();
for (let i = 2; i < process.argv.length; i += 2) args.set(process.argv[i], process.argv[i + 1]);
for (const key of ["--output", "--menu", "--servers", "--console", "--loading", "--ui-dir", "--menu-manifest", "--hud", "--crosshair",
  "--crosshair-machinegun", "--crosshair-lightning", "--l10n", "--scene",
  "--tokens", "--theme-mode", "--theme-accent"])
  if (!args.get(key)) throw new Error(`missing ${key}`);

const menuPath = resolve(args.get("--menu"));
const authoredRoot = dirname(dirname(menuPath));
const expandIncludes = async (source, stack = []) => {
  const expression = /^\s*#include\s+"([^"]+)"\s*$/gm;
  let output = "", offset = 0;
  for (const match of source.matchAll(expression)) {
    const includePath = resolve(authoredRoot, match[1]);
    if (stack.includes(includePath)) throw new Error(`cyclic menu include ${match[1]}`);
    output += source.slice(offset, match.index);
    output += await expandIncludes(await readFile(includePath, "utf8"), [...stack, includePath]);
    offset = match.index + match[0].length;
  }
  return output + source.slice(offset);
};
const [menuRootSource, serverRootSource, consoleRootSource, loadingRootSource,
  hudRootSource, crosshairSource, machinegunCrosshairSource,
  lightningCrosshairSource, l10nSource, sceneSource, tokenSource, modeTokenSource,
  accentTokenSource] = await Promise.all([
  readFile(menuPath, "utf8"),
  readFile(args.get("--servers"), "utf8"),
  readFile(args.get("--console"), "utf8"),
  readFile(args.get("--loading"), "utf8"),
  readFile(args.get("--hud"), "utf8"),
  readFile(args.get("--crosshair"), "utf8"),
  readFile(args.get("--crosshair-machinegun"), "utf8"),
  readFile(args.get("--crosshair-lightning"), "utf8"),
  readFile(args.get("--l10n"), "utf8"),
  readFile(args.get("--scene"), "utf8"),
  readFile(args.get("--tokens"), "utf8"),
  readFile(args.get("--theme-mode"), "utf8"),
  readFile(args.get("--theme-accent"), "utf8")
]);
const menuSource = await expandIncludes(menuRootSource, [menuPath]);
const serverSource = await expandIncludes(serverRootSource, [resolve(args.get("--servers"))]);
const consoleSource = await expandIncludes(consoleRootSource, [resolve(args.get("--console"))]);
const loadingSource = await expandIncludes(loadingRootSource, [resolve(args.get("--loading"))]);
const hudSource = await expandIncludes(hudRootSource, [resolve(args.get("--hud"))]);
const menuManifestSource = await readFile(args.get("--menu-manifest"), "utf8");
const uiDir = resolve(args.get("--ui-dir"));
const sourcePaths = [...menuManifestSource.matchAll(/load_menu\("(ui\/[^"\r\n]+\.(?:wui|wmenu|whud))"\)/g)]
  .map(match => match[1]);
if (sourcePaths.length < 32 || sourcePaths.length > 64)
  throw new Error(`invalid authored source count ${sourcePaths.length}`);
if (new Set(sourcePaths).size !== sourcePaths.length)
  throw new Error("duplicate authored source path");
const authoredSources = [];
const roots = [];
for (const sourcePath of sourcePaths) {
  const source = await readFile(resolve(uiDir, sourcePath.slice(3)), "utf8");
  authoredSources.push({ sourcePath, source });
  const menu = source.match(/\bmenuDef\s*\{[\s\S]*?\bname\s+"([^"]+)"/);
  if (!menu) continue;
  const layer = source.match(/\bmenuDef\s*\{[\s\S]*?\blayer\s+"([^"]+)"/)?.[1] ?? "menu";
  if (layer !== "menu" && layer !== "popup") continue;
  roots.push({ sourcePath, menuName: menu[1], layer: layer === "menu" ? 1 : 2, source });
}
roots.sort((a, b) => a.sourcePath.localeCompare(b.sourcePath));
if (roots.length < 20 || roots.length > 48) throw new Error(`invalid MENU/POPUP root count ${roots.length}`);
if (new Set(roots.map(root => root.menuName)).size !== roots.length)
  throw new Error("duplicate authored MENU/POPUP root name");
const hex = value => Buffer.from(String(value), "utf8").toString("hex");
const tokenValues = new Map();
for (const source of [tokenSource, modeTokenSource, accentTokenSource]) {
  for (const match of source.matchAll(/^\s*token\s+([A-Za-z_][\w]*)\s+(?:"([^"]+)"|([^\s/]+))/gm))
    tokenValues.set(match[1], match[2] ?? match[3]);
}
const colorToken = name => {
  const raw = tokenValues.get(name);
  if (!raw) throw new Error(`missing authored color token ${name}`);
  let values;
  const hexColor = raw.match(/^#([0-9a-f]{6})([0-9a-f]{2})?$/i);
  const rgba = raw.match(/^rgba\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*\)$/i);
  if (hexColor) {
    values = [0, 2, 4].map(offset => parseInt(hexColor[1].slice(offset, offset + 2), 16) / 255);
    values.push(hexColor[2] ? parseInt(hexColor[2], 16) / 255 : 1);
  } else if (rgba) {
    values = rgba.slice(1).map(Number);
    for (let i = 0; i < 3; ++i) if (values[i] > 1) values[i] /= 255;
  } else throw new Error(`unsupported authored color token ${name}: ${raw}`);
  if (values.some(value => !Number.isFinite(value) || value < 0 || value > 1))
    throw new Error(`out-of-range authored color token ${name}: ${raw}`);
  return values.map(value => Number(value.toFixed(6)));
};
const paletteNames = ["ink", "panel", "line", "bone", "boneDim", "accent", "accentDim", "accentSoft"];
const palette = paletteNames.flatMap(colorToken);
const number = (source, expression, label) => {
  const match = source.match(expression);
  if (!match || !Number.isFinite(Number(match[1]))) throw new Error(`missing ${label}`);
  return Number(match[1]);
};
const namedBlock = (source, name) => {
  const marker = source.indexOf(`name "${name}"`);
  if (marker < 0) throw new Error(`missing menu block ${name}`);
  const start = source.lastIndexOf("itemDef", marker);
  const brace = source.indexOf("{", start);
  let depth = 0;
  for (let i = brace; i < source.length; ++i) {
    if (source[i] === "{") ++depth;
    else if (source[i] === "}" && --depth === 0) return source.slice(brace + 1, i);
  }
  throw new Error(`unterminated menu block ${name}`);
};
const size = (source, property, unit, label) => number(source,
  new RegExp(`\\b${property}\\s+(?:FIXED\\s+)?([\\d.]+)${unit}\\b`), label);
const padding = (source, label) => {
  const match = source.match(/\bpadding\s+([^\s{}]+)\s+([^\s{}]+)\s+([^\s{}]+)\s+([^\s{}]+)/);
  if (!match) throw new Error(`missing ${label} padding`);
  const value = (token, unit, edge) => {
    const parsed = token.match(/^([\d.]+)([a-z%]*)$/i);
    if (!parsed || !Number.isFinite(Number(parsed[1]))
        || (Number(parsed[1]) !== 0 && parsed[2] !== unit))
      throw new Error(`invalid ${label} ${edge} padding`);
    return Number(parsed[1]);
  };
  return [value(match[1], "vh", "top"), value(match[2], "vw", "right"),
    value(match[3], "vh", "bottom"), value(match[4], "vw", "left")];
};
const tableBlock = (source, name) => {
  const marker = source.indexOf(name);
  const brace = source.indexOf("{", marker);
  if (marker < 0 || brace < 0) throw new Error(`missing scene block ${name}`);
  let depth = 0;
  for (let i = brace; i < source.length; ++i) {
    if (source[i] === "{") ++depth;
    else if (source[i] === "}" && --depth === 0) return source.slice(brace + 1, i);
  }
  throw new Error(`unterminated scene block ${name}`);
};

if (!/menuDef\s*\{[\s\S]*?name\s+"main"/.test(menuSource))
  throw new Error("authored main menu is missing");
if (!/menuDef\s*\{[\s\S]*?name\s+"servers"/.test(serverSource))
  throw new Error("authored server browser is missing");
if (!/menuDef\s*\{[\s\S]*?name\s+"console_panel"[\s\S]*?layer\s+"console"/.test(consoleSource))
  throw new Error("authored console panel is missing");
if (!/menuDef\s*\{[\s\S]*?name\s+"loading_screen"[\s\S]*?layer\s+"loading"/.test(loadingSource))
  throw new Error("authored loading screen is missing");
const left = namedBlock(menuSource, "left_region");
const right = namedBlock(menuSource, "right_region");
const content = namedBlock(menuSource, "main_content");
const topBar = namedBlock(menuSource, "top_bar");
const contentPadding = padding(content, "main content");
const leftPadding = padding(left, "left region");
const topBarHeight = size(topBar, "height", "vh", "top bar height");
const menuItems = [...menuRootSource.matchAll(/QW_MENU_ITEM\(\s*"([^"]+)"\s*,\s*"[^"]+"\s*,\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*\{([^}]*)\}\s*\)/g)]
  .map(match => {
    const open = match[4].match(/\bopen\s+"([^"]+)"/);
    const exec = match[4].match(/\bexec\s+"([^"]+)"/);
    if (!open && !exec) throw new Error(`unsupported authored action for ${match[1]}`);
    return { id: match[1], label: match[2], subtitle: match[3],
      actionKind: open ? 1 : 2, action: (open ?? exec)[1] };
  });
if (menuItems.length < 1 || menuItems.length > 16) throw new Error("invalid authored main-menu item count");
const layout = {
  left: [contentPadding[3], topBarHeight + contentPadding[0] + leftPadding[0],
    size(left, "width", "vw", "left width")],
  right: [contentPadding[1], topBarHeight + contentPadding[0],
    size(right, "width", "vw", "right width")]
};
const cardNames = ["card_profile", "card_last_match", "card_global_feed", "card_arena"];
const cards = cardNames.map(name => {
  const block = namedBlock(menuSource, name);
  const lines = [...block.matchAll(/\btext\s+"([^"]+)"/g)]
    .map(match => {
      const end = block.indexOf("\n", match.index);
      const declaration = block.slice(match.index, end < 0 ? block.length : end);
      return { text: match[1], binding: declaration.match(/\bcvar\s+"([^"]+)"/)?.[1] ?? "" };
    }).slice(0, 8);
  if (lines.length < 1) throw new Error(`authored card ${name} has no text`);
  return { name, height: size(block, "height", "vh", `${name} height`), lines };
});
const serverList = namedBlock(serverSource, "serverlist");
const serverRowHeight = number(serverList, /\belementheight\s+([\d.]+)/, "server row height");
const serverColumnMatch = serverList.match(/\bcolumns\s+(\d+)\s+([^\n\r]+)/);
const serverHeaderMatch = serverList.match(/\bcolumnHeaders\s*\{([^}]*)\}/);
if (!serverColumnMatch || !serverHeaderMatch) throw new Error("missing authored server columns");
const serverColumnCount = Number(serverColumnMatch[1]);
const serverColumnValues = serverColumnMatch[2].trim().split(/\s+/).map(Number);
const serverHeaders = new Map([...serverHeaderMatch[1].matchAll(/(\d+)\s+"([^"]+)"/g)]
  .map(match => [Number(match[1]), match[2]]));
if (!Number.isInteger(serverColumnCount) || serverColumnCount < 1 || serverColumnCount > 8
    || serverColumnValues.length !== serverColumnCount * 3
    || serverColumnValues.some(value => !Number.isFinite(value)))
  throw new Error("invalid authored server columns");
const serverExtent = Math.max(...Array.from({ length: serverColumnCount }, (_, index) =>
  serverColumnValues[index * 3] + serverColumnValues[index * 3 + 1]));
const serverColumns = Array.from({ length: serverColumnCount }, (_, index) => ({
  title: serverHeaders.get(index) ?? `COLUMN ${index + 1}`,
  widthPercent: Number((serverColumnValues[index * 3 + 1] * 100 / serverExtent).toFixed(6))
}));
const consoleView = namedBlock(consoleSource, "console_view");
const consoleHeight = number(consoleView,
  /\bheight\s+FIXED\s+([\d.]+)/, "console view height");
if (consoleHeight <= 0 || consoleHeight > 1) throw new Error("invalid authored console height");
const loadingTopBar = namedBlock(loadingSource, "loading_topbar");
const loadingLeftPanel = namedBlock(loadingSource, "loading_left_panel");
const loadingDivider = namedBlock(loadingSource, "loading_divider");
const loadingBottom = namedBlock(loadingSource, "loading_bottom_strip");
const loadingPhase = namedBlock(loadingSource, "loading_phase_text");
const loadingBar = namedBlock(loadingSource, "loading_overall_bar");
const loadingFooter = namedBlock(loadingSource, "loading_server_strip_label");
const loadingFooterText = loadingFooter.match(/\btext\s+"([^"]*)"/)?.[1];
if (!loadingFooterText) throw new Error("missing authored loading footer text");
for (const custom of ["loading_backdrop", "loading_topbar", "loading_wireframe",
  "loading_maptitle_block", "loading_mapinfo_stats", "loading_streaming_rows",
  "loading_server_info_strip", "loading_vulkan_badge"])
  if (!new RegExp(`\\bcustom\\s+"${custom}"`).test(loadingSource))
    throw new Error(`missing authored loading custom draw ${custom}`);
const loading = [
  size(loadingTopBar, "height", "vh", "loading top bar height"),
  size(loadingLeftPanel, "width", "vw", "loading left panel width"),
  size(loadingDivider, "width", "vw", "loading divider width"),
  size(loadingBottom, "height", "vh", "loading bottom strip height"),
  size(loadingPhase, "height", "vh", "loading phase height"),
  size(loadingBar, "height", "vh", "loading overall bar height")
];
const hudGrid = namedBlock(hudSource, "hud_corner_grid");
const healthPanel = namedBlock(hudSource, "hud_active_health_panel");
const armorPanel = namedBlock(hudSource, "hud_active_armor_panel");
const ammoPanel = namedBlock(hudSource, "hud_ammo_readout");
const hudPadding = padding(hudGrid, "HUD corner grid");
const hud = [
  hudPadding[3], hudPadding[1], hudPadding[2],
  size(healthPanel, "width", "vw", "health panel width"),
  size(armorPanel, "width", "vw", "armor panel width"),
  size(ammoPanel, "width", "vw", "ammo panel width"),
  size(healthPanel, "height", "vh", "HUD panel height")
];
const optionalNumber = (source, expression, fallback) => {
  const match = source.match(expression);
  return match && Number.isFinite(Number(match[1])) ? Number(match[1]) : fallback;
};
const optionalBool = (source, expression, fallback) => {
  const match = source.match(expression);
  return match ? match[1] === "true" : fallback;
};
const crosshairSpec = (source, weapon, dynamicKind, inherited = null) => {
  const base = tableBlock(source, "base");
  const arms = tableBlock(base, "arms");
  const top = tableBlock(arms, "top");
  const dot = base.includes("dot") ? tableBlock(base, "dot") : "";
  const ring = base.includes("ring") ? tableBlock(base, "ring") : "";
  const outline = base.includes("outline") ? tableBlock(base, "outline") : "";
  const colorMatch = base.match(/\bcolor\s*=\s*\{\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)/);
  const inheritedColor = inherited?.color ?? [1, 1, 1, 1];
  const color = colorMatch ? colorMatch.slice(1).map(Number) : inheritedColor;
  return {
    weapon, dynamicKind, color,
    gap: optionalNumber(base, /\bgap\s*=\s*([\d.]+)/, inherited?.gap ?? 0),
    armLength: optionalNumber(top, /\blength\s*=\s*([\d.]+)/, inherited?.armLength ?? 0),
    armThickness: optionalNumber(top, /\bthickness\s*=\s*([\d.]+)/, inherited?.armThickness ?? 1),
    dotEnabled: optionalBool(dot, /\benabled\s*=\s*(true|false)/, inherited?.dotEnabled ?? false),
    dotRadius: optionalNumber(dot, /\bradius\s*=\s*([\d.]+)/, inherited?.dotRadius ?? 0),
    ringEnabled: optionalBool(ring, /\benabled\s*=\s*(true|false)/, inherited?.ringEnabled ?? false),
    ringRadius: optionalNumber(ring, /\bradius\s*=\s*([\d.]+)/, inherited?.ringRadius ?? 0),
    ringThickness: optionalNumber(ring, /\bthickness\s*=\s*([\d.]+)/, inherited?.ringThickness ?? 1),
    outlineThickness: optionalNumber(outline, /\bthickness\s*=\s*([\d.]+)/, inherited?.outlineThickness ?? 0),
    outlineAlpha: optionalNumber(outline, /\balpha\s*=\s*([\d.]+)/, inherited?.outlineAlpha ?? 0)
  };
};
const defaultCrosshair = crosshairSpec(crosshairSource, 0, 0);
const crosshairs = [defaultCrosshair,
  crosshairSpec(machinegunCrosshairSource, 2, 1, defaultCrosshair),
  crosshairSpec(lightningCrosshairSource, 6, 2, defaultCrosshair)];
const crosshairRecord = spec => [spec.weapon, spec.dynamicKind, ...spec.color,
  spec.gap, spec.armLength, spec.armThickness, spec.dotEnabled ? 1 : 0,
  spec.dotRadius, spec.ringEnabled ? 1 : 0, spec.ringRadius, spec.ringThickness,
  spec.outlineThickness, spec.outlineAlpha].join("|");

const l10n = [...l10nSource.matchAll(/\[\s*"([^"]+)"\s*\]\s*=\s*"([^"]*)"/g)]
  .map(match => ({ key: match[1], value: match[2] }));
if (l10n.length < 1 || l10n.length > 128) throw new Error("invalid localization table");

const eye = tableBlock(sceneSource, "eyePath");
const knots = [...eye.matchAll(/\{\s*t\s*=\s*(-?[\d.]+)\s*,\s*pos\s*=\s*\{\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\}/g)]
  .map(match => match.slice(1).map(Number));
if (knots.length < 2 || knots.length > 64) throw new Error("invalid authored scene eye path");
const fovBlock = tableBlock(sceneSource, "fov");
const fov = [number(fovBlock, /start\s*=\s*(-?[\d.]+)/, "fov start"),
  number(fovBlock, /(?:\[\s*['"]end['"]\]|end)\s*=\s*(-?[\d.]+)/, "fov end"),
  number(fovBlock, /length\s*=\s*(-?[\d.]+)/, "fov length")];
const eventBlock = tableBlock(sceneSource, "events");
const events = [...eventBlock.matchAll(/\{([^{}]+)\}/g)].map(match => {
  const body = match[1];
  const verb = body.match(/verb\s*=\s*['"]([^'"]+)['"]/)?.[1];
  const time = Number(body.match(/time\s*=\s*(-?[\d.]+)/)?.[1] ?? 0);
  if (!verb || !Number.isFinite(time)) throw new Error("invalid authored scene event");
  const a = Number(body.match(/(?:fov|seconds|on)\s*=\s*(-?[\d.]+)/)?.[1] ?? 0);
  const b = Number(body.match(/length\s*=\s*(-?[\d.]+)/)?.[1] ?? 0);
  const text = body.match(/(?:name|key)\s*=\s*['"]([^'"]*)['"]/)?.[1] ?? "";
  return { verb, time, a, b, text };
});
if (events.length > 64) throw new Error("too many authored scene events");

const digest = createHash("sha256").update(menuSource).update("\0")
  .update(serverSource).update("\0")
  .update(consoleSource).update("\0").update(loadingSource).update("\0")
  .update(hudSource).update("\0").update(crosshairSource).update("\0")
  .update(machinegunCrosshairSource).update("\0").update(lightningCrosshairSource).update("\0")
  .update(l10nSource).update("\0").update(sceneSource).update("\0")
  .update(tokenSource).update("\0").update(modeTokenSource).update("\0")
  .update(accentTokenSource).update("\0").update(menuManifestSource).update("\0")
  .update(authoredSources.map(entry => `${entry.sourcePath}\0${entry.source}`).join("\0"))
  .digest("hex");
const lines = [`WAC1|${digest}`,
  `PALETTE|${palette.join("|")}`,
  `MENU|${hex("main")}|${layout.left.join("|")}|${layout.right.join("|")}`,
  ...menuItems.map(item => `ITEM|${hex(item.id)}|${hex(item.label)}|${hex(item.subtitle)}|${item.actionKind}|${hex(item.action)}`),
  ...cards.map(card => `CARD|${hex(card.name)}|${card.height}|${card.lines.length}|${card.lines.flatMap(line => [hex(line.text), hex(line.binding)]).join("|")}`),
  `SERVER|${hex("servers")}|${serverRowHeight}|${serverColumns.length}`,
  ...serverColumns.map(column => `SCOL|${hex(column.title)}|${column.widthPercent}`),
  `CONSOLE|${hex("console_panel")}|${consoleHeight * 100}`,
  `LOADING|${hex("loading_screen")}|${loading.join("|")}|${hex(loadingFooterText)}`,
  ...sourcePaths.map(sourcePath => `SOURCE|${hex(sourcePath)}`),
  ...roots.map(root => `ROOT|${hex(root.sourcePath)}|${hex(root.menuName)}|${root.layer}`),
  ...l10n.map(entry => `L10N|${hex(entry.key)}|${hex(entry.value)}`),
  `HUD|${hud.join("|")}`,
  ...crosshairs.map(spec => `XHAIR|${crosshairRecord(spec)}`),
  `SCENE|${hex("arena1")}|${number(sceneSource, /\btime\s*=\s*(-?[\d.]+)/, "scene time")}|${/cameraSpace\s*=\s*['"]player['"]/.test(sceneSource) ? 1 : 0}|${fov.join("|")}`,
  `CURVE|${/type\s*=\s*['"]tcb['"]/.test(eye) ? 1 : 0}|${/boundary\s*=\s*['"]clamped['"]/.test(eye) ? 1 : /boundary\s*=\s*['"]closed['"]/.test(eye) ? 2 : 0}`,
  ...knots.map(knot => `EYE|${knot.join("|")}`),
  ...events.map(event => `EVENT|${hex(event.verb)}|${event.time}|${event.a}|${event.b}|${hex(event.text)}`),
  "END", ""];
await mkdir(dirname(args.get("--output")), { recursive: true });
await writeFile(args.get("--output"), lines.join("\n"));
console.log(`authored-content ${digest} menu=${menuItems.length} cards=${cards.length} serverColumns=${serverColumns.length} console=1 loading=1 sources=${sourcePaths.length} roots=${roots.length} hud=classic crosshairs=${crosshairs.length} l10n=${l10n.length} knots=${knots.length} events=${events.length}`);
