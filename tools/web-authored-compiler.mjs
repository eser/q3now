// SPDX-License-Identifier: GPL-3.0-or-later

import { createHash } from "node:crypto";
import { readFile, writeFile, mkdir } from "node:fs/promises";
import { dirname } from "node:path";

const args = new Map();
for (let i = 2; i < process.argv.length; i += 2) args.set(process.argv[i], process.argv[i + 1]);
for (const key of ["--output", "--menu", "--l10n", "--scene"])
  if (!args.get(key)) throw new Error(`missing ${key}`);

const [menuSource, l10nSource, sceneSource] = await Promise.all([
  readFile(args.get("--menu"), "utf8"),
  readFile(args.get("--l10n"), "utf8"),
  readFile(args.get("--scene"), "utf8")
]);
const hex = value => Buffer.from(String(value), "utf8").toString("hex");
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
const left = namedBlock(menuSource, "left_region");
const right = namedBlock(menuSource, "right_region");
const menuItems = [...menuSource.matchAll(/QW_MENU_ITEM\(\s*"([^"]+)"\s*,\s*"[^"]+"\s*,\s*"([^"]+)"\s*,\s*"([^"]*)"/g)]
  .map(match => ({ id: match[1], label: match[2], subtitle: match[3] }));
if (menuItems.length < 1 || menuItems.length > 16) throw new Error("invalid authored main-menu item count");
const layout = {
  left: [number(left, /left\s+([\d.]+)vw/, "left vw"), number(left, /top\s+([\d.]+)vh/, "left top vh"),
    number(left, /width\s+([\d.]+)vw/, "left width vw")],
  right: [number(right, /right\s+([\d.]+)vw/, "right vw"), number(right, /top\s+([\d.]+)vh/, "right top vh"),
    number(right, /width\s+([\d.]+)vw/, "right width vw")]
};

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
  .update(l10nSource).update("\0").update(sceneSource).digest("hex");
const lines = [`WAC1|${digest}`,
  `MENU|${hex("main")}|${layout.left.join("|")}|${layout.right.join("|")}`,
  ...menuItems.map(item => `ITEM|${hex(item.id)}|${hex(item.label)}|${hex(item.subtitle)}`),
  ...l10n.map(entry => `L10N|${hex(entry.key)}|${hex(entry.value)}`),
  `SCENE|${hex("arena1")}|${number(sceneSource, /\btime\s*=\s*(-?[\d.]+)/, "scene time")}|${/cameraSpace\s*=\s*['"]player['"]/.test(sceneSource) ? 1 : 0}|${fov.join("|")}`,
  `CURVE|${/type\s*=\s*['"]tcb['"]/.test(eye) ? 1 : 0}|${/boundary\s*=\s*['"]clamped['"]/.test(eye) ? 1 : /boundary\s*=\s*['"]closed['"]/.test(eye) ? 2 : 0}`,
  ...knots.map(knot => `EYE|${knot.join("|")}`),
  ...events.map(event => `EVENT|${hex(event.verb)}|${event.time}|${event.a}|${event.b}|${hex(event.text)}`),
  "END", ""];
await mkdir(dirname(args.get("--output")), { recursive: true });
await writeFile(args.get("--output"), lines.join("\n"));
console.log(`authored-content ${digest} menu=${menuItems.length} l10n=${l10n.length} knots=${knots.length} events=${events.length}`);
